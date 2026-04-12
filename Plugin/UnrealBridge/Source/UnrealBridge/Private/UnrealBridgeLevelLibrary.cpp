#include "UnrealBridgeLevelLibrary.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"                 // TActorIterator
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Editor.h"                      // GEditor
#include "Editor/EditorEngine.h"
#include "Selection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WorldPartition/WorldPartition.h"
#include "Engine/Blueprint.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeLevel"

// ─── Helpers ────────────────────────────────────────────────

namespace
{
	UWorld* GetEditorWorld()
	{
		if (GEditor)
		{
			return GEditor->GetEditorWorldContext().World();
		}
		return nullptr;
	}

	AActor* FindActor(UWorld* World, const FString& NameOrLabel)
	{
		if (!World || NameOrLabel.IsEmpty())
		{
			return nullptr;
		}
		const FName AsName(*NameOrLabel);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A)
			{
				continue;
			}
			if (A->GetFName() == AsName)
			{
				return A;
			}
			if (A->GetActorLabel() == NameOrLabel)
			{
				return A;
			}
		}
		return nullptr;
	}

	bool MatchesClassFilter(const UClass* Cls, const FString& Filter)
	{
		if (Filter.IsEmpty())
		{
			return true;
		}
		if (!Cls)
		{
			return false;
		}
		for (const UClass* Cur = Cls; Cur; Cur = Cur->GetSuperClass())
		{
			if (Cur->GetName() == Filter || Cur->GetPathName() == Filter)
			{
				return true;
			}
		}
		return false;
	}

	FBridgeTransform ToBridgeTransform(const FTransform& T)
	{
		FBridgeTransform Out;
		Out.Location = T.GetLocation();
		Out.Rotation = T.Rotator();
		Out.Scale = T.GetScale3D();
		return Out;
	}

	FBridgeActorBrief MakeBrief(AActor* Actor)
	{
		FBridgeActorBrief B;
		B.Name = Actor->GetFName().ToString();
		B.Label = Actor->GetActorLabel();
		B.ClassName = Actor->GetClass()->GetName();
		B.Location = Actor->GetActorLocation();
		for (const FName& Tag : Actor->Tags)
		{
			B.Tags.Add(Tag.ToString());
		}
		B.bHidden = Actor->IsHidden();
		return B;
	}

	FBridgeLevelComponentInfo MakeComponentInfo(UActorComponent* C)
	{
		FBridgeLevelComponentInfo CI;
		CI.Name = C->GetName();
		CI.ClassName = C->GetClass()->GetName();
		if (USceneComponent* SC = Cast<USceneComponent>(C))
		{
			CI.RelativeTransform.Location = SC->GetRelativeLocation();
			CI.RelativeTransform.Rotation = SC->GetRelativeRotation();
			CI.RelativeTransform.Scale = SC->GetRelativeScale3D();
			if (SC->GetAttachParent())
			{
				CI.AttachParent = SC->GetAttachParent()->GetName();
			}
		}
		return CI;
	}

	bool IsActorSelected(AActor* Actor)
	{
		if (!GEditor || !Actor)
		{
			return false;
		}
		return GEditor->GetSelectedActors()->IsSelected(Actor);
	}
}

// ─── Read ───────────────────────────────────────────────────

FBridgeLevelSummary UUnrealBridgeLevelLibrary::GetLevelSummary()
{
	FBridgeLevelSummary S;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return S;
	}

	S.LevelName = World->GetName();
	if (UPackage* Pkg = World->GetOutermost())
	{
		S.LevelPath = Pkg->GetName();
	}

	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		++Count;
	}
	S.NumActors = Count;

	S.NumStreamingLevels = World->GetStreamingLevels().Num();

	switch (World->WorldType)
	{
	case EWorldType::Editor:         S.WorldType = TEXT("Editor"); break;
	case EWorldType::PIE:            S.WorldType = TEXT("PIE"); break;
	case EWorldType::Game:           S.WorldType = TEXT("Game"); break;
	case EWorldType::EditorPreview:  S.WorldType = TEXT("EditorPreview"); break;
	case EWorldType::GamePreview:    S.WorldType = TEXT("GamePreview"); break;
	default:                         S.WorldType = TEXT("Other"); break;
	}

	S.bWorldPartition = (World->GetWorldPartition() != nullptr);
	return S;
}

int32 UUnrealBridgeLevelLibrary::GetActorCount(const FString& ClassFilter)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	int32 N = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (MatchesClassFilter(It->GetClass(), ClassFilter))
		{
			++N;
		}
	}
	return N;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorNames(const FString& ClassFilter, const FString& TagFilter, const FString& NameFilter)
{
	TArray<FString> Out;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	const FName TagName(*TagFilter);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		if (!MatchesClassFilter(A->GetClass(), ClassFilter))
		{
			continue;
		}
		if (!TagFilter.IsEmpty() && !A->Tags.Contains(TagName))
		{
			continue;
		}
		const FString Label = A->GetActorLabel();
		if (!NameFilter.IsEmpty() && !Label.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Out.Add(Label);
	}
	return Out;
}

TArray<FBridgeActorBrief> UUnrealBridgeLevelLibrary::ListActors(
	const FString& ClassFilter,
	const FString& TagFilter,
	const FString& NameFilter,
	bool bSelectedOnly,
	int32 MaxResults)
{
	TArray<FBridgeActorBrief> Out;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	const FName TagName(*TagFilter);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		if (!MatchesClassFilter(A->GetClass(), ClassFilter))
		{
			continue;
		}
		if (!TagFilter.IsEmpty() && !A->Tags.Contains(TagName))
		{
			continue;
		}
		const FString Label = A->GetActorLabel();
		if (!NameFilter.IsEmpty() && !Label.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (bSelectedOnly && !IsActorSelected(A))
		{
			continue;
		}
		Out.Add(MakeBrief(A));
		if (MaxResults > 0 && Out.Num() >= MaxResults)
		{
			break;
		}
	}
	return Out;
}

FBridgeActorInfo UUnrealBridgeLevelLibrary::GetActorInfo(const FString& ActorName)
{
	FBridgeActorInfo Info;
	UWorld* World = GetEditorWorld();
	AActor* Actor = FindActor(World, ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return Info;
	}

	Info.Name = Actor->GetFName().ToString();
	Info.Label = Actor->GetActorLabel();
	Info.ClassName = Actor->GetClass()->GetName();
	Info.ClassPath = Actor->GetClass()->GetPathName();
	Info.Transform = ToBridgeTransform(Actor->GetActorTransform());
	for (const FName& Tag : Actor->Tags)
	{
		Info.Tags.Add(Tag.ToString());
	}
	Info.bHidden = Actor->IsHidden();
	Info.bHiddenInGame = Actor->IsHidden();

	if (AActor* Parent = Actor->GetAttachParentActor())
	{
		Info.AttachedTo = Parent->GetActorLabel();
	}
	TArray<AActor*> Children;
	Actor->GetAttachedActors(Children);
	for (AActor* C : Children)
	{
		if (C)
		{
			Info.Children.Add(C->GetActorLabel());
		}
	}

	TArray<UActorComponent*> Comps;
	Actor->GetComponents(Comps);
	for (UActorComponent* C : Comps)
	{
		if (C)
		{
			Info.Components.Add(MakeComponentInfo(C));
		}
	}
	return Info;
}

FBridgeTransform UUnrealBridgeLevelLibrary::GetActorTransform(const FString& ActorName)
{
	FBridgeTransform T;
	if (AActor* A = FindActor(GetEditorWorld(), ActorName))
	{
		T = ToBridgeTransform(A->GetActorTransform());
	}
	return T;
}

TArray<FBridgeLevelComponentInfo> UUnrealBridgeLevelLibrary::GetActorComponents(const FString& ActorName)
{
	TArray<FBridgeLevelComponentInfo> Out;
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return Out;
	}
	TArray<UActorComponent*> Comps;
	Actor->GetComponents(Comps);
	for (UActorComponent* C : Comps)
	{
		if (C)
		{
			Out.Add(MakeComponentInfo(C));
		}
	}
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::FindActorsByClass(const FString& ClassPath, int32 MaxResults)
{
	TArray<FString> Out;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		if (MatchesClassFilter(A->GetClass(), ClassPath))
		{
			Out.Add(A->GetActorLabel());
			if (MaxResults > 0 && Out.Num() >= MaxResults)
			{
				break;
			}
		}
	}
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::FindActorsByTag(const FString& Tag)
{
	TArray<FString> Out;
	UWorld* World = GetEditorWorld();
	if (!World || Tag.IsEmpty())
	{
		return Out;
	}
	const FName TagName(*Tag);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->Tags.Contains(TagName))
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetSelectedActors()
{
	TArray<FString> Out;
	if (!GEditor)
	{
		return Out;
	}
	USelection* Sel = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AActor* A = Cast<AActor>(*It))
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

TArray<FBridgeStreamingLevel> UUnrealBridgeLevelLibrary::GetStreamingLevels()
{
	TArray<FBridgeStreamingLevel> Out;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	for (ULevelStreaming* SL : World->GetStreamingLevels())
	{
		if (!SL)
		{
			continue;
		}
		FBridgeStreamingLevel S;
		S.PackageName = SL->GetWorldAssetPackageName();
		S.bLoaded = SL->IsLevelLoaded();
		S.bVisible = SL->IsLevelVisible();
		Out.Add(S);
	}
	return Out;
}

FString UUnrealBridgeLevelLibrary::GetCurrentLevelPath()
{
	if (UWorld* World = GetEditorWorld())
	{
		if (UPackage* Pkg = World->GetOutermost())
		{
			return Pkg->GetName();
		}
	}
	return FString();
}

// ─── Write ──────────────────────────────────────────────────

namespace
{
	UClass* ResolveActorClass(const FString& ClassPath)
	{
		if (ClassPath.IsEmpty())
		{
			return nullptr;
		}
		// Try direct class load first (works for /Script/... and already-generated BPs ending in _C)
		if (UClass* Cls = LoadClass<AActor>(nullptr, *ClassPath))
		{
			return Cls;
		}
		// Try as Blueprint asset → GeneratedClass
		if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassPath))
		{
			if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(AActor::StaticClass()))
			{
				return BP->GeneratedClass;
			}
		}
		// Try with _C suffix
		const FString WithSuffix = ClassPath + TEXT("_C");
		if (UClass* Cls = LoadClass<AActor>(nullptr, *WithSuffix))
		{
			return Cls;
		}
		return nullptr;
	}
}

FString UUnrealBridgeLevelLibrary::SpawnActor(const FString& ClassPath, FVector Location, FRotator Rotation)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FString();
	}
	UClass* Cls = ResolveActorClass(ClassPath);
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not resolve actor class '%s'"), *ClassPath);
		return FString();
	}

	FScopedTransaction Tr(LOCTEXT("SpawnActor", "Spawn Actor"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* New = World->SpawnActor<AActor>(Cls, Location, Rotation, Params);
	if (!New)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SpawnActor failed for class '%s'"), *ClassPath);
		return FString();
	}
	return New->GetActorLabel();
}

bool UUnrealBridgeLevelLibrary::DestroyActor(const FString& ActorName)
{
	UWorld* World = GetEditorWorld();
	AActor* Actor = FindActor(World, ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("DestroyActor", "Destroy Actor"));
	Actor->Modify();
	return World->EditorDestroyActor(Actor, true);
}

int32 UUnrealBridgeLevelLibrary::DestroyActors(const TArray<FString>& ActorNames)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	FScopedTransaction Tr(LOCTEXT("DestroyActors", "Destroy Actors"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		if (AActor* A = FindActor(World, Name))
		{
			A->Modify();
			if (World->EditorDestroyActor(A, true))
			{
				++Count;
			}
		}
	}
	return Count;
}

bool UUnrealBridgeLevelLibrary::SetActorTransform(const FString& ActorName, FVector Location, FRotator Rotation, FVector Scale)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorTransform", "Set Actor Transform"));
	Actor->Modify();
	Actor->SetActorLocationAndRotation(Location, Rotation);
	Actor->SetActorScale3D(Scale);
	return true;
}

bool UUnrealBridgeLevelLibrary::MoveActor(const FString& ActorName, FVector DeltaLocation, FRotator DeltaRotation)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("MoveActor", "Move Actor"));
	Actor->Modify();
	const FVector NewLoc = Actor->GetActorLocation() + DeltaLocation;
	const FRotator NewRot = Actor->GetActorRotation() + DeltaRotation;
	Actor->SetActorLocationAndRotation(NewLoc, NewRot);
	return true;
}

bool UUnrealBridgeLevelLibrary::AttachActor(const FString& ChildName, const FString& ParentName, const FString& SocketName)
{
	UWorld* World = GetEditorWorld();
	AActor* Child = FindActor(World, ChildName);
	AActor* Parent = FindActor(World, ParentName);
	if (!Child || !Parent)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AttachActor — child or parent not found"));
		return false;
	}
	USceneComponent* ChildRoot = Child->GetRootComponent();
	USceneComponent* ParentRoot = Parent->GetRootComponent();
	if (!ChildRoot || !ParentRoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AttachActor — missing root component"));
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("AttachActor", "Attach Actor"));
	Child->Modify();
	ChildRoot->AttachToComponent(
		ParentRoot,
		FAttachmentTransformRules::KeepWorldTransform,
		FName(*SocketName));
	return true;
}

bool UUnrealBridgeLevelLibrary::DetachActor(const FString& ActorName)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("DetachActor", "Detach Actor"));
	Actor->Modify();
	Root->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	return true;
}

bool UUnrealBridgeLevelLibrary::SelectActors(const TArray<FString>& ActorNames, bool bAddToSelection)
{
	if (!GEditor)
	{
		return false;
	}
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return false;
	}
	if (!bAddToSelection)
	{
		GEditor->SelectNone(false, true, false);
	}
	for (const FString& Name : ActorNames)
	{
		if (AActor* A = FindActor(World, Name))
		{
			GEditor->SelectActor(A, true, true, true, false);
		}
	}
	GEditor->NoteSelectionChange();
	return true;
}

bool UUnrealBridgeLevelLibrary::DeselectAllActors()
{
	if (!GEditor)
	{
		return false;
	}
	GEditor->SelectNone(true, true, false);
	return true;
}

bool UUnrealBridgeLevelLibrary::SetActorLabel(const FString& ActorName, const FString& NewLabel)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorLabel", "Set Actor Label"));
	Actor->Modify();
	Actor->SetActorLabel(NewLabel);
	return true;
}

bool UUnrealBridgeLevelLibrary::SetActorHiddenInGame(const FString& ActorName, bool bHidden)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorHiddenInGame", "Set Actor Hidden In Game"));
	Actor->Modify();
	Actor->SetActorHiddenInGame(bHidden);
	return true;
}

// ─── Deep queries ───────────────────────────────────────────

namespace
{
	/**
	 * Walk a dotted property path. On success fills OutProp + OutContainer so caller
	 * can read/write using ExportTextItem_Direct / ImportText_Direct.
	 * Supports descent through FStructProperty and FObjectProperty (incl. components).
	 */
	bool ResolvePropertyPath(UObject* Root, const FString& Path, FProperty*& OutProp, void*& OutContainer, UObject*& OutOwner)
	{
		if (!Root || Path.IsEmpty())
		{
			return false;
		}
		TArray<FString> Parts;
		Path.ParseIntoArray(Parts, TEXT("."));
		if (Parts.Num() == 0)
		{
			return false;
		}

		void* Container = Root;
		UStruct* Struct = Root->GetClass();
		UObject* Owner = Root;

		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			FProperty* Prop = FindFProperty<FProperty>(Struct, *Parts[i]);
			if (!Prop)
			{
				// Fallback: actor may host components whose names match the path segment.
				if (AActor* ActorOwner = Cast<AActor>(Owner))
				{
					TArray<UActorComponent*> Comps;
					ActorOwner->GetComponents(Comps);
					UActorComponent* Found = nullptr;
					for (UActorComponent* C : Comps)
					{
						if (C && (C->GetName() == Parts[i] || C->GetClass()->GetName() == Parts[i]))
						{
							Found = C;
							break;
						}
					}
					if (Found)
					{
						Owner = Found;
						Container = Found;
						Struct = Found->GetClass();
						continue;
					}
				}
				return false;
			}

			const bool bIsLast = (i == Parts.Num() - 1);
			if (bIsLast)
			{
				OutProp = Prop;
				OutContainer = Container;
				OutOwner = Owner;
				return true;
			}

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
			if (FStructProperty* SP = CastField<FStructProperty>(Prop))
			{
				Container = ValuePtr;
				Struct = SP->Struct;
				// Owner stays the same for nested struct reads; writes trigger on outer UObject.
			}
			else if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
			{
				UObject* Sub = OP->GetObjectPropertyValue(ValuePtr);
				if (!Sub)
				{
					return false;
				}
				Owner = Sub;
				Container = Sub;
				Struct = Sub->GetClass();
			}
			else
			{
				return false;
			}
		}
		return false;
	}

	void CollectAttachmentTree(AActor* Actor, int32 Depth, TArray<FString>& Out)
	{
		if (!Actor)
		{
			return;
		}
		FString Indent;
		for (int32 i = 0; i < Depth; ++i)
		{
			Indent += TEXT("  ");
		}
		Out.Add(FString::Printf(TEXT("%s%s (%s)"), *Indent, *Actor->GetActorLabel(), *Actor->GetClass()->GetName()));
		TArray<AActor*> Children;
		Actor->GetAttachedActors(Children);
		for (AActor* C : Children)
		{
			CollectAttachmentTree(C, Depth + 1, Out);
		}
	}
}

FString UUnrealBridgeLevelLibrary::GetActorProperty(const FString& ActorName, const FString& PropertyPath)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return FString();
	}
	FProperty* Prop = nullptr;
	void* Container = nullptr;
	UObject* Owner = nullptr;
	if (!ResolvePropertyPath(Actor, PropertyPath, Prop, Container, Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Property path '%s' not found on '%s'"), *PropertyPath, *ActorName);
		return FString();
	}
	FString Out;
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
	Prop->ExportTextItem_Direct(Out, ValuePtr, nullptr, Owner, PPF_None);
	return Out;
}

bool UUnrealBridgeLevelLibrary::SetActorProperty(const FString& ActorName, const FString& PropertyPath, const FString& ExportedValue)
{
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FProperty* Prop = nullptr;
	void* Container = nullptr;
	UObject* Owner = nullptr;
	if (!ResolvePropertyPath(Actor, PropertyPath, Prop, Container, Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Property path '%s' not found on '%s'"), *PropertyPath, *ActorName);
		return false;
	}

	FScopedTransaction Tr(LOCTEXT("SetActorProperty", "Set Actor Property"));
	if (Owner)
	{
		Owner->Modify();
	}
	Actor->Modify();

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
	const TCHAR* ImportResult = Prop->ImportText_Direct(*ExportedValue, ValuePtr, Owner, PPF_None);
	if (!ImportResult)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: ImportText failed for path '%s' = '%s'"), *PropertyPath, *ExportedValue);
		return false;
	}

	// Notify property change so the editor updates.
	FPropertyChangedEvent ChangeEvent(Prop);
	if (Owner)
	{
		Owner->PostEditChangeProperty(ChangeEvent);
	}
	if (Owner != Actor)
	{
		Actor->PostEditChangeProperty(ChangeEvent);
	}
	return true;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetAttachmentTree(const FString& ActorName)
{
	TArray<FString> Out;
	AActor* Actor = FindActor(GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	CollectAttachmentTree(Actor, 0, Out);
	return Out;
}

TArray<FBridgeActorRadiusHit> UUnrealBridgeLevelLibrary::FindActorsInRadius(FVector Location, float Radius, const FString& ClassFilter)
{
	TArray<FBridgeActorRadiusHit> Out;
	UWorld* World = GetEditorWorld();
	if (!World || Radius <= 0.f)
	{
		return Out;
	}
	const float RadiusSq = Radius * Radius;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		if (!MatchesClassFilter(A->GetClass(), ClassFilter))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(A->GetActorLocation(), Location);
		if (DistSq > RadiusSq)
		{
			continue;
		}
		FBridgeActorRadiusHit H;
		H.Name = A->GetActorLabel();
		H.ClassName = A->GetClass()->GetName();
		H.Distance = FMath::Sqrt(DistSq);
		Out.Add(H);
	}
	Out.Sort([](const FBridgeActorRadiusHit& A, const FBridgeActorRadiusHit& B) { return A.Distance < B.Distance; });
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::DuplicateActors(const TArray<FString>& ActorNames)
{
	TArray<FString> Out;
	if (!GEditor)
	{
		return Out;
	}
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	UEditorActorSubsystem* EAS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!EAS)
	{
		return Out;
	}
	FScopedTransaction Tr(LOCTEXT("DuplicateActors", "Duplicate Actors"));
	for (const FString& Name : ActorNames)
	{
		AActor* Src = FindActor(World, Name);
		if (!Src)
		{
			continue;
		}
		if (AActor* New = EAS->DuplicateActor(Src, World))
		{
			Out.Add(New->GetActorLabel());
		}
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE

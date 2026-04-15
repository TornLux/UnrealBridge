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
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/WorldSettings.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeLevel"

// ─── Helpers ────────────────────────────────────────────────

namespace BridgeLevelImpl
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

	bool ActorIsSelected(AActor* Actor)
	{
		if (!GEditor || !Actor)
		{
			return false;
		}
		return GEditor->GetSelectedActors()->IsSelected(Actor);
	}

	UActorComponent* FindComponent(AActor* Actor, const FString& ComponentName)
	{
		if (!Actor || ComponentName.IsEmpty())
		{
			return nullptr;
		}
		TArray<UActorComponent*> Comps;
		Actor->GetComponents(Comps);
		for (UActorComponent* C : Comps)
		{
			if (C && C->GetName() == ComponentName)
			{
				return C;
			}
		}
		return nullptr;
	}
}

// ─── Read ───────────────────────────────────────────────────

FBridgeLevelSummary UUnrealBridgeLevelLibrary::GetLevelSummary()
{
	FBridgeLevelSummary S;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	int32 N = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (BridgeLevelImpl::MatchesClassFilter(It->GetClass(), ClassFilter))
		{
			++N;
		}
	}
	return N;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorNames(const FString& ClassFilter, const FString& TagFilter, const FString& NameFilter)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
		if (!BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter))
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
		if (!BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter))
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
		if (bSelectedOnly && !BridgeLevelImpl::ActorIsSelected(A))
		{
			continue;
		}
		Out.Add(BridgeLevelImpl::MakeBrief(A));
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AActor* Actor = BridgeLevelImpl::FindActor(World, ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return Info;
	}

	Info.Name = Actor->GetFName().ToString();
	Info.Label = Actor->GetActorLabel();
	Info.ClassName = Actor->GetClass()->GetName();
	Info.ClassPath = Actor->GetClass()->GetPathName();
	Info.Transform = BridgeLevelImpl::ToBridgeTransform(Actor->GetActorTransform());
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
			Info.Components.Add(BridgeLevelImpl::MakeComponentInfo(C));
		}
	}
	return Info;
}

FBridgeTransform UUnrealBridgeLevelLibrary::GetActorTransform(const FString& ActorName)
{
	FBridgeTransform T;
	if (AActor* A = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName))
	{
		T = BridgeLevelImpl::ToBridgeTransform(A->GetActorTransform());
	}
	return T;
}

TArray<FBridgeLevelComponentInfo> UUnrealBridgeLevelLibrary::GetActorComponents(const FString& ActorName)
{
	TArray<FBridgeLevelComponentInfo> Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
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
			Out.Add(BridgeLevelImpl::MakeComponentInfo(C));
		}
	}
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::FindActorsByClass(const FString& ClassPath, int32 MaxResults)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
		if (BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassPath))
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
	if (UWorld* World = BridgeLevelImpl::GetEditorWorld())
	{
		if (UPackage* Pkg = World->GetOutermost())
		{
			return Pkg->GetName();
		}
	}
	return FString();
}

// ─── Write ──────────────────────────────────────────────────

namespace BridgeLevelImpl
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return FString();
	}
	UClass* Cls = BridgeLevelImpl::ResolveActorClass(ClassPath);
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AActor* Actor = BridgeLevelImpl::FindActor(World, ActorName);
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	FScopedTransaction Tr(LOCTEXT("DestroyActors", "Destroy Actors"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		if (AActor* A = BridgeLevelImpl::FindActor(World, Name))
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AActor* Child = BridgeLevelImpl::FindActor(World, ChildName);
	AActor* Parent = BridgeLevelImpl::FindActor(World, ParentName);
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
		if (AActor* A = BridgeLevelImpl::FindActor(World, Name))
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
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

namespace BridgeLevelImpl
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
			BridgeLevelImpl::CollectAttachmentTree(C, Depth + 1, Out);
		}
	}
}

FString UUnrealBridgeLevelLibrary::GetActorProperty(const FString& ActorName, const FString& PropertyPath)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Actor '%s' not found"), *ActorName);
		return FString();
	}
	FProperty* Prop = nullptr;
	void* Container = nullptr;
	UObject* Owner = nullptr;
	if (!BridgeLevelImpl::ResolvePropertyPath(Actor, PropertyPath, Prop, Container, Owner))
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FProperty* Prop = nullptr;
	void* Container = nullptr;
	UObject* Owner = nullptr;
	if (!BridgeLevelImpl::ResolvePropertyPath(Actor, PropertyPath, Prop, Container, Owner))
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
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	BridgeLevelImpl::CollectAttachmentTree(Actor, 0, Out);
	return Out;
}

TArray<FBridgeActorRadiusHit> UUnrealBridgeLevelLibrary::FindActorsInRadius(FVector Location, float Radius, const FString& ClassFilter)
{
	TArray<FBridgeActorRadiusHit> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
		if (!BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter))
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
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
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
		AActor* Src = BridgeLevelImpl::FindActor(World, Name);
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

// ─── Spatial queries ───────────────────────────────────────

FBridgeActorBounds UUnrealBridgeLevelLibrary::GetActorBounds(const FString& ActorName)
{
	FBridgeActorBounds Out;
	AActor* A = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!A)
	{
		return Out;
	}
	FVector Origin, Extent;
	A->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent, /*bIncludeFromChildActors=*/true);
	Out.Origin = Origin;
	Out.BoxExtent = Extent;
	Out.SphereRadius = Extent.Size();
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorsInBox(FVector Min, FVector Max, const FString& ClassFilter)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	const FBox Box(Min, Max);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || !BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter))
		{
			continue;
		}
		if (Box.IsInsideOrOn(A->GetActorLocation()))
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

FString UUnrealBridgeLevelLibrary::FindNearestActor(FVector Location, const FString& ClassFilter)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return FString();
	}
	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || !BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(A->GetActorLocation(), Location);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = A;
		}
	}
	return Best ? Best->GetActorLabel() : FString();
}

float UUnrealBridgeLevelLibrary::GetActorDistance(const FString& ActorA, const FString& ActorB)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AActor* A = BridgeLevelImpl::FindActor(World, ActorA);
	AActor* B = BridgeLevelImpl::FindActor(World, ActorB);
	if (!A || !B)
	{
		return -1.f;
	}
	return (float)FVector::Dist(A->GetActorLocation(), B->GetActorLocation());
}

bool UUnrealBridgeLevelLibrary::IsActorSelected(const FString& ActorName)
{
	if (!GEditor)
	{
		return false;
	}
	AActor* A = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	return A && GEditor->GetSelectedActors()->IsSelected(A);
}

bool UUnrealBridgeLevelLibrary::SetActorHiddenInEditor(const FString& ActorName, bool bHidden)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorHiddenInEditor", "Set Actor Hidden In Editor"));
	Actor->Modify();
	Actor->SetIsTemporarilyHiddenInEditor(bHidden);
	return true;
}

bool UUnrealBridgeLevelLibrary::AddActorTag(const FString& ActorName, const FName Tag)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor || Tag.IsNone())
	{
		return false;
	}
	if (Actor->Tags.Contains(Tag))
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("AddActorTag", "Add Actor Tag"));
	Actor->Modify();
	Actor->Tags.Add(Tag);
	return true;
}

bool UUnrealBridgeLevelLibrary::RemoveActorTag(const FString& ActorName, const FName Tag)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor || Tag.IsNone())
	{
		return false;
	}
	if (!Actor->Tags.Contains(Tag))
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("RemoveActorTag", "Remove Actor Tag"));
	Actor->Modify();
	const int32 Removed = Actor->Tags.Remove(Tag);
	return Removed > 0;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorClassHistogram()
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	TMap<FString, int32> Counts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->GetClass())
		{
			continue;
		}
		const FString ClassName = A->GetClass()->GetName();
		Counts.FindOrAdd(ClassName)++;
	}
	Counts.ValueSort([](int32 L, int32 R) { return L > R; });
	Out.Reserve(Counts.Num());
	for (const TPair<FString, int32>& P : Counts)
	{
		Out.Add(FString::Printf(TEXT("%d\t%s"), P.Value, *P.Key));
	}
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorMaterials(const FString& ActorName)
{
	TArray<FString> Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	TArray<UMeshComponent*> MeshComps;
	Actor->GetComponents<UMeshComponent>(MeshComps);
	TSet<FString> Seen;
	for (UMeshComponent* MC : MeshComps)
	{
		if (!MC)
		{
			continue;
		}
		const int32 Num = MC->GetNumMaterials();
		for (int32 i = 0; i < Num; ++i)
		{
			UMaterialInterface* Mat = MC->GetMaterial(i);
			if (!Mat)
			{
				continue;
			}
			const FString Path = Mat->GetPathName();
			if (!Seen.Contains(Path))
			{
				Seen.Add(Path);
				Out.Add(Path);
			}
		}
	}
	return Out;
}

// ─── Folder organization ────────────────────────────────────

FString UUnrealBridgeLevelLibrary::GetActorFolder(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return FString();
	}
	const FName F = Actor->GetFolderPath();
	return F.IsNone() ? FString() : F.ToString();
}

bool UUnrealBridgeLevelLibrary::SetActorFolder(const FString& ActorName, const FString& FolderPath)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorFolder", "Set Actor Folder"));
	Actor->Modify();
	Actor->SetFolderPath(FolderPath.IsEmpty() ? FName() : FName(*FolderPath));
	return true;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorFolders()
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	TSet<FString> Seen;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		const FName F = A->GetFolderPath();
		if (F.IsNone())
		{
			continue;
		}
		const FString S = F.ToString();
		if (!Seen.Contains(S))
		{
			Seen.Add(S);
			Out.Add(S);
		}
	}
	Out.Sort();
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorsInFolder(const FString& FolderPath, bool bRecursive)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	const FString Prefix = FolderPath + TEXT("/");
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		const FString ActorFolder = A->GetFolderPath().ToString();
		bool bMatch = false;
		if (ActorFolder == FolderPath)
		{
			bMatch = true;
		}
		else if (bRecursive && !FolderPath.IsEmpty() && ActorFolder.StartsWith(Prefix))
		{
			bMatch = true;
		}
		else if (bRecursive && FolderPath.IsEmpty() && !ActorFolder.IsEmpty())
		{
			// Recursive from root = every actor with any folder
			bMatch = true;
		}
		if (bMatch)
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

// ─── Spatial — trace ────────────────────────────────────────

FString UUnrealBridgeLevelLibrary::LineTraceFirstActor(FVector Start, FVector End)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return FString();
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeLineTrace), /*bTraceComplex*/ true);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return FString();
	}
	AActor* A = Hit.GetActor();
	return A ? A->GetActorLabel() : FString();
}

TArray<FString> UUnrealBridgeLevelLibrary::MultiLineTraceActors(FVector Start, FVector End)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeMultiLineTrace), /*bTraceComplex*/ true);
	World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params);
	TSet<AActor*> Seen;
	for (const FHitResult& H : Hits)
	{
		AActor* A = H.GetActor();
		if (A && !Seen.Contains(A))
		{
			Seen.Add(A);
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

FString UUnrealBridgeLevelLibrary::SphereTraceFirstActor(FVector Start, FVector End, float Radius)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return FString();
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeSphereTrace), /*bTraceComplex*/ false);
	const bool bHit = World->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(FMath::Max(Radius, 0.0f)), Params);
	if (!bHit)
	{
		return FString();
	}
	AActor* A = Hit.GetActor();
	return A ? A->GetActorLabel() : FString();
}

TArray<FString> UUnrealBridgeLevelLibrary::MultiSphereTraceActors(FVector Start, FVector End, float Radius)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeMultiSphereTrace), /*bTraceComplex*/ false);
	World->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(FMath::Max(Radius, 0.0f)), Params);
	TSet<AActor*> Seen;
	for (const FHitResult& H : Hits)
	{
		AActor* A = H.GetActor();
		if (A && !Seen.Contains(A))
		{
			Seen.Add(A);
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

FString UUnrealBridgeLevelLibrary::BoxTraceFirstActor(FVector Start, FVector End, FVector BoxHalfExtent)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return FString();
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeBoxTrace), /*bTraceComplex*/ false);
	const FVector SafeExtent(
		FMath::Max(BoxHalfExtent.X, 0.0f),
		FMath::Max(BoxHalfExtent.Y, 0.0f),
		FMath::Max(BoxHalfExtent.Z, 0.0f));
	const bool bHit = World->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeBox(SafeExtent), Params);
	if (!bHit)
	{
		return FString();
	}
	AActor* A = Hit.GetActor();
	return A ? A->GetActorLabel() : FString();
}

TArray<FString> UUnrealBridgeLevelLibrary::OverlapSphereActors(FVector Center, float Radius, const FString& ClassFilter)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeOverlapSphere), /*bTraceComplex*/ false);
	World->OverlapMultiByChannel(
		Overlaps, Center, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(FMath::Max(Radius, 0.0f)), Params);
	TSet<AActor*> Seen;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || Seen.Contains(A))
		{
			continue;
		}
		if (!BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter))
		{
			continue;
		}
		Seen.Add(A);
		Out.Add(A->GetActorLabel());
	}
	return Out;
}

// ─── Components / sockets ───────────────────────────────────

TArray<FString> UUnrealBridgeLevelLibrary::GetActorSockets(const FString& ActorName)
{
	TArray<FString> Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	TArray<USceneComponent*> SceneComps;
	Actor->GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* SC : SceneComps)
	{
		if (!SC)
		{
			continue;
		}
		TArray<FName> SocketNames = SC->GetAllSocketNames();
		for (const FName& SN : SocketNames)
		{
			Out.Add(FString::Printf(TEXT("%s:%s"), *SC->GetName(), *SN.ToString()));
		}
	}
	return Out;
}

FBridgeTransform UUnrealBridgeLevelLibrary::GetSocketWorldTransform(const FString& ActorName, const FString& ComponentName, const FName SocketName)
{
	FBridgeTransform Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	USceneComponent* SC = Cast<USceneComponent>(BridgeLevelImpl::FindComponent(Actor, ComponentName));
	if (!SC)
	{
		return Out;
	}
	if (!SocketName.IsNone() && !SC->DoesSocketExist(SocketName))
	{
		return Out;
	}
	const FTransform T = SC->GetSocketTransform(SocketName, RTS_World);
	return BridgeLevelImpl::ToBridgeTransform(T);
}

FBridgeTransform UUnrealBridgeLevelLibrary::GetComponentWorldTransform(const FString& ActorName, const FString& ComponentName)
{
	FBridgeTransform Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	USceneComponent* SC = Cast<USceneComponent>(BridgeLevelImpl::FindComponent(Actor, ComponentName));
	if (!SC)
	{
		return Out;
	}
	return BridgeLevelImpl::ToBridgeTransform(SC->GetComponentTransform());
}

bool UUnrealBridgeLevelLibrary::SetComponentVisibility(const FString& ActorName, const FString& ComponentName, bool bVisible, bool bPropagateToChildren)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	USceneComponent* SC = Cast<USceneComponent>(BridgeLevelImpl::FindComponent(Actor, ComponentName));
	if (!SC)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetComponentVisibility", "Set Component Visibility"));
	Actor->Modify();
	SC->Modify();
	SC->SetVisibility(bVisible, bPropagateToChildren);
	return true;
}

bool UUnrealBridgeLevelLibrary::SetComponentMobility(const FString& ActorName, const FString& ComponentName, const FString& Mobility)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	USceneComponent* SC = Cast<USceneComponent>(BridgeLevelImpl::FindComponent(Actor, ComponentName));
	if (!SC)
	{
		return false;
	}
	EComponentMobility::Type NewMobility;
	if (Mobility.Equals(TEXT("Static"), ESearchCase::IgnoreCase))
	{
		NewMobility = EComponentMobility::Static;
	}
	else if (Mobility.Equals(TEXT("Stationary"), ESearchCase::IgnoreCase))
	{
		NewMobility = EComponentMobility::Stationary;
	}
	else if (Mobility.Equals(TEXT("Movable"), ESearchCase::IgnoreCase))
	{
		NewMobility = EComponentMobility::Movable;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Invalid Mobility '%s' (expected Static/Stationary/Movable)"), *Mobility);
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetComponentMobility", "Set Component Mobility"));
	Actor->Modify();
	SC->Modify();
	SC->SetMobility(NewMobility);
	return true;
}

// ─── Bulk transform + level-wide spatial ───────────────────────────────

bool UUnrealBridgeLevelLibrary::SnapActorToFloor(const FString& ActorName, float MaxDistance)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AActor* Actor = BridgeLevelImpl::FindActor(World, ActorName);
	if (!Actor)
	{
		return false;
	}
	const FVector Start = Actor->GetActorLocation();
	const FVector End = Start - FVector(0, 0, FMath::Max(MaxDistance, 0.0f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeSnapToFloor), /*bTraceComplex=*/ false, Actor);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SnapActorToFloor", "Snap Actor To Floor"));
	Actor->Modify();
	FVector NewLoc = Start;
	NewLoc.Z = Hit.ImpactPoint.Z;
	Actor->SetActorLocation(NewLoc);
	return true;
}

int32 UUnrealBridgeLevelLibrary::SnapActorsToGrid(const TArray<FString>& ActorNames, float GridSize)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World || GridSize <= 0.0f)
	{
		return 0;
	}
	FScopedTransaction Tr(LOCTEXT("SnapActorsToGrid", "Snap Actors To Grid"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		const FVector L = A->GetActorLocation();
		const FVector Snapped(
			FMath::GridSnap(L.X, GridSize),
			FMath::GridSnap(L.Y, GridSize),
			FMath::GridSnap(L.Z, GridSize));
		A->SetActorLocation(Snapped);
		++Count;
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::OffsetActors(const TArray<FString>& ActorNames, FVector DeltaLocation)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	FScopedTransaction Tr(LOCTEXT("OffsetActors", "Offset Actors"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		A->SetActorLocation(A->GetActorLocation() + DeltaLocation);
		++Count;
	}
	return Count;
}

FBridgeActorBounds UUnrealBridgeLevelLibrary::GetLevelBounds()
{
	FBridgeActorBounds Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	FBox Total(ForceInit);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		FVector Origin, Extent;
		A->GetActorBounds(/*bOnlyCollidingComponents=*/ false, Origin, Extent, /*bIncludeFromChildActors=*/ true);
		if (!Extent.IsNearlyZero())
		{
			Total += FBox::BuildAABB(Origin, Extent);
		}
	}
	if (!Total.IsValid)
	{
		return Out;
	}
	Out.Origin = Total.GetCenter();
	Out.BoxExtent = Total.GetExtent();
	Out.SphereRadius = Out.BoxExtent.Size();
	return Out;
}

// ─── Editor visibility grouping ────────────────────────────────────────

int32 UUnrealBridgeLevelLibrary::IsolateActors(const TArray<FString>& KeepVisible)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	TSet<AActor*> Keep;
	for (const FString& Name : KeepVisible)
	{
		if (AActor* A = BridgeLevelImpl::FindActor(World, Name))
		{
			Keep.Add(A);
		}
	}
	FScopedTransaction Tr(LOCTEXT("IsolateActors", "Isolate Actors"));
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || Keep.Contains(A) || A->IsHiddenEd())
		{
			continue;
		}
		A->Modify();
		A->SetIsTemporarilyHiddenInEditor(true);
		++Count;
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::ShowAllActors()
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	FScopedTransaction Tr(LOCTEXT("ShowAllActors", "Show All Actors"));
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->IsHiddenEd())
		{
			continue;
		}
		A->Modify();
		A->SetIsTemporarilyHiddenInEditor(false);
		++Count;
	}
	return Count;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetHiddenActorNames()
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return Out;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->IsHiddenEd())
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

int32 UUnrealBridgeLevelLibrary::ToggleActorsHidden(const TArray<FString>& ActorNames)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return 0;
	}
	FScopedTransaction Tr(LOCTEXT("ToggleActorsHidden", "Toggle Actors Hidden"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		A->SetIsTemporarilyHiddenInEditor(!A->IsHiddenEd());
		++Count;
	}
	return Count;
}

// ─── Static mesh + material setters ────────────────────────────────────

FString UUnrealBridgeLevelLibrary::GetActorMesh(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return FString();
	}
	UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
	if (!SMC || !SMC->GetStaticMesh())
	{
		return FString();
	}
	return SMC->GetStaticMesh()->GetPathName();
}

bool UUnrealBridgeLevelLibrary::SetActorMesh(const FString& ActorName, const FString& MeshAssetPath)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
	if (!SMC)
	{
		return false;
	}
	UStaticMesh* NewMesh = LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);
	if (!NewMesh)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorMesh", "Set Actor Mesh"));
	Actor->Modify();
	SMC->Modify();
	SMC->SetStaticMesh(NewMesh);
	return true;
}

bool UUnrealBridgeLevelLibrary::SetActorMaterial(const FString& ActorName, int32 MaterialIndex, const FString& MaterialAssetPath)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	UMeshComponent* MC = Actor->FindComponentByClass<UMeshComponent>();
	if (!MC)
	{
		return false;
	}
	if (MaterialIndex < 0 || MaterialIndex >= MC->GetNumMaterials())
	{
		return false;
	}
	UMaterialInterface* NewMat = nullptr;
	if (!MaterialAssetPath.IsEmpty())
	{
		NewMat = LoadObject<UMaterialInterface>(nullptr, *MaterialAssetPath);
		if (!NewMat)
		{
			return false;
		}
	}
	FScopedTransaction Tr(LOCTEXT("SetActorMaterial", "Set Actor Material"));
	Actor->Modify();
	MC->Modify();
	MC->SetMaterial(MaterialIndex, NewMat);
	return true;
}

namespace BridgeLevelImpl
{
	/** Prefer the root component if it's a primitive; else the first primitive found. */
	static UPrimitiveComponent* GetPrimaryPrimitive(AActor* Actor)
	{
		if (!Actor) return nullptr;
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			return Root;
		}
		return Actor->FindComponentByClass<UPrimitiveComponent>();
	}
}

FString UUnrealBridgeLevelLibrary::GetActorCollisionProfile(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	UPrimitiveComponent* Prim = BridgeLevelImpl::GetPrimaryPrimitive(Actor);
	if (!Prim)
	{
		return FString();
	}
	return Prim->GetCollisionProfileName().ToString();
}

bool UUnrealBridgeLevelLibrary::SetActorCollisionProfile(const FString& ActorName, const FString& ProfileName)
{
	if (ProfileName.IsEmpty())
	{
		return false;
	}
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	UPrimitiveComponent* Prim = BridgeLevelImpl::GetPrimaryPrimitive(Actor);
	if (!Prim)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorCollisionProfile", "Set Actor Collision Profile"));
	Actor->Modify();
	Prim->Modify();
	Prim->SetCollisionProfileName(FName(*ProfileName));
	return true;
}

bool UUnrealBridgeLevelLibrary::SetActorSimulatePhysics(const FString& ActorName, bool bSimulate)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	UPrimitiveComponent* Prim = BridgeLevelImpl::GetPrimaryPrimitive(Actor);
	if (!Prim)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorSimulatePhysics", "Set Actor Simulate Physics"));
	Actor->Modify();
	Prim->Modify();
	// Physics sim requires Movable mobility — auto-promote and let the
	// transaction revert it on undo.
	if (bSimulate && Prim->Mobility != EComponentMobility::Movable)
	{
		Prim->SetMobility(EComponentMobility::Movable);
	}
	Prim->SetSimulatePhysics(bSimulate);
	return true;
}

bool UUnrealBridgeLevelLibrary::SetActorEnableCollision(const FString& ActorName, bool bEnabled)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetActorEnableCollision", "Set Actor Enable Collision"));
	Actor->Modify();
	Actor->SetActorEnableCollision(bEnabled);
	return true;
}

// ─── Component add/remove + root query ─────────────────────────────────

FString UUnrealBridgeLevelLibrary::GetActorRootComponentName(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor || !Actor->GetRootComponent())
	{
		return FString();
	}
	return Actor->GetRootComponent()->GetName();
}

FString UUnrealBridgeLevelLibrary::AddComponentOfClass(const FString& ActorName, const FString& ComponentClassPath)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return FString();
	}
	UClass* CompClass = LoadObject<UClass>(nullptr, *ComponentClassPath);
	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return FString();
	}
	FScopedTransaction Tr(LOCTEXT("AddComponentOfClass", "Add Component Of Class"));
	Actor->Modify();

	UActorComponent* NewComp = NewObject<UActorComponent>(Actor, CompClass, NAME_None, RF_Transactional);
	if (!NewComp)
	{
		return FString();
	}
	Actor->AddInstanceComponent(NewComp);
	// Scene components need attachment to the root; non-scene are just registered.
	if (USceneComponent* SC = Cast<USceneComponent>(NewComp))
	{
		if (USceneComponent* Root = Actor->GetRootComponent())
		{
			SC->SetupAttachment(Root);
		}
	}
	NewComp->RegisterComponent();
	Actor->RerunConstructionScripts();
	return NewComp->GetName();
}

bool UUnrealBridgeLevelLibrary::RemoveComponent(const FString& ActorName, const FString& ComponentName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	UActorComponent* Comp = BridgeLevelImpl::FindComponent(Actor, ComponentName);
	if (!Comp)
	{
		return false;
	}
	// Only instance components can be safely removed — refuse CDO/native.
	if (!Actor->GetInstanceComponents().Contains(Comp))
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("RemoveComponent", "Remove Component"));
	Actor->Modify();
	Comp->Modify();
	Actor->RemoveInstanceComponent(Comp);
	Comp->DestroyComponent();
	Actor->RerunConstructionScripts();
	return true;
}

// ─── Level streaming runtime control ───────────────────────────────────

namespace BridgeLevelImpl
{
	static ULevelStreaming* FindStreamingLevel(UWorld* World, const FString& PackageName)
	{
		if (!World || PackageName.IsEmpty()) return nullptr;
		for (ULevelStreaming* SL : World->GetStreamingLevels())
		{
			if (SL && SL->GetWorldAssetPackageName() == PackageName)
			{
				return SL;
			}
		}
		return nullptr;
	}
}

bool UUnrealBridgeLevelLibrary::SetStreamingLevelLoaded(const FString& PackageName, bool bLoaded)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	ULevelStreaming* SL = BridgeLevelImpl::FindStreamingLevel(World, PackageName);
	if (!SL)
	{
		return false;
	}
	SL->SetShouldBeLoaded(bLoaded);
	return true;
}

bool UUnrealBridgeLevelLibrary::SetStreamingLevelVisible(const FString& PackageName, bool bVisible)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	ULevelStreaming* SL = BridgeLevelImpl::FindStreamingLevel(World, PackageName);
	if (!SL)
	{
		return false;
	}
	SL->SetShouldBeVisible(bVisible);
	return true;
}

bool UUnrealBridgeLevelLibrary::IsStreamingLevelLoaded(const FString& PackageName)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	ULevelStreaming* SL = BridgeLevelImpl::FindStreamingLevel(World, PackageName);
	return SL && SL->IsLevelLoaded();
}

bool UUnrealBridgeLevelLibrary::FlushLevelStreaming()
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World)
	{
		return false;
	}
	World->FlushLevelStreaming();
	return true;
}

// ─── World settings: gravity + kill Z ──────────────────────────────────

float UUnrealBridgeLevelLibrary::GetWorldGravity()
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AWorldSettings* WS = World ? World->GetWorldSettings() : nullptr;
	return WS ? WS->GetGravityZ() : 0.0f;
}

bool UUnrealBridgeLevelLibrary::SetWorldGravity(float Gravity, bool bOverride)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AWorldSettings* WS = World ? World->GetWorldSettings() : nullptr;
	if (!WS)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetWorldGravity", "Set World Gravity"));
	WS->Modify();
	WS->WorldGravityZ = Gravity;
	WS->bWorldGravitySet = bOverride;
	return true;
}

float UUnrealBridgeLevelLibrary::GetKillZ()
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AWorldSettings* WS = World ? World->GetWorldSettings() : nullptr;
	return WS ? WS->KillZ : 0.0f;
}

bool UUnrealBridgeLevelLibrary::SetKillZ(float NewKillZ)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AWorldSettings* WS = World ? World->GetWorldSettings() : nullptr;
	if (!WS)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetKillZ", "Set Kill Z"));
	WS->Modify();
	WS->KillZ = NewKillZ;
	return true;
}

// ─── Ground / downward trace helpers ───────────────────────────────────

namespace BridgeLevelImpl
{
	static bool DownwardTrace(UWorld* World, float X, float Y, float StartHeight, FHitResult& OutHit, AActor* IgnoreActor = nullptr)
	{
		if (!World)
		{
			return false;
		}
		const FVector Start(X, Y, StartHeight);
		const FVector End(X, Y, -StartHeight);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeGroundTrace), /*bTraceComplex=*/ false);
		if (IgnoreActor) Params.AddIgnoredActor(IgnoreActor);
		return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	}
}

float UUnrealBridgeLevelLibrary::GetGroundHeightAt(float X, float Y, float StartHeight)
{
	FHitResult Hit;
	if (!BridgeLevelImpl::DownwardTrace(BridgeLevelImpl::GetEditorWorld(), X, Y, StartHeight, Hit))
	{
		return -1.0e6f;
	}
	return Hit.ImpactPoint.Z;
}

bool UUnrealBridgeLevelLibrary::GetGroundNormalAt(float X, float Y, FVector& OutNormal, float StartHeight)
{
	OutNormal = FVector::ZAxisVector;
	FHitResult Hit;
	if (!BridgeLevelImpl::DownwardTrace(BridgeLevelImpl::GetEditorWorld(), X, Y, StartHeight, Hit))
	{
		return false;
	}
	OutNormal = Hit.ImpactNormal;
	return true;
}

FString UUnrealBridgeLevelLibrary::GetGroundHitActor(float X, float Y, float StartHeight)
{
	FHitResult Hit;
	if (!BridgeLevelImpl::DownwardTrace(BridgeLevelImpl::GetEditorWorld(), X, Y, StartHeight, Hit))
	{
		return FString();
	}
	AActor* A = Hit.GetActor();
	return A ? A->GetActorLabel() : FString();
}

float UUnrealBridgeLevelLibrary::GetActorGroundClearance(const FString& ActorName, float MaxDistance)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	AActor* Actor = BridgeLevelImpl::FindActor(World, ActorName);
	if (!Actor)
	{
		return -1.0f;
	}
	const FVector Start = Actor->GetActorLocation();
	const FVector End = Start - FVector(0, 0, FMath::Max(MaxDistance, 0.0f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeActorGround), /*bTraceComplex=*/ false, Actor);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return -1.0f;
	}
	return (Start - Hit.ImpactPoint).Size();
}

// ─── Actor hierarchy traversal ─────────────────────────────────────────

namespace BridgeLevelImpl
{
	static void CollectDescendantsRecursive(AActor* Root, TArray<FString>& Out, int32 Guard = 0)
	{
		if (!Root || Guard > 1024) return;
		TArray<AActor*> Children;
		Root->GetAttachedActors(Children, /*bResetArray=*/ false);
		for (AActor* C : Children)
		{
			if (!C) continue;
			Out.Add(C->GetActorLabel());
			CollectDescendantsRecursive(C, Out, Guard + 1);
		}
	}
}

TArray<FString> UUnrealBridgeLevelLibrary::GetAllDescendants(const FString& ActorName)
{
	TArray<FString> Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return Out;
	}
	BridgeLevelImpl::CollectDescendantsRecursive(Actor, Out);
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorSiblings(const FString& ActorName)
{
	TArray<FString> Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return Out;
	AActor* Parent = Actor->GetAttachParentActor();
	if (!Parent) return Out;
	TArray<AActor*> Siblings;
	Parent->GetAttachedActors(Siblings, /*bResetArray=*/ false);
	for (AActor* S : Siblings)
	{
		if (S && S != Actor)
		{
			Out.Add(S->GetActorLabel());
		}
	}
	return Out;
}

FString UUnrealBridgeLevelLibrary::GetRootAttachParent(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return FString();
	AActor* Cursor = Actor;
	// Walk up with a small guard against malformed cycles.
	for (int32 i = 0; i < 1024; ++i)
	{
		AActor* P = Cursor->GetAttachParentActor();
		if (!P) break;
		Cursor = P;
	}
	return Cursor->GetActorLabel();
}

int32 UUnrealBridgeLevelLibrary::GetAttachmentDepth(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return -1;
	int32 Depth = 0;
	AActor* Cursor = Actor;
	for (int32 i = 0; i < 1024; ++i)
	{
		AActor* P = Cursor->GetAttachParentActor();
		if (!P) break;
		++Depth;
		Cursor = P;
	}
	return Depth;
}

// ─── Static-mesh budget stats ──────────────────────────────────────────

int32 UUnrealBridgeLevelLibrary::GetActorVertexCount(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return -1;
	TArray<UStaticMeshComponent*> SMCs;
	Actor->GetComponents<UStaticMeshComponent>(SMCs);
	int32 Total = 0;
	for (UStaticMeshComponent* SMC : SMCs)
	{
		UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
		if (!Mesh || !Mesh->GetRenderData()) continue;
		const auto& LODs = Mesh->GetRenderData()->LODResources;
		if (LODs.Num() > 0)
		{
			Total += LODs[0].GetNumVertices();
		}
	}
	return Total;
}

int32 UUnrealBridgeLevelLibrary::GetActorTriangleCount(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return -1;
	TArray<UStaticMeshComponent*> SMCs;
	Actor->GetComponents<UStaticMeshComponent>(SMCs);
	int32 Total = 0;
	for (UStaticMeshComponent* SMC : SMCs)
	{
		UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
		if (!Mesh || !Mesh->GetRenderData()) continue;
		const auto& LODs = Mesh->GetRenderData()->LODResources;
		if (LODs.Num() > 0)
		{
			Total += LODs[0].GetNumTriangles();
		}
	}
	return Total;
}

int32 UUnrealBridgeLevelLibrary::GetActorMaterialSlotCount(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return -1;
	TArray<UMeshComponent*> MCs;
	Actor->GetComponents<UMeshComponent>(MCs);
	int32 Total = 0;
	for (UMeshComponent* MC : MCs)
	{
		if (MC) Total += MC->GetNumMaterials();
	}
	return Total;
}

int32 UUnrealBridgeLevelLibrary::GetActorLODCount(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return -1;
	TArray<UStaticMeshComponent*> SMCs;
	Actor->GetComponents<UStaticMeshComponent>(SMCs);
	int32 MaxLODs = 0;
	for (UStaticMeshComponent* SMC : SMCs)
	{
		UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
		if (!Mesh || !Mesh->GetRenderData()) continue;
		MaxLODs = FMath::Max(MaxLODs, Mesh->GetRenderData()->LODResources.Num());
	}
	return MaxLODs;
}

// ─── Tag bulk ops ──────────────────────────────────────────────────────

TArray<FString> UUnrealBridgeLevelLibrary::GetAllActorTagsInLevel()
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return Out;
	TSet<FName> Unique;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (AActor* A = *It)
		{
			for (const FName& T : A->Tags) Unique.Add(T);
		}
	}
	Out.Reserve(Unique.Num());
	for (const FName& T : Unique) Out.Add(T.ToString());
	Out.Sort();
	return Out;
}

int32 UUnrealBridgeLevelLibrary::CountActorsByTag(const FName Tag)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World || Tag.IsNone()) return 0;
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (AActor* A = *It)
		{
			if (A->Tags.Contains(Tag)) ++Count;
		}
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::SelectActorsByTag(const FName Tag, bool bAddToSelection)
{
	if (!GEditor || Tag.IsNone()) return 0;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	if (!bAddToSelection)
	{
		GEditor->SelectNone(false, true, false);
	}
	int32 Added = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->Tags.Contains(Tag))
		{
			GEditor->SelectActor(A, true, true, true, false);
			++Added;
		}
	}
	GEditor->NoteSelectionChange();
	return Added;
}

int32 UUnrealBridgeLevelLibrary::RemoveTagFromAllActors(const FName Tag)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World || Tag.IsNone()) return 0;
	FScopedTransaction Tr(LOCTEXT("RemoveTagFromAllActors", "Remove Tag From All Actors"));
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		const int32 Removed = A->Tags.Remove(Tag);
		if (Removed > 0)
		{
			A->Modify();
			++Count;
		}
	}
	return Count;
}

// ─── Actor class introspection ─────────────────────────────────────────

bool UUnrealBridgeLevelLibrary::IsActorOfClass(const FString& ActorName, const FString& ClassPath)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return false;
	return BridgeLevelImpl::MatchesClassFilter(Actor->GetClass(), ClassPath);
}

FString UUnrealBridgeLevelLibrary::GetActorParentClass(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor || !Actor->GetClass() || !Actor->GetClass()->GetSuperClass())
	{
		return FString();
	}
	return Actor->GetClass()->GetSuperClass()->GetName();
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorClassHierarchy(const FString& ActorName)
{
	TArray<FString> Out;
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return Out;
	UClass* Cls = Actor->GetClass();
	while (Cls)
	{
		Out.Add(Cls->GetName());
		Cls = Cls->GetSuperClass();
	}
	return Out;
}

TArray<FString> UUnrealBridgeLevelLibrary::FindActorsByClassAndTag(const FString& ClassFilter, const FName Tag)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World || Tag.IsNone()) return Out;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		if (!BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter)) continue;
		if (!A->Tags.Contains(Tag)) continue;
		Out.Add(A->GetActorLabel());
	}
	return Out;
}

// ─── Bulk rotate / scale / mirror ──────────────────────────────────────

int32 UUnrealBridgeLevelLibrary::RotateActors(const TArray<FString>& ActorNames, FRotator DeltaRotation)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("RotateActors", "Rotate Actors"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		A->SetActorRotation(A->GetActorRotation() + DeltaRotation);
		++Count;
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::ScaleActors(const TArray<FString>& ActorNames, FVector ScaleMultiplier)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("ScaleActors", "Scale Actors"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		A->SetActorScale3D(A->GetActorScale3D() * ScaleMultiplier);
		++Count;
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::SetActorsUniformScale(const TArray<FString>& ActorNames, float UniformScale)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("SetActorsUniformScale", "Set Actors Uniform Scale"));
	const FVector S(UniformScale, UniformScale, UniformScale);
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		A->SetActorScale3D(S);
		++Count;
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::MoveActorsToFolder(const TArray<FString>& ActorNames, const FString& FolderPath)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("MoveActorsToFolder", "Move Actors To Folder"));
	const FName Target = FolderPath.IsEmpty() ? FName() : FName(*FolderPath);
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		A->SetFolderPath(Target);
		++Count;
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::RenameFolder(const FString& OldFolder, const FString& NewFolder)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("RenameFolder", "Rename Folder"));
	const FName Target = NewFolder.IsEmpty() ? FName() : FName(*NewFolder);
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetFolderPath().ToString() == OldFolder)
		{
			A->Modify();
			A->SetFolderPath(Target);
			++Count;
		}
	}
	return Count;
}

int32 UUnrealBridgeLevelLibrary::DissolveFolder(const FString& FolderPath)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("DissolveFolder", "Dissolve Folder"));
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetFolderPath().ToString() == FolderPath)
		{
			A->Modify();
			A->SetFolderPath(FName());
			++Count;
		}
	}
	return Count;
}

bool UUnrealBridgeLevelLibrary::IsFolderEmpty(const FString& FolderPath)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return true;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetFolderPath().ToString() == FolderPath)
		{
			return false;
		}
	}
	return true;
}

// ─── Sublevel-scoped actor queries ─────────────────────────────────────

namespace BridgeLevelImpl
{
	static ULevel* FindLevelByPackage(UWorld* World, const FString& PackageName)
	{
		if (!World || PackageName.IsEmpty()) return nullptr;
		for (ULevel* L : World->GetLevels())
		{
			if (!L) continue;
			UPackage* Pkg = L->GetOutermost();
			if (Pkg && Pkg->GetName() == PackageName)
			{
				return L;
			}
		}
		return nullptr;
	}
}

TArray<FString> UUnrealBridgeLevelLibrary::GetActorsInSublevel(const FString& PackageName)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	ULevel* Level = BridgeLevelImpl::FindLevelByPackage(World, PackageName);
	if (!Level) return Out;
	for (AActor* A : Level->Actors)
	{
		if (A)
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

int32 UUnrealBridgeLevelLibrary::CountActorsInSublevel(const FString& PackageName)
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	ULevel* Level = BridgeLevelImpl::FindLevelByPackage(World, PackageName);
	if (!Level) return 0;
	int32 Count = 0;
	for (AActor* A : Level->Actors)
	{
		if (A) ++Count;
	}
	return Count;
}

FString UUnrealBridgeLevelLibrary::GetActorLevelPackageName(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor) return FString();
	ULevel* Level = Actor->GetLevel();
	UPackage* Pkg = Level ? Level->GetOutermost() : nullptr;
	return Pkg ? Pkg->GetName() : FString();
}

int32 UUnrealBridgeLevelLibrary::GetPersistentLevelActorCount()
{
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World || !World->PersistentLevel) return 0;
	int32 Count = 0;
	for (AActor* A : World->PersistentLevel->Actors)
	{
		if (A) ++Count;
	}
	return Count;
}

// ─── Bulk selection helpers ────────────────────────────────────────────

int32 UUnrealBridgeLevelLibrary::InvertSelection()
{
	if (!GEditor) return 0;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;

	// Snapshot current selection so we don't flip actors twice.
	TSet<AActor*> Selected;
	if (USelection* Sel = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Sel); It; ++It)
		{
			if (AActor* A = Cast<AActor>(*It))
			{
				Selected.Add(A);
			}
		}
	}
	GEditor->SelectNone(false, true, false);
	int32 NowSelected = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		const bool bShouldSelect = !Selected.Contains(A);
		if (bShouldSelect)
		{
			GEditor->SelectActor(A, true, true, true, false);
			++NowSelected;
		}
	}
	GEditor->NoteSelectionChange();
	return NowSelected;
}

int32 UUnrealBridgeLevelLibrary::SelectAllActors()
{
	if (!GEditor) return 0;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	GEditor->SelectNone(false, true, false);
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (AActor* A = *It)
		{
			GEditor->SelectActor(A, true, true, true, false);
			++Count;
		}
	}
	GEditor->NoteSelectionChange();
	return Count;
}

int32 UUnrealBridgeLevelLibrary::SelectActorsInBox(FVector Min, FVector Max, bool bAddToSelection)
{
	if (!GEditor) return 0;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	const FBox Box(Min, Max);
	if (!bAddToSelection)
	{
		GEditor->SelectNone(false, true, false);
	}
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && Box.IsInside(A->GetActorLocation()))
		{
			GEditor->SelectActor(A, true, true, true, false);
			++Count;
		}
	}
	GEditor->NoteSelectionChange();
	return Count;
}

int32 UUnrealBridgeLevelLibrary::GetSelectionCount()
{
	if (!GEditor) return 0;
	USelection* Sel = GEditor->GetSelectedActors();
	return Sel ? Sel->Num() : 0;
}

FBridgeActorBounds UUnrealBridgeLevelLibrary::GetSelectionBounds()
{
	FBridgeActorBounds Out;
	if (!GEditor) return Out;
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel) return Out;
	FBox Total(ForceInit);
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (!A) continue;
		FVector Origin, Extent;
		A->GetActorBounds(/*bOnlyColliding=*/ false, Origin, Extent, /*bIncludeChildren=*/ true);
		if (!Extent.IsNearlyZero())
		{
			Total += FBox::BuildAABB(Origin, Extent);
		}
	}
	if (!Total.IsValid) return Out;
	Out.Origin = Total.GetCenter();
	Out.BoxExtent = Total.GetExtent();
	Out.SphereRadius = Out.BoxExtent.Size();
	return Out;
}

FVector UUnrealBridgeLevelLibrary::GetSelectionCentroid()
{
	if (!GEditor) return FVector::ZeroVector;
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel) return FVector::ZeroVector;
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AActor* A = Cast<AActor>(*It))
		{
			Sum += A->GetActorLocation();
			++Count;
		}
	}
	return Count > 0 ? (Sum / Count) : FVector::ZeroVector;
}

TArray<FString> UUnrealBridgeLevelLibrary::GetSelectionClassSet()
{
	TArray<FString> Out;
	if (!GEditor) return Out;
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel) return Out;
	TSet<FString> Unique;
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AActor* A = Cast<AActor>(*It))
		{
			Unique.Add(A->GetClass()->GetName());
		}
	}
	Out.Reserve(Unique.Num());
	for (const FString& S : Unique) Out.Add(S);
	Out.Sort();
	return Out;
}

int32 UUnrealBridgeLevelLibrary::SelectActorsInSphere(FVector Center, float Radius, bool bAddToSelection)
{
	if (!GEditor) return 0;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	const float RadSq = FMath::Max(Radius, 0.0f) * FMath::Max(Radius, 0.0f);
	if (!bAddToSelection)
	{
		GEditor->SelectNone(false, true, false);
	}
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && FVector::DistSquared(A->GetActorLocation(), Center) <= RadSq)
		{
			GEditor->SelectActor(A, true, true, true, false);
			++Count;
		}
	}
	GEditor->NoteSelectionChange();
	return Count;
}

int32 UUnrealBridgeLevelLibrary::MirrorActors(const TArray<FString>& ActorNames, const FString& Axis)
{
	const FString L = Axis.ToLower();
	int32 AxisIdx = -1;
	if (L == TEXT("x")) AxisIdx = 0;
	else if (L == TEXT("y")) AxisIdx = 1;
	else if (L == TEXT("z")) AxisIdx = 2;
	else return 0;

	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return 0;
	FScopedTransaction Tr(LOCTEXT("MirrorActors", "Mirror Actors"));
	int32 Count = 0;
	for (const FString& Name : ActorNames)
	{
		AActor* A = BridgeLevelImpl::FindActor(World, Name);
		if (!A) continue;
		A->Modify();
		FVector Loc = A->GetActorLocation();
		FVector Scl = A->GetActorScale3D();
		Loc[AxisIdx] = -Loc[AxisIdx];
		Scl[AxisIdx] = -Scl[AxisIdx];
		A->SetActorLocation(Loc);
		A->SetActorScale3D(Scl);
		++Count;
	}
	return Count;
}

bool UUnrealBridgeLevelLibrary::SetComponentRelativeTransform(
	const FString& ActorName,
	const FString& ComponentName,
	FVector Location,
	FRotator Rotation,
	FVector Scale)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return false;
	}
	USceneComponent* SC = Cast<USceneComponent>(BridgeLevelImpl::FindComponent(Actor, ComponentName));
	if (!SC)
	{
		return false;
	}
	FScopedTransaction Tr(LOCTEXT("SetComponentRelativeTransform", "Set Component Relative Transform"));
	Actor->Modify();
	SC->Modify();
	SC->SetRelativeLocation(Location);
	SC->SetRelativeRotation(Rotation);
	SC->SetRelativeScale3D(Scale);
	return true;
}

int32 UUnrealBridgeLevelLibrary::ResetActorMaterials(const FString& ActorName)
{
	AActor* Actor = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!Actor)
	{
		return 0;
	}
	TArray<UMeshComponent*> MeshComps;
	Actor->GetComponents<UMeshComponent>(MeshComps);
	FScopedTransaction Tr(LOCTEXT("ResetActorMaterials", "Reset Actor Materials"));
	int32 Cleared = 0;
	for (UMeshComponent* MC : MeshComps)
	{
		if (!MC) continue;
		const int32 Num = MC->GetNumMaterials();
		bool bTouched = false;
		for (int32 i = 0; i < Num; ++i)
		{
			if (MC->OverrideMaterials.IsValidIndex(i) && MC->OverrideMaterials[i] != nullptr)
			{
				if (!bTouched)
				{
					MC->Modify();
					bTouched = true;
				}
				MC->SetMaterial(i, nullptr);
				++Cleared;
			}
		}
		if (bTouched)
		{
			Actor->Modify();
		}
	}
	return Cleared;
}

#undef LOCTEXT_NAMESPACE

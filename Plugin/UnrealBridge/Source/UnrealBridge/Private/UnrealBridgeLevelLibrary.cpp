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
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Algo/Reverse.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "PreviewScene.h"
#include "Components/DirectionalLightComponent.h"
#include "RenderingThread.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "HAL/PlatformFileManager.h"

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

	/** Pick the world most useful for runtime queries: prefer the live PIE
	 *  world when Play-in-Editor is active, otherwise fall back to the
	 *  editor world. Agents running inside PIE should hit dynamic objects
	 *  (spawned actors, moving platforms, destructible walls) — all of which
	 *  only exist in the PIE world — not the frozen editor copy. */
	UWorld* GetRuntimeWorld()
	{
		if (GEditor)
		{
			for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
			{
				if (Ctx.WorldType == EWorldType::PIE && Ctx.World() && Ctx.World()->HasBegunPlay())
				{
					return Ctx.World();
				}
			}
		}
		return GetEditorWorld();
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
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
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

bool UUnrealBridgeLevelLibrary::GetHeightAt(
	float X, float Y, float ZStart, float ZEnd,
	FString& OutActorLabel, float& OutGroundZ)
{
	OutActorLabel.Reset();
	OutGroundZ = ZEnd;

	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	if (!World)
	{
		return false;
	}
	const FVector Start(X, Y, ZStart);
	const FVector End(X, Y, ZEnd);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeGetHeightAt), /*bTraceComplex*/ true);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}
	if (AActor* A = Hit.GetActor())
	{
		OutActorLabel = A->GetActorLabel();
	}
	OutGroundZ = Hit.ImpactPoint.Z;
	return true;
}

int32 UUnrealBridgeLevelLibrary::GetHeightProfileAlong(
	const FVector& StartXY, const FVector& EndXY,
	int32 SampleCount, float ZStart, float ZEnd,
	TArray<float>& OutHeights, TArray<FString>& OutActorLabels)
{
	OutHeights.Reset();
	OutActorLabels.Reset();
	if (SampleCount < 2)
	{
		return 0;
	}
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	if (!World)
	{
		return 0;
	}
	OutHeights.Reserve(SampleCount);
	OutActorLabels.Reserve(SampleCount);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeHeightProfile), /*bTraceComplex*/ true);
	for (int32 i = 0; i < SampleCount; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(SampleCount - 1);
		const float SX = FMath::Lerp(StartXY.X, EndXY.X, T);
		const float SY = FMath::Lerp(StartXY.Y, EndXY.Y, T);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(
			Hit, FVector(SX, SY, ZStart), FVector(SX, SY, ZEnd), ECC_Visibility, Params);
		if (bHit)
		{
			OutHeights.Add(Hit.ImpactPoint.Z);
			OutActorLabels.Add(Hit.GetActor() ? Hit.GetActor()->GetActorLabel() : FString());
		}
		else
		{
			OutHeights.Add(ZEnd);
			OutActorLabels.Add(FString());
		}
	}
	return SampleCount;
}

float UUnrealBridgeLevelLibrary::MeasureCeilingHeight(const FVector& Origin, float MaxUp)
{
	const float SafeMax = FMath::Max(MaxUp, 0.0f);
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	if (!World)
	{
		return SafeMax;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeMeasureCeiling), /*bTraceComplex*/ true);
	const FVector End = Origin + FVector(0, 0, SafeMax);
	if (!World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
	{
		return SafeMax;
	}
	return Hit.Distance;
}

int32 UUnrealBridgeLevelLibrary::ProbeFanXY(const FVector& Origin,
	int32 NumRays, float MaxDistance,
	float StartAngleDeg, float SpanDeg,
	TArray<float>& OutDistances, TArray<FString>& OutActorLabels)
{
	OutDistances.Reset();
	OutActorLabels.Reset();
	if (NumRays <= 0)
	{
		return 0;
	}
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	if (!World)
	{
		return 0;
	}
	OutDistances.Reserve(NumRays);
	OutActorLabels.Reserve(NumRays);
	const float SafeDist = FMath::Max(MaxDistance, 1.0f);
	// If SpanDeg is 360 we want evenly spaced rays without double-counting the
	// last angle; otherwise we want both endpoints included.
	const bool bFullCircle = FMath::IsNearlyEqual(FMath::Abs(SpanDeg), 360.0f);
	const float Divisor = (bFullCircle || NumRays == 1) ? NumRays : (NumRays - 1);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeProbeFan), /*bTraceComplex*/ true);
	for (int32 i = 0; i < NumRays; ++i)
	{
		const float Deg = StartAngleDeg + (SpanDeg * i) / Divisor;
		const float Rad = FMath::DegreesToRadians(Deg);
		const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f);
		const FVector End = Origin + Dir * SafeDist;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
		{
			OutDistances.Add(Hit.Distance);
			OutActorLabels.Add(Hit.GetActor() ? Hit.GetActor()->GetActorLabel() : FString());
		}
		else
		{
			OutDistances.Add(SafeDist);
			OutActorLabels.Add(FString());
		}
	}
	return NumRays;
}

// ─── NavGraph state + Dijkstra ─────────────────────────────────────────

namespace BridgeLevelImpl
{
	struct FNavNode
	{
		FVector Location = FVector::ZeroVector;
		TMap<FString, float> OutEdges;   // to_name -> cost
	};

	// Process-global so the graph survives across bridge exec calls.
	static TMap<FString, FNavNode> GNavGraph;

	/** Classic Dijkstra on `GNavGraph`. Returns the ordered node path
	 *  (inclusive of From and To) and total cost, or empty + INF on failure. */
	static TArray<FString> DijkstraPath(const FString& From, const FString& To, float& OutTotal)
	{
		OutTotal = TNumericLimits<float>::Max();
		TArray<FString> Empty;
		if (!GNavGraph.Contains(From) || !GNavGraph.Contains(To))
		{
			return Empty;
		}
		if (From == To)
		{
			OutTotal = 0.0f;
			return { From };
		}

		TMap<FString, float> Dist;
		TMap<FString, FString> Prev;
		for (const auto& Pair : GNavGraph)
		{
			Dist.Add(Pair.Key, TNumericLimits<float>::Max());
		}
		Dist[From] = 0.0f;

		// Simple priority set — pull minimum each iteration. N^2 works fine
		// for the node counts an agent builds up (tens to hundreds).
		TSet<FString> Unvisited;
		for (const auto& Pair : GNavGraph) { Unvisited.Add(Pair.Key); }

		while (Unvisited.Num() > 0)
		{
			FString U;
			float Best = TNumericLimits<float>::Max();
			for (const FString& Name : Unvisited)
			{
				if (Dist[Name] < Best)
				{
					Best = Dist[Name];
					U = Name;
				}
			}
			if (U.IsEmpty() || Best == TNumericLimits<float>::Max())
			{
				break; // remaining nodes unreachable
			}
			Unvisited.Remove(U);
			if (U == To) { break; }

			const FNavNode& UNode = GNavGraph[U];
			for (const auto& Edge : UNode.OutEdges)
			{
				const FString& V = Edge.Key;
				if (!Unvisited.Contains(V)) continue;
				const float Alt = Dist[U] + Edge.Value;
				if (Alt < Dist[V])
				{
					Dist[V] = Alt;
					Prev.Add(V, U);
				}
			}
		}

		if (Dist[To] == TNumericLimits<float>::Max())
		{
			return Empty;
		}
		// Reconstruct path by walking Prev backwards from To.
		TArray<FString> Reverse;
		FString Cur = To;
		Reverse.Add(Cur);
		while (Cur != From)
		{
			const FString* P = Prev.Find(Cur);
			if (!P) { return Empty; }
			Cur = *P;
			Reverse.Add(Cur);
		}
		Algo::Reverse(Reverse);
		OutTotal = Dist[To];
		return Reverse;
	}
}

void UUnrealBridgeLevelLibrary::NavGraphClear()
{
	BridgeLevelImpl::GNavGraph.Reset();
}

bool UUnrealBridgeLevelLibrary::NavGraphAddNode(const FString& Name, const FVector& Location)
{
	if (Name.IsEmpty()) return false;
	const bool bIsNew = !BridgeLevelImpl::GNavGraph.Contains(Name);
	BridgeLevelImpl::FNavNode& Node = BridgeLevelImpl::GNavGraph.FindOrAdd(Name);
	Node.Location = Location;
	return bIsNew;
}

bool UUnrealBridgeLevelLibrary::NavGraphAddEdge(const FString& From, const FString& To, float Cost)
{
	if (From.IsEmpty() || To.IsEmpty()) return false;
	BridgeLevelImpl::FNavNode* FromN = BridgeLevelImpl::GNavGraph.Find(From);
	BridgeLevelImpl::FNavNode* ToN = BridgeLevelImpl::GNavGraph.Find(To);
	if (!FromN || !ToN) return false;
	FromN->OutEdges.Add(To, FMath::Max(Cost, 0.0f));
	return true;
}

TArray<FString> UUnrealBridgeLevelLibrary::NavGraphListNodes()
{
	TArray<FString> Names;
	BridgeLevelImpl::GNavGraph.GetKeys(Names);
	return Names;
}

bool UUnrealBridgeLevelLibrary::NavGraphGetNodeLocation(const FString& Name, FVector& OutLocation)
{
	OutLocation = FVector::ZeroVector;
	if (const BridgeLevelImpl::FNavNode* Node = BridgeLevelImpl::GNavGraph.Find(Name))
	{
		OutLocation = Node->Location;
		return true;
	}
	return false;
}

TArray<FString> UUnrealBridgeLevelLibrary::NavGraphShortestPath(const FString& From, const FString& To, float& OutTotalCost)
{
	return BridgeLevelImpl::DijkstraPath(From, To, OutTotalCost);
}

bool UUnrealBridgeLevelLibrary::NavGraphSaveJson(const FString& FilePath)
{
	if (FilePath.IsEmpty()) return false;

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> NodesJson;
	TArray<TSharedPtr<FJsonValue>> EdgesJson;
	for (const auto& Pair : BridgeLevelImpl::GNavGraph)
	{
		TSharedRef<FJsonObject> N = MakeShared<FJsonObject>();
		N->SetStringField(TEXT("name"), Pair.Key);
		N->SetNumberField(TEXT("x"), Pair.Value.Location.X);
		N->SetNumberField(TEXT("y"), Pair.Value.Location.Y);
		N->SetNumberField(TEXT("z"), Pair.Value.Location.Z);
		NodesJson.Add(MakeShared<FJsonValueObject>(N));

		for (const auto& Edge : Pair.Value.OutEdges)
		{
			TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
			E->SetStringField(TEXT("from"), Pair.Key);
			E->SetStringField(TEXT("to"), Edge.Key);
			E->SetNumberField(TEXT("cost"), Edge.Value);
			EdgesJson.Add(MakeShared<FJsonValueObject>(E));
		}
	}
	Root->SetArrayField(TEXT("nodes"), NodesJson);
	Root->SetArrayField(TEXT("edges"), EdgesJson);

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Serialized, *FilePath);
}

bool UUnrealBridgeLevelLibrary::NavGraphLoadJson(const FString& FilePath)
{
	if (FilePath.IsEmpty()) return false;
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	BridgeLevelImpl::GNavGraph.Reset();
	const TArray<TSharedPtr<FJsonValue>>* NodesArr;
	if (Root->TryGetArrayField(TEXT("nodes"), NodesArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *NodesArr)
		{
			const TSharedPtr<FJsonObject>* Obj;
			if (!Val->TryGetObject(Obj)) continue;
			const FString Name = (*Obj)->GetStringField(TEXT("name"));
			if (Name.IsEmpty()) continue;
			FVector Loc(
				(*Obj)->GetNumberField(TEXT("x")),
				(*Obj)->GetNumberField(TEXT("y")),
				(*Obj)->GetNumberField(TEXT("z")));
			BridgeLevelImpl::FNavNode& N = BridgeLevelImpl::GNavGraph.FindOrAdd(Name);
			N.Location = Loc;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* EdgesArr;
	if (Root->TryGetArrayField(TEXT("edges"), EdgesArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *EdgesArr)
		{
			const TSharedPtr<FJsonObject>* Obj;
			if (!Val->TryGetObject(Obj)) continue;
			const FString From = (*Obj)->GetStringField(TEXT("from"));
			const FString To = (*Obj)->GetStringField(TEXT("to"));
			const double Cost = (*Obj)->GetNumberField(TEXT("cost"));
			if (From.IsEmpty() || To.IsEmpty()) continue;
			BridgeLevelImpl::FNavNode* FromN = BridgeLevelImpl::GNavGraph.Find(From);
			if (FromN && BridgeLevelImpl::GNavGraph.Contains(To))
			{
				FromN->OutEdges.Add(To, static_cast<float>(FMath::Max(Cost, 0.0)));
			}
		}
	}
	return true;
}

// ─── Vision: SceneCapture2D → PNG ──────────────────────────────────────

namespace BridgeLevelImpl
{
	/** When a render target reads back with RGB but zero alpha (a common
	 *  SceneCapture2D quirk on the PNG path), force alpha to 255 so saved
	 *  files aren't transparent. Adapted from LocomotionOrthoTileCapture. */
	static void FixZeroAlphaWhenRgbVisible(FColor* Pixels, int32 Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			FColor& C = Pixels[i];
			if (C.A == 0 && FMath::Max3(C.R, C.G, C.B) > 4)
			{
				C.A = 255;
			}
		}
	}

	/** Save an FImage to PNG, creating parent directories as needed. */
	static bool SaveImageToPng(const FImage& Image, const FString& FilePath)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FilePath));
		return FImageUtils::SaveImageByExtension(*FilePath, Image, /*CompressionQuality=*/ 0);
	}

	/** Core capture routine shared by the ortho + perspective wrappers.
	 *  Spawns a transient SceneCapture2D, issues a one-off CaptureScene(),
	 *  flushes the render thread, reads pixels, writes PNG, destroys the
	 *  capture actor. Returns false on any failure. */
	static bool CaptureSceneToPng(
		UWorld* World,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		bool bOrthographic,
		float OrthoWidth,
		float FOVDeg,
		int32 PixelWidth,
		int32 PixelHeight,
		const FString& OutPngPath)
	{
		if (!World || PixelWidth <= 0 || PixelHeight <= 0 || OutPngPath.IsEmpty())
		{
			return false;
		}
		if (bOrthographic && OrthoWidth <= 0.0f) return false;
		if (!bOrthographic && (FOVDeg <= 0.0f || FOVDeg >= 180.0f)) return false;

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		SpawnInfo.Name = MakeUniqueObjectName(World, ASceneCapture2D::StaticClass(), TEXT("BridgeCaptureCam"));

		ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
			CameraLocation, CameraRotation, SpawnInfo);
		if (!CaptureActor || !CaptureActor->GetCaptureComponent2D())
		{
			return false;
		}

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass(), TEXT("BridgeCaptureRT")));
		RT->ClearColor = FLinearColor::Black;
		RT->bAutoGenerateMips = false;
		RT->InitCustomFormat(PixelWidth, PixelHeight, PF_B8G8R8A8, /*bForceLinearGamma=*/ false);
		RT->UpdateResourceImmediate(true);

		USceneCaptureComponent2D* SCC = CaptureActor->GetCaptureComponent2D();
		SCC->ProjectionType = bOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
		if (bOrthographic)
		{
			SCC->OrthoWidth = OrthoWidth;
		}
		else
		{
			SCC->FOVAngle = FOVDeg;
		}
		SCC->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		SCC->bCaptureEveryFrame = false;
		SCC->bCaptureOnMovement = false;
		SCC->TextureTarget = RT;

		SCC->CaptureScene();
		FlushRenderingCommands();

		TArray<FColor> Pixels;
		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
		if (!Res)
		{
			CaptureActor->Destroy();
			return false;
		}
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
		ReadFlags.SetLinearToGamma(false);
		if (!Res->ReadPixels(Pixels, ReadFlags) || Pixels.Num() != PixelWidth * PixelHeight)
		{
			CaptureActor->Destroy();
			return false;
		}

		FixZeroAlphaWhenRgbVisible(Pixels.GetData(), Pixels.Num());

		FImage Img;
		Img.Init(PixelWidth, PixelHeight, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		// Flip vertically on copy: SceneCapture2D readback lands rows bottom-
		// up in ReadPixels (GPU viewport origin convention), but PNG expects
		// rows top-down. Without this perspective shots render upside-down.
		FColor* Dst = reinterpret_cast<FColor*>(Img.RawData.GetData());
		const FColor* Src = Pixels.GetData();
		for (int32 Y = 0; Y < PixelHeight; ++Y)
		{
			FMemory::Memcpy(
				Dst + Y * PixelWidth,
				Src + (PixelHeight - 1 - Y) * PixelWidth,
				PixelWidth * sizeof(FColor));
		}

		const bool bSaved = SaveImageToPng(Img, OutPngPath);
		CaptureActor->Destroy();
		return bSaved;
	}
}

bool UUnrealBridgeLevelLibrary::CaptureOrthoTopDown(
	const FVector& Center, float WorldSize,
	int32 Width, int32 Height, const FString& FilePath,
	float CameraHeight)
{
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	const FVector CamLoc(Center.X, Center.Y, Center.Z + CameraHeight);
	const FRotator CamRot(-90.0f, 0.0f, 0.0f);  // look straight down
	return BridgeLevelImpl::CaptureSceneToPng(
		World, CamLoc, CamRot,
		/*bOrthographic=*/ true,
		/*OrthoWidth=*/ WorldSize,
		/*FOVDeg=*/ 0.0f,
		Width, Height, FilePath);
}

bool UUnrealBridgeLevelLibrary::CaptureFromPose(
	const FVector& CameraLocation, const FRotator& CameraRotation,
	float FOVDeg, int32 Width, int32 Height, const FString& FilePath)
{
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	return BridgeLevelImpl::CaptureSceneToPng(
		World, CameraLocation, CameraRotation,
		/*bOrthographic=*/ false,
		/*OrthoWidth=*/ 0.0f,
		FOVDeg,
		Width, Height, FilePath);
}

// ─── Anim pose grid capture (isolated FPreviewScene) ──────────

namespace BridgeLevelImpl
{
	/** Offset direction from mesh centre for a named view (pre-normalised). */
	static bool ResolveViewDirection(const FString& Name, FVector& OutDir)
	{
		if (Name.Equals(TEXT("Front"),        ESearchCase::IgnoreCase)) { OutDir = FVector( 1,  0,  0);                       return true; }
		if (Name.Equals(TEXT("Back"),         ESearchCase::IgnoreCase)) { OutDir = FVector(-1,  0,  0);                       return true; }
		if (Name.Equals(TEXT("Side"),         ESearchCase::IgnoreCase) ||
			Name.Equals(TEXT("SideRight"),    ESearchCase::IgnoreCase)) { OutDir = FVector( 0,  1,  0);                       return true; }
		if (Name.Equals(TEXT("SideLeft"),     ESearchCase::IgnoreCase)) { OutDir = FVector( 0, -1,  0);                       return true; }
		if (Name.Equals(TEXT("ThreeQuarter"), ESearchCase::IgnoreCase)) { OutDir = FVector( 1,  1, 0.35f).GetSafeNormal();    return true; }
		if (Name.Equals(TEXT("Top"),          ESearchCase::IgnoreCase)) { OutDir = FVector( 0,  0,  1);                       return true; }
		if (Name.Equals(TEXT("Bottom"),       ESearchCase::IgnoreCase)) { OutDir = FVector( 0,  0, -1);                       return true; }
		return false;
	}

	/** Drive the skel mesh component's pose to a specific anim time. */
	static void ApplyPoseAtTime(USkeletalMeshComponent* SkelComp, UAnimSequenceBase* Anim, float Time)
	{
		if (!SkelComp || !Anim) return;
		const float ClampedTime = FMath::Clamp(Time, 0.0f, Anim->GetPlayLength());

		// Only create the AnimInstance once; reuse it for subsequent poses.
		// Calling PlayAnimation in a loop creates a new AnimSingleNodeInstance
		// each time, and the fresh instance's update counter/state may cause
		// RefreshBoneTransforms to evaluate stale data.
		if (!SkelComp->GetAnimInstance())
		{
			SkelComp->PlayAnimation(Anim, /*bLooping=*/ false);
		}
		SkelComp->SetPosition(ClampedTime, /*bFireNotifies=*/ false);

		++GFrameCounter;
		// TickAnimation propagates SetPosition into the AnimInstance's
		// evaluation state and marks it as needing a new evaluation.
		SkelComp->TickAnimation(0.f, /*bNeedsValidRootMotion=*/ false);
		SkelComp->RefreshBoneTransforms();
		SkelComp->UpdateChildTransforms();
		SkelComp->UpdateBounds();
	}

	/** Flush bone transforms to the render proxy so CaptureScene sees them. */
	static void FlushPoseToRenderProxy(USkeletalMeshComponent* SkelComp)
	{
		if (!SkelComp) return;
		SkelComp->MarkRenderDynamicDataDirty();
		if (UWorld* World = SkelComp->GetWorld())
		{
			World->SendAllEndOfFrameUpdates();
		}
		FlushRenderingCommands();
	}

	/** Index of a pelvis-equivalent bone in the ref skeleton, or INDEX_NONE. */
	static int32 FindPelvisBoneIndex(const FReferenceSkeleton& RefSkel, int32 ValidBoneCount)
	{
		static const TCHAR* Candidates[] = {
			TEXT("pelvis"), TEXT("Pelvis"), TEXT("hips"), TEXT("Hips"),
			TEXT("spine_01"), TEXT("spine"), TEXT("root"), TEXT("Root"),
		};
		for (const TCHAR* Name : Candidates)
		{
			const int32 Idx = RefSkel.FindBoneIndex(FName(Name));
			if (Idx != INDEX_NONE && Idx < ValidBoneCount) return Idx;
		}
		return INDEX_NONE;
	}

	/**
	 * Compute camera target + radius for the CURRENTLY posed skel mesh.
	 * Centre prefers pelvis; radius uses the max corner distance of the
	 * bone AABB from Centre, padded 1.2× for skin stretch.
	 */
	static void ComputePoseFraming(
		USkeletalMesh* Mesh, USkeletalMeshComponent* SkelComp,
		FVector& OutCentre, float& OutRadius)
	{
		OutCentre = FVector::ZeroVector;
		OutRadius = 100.0f;

		const TArray<FTransform>& CSBones = SkelComp->GetComponentSpaceTransforms();
		if (CSBones.Num() > 0 && Mesh)
		{
			const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
			const int32 PelvisIdx = FindPelvisBoneIndex(RefSkel, CSBones.Num());

			FBox BonesAABB(ForceInit);
			for (const FTransform& T : CSBones) { BonesAABB += T.GetLocation(); }

			OutCentre = (PelvisIdx != INDEX_NONE)
				? CSBones[PelvisIdx].GetLocation()
				: BonesAABB.GetCenter();
			const FVector ToMin = (BonesAABB.Min - OutCentre).GetAbs();
			const FVector ToMax = (BonesAABB.Max - OutCentre).GetAbs();
			const FVector ToCorner = ToMin.ComponentMax(ToMax);
			OutRadius = FMath::Max(10.0f, ToCorner.Size() * 1.2f);
		}
		else if (Mesh)
		{
			const FBoxSphereBounds Fallback = Mesh->GetBounds();
			OutCentre = Fallback.Origin;
			OutRadius = FMath::Max(10.0f, Fallback.SphereRadius);
		}
	}

	/** Capture one view of the preview world into an in-memory pixel buffer. */
	static bool CaptureViewToPixels(
		UWorld* World,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOVDeg, int32 PixelWidth, int32 PixelHeight,
		TArray<FColor>& OutPixels)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		SpawnInfo.Name = MakeUniqueObjectName(World, ASceneCapture2D::StaticClass(), TEXT("BridgeAnimPoseCam"));
		ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
			CameraLocation, CameraRotation, SpawnInfo);
		if (!CaptureActor || !CaptureActor->GetCaptureComponent2D()) return false;

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass(), TEXT("BridgeAnimPoseRT")));
		RT->ClearColor = FLinearColor(0.85f, 0.85f, 0.87f, 1.0f); // light grey background for contrast
		RT->bAutoGenerateMips = false;
		RT->InitCustomFormat(PixelWidth, PixelHeight, PF_B8G8R8A8, /*bForceLinearGamma=*/ false);
		RT->UpdateResourceImmediate(true);

		USceneCaptureComponent2D* SCC = CaptureActor->GetCaptureComponent2D();
		SCC->ProjectionType = ECameraProjectionMode::Perspective;
		SCC->FOVAngle = FOVDeg;
		// Final color with enhanced preview lighting — gives depth,
		// specular highlights and readable silhouettes.
		SCC->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		SCC->bCaptureEveryFrame = false;
		SCC->bCaptureOnMovement = false;
		SCC->ShowFlags.SetFog(false);
		SCC->ShowFlags.SetAtmosphere(false);
		SCC->ShowFlags.SetVolumetricFog(false);
		SCC->TextureTarget = RT;

		SCC->CaptureScene();
		FlushRenderingCommands();

		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
		if (!Res)
		{
			CaptureActor->Destroy();
			return false;
		}
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
		ReadFlags.SetLinearToGamma(false);
		if (!Res->ReadPixels(OutPixels, ReadFlags) || OutPixels.Num() != PixelWidth * PixelHeight)
		{
			CaptureActor->Destroy();
			return false;
		}
		FixZeroAlphaWhenRgbVisible(OutPixels.GetData(), OutPixels.Num());
		CaptureActor->Destroy();
		return true;
	}

	// ─── Preview-asset attachment helper ──────────────────────────
	//
	// Skeleton assets carry a PreviewAttachedAssetContainer that tells
	// Persona which meshes (weapons, shields, etc.) to mount on which
	// sockets during animation preview. We replicate that here so the
	// offline capture shows the same equipment the artist sees.

	static void AttachPreviewAssets(
		USkeletalMeshComponent* SkelComp,
		USkeleton* Skel,
		FPreviewScene& Scene)
	{
		if (!Skel || !SkelComp) return;

		for (auto It = Skel->PreviewAttachedAssetContainer.CreateConstIterator(); It; ++It)
		{
			UObject* Asset = It->GetAttachedObject();
			if (!Asset) continue;
			FName SocketOrBone = It->AttachedTo;

			if (UStaticMesh* SM = Cast<UStaticMesh>(Asset))
			{
				UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(
					GetTransientPackage(), UStaticMeshComponent::StaticClass());
				Comp->SetStaticMesh(SM);
				Scene.AddComponent(Comp, FTransform::Identity);
				Comp->AttachToComponent(SkelComp,
					FAttachmentTransformRules::SnapToTargetIncludingScale, SocketOrBone);
			}
			else if (USkeletalMesh* AttachMesh = Cast<USkeletalMesh>(Asset))
			{
				USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(
					GetTransientPackage(), USkeletalMeshComponent::StaticClass());
				Comp->SetSkeletalMesh(AttachMesh);
				Scene.AddComponent(Comp, FTransform::Identity);
				Comp->AttachToComponent(SkelComp,
					FAttachmentTransformRules::SnapToTargetIncludingScale, SocketOrBone);
			}
		}
	}

	// ─── Lighting enhancement helper ─────────────────────────────
	//
	// FPreviewScene's default lighting is a single weak directional
	// light + skylight without cubemap. We add a proper 3-point setup
	// so SCS_FinalColorLDR produces readable, well-lit captures.

	static void EnhancePreviewLighting(FPreviewScene& Scene)
	{
		// Key light — strong top-front-right for main shading.
		UDirectionalLightComponent* KeyLight = NewObject<UDirectionalLightComponent>(
			GetTransientPackage(), UDirectionalLightComponent::StaticClass());
		KeyLight->SetIntensity(4.0f);
		KeyLight->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));
		Scene.AddComponent(KeyLight, FTransform(FRotator(-45.0f, 30.0f, 0.0f)));

		// Fill light — softer from opposite side to reduce harsh shadows.
		UDirectionalLightComponent* FillLight = NewObject<UDirectionalLightComponent>(
			GetTransientPackage(), UDirectionalLightComponent::StaticClass());
		FillLight->SetIntensity(1.5f);
		FillLight->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f));
		Scene.AddComponent(FillLight, FTransform(FRotator(-30.0f, -150.0f, 0.0f)));

		// Rim/back light — subtle edge highlight for silhouette readability.
		UDirectionalLightComponent* RimLight = NewObject<UDirectionalLightComponent>(
			GetTransientPackage(), UDirectionalLightComponent::StaticClass());
		RimLight->SetIntensity(2.0f);
		RimLight->SetLightColor(FLinearColor(0.95f, 0.95f, 1.0f));
		Scene.AddComponent(RimLight, FTransform(FRotator(-20.0f, 180.0f, 0.0f)));
	}

	// ─── Bone overlay + per-view framing helpers ─────────────────

	struct FBoneChainDef
	{
		TArray<int32> BoneIndices;
		FColor Color = FColor::White;
	};

	static int32 FindFirstBone(
		const FReferenceSkeleton& RefSkel,
		const TCHAR* const* Names, int32 Count, int32 ValidBoneCount)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 Idx = RefSkel.FindBoneIndex(FName(Names[i]));
			if (Idx != INDEX_NONE && Idx < ValidBoneCount) return Idx;
		}
		return INDEX_NONE;
	}

	/**
	 * Resolve canonical bone names on the skeleton (pelvis/spine/arms/legs),
	 * producing ordered parent→child chains with a colour per group. Missing
	 * bones are skipped — a chain shorter than 2 is dropped so the overlay
	 * degrades gracefully on non-humanoid rigs.
	 */
	static void BuildBoneChains(
		const FReferenceSkeleton& RefSkel, int32 ValidBoneCount,
		TArray<FBoneChainDef>& OutChains)
	{
		// Canonical name candidates with priority order. Covers UE Mannequin
		// (`spine_01`/`upperarm_l`), Mixamo (`LeftArm`), and Naughty Dog
		// naming (`spinea`, `l_shoulder`, `l_upper_leg`) seen in TLOU rigs.
		static const TCHAR* SpineSeg1[] = { TEXT("pelvis"), TEXT("Pelvis"), TEXT("hips"), TEXT("Hips"), TEXT("root"), TEXT("Root") };
		static const TCHAR* SpineSeg2[] = { TEXT("spine_01"), TEXT("Spine_01"), TEXT("spinea"), TEXT("SpineA"), TEXT("spine"), TEXT("Spine") };
		static const TCHAR* SpineSeg3[] = { TEXT("spine_03"), TEXT("Spine_03"), TEXT("spined"), TEXT("SpineD"), TEXT("spine_02"), TEXT("Spine_02"), TEXT("spinec"), TEXT("SpineC") };
		static const TCHAR* SpineSeg4[] = { TEXT("neck_01"), TEXT("Neck_01"), TEXT("neck"), TEXT("Neck") };
		static const TCHAR* SpineSeg5[] = { TEXT("head"), TEXT("Head"), TEXT("heada"), TEXT("HeadA") };

		static const TCHAR* LClav[]  = { TEXT("clavicle_l"), TEXT("Clavicle_L"), TEXT("l_clavicle"), TEXT("L_Clavicle") };
		static const TCHAR* LUpper[] = { TEXT("upperarm_l"), TEXT("UpperArm_L"), TEXT("l_shoulder"), TEXT("L_Shoulder"), TEXT("shoulder_l"), TEXT("Shoulder_L"), TEXT("LeftArm") };
		static const TCHAR* LLower[] = { TEXT("lowerarm_l"), TEXT("LowerArm_L"), TEXT("l_elbow"), TEXT("L_Elbow"), TEXT("forearm_l"), TEXT("Forearm_L"), TEXT("LeftForeArm") };
		static const TCHAR* LHand[]  = { TEXT("hand_l"), TEXT("Hand_L"), TEXT("l_wrist"), TEXT("L_Wrist"), TEXT("LeftHand") };

		static const TCHAR* RClav[]  = { TEXT("clavicle_r"), TEXT("Clavicle_R"), TEXT("r_clavicle"), TEXT("R_Clavicle") };
		static const TCHAR* RUpper[] = { TEXT("upperarm_r"), TEXT("UpperArm_R"), TEXT("r_shoulder"), TEXT("R_Shoulder"), TEXT("shoulder_r"), TEXT("Shoulder_R"), TEXT("RightArm") };
		static const TCHAR* RLower[] = { TEXT("lowerarm_r"), TEXT("LowerArm_R"), TEXT("r_elbow"), TEXT("R_Elbow"), TEXT("forearm_r"), TEXT("Forearm_R"), TEXT("RightForeArm") };
		static const TCHAR* RHand[]  = { TEXT("hand_r"), TEXT("Hand_R"), TEXT("r_wrist"), TEXT("R_Wrist"), TEXT("RightHand") };

		static const TCHAR* PelvisAlias[] = { TEXT("pelvis"), TEXT("Pelvis"), TEXT("hips"), TEXT("Hips") };
		static const TCHAR* LThigh[] = { TEXT("thigh_l"), TEXT("Thigh_L"), TEXT("l_upper_leg"), TEXT("L_Upper_Leg"), TEXT("LeftUpLeg") };
		static const TCHAR* LCalf[]  = { TEXT("calf_l"), TEXT("Calf_L"), TEXT("l_knee"), TEXT("L_Knee"), TEXT("LeftLeg"), TEXT("shin_l"), TEXT("Shin_L") };
		static const TCHAR* LFoot[]  = { TEXT("foot_l"), TEXT("Foot_L"), TEXT("l_ankle"), TEXT("L_Ankle"), TEXT("LeftFoot") };

		static const TCHAR* RThigh[] = { TEXT("thigh_r"), TEXT("Thigh_R"), TEXT("r_upper_leg"), TEXT("R_Upper_Leg"), TEXT("RightUpLeg") };
		static const TCHAR* RCalf[]  = { TEXT("calf_r"), TEXT("Calf_R"), TEXT("r_knee"), TEXT("R_Knee"), TEXT("RightLeg"), TEXT("shin_r"), TEXT("Shin_R") };
		static const TCHAR* RFoot[]  = { TEXT("foot_r"), TEXT("Foot_R"), TEXT("r_ankle"), TEXT("R_Ankle"), TEXT("RightFoot") };

		#define BRIDGE_FIND(arr) FindFirstBone(RefSkel, arr, UE_ARRAY_COUNT(arr), ValidBoneCount)

		auto AppendBone = [&](FBoneChainDef& Chain, int32 Idx)
		{
			if (Idx != INDEX_NONE) Chain.BoneIndices.Add(Idx);
		};
		auto Finalise = [&](FBoneChainDef& Chain)
		{
			if (Chain.BoneIndices.Num() >= 2) OutChains.Add(MoveTemp(Chain));
		};

		// Spine — cyan
		{
			FBoneChainDef C; C.Color = FColor(0, 220, 255, 255);
			AppendBone(C, BRIDGE_FIND(SpineSeg1));
			AppendBone(C, BRIDGE_FIND(SpineSeg2));
			AppendBone(C, BRIDGE_FIND(SpineSeg3));
			AppendBone(C, BRIDGE_FIND(SpineSeg4));
			AppendBone(C, BRIDGE_FIND(SpineSeg5));
			Finalise(C);
		}
		// Left arm — yellow
		{
			FBoneChainDef C; C.Color = FColor(255, 220, 0, 255);
			AppendBone(C, BRIDGE_FIND(LClav));
			AppendBone(C, BRIDGE_FIND(LUpper));
			AppendBone(C, BRIDGE_FIND(LLower));
			AppendBone(C, BRIDGE_FIND(LHand));
			Finalise(C);
		}
		// Right arm — orange (distinguishable from left under vision analysis)
		{
			FBoneChainDef C; C.Color = FColor(255, 140, 0, 255);
			AppendBone(C, BRIDGE_FIND(RClav));
			AppendBone(C, BRIDGE_FIND(RUpper));
			AppendBone(C, BRIDGE_FIND(RLower));
			AppendBone(C, BRIDGE_FIND(RHand));
			Finalise(C);
		}
		// Left leg — magenta
		{
			FBoneChainDef C; C.Color = FColor(255, 60, 220, 255);
			AppendBone(C, BRIDGE_FIND(PelvisAlias));
			AppendBone(C, BRIDGE_FIND(LThigh));
			AppendBone(C, BRIDGE_FIND(LCalf));
			AppendBone(C, BRIDGE_FIND(LFoot));
			Finalise(C);
		}
		// Right leg — purple
		{
			FBoneChainDef C; C.Color = FColor(160, 60, 255, 255);
			AppendBone(C, BRIDGE_FIND(PelvisAlias));
			AppendBone(C, BRIDGE_FIND(RThigh));
			AppendBone(C, BRIDGE_FIND(RCalf));
			AppendBone(C, BRIDGE_FIND(RFoot));
			Finalise(C);
		}

		#undef BRIDGE_FIND
	}

	/** World-space position of every bone in the skel mesh component, appended. */
	static void CollectBoneWorldPositions(
		USkeletalMeshComponent* SkelComp, TArray<FVector>& Out)
	{
		if (!SkelComp) return;
		const TArray<FTransform>& CS = SkelComp->GetComponentSpaceTransforms();
		const FTransform CompToWorld = SkelComp->GetComponentTransform();
		Out.Reserve(Out.Num() + CS.Num() + 8);
		for (const FTransform& T : CS)
		{
			Out.Add(CompToWorld.TransformPosition(T.GetLocation()));
		}

		// Bone joints only capture the skeleton's origin points — the rendered
		// mesh silhouette extends past them (hair, hands, feet, hats, weapons,
		// cloth). Append the 8 corners of the posed mesh's world-space
		// bounding box so framing covers the actual visible extent, not just
		// the joint cloud.
		const FBoxSphereBounds Bounds = SkelComp->CalcBounds(CompToWorld);
		const FVector C = Bounds.Origin;
		const FVector E = Bounds.BoxExtent;
		for (int32 sx = -1; sx <= 1; sx += 2)
		for (int32 sy = -1; sy <= 1; sy += 2)
		for (int32 sz = -1; sz <= 1; sz += 2)
		{
			Out.Add(C + FVector(sx * E.X, sy * E.Y, sz * E.Z));
		}
	}

	/** Fill a filled square (axis-aligned) centred on (CX, CY). */
	static void DrawFilledSquareFColor(
		FColor* Pixels, int32 W, int32 H,
		int32 CX, int32 CY, int32 HalfSize, const FColor& Color)
	{
		for (int32 dy = -HalfSize; dy <= HalfSize; ++dy)
		{
			for (int32 dx = -HalfSize; dx <= HalfSize; ++dx)
			{
				const int32 px = CX + dx;
				const int32 py = CY + dy;
				if (px >= 0 && px < W && py >= 0 && py < H)
				{
					Pixels[py * W + px] = Color;
				}
			}
		}
	}

	/**
	 * Bresenham line into a BGRA pixel buffer (FColor). Thickness ≥ 1 is
	 * approximated by painting a square brush of (2·T+1)² at each step.
	 * Pixel writes are bounds-checked; no blending.
	 */
	static void DrawLineFColor(
		FColor* Pixels, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1,
		const FColor& Color, int32 Thickness)
	{
		const int32 dx = FMath::Abs(X1 - X0);
		const int32 dy = FMath::Abs(Y1 - Y0);
		const int32 sx = X0 < X1 ? 1 : -1;
		const int32 sy = Y0 < Y1 ? 1 : -1;
		int32 err = dx - dy;
		int32 x = X0, y = Y0;
		const int32 T = FMath::Max(0, Thickness / 2);
		const int32 MaxIter = (dx + dy + 2);
		for (int32 Guard = 0; Guard <= MaxIter; ++Guard)
		{
			for (int32 ty = -T; ty <= T; ++ty)
			{
				for (int32 tx = -T; tx <= T; ++tx)
				{
					const int32 px = x + tx;
					const int32 py = y + ty;
					if (px >= 0 && px < W && py >= 0 && py < H)
					{
						Pixels[py * W + px] = Color;
					}
				}
			}
			if (x == X1 && y == Y1) break;
			const int32 e2 = 2 * err;
			if (e2 > -dy) { err -= dy; x += sx; }
			if (e2 <  dx) { err += dx; y += sy; }
		}
	}

	/**
	 * Draw the bone chains on one cell's pixel buffer, projecting bone world
	 * positions through the SceneCapture's derived camera basis. Using
	 * FRotationMatrix(CamRot) to pull axes ensures the overlay matches UE's
	 * actual scene-capture orientation (including the implicit up-flip on
	 * top/bottom views where roll is ambiguous).
	 */
	static void DrawBoneOverlayOnCell(
		FColor* Pixels, int32 CellW, int32 CellH,
		USkeletalMeshComponent* SkelComp,
		const TArray<FBoneChainDef>& Chains,
		const FVector& CamLoc, const FRotator& CamRot, float FOVDeg)
	{
		if (!SkelComp || Chains.Num() == 0) return;
		const FRotationMatrix RotMat(CamRot);
		const FVector Forward = RotMat.GetUnitAxis(EAxis::X);
		const FVector Right   = RotMat.GetUnitAxis(EAxis::Y);
		const FVector Up      = RotMat.GetUnitAxis(EAxis::Z);
		const float HalfFovRadH = FMath::DegreesToRadians(FOVDeg * 0.5f);
		const float TanH = FMath::Tan(HalfFovRadH);
		const float TanV = TanH * static_cast<float>(CellH) / FMath::Max(1.0f, static_cast<float>(CellW));
		const TArray<FTransform>& CS = SkelComp->GetComponentSpaceTransforms();
		const FTransform CompToWorld = SkelComp->GetComponentTransform();

		auto Project = [&](const FVector& W, float& OX, float& OY) -> bool
		{
			const FVector Rel = W - CamLoc;
			const float D = FVector::DotProduct(Rel, Forward);
			if (D <= 1.0f) return false;  // behind / too close
			const float U = FVector::DotProduct(Rel, Right);
			const float V = FVector::DotProduct(Rel, Up);
			const float NdcX = U / (D * TanH);
			const float NdcY = V / (D * TanV);
			OX = (NdcX * 0.5f + 0.5f) * static_cast<float>(CellW);
			OY = (0.5f - NdcY * 0.5f) * static_cast<float>(CellH);
			return true;
		};

		for (const FBoneChainDef& Chain : Chains)
		{
			const int32 N = Chain.BoneIndices.Num();
			TArray<FVector2D> Pts;  Pts.SetNum(N);
			TArray<uint8>     Ok;   Ok.SetNum(N);
			for (int32 i = 0; i < N; ++i)
			{
				const int32 BoneIdx = Chain.BoneIndices[i];
				Ok[i] = 0;
				if (BoneIdx < 0 || BoneIdx >= CS.Num()) continue;
				const FVector WPos = CompToWorld.TransformPosition(CS[BoneIdx].GetLocation());
				float X, Y;
				if (Project(WPos, X, Y)) { Pts[i] = FVector2D(X, Y); Ok[i] = 1; }
			}
			// Chain segments (thicker so lines survive PNG compression).
			for (int32 i = 1; i < N; ++i)
			{
				if (!Ok[i - 1] || !Ok[i]) continue;
				DrawLineFColor(Pixels, CellW, CellH,
					FMath::RoundToInt(Pts[i - 1].X), FMath::RoundToInt(Pts[i - 1].Y),
					FMath::RoundToInt(Pts[i].X),     FMath::RoundToInt(Pts[i].Y),
					Chain.Color, /*Thickness=*/ 3);
			}
			// Joint dots (white, slightly larger than line thickness).
			for (int32 i = 0; i < N; ++i)
			{
				if (!Ok[i]) continue;
				DrawFilledSquareFColor(Pixels, CellW, CellH,
					FMath::RoundToInt(Pts[i].X), FMath::RoundToInt(Pts[i].Y),
					/*HalfSize=*/ 2, FColor(255, 255, 255, 255));
			}
		}
	}

	/**
	 * Per-view framing. For each requested direction, project all supplied
	 * bone world positions into the view's derived camera basis, compute a
	 * tight 2D bounding box + mean depth, then recentre the camera target
	 * and compute a distance that fits the bbox within the FOV. A consensus
	 * (max) distance is applied across all views so character scale stays
	 * equal between cells. Invalid directions (zero-vector) are left with
	 * fallback cam pose so the caller can skip them later without special
	 * handling.
	 */
	static void ComputePerViewFraming(
		const TArray<FVector>& BoneWorldPositions,
		const TArray<FVector>& ViewDirs,
		const FVector& FallbackCentre,
		float FallbackRadius,
		float FOVDeg, int32 CellW, int32 CellH,
		TArray<FVector>& OutCamLocs,
		TArray<FRotator>& OutCamRots)
	{
		const int32 NumViews = ViewDirs.Num();
		OutCamLocs.SetNum(NumViews);
		OutCamRots.SetNum(NumViews);
		if (NumViews == 0) return;

		const float HalfFovRadH = FMath::DegreesToRadians(FOVDeg * 0.5f);
		const float TanH = FMath::Tan(HalfFovRadH);
		const float TanV = TanH * static_cast<float>(CellH) / FMath::Max(1.0f, static_cast<float>(CellW));

		// Fallback distance for the legacy-style framing when no bones are given.
		const float FallbackDistance = FallbackRadius / FMath::Max(0.01f, TanH);

		if (BoneWorldPositions.Num() == 0)
		{
			for (int32 i = 0; i < NumViews; ++i)
			{
				const FVector Dir = ViewDirs[i].GetSafeNormal();
				OutCamLocs[i] = FallbackCentre + Dir * FallbackDistance;
				OutCamRots[i] = (FallbackCentre - OutCamLocs[i]).Rotation();
			}
			return;
		}

		struct FViewState { FVector Target; FVector Forward; float ReqDist; bool bValid; };
		TArray<FViewState> States; States.SetNum(NumViews);
		float Consensus = 0.0f;

		for (int32 i = 0; i < NumViews; ++i)
		{
			FVector Dir = ViewDirs[i];
			if (Dir.IsNearlyZero())
			{
				States[i].bValid = false;
				continue;
			}
			Dir = Dir.GetSafeNormal();

			// Seed camera far enough out that every bone lies in front of it.
			const float SeedDist = FMath::Max(500.0f, FallbackRadius * 4.0f);
			const FVector SeedCamLoc = FallbackCentre + Dir * SeedDist;
			const FRotator SeedRot = (FallbackCentre - SeedCamLoc).Rotation();
			const FRotationMatrix RotMat(SeedRot);
			const FVector Forward = RotMat.GetUnitAxis(EAxis::X);
			const FVector Right   = RotMat.GetUnitAxis(EAxis::Y);
			const FVector Up      = RotMat.GetUnitAxis(EAxis::Z);

			float MinU = FLT_MAX, MaxU = -FLT_MAX;
			float MinV = FLT_MAX, MaxV = -FLT_MAX;
			double SumD = 0.0;
			for (const FVector& P : BoneWorldPositions)
			{
				const FVector Rel = P - SeedCamLoc;
				const float U = FVector::DotProduct(Rel, Right);
				const float V = FVector::DotProduct(Rel, Up);
				const float D = FVector::DotProduct(Rel, Forward);
				MinU = FMath::Min(MinU, U); MaxU = FMath::Max(MaxU, U);
				MinV = FMath::Min(MinV, V); MaxV = FMath::Max(MaxV, V);
				SumD += D;
			}
			const float MeanD = static_cast<float>(SumD / BoneWorldPositions.Num());
			const float MidU  = 0.5f * (MinU + MaxU);
			const float MidV  = 0.5f * (MinV + MaxV);
			// Target lies at the bbox centre, projected from the seed cam basis.
			const FVector Target = SeedCamLoc + MeanD * Forward + MidU * Right + MidV * Up;

			const float HalfU = 0.5f * (MaxU - MinU);
			const float HalfV = 0.5f * (MaxV - MinV);
			const float DistU = HalfU / FMath::Max(0.001f, TanH);
			const float DistV = HalfV / FMath::Max(0.001f, TanV);
			const float ReqDist = FMath::Max(50.0f, FMath::Max(DistU, DistV) * 1.20f);

			States[i].Target = Target;
			States[i].Forward = Forward;
			States[i].ReqDist = ReqDist;
			States[i].bValid = true;
			if (ReqDist > Consensus) Consensus = ReqDist;
		}
		Consensus = FMath::Max(Consensus, 100.0f);

		for (int32 i = 0; i < NumViews; ++i)
		{
			if (!States[i].bValid)
			{
				OutCamLocs[i] = FallbackCentre;
				OutCamRots[i] = FRotator::ZeroRotator;
				continue;
			}
			OutCamLocs[i] = States[i].Target - States[i].Forward * Consensus;
			OutCamRots[i] = States[i].Forward.Rotation();
		}
	}

	// ─── Ground grid + root trajectory helpers ───────────────────

	/**
	 * Spawn real static-mesh geometry for the ground grid inside the preview
	 * scene, so the renderer depth-tests the lines against the character
	 * (character occludes the grid where it stands). Earlier versions drew
	 * the grid as a 2D overlay in the pixel buffer AFTER capture, which
	 * always rendered on top of everything.
	 *
	 * Each grid line becomes one UStaticMeshComponent using /Engine/BasicShapes/
	 * Cube (100x100x100 cm base) scaled to LineLength × Thickness × Thickness
	 * and rotated to align its local X with the line direction. Thickness is
	 * expressed in world cm so the lines stay readable at typical framing
	 * distances (2 cm ≈ 3–4 px in a 512-cell view at 4 m).
	 *
	 * Uses two materials to keep axis lines visually distinct without relying
	 * on MID parameter support: the regular lines get the stock
	 * BasicShapeMaterial (light lit neutral), axis lines get BlackUnlitMaterial
	 * for high contrast.
	 */
	static void SpawnGroundGridGeometry(
		FPreviewScene& PreviewScene,
		const FVector& Center, float HalfExtent, float Spacing,
		float LineThickness, float AxisThickness)
	{
		if (HalfExtent <= 0.0f || Spacing <= 0.0f) return;

		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (!CubeMesh) return;
		UMaterialInterface* LineMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UMaterialInterface* AxisMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/EngineDebugMaterials/BlackUnlitMaterial.BlackUnlitMaterial"));
		// Fall back to LineMat if the axis material is missing (non-default
		// engine content). The axis stays visually distinct via thickness.
		if (!AxisMat) AxisMat = LineMat;

		// Snap the grid centre to the nearest spacing multiple so lines always
		// pass through world X=0 and Y=0 when those are within extent. Without
		// the snap, the grid centres on pelvis XY (or trajectory bbox) and the
		// world-origin lines would fall between grid cells.
		const float CX = FMath::RoundToFloat(Center.X / Spacing) * Spacing;
		const float CY = FMath::RoundToFloat(Center.Y / Spacing) * Spacing;
		const float Z  = Center.Z;

		const int32 NumSteps = FMath::CeilToInt(HalfExtent / Spacing);
		const float AdjHalf  = NumSteps * Spacing;

		// "Axis" = the line that passes through world X=0 or Y=0. Use a small
		// tolerance (1% of Spacing) so float-snap noise doesn't miss the hit.
		const float AxisTol = Spacing * 0.01f;

		auto AddLine = [&](const FVector& A, const FVector& B,
			float Thickness, UMaterialInterface* Mat)
		{
			UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(GetTransientPackage());
			MC->SetStaticMesh(CubeMesh);
			if (Mat) MC->SetMaterial(0, Mat);
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MC->SetCastShadow(false);
			MC->bReceivesDecals = false;

			const FVector   Mid  = (A + B) * 0.5f;
			const FVector   Dir  = (B - A).GetSafeNormal();
			const FQuat     Rot  = FRotationMatrix::MakeFromX(Dir).ToQuat();
			const float     Len  = (B - A).Size();
			const FTransform Xform(
				Rot, Mid,
				FVector(Len / 100.0f,
				        Thickness / 100.0f,
				        Thickness / 100.0f));
			PreviewScene.AddComponent(MC, Xform);
		};

		// Lines parallel to Y axis (constant X). Axis line = world Y-axis at X=0.
		for (int32 i = -NumSteps; i <= NumSteps; ++i)
		{
			const float X = CX + i * Spacing;
			const FVector A(X, CY - AdjHalf, Z);
			const FVector B(X, CY + AdjHalf, Z);
			const bool bAxis = (FMath::Abs(X) <= AxisTol);
			AddLine(A, B, bAxis ? AxisThickness : LineThickness,
			        bAxis ? AxisMat : LineMat);
		}
		// Lines parallel to X axis (constant Y). Axis line = world X-axis at Y=0.
		for (int32 j = -NumSteps; j <= NumSteps; ++j)
		{
			const float Y = CY + j * Spacing;
			const FVector A(CX - AdjHalf, Y, Z);
			const FVector B(CX + AdjHalf, Y, Z);
			const bool bAxis = (FMath::Abs(Y) <= AxisTol);
			AddLine(A, B, bAxis ? AxisThickness : LineThickness,
			        bAxis ? AxisMat : LineMat);
		}
	}


	/**
	 * Derive the camera basis + vertical FOV tangent once, so line projection
	 * doesn't repeat the work per call. Kept lightweight (raw struct) since
	 * these helpers are tight loops.
	 */
	struct FCellProjector
	{
		FVector  CamLoc;
		FVector  Forward;
		FVector  Right;
		FVector  Up;
		float    TanH;
		float    TanV;
		float    CellW;
		float    CellH;
		float    NearDist;  // forward-distance threshold for "in front of camera"

		FCellProjector(const FVector& InCamLoc, const FRotator& InCamRot,
			float FOVDeg, int32 InCellW, int32 InCellH)
		{
			CamLoc = InCamLoc;
			const FRotationMatrix RotMat(InCamRot);
			Forward = RotMat.GetUnitAxis(EAxis::X);
			Right   = RotMat.GetUnitAxis(EAxis::Y);
			Up      = RotMat.GetUnitAxis(EAxis::Z);
			const float HalfFovRadH = FMath::DegreesToRadians(FOVDeg * 0.5f);
			TanH = FMath::Tan(HalfFovRadH);
			TanV = TanH * static_cast<float>(InCellH) / FMath::Max(1.0f, static_cast<float>(InCellW));
			CellW = static_cast<float>(InCellW);
			CellH = static_cast<float>(InCellH);
			NearDist = 5.0f;  // clip anything within 5 cm of camera plane
		}

		bool Project(const FVector& W, float& OX, float& OY, float& OutDepth) const
		{
			const FVector Rel = W - CamLoc;
			OutDepth = FVector::DotProduct(Rel, Forward);
			if (OutDepth <= NearDist) return false;
			const float U = FVector::DotProduct(Rel, Right);
			const float V = FVector::DotProduct(Rel, Up);
			const float NdcX = U / (OutDepth * TanH);
			const float NdcY = V / (OutDepth * TanV);
			OX = (NdcX * 0.5f + 0.5f) * CellW;
			OY = (0.5f - NdcY * 0.5f) * CellH;
			return true;
		}
	};

	/**
	 * Draw a world-space line segment into a cell buffer.
	 * Near-plane clipping: if exactly one endpoint is behind the camera,
	 * the segment is clipped to NearDist so the visible half still draws;
	 * if both are behind, the line is dropped entirely.
	 */
	static void DrawWorldLineOnCell(
		FColor* Pixels, const FCellProjector& P,
		int32 CellW, int32 CellH,
		const FVector& WA, const FVector& WB,
		const FColor& Color, int32 Thickness)
	{
		FVector A = WA;
		FVector B = WB;
		const float DA = FVector::DotProduct(A - P.CamLoc, P.Forward);
		const float DB = FVector::DotProduct(B - P.CamLoc, P.Forward);
		if (DA <= P.NearDist && DB <= P.NearDist) return;
		if (DA <= P.NearDist)
		{
			const float t = (P.NearDist - DA) / FMath::Max(0.0001f, (DB - DA));
			A = A + t * (B - A);
		}
		else if (DB <= P.NearDist)
		{
			const float t = (P.NearDist - DB) / FMath::Max(0.0001f, (DA - DB));
			B = B + t * (A - B);
		}
		float XA, YA, XB, YB, _d;
		if (!P.Project(A, XA, YA, _d)) return;
		if (!P.Project(B, XB, YB, _d)) return;
		DrawLineFColor(Pixels, CellW, CellH,
			FMath::RoundToInt(XA), FMath::RoundToInt(YA),
			FMath::RoundToInt(XB), FMath::RoundToInt(YB),
			Color, Thickness);
	}

	/**
	 * Sample the pelvis bone's world position across the whole anim
	 * duration. Caller MUST re-apply the desired display pose via
	 * ApplyPoseAtTime after this returns — sampling leaves the skel
	 * mesh at the last sampled time. Ground path is the XY of the
	 * pelvis flattened to Z=GroundZ, so overlays draw the "shadow"
	 * of the motion on the floor.
	 */
	static void SampleRootTrajectory(
		USkeletalMeshComponent* SkelComp, UAnimSequenceBase* Anim,
		int32 PelvisIdx, float GroundZ, int32 NumSamples,
		TArray<FVector>& OutGroundPath,
		TArray<FVector>& OutPelvisPath,
		TArray<float>& OutTimes)
	{
		if (!SkelComp || !Anim || NumSamples <= 0) return;
		OutGroundPath.Reset(NumSamples);
		OutPelvisPath.Reset(NumSamples);
		OutTimes.Reset(NumSamples);
		const float PlayLength = Anim->GetPlayLength();
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float t = (NumSamples == 1) ? 0.0f
				: (i * PlayLength / static_cast<float>(NumSamples - 1));
			ApplyPoseAtTime(SkelComp, Anim, t);
			const TArray<FTransform>& CS = SkelComp->GetComponentSpaceTransforms();
			const FTransform CompToWorld = SkelComp->GetComponentTransform();
			FVector PelvisWorld = CompToWorld.GetLocation();
			if (PelvisIdx >= 0 && PelvisIdx < CS.Num())
			{
				PelvisWorld = CompToWorld.TransformPosition(CS[PelvisIdx].GetLocation());
			}
			OutGroundPath.Add(FVector(PelvisWorld.X, PelvisWorld.Y, GroundZ));
			OutPelvisPath.Add(PelvisWorld);
			OutTimes.Add(t);
		}
	}

	/**
	 * Draw the trajectory polyline + tick markers on a cell. Line connects
	 * consecutive GroundPath points; ticks are filled squares at path
	 * indices whose time matches a HighlightTime (nearest-time lookup).
	 * The current-frame marker (CurrentTime) gets a larger filled circle.
	 */
	static void DrawTrajectoryOnCell(
		FColor* Pixels, int32 CellW, int32 CellH,
		const TArray<FVector>& GroundPath,
		const TArray<float>&   PathTimes,
		const TArray<float>&   HighlightTimes,
		float                   CurrentTime,
		const FVector& CamLoc, const FRotator& CamRot, float FOVDeg,
		const FColor& LineColor, const FColor& TickColor, const FColor& CurrentColor)
	{
		if (GroundPath.Num() < 2) return;
		const FCellProjector P(CamLoc, CamRot, FOVDeg, CellW, CellH);

		// Polyline.
		for (int32 i = 1; i < GroundPath.Num(); ++i)
		{
			DrawWorldLineOnCell(Pixels, P, CellW, CellH,
				GroundPath[i - 1], GroundPath[i],
				LineColor, /*Thickness=*/ 2);
		}

		auto NearestIndex = [&](float T) -> int32
		{
			int32 Best = 0;
			float BestDiff = FMath::Abs(PathTimes[0] - T);
			for (int32 i = 1; i < PathTimes.Num(); ++i)
			{
				const float Diff = FMath::Abs(PathTimes[i] - T);
				if (Diff < BestDiff) { BestDiff = Diff; Best = i; }
			}
			return Best;
		};

		auto DrawMarker = [&](const FVector& World, const FColor& C, int32 HalfSize)
		{
			float X, Y, D;
			if (!P.Project(World, X, Y, D)) return;
			DrawFilledSquareFColor(Pixels, CellW, CellH,
				FMath::RoundToInt(X), FMath::RoundToInt(Y), HalfSize, C);
		};

		// Tick at each highlight time.
		for (float Ht : HighlightTimes)
		{
			const int32 Idx = NearestIndex(Ht);
			DrawMarker(GroundPath[Idx], TickColor, /*HalfSize=*/ 3);
		}

		// Current-frame marker (slightly larger, different colour).
		if (CurrentTime >= 0.0f)
		{
			const int32 Idx = NearestIndex(CurrentTime);
			DrawMarker(GroundPath[Idx], CurrentColor, /*HalfSize=*/ 5);
		}
	}
}

bool UUnrealBridgeLevelLibrary::CaptureAnimPoseGrid(
	const FString& AnimPath,
	float Time,
	const FString& SkeletalMeshPath,
	const TArray<FString>& Views,
	bool bBoneOverlay,
	bool bPerViewFraming,
	bool bGroundGrid,
	bool bRootTrajectory,
	float GroundZ,
	int32 GridCols,
	int32 CellWidth,
	int32 CellHeight,
	const FString& FilePath)
{
	if (AnimPath.IsEmpty() || FilePath.IsEmpty() ||
		Views.Num() == 0 || GridCols <= 0 || CellWidth <= 0 || CellHeight <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureAnimPoseGrid: bad input."));
		return false;
	}

	// 1. Resolve anim asset.
	UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *AnimPath);
	if (!Anim)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureAnimPoseGrid: failed to load anim '%s'"), *AnimPath);
		return false;
	}

	// 2. Resolve mesh — explicit param wins, else Skeleton's preview mesh.
	USkeletalMesh* Mesh = nullptr;
	if (!SkeletalMeshPath.IsEmpty())
	{
		Mesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
	}
	if (!Mesh)
	{
		if (USkeleton* Skel = Anim->GetSkeleton())
		{
			Mesh = Skel->GetPreviewMesh();
		}
	}
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CaptureAnimPoseGrid: no mesh — pass SkeletalMeshPath or set the Skeleton's PreviewMesh"));
		return false;
	}

	// 3. Build preview scene. FPreviewScene auto-provisions a neutral skylight
	//    + directional light and nothing else — exactly what we want.
	FPreviewScene::ConstructionValues CVS;
	CVS.SetCreatePhysicsScene(false);
	CVS.SetTransactional(false);
	FPreviewScene PreviewScene(CVS);
	UWorld* PreviewWorld = PreviewScene.GetWorld();
	if (!PreviewWorld) return false;

	// 4. Add a SkeletalMeshComponent with the anim.
	USkeletalMeshComponent* SkelComp = NewObject<USkeletalMeshComponent>(
		GetTransientPackage(), USkeletalMeshComponent::StaticClass());
	SkelComp->SetSkeletalMesh(Mesh);
	SkelComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SkelComp->SetUpdateAnimationInEditor(true);
	SkelComp->bForceRefpose = false;
	PreviewScene.AddComponent(SkelComp, FTransform::Identity);

	// 4b. Attach preview assets (weapons, etc.) from Skeleton.
	BridgeLevelImpl::AttachPreviewAssets(SkelComp, Anim->GetSkeleton(), PreviewScene);

	// 4c. Enhance lighting for proper shading.
	BridgeLevelImpl::EnhancePreviewLighting(PreviewScene);

	// 5. Determine pelvis index up-front — used for ground-grid centring and
	//    root-motion trajectory sampling. Safe to call before first pose eval
	//    because ref-skeleton indices are pose-independent.
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	// A temp pose eval at t=0 gives us a populated CSBones array sized right.
	BridgeLevelImpl::ApplyPoseAtTime(SkelComp, Anim, 0.0f);
	const int32 PelvisIdx = BridgeLevelImpl::FindPelvisBoneIndex(
		RefSkel, SkelComp->GetComponentSpaceTransforms().Num());

	// 5b. Sample root trajectory BEFORE the display-pose application — sampling
	//     overrides pose state. We re-apply the display pose afterwards.
	TArray<FVector> TrajGround;
	TArray<FVector> TrajPelvis;
	TArray<float>   TrajTimes;
	if (bRootTrajectory && PelvisIdx != INDEX_NONE)
	{
		const float PlayLength = Anim->GetPlayLength();
		const int32 NumTrajSamples = FMath::Clamp(
			FMath::RoundToInt(PlayLength * 30.0f), 16, 240);
		BridgeLevelImpl::SampleRootTrajectory(
			SkelComp, Anim, PelvisIdx, GroundZ, NumTrajSamples,
			TrajGround, TrajPelvis, TrajTimes);
	}

	// 6. Drive the pose to the requested display time.
	BridgeLevelImpl::ApplyPoseAtTime(SkelComp, Anim, Time);
	BridgeLevelImpl::FlushPoseToRenderProxy(SkelComp);

	// 7. Framing. Legacy path: pelvis-anchored single radius + distance; per-view:
	//    project bones into each view basis then apply consensus distance so scale
	//    stays consistent across cells.
	constexpr float FOVDeg = 45.0f;
	FVector LegacyCentre;
	float LegacyRadius;
	BridgeLevelImpl::ComputePoseFraming(Mesh, SkelComp, LegacyCentre, LegacyRadius);
	const float HalfFovRad = FMath::DegreesToRadians(FOVDeg * 0.5f);
	const float LegacyDistance = LegacyRadius / FMath::Max(0.01f, FMath::Tan(HalfFovRad));

	const int32 NumViews = Views.Num();
	TArray<FVector>  ViewDirs; ViewDirs.SetNum(NumViews);
	TArray<bool>     ViewOk;   ViewOk.SetNum(NumViews);
	for (int32 i = 0; i < NumViews; ++i)
	{
		ViewOk[i] = BridgeLevelImpl::ResolveViewDirection(Views[i], ViewDirs[i]);
		if (!ViewOk[i])
		{
			UE_LOG(LogTemp, Warning,
				TEXT("CaptureAnimPoseGrid: unknown view '%s' — skipping"), *Views[i]);
			ViewDirs[i] = FVector::ZeroVector;
		}
	}

	TArray<FVector>  CamLocs;  CamLocs.SetNum(NumViews);
	TArray<FRotator> CamRots;  CamRots.SetNum(NumViews);
	if (bPerViewFraming)
	{
		TArray<FVector> BonePositions;
		BridgeLevelImpl::CollectBoneWorldPositions(SkelComp, BonePositions);
		// Include trajectory path in the framing bbox so the full motion
		// stays visible; include ground-centre point so the grid doesn't
		// fall off the edge on airborne poses.
		if (bRootTrajectory) BonePositions.Append(TrajGround);
		if (bGroundGrid)
		{
			const FVector GroundCentre = TrajGround.Num() > 0
				? FVector(TrajGround[0].X, TrajGround[0].Y, GroundZ)
				: FVector(LegacyCentre.X, LegacyCentre.Y, GroundZ);
			BonePositions.Add(GroundCentre);
		}
		BridgeLevelImpl::ComputePerViewFraming(
			BonePositions, ViewDirs, LegacyCentre, LegacyRadius,
			FOVDeg, CellWidth, CellHeight, CamLocs, CamRots);
	}
	else
	{
		for (int32 i = 0; i < NumViews; ++i)
		{
			if (!ViewOk[i]) continue;
			CamLocs[i] = LegacyCentre + ViewDirs[i].GetSafeNormal() * LegacyDistance;
			CamRots[i] = (LegacyCentre - CamLocs[i]).Rotation();
		}
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("CaptureAnimPoseGrid: mesh=%s bone_count=%d centre=%s radius=%.1f per_view=%d overlay=%d ground=%d traj=%d"),
		*Mesh->GetName(), SkelComp->GetComponentSpaceTransforms().Num(),
		*LegacyCentre.ToString(), LegacyRadius,
		bPerViewFraming ? 1 : 0, bBoneOverlay ? 1 : 0,
		bGroundGrid ? 1 : 0, bRootTrajectory ? 1 : 0);

	// 8. Bone chains (built once — skeleton is stable for the whole call).
	TArray<BridgeLevelImpl::FBoneChainDef> BoneChains;
	if (bBoneOverlay)
	{
		BridgeLevelImpl::BuildBoneChains(
			RefSkel, SkelComp->GetComponentSpaceTransforms().Num(),
			BoneChains);
	}

	// 8b. Pre-compute ground-grid centre + extent.
	//     Centre on pelvis XY at display time; if trajectory present, use the
	//     bbox mid of the trajectory so the grid stays centred on the motion.
	FVector GroundCenter(0, 0, GroundZ);
	float GroundHalfExtent = 300.0f;  // cm; 6 m square by default
	if (bGroundGrid)
	{
		const TArray<FTransform>& CS = SkelComp->GetComponentSpaceTransforms();
		const FTransform CompToWorld = SkelComp->GetComponentTransform();
		FVector PelvisWorld = (PelvisIdx != INDEX_NONE && PelvisIdx < CS.Num())
			? CompToWorld.TransformPosition(CS[PelvisIdx].GetLocation())
			: LegacyCentre;
		GroundCenter = FVector(PelvisWorld.X, PelvisWorld.Y, GroundZ);
		if (TrajGround.Num() >= 2)
		{
			FBox XYBox(ForceInit);
			for (const FVector& P : TrajGround) XYBox += P;
			GroundCenter = FVector(XYBox.GetCenter().X, XYBox.GetCenter().Y, GroundZ);
			const float TrajHalf = 0.5f * FMath::Max(
				XYBox.Max.X - XYBox.Min.X, XYBox.Max.Y - XYBox.Min.Y);
			GroundHalfExtent = FMath::Max(300.0f, TrajHalf + 150.0f);
		}

		// Spawn the grid as real geometry in the preview scene so the
		// renderer depth-tests it against the character (occlusion works
		// correctly). Thickness in cm — tuned so the lines read clearly at
		// typical 3–5 m framing distances. Regular 2 cm, axis 4 cm so the
		// centre cross remains identifiable.
		BridgeLevelImpl::SpawnGroundGridGeometry(
			PreviewScene,
			GroundCenter, GroundHalfExtent, /*Spacing=*/ 50.0f,
			/*LineThickness=*/ 2.0f, /*AxisThickness=*/ 4.0f);
	}

	// 9. Allocate composite image.
	const int32 Rows = FMath::DivideAndRoundUp(NumViews, GridCols);
	const int32 CompW = GridCols * CellWidth;
	const int32 CompH = Rows * CellHeight;

	FImage Composite;
	Composite.Init(CompW, CompH, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	FColor* CompDst = reinterpret_cast<FColor*>(Composite.RawData.GetData());
	for (int32 i = 0; i < CompW * CompH; ++i)
	{
		CompDst[i] = FColor(217, 217, 222, 255);
	}

	// Overlay colours — consistent across pose-grid and timeline.
	const FColor TrajColor(40, 210, 80, 255);
	const FColor TrajTickColor(255, 255, 255, 255);
	const FColor TrajCurrentColor(255, 60, 60, 255);

	// 10. Render each view + optional overlays + copy into cell.
	for (int32 ViewIdx = 0; ViewIdx < NumViews; ++ViewIdx)
	{
		if (!ViewOk[ViewIdx]) continue;

		TArray<FColor> Pixels;
		if (!BridgeLevelImpl::CaptureViewToPixels(
				PreviewWorld, CamLocs[ViewIdx], CamRots[ViewIdx],
				FOVDeg, CellWidth, CellHeight, Pixels))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("CaptureAnimPoseGrid: capture for view '%s' failed"), *Views[ViewIdx]);
			continue;
		}

		// Overlay order: ground grid is now real scene geometry (occluded by
		// character naturally). Only trajectory + bones remain as 2D overlays
		// drawn after capture.
		if (bRootTrajectory && TrajGround.Num() >= 2)
		{
			// For pose grid there's one "current" time (Time).
			TArray<float> Highlights;
			Highlights.Add(Time);
			BridgeLevelImpl::DrawTrajectoryOnCell(
				Pixels.GetData(), CellWidth, CellHeight,
				TrajGround, TrajTimes, Highlights, Time,
				CamLocs[ViewIdx], CamRots[ViewIdx], FOVDeg,
				TrajColor, TrajTickColor, TrajCurrentColor);
		}
		if (bBoneOverlay)
		{
			BridgeLevelImpl::DrawBoneOverlayOnCell(
				Pixels.GetData(), CellWidth, CellHeight,
				SkelComp, BoneChains,
				CamLocs[ViewIdx], CamRots[ViewIdx], FOVDeg);
		}

		const int32 Col = ViewIdx % GridCols;
		const int32 Row = ViewIdx / GridCols;
		const int32 DstX = Col * CellWidth;
		const int32 DstY = Row * CellHeight;
		for (int32 Y = 0; Y < CellHeight; ++Y)
		{
			FMemory::Memcpy(
				CompDst + (DstY + Y) * CompW + DstX,
				Pixels.GetData() + Y * CellWidth,
				CellWidth * sizeof(FColor));
		}
	}

	// 9. Save PNG.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FilePath));
	return FImageUtils::SaveImageByExtension(*FilePath, Composite, /*CompressionQuality=*/ 0);
}

// ─── Anim montage timeline grid ───────────────────────────────

bool UUnrealBridgeLevelLibrary::CaptureAnimMontageTimeline(
	const FString& AnimPath,
	const FString& SkeletalMeshPath,
	int32 NumTimeSamples,
	const TArray<FString>& Views,
	bool bBoneOverlay,
	bool bPerViewFraming,
	bool bGroundGrid,
	bool bRootTrajectory,
	float GroundZ,
	int32 CellWidth,
	int32 CellHeight,
	const FString& FilePath)
{
	if (AnimPath.IsEmpty() || FilePath.IsEmpty() ||
		Views.Num() == 0 || NumTimeSamples <= 0 ||
		CellWidth <= 0 || CellHeight <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureAnimMontageTimeline: bad input."));
		return false;
	}

	// 1. Load anim + mesh (same fallback chain as CaptureAnimPoseGrid).
	UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *AnimPath);
	if (!Anim)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureAnimMontageTimeline: failed to load anim '%s'"), *AnimPath);
		return false;
	}
	USkeletalMesh* Mesh = nullptr;
	if (!SkeletalMeshPath.IsEmpty())
	{
		Mesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
	}
	if (!Mesh && Anim->GetSkeleton())
	{
		Mesh = Anim->GetSkeleton()->GetPreviewMesh();
	}
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CaptureAnimMontageTimeline: no mesh — pass SkeletalMeshPath or set the Skeleton's PreviewMesh"));
		return false;
	}

	// 2. Build preview scene + skel mesh component.
	FPreviewScene::ConstructionValues CVS;
	CVS.SetCreatePhysicsScene(false);
	CVS.SetTransactional(false);
	FPreviewScene PreviewScene(CVS);
	UWorld* PreviewWorld = PreviewScene.GetWorld();
	if (!PreviewWorld) return false;

	USkeletalMeshComponent* SkelComp = NewObject<USkeletalMeshComponent>(
		GetTransientPackage(), USkeletalMeshComponent::StaticClass());
	SkelComp->SetSkeletalMesh(Mesh);
	SkelComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SkelComp->SetUpdateAnimationInEditor(true);
	SkelComp->bForceRefpose = false;
	PreviewScene.AddComponent(SkelComp, FTransform::Identity);

	// 2b. Attach preview assets (weapons, etc.) from Skeleton.
	BridgeLevelImpl::AttachPreviewAssets(SkelComp, Anim->GetSkeleton(), PreviewScene);

	// 2c. Enhance lighting for proper shading.
	BridgeLevelImpl::EnhancePreviewLighting(PreviewScene);

	// 3. Generate time samples across [0, PlayLength].
	const int32 N = FMath::Max(1, NumTimeSamples);
	const float PlayLength = Anim->GetPlayLength();
	TArray<float> SampleTimes;
	SampleTimes.Reserve(N);
	if (N == 1)
	{
		SampleTimes.Add(0.0f);
	}
	else
	{
		for (int32 i = 0; i < N; ++i)
		{
			SampleTimes.Add(i * PlayLength / static_cast<float>(N - 1));
		}
	}

	// 4. Motion-aware framing: evaluate each sample once to build a union
	//    of bone positions (legacy: AABB only; per-view: full position set
	//    for camera-basis reprojection). Camera then stays fixed across
	//    rows so motion reads cleanly and scale is consistent between frames.
	constexpr float FOVDeg = 45.0f;
	FBox MotionAABB(ForceInit);
	TArray<FVector> UnionBoneWorldPositions;
	if (bPerViewFraming)
	{
		UnionBoneWorldPositions.Reserve(N * SkelComp->GetComponentSpaceTransforms().Num());
	}
	for (float T : SampleTimes)
	{
		BridgeLevelImpl::ApplyPoseAtTime(SkelComp, Anim, T);
		const TArray<FTransform>& CSBones = SkelComp->GetComponentSpaceTransforms();
		const FTransform CompToWorld = SkelComp->GetComponentTransform();
		for (const FTransform& Tr : CSBones)
		{
			const FVector W = CompToWorld.TransformPosition(Tr.GetLocation());
			MotionAABB += W;
			if (bPerViewFraming)
			{
				UnionBoneWorldPositions.Add(W);
			}
		}
	}
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	const int32 PelvisIdx = BridgeLevelImpl::FindPelvisBoneIndex(
		RefSkel, SkelComp->GetComponentSpaceTransforms().Num());

	// 4b. Dense trajectory sampling (independent of the row samples so the
	//     path reads smoothly). Uses ApplyPoseAtTime — done BEFORE setting
	//     the mid-sample display pose below.
	TArray<FVector> TrajGround;
	TArray<FVector> TrajPelvis;
	TArray<float>   TrajTimes;
	if (bRootTrajectory && PelvisIdx != INDEX_NONE)
	{
		const int32 NumTrajSamples = FMath::Clamp(
			FMath::RoundToInt(PlayLength * 30.0f), 16, 240);
		BridgeLevelImpl::SampleRootTrajectory(
			SkelComp, Anim, PelvisIdx, GroundZ, NumTrajSamples,
			TrajGround, TrajPelvis, TrajTimes);
	}

	// Centre on pelvis at the middle sample (stable visual anchor); fall
	// back to motion AABB centre.
	const int32 MidIdx = N / 2;
	BridgeLevelImpl::ApplyPoseAtTime(SkelComp, Anim, SampleTimes[MidIdx]);
	FVector Centre = (PelvisIdx != INDEX_NONE)
		? SkelComp->GetComponentTransform().TransformPosition(
			SkelComp->GetComponentSpaceTransforms()[PelvisIdx].GetLocation())
		: MotionAABB.GetCenter();
	const FVector ToMin = (MotionAABB.Min - Centre).GetAbs();
	const FVector ToMax = (MotionAABB.Max - Centre).GetAbs();
	const FVector ToCorner = ToMin.ComponentMax(ToMax);
	const float Radius = FMath::Max(10.0f, ToCorner.Size() * 1.15f);
	const float HalfFovRad = FMath::DegreesToRadians(FOVDeg * 0.5f);
	const float Distance = Radius / FMath::Max(0.01f, FMath::Tan(HalfFovRad));

	UE_LOG(LogTemp, Verbose,
		TEXT("CaptureAnimMontageTimeline: mesh=%s N=%d centre=%s radius=%.1f per_view=%d overlay=%d ground=%d traj=%d"),
		*Mesh->GetName(), N, *Centre.ToString(), Radius,
		bPerViewFraming ? 1 : 0, bBoneOverlay ? 1 : 0,
		bGroundGrid ? 1 : 0, bRootTrajectory ? 1 : 0);

	// 5. Pre-compute camera poses per view (fixed across rows).
	const int32 NumViews = Views.Num();
	TArray<FVector>  ViewDirs; ViewDirs.SetNum(NumViews);
	TArray<bool>     ViewOk;   ViewOk.SetNum(NumViews);
	for (int32 c = 0; c < NumViews; ++c)
	{
		ViewOk[c] = BridgeLevelImpl::ResolveViewDirection(Views[c], ViewDirs[c]);
		if (!ViewOk[c])
		{
			UE_LOG(LogTemp, Warning,
				TEXT("CaptureAnimMontageTimeline: unknown view '%s' — skipping"), *Views[c]);
			ViewDirs[c] = FVector::ZeroVector;
		}
	}
	TArray<FVector>  CamLocs;  CamLocs.SetNum(NumViews);
	TArray<FRotator> CamRots;  CamRots.SetNum(NumViews);
	if (bPerViewFraming)
	{
		// Include trajectory path + ground centre in framing so the
		// per-view bbox captures those overlays too.
		TArray<FVector> FramingPoints = UnionBoneWorldPositions;
		if (bRootTrajectory) FramingPoints.Append(TrajGround);
		if (bGroundGrid)
		{
			FramingPoints.Add(FVector(Centre.X, Centre.Y, GroundZ));
		}
		BridgeLevelImpl::ComputePerViewFraming(
			FramingPoints, ViewDirs, Centre, Radius,
			FOVDeg, CellWidth, CellHeight, CamLocs, CamRots);
	}
	else
	{
		for (int32 c = 0; c < NumViews; ++c)
		{
			if (!ViewOk[c]) continue;
			CamLocs[c] = Centre + ViewDirs[c].GetSafeNormal() * Distance;
			CamRots[c] = (Centre - CamLocs[c]).Rotation();
		}
	}

	// 5b. Bone chains (built once — skeleton is stable across time samples).
	TArray<BridgeLevelImpl::FBoneChainDef> BoneChains;
	if (bBoneOverlay)
	{
		BridgeLevelImpl::BuildBoneChains(
			RefSkel, SkelComp->GetComponentSpaceTransforms().Num(), BoneChains);
	}

	// 5c. Ground-grid centre + extent (covers motion path) + spawn geometry.
	FVector GroundCenter(Centre.X, Centre.Y, GroundZ);
	float GroundHalfExtent = 300.0f;
	if (bGroundGrid)
	{
		if (TrajGround.Num() >= 2)
		{
			FBox XYBox(ForceInit);
			for (const FVector& P : TrajGround) XYBox += P;
			GroundCenter = FVector(XYBox.GetCenter().X, XYBox.GetCenter().Y, GroundZ);
			const float TrajHalf = 0.5f * FMath::Max(
				XYBox.Max.X - XYBox.Min.X, XYBox.Max.Y - XYBox.Min.Y);
			GroundHalfExtent = FMath::Max(300.0f, TrajHalf + 150.0f);
		}

		// Spawn grid as real scene geometry so it's depth-tested against the
		// character (occluded correctly) and stays legible at distance.
		BridgeLevelImpl::SpawnGroundGridGeometry(
			PreviewScene,
			GroundCenter, GroundHalfExtent, /*Spacing=*/ 50.0f,
			/*LineThickness=*/ 2.0f, /*AxisThickness=*/ 4.0f);
	}

	// 6. Allocate composite: rows = N (time), cols = NumViews.
	const int32 CompW = NumViews * CellWidth;
	const int32 CompH = N * CellHeight;
	FImage Composite;
	Composite.Init(CompW, CompH, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	FColor* CompDst = reinterpret_cast<FColor*>(Composite.RawData.GetData());
	for (int32 i = 0; i < CompW * CompH; ++i)
	{
		CompDst[i] = FColor(217, 217, 222, 255);
	}

	// Overlay colours — consistent across pose-grid and timeline.
	const FColor TrajColor(40, 210, 80, 255);
	const FColor TrajTickColor(255, 255, 255, 255);
	const FColor TrajCurrentColor(255, 60, 60, 255);

	// 7. For each row (time), set pose + capture each view + overlays.
	for (int32 Row = 0; Row < N; ++Row)
	{
		BridgeLevelImpl::ApplyPoseAtTime(SkelComp, Anim, SampleTimes[Row]);
		BridgeLevelImpl::FlushPoseToRenderProxy(SkelComp);

		for (int32 Col = 0; Col < NumViews; ++Col)
		{
			if (!ViewOk[Col]) continue;
			TArray<FColor> Pixels;
			if (!BridgeLevelImpl::CaptureViewToPixels(
					PreviewWorld, CamLocs[Col], CamRots[Col], FOVDeg,
					CellWidth, CellHeight, Pixels))
			{
				continue;
			}

			// Ground grid is real scene geometry now (occluded by character).
			// Only trajectory + bone overlays stay as 2D post-capture passes.
			if (bRootTrajectory && TrajGround.Num() >= 2)
			{
				// Ticks on every row time; current-row time highlighted.
				BridgeLevelImpl::DrawTrajectoryOnCell(
					Pixels.GetData(), CellWidth, CellHeight,
					TrajGround, TrajTimes, SampleTimes, SampleTimes[Row],
					CamLocs[Col], CamRots[Col], FOVDeg,
					TrajColor, TrajTickColor, TrajCurrentColor);
			}
			if (bBoneOverlay)
			{
				BridgeLevelImpl::DrawBoneOverlayOnCell(
					Pixels.GetData(), CellWidth, CellHeight,
					SkelComp, BoneChains,
					CamLocs[Col], CamRots[Col], FOVDeg);
			}

			const int32 DstX = Col * CellWidth;
			const int32 DstY = Row * CellHeight;
			for (int32 Y = 0; Y < CellHeight; ++Y)
			{
				FMemory::Memcpy(
					CompDst + (DstY + Y) * CompW + DstX,
					Pixels.GetData() + Y * CellWidth,
					CellWidth * sizeof(FColor));
			}
		}
	}

	// 8. Save PNG.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FilePath));
	return FImageUtils::SaveImageByExtension(*FilePath, Composite, /*CompressionQuality=*/ 0);
}

bool UUnrealBridgeLevelLibrary::LineTraceHitInfo(
	FVector Start, FVector End,
	FString& OutActorLabel, float& OutDistance, FVector& OutImpactLocation)
{
	OutActorLabel.Reset();
	OutDistance = static_cast<float>((End - Start).Size());
	OutImpactLocation = End;

	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeLineTraceHitInfo), /*bTraceComplex*/ true);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return false;
	}
	if (AActor* A = Hit.GetActor())
	{
		OutActorLabel = A->GetActorLabel();
	}
	OutDistance = Hit.Distance;
	OutImpactLocation = Hit.ImpactPoint;
	return true;
}

TArray<FString> UUnrealBridgeLevelLibrary::MultiLineTraceActors(FVector Start, FVector End)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
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
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
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
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
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
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
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
	UWorld* World = BridgeLevelImpl::GetRuntimeWorld();
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
	if (!BridgeLevelImpl::DownwardTrace(BridgeLevelImpl::GetRuntimeWorld(), X, Y, StartHeight, Hit))
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

// ─── Cone / segment geometry helpers ───────────────────────────────────

namespace BridgeLevelImpl
{
	static bool IsPointInsideCone(const FVector& Point, const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float MaxDistance)
	{
		const FVector ToPoint = Point - Origin;
		const float DistSq = ToPoint.SizeSquared();
		if (DistSq > FMath::Square(FMath::Max(MaxDistance, 0.0f)))
		{
			return false;
		}
		FVector Dir = Direction;
		if (!Dir.Normalize())
		{
			return false;
		}
		if (DistSq < KINDA_SMALL_NUMBER)
		{
			return true; // at origin
		}
		const FVector ToPointDir = ToPoint.GetSafeNormal();
		const float Dot = FVector::DotProduct(Dir, ToPointDir);
		const float CosLimit = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 0.0f, 180.0f)));
		return Dot >= CosLimit;
	}
}

TArray<FString> UUnrealBridgeLevelLibrary::FindActorsInCone(FVector Origin, FVector Direction, float HalfAngleDeg, float MaxDistance, const FString& ClassFilter)
{
	TArray<FString> Out;
	UWorld* World = BridgeLevelImpl::GetEditorWorld();
	if (!World) return Out;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		if (!BridgeLevelImpl::MatchesClassFilter(A->GetClass(), ClassFilter)) continue;
		if (BridgeLevelImpl::IsPointInsideCone(A->GetActorLocation(), Origin, Direction, HalfAngleDeg, MaxDistance))
		{
			Out.Add(A->GetActorLabel());
		}
	}
	return Out;
}

bool UUnrealBridgeLevelLibrary::IsActorInCone(const FString& ActorName, FVector Origin, FVector Direction, float HalfAngleDeg, float MaxDistance)
{
	AActor* A = BridgeLevelImpl::FindActor(BridgeLevelImpl::GetEditorWorld(), ActorName);
	if (!A) return false;
	return BridgeLevelImpl::IsPointInsideCone(A->GetActorLocation(), Origin, Direction, HalfAngleDeg, MaxDistance);
}

FVector UUnrealBridgeLevelLibrary::ClosestPointOnSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd)
{
	return FMath::ClosestPointOnSegment(Point, SegmentStart, SegmentEnd);
}

float UUnrealBridgeLevelLibrary::DistanceFromPointToSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd)
{
	return FMath::PointDistToSegment(Point, SegmentStart, SegmentEnd);
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

#include "UnrealBridgePCGLibrary.h"

#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "PCGComponent.h"
#include "PCGGraph.h"

#include "Engine/World.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"
#include "UObject/UnrealType.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UnrealBridgePCG"

namespace BridgePCGImpl
{
	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
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
			if (!A) { continue; }
			if (A->GetFName() == AsName || A->GetActorLabel() == NameOrLabel)
			{
				return A;
			}
		}
		return nullptr;
	}

	UPCGComponent* FindPCGComponent(AActor* Actor, const FString& ComponentName)
	{
		if (!Actor)
		{
			return nullptr;
		}
		TArray<UPCGComponent*> Components;
		Actor->GetComponents<UPCGComponent>(Components);
		if (ComponentName.IsEmpty())
		{
			return Components.Num() > 0 ? Components[0] : nullptr;
		}
		const FName AsName(*ComponentName);
		for (UPCGComponent* C : Components)
		{
			if (C && (C->GetFName() == AsName || C->GetName() == ComponentName))
			{
				return C;
			}
		}
		return nullptr;
	}

	FString PathForGraph(const UPCGGraph* Graph)
	{
		return Graph ? Graph->GetPathName() : FString{};
	}
}

TArray<FString> UUnrealBridgePCGLibrary::ListPCGGraphAssets(const FString& Filter, int32 Max)
{
	TArray<FString> Out;
	const int32 Cap = FMath::Max(1, Max);

	IAssetRegistry& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	Reg.GetAssetsByClass(UPCGGraph::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

	for (const FAssetData& A : Assets)
	{
		if (Out.Num() >= Cap)
		{
			break;
		}
		const FString Path = A.GetObjectPathString();
		if (Filter.IsEmpty() || A.AssetName.ToString().Contains(Filter) || Path.Contains(Filter))
		{
			Out.Add(Path);
		}
	}
	return Out;
}

TArray<FBridgePCGComponentEntry> UUnrealBridgePCGLibrary::ListPCGComponentsInLevel(const FString& LevelFilter, int32 Max)
{
	using namespace BridgePCGImpl;
	TArray<FBridgePCGComponentEntry> Out;
	const int32 Cap = FMath::Max(1, Max);

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Out;
	}

	for (TActorIterator<AActor> It(World); It && Out.Num() < Cap; ++It)
	{
		AActor* A = *It;
		if (!A) { continue; }

		// Optional level filter: skip actors whose outer-level package name doesn't contain the filter.
		if (!LevelFilter.IsEmpty())
		{
			const ULevel* Level = A->GetLevel();
			const FString LevelName = Level ? Level->GetOutermost()->GetName() : FString{};
			if (!LevelName.Contains(LevelFilter))
			{
				continue;
			}
		}

		TArray<UPCGComponent*> Comps;
		A->GetComponents<UPCGComponent>(Comps);
		for (UPCGComponent* C : Comps)
		{
			if (!C || Out.Num() >= Cap) { continue; }
			FBridgePCGComponentEntry E;
			E.ActorLabel    = A->GetActorLabel();
			E.ComponentName = C->GetName();
			E.GraphPath     = PathForGraph(C->GetGraph());
			E.bGenerated    = C->bGenerated;
			E.bGenerating   = C->IsGenerating();
			Out.Add(MoveTemp(E));
		}
	}
	return Out;
}

FBridgePCGComponentState UUnrealBridgePCGLibrary::GetPCGComponentState(const FString& ActorLabel, const FString& ComponentName)
{
	using namespace BridgePCGImpl;
	FBridgePCGComponentState State;
	UWorld* World = GetEditorWorld();
	AActor* A = FindActor(World, ActorLabel);
	UPCGComponent* C = FindPCGComponent(A, ComponentName);
	if (!C)
	{
		return State;
	}

	State.GraphPath        = PathForGraph(C->GetGraph());
	State.bGenerated       = C->bGenerated;
	State.bDirty           = false;  // bDirtyGenerated is private; expose via getter when available
	State.bGenerating      = C->IsGenerating();
	State.GeneratedBounds  = C->GetLastGeneratedBounds();
	State.LastGenerationIso = FDateTime::UtcNow().ToIso8601();  // best-effort marker (not persisted)
	return State;
}

TArray<FBridgePCGOverrideEntry> UUnrealBridgePCGLibrary::GetPCGComponentOverrides(const FString& ActorLabel, const FString& ComponentName)
{
	using namespace BridgePCGImpl;
	TArray<FBridgePCGOverrideEntry> Out;
	UWorld* World = GetEditorWorld();
	AActor* A = FindActor(World, ActorLabel);
	UPCGComponent* C = FindPCGComponent(A, ComponentName);
	if (!C)
	{
		return Out;
	}
	UPCGGraphInstance* GI = C->GetGraphInstance();
	if (!GI)
	{
		return Out;
	}
	const FInstancedPropertyBag* UserParams = GI->GetUserParametersStruct();
	if (!UserParams)
	{
		return Out;
	}
	const UPropertyBag* BagDesc = UserParams->GetPropertyBagStruct();
	const uint8* Memory          = UserParams->GetValue().GetMemory();
	if (!BagDesc || !Memory)
	{
		return Out;
	}

	for (const FPropertyBagPropertyDesc& Desc : BagDesc->GetPropertyDescs())
	{
		if (!Desc.CachedProperty)
		{
			continue;
		}
		FBridgePCGOverrideEntry E;
		E.Name    = Desc.Name.ToString();
		E.TypeStr = Desc.CachedProperty->GetCPPType();

		FString Exported;
		const void* Addr = Desc.CachedProperty->ContainerPtrToValuePtr<void>(Memory);
		Desc.CachedProperty->ExportText_Direct(Exported, Addr, Addr, /*Parent=*/nullptr, PPF_None);
		E.ValueStr = Exported;
		Out.Add(MoveTemp(E));
	}
	return Out;
}

bool UUnrealBridgePCGLibrary::SetPCGComponentOverride(
	const FString& ActorLabel, const FString& ComponentName,
	const FString& Name, const FString& ExportedValue)
{
	using namespace BridgePCGImpl;
	UWorld* World = GetEditorWorld();
	AActor* A = FindActor(World, ActorLabel);
	UPCGComponent* C = FindPCGComponent(A, ComponentName);
	if (!C)
	{
		return false;
	}
	UPCGGraphInstance* GI = C->GetGraphInstance();
	if (!GI)
	{
		return false;
	}
	FInstancedPropertyBag* UserParams = GI->GetMutableUserParametersStruct_Unsafe();
	if (!UserParams)
	{
		return false;
	}
	const UPropertyBag* BagDesc = UserParams->GetPropertyBagStruct();
	uint8* Memory               = const_cast<uint8*>(UserParams->GetValue().GetMemory());
	if (!BagDesc || !Memory)
	{
		return false;
	}

	const FName TargetName(*Name);
	for (const FPropertyBagPropertyDesc& Desc : BagDesc->GetPropertyDescs())
	{
		if (Desc.Name != TargetName || !Desc.CachedProperty)
		{
			continue;
		}
		// Reject empty input. UE's ImportText silently accepts "" on numeric
		// properties and writes zero — caller must use the property's exported
		// "empty" form (e.g. `""` for FString, not the empty Python string).
		if (ExportedValue.IsEmpty())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge|PCG: empty value rejected for '%s' (type=%s)"),
				*Name, *Desc.CachedProperty->GetCPPType());
			return false;
		}

		const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "SetPCGOverride", "Set PCG Component Override"));
		C->Modify();
		GI->Modify();
		void* Addr = Desc.CachedProperty->ContainerPtrToValuePtr<void>(Memory);

		// Snapshot the current value so we can restore on parse failure. UE's
		// FDoubleProperty / FFloatProperty / FIntProperty silently accept garbage:
		// they return the input pointer unchanged (non-null) and write zero to the
		// target. Without snapshot+restore, even when we detect the failure via
		// "no characters consumed", the target value has already been zeroed.
		FString OriginalExport;
		Desc.CachedProperty->ExportText_Direct(OriginalExport, Addr, Addr, /*Parent=*/nullptr, PPF_None);

		const TCHAR* Cursor = *ExportedValue;
		const TCHAR* Parsed = Desc.CachedProperty->ImportText_Direct(Cursor, Addr, /*Parent=*/nullptr, PPF_None);

		const bool bConsumedNothing = (Parsed == Cursor);
		if (!Parsed || bConsumedNothing)
		{
			// Restore the pre-write value via ImportText of the snapshot.
			const TCHAR* OrigCursor = *OriginalExport;
			Desc.CachedProperty->ImportText_Direct(OrigCursor, Addr, /*Parent=*/nullptr, PPF_None);

			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge|PCG: ImportText failed for '%s' = '%s' (type=%s); restored '%s'"),
				*Name, *ExportedValue, *Desc.CachedProperty->GetCPPType(), *OriginalExport);
			return false;
		}
		GI->UpdatePropertyOverride(Desc.CachedProperty, true);
		GI->OnGraphParametersChanged(EPCGGraphParameterEvent::ValueModifiedLocally, Desc.Name);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|PCG: override property '%s' not found on component"), *Name);
	return false;
}

bool UUnrealBridgePCGLibrary::TriggerPCGGenerate(const FString& ActorLabel, const FString& ComponentName, bool bForce)
{
	using namespace BridgePCGImpl;
	UWorld* World = GetEditorWorld();
	AActor* A = FindActor(World, ActorLabel);
	UPCGComponent* C = FindPCGComponent(A, ComponentName);
	if (!C)
	{
		return false;
	}
	C->Generate(bForce);
	return true;
}

FBridgePCGWaitResult UUnrealBridgePCGLibrary::WaitForPCGGenerate(const FString& ActorLabel, const FString& ComponentName, float TimeoutSec)
{
	using namespace BridgePCGImpl;
	FBridgePCGWaitResult Result;
	UWorld* World = GetEditorWorld();
	AActor* A = FindActor(World, ActorLabel);
	UPCGComponent* C = FindPCGComponent(A, ComponentName);
	if (!C)
	{
		Result.Note = TEXT("component not found");
		return Result;
	}

	const double Start = FPlatformTime::Seconds();
	const double Deadline = Start + FMath::Max(0.f, TimeoutSec);

	while (FPlatformTime::Seconds() < Deadline)
	{
		if (!C->IsGenerating())
		{
			Result.bSuccess = true;
			Result.ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0f;
			Result.Note = C->bGenerated ? TEXT("generated") : TEXT("not generated (no work to do?)");
			return Result;
		}
		FPlatformProcess::Sleep(0.05f);
	}
	Result.ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0f;
	Result.Note = TEXT("timeout");
	return Result;
}

bool UUnrealBridgePCGLibrary::CleanupPCGComponent(const FString& ActorLabel, const FString& ComponentName, bool bRemoveComponents)
{
	using namespace BridgePCGImpl;
	UWorld* World = GetEditorWorld();
	AActor* A = FindActor(World, ActorLabel);
	UPCGComponent* C = FindPCGComponent(A, ComponentName);
	if (!C)
	{
		return false;
	}
	C->Cleanup(bRemoveComponents);
	return true;
}

#undef LOCTEXT_NAMESPACE

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

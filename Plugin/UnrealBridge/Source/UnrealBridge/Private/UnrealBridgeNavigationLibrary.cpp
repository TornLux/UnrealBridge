#include "UnrealBridgeNavigationLibrary.h"

#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeNav, Log, All);

namespace BridgeNavImpl
{
	UWorld* GetEditorWorld()
	{
		if (GEditor)
		{
			if (FWorldContext* Ctx = GEditor->GetEditorWorldContext(false).World() ? &GEditor->GetEditorWorldContext(false) : nullptr)
			{
				(void)Ctx; // silence unused
			}
			return GEditor->GetEditorWorldContext(false).World();
		}
		return nullptr;
	}

	ARecastNavMesh* FindRecastNavMesh(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		// Primary: NavigationSystem's registered nav data.
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			for (ANavigationData* Data : NavSys->NavDataSet)
			{
				if (ARecastNavMesh* Recast = Cast<ARecastNavMesh>(Data))
				{
					return Recast;
				}
			}
			UE_LOG(LogUnrealBridgeNav, Log,
				TEXT("NavSys present but NavDataSet has %d entries — none Recast"),
				NavSys->NavDataSet.Num());
		}

		// Fallback: scan world for any RecastNavMesh actor directly.
		for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

bool UUnrealBridgeNavigationLibrary::ExportNavMeshToObj(const FString& OutFilePath, FString& OutSummary)
{
	UWorld* World = BridgeNavImpl::GetEditorWorld();
	if (!World)
	{
		OutSummary = TEXT("no editor world");
		UE_LOG(LogUnrealBridgeNav, Warning, TEXT("ExportNavMeshToObj: %s"), *OutSummary);
		return false;
	}

	ARecastNavMesh* NavMesh = BridgeNavImpl::FindRecastNavMesh(World);
	if (!NavMesh)
	{
		OutSummary = TEXT("no ARecastNavMesh in current level");
		UE_LOG(LogUnrealBridgeNav, Warning, TEXT("ExportNavMeshToObj: %s (world=%s)"),
			*OutSummary, *World->GetName());
		return false;
	}
	UE_LOG(LogUnrealBridgeNav, Log, TEXT("ExportNavMeshToObj: found navmesh=%s tiles=%d"),
		*NavMesh->GetName(), NavMesh->GetNavMeshTilesCount());

	// Pull aggregate geometry across all tiles in one call. Passing a default
	// FNavTileRef tells the Recast impl to walk every loaded tile and merge
	// into one vertex/index buffer (same path the navmesh rendering
	// component uses — see NavMeshRenderingComponent.cpp ~line 704).
	FRecastDebugGeometry AggregateGeom;
	NavMesh->BeginBatchQuery();
	NavMesh->GetDebugGeometryForTile(AggregateGeom, FNavTileRef());
	NavMesh->FinishBatchQuery();

	// AreaIndices[] holds the permanent navigable surface partitioned by
	// recast area id. BuiltMeshIndices is a transient "currently-being-built"
	// overlay (PImplRecastNavMesh.cpp line ~3118: populated only when
	// bIsBeingBuilt is true), so merging both would double-count any tile
	// that's mid-rebuild. We take AreaIndices only.
	TArray<FVector>& AllVerts = AggregateGeom.MeshVerts;
	TArray<int32> AllTriIndices;
	for (int32 AreaIdx = 0; AreaIdx < RECAST_MAX_AREAS; ++AreaIdx)
	{
		AllTriIndices.Append(AggregateGeom.AreaIndices[AreaIdx]);
	}
	const int32 TileCount = 1; // aggregated across all tiles in one call

	if (AllVerts.Num() == 0 || AllTriIndices.Num() == 0)
	{
		OutSummary = FString::Printf(TEXT("navmesh '%s' has no built tiles"), *NavMesh->GetName());
		UE_LOG(LogUnrealBridgeNav, Warning, TEXT("ExportNavMeshToObj: %s"), *OutSummary);
		return false;
	}

	// Resolve output path. Default: <Project>/Saved/NavMeshExport.obj.
	FString FinalPath = OutFilePath;
	if (FinalPath.IsEmpty())
	{
		FinalPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NavMeshExport.obj"));
	}
	FinalPath = FPaths::ConvertRelativePathToFull(FinalPath);

	// Write OBJ. UE uses cm, Z-up, left-handed; we preserve that coord system
	// rather than re-projecting. Consumers (Blender, Houdini) can import with
	// "UE" presets and remap axes there.
	FString ObjText;
	ObjText.Reserve(AllVerts.Num() * 32 + AllTriIndices.Num() * 12);
	ObjText += FString::Printf(TEXT("# UnrealBridge NavMesh export\n"));
	ObjText += FString::Printf(TEXT("# navmesh: %s\n"), *NavMesh->GetName());
	ObjText += FString::Printf(TEXT("# verts: %d  tris: %d  (tiles merged)\n"),
		AllVerts.Num(), AllTriIndices.Num() / 3);
	(void)TileCount; // reserved for per-tile export variant
	ObjText += FString::Printf(TEXT("o %s\n"), *NavMesh->GetName());

	for (const FVector& V : AllVerts)
	{
		ObjText += FString::Printf(TEXT("v %f %f %f\n"), V.X, V.Y, V.Z);
	}
	const int32 NumTris = AllTriIndices.Num() / 3;
	for (int32 T = 0; T < NumTris; ++T)
	{
		// OBJ is 1-indexed.
		const int32 A = AllTriIndices[T * 3 + 0] + 1;
		const int32 B = AllTriIndices[T * 3 + 1] + 1;
		const int32 C = AllTriIndices[T * 3 + 2] + 1;
		ObjText += FString::Printf(TEXT("f %d %d %d\n"), A, B, C);
	}

	if (!FFileHelper::SaveStringToFile(ObjText, *FinalPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutSummary = FString::Printf(TEXT("failed to write '%s'"), *FinalPath);
		UE_LOG(LogUnrealBridgeNav, Warning, TEXT("ExportNavMeshToObj: %s"), *OutSummary);
		return false;
	}

	OutSummary = FString::Printf(
		TEXT("wrote '%s' (navmesh=%s verts=%d tris=%d)"),
		*FinalPath, *NavMesh->GetName(), AllVerts.Num(), NumTris);
	UE_LOG(LogUnrealBridgeNav, Log, TEXT("ExportNavMeshToObj: %s"), *OutSummary);
	return true;
}

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

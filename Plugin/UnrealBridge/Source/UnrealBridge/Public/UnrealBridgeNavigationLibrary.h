#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeNavigationLibrary.generated.h"

/**
 * Editor-side navigation introspection and export.
 *
 * Surfaces ARecastNavMesh debug geometry (the same triangles UE renders for
 * `show navmesh` in viewport) so external tools can consume the navmesh
 * without having to reverse the binary tile format.
 */
UCLASS()
class UUnrealBridgeNavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Export the current editor world's NavMesh as a Wavefront OBJ.
	 *
	 * @param OutFilePath  Absolute path to write to (will overwrite).
	 *                     If empty, defaults to <Project>/Saved/NavMeshExport.obj.
	 * @param OutSummary   Receives a one-line summary on success
	 *                     (verts, tris, output path, navmesh name).
	 * @return true on successful write; false if no NavMesh exists in the
	 *         editor world or the file write failed.
	 *
	 * Cost: O(navmesh tile count + total triangles). Single GameThread call;
	 * GetDebugGeometry on a typical level (~50k tris) completes in <100 ms.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Navigation")
	static bool ExportNavMeshToObj(const FString& OutFilePath, FString& OutSummary);
};

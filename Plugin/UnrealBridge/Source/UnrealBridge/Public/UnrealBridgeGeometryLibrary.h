#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeGeometryLibrary.generated.h"

class UMaterialInterface;
class UTexture2D;

/**
 * Static facts about a UDynamicMesh — what M4-6 returns. Field names follow
 * the standard UE Python snake_case projection, so e.g. `bHasNormals` becomes
 * `.has_normals` (NOT `.b_has_normals` — see feedback_ue_python_bool_prefix).
 */
USTRUCT(BlueprintType)
struct FBridgeMeshInfo
{
	GENERATED_BODY()

	/** Triangle count from `UDynamicMesh::GetTriangleCount()` (NOT NumTriangleIDs — gaps after deletes excluded). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Geometry")
	int32 NumTriangles = 0;

	/** Vertex count from `MeshQueryFunctions::GetVertexCount` (NOT NumVertexIDs — gaps after deletes excluded). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Geometry")
	int32 NumVertices = 0;

	/** Number of UV channels (`MeshQueryFunctions::GetNumUVSets`). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Geometry")
	int32 NumUVLayers = 0;

	/** True when the mesh has the per-triangle Normals attribute enabled (`GetHasTriangleNormals`). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Geometry")
	bool bHasNormals = false;

	/** True when the Vertex Colors attribute is enabled (`GetHasVertexColors`). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Geometry")
	bool bHasVertexColors = false;

	/** Local-space bounding box (`MeshQueryFunctions::GetMeshBoundingBox`). Empty when handle is invalid. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Geometry")
	FBox Bounds = FBox(ForceInit);
};

/**
 * Geometry Script wrapper — Lane 2 of the procedural-content roadmap.
 *
 * Design contract (see docs/plans/procedural-content-roadmap.md §4):
 *  - UDynamicMesh handles are int32 keys into a process-global map. The
 *    map holds TStrongObjectPtr<UDynamicMesh> so handles survive GC for
 *    as long as the bridge keeps them registered. Caller must Release()
 *    when done.
 *  - All ops run on GameThread (bridge worker dispatches there); large
 *    sweeps (M5-6 voxel merge, M5-2 boolean on dense meshes) can block
 *    the GT for seconds. Split them across exec calls per
 *    feedback_bridge_exec_holds_gamethread.
 *  - The whole library is gated to UE 5.7+. On 5.4-5.6 the UFUNCTIONs
 *    still exist (UHT requires unconditional declarations) but their
 *    bodies live in UnrealBridgeGeometryLibrary_Stubs.cpp and log a
 *    warning + return T{}. See feedback_uht_no_if_around_reflection.
 *  - Asset save UFUNCTIONs (M4-4 / M4-5) must run in editor world only;
 *    invoking them from PIE / cmdlet silently no-ops.
 *
 * UFUNCTIONs land in three batches:
 *   Phase 2 — M4: handle pool + asset I/O (8 ops) — THIS BATCH
 *   Phase 3 — M5/M6 P2 priority: boolean / smooth / decimate / recompute_normals (4 ops)
 *   Future  — M5 rest + M6-1/2 bake (12 ops)
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeGeometryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ─── M4 — Handle pool + asset I/O ────────────────────────────

	/**
	 * Allocate a fresh, empty UDynamicMesh and register it under a new int32 handle.
	 * Caller must release the handle via ReleaseDynamicMesh when done.
	 *
	 * @return Positive handle (1+); 0 indicates allocation failure (very unlikely).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static int32 CreateDynamicMesh();

	/**
	 * Drop a handle from the registry. The underlying UDynamicMesh becomes eligible
	 * for GC once no other reference holds it. Idempotent — releasing an unknown
	 * handle just returns false.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool ReleaseDynamicMesh(int32 Handle);

	/**
	 * List all currently-registered handles. Useful for debugging leaks ("did I
	 * forget to release?") or batch cleanup before shutdown.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static TArray<int32> ListDynamicMeshHandles();

	/**
	 * Replace the contents of the handle's mesh with the geometry of a StaticMesh asset.
	 * Wraps `UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMeshV2` with
	 * default options (apply build settings, request tangents, use section materials).
	 *
	 * @param Handle    Bridge handle (must be live).
	 * @param AssetPath Content path of the StaticMesh asset, e.g. "/Engine/BasicShapes/Cube".
	 * @param Lod       LOD index against the source-model LOD chain. Silently clamped
	 *                  to the available LOD count.
	 * @return true on Success outcome from Geometry Script; false on Failure (asset not
	 *         loadable, handle invalid, source-model LODs unavailable at runtime).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool LoadMeshFromStaticMesh(int32 Handle, const FString& AssetPath, int32 Lod = 0);

	/**
	 * Pull mesh geometry from a primitive component on a level actor. Prefer this over
	 * LoadMeshFromStaticMesh when the actor's component has runtime overrides
	 * (instance-specific materials, deformations, etc.). Wraps
	 * `USceneUtilityFunctions::CopyMeshFromComponent` with default options
	 * (want normals + tangents + UVs + colors).
	 *
	 * @param ActorLabel    User-visible label or FName of the level actor (matches
	 *                      `UnrealBridgeLevelLibrary` resolver semantics).
	 * @param ComponentName FName of the component on that actor. Empty string falls
	 *                      back to the root component.
	 * @param Handle        Target bridge handle (must be live).
	 * @return true on Success outcome; false if actor / component / handle is unknown.
	 *
	 * Editor world only; invoking from PIE returns false + logs a warning.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool LoadMeshFromComponent(const FString& ActorLabel, const FString& ComponentName, int32 Handle);

	/**
	 * Save the handle's mesh as a brand-new StaticMesh asset on disk. Wraps
	 * `UGeometryScriptLibrary_CreateNewAssetUtilityFunctions::CreateNewStaticMeshAssetFromMesh`.
	 * Editor-only — `WITH_EDITOR` gate in the body returns "" outside the editor.
	 *
	 * Per project_split_asset_ops feedback: do NOT chain this with mesh ops in a
	 * single bridge exec. CreateNewStaticMeshAssetFromMesh triggers an asset-reference
	 * completing modal that deadlocks the GameThread when other ops are waiting in
	 * the queue. Pattern:
	 *   exec1: load + transform/edit; exec2: save_to_new
	 *
	 * @param Handle       Source bridge handle.
	 * @param NewAssetPath Content path for the new asset, e.g. "/Game/_BridgeTest/SM_Out".
	 * @param MaterialList Optional materials for the new asset's slots. Order maps
	 *                     1:1 to the dynamic mesh's MaterialIDs (slot 0 ← idx 0, etc.).
	 *                     Empty list → engine assigns default material to every slot.
	 * @return The saved asset's package path on Success; "" on Failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static FString SaveMeshToNewStaticMesh(int32 Handle, const FString& NewAssetPath, const TArray<UMaterialInterface*>& MaterialList);

	/**
	 * Push the handle's mesh INTO an existing StaticMesh asset, replacing its
	 * source-model LOD0 geometry. Wraps `UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh`.
	 * Editor-only.
	 *
	 * Calls `Modify()` + `MarkPackageDirty()` on the target so undo + Save All pick
	 * up the change. Caller is still responsible for `editor_asset_lib.save_loaded_asset(...)`
	 * if persistence to disk is required.
	 *
	 * @param Handle             Source bridge handle.
	 * @param ExistingAssetPath  Content path to an EXISTING StaticMesh asset.
	 * @param bReplaceMaterials  When true, the dynamic mesh's MaterialIDs replace the
	 *                           existing material slot list. When false, slot mapping
	 *                           is preserved by section index (most common).
	 * @return true on Success outcome; false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool SaveMeshToExistingStaticMesh(int32 Handle, const FString& ExistingAssetPath, bool bReplaceMaterials);

	/** Static facts about the mesh held by Handle. Returns an all-zero struct when handle invalid. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static FBridgeMeshInfo GetMeshInfo(int32 Handle);

	/**
	 * Read local mesh positions as contiguous triangle triples (A, B, C), in cm.
	 * Vertex/triangle IDs are not preserved. MaxTriangles must be 1..1000000;
	 * invalid handles, invalid limits or meshes exceeding the limit return an empty array.
	 * No mesh or asset is modified.
	 * 按连续三角形三元组读取网格局部顶点，单位 cm，不保留原始顶点或三角形 ID。
	 * MaxTriangles 有效范围为 1..1000000；句柄无效、范围无效或超限时返回空数组，不修改网格及资产。
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static TArray<FVector> GetMeshTriangleVertices(int32 Handle, int32 MaxTriangles = 100000);

	/**
	 * Append a copy of SourceHandle to TargetHandle using Transform (local positions in cm).
	 * UVs and mesh attributes are copied without resampling; disconnected pieces remain disconnected.
	 * SourceHandle is unchanged. Invalid handles, self-append or an invalid transform return false.
	 * 将源网格的副本按 Transform 追加到目标网格，局部坐标单位 cm；保留 UV 与属性，不重采样，不焊接独立部件。
	 * 不修改源网格；无效句柄、自身追加或无效变换返回 false。两个句柄均需由调用方释放。
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool AppendDynamicMesh(int32 TargetHandle, int32 SourceHandle, FTransform Transform);

	// ─── M5/M6 — P2 priority subset (boolean / smooth / decimate / recompute_normals) ─

	/**
	 * Boolean two meshes in-place on `HandleA`. `HandleB` is the tool mesh and is
	 * untouched. Wraps `MeshBooleanFunctions::ApplyMeshBoolean` with default options
	 * (fill holes, simplify output, no weld). Both transforms are identity — apply
	 * `mesh_transform` (Phase 3+) ahead of time if the meshes need world-space
	 * placement to overlap correctly.
	 *
	 * @param HandleA Target mesh (modified in place).
	 * @param HandleB Tool mesh (read-only).
	 * @param Op One of "union" / "intersect" / "intersection" / "subtract"
	 *           (case-insensitive). Unknown values fail loudly + return false.
	 * @return true on Success outcome.
	 *
	 * Cost: O(NA × NB) worst case for the BSP tree intersection. Dense meshes
	 * (>50k tri × 50k tri) can block the GameThread for several seconds — split
	 * into separate exec calls for chained ops (per feedback_bridge_exec_holds_gamethread).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshBoolean(int32 HandleA, int32 HandleB, const FString& Op);

	/**
	 * Iterative Laplacian smoothing in-place. Wraps `MeshDeformFunctions::ApplyIterativeSmoothingToMesh`
	 * with empty selection (whole mesh) and `Alpha` interpreted as `Strength`
	 * (clamped to [0, 1]).
	 *
	 * @param Handle Target mesh.
	 * @param Iterations Smoothing passes (≥1). Higher = smoother but more shrinkage.
	 *                   Engine default is 10; common practical range 2-20.
	 * @param Strength Per-iteration weight in [0, 1]. 0 = no movement; 1 = full
	 *                 Laplacian step. Engine default is 0.2.
	 * @return true on Success outcome.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshSmooth(int32 Handle, int32 Iterations, float Strength);

	/**
	 * Reduce mesh down to a target triangle count. Wraps
	 * `MeshSimplifyFunctions::ApplySimplifyToTriangleCount` with default
	 * AttributeAware options.
	 *
	 * @param Handle Target mesh.
	 * @param TargetTris Desired final triangle count (≥4). The simplifier may
	 *                   not hit it exactly — typically lands within ±5%.
	 * @return true on Success outcome.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshDecimate(int32 Handle, int32 TargetTris);

	/**
	 * Recompute normals and tangents in-place. When `AngleThresholdDeg > 0`,
	 * applies hard-edge splitting first (`ComputeSplitNormals` with the given
	 * opening angle) — this is what you almost always want after a boolean,
	 * decimate, or sweep, since those operations leave smooth-shaded seams
	 * everywhere. With `AngleThresholdDeg ≤ 0`, falls back to plain
	 * `RecomputeNormals` (no hard edges).
	 *
	 * Tangents are always recomputed via `ComputeTangents` after the normal step.
	 *
	 * @param Handle Target mesh.
	 * @param AngleThresholdDeg Opening-angle threshold (degrees) for hard-edge
	 *                          detection. Common values: 30-60 for organic, 45
	 *                          for hard-surface. Pass 0 or negative to skip
	 *                          split-normals.
	 * @return true on Success outcome.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool RecomputeNormalsAndTangents(int32 Handle, float AngleThresholdDeg);

	// ─── M5-1 — Primitive append ─────────────────────────────────

	/**
	 * Append an axis-aligned box primitive centered at `Origin`. Wraps
	 * `MeshPrimitiveFunctions::AppendBox` with `Origin = Center` and the
	 * supplied `Size` mapped to (DimensionX, DimensionY, DimensionZ).
	 *
	 * @param Handle Target mesh.
	 * @param Origin Box center in mesh-local space.
	 * @param Size   Total extent (e.g. 100 → 100 cm box). Each component clamped ≥ 0.
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool AppendBox(int32 Handle, FVector Origin, FVector Size);

	/**
	 * Append a UV-mapped sphere via `AppendSphereLatLong`. Origin = Center.
	 *
	 * @param Handle        Target mesh.
	 * @param Origin        Sphere center in mesh-local space.
	 * @param Radius        cm.
	 * @param ResolutionUV  Theta steps (longitude). Phi steps (latitude) auto-set to
	 *                      ResolutionUV/2 (typical 2:1 longitude:latitude aspect).
	 *                      Common values: 16 (low) / 32 (med) / 64 (hi-poly).
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool AppendSphere(int32 Handle, FVector Origin, float Radius, int32 ResolutionUV);

	/**
	 * Append a capped cylinder along +Z. Origin = Base; Height extends along +Z.
	 *
	 * @param Handle         Target mesh.
	 * @param Origin         Base-center in mesh-local space.
	 * @param Radius         cm.
	 * @param Height         cm along +Z.
	 * @param RadialSegments Steps around the circumference (≥3). Engine default 12.
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool AppendCylinder(int32 Handle, FVector Origin, float Radius, float Height, int32 RadialSegments);

	/**
	 * Append a true cone (TopRadius = 0). For frustums, follow up with `mesh_transform`
	 * + a manual append. Origin = Base; apex at Origin + Z*Height.
	 *
	 * @param Handle         Target mesh.
	 * @param Origin         Base-center in mesh-local space.
	 * @param BaseRadius     cm.
	 * @param Height         cm along +Z.
	 * @param RadialSegments Steps around base ring (≥3). Engine default 12.
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool AppendCone(int32 Handle, FVector Origin, float BaseRadius, float Height, int32 RadialSegments);

	// ─── M5-8 — Transform ────────────────────────────────────────

	/**
	 * Apply a full transform (translation + rotation + scale) to every vertex.
	 * Wraps `MeshTransformFunctions::TransformMesh` with the orientation-flip-on-
	 * negative-scale fix enabled (engine default).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshTransform(int32 Handle, FTransform Transform);

	// ─── M5-9 — Uniform remesh (a.k.a. "triangulate" in roadmap shorthand) ─

	/**
	 * Re-tessellate the mesh to approximately `TargetTriCount` triangles using
	 * uniform remeshing — produces an even tri-density distribution and is the
	 * standard "clean up topology" pass after sweep / boolean / decimate. Wraps
	 * `RemeshingFunctions::ApplyUniformRemesh`.
	 *
	 * Caveat: not deterministic; output count is approximate; can be expensive
	 * for high tri counts. Engine doc says "future-version output may differ".
	 *
	 * @param Handle         Target mesh.
	 * @param TargetTriCount Approximate desired triangle count (≥4).
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshUniformRemesh(int32 Handle, int32 TargetTriCount);

	// ─── M5-3 — Texture-driven displacement ──────────────────────

	/**
	 * Displace mesh vertices along normals using a heightmap texture. Wraps
	 * `MeshDeformFunctions::ApplyDisplaceFromTextureMap`.
	 *
	 * @param Handle      Target mesh.
	 * @param TexturePath Content path of a UTexture2D. **Must be set to**
	 *                    `CompressionSettings = TC_VectorDisplacementmap` (and
	 *                    `sRGB = false`) so the runtime can read raw color —
	 *                    default RuntimeBulkData compression hides the source
	 *                    pixels (roadmap pit #11).
	 * @param Magnitude   Displacement scale in cm. Engine default 1.0 — bump to
	 *                    10-100 for visible displacement on cm-scale meshes.
	 * @param UVChannel   Which UV layer's coordinates drive the texture sample.
	 *                    Defaults to 0; mesh must have at least UVChannel+1 layers
	 *                    (mesh_uv_unwrap can populate channel 0 if missing).
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshDisplaceFromTexture(int32 Handle, const FString& TexturePath, float Magnitude, int32 UVChannel = 0);

	// ─── M5-6 — Voxel merge / solidify ───────────────────────────

	/**
	 * Voxel-solidify a chain of meshes into a single watertight result. The first
	 * handle in `Handles` is the merge target; remaining handles are appended into
	 * it via `AppendMesh` (identity transform), then `ApplyMeshSolidify` runs on
	 * the combined mesh with `GridCellSize=CellSizeCm`.
	 *
	 * Output writes back into `Handles[0]`. Other handles are not modified.
	 *
	 * @param Handles    Ordered handle list. `Handles[0]` is mutated.
	 * @param CellSizeCm Voxel grid cell size in cm. Smaller = higher fidelity but
	 *                   exponentially more memory + time. 0.5-2.0 cm typical for
	 *                   character-scale; 5-20 cm for environment-scale.
	 * @return true on success.
	 *
	 * Cost: O((bounds_volume / cell_size³)). 1cm cells on a 1m³ region = 1M voxels;
	 * easily blocks the GameThread for seconds. Per
	 * feedback_bridge_exec_holds_gamethread, isolate dense voxel calls in their
	 * own bridge exec.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshVoxelMerge(const TArray<int32>& Handles, float CellSizeCm);

	// ─── M5-7 — Auto UV unwrap (box / cylinder / plane projections) ─

	/**
	 * Generate UVs on UV channel 0 by projection. Wraps `MeshUVFunctions::Set-
	 * MeshUVsFrom*Projection`. The projection volume is centered on the mesh's
	 * local-space bounding box and sized to match it by default.
	 *
	 * @param Handle Target mesh.
	 * @param Method Case-insensitive: `"box"` (tri-planar projection, hardest
	 *               UVs but no stretching), `"cylinder"` (good for capsule-like
	 *               meshes; SplitAngle=45°), `"plane"` (single planar projection
	 *               from +Z, simplest but worst on non-planar geometry).
	 * @param ProjectionSizeCm 0 keeps bounding-box sizing; >0 uses a uniform
	 *               projection cube of this side length in cm. Negative/nonfinite values fail.
	 *               0 使用原包围盒尺寸；正值按指定厘米边长统一投影，保持纹理密度；负值或非有限值失败。
	 * @return true on success. Unknown method strings fail loudly + return false.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool MeshUVUnwrap(int32 Handle, const FString& Method, float ProjectionSizeCm = 0.0f);

	// ─── M5-10/11 — Selection state + extrude ────────────────────

	/**
	 * Select triangles whose face-normal lies within `MaxAngleDeg` of `Normal`.
	 * Returns a process-global selection id (≥1) that subsequent calls
	 * (`extrude_selection`, future selection-consuming ops) can reference.
	 * Caller must `release_selection(id)` when done.
	 *
	 * Roadmap M5-10 originally said "select_by_plane" — the Geometry Script API
	 * exposes only normal-direction selection (`SelectMeshElementsByNormalAngle`),
	 * which covers the common "select the top of the mesh" / "select front-facing"
	 * use cases. For arbitrary plane-side selection, fall back to a future
	 * `EditMesh` lambda primitive.
	 *
	 * @param Handle      Source mesh.
	 * @param Normal      Reference normal direction (will be normalized internally).
	 * @param MaxAngleDeg Half-angle cone around `Normal` in degrees. 0 = exact
	 *                    match (almost never matches anything); 30-90 typical.
	 * @return Selection id (≥1) on success; 0 on failure (handle invalid or
	 *         empty selection).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static int32 SelectByNormalDirection(int32 Handle, FVector Normal, float MaxAngleDeg);

	/** Drop a selection id. Idempotent — releasing an unknown id returns false. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool ReleaseSelection(int32 SelectionId);

	/** All currently-registered selection ids (sorted ascending). Debug aid. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static TArray<int32> ListSelections();

	/**
	 * Linear-extrude the triangles identified by `SelectionId` along their
	 * averaged normal by `Distance` cm. Wraps `MeshModelingFunctions::ApplyMesh-
	 * LinearExtrudeFaces` with `DirectionMode=AveragedFaceNormal` (each selected
	 * face moves along its own average normal — gives natural-looking growth on
	 * curved surfaces; uniform Z extrusion is also possible by passing a fixed
	 * `Direction`, not exposed here).
	 *
	 * @param Handle      Target mesh.
	 * @param SelectionId Live selection id from `select_by_normal_direction`.
	 * @param Distance    cm along the per-face averaged normal.
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool ExtrudeSelection(int32 Handle, int32 SelectionId, float Distance);

	// ─── M5-12 — Sweep along spline ──────────────────────────────

	/**
	 * Append a swept-polygon mesh by sampling a spline component on a level
	 * actor. Wraps `MeshPrimitiveFunctions::AppendSweepPolygon`. Sample count
	 * controls path resolution.
	 *
	 * @param Handle        Target mesh (geometry is appended; existing geometry
	 *                      is preserved).
	 * @param ProfileXY     Cross-section polygon in XY plane (cm). Vertices in
	 *                      CCW order. Min 3 verts.
	 * @param ActorLabel    Level actor that owns the spline component.
	 * @param ComponentName FName of the USplineComponent on the actor. Empty →
	 *                      first USplineComponent found.
	 * @param NumPathSamples Number of evenly-spaced transforms sampled along
	 *                      the spline (≥2). Higher = smoother sweep, more tris.
	 *                      Engine default for cylinder-like sweeps is ~32.
	 * @return true on success.
	 *
	 * Editor world only.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool SweepAlongSpline(
		int32 Handle,
		const TArray<FVector2D>& ProfileXY,
		const FString& ActorLabel,
		const FString& ComponentName,
		int32 NumPathSamples = 32);

	// ─── M6-1/M6-2 — Bake → texture / vertex colors ──────────────

	/**
	 * Bake the mesh's tangent-space normals into a new Texture2D asset on disk.
	 * Same-mesh bake (target=source). Wraps `MeshBakeFunctions::BakeTexture` with
	 * `[MakeBakeTypeTangentNormal()]`, then `CreateNewAssetFunctions::CreateNew-
	 * Texture2DAsset` to persist the transient bake result.
	 *
	 * Editor-world only (`WITH_EDITOR` gate). Per `feedback_split_asset_ops`,
	 * keep this in its own bridge exec — texture asset creation triggers a
	 * modal on the GameThread.
	 *
	 * @param Handle           Source mesh (must have UVs on channel 0).
	 * @param NewTexturePath   Content path, e.g. `/Game/Bakes/T_Normals`.
	 * @param Resolution       Texture edge size in px. Mapped to the closest
	 *                         `EGeometryScriptBakeResolution` (16-8192). Values
	 *                         outside that range fall back to 256.
	 * @return Saved asset path on success; "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static FString BakeNormalsToTexture(int32 Handle, const FString& NewTexturePath, int32 Resolution);

	/**
	 * Bake ambient occlusion into the mesh's vertex colors in-place. Same-mesh
	 * bake. Wraps `MeshBakeFunctions::BakeVertex` with `OutputMode=RGBA`,
	 * `RGBA=MakeBakeTypeAmbientOcclusion(...)`.
	 *
	 * @param Handle         Target mesh.
	 * @param OcclusionRays  Hemisphere sample count (engine default 16; 16-64
	 *                       typical, 256 for high-quality).
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Geometry")
	static bool BakeOcclusionToVertexColor(int32 Handle, int32 OcclusionRays = 16);
};

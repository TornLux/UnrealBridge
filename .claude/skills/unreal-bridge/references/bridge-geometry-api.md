# bridge-geometry-api

`unreal.UnrealBridgeGeometryLibrary` — Lane 2 of the procedural-content roadmap. Wraps the runtime + editor Geometry Script libraries to give agents a `UDynamicMesh`-based authoring pipeline (load static mesh → operate → save). See `docs/plans/procedural-content-roadmap.md` §4 for the full design contract.

Hard contract:
- All ops require **UE 5.7+**. On 5.4-5.6 every UFUNCTION returns its zero value + logs a warning (compiled-in stub bodies; see `feedback_uht_no_if_around_reflection`).
- Asset-save ops (`save_mesh_to_new_static_mesh` / `save_mesh_to_existing_static_mesh`) are **editor-world only** — invoking from PIE / cmdlet returns "" / false.
- Mesh handles are **process-global integers** mapped to `UDynamicMesh*` via `TStrongObjectPtr` (GC-safe). Caller must `release_dynamic_mesh(handle)` when done; handles are never reused (monotonic counter), so stale handles fail loudly rather than aliasing fresh meshes.
- Big mesh ops (boolean / voxel-merge on dense meshes) block the GameThread. Per `feedback_bridge_exec_holds_gamethread`, split heavy chains across exec calls.
- Per `feedback_split_asset_ops`, **never** chain `create_dynamic_mesh` + `load_*` + `save_*` in a single bridge exec — `CreateNewStaticMeshAssetFromMesh` opens an asset-reference completing modal that deadlocks the bridge if other ops are queued behind it. Pattern:
  - exec1: `create → load → ops → get_info`
  - exec2: `save_to_new_static_mesh → release`

## What's shipped

**Phase 2 — M4 (8 ops + 1 USTRUCT)**:
- handle pool: `create_dynamic_mesh`, `release_dynamic_mesh`, `list_dynamic_mesh_handles`
- ingest: `load_mesh_from_static_mesh`, `load_mesh_from_component`
- save: `save_mesh_to_new_static_mesh`, `save_mesh_to_existing_static_mesh`
- query: `get_mesh_info` → `FBridgeMeshInfo`

**Phase 3 — M5/M6 P2 priority (4 ops)**:
- `mesh_boolean` (M5-2)
- `mesh_smooth` (M5-4)
- `mesh_decimate` (M5-5)
- `recompute_normals_and_tangents` (M6-3)

**Phase 4 — primitives + transform + uniform remesh (6 ops)**:
- `append_box`, `append_sphere`, `append_cylinder`, `append_cone` (M5-1)
- `mesh_transform` (M5-8)
- `mesh_uniform_remesh` (M5-9, "triangulate" in roadmap shorthand)

**Phase 5 — displace + voxel + UV unwrap (3 ops)**:
- `mesh_displace_from_texture` (M5-3)
- `mesh_voxel_merge` (M5-6) — multi-handle merge → solidify
- `mesh_uv_unwrap` (M5-7) — box / cylinder / plane projections

**Phase 6 — selection + extrude + sweep (5 ops, 3 user-facing + 2 selection-pool helpers)**:
- `select_by_normal_direction` (M5-10) — wraps SelectMeshElementsByNormalAngle
- `release_selection`, `list_selections` — selection-pool helpers (parallel pool to mesh handles)
- `extrude_selection` (M5-11)
- `sweep_along_spline` (M5-12)

**Phase 7 — bake (2 ops, closes Lane 2 23/23)**:
- `bake_normals_to_texture` (M6-1) — same-mesh tangent-space normal bake → new Texture2D asset
- `bake_occlusion_to_vertex_color` (M6-2) — same-mesh AO bake → vertex colors in-place

Lane 2 = 23/23 ops shipped. Lane 3 PCG read+trigger lands separately.

---

## FBridgeMeshInfo

Static facts about a `UDynamicMesh`. Field names follow the standard UE Python projection (`bHasNormals` → `.has_normals`).

| Python field | C++ source | Notes |
|---|---|---|
| `num_triangles` | `UDynamicMesh::GetTriangleCount()` | Triangle count after compaction; not `NumTriangleIDs`. |
| `num_vertices` | `MeshQueryFunctions::GetVertexCount` | Vertex count after compaction. |
| `num_uv_layers` | `MeshQueryFunctions::GetNumUVSets` | 0 when the mesh has no UV attribute set. |
| `has_normals` | `MeshQueryFunctions::GetHasTriangleNormals` | The Normals attribute (split-normals capable). |
| `has_vertex_colors` | `MeshQueryFunctions::GetHasVertexColors` | |
| `bounds` | `MeshQueryFunctions::GetMeshBoundingBox` | Local-space `unreal.Box`. Empty when handle invalid. |

---

## create_dynamic_mesh() -> int

Allocate a fresh, empty `UDynamicMesh` and register it under a new int handle.

Returns: positive int handle (≥1); 0 indicates allocation failure.

**Cost** — O(1).

**Example**
```python
import unreal as u
G = u.UnrealBridgeGeometryLibrary
h = G.create_dynamic_mesh()
print('handle:', h)
```

**Pitfalls**
- Handles are never reused — every call increments a process-global counter. After 2³¹ calls (~2.1B) the int wraps; not realistic in practice.
- Mesh is empty until populated by `load_mesh_from_*` or (Phase 3+) primitive `append_*` ops.

---

## release_dynamic_mesh(handle) -> bool

Drop a handle from the registry. The underlying `UDynamicMesh` becomes eligible for GC. Idempotent.

Returns: true if the handle was registered (and is now released), false if it was unknown.

**Pitfalls**
- Releasing a stale handle returns false — that's a no-op, not an error. Use it before reusing handle slots in batch jobs.

---

## list_dynamic_mesh_handles() -> list[int]

All currently-registered handles. Sorted ascending. Useful for leak detection ("did I forget to release?") and end-of-session cleanup.

---

## get_mesh_triangle_vertices(handle, max_triangles=100000) -> list[Vector]

`get_mesh_triangle_vertices(handle, max_triangles=100000)` reads local-space cm
positions in contiguous `(A, B, C)` triangle triples without changing the mesh.
The limit is 1..1000000 triangles; invalid handles/limits and over-limit meshes
return an empty array. IDs are not preserved. Use it for geometric measurements
and cross sections after loading a mesh, then release the handle normally.
局部三角形顶点读取用于测量与截面提取；返回连续三元组，不保留 ID，不修改源资产。

`mesh_uv_unwrap(handle, method, projection_size_cm=0)` keeps its existing
bounding-box projection when the size is zero. A positive size uses a uniform
projection cube in cm, so new procedural parts can match a fixed texture density;
negative or nonfinite sizes fail. 指定正值边长可固定生成构件的 UV 密度，0 保留旧行为。

## append_dynamic_mesh(target_handle, source_handle, transform) -> bool

`append_dynamic_mesh(target_handle, source_handle, transform)` appends a copy
with the given local-space transform (positions in cm), preserving UVs and
attributes without welding disconnected pieces or changing the source handle.
Invalid handles, self-append or an invalid transform return false. Release both
handles after use. 用于保留原 UV 的裁切部件拼合，不会重采样或修改源网格。

## load_mesh_from_static_mesh(handle, asset_path, lod=0) -> bool

Replace the handle's mesh with the geometry of a `UStaticMesh` asset. Wraps `CopyMeshFromStaticMeshV2` with default options (apply build settings, request tangents, use section materials).

| Param | Type | Notes |
|---|---|---|
| `handle` | int | Live handle. |
| `asset_path` | str | Content path, e.g. `/Engine/BasicShapes/Cube`. |
| `lod` | int (default 0) | Source-model LOD index. Silently clamped to available count. |

Returns: true on Geometry Script `Success` outcome.

**Pitfalls**
- Source-model LODs are **not** available at runtime — the call works in editor but in cooked builds you'd need `EGeometryScriptLODType::RenderData` (not exposed yet — file an issue if you need cooked-build support).
- Failure cases that return false: asset path unloadable, handle invalid, requested LOD genuinely missing.

**Example**
```python
ok = G.load_mesh_from_static_mesh(h, '/Engine/BasicShapes/Cube', 0)
assert ok
```

---

## load_mesh_from_component(actor_label, component_name, handle) -> bool

Pull mesh geometry from a primitive component on a level actor (editor world). Useful when the actor's component has runtime overrides — instance materials, dynamic mesh components, deformation modifiers — that don't exist in the underlying asset. Wraps `SceneUtilityFunctions::CopyMeshFromComponent` with normals + tangents + UVs + colors enabled.

| Param | Type | Notes |
|---|---|---|
| `actor_label` | str | User-visible label or FName (matches `UnrealBridgeLevelLibrary` resolver). |
| `component_name` | str | Component FName / GetName. Empty string falls back to root component. |
| `handle` | int | Target bridge handle. |

Returns: true on Success.

**Pitfalls**
- Editor world only — invoking from PIE returns false. For PIE-side mesh capture, use the runtime trace family from `UnrealBridgeLevelLibrary`.
- The Local-to-World transform is consumed internally (we always use the local-space mesh for further authoring); pass through `mesh_transform` (Phase 3+) if you need world-space placement.

---

## save_mesh_to_new_static_mesh(handle, new_asset_path, material_list) -> str

Save the handle's mesh as a brand-new `UStaticMesh` asset on disk via `CreateNewStaticMeshAssetFromMesh`.

| Param | Type | Notes |
|---|---|---|
| `handle` | int | Source handle. |
| `new_asset_path` | str | Content path, e.g. `/Game/_BridgeTest/SM_Out`. |
| `material_list` | `list[UMaterialInterface]` | Optional. Maps 1:1 to MaterialIDs in the dynamic mesh — slot N ← list[N]. Empty list → engine default material in every slot. |

Returns: the saved asset's package path on Success; empty string on Failure.

**Editor-world only** (`WITH_EDITOR` gate). Returns "" outside the editor.

**Pitfalls**
- Per `feedback_split_asset_ops` — **must** be the only asset-write op in its bridge exec. The function opens an asset-reference completing modal that deadlocks the bridge if other ops are queued.
- Default options are conservative: Nanite off, no normal/tangent recompute, collision on, default trace flag. Run `recompute_normals_and_tangents` (Phase 3+) before saving if your edits invalidated normals.
- The function creates the package; caller doesn't need to `editor_asset_lib.save_loaded_asset(...)` separately.

**Example (split exec form)**
```python
# exec1: build the mesh
import unreal as u; G = u.UnrealBridgeGeometryLibrary
h = G.create_dynamic_mesh()
G.load_mesh_from_static_mesh(h, '/Engine/BasicShapes/Cube', 0)
print(G.get_mesh_info(h))
```
```python
# exec2: save + release
import unreal as u; G = u.UnrealBridgeGeometryLibrary
hs = list(G.list_dynamic_mesh_handles()); h = hs[-1]
print(G.save_mesh_to_new_static_mesh(h, '/Game/_BridgeTest/SM_CubeCopy', []))
G.release_dynamic_mesh(h)
```

---

## save_mesh_to_existing_static_mesh(handle, existing_asset_path, b_replace_materials) -> bool

Push the handle's mesh INTO an existing `UStaticMesh` asset, replacing its source-model LOD0 geometry. Wraps `CopyMeshToStaticMesh`.

| Param | Type | Notes |
|---|---|---|
| `handle` | int | Source handle. |
| `existing_asset_path` | str | Content path to an EXISTING StaticMesh. |
| `b_replace_materials` | bool | True → dynamic mesh's MaterialIDs replace existing slot list. False → preserve slot mapping by section index (most common). |

Returns: true on Success. Calls `Modify()` + `MarkPackageDirty()` so undo + Save All pick up the change.

**Editor-world only**.

**Pitfalls**
- After return, the asset is dirty in memory but not yet saved to disk. Caller still needs `editor_asset_lib.save_loaded_asset(asset_path)` for persistence.
- If the existing asset's slot count is smaller than the dynamic mesh's MaterialIDs and `b_replace_materials=False`, extra material IDs map to the highest-index slot.

---

## get_mesh_info(handle) -> FBridgeMeshInfo

Static facts about the mesh held by `handle`. See FBridgeMeshInfo above.

Returns the all-zero/empty struct when handle invalid (use `list_dynamic_mesh_handles()` to verify before calling).

**Cost** — O(NumVertices + NumTriangles) for the queries that aren't cached. ~1ms per 100k tris.

---

---

## mesh_boolean(handle_a, handle_b, op) -> bool

Boolean two meshes in-place on `handle_a`. `handle_b` is the tool mesh and is **not** modified. Wraps `MeshBooleanFunctions::ApplyMeshBoolean` with default options (fill holes, simplify output) and identity transforms on both sides.

| Param | Type | Notes |
|---|---|---|
| `handle_a` | int | Target (modified in place). |
| `handle_b` | int | Tool mesh (read-only). |
| `op` | str | `"union"` / `"intersect"` / `"intersection"` / `"subtract"` / `"difference"`. Case-insensitive. |

Returns: true on Success.

**Cost** — O(NA × NB) worst case for the BSP intersection. Dense pairs (>50k×50k tri) can block the GameThread for seconds; chain across separate execs.

**Pitfalls**
- Both meshes use identity transform — apply `mesh_transform` (not yet shipped) ahead of time if they need to be at world-space offsets to overlap meaningfully.
- After boolean, normals at the seam are smooth-shaded by default. Run `recompute_normals_and_tangents(handle, 45.0)` to add hard edges.
- Unknown `op` strings fail loudly: returns false + `LogTemp` warning.

---

## mesh_smooth(handle, iterations, strength) -> bool

Iterative Laplacian smoothing in-place over the whole mesh. Wraps `MeshDeformFunctions::ApplyIterativeSmoothingToMesh`.

| Param | Type | Notes |
|---|---|---|
| `handle` | int | Target. |
| `iterations` | int | ≥1. Engine default is 10; practical range 2-20 (more = smoother + more shrinkage). |
| `strength` | float | Per-iteration weight in [0, 1]. Engine default is 0.2. |

Returns: true on Success.

**Pitfalls**
- Heavy smoothing collapses small features. Run `mesh_decimate` first if you only want to remove fine noise without losing volumes.
- Doesn't preserve UV seams or hard edges — for that, recompute normals afterward.

---

## mesh_decimate(handle, target_tris) -> bool

Reduce mesh down to a target triangle count. Wraps `MeshSimplifyFunctions::ApplySimplifyToTriangleCount` with default `AttributeAware` options.

| Param | Type | Notes |
|---|---|---|
| `handle` | int | Target. |
| `target_tris` | int | Desired final count (≥4). Simplifier may not hit exactly — typically within ±5%. |

Returns: true on Success.

**Pitfalls**
- AttributeAware mode preserves UV / normals / vertex color attributes at the cost of fewer reachable simplification states. Pure-geometry decimation needs a different `Method` (not exposed in this phase).
- Below ~50 triangles the simplifier may refuse to go further — meshes that small are degenerate for most rendering pipelines.
- "Editor" simplifier variant (`ApplyEditorSimplifyToTriangleCount`) is higher quality but only available in editor; not exposed here. File a request if needed.

---

## recompute_normals_and_tangents(handle, angle_threshold_deg) -> bool

Recompute normals (with optional hard-edge splitting) + tangents in-place. Almost always required after boolean / decimate / sweep.

| Param | Type | Notes |
|---|---|---|
| `handle` | int | Target. |
| `angle_threshold_deg` | float | Opening angle in degrees. >0 → split normals at edges with greater opening angle (hard edges); ≤0 → plain `RecomputeNormals` (all soft). Common: 30-60 organic, 45 hard-surface. |

Returns: true on Success.

Internally always runs `ComputeTangents` after the normal step — RecomputeNormals doesn't touch the tangents overlay.

**Pitfalls**
- Without this after a boolean, the seam between unioned shapes will look smooth-shaded and "blobby". A 30° threshold gives natural-looking hard edges for most mechanical shapes.
- ComputeTangents requires UVs — if the mesh has 0 UV layers, tangent computation no-ops silently (still returns true on the normal step).

---

---

## append_box(handle, origin, size) -> bool / append_sphere(...) / append_cylinder(...) / append_cone(...)

Append a primitive shape to the mesh held by `handle`. Each wraps one of the
`MeshPrimitiveFunctions::Append*` calls with sane defaults and origin-mode set
appropriately (Center for box/sphere, Base for cylinder/cone).

| Function | Params (after `handle, origin`) | Wraps |
|---|---|---|
| `append_box` | `size: Vector` | `AppendBox` (Center origin) |
| `append_sphere` | `radius: float, resolution_uv: int` | `AppendSphereLatLong` (StepsTheta=resolution_uv, StepsPhi=resolution_uv/2, Center origin) |
| `append_cylinder` | `radius: float, height: float, radial_segments: int` | `AppendCylinder` (capped, Base origin, Z-axis) |
| `append_cone` | `base_radius: float, height: float, radial_segments: int` | `AppendCone` (TopRadius=0, capped, Base origin, Z-axis) |

Returns: true on Success.

Each primitive **adds to** the mesh — successive calls accumulate. Use `create_dynamic_mesh()` for a fresh empty mesh.

**Pitfalls**
- Negative `size`/`radius`/`height` clamps to 0 (degenerate result, no crash).
- For frustums (truncated cones) you need the underlying `AppendCone` with `TopRadius>0` — not exposed; fall back to direct `unreal.UGeometryScriptLibrary_MeshPrimitiveFunctions.append_cone(...)` until exposed.

---

## mesh_transform(handle, transform) -> bool

Apply a full `Transform` (translation + rotation + scale) to every vertex of the mesh in-place. Wraps `MeshTransformFunctions::TransformMesh` with `bFixOrientationForNegativeScale=true` (engine default).

**Pitfalls**
- For translation-only or rotation-only there are cheaper specialized APIs (`TranslateMesh` / `RotateMesh`) — not exposed yet; use this `mesh_transform` for now.

---

## mesh_uniform_remesh(handle, target_tri_count) -> bool

Re-tessellate the mesh into approximately `target_tri_count` uniform-density triangles. Wraps `RemeshingFunctions::ApplyUniformRemesh` with `TargetType=TriangleCount`.

The roadmap labels this M5-9 "triangulate" — the actual operation is uniform remeshing, which does triangulate non-tri faces as a side effect but is primarily about tri-density evening.

**Pitfalls**
- Engine docs flag this as "expensive" + "non-deterministic, expected to change in future versions" — don't depend on exact output across UE upgrades.
- Output count is approximate; ±50% is common at low targets.
- Boundary edges default to `Free` (movable) — see `FGeometryScriptRemeshOptions` if you need to lock open boundaries.

---

## End-to-end smoke pattern (split exec, per pit #15)

```python
# exec1
import unreal as u; G = u.UnrealBridgeGeometryLibrary
h = G.create_dynamic_mesh()
G.load_mesh_from_static_mesh(h, '/Engine/BasicShapes/Cube', 0)
info = G.get_mesh_info(h)
print('cube:', info.num_triangles, 'tri /', info.num_vertices, 'vert')
print('handles:', list(G.list_dynamic_mesh_handles()))
```
```python
# exec2
import unreal as u; G = u.UnrealBridgeGeometryLibrary
hs = list(G.list_dynamic_mesh_handles()); h = hs[-1]
out_path = G.save_mesh_to_new_static_mesh(h, '/Game/_BridgeTest/SM_CubeCopy', [])
print('saved:', out_path)
print('released:', G.release_dynamic_mesh(h))
```

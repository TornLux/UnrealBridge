# bridge-navigation-api

`unreal.UnrealBridgeNavigationLibrary` — navigation system introspection and export.

Depends on engine modules `NavigationSystem` + `Navmesh` (added to plugin `.Build.cs`; no project-level setup required).

---

## export_nav_mesh_to_obj(out_file_path) -> (success, summary)

Export the current editor world's `ARecastNavMesh` as a Wavefront OBJ (v/f only — no normals, no uvs).

**Parameters**
- `out_file_path` (str): absolute path to write. Pass empty string to use the default `<Project>/Saved/NavMeshExport.obj`.

**Returns** — tuple of:
- `success` (bool): `True` if a NavMesh was found and the file was written.
- `summary` (str): human-readable result, e.g. `wrote '<absolute-path>/NavMeshExport.obj' (navmesh=RecastNavMesh-Default tiles=128 verts=16384 tris=32768)` on success, or the failure reason (`no editor world`, `no ARecastNavMesh in current level`, `navmesh 'X' has no built tiles`, `failed to write 'Y'`).

**Coordinate system** — UE native: cm, Z-up, left-handed. Consumers (Blender, Houdini, etc.) should import with a UE preset and remap axes there, not here. No per-tile color / area classification is emitted; `BuiltMeshIndices` is the fully-merged navigable surface.

**Cost**
- Complexity: O(tile count + total triangles). `GetDebugGeometry` iterates each tile once.
- GameThread: yes. ~50 k triangles completes in <100 ms on a typical level.
- File I/O: synchronous UTF-8 write of the full OBJ string.

**Output footprint** — small (two values: bool + one-line string). The OBJ itself goes to disk, not to the bridge response.

**Example**
```python
import unreal
ok, msg = unreal.UnrealBridgeNavigationLibrary.export_nav_mesh_to_obj('')
print(msg)
# wrote '<project-root>/Saved/NavMeshExport.obj' (navmesh=RecastNavMesh-Default tiles=12 verts=3420 tris=4988)
```

**Pitfalls**
- If the level has no `ARecastNavMesh` actor (only `ANavMeshBoundsVolume` without an explicit navmesh actor placed), returns `(False, "no ARecastNavMesh in current level")`. Add a `RecastNavMesh` via Place Actors → Volumes or via `World Settings → NavMesh` if missing.
- If navmesh exists but hasn't been built (runtime generation disabled, no nav bounds, or blank), returns `(False, "navmesh '…' has no built tiles")`. Trigger a build via `Build → Build Paths` or `unreal.EditorLevelLibrary.build_nav_mesh()` (when exposed).

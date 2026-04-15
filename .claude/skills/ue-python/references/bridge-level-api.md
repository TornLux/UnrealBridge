# UnrealBridge Level Library API

Module: `unreal.UnrealBridgeLevelLibrary`

Operates on the editor world. All write operations are wrapped in `FScopedTransaction` (Ctrl+Z undoable).

## Read — Level / Summary

### get_level_summary() -> FBridgeLevelSummary

Return a compact summary of the current editor world.

```python
s = unreal.UnrealBridgeLevelLibrary.get_level_summary()
print(f'{s.level_name} [{s.world_type}] actors={s.num_actors} streaming={s.num_streaming_levels} WP={s.b_world_partition}')
```

### FBridgeLevelSummary fields

| Field | Type | Description |
|-------|------|-------------|
| `level_name` | str | Persistent level name |
| `level_path` | str | Package path of the persistent level |
| `num_actors` | int | Total actors in the world |
| `num_streaming_levels` | int | Number of streaming levels |
| `world_type` | str | "Editor", "PIE", "Game" |
| `b_world_partition` | bool | True if this is a World Partition map |

### get_current_level_path() -> str

Return the package path of the persistent level.

### get_streaming_levels() -> list[FBridgeStreamingLevel]

List streaming sublevels with load/visible flags.

### FBridgeStreamingLevel fields

| Field | Type | Description |
|-------|------|-------------|
| `package_name` | str | Sublevel package name |
| `b_loaded` | bool | Currently loaded |
| `b_visible` | bool | Currently visible |

---

## Read — Actor Queries

### get_actor_count(class_filter) -> int

Count actors passing an optional class filter (short name or full path). Empty string = all.

### get_actor_names(class_filter, tag_filter, name_filter) -> list[str]

Return **labels** (user-visible names). All filters are optional (pass "" to skip). `name_filter` is a case-insensitive substring match on the label.

> ⚠️ **Token cost: MEDIUM–HIGH on populated levels.** No built-in result cap. On World Partition / open-world maps (5k+ actors) an unfiltered call returns thousands of strings. **Always pass at least one filter**, or use `get_actor_count` first to size the sweep.

```python
names = unreal.UnrealBridgeLevelLibrary.get_actor_names('StaticMeshActor', '', 'Wall')
```

### list_actors(class_filter, tag_filter, name_filter, selected_only, max_results) -> list[FBridgeActorBrief]

Detailed list. `max_results=0` = unlimited.

> ⚠️ **Token cost: HIGH.** Each brief carries 6 fields (name, label, class, location, tags, hidden). Multiply by actor count — an unfiltered `max_results=0` on a WP map can return several hundred KB. **Never pass `max_results=0` without a narrowing filter.** Prefer `get_actor_names` (labels only) for discovery, then `get_actor_info` on the specific actor.

```python
briefs = unreal.UnrealBridgeLevelLibrary.list_actors('', '', '', False, 50)
for a in briefs:
    print(f'[{a.class_name}] {a.label} @ {a.location}')
```

### FBridgeActorBrief fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Internal FName |
| `label` | str | User-visible label |
| `class_name` | str | Actor class short name |
| `location` | Vector | World location |
| `tags` | list[str] | Actor tags |
| `b_hidden` | bool | Hidden in editor |

### find_actors_by_class(class_path, max_results) -> list[str]

Return labels of actors matching a class path. Accepts short name or full `/Script/...` path. `max_results=0` = unlimited.

### find_actors_by_tag(tag) -> list[str]

Return labels of actors having the given tag.

> ⚠️ **Token cost: MEDIUM.** No result cap. If the tag is broad (e.g. "Enemy" on hundreds of spawns), the return can grow large. Check `get_actor_count` or use `find_actors_by_class` with `max_results` when scale is unknown.

### find_actors_in_radius(location, radius, class_filter) -> list[FBridgeActorRadiusHit]

Actors within `radius` (cm) of `location`, distance-sorted.

```python
hits = unreal.UnrealBridgeLevelLibrary.find_actors_in_radius(unreal.Vector(0,0,0), 500.0, '')
for h in hits:
    print(f'{h.name} [{h.class_name}] d={h.distance:.1f}')
```

### FBridgeActorRadiusHit fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Actor label |
| `class_name` | str | Class short name |
| `distance` | float | Distance to probe point (cm) |

### get_selected_actors() -> list[str]

Return labels of currently selected actors.

---

## Read — Actor Detail

### get_actor_info(actor_name) -> FBridgeActorInfo

Look up by FName or label. On duplicate labels, first match wins.

```python
info = unreal.UnrealBridgeLevelLibrary.get_actor_info('BP_Hero_1')
print(f'{info.label} class={info.class_name} parent={info.attached_to} children={list(info.children)}')
for c in info.components:
    print(f'  [{c.class_name}] {c.name} parent={c.attach_parent}')
```

### FBridgeActorInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Internal FName |
| `label` | str | User-visible label |
| `class_name` | str | Actor class short name |
| `class_path` | str | Full class path |
| `transform` | FBridgeTransform | World transform |
| `tags` | list[str] | Actor tags |
| `attached_to` | str | Parent actor name (empty if detached) |
| `children` | list[str] | Attached child actor names |
| `components` | list[FBridgeLevelComponentInfo] | Components |
| `b_hidden` | bool | Hidden in editor |
| `b_hidden_in_game` | bool | Hidden at runtime |

### FBridgeTransform fields

| Field | Type | Description |
|-------|------|-------------|
| `location` | Vector | Location |
| `rotation` | Rotator | Rotation |
| `scale` | Vector | Scale |

### FBridgeLevelComponentInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Component name |
| `class_name` | str | Component class |
| `attach_parent` | str | Parent component name (empty if root/non-scene) |
| `relative_transform` | FBridgeTransform | Relative transform |

### get_actor_transform(actor_name) -> FBridgeTransform

### get_actor_components(actor_name) -> list[FBridgeLevelComponentInfo]

### get_attachment_tree(actor_name) -> list[str]

Recursive attachment hierarchy as indented lines, one per descendant.

### get_actor_property(actor_name, property_path) -> str

Return the exported-text value of a property. Supports dotted nesting into structs and subobjects (e.g. `RootComponent.RelativeLocation`, `StaticMeshComponent.Mobility`).

---

## Write — Spawn / Destroy / Duplicate

### spawn_actor(class_path, location, rotation) -> str

Spawn an actor. Accepts `/Script/Engine.StaticMeshActor` or Blueprint path `/Game/.../BP_Foo` (`_C` suffix optional). Returns the spawned actor's label, or empty string on failure.

```python
label = unreal.UnrealBridgeLevelLibrary.spawn_actor(
    '/Script/Engine.StaticMeshActor',
    unreal.Vector(0, 0, 100),
    unreal.Rotator(0, 0, 0),
)
```

### destroy_actor(actor_name) -> bool

### destroy_actors(actor_names) -> int

Destroy many actors in a single undo transaction. Returns count destroyed.

### duplicate_actors(actor_names) -> list[str]

Duplicate actors in a single undo transaction. Returns labels of the new copies.

---

## Write — Transform / Hierarchy

### set_actor_transform(actor_name, location, rotation, scale) -> bool

### move_actor(actor_name, delta_location, delta_rotation) -> bool

Apply a delta to the current transform.

### attach_actor(child_name, parent_name, socket_name) -> bool

Attach `child_name` to `parent_name` (optionally at a socket).

### detach_actor(actor_name) -> bool

### set_actor_property(actor_name, property_path, exported_value) -> bool

Set a property from an exported-text value. Dotted path supported. ⚠️ For transient struct fields (e.g. component `RelativeLocation`), prefer `set_actor_transform` / `move_actor` — direct struct writes don't trigger component update notifications.

---

## Write — Selection / Visibility / Labels

### select_actors(actor_names, add_to_selection) -> bool

Select in the editor viewport. If `add_to_selection` is False, the selection is cleared first.

### deselect_all_actors() -> bool

### set_actor_label(actor_name, new_label) -> bool

### set_actor_hidden_in_game(actor_name, hidden) -> bool

---

## Read — Spatial Queries

### get_actor_bounds(actor_name) -> FBridgeActorBounds

World-space bounds of `actor_name` (all colliding + non-colliding primitives, including child actors). Zero-bounds if the actor has no renderable/collision geometry or is missing.

```python
b = unreal.UnrealBridgeLevelLibrary.get_actor_bounds('BP_Cube_1')
print(b.origin, b.box_extent, b.sphere_radius)
```

### FBridgeActorBounds fields

| Field | Type | Description |
|-------|------|-------------|
| `origin` | Vector | World-space center of the AABB |
| `box_extent` | Vector | Half-extents on each axis |
| `sphere_radius` | float | Bounding sphere radius |

### get_actors_in_box(min, max, class_filter) -> list[str]

Labels of actors whose world location lies inside the axis-aligned box `[min, max]`. Pass empty string for `class_filter` to match any class.

```python
names = unreal.UnrealBridgeLevelLibrary.get_actors_in_box(
    unreal.Vector(-500,-500,0), unreal.Vector(500,500,500), '')
```

### find_nearest_actor(location, class_filter) -> str

Label of the actor nearest to `location`, or empty string if none match `class_filter`.

### get_actor_distance(actor_a, actor_b) -> float

Distance between two actors' world locations in cm. Returns `-1.0` if either actor is missing.

### is_actor_selected(actor_name) -> bool

`True` if `actor_name` is currently selected in the editor viewport.


### set_actor_hidden_in_editor(actor_name, hidden) -> bool

Toggle editor viewport visibility (SetIsTemporarilyHiddenInEditor). Transaction-wrapped. Cost: O(1) plus lookup.

### add_actor_tag(actor_name, tag) -> bool

Add an FName tag to Actor.Tags. Returns False if already present or actor missing. Transaction-wrapped.

```python
unreal.UnrealBridgeLevelLibrary.add_actor_tag('BP_Cube_1', 'interactable')
```

### remove_actor_tag(actor_name, tag) -> bool

Remove an FName tag from Actor.Tags. Returns True only if a tag was removed. Transaction-wrapped.

### get_actor_class_histogram() -> list[str]

Per-class counts across all actors in the editor world, sorted descending. Each line is "Count	ClassName". Cost: O(N) over all actors; small output (one line per distinct class).

```python
for line in unreal.UnrealBridgeLevelLibrary.get_actor_class_histogram()[:10]:
    print(line)
```

### get_actor_materials(actor_name) -> list[str]

Deduplicated material asset paths from the actor's `UMeshComponent`-derived components (static + skeletal + instanced). Empty list if no mesh components. Cost: O(components * material slots); small output. Note: `Landscape` actors have no `UMeshComponent` and return `[]` — use material queries against the landscape component separately if needed.

```python
unreal.UnrealBridgeLevelLibrary.get_actor_materials('BP_Cube_1')
# ['/Game/Materials/M_Base.M_Base', ...]
```

---

## Folder Organization

World Outliner folder paths use `/` as a separator (e.g. `"Enemies/Boss"`). The root is represented as the empty string `""`.

### get_actor_folder(actor_name) -> str

Return the actor's World Outliner folder path, or `""` if the actor is at root (or missing). Cost: O(1) plus lookup.

```python
unreal.UnrealBridgeLevelLibrary.get_actor_folder('BP_Hero_1')  # "Heroes/Main"
```

### set_actor_folder(actor_name, folder_path) -> bool

Move an actor to `folder_path`. Pass `""` to move it to the outliner root. Transaction-wrapped (Ctrl+Z undoable). Returns `False` if the actor is missing.

```python
unreal.UnrealBridgeLevelLibrary.set_actor_folder('BP_Cube_1', 'Props/Crates')
```

### get_actor_folders() -> list[str]

Sorted distinct folder paths used by any actor in the current level. Actors at root (unfoldered) do not contribute a `""` entry. Cost: O(N) over all actors; small output (one line per distinct folder).

> Token cost: LOW — one short string per folder; typical levels have fewer than ~30 folders.

### get_actors_in_folder(folder_path, recursive) -> list[str]

Labels of actors whose folder equals `folder_path`.

- `recursive=False` — exact match only.
- `recursive=True` — also include actors in sub-folders (`"Foo/Bar"` when querying `"Foo"`).
- Pass `folder_path=""` for root-level actors; with `recursive=True` and an empty path, every foldered actor is returned (useful for "everything that has been organized").

> Token cost: MEDIUM on large folders. No result cap. Combine with class/name filters via `get_actor_names` then filter in Python if you need finer slicing.

```python
# Re-parent all actors under "Old/Legacy" to "Archive"
for label in unreal.UnrealBridgeLevelLibrary.get_actors_in_folder('Old/Legacy', True):
    unreal.UnrealBridgeLevelLibrary.set_actor_folder(label, 'Archive')
```

---

## Spatial — Line Trace

### line_trace_first_actor(start, end) -> str

Cast a single line-trace through the editor world against the **Visibility** collision channel (complex collision). Returns the label of the first actor hit, or `""` if nothing was hit or the world is unavailable.

> Cost: O(world-trace). Small output (single string). Useful for picking an actor beneath the viewport camera or probing what an actor would "see" along a vector.

```python
hit = unreal.UnrealBridgeLevelLibrary.line_trace_first_actor(
    unreal.Vector(0, 0, 100000),
    unreal.Vector(0, 0, -100000),
)
print(hit)  # e.g. "Landscape"
```

### multi_line_trace_actors(start, end) -> list[str]

Cast a multi-hit line-trace (visibility channel, complex collision) and return deduplicated actor labels ordered near → far.

> Cost: O(world-trace + hits). Small output. Prefer over looping `line_trace_first_actor` with temporary occluders — this returns the whole penetration set in one call.

```python
pierced = unreal.UnrealBridgeLevelLibrary.multi_line_trace_actors(
    unreal.Vector(0, 0, 10000),
    unreal.Vector(0, 0, -10000),
)
# e.g. ['Ceiling', 'Platform_2', 'Landscape']
```

### sphere_trace_first_actor(start, end, radius) -> str

Sphere sweep (fat ray) against the editor world's visibility channel.
Catches actors a thin line-trace would miss when they graze the ray —
useful for cover / line-of-interest detection. Returns the first hit
actor's label, or empty string.

```python
hit = unreal.UnrealBridgeLevelLibrary.sphere_trace_first_actor(
    unreal.Vector(0, 0, 0), unreal.Vector(0, 0, -10000), 50.0)
```

### multi_sphere_trace_actors(start, end, radius) -> list[str]

Multi-hit sphere sweep. Deduplicated labels ordered near → far.

### box_trace_first_actor(start, end, box_half_extent) -> str

Axis-aligned box sweep (`FQuat::Identity` orientation). `box_half_extent`
is a `Vector` — the box's half-size on each axis. Returns the first hit
actor's label, or empty string. For oriented-box sweeps drop to the UE
Python API directly — this wrapper intentionally keeps the surface flat.

```python
hit = unreal.UnrealBridgeLevelLibrary.box_trace_first_actor(
    unreal.Vector(0, 0, 0), unreal.Vector(0, 0, -10000),
    unreal.Vector(50, 50, 50))
```

### overlap_sphere_actors(center, radius, class_filter) -> list[str]

Physics-overlap: actors whose collision primitives intersect a sphere at
`center` with `radius` cm. **Distinct from `find_actors_in_radius`**,
which tests actor centroids only — overlap catches large actors that
straddle the sphere even if their pivot is outside.

Results are deduplicated; order follows the query's internal traversal
(not distance-sorted). Pass `class_filter=""` for no filter.

```python
nearby = unreal.UnrealBridgeLevelLibrary.overlap_sphere_actors(
    unreal.Vector(0, 0, 0), 500.0, 'StaticMeshActor')
```

**Pitfalls (all sweep/overlap queries)**

- Use the *visibility* collision channel — actors with `NoCollision` or
  non-visibility collision profiles won't be reported.
- All four wrappers run `bTraceComplex=false` (primitive shapes, not
  triangle-level). For per-triangle sweeps use the native UE API.
- `radius` / `box_half_extent` are clamped to ≥ 0 internally; negative
  values become 0 (degenerates to a line trace).

---

## Components / Sockets

### get_actor_sockets(actor_name) -> list[str]

Enumerate sockets on every `USceneComponent` of the actor. Each line is `"ComponentName:SocketName"`.

> Cost: O(components × sockets). Small output on typical actors. Use to discover valid `socket_name` arguments for `attach_actor` or `get_socket_world_transform`.

```python
for entry in unreal.UnrealBridgeLevelLibrary.get_actor_sockets('BP_Weapon_1'):
    print(entry)  # e.g. "Mesh:Muzzle"
```

### get_socket_world_transform(actor_name, component_name, socket_name) -> FBridgeTransform

World transform of `socket_name` on `component_name`. Returns an identity transform if the actor, component, or socket is missing.

```python
t = unreal.UnrealBridgeLevelLibrary.get_socket_world_transform(
    'BP_Character_1', 'Mesh', 'hand_r')
print(t.location, t.rotation)
```

### get_component_world_transform(actor_name, component_name) -> FBridgeTransform

World transform of a scene component by name. Returns identity if the component is missing or non-scene. Cost: O(1) plus component lookup.

### set_component_visibility(actor_name, component_name, visible, propagate_to_children) -> bool

Toggle visibility of a scene component. If `propagate_to_children=True`, the change cascades down the attachment tree. Transaction-wrapped. Returns `False` if the component is missing or not a scene component.

```python
unreal.UnrealBridgeLevelLibrary.set_component_visibility(
    'BP_Hero_1', 'Cape', False, True)
```

### set_component_mobility(actor_name, component_name, mobility) -> bool

Set a scene component's mobility. `mobility` must be one of `"Static"`, `"Stationary"`, or `"Movable"` (case-insensitive). Transaction-wrapped. Returns `False` if the component is missing, non-scene, or the mobility string is invalid.

> ⚠️ Movable → Static demotions may invalidate baked lighting. Callers should typically re-bake / rebuild lighting after changing lots of actors.

```python
unreal.UnrealBridgeLevelLibrary.set_component_mobility(
    'BP_Prop_1', 'StaticMeshComponent', 'Movable')
```

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

```python
names = unreal.UnrealBridgeLevelLibrary.get_actor_names('StaticMeshActor', '', 'Wall')
```

### list_actors(class_filter, tag_filter, name_filter, selected_only, max_results) -> list[FBridgeActorBrief]

Detailed list. `max_results=0` = unlimited. ⚠️ Large on populated levels — prefer `get_actor_names` or tighten filters.

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

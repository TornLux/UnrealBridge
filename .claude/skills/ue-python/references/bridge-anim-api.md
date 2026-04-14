# UnrealBridge Animation Library API

Module: `unreal.UnrealBridgeAnimLibrary`

## State Machine Info

### get_anim_graph_info(anim_blueprint_path) -> list[FBridgeStateMachineInfo]

Get all state machines in an Animation Blueprint with states and transitions.

```python
machines = unreal.UnrealBridgeAnimLibrary.get_anim_graph_info('/Game/Anim/ABP_Character')
for sm in machines:
    print(f'SM: {sm.name}')
    for s in sm.states:
        tags = '[default]' if s.is_default else ('[conduit]' if s.is_conduit else '')
        print(f'  State: {s.name} {tags}')
    for t in sm.transitions:
        print(f'  {t.from_state} -> {t.to_state} (blend={t.crossfade_duration}s)')
```

---

## AnimGraph Nodes

### get_anim_graph_nodes(anim_blueprint_path) -> list[FBridgeAnimGraphNodeInfo]

List all nodes in the AnimGraph with connections. Shows the animation pipeline topology.

> ⚠️ **Token cost: MEDIUM–HIGH on large ABPs.** Returns every node plus all pose-link connections. Production character ABPs (ALS/Lyra) routinely expose 80–200 nodes. Prefer `get_anim_graph_info` for state-machine shape and `get_anim_slots`/`get_anim_linked_layers` for targeted questions; reach for this only when you need the full pose pipeline, and use `get_anim_node_details(path, node_title)` to drill into one node.

```python
nodes = unreal.UnrealBridgeAnimLibrary.get_anim_graph_nodes('/Game/Anim/ABP_Character')
for n in nodes:
    conns = ', '.join(f'{c.source_pin}->[{c.target_node_index}].{c.target_pin}' for c in n.connections)
    print(f'[{n.node_index}] {n.node_title} ({n.node_type}) -> {conns}')
```

### FBridgeAnimGraphNodeInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `node_index` | int | Index in result array |
| `node_title` | str | Display title |
| `node_type` | str | Class name, e.g. "AnimGraphNode_SequencePlayer" |
| `detail` | str | Extra context (ListView title) |
| `connections` | list[FBridgeAnimNodeConnection] | Output connections |

---

## Linked Anim Layers

### get_anim_linked_layers(anim_blueprint_path) -> list[FBridgeAnimLayerInfo]

Get linked anim layer bindings — the UE5 modular animation architecture.

```python
layers = unreal.UnrealBridgeAnimLibrary.get_anim_linked_layers('/Game/Anim/ABP_Character')
for l in layers:
    print(f'{l.interface_name} - {l.layer_name}')
```

### FBridgeAnimLayerInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `interface_name` | str | Interface class name |
| `layer_name` | str | Layer function name |
| `implementing_class` | str | Full node title |

---

## Anim Slots

### get_anim_slots(anim_blueprint_path) -> list[FBridgeAnimSlotInfo]

Get all Slot nodes and their slot names (montage playback paths).

```python
slots = unreal.UnrealBridgeAnimLibrary.get_anim_slots('/Game/Anim/ABP_Character')
for s in slots:
    print(f'Slot: {s.slot_name} in {s.graph_name}')
```

### FBridgeAnimSlotInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `slot_name` | str | e.g. "DefaultSlot", "TwoArms" |
| `graph_name` | str | Which graph contains this slot |
| `node_title` | str | Node display title |

---

## Anim Node Details

### get_anim_node_details(anim_blueprint_path, node_title) -> list[str]

Get non-default properties of a specific anim graph node by title substring. Returns `["PropName (Type) = Value", ...]`.

```python
details = unreal.UnrealBridgeAnimLibrary.get_anim_node_details('/Game/Anim/ABP_Char', 'MotionMatching')
for d in details:
    print(f'  {d}')
```

---

## Anim Curves

### get_anim_curves(anim_blueprint_path) -> list[str]

Get all animation curve names from the skeleton's curve metadata.

```python
curves = unreal.UnrealBridgeAnimLibrary.get_anim_curves('/Game/Anim/ABP_Character')
for c in curves:
    print(c)
```

---

## Animation Sequence Info

### get_anim_sequence_info(sequence_path) -> FBridgeAnimSequenceInfo

Get animation sequence details: length, frames, notifies, curves, root motion.

```python
info = unreal.UnrealBridgeAnimLibrary.get_anim_sequence_info('/Game/Anim/Run_Fwd')
print(f'{info.name}: {info.play_length}s, {info.num_frames} frames, rate={info.rate_scale}')
print(f'Root motion: {info.has_root_motion}, Skeleton: {info.skeleton_name}')
for n in info.notifies:
    print(f'  Notify: {n.notify_name} at {n.trigger_time}s ({n.notify_type})')
for c in info.curves:
    print(f'  Curve: {c.curve_name} ({c.num_keys} keys)')
```

### FBridgeAnimSequenceInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Asset name |
| `skeleton_name` | str | Skeleton name |
| `play_length` | float | Duration in seconds |
| `num_frames` | int | Number of sampled keys |
| `frame_rate` | float | Frames per second |
| `rate_scale` | float | Playback rate multiplier |
| `has_root_motion` | bool | Root motion enabled |
| `notifies` | list[FBridgeAnimNotifyInfo] | Animation notifies |
| `curves` | list[FBridgeAnimCurveInfo] | Animation curves |

---

## Montage Info

### get_montage_info(montage_path) -> FBridgeMontageInfo

Get montage details: sections, slot, blend settings, notifies.

```python
info = unreal.UnrealBridgeAnimLibrary.get_montage_info('/Game/Anim/AM_Attack')
print(f'{info.name}: slot={info.slot_name}, {info.play_length}s')
print(f'Blend: in={info.blend_in_time}s, out={info.blend_out_time}s, auto={info.enable_auto_blend_out}')
for s in info.sections:
    print(f'  Section: {s.section_name} at {s.start_time}s -> {s.next_section_name}')
```

### FBridgeMontageInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Asset name |
| `skeleton_name` | str | Skeleton name |
| `play_length` | float | Duration in seconds |
| `slot_name` | str | Slot name for playback |
| `blend_in_time` | float | Blend in duration |
| `blend_out_time` | float | Blend out duration |
| `enable_auto_blend_out` | bool | Auto blend out |
| `sections` | list[FBridgeMontageSectionInfo] | Composite sections |
| `notifies` | list[FBridgeAnimNotifyInfo] | Animation notifies |

---

## BlendSpace Info

### get_blend_space_info(blend_space_path) -> FBridgeBlendSpaceInfo

Get blend space axes and sample points.

```python
info = unreal.UnrealBridgeAnimLibrary.get_blend_space_info('/Game/Anim/BS_Locomotion')
for ax in info.axes:
    print(f'Axis: {ax.name} [{ax.min}..{ax.max}] grid={ax.grid_divisions}')
for s in info.samples:
    print(f'  {s.animation_name} at ({s.sample_value.x}, {s.sample_value.y})')
```

### FBridgeBlendSpaceInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Asset name |
| `skeleton_name` | str | Skeleton name |
| `num_axes` | int | 1 or 2 |
| `axes` | list[FBridgeBlendSpaceAxis] | Axis definitions |
| `samples` | list[FBridgeBlendSpaceSample] | Sample points |

---

## Skeleton Bone Tree

### get_skeleton_bone_tree(skeleton_path) -> list[FBridgeBoneInfo]

Get the full bone hierarchy of a skeleton.

> ⚠️ **Token cost: MEDIUM–HIGH on high-res rigs.** No result cap and no filter. MetaHuman skeletons are ~700 bones × 4 fields; custom rigs with facial joints can exceed 1000. If you only need a specific bone or a subtree, filter the result client-side after one call — but don't call this in a loop.

```python
bones = unreal.UnrealBridgeAnimLibrary.get_skeleton_bone_tree('/Game/Skel/SK_Character_Skeleton')
for b in bones:
    indent = '  ' * (b.bone_index // 5)  # rough depth
    print(f'{b.bone_name} (idx={b.bone_index}, parent={b.parent_name})')
```

### FBridgeBoneInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `bone_name` | str | Bone name |
| `bone_index` | int | Index in skeleton |
| `parent_index` | int | Parent bone index (-1 for root) |
| `parent_name` | str | Parent bone name (empty for root) |

---

## Skeleton Sockets

### get_skeleton_sockets(skeleton_path) -> list[FBridgeSocketInfo]

Get all sockets defined on a skeleton (attach points for weapons, FX, etc.).

```python
sockets = unreal.UnrealBridgeAnimLibrary.get_skeleton_sockets('/Game/Skel/SK_Character_Skeleton')
for s in sockets:
    print(f'{s.socket_name} on {s.parent_bone_name} @ {s.relative_location}')
```

### FBridgeSocketInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `socket_name` | str | Socket name |
| `parent_bone_name` | str | Bone the socket is attached to |
| `relative_location` | Vector | Offset from parent bone |
| `relative_rotation` | Rotator | Rotation relative to parent bone |
| `relative_scale` | Vector | Scale relative to parent bone |

---

## Skeleton Virtual Bones

### get_skeleton_virtual_bones(skeleton_path) -> list[FBridgeVirtualBoneInfo]

Get virtual bones on a skeleton — synthetic bones that are defined as `Source -> Target` pairs (typically used for IK end-effectors like `VB ik_foot_l`). Returns `[]` for skeletons without virtual bones.

> Token cost: LOW. Production rigs have a handful of virtual bones (usually < 10). Returns three string fields per entry.

```python
vbs = unreal.UnrealBridgeAnimLibrary.get_skeleton_virtual_bones('/Game/Skel/SK_Hero_Skeleton')
for v in vbs:
    print(f'{v.virtual_bone_name}: {v.source_bone_name} -> {v.target_bone_name}')
```

### FBridgeVirtualBoneInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `virtual_bone_name` | str | Virtual bone name, already includes `"VB "` prefix |
| `source_bone_name` | str | Source (parent) bone |
| `target_bone_name` | str | Target bone whose transform drives the VB |

---

## Montage Slot Segments

### get_montage_slot_segments(montage_path) -> list[FBridgeMontageSlotSegment]

List the anim segments laid out on each of a montage's slot tracks. One entry per `FAnimSegment` in every `FSlotAnimationTrack`. Use this to discover which `AnimSequence` (or `AnimComposite`) a montage actually plays, its sub-clip window, play rate, and looping. Results are returned in layout order (per slot).

> Token cost: LOW–MEDIUM. Typical single-slot melee/locomotion montages return 1–3 segments. Multi-slot montages (e.g. additive upper-body over locomotion) may return 5–10. Segment paths are full `/Game/...` soft object paths — trim them client-side if you only need asset names.

```python
segs = unreal.UnrealBridgeAnimLibrary.get_montage_slot_segments('/Game/Anim/AM_Combo')
for s in segs:
    print(f'{s.slot_name}  +{s.start_pos:.2f}s  {s.anim_reference_path}  '
          f'clip=[{s.anim_start_time:.2f}..{s.anim_end_time:.2f}]  x{s.anim_play_rate}  loop={s.looping_count}')
```

### FBridgeMontageSlotSegment fields

| Field | Type | Description |
|-------|------|-------------|
| `slot_name` | str | Owning slot name |
| `anim_reference_path` | str | Soft path to the referenced anim (empty if unset) |
| `start_pos` | float | Montage-local start in seconds |
| `anim_start_time` | float | Sub-clip start inside the referenced anim |
| `anim_end_time` | float | Sub-clip end inside the referenced anim |
| `anim_play_rate` | float | Segment play rate multiplier |
| `looping_count` | int | Times the sub-clip loops inside the segment |

---

## Write Ops

All writes mark the package dirty in memory. The caller is responsible for `unreal.EditorAssetLibrary.save_asset(path)` to persist. Crashing / killing the editor before save discards the changes.

### add_anim_notify(sequence_path, notify_name, trigger_time, duration) -> bool

Append a name-only notify to an `AnimSequence` / `AnimSequenceBase`. `duration > 0` stores a state-notify duration (without a notify-state class — useful as a marker you later hook up manually). Trigger time outside `[0, play_length]` is rejected.

```python
ok = unreal.UnrealBridgeAnimLibrary.add_anim_notify(
    '/Game/Anim/Attack', 'HitFrame', 0.42, 0.0)            # instant
ok = unreal.UnrealBridgeAnimLibrary.add_anim_notify(
    '/Game/Anim/Attack', 'HitWindow', 0.30, 0.15)          # stateful window
```

### remove_anim_notifies_by_name(sequence_path, notify_name) -> int

Remove every notify whose name matches (exact `FName` compare). Returns count removed.

```python
n = unreal.UnrealBridgeAnimLibrary.remove_anim_notifies_by_name('/Game/Anim/Attack', 'HitFrame')
```

### set_anim_sequence_rate_scale(sequence_path, rate_scale) -> bool

Set `RateScale` on an `UAnimSequence`. Accepts negative values (reversed playback).

### add_montage_section(montage_path, section_name, start_time) -> bool

Add a new composite section. Rejects duplicate name or out-of-range `start_time`. Sections are re-sorted by start time.

```python
unreal.UnrealBridgeAnimLibrary.add_montage_section('/Game/Anim/AM_Combo', 'Swing2', 0.45)
```

### set_montage_section_next(montage_path, section_name, next_section_name) -> bool

Wire `section_name -> next_section_name` for auto-advance. Pass `""` for `next_section_name` to clear the link. Returns false when either section is missing.

### remove_montage_section(montage_path, section_name) -> bool

Remove a composite section by name. Also clears any other section's `next_section_name` that pointed at the removed one (so you never end up with a dangling link). Returns `False` when the section does not exist.

```python
unreal.UnrealBridgeAnimLibrary.remove_montage_section('/Game/Anim/AM_Combo', 'Swing2')
```

### set_montage_blend_times(montage_path, blend_in_time, blend_out_time, blend_out_trigger_time, enable_auto_blend_out) -> bool

Write montage blend settings in one call.

- `blend_in_time` / `blend_out_time`: pass `>= 0` to set the corresponding `FAlphaBlend`, or `< 0` to leave unchanged.
- `blend_out_trigger_time`: pass `-1` for "blend out at end"; any other non-negative value sets an explicit trigger time (`PlayLength - BlendOutTriggerTime`). Always written.
- `enable_auto_blend_out`: always written.

Returns `False` only when the montage fails to load.

```python
# Shorten blend-in only, keep the rest.
unreal.UnrealBridgeAnimLibrary.set_montage_blend_times('/Game/Anim/AM_Attack', 0.1, -1.0, -1.0, True)
```

### add_skeleton_socket(skeleton_path, socket_name, parent_bone_name, relative_location, relative_rotation, relative_scale) -> bool

Add a new `USkeletalMeshSocket` to a skeleton. Fails when:
- The skeleton fails to load.
- `parent_bone_name` is not present in the skeleton's reference skeleton.
- A socket with the same name already exists (use `get_skeleton_sockets` first to check).

The socket is appended to `USkeleton::Sockets` and the package is marked dirty; caller must save the skeleton to persist.

```python
ok = unreal.UnrealBridgeAnimLibrary.add_skeleton_socket(
    '/Game/Skel/SK_Hero_Skeleton',
    'weapon_r_socket',
    'hand_r',
    unreal.Vector(5.0, 0.0, 0.0),
    unreal.Rotator(0.0, 90.0, 0.0),
    unreal.Vector(1.0, 1.0, 1.0))
```

### list_assets_for_skeleton(skeleton_path, asset_type, max_results) -> list[str]

Find anim assets bound to a skeleton via `FAssetData` `Skeleton` tag. `asset_type` accepts `"Sequence"`, `"Montage"`, `"BlendSpace"`, or `""` for all three.

> ⚠️ **Token cost: MEDIUM–HIGH on game-scale libraries.** Characters with production animation sets (Lyra, ALS, TLOU-style) often have 1000–4000+ sequences. Always pass `max_results > 0` unless you really need the full set, and narrow `asset_type` when you only want one kind.

```python
montages = unreal.UnrealBridgeAnimLibrary.list_assets_for_skeleton(
    '/Game/Characters/Hero/SK_Hero_Skeleton.SK_Hero_Skeleton', 'Montage', 100)
```

### remove_skeleton_socket(skeleton_path, socket_name) -> bool

Remove a named `USkeletalMeshSocket` from a skeleton. Returns `False` when the skeleton fails to load or no socket with that name exists. Marks the package dirty on success — save the skeleton to persist.

> Token cost: LOW. One round-trip, bool return.

```python
ok = unreal.UnrealBridgeAnimLibrary.remove_skeleton_socket(
    '/Game/Skel/SK_Hero_Skeleton', 'weapon_r_socket')
if ok:
    unreal.EditorAssetLibrary.save_asset('/Game/Skel/SK_Hero_Skeleton')
```

---

## Anim Sync Markers

### get_anim_sync_markers(sequence_path) -> list[FBridgeAnimSyncMarker]

Get the authored sync markers (`UAnimSequence::AuthoredSyncMarkers`) on a sequence, sorted by time. Sync markers drive synchronized playback across different-length loops (e.g. matching foot-plant phases between walk and run cycles in a BlendSpace). Returns `[]` for non-`UAnimSequence` assets (montages, composites) or sequences with no markers.

> Token cost: LOW. Typical locomotion cycles carry 2–8 markers. Three scalar fields per entry.

```python
marks = unreal.UnrealBridgeAnimLibrary.get_anim_sync_markers('/Game/Anim/Run_Fwd')
for m in marks:
    print(f'{m.marker_name} @ {m.time:.3f}s (track {m.track_index})')
```

### FBridgeAnimSyncMarker fields

| Field | Type | Description |
|-------|------|-------------|
| `marker_name` | str | Marker name (e.g. `"LeftFoot"`, `"RightFoot"`) |
| `time` | float | Trigger time in seconds |
| `track_index` | int | Editor notify-track index; `-1` in non-editor builds |

---

## Skeleton Blend Profiles

Blend profiles scale per-bone blending for smoother state-machine transitions (e.g. upper body blends slower than legs). A skeleton can own multiple profiles; each has one of three modes — `TimeFactor`, `WeightFactor`, `BlendMask`.

### get_skeleton_blend_profiles(skeleton_path) -> list[FBridgeBlendProfileInfo]

List blend profiles by name + mode + entry count. Use this to discover profile names before calling `get_blend_profile_entries`.

> Token cost: LOW. Most rigs carry 0–5 blend profiles.

```python
profiles = unreal.UnrealBridgeAnimLibrary.get_skeleton_blend_profiles(
    '/Game/Skel/SK_Hero_Skeleton')
for p in profiles:
    print(f'{p.name}  mode={p.mode}  entries={p.num_entries}')
```

### FBridgeBlendProfileInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Profile object name |
| `mode` | str | `"TimeFactor"`, `"WeightFactor"`, or `"BlendMask"` |
| `num_entries` | int | Number of bone entries in the profile |

### get_blend_profile_entries(skeleton_path, profile_name) -> list[FBridgeBlendProfileEntry]

Get the per-bone blend-scale entries for a specific profile. Only bones whose blend scale is authored in the profile are returned (bones without entries use the profile's default scale at runtime). Returns `[]` when the profile does not exist.

> Token cost: LOW–MEDIUM. Typical upper-body/lower-body profiles have 10–40 entries. Large face-mask profiles can reach ~100.

```python
entries = unreal.UnrealBridgeAnimLibrary.get_blend_profile_entries(
    '/Game/Skel/SK_Hero_Skeleton', 'UpperBody')
for e in entries:
    print(f'{e.bone_name}  scale={e.blend_scale}')
```

### FBridgeBlendProfileEntry fields

| Field | Type | Description |
|-------|------|-------------|
| `bone_name` | str | Bone name |
| `blend_scale` | float | Authored blend scale for that bone |

---

### set_montage_section_start_time(montage_path, section_name, start_time) -> bool

Move an existing composite section to a new `start_time` (in seconds, `[0, play_length]`). Sections are re-sorted on success so iteration order stays time-ascending. Returns `False` when the section does not exist or the time is out of range.

```python
unreal.UnrealBridgeAnimLibrary.set_montage_section_start_time(
    '/Game/Anim/AM_Combo', 'Swing2', 0.60)
```

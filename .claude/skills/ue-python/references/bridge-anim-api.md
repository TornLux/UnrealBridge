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

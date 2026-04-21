# UnrealBridge Animation Library API

Module: `unreal.UnrealBridgeAnimLibrary`

> **Authoring or modifying an ABP?** Jump to [**Authoring an Animation Blueprint (agent workflow)**](#authoring-an-animation-blueprint-agent-workflow) before touching any write op. That section is the ordered recipe (plan → vars → SM → states → rules → outer graph → layout → verify) with the pitfalls that otherwise cost round-trips: UE 5 float→double rename, rule-graph naming collisions, pose-pin plurality, `bCanEnterTransition` wiring, and the post-authoring verification checklist.

> **Reactive framework:** this library *inspects* animations statically.
> To run a Python script **when** an `AnimNotify` plays or a montage
> section triggers during PIE, use
> `UnrealBridgeReactiveLibrary.register_runtime_anim_notify` — see
> `bridge-reactive.md` for the full registration shape. Pair
> `get_anim_sequence_info(...)` / `get_montage_info(...)` here with
> reactive registration to discover notify names first, then bind
> handlers to the ones you care about.

> **Visual pose inspection lives in `bridge-level-api.md`, not here.**
> This library returns *metadata* (durations, sections, notifies,
> segments, curves). To see what the character actually looks like at
> a given time — the visual pose — use the Vision-category renderers
> on `UnrealBridgeLevelLibrary`:
>
> - `capture_anim_pose_grid(anim_path, time, mesh, views, ...)` — one
>   pose at a specific time, multi-view PNG grid.
> - `capture_anim_montage_timeline(anim_path, mesh, num_time_samples,
>   views, ...)` — whole timeline as a grid (rows = time samples,
>   columns = views), with optional bone overlay, ground grid, and
>   root-trajectory polyline.
>
> They run in an isolated `FPreviewScene`, so the project's level
> lighting / sky / actors do not appear in the frame. Read
> `bridge-level-api.md` for full signatures.
>
> **Sampling rule (mandatory for "analyze this animation" requests):**
> use **at least 16 time samples** in `capture_anim_montage_timeline`
> so sub-0.2-second pose transitions don't get missed. Short montages
> (<1s) can still use 16 — the cost is linear in samples, and under-
> sampling hides impact frames / windup peaks that are the entire
> point of reading the pose. Go higher (24-32) for long combat
> montages where multiple strikes chain. **Do NOT** answer an
> "analyze pose" / "analyze animation" question from metadata alone;
> render the grid first, look at it, then correlate against metadata.

---

## Authoring an Animation Blueprint (agent workflow)

> **Read this entire section before writing or modifying an ABP.** It replaces "just call the write ops" with a verified-correct recipe. Skipping it routinely produces ABPs that compile-warn (`"transition will never be taken"`), have collision-named rule graphs, or leave the Output Pose floating unconnected.

The AnimGraph write ops in this file + the graph-write ops in `bridge-blueprint-api.md` are the authoring surface. State machines, states, conduits, transitions, rule logic, and the outer blend graph can all be authored from a single `exec-file` script. Reads (`list_anim_graphs`, `find_anim_graph_node_by_class`, `get_anim_graph_info`) bridge the gaps when write ops need node GUIDs you haven't stored in a variable.

### Workflow (in order — each step depends on the previous)

**1. Plan on paper first.** Before any bridge call, write down in text:
- State list + which is default + which sequence each state plays.
- Transition edges: `from -> to`, crossfade duration, and the rule condition ("Speed > 200").
- Outer graph topology: what sits between the state machine output and `Output Pose` (Slot? LayeredBoneBlend? ApplyAdditive?) and where secondary pose sources plug in.
- Driving variables: name + type + default (e.g. `Speed : Float = 0.0`, `IsAiming : Bool = false`).

An ABP authored from a complete plan compiles clean. An ABP authored incrementally usually has orphan nodes and missing default-state links.

**2. Create the ABP asset (raw `unreal` — there's no bridge wrapper for this):**
```python
f = unreal.AnimBlueprintFactory()
f.set_editor_property('target_skeleton', unreal.load_asset(SKELETON_PATH))
unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    ABP_SHORT_NAME, ABP_PACKAGE_PATH, unreal.AnimBlueprint, f)
```
`set_editor_property('target_skeleton', ...)` is required — `target_skeleton = skel` raises `AttributeError`.

**3. Add ABP variables** via the BP library (`blib.add_blueprint_variable(abp, name, 'Float', '0.0')`). Type strings: `"Bool"`, `"Int"`, `"Float"`, `"Double"`, `"String"`, `"Name"`, `"Vector"`, `"Rotator"`, `"Transform"`, class path for object refs, `"Array of Float"` for arrays.

**4. Build the state machine shell:**
```python
sm = alib.add_anim_graph_node_state_machine(ABP, 'AnimGraph', 'Locomotion', x, y)
for name, px in [('Idle', 0), ('Walk', 400), ('Run', 800)]:
    alib.add_anim_state(ABP, 'Locomotion', name, px, 0)
alib.set_anim_state_default(ABP, 'Locomotion', 'Idle')  # REQUIRED — an SM with no default fails at runtime
```
Two invariants to check:
- The `graph_name` argument for every subsequent state-machine op is the **interior graph name** (the second arg to `add_anim_graph_node_state_machine`), **not** the outer SM node's GUID.
- Every state's inner anim graph is addressed by the **state name** (same string you passed to `add_anim_state`) because `UAnimStateNode::GetStateName() == BoundGraph->GetName()`.

**5. Author transitions (endpoints only, properties, then rule logic):**
```python
for fr, to, cx in EDGES:
    alib.add_anim_transition(ABP, 'Locomotion', fr, to)
    alib.set_anim_transition_properties(ABP, 'Locomotion', fr, to, cx, 0, False)
# Rule graphs are auto-named "Rule_<From>_to_<To>". Do NOT hard-code that
# string — query via get_anim_transition_rule_graph_name for future-proofing.
```

**6. Fill each state's inner anim graph.** At minimum every state needs one pose source wired to its auto-created `StateResult`:
```python
for state, seq in STATE_SEQS:
    sp = alib.add_anim_graph_node_sequence_player(ABP, state, seq, -400, 0)
    result = alib.find_anim_graph_node_by_class(ABP, state, 'AnimGraphNode_StateResult')
    alib.connect_anim_graph_pins(ABP, state, sp, 'Pose', result, 'Result')
```
Conduits use a bool-rule graph internally (same shape as a transition rule), not an anim graph — don't try to drop a SequencePlayer in a conduit.

**7. Author each transition's rule** (this is the step that's easy to skip — compile still succeeds, but the ABP never moves between states):
```python
MATH = '/Script/Engine.KismetMathLibrary'
def author_speed_gt(fr, to, rhs):
    rg = alib.get_anim_transition_rule_graph_name(ABP, 'Locomotion', fr, to)
    var = blib.add_variable_node(ABP, rg, 'Speed', False, -400, 0)
    cmp = blib.add_call_function_node(ABP, rg, MATH, 'Greater_DoubleDouble', -150, 0)
    res = alib.find_anim_graph_node_by_class(ABP, rg, 'AnimGraphNode_TransitionResult')
    blib.connect_graph_pins(ABP, rg, var, 'Speed',       cmp, 'A')
    blib.set_pin_default_value(ABP, rg, cmp, 'B', str(rhs))
    blib.connect_graph_pins(ABP, rg, cmp, 'ReturnValue', res, 'bCanEnterTransition')
```
For "always true" / "never" rules, skip the node authoring and use `alib.set_anim_transition_const_rule(abp, sm, fr, to, bool_value)` instead. That's the only time the const-rule shortcut is correct — use it sparingly (it's useless for anything real).

**Boolean combinations.** AND / OR between multiple comparators: author both comparators, then a `BooleanAND` / `BooleanOR` call into the result pin. Use `Not_PreBool` for negation. All live in `KismetMathLibrary`.

**Re-querying rules from `get_anim_graph_info`** is read-only — a transition whose rule graph contains nodes looks identical to a transition with a const-true shortcut. Use `list_anim_graph_nodes(abp, rule_graph_name)` if you need to see *how* a rule is implemented.

**8. Wire the outer AnimGraph.** Pattern: `StateMachine -> Slot (for montages) -> LayeredBoneBlend / ApplyAdditive (for overlays) -> Output Pose`. The `Output Pose` root is auto-created — find it via `alib.find_anim_graph_node_by_class(abp, 'AnimGraph', 'AnimGraphNode_Root')`, then wire `<last_node>.Pose -> Root.Result`.

Pose-input pin names come from the runtime struct's `FPoseLink` field name:
| Node class | Input pins | Output pin |
|------------|------------|------------|
| `UAnimGraphNode_Slot` | `Source` | `Pose` |
| `UAnimGraphNode_LayeredBoneBlend` | `BasePose`, `BlendPoses_0`, `BlendPoses_1`, ... (**plural!**) + `BlendWeights_0`, ... | `Pose` |
| `UAnimGraphNode_ApplyAdditive` | `Base`, `Additive`, `Alpha` | `Pose` |
| `UAnimGraphNode_TwoWayBlend` | `A`, `B`, `Alpha` | `Pose` |
| `UAnimGraphNode_BlendListByBool` | `ActiveValue`, `True Pose`, `False Pose` | `Pose` |
| `UAnimGraphNode_BlendListByInt` | `ActiveChildIndex`, `Blend Pose 0`, `Blend Pose 1`, ... | `Pose` |
| `UAnimGraphNode_StateMachine` | — | `Pose` |
| `UAnimGraphNode_SequencePlayer` / `_BlendSpacePlayer` / `_LinkedAnimLayer` | (asset + param pins) | `Pose` |
| `UAnimGraphNode_Root` (Output Pose) | `Result` | — |

If a `connect_anim_graph_pins(...)` returns `False`, the pin name is almost always wrong (plural vs singular, or the node's renamed it). **First debug step** is always `blib.get_node_pins(abp, graph_name, node_guid)` — it prints every pin's authoritative name + direction + type.

**9. Auto-layout.** Mandatory after **any** anim write — AnimGraph nodes spawn at whatever `(x,y)` you passed, which is a human's estimate, not a tidy layout:
```python
alib.auto_layout_anim_graph(ABP, 'AnimGraph', 0, 0)
alib.auto_layout_state_machine(ABP, 'Locomotion', 0, 0)
# state-machine layout recurses into state inner graphs + rule graphs; you
# don't need a separate pass per state unless you want different spacing.
```
`auto_layout_state_machine` recurses into every state's inner anim graph AND every transition rule graph — one call tidies the entire machine.

**10. Compile + verify + save** (see next section). Save only after the verification pass is clean.

### Graph addressing cheat sheet

Every write op takes a `graph_name` string. Decoding:

| You want to write in… | `graph_name` to pass |
|-----------------------|----------------------|
| The top-level AnimGraph | `"AnimGraph"` |
| The interior of a state machine | The string you passed to `add_anim_graph_node_state_machine` (e.g. `"Locomotion"`) |
| A state's inner pose graph | The state name (e.g. `"Idle"`) |
| A conduit's rule | The conduit name |
| A transition's rule | `get_anim_transition_rule_graph_name(abp, sm, from, to)` — returns `"Rule_<From>_to_<To>"` |
| Any of the above, listed programmatically | `list_anim_graphs(abp)` — `.name` column |

If unsure, dump every graph first: `for g in lib.list_anim_graphs(abp): print(g.kind, g.name, g.parent_graph_name)`.

### Rule-authoring gotchas (read these; they cost real round-trips)

1. **UE 5 renamed float comparators.** `Greater_FloatFloat` / `Less_FloatFloat` / `EqualEqual_FloatFloat` / `NotEqual_FloatFloat` do **not exist** in UE 5.x. Use `Greater_DoubleDouble` / `Less_DoubleDouble` / `EqualEqual_DoubleDouble` / `NotEqual_DoubleDouble`. All `KismetMathLibrary` float math went to double. Using the old name makes `add_call_function_node` silently return an empty GUID — and the compile will then warn `"transition will never be taken"` with no other hint.
2. **Rule-result pin name.** Always `bCanEnterTransition` (with the `b` in C++). From Python via `set_pin_default_value(..., 'bCanEnterTransition', ...)` — note: this is a **pin name** being passed as a string, so the `b` prefix stays (unlike USTRUCT fields which Python strips).
3. **Always break any pre-existing link before setting a pin default.** `set_anim_transition_const_rule` does this for you. Custom rule authoring should call `connect_graph_pins(cmp, 'ReturnValue', res, 'bCanEnterTransition')` which replaces the default.
4. **Bi-directional transitions.** The `bidirectional` flag on a single transition is NOT a shortcut for authoring two one-way edges — it's a specific feature that lets a single transition run in reverse during blend-out. For normal "Idle ⇄ Walk" behaviour, author **two separate transitions** (Idle→Walk and Walk→Idle), each with its own rule.
5. **Conduit rule graphs.** A conduit is a branching state whose `BoundGraph` is a bool rule graph (not an anim graph). Author rule nodes the same way as a transition rule — the sink is `AnimGraphNode_TransitionResult` with a `bCanEnterTransition` pin. To let execution flow *through* the conduit, wire *outgoing* transitions from the conduit to the real target states.
6. **Don't author inside the `EventGraph`** by accident. `EventGraph` on an ABP is a K2 graph for `BlueprintUpdateAnimation` and `BlueprintThreadSafeUpdateAnimation` — it's where you would *compute* `Speed` (e.g. read from the owning pawn's velocity). Logic that computes transition inputs goes here; rule comparisons go in the transition rule graphs.

### Post-authoring verification (mandatory)

After every authoring pass, run this checklist before returning "done":

```python
import unreal
ABP = '...'  # your ABP path
alib = unreal.UnrealBridgeAnimLibrary
blib = unreal.UnrealBridgeBlueprintLibrary
elib = unreal.UnrealBridgeEditorLibrary

# 1. Recompile — if this fails or warns, no other check matters.
ok = elib.recompile_blueprint(ABP)
errors = blib.get_compile_errors(ABP)
print('recompile=', ok, '  issues=', len(errors))
for e in errors:
    print(f'  [{e.severity}] {e.message}')

# 2. Structural: is there exactly one default state per state machine? Is
#    every state reachable? Do all transitions have both endpoints?
for sm in alib.get_anim_graph_info(ABP):
    defaults = [s for s in sm.states if s.is_default]
    print(f'SM "{sm.name.splitlines()[0]}": states={len(sm.states)} '
          f'default={defaults[0].name if defaults else "NONE"} trans={len(sm.transitions)}')
    orphans = {s.name for s in sm.states} - {t.from_state for t in sm.transitions} \
                                           - {t.to_state   for t in sm.transitions}
    if orphans:
        print(f'  WARNING: unreachable states: {orphans}')

# 3. Rule graphs: every transition's rule graph should have at least 2 nodes
#    (TransitionResult + at least one bool producer). A 1-node rule graph
#    means no rule was authored — that transition will never fire.
for g in alib.list_anim_graphs(ABP):
    if g.kind == 'TransitionRule' and g.num_nodes < 2:
        print(f'  WARNING: rule graph {g.name} has only {g.num_nodes} node(s)')

# 4. Outer AnimGraph: the Root (Output Pose) must have its Result pin
#    connected. A disconnected root compiles with a warning but renders
#    nothing.
root = alib.find_anim_graph_node_by_class(ABP, 'AnimGraph', 'AnimGraphNode_Root')
pins = blib.get_node_pins(ABP, 'AnimGraph', root)
for p in pins:
    if p.name == 'Result':
        if not p.connections:
            print('  WARNING: AnimGraph root has no Result connection')

# 5. Each state's inner graph: StateResult.Result should be connected.
for g in alib.list_anim_graphs(ABP):
    if g.kind != 'State':
        continue
    sr = alib.find_anim_graph_node_by_class(ABP, g.name, 'AnimGraphNode_StateResult')
    if not sr:
        print(f'  WARNING: state {g.name} has no StateResult (unexpected)')
        continue
    pins = blib.get_node_pins(ABP, g.name, sr)
    for p in pins:
        if p.name == 'Result' and not p.connections:
            print(f'  WARNING: state {g.name} StateResult has no pose source')
```

Interpret the output:

- **Any `[Error]` from `get_compile_errors`** → fix before anything else.
- **`"transition will never be taken"`** → that edge has no rule. Go back to step 7 and author one (or explicitly use `set_anim_transition_const_rule` if always-true is truly what you want).
- **`"Blend Poses N was visible but ignored"`** → you spawned `add_anim_graph_node_layered_bone_blend(..., n, ...)` with too many blend slots. Either wire the extras or re-create with a smaller count.
- **`default=NONE`** on a state machine → `set_anim_state_default` wasn't called. Runtime will pick the first state deterministically but this is almost always a mistake.
- **Unreachable states** → a state exists but no transition ends at it. Either author a transition into it or remove it.
- **Rule graph with only 1 node** → the `TransitionResult` sink exists but nothing feeds `bCanEnterTransition`. That transition never fires.
- **Root.Result unconnected** → the ABP evaluates to the ref pose (T-pose). Wire your final pose sink to it.
- **StateResult.Result unconnected in a state** → the state itself evaluates to ref pose when active.

Only after every warning is resolved or explicitly justified, call:
```python
unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(ABP))
```

### When things go wrong

| Symptom | Most likely cause | First check |
|---------|------------------|-------------|
| `add_call_function_node` returns `""` | Function name wrong (UE 5 rename or typo) | Try the `_DoubleDouble` variant; grep engine headers for the FName |
| `connect_anim_graph_pins` returns `False` | Pin name wrong (plural vs singular, renamed) | `blib.get_node_pins(abp, graph_name, node_guid)` — trust its output |
| `add_anim_state` succeeds but state is invisible in editor | The editor has the SM graph open and needs to refresh | Re-open the asset: `elib.open_asset(abp)` |
| `set_anim_transition_properties` seems to not change anything | You passed `-1` / `MIN_int32` sentinels | `crossfade=0.2, priority=0` — negative means "leave unchanged" |
| Compile: `"transition will never be taken"` | Rule graph empty or wrong wiring | `alib.list_anim_graph_nodes(abp, 'Rule_X_to_Y')` to inspect |
| Everything looks right, ABP shows T-pose at runtime | `Output Pose.Result` unconnected OR every StateResult empty | Run verification step 4 + step 5 |
| Python `AttributeError: 'FBridgeX' object has no attribute 'b_...'` | UE Python strips `b` prefix from USTRUCT bool fields | `.succeeded` not `.b_succeeded`; `.is_default` not `.b_is_default` |

---

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

> **Pairs with reactive:** the `notifies` list here surfaces every
> `notify_name` on the sequence. Feed those names straight into
> `register_runtime_anim_notify(notify_name=...)` from `bridge-reactive.md`
> to run Python when a notify plays during PIE.

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

> **Pairs with reactive:** the returned `notifies` list enumerates
> montage notifies at their trigger times. Same registration pattern as
> `get_anim_sequence_info` — bind each interesting `notify_name` via
> `register_runtime_anim_notify` in `bridge-reactive.md` to react to
> frame-perfect events (impact, SFX cue, combo window).

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

---

## Sync Marker Write Ops

### add_anim_sync_marker(sequence_path, marker_name, time) -> bool

Append an authored sync marker to an `UAnimSequence`. Rejects empty name or `time` outside `[0, play_length]`. After write, `RefreshSyncMarkerDataFromAuthored` is called so the skeleton's unique-marker-name cache is updated and the marker is usable at runtime. Markers are re-sorted by time. Marks the package dirty on success.

> Token cost: LOW. One round-trip, bool return.

```python
ok = unreal.UnrealBridgeAnimLibrary.add_anim_sync_marker(
    '/Game/Anim/Run_Fwd', 'LeftFoot', 0.25)
if ok:
    unreal.EditorAssetLibrary.save_asset('/Game/Anim/Run_Fwd')
```

### remove_anim_sync_markers_by_name(sequence_path, marker_name) -> int

Remove every authored sync marker whose name matches (exact `FName` compare). Returns the count removed; `0` if none matched. `RefreshSyncMarkerDataFromAuthored` is called when any were removed.

```python
n = unreal.UnrealBridgeAnimLibrary.remove_anim_sync_markers_by_name(
    '/Game/Anim/Run_Fwd', 'LeftFoot')
```

---

## Socket Write Ops (edit / rename)

### set_skeleton_socket_transform(skeleton_path, socket_name, relative_location, relative_rotation, relative_scale) -> bool

Overwrite the transform of an existing socket on a skeleton. Returns `False` when the socket is missing. Marks the skeleton package dirty on success — caller must save to persist.

```python
ok = unreal.UnrealBridgeAnimLibrary.set_skeleton_socket_transform(
    '/Game/Skel/SK_Hero_Skeleton',
    'weapon_r_socket',
    unreal.Vector(5.0, 2.0, 0.0),
    unreal.Rotator(0.0, 90.0, 0.0),
    unreal.Vector(1.0, 1.0, 1.0))
```

### rename_skeleton_socket(skeleton_path, old_name, new_name) -> bool

Rename an existing skeleton socket. Fails when the old socket does not exist, `new_name` is empty, `old_name == new_name`, or another socket already uses `new_name`.

> Note: existing meshes / attachments that reference the old socket name will break — fix those up separately.

```python
ok = unreal.UnrealBridgeAnimLibrary.rename_skeleton_socket(
    '/Game/Skel/SK_Hero_Skeleton', 'weapon_socket', 'weapon_r_socket')
```

---

## AnimBlueprint Metadata

### get_anim_blueprint_info(anim_blueprint_path) -> FBridgeAnimBlueprintInfo

Single-call summary of an AnimBlueprint's high-level shape: parent class, target skeleton, preview mesh, counts of state machines / linked layers / slots, and implemented interfaces. Cheap alternative to `get_anim_graph_info + get_anim_linked_layers + get_anim_slots` when you only want counts and which ALI interfaces the ABP exposes.

> Token cost: LOW. Fixed-size record plus a short interface-name list (typically 0–3 entries).

```python
info = unreal.UnrealBridgeAnimLibrary.get_anim_blueprint_info('/Game/Anim/ABP_Character')
print(f'{info.name} : {info.parent_class}')
print(f'  skeleton = {info.target_skeleton}')
print(f'  preview_mesh = {info.preview_skeletal_mesh}')
print(f'  template = {info.is_template}')
print(f'  SMs={info.num_state_machines}  layers={info.num_linked_layers}  slots={info.num_slots}')
for iface in info.implemented_interfaces:
    print(f'  implements {iface}')
```

### FBridgeAnimBlueprintInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | ABP asset name |
| `parent_class` | str | Parent class path (e.g. `/Script/Engine.AnimInstance`) |
| `target_skeleton` | str | Target skeleton soft path (empty on a Template ABP) |
| `preview_skeletal_mesh` | str | Editor preview mesh soft path (empty if unset) |
| `is_template` | bool | True for skeleton-agnostic Template AnimBlueprints |
| `num_state_machines` | int | Count of `AnimGraphNode_StateMachineBase` across all graphs |
| `num_linked_layers` | int | Count of `AnimGraphNode_LinkedAnimLayer` bindings |
| `num_slots` | int | Count of `AnimGraphNode_Slot` nodes |

---

## AnimGraph / State Machine write ops

Mirror image of the read ops above. Every graph-modifying call:

- runs inside an `FScopedTransaction` so Ctrl+Z reverts it,
- ends with `UEdGraph::NotifyGraphChanged()` + `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ABP)`,
- does **not** force-compile — call `unreal.UnrealBridgeEditorLibrary.recompile_blueprint(path)` once after a batch of edits and `unreal.EditorAssetLibrary.save_loaded_asset(...)` to persist.

**Graph addressing.** Every write op takes a `graph_name` string. This is the `UEdGraph::GetName()` of the target graph:

- `"AnimGraph"` → the top-level anim graph.
- A state-machine graph name (e.g. `"Locomotion"`) → its interior, used for states/conduits/transitions.
- A state's name (e.g. `"Idle"`) → the anim graph inside that state. Add pose nodes with `add_anim_graph_node_*` just like the top-level AnimGraph.
- A transition's rule graph name (the ugly `"Transition"` / `"AnimationTransitionGraph_N"` default) → the bool-rule sub-graph; usually you set constant rules via `set_anim_transition_const_rule` rather than authoring nodes.

Enumerate all of them with `list_anim_graphs`:

### list_anim_graphs(anim_blueprint_path) → list[FBridgeAnimGraphSummary]

Returns every graph inside the ABP (top-level anim graph, each state-machine graph, each state's inner anim graph, each conduit, each transition rule graph, plus regular function / ubergraph / macro graphs). Use the `name` field as the `graph_name` parameter to every write op below.

```python
for g in unreal.UnrealBridgeAnimLibrary.list_anim_graphs(ABP):
    print(f'{g.kind:15s} {g.name:30s} parent={g.parent_graph_name} nodes={g.num_nodes}')
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | `UEdGraph::GetName()` — pass this as `graph_name` |
| `kind` | str | `"AnimGraph"`, `"StateMachine"`, `"State"`, `"Conduit"`, `"TransitionRule"`, `"Function"`, `"Ubergraph"`, `"Macro"` |
| `parent_graph_name` | str | Enclosing graph name (empty for top-level entries) |
| `num_nodes` | int | Node count in this graph |

### list_anim_graph_nodes(abp, graph_name) → list[str]

Returns one row per node in any graph reachable through `list_anim_graphs` — state machine interiors, state BoundGraphs, and transition rule graphs included. Rows are tab-separated `<guid>\t<short_class_name>\t<list_view_title>`.

```python
for row in unreal.UnrealBridgeAnimLibrary.list_anim_graph_nodes(ABP, 'Idle'):
    guid, cls, title = row.split('\t', 2)
    print(cls, title)
```

Prefer this over `UnrealBridgeBlueprintLibrary.get_function_nodes` when addressing state-machine interiors: the BP-side helper only walks top-level `FunctionGraphs`.

### find_anim_graph_node_by_class(abp, graph_name, short_class_name) → str

Returns the GUID of the first node of the given UClass short name in the graph, or empty string. Handy for locating auto-created sinks / entries when you need to wire to them:

```python
# Find the Output Pose of the top-level AnimGraph
root = lib.find_anim_graph_node_by_class(ABP, 'AnimGraph', 'AnimGraphNode_Root')
# Find the StateResult inside a named state
result = lib.find_anim_graph_node_by_class(ABP, 'Idle', 'AnimGraphNode_StateResult')
# Find the Entry node of a state machine
entry = lib.find_anim_graph_node_by_class(ABP, 'Locomotion', 'AnimStateEntryNode')
```

### AnimGraph node factories

All return the new node's GUID as a hex string (`EGuidFormats::Digits`), or empty string on failure.

| Function | Maps to UE class |
|----------|------------------|
| `add_anim_graph_node_sequence_player(abp, graph_name, sequence_path, x, y)` | `UAnimGraphNode_SequencePlayer` (empty path = unbound) |
| `add_anim_graph_node_blend_space_player(abp, graph_name, blend_space_path, x, y)` | `UAnimGraphNode_BlendSpacePlayer` (1D or 2D) |
| `add_anim_graph_node_slot(abp, graph_name, slot_name, x, y)` | `UAnimGraphNode_Slot` — slot auto-registered on target skeleton |
| `add_anim_graph_node_state_machine(abp, graph_name, sm_name, x, y)` | `UAnimGraphNode_StateMachine` — interior `UAnimationStateMachineGraph` auto-created and renamed to `sm_name` |
| `add_anim_graph_node_layered_bone_blend(abp, graph_name, num_blend_poses, x, y)` | `UAnimGraphNode_LayeredBoneBlend` (extra poses added via `AddPinToBlendByFilter`) |
| `add_anim_graph_node_blend_list_by_bool(abp, graph_name, x, y)` | `UAnimGraphNode_BlendListByBool` |
| `add_anim_graph_node_blend_list_by_int(abp, graph_name, num_poses, x, y)` | `UAnimGraphNode_BlendListByInt` (min 2; extras via `AddPinToBlendList`) |
| `add_anim_graph_node_two_way_blend(abp, graph_name, x, y)` | `UAnimGraphNode_TwoWayBlend` — note: runtime field is `BlendNode`, not `Node` |
| `add_anim_graph_node_linked_anim_layer(abp, graph_name, interface_class_path, layer_name, x, y)` | `UAnimGraphNode_LinkedAnimLayer` — reconstructs to surface interface-defined pins |
| `add_anim_graph_node_by_class_name(abp, graph_name, short_class_name, x, y)` | Fallback: any concrete `UAnimGraphNode_Base` subclass by short name (e.g. `"AnimGraphNode_ApplyAdditive"`, `"AnimGraphNode_ModifyBone"`, `"AnimGraphNode_UseCachedPose"`, `"AnimGraphNode_SaveCachedPose"`, `"AnimGraphNode_ObserveBone"`, `"AnimGraphNode_RefPose"`) |

```python
lib = unreal.UnrealBridgeAnimLibrary
seq  = lib.add_anim_graph_node_sequence_player(ABP, 'AnimGraph', '/Game/Anim/AS_Walk', -800, 0)
slot = lib.add_anim_graph_node_slot(ABP, 'AnimGraph', 'UpperBody', -500, 0)
sm   = lib.add_anim_graph_node_state_machine(ABP, 'AnimGraph', 'Locomotion', 0, 0)
apply_additive = lib.add_anim_graph_node_by_class_name(ABP, 'AnimGraph', 'AnimGraphNode_ApplyAdditive', 300, 0)
```

### Pin / node write ops

| Function | Behaviour |
|----------|-----------|
| `connect_anim_graph_pins(abp, graph_name, src_guid, src_pin, tgt_guid, tgt_pin)` → bool | Uses the owning graph's schema — `UAnimationGraphSchema` auto-inserts local↔component conversion nodes when needed. Pose-input pins on anim nodes are named after the `FPoseLink` property (e.g. Slot → `"Source"`, LayeredBoneBlend → `"BasePose"`); pose-output pin is `"Pose"`. |
| `disconnect_anim_graph_pin(abp, graph_name, node_guid, pin_name)` → bool | Breaks every link on one pin. |
| `remove_anim_graph_node(abp, graph_name, node_guid)` → bool | Removes the node and breaks all its links. |
| `set_anim_graph_node_position(abp, graph_name, node_guid, x, y)` → bool | Updates `NodePosX` / `NodePosY`. |
| `set_anim_sequence_player_sequence(abp, graph_name, node_guid, sequence_path)` → bool | Swap the bound sequence on an existing SequencePlayer. Empty path clears it. |
| `set_anim_slot_name(abp, graph_name, node_guid, slot_name)` → bool | Change a Slot node's `SlotName`; slot auto-registered on the skeleton if missing. |

### State machine interior write ops

Target the state-machine graph by name (the `name` returned by `list_anim_graphs` for entries of kind `"StateMachine"`).

| Function | Behaviour |
|----------|-----------|
| `add_anim_state(abp, sm_graph_name, state_name, x, y)` → guid | Creates a `UAnimStateNode`; its inner `UAnimationStateGraph` + `UAnimGraphNode_StateResult` are auto-created and the graph is renamed to `state_name`. |
| `add_anim_conduit(abp, sm_graph_name, conduit_name, x, y)` → guid | `UAnimStateConduitNode` — inner graph is a **bool rule graph** (not an anim graph). |
| `add_anim_transition(abp, sm_graph_name, from_state_name, to_state_name)` → guid | Creates a `UAnimStateTransitionNode` and wires its In/Out pins in one atomic step. Midpoint position is derived from the two endpoints. |
| `remove_anim_state(abp, sm_graph_name, state_name)` → bool | Removes the state + every transition attached to it. |
| `remove_anim_transition(abp, sm_graph_name, from_state_name, to_state_name)` → bool | Removes the first matching transition. |
| `set_anim_state_default(abp, sm_graph_name, state_name)` → bool | Relinks the `UAnimStateEntryNode` output pin to the given state's input pin. |
| `rename_anim_state(abp, sm_graph_name, old_name, new_name)` → bool | Rename is done via `FEdGraphUtilities::RenameGraphToNameOrCloseToName` on the state's `BoundGraph` — the graph's name IS the state name (see `UAnimStateNode::GetStateName`). |
| `set_anim_transition_properties(abp, sm_graph_name, from_name, to_name, crossfade, priority, bidirectional)` → bool | `crossfade < 0` and `priority == -2147483648` (`MIN_int32`) are sentinels for "leave unchanged". `bidirectional` is always written. |
| `set_anim_transition_const_rule(abp, sm_graph_name, from_name, to_name, bool_value)` → bool | Shortcut: set the rule graph's `bCanEnterTransition` pin default to `true` / `false` (breaking any pre-existing links). Use this for always-transition / never-transition cases; for conditional rules, use `get_anim_transition_rule_graph_name` + the Blueprint library. |
| `get_anim_transition_rule_graph_name(abp, sm_graph_name, from_name, to_name)` → str | Get the rule graph's name (`Rule_<From>_to_<To>`) for a transition. Pass this as `graph_name` to the BP library's `add_variable_node` / `add_call_function_node` / `connect_graph_pins` to author real conditional logic. Empty string if the transition doesn't exist. |

```python
sm  = lib.add_anim_graph_node_state_machine(ABP, 'AnimGraph', 'Locomotion', 0, 0)
idle = lib.add_anim_state(ABP, 'Locomotion', 'Idle',  0,    0)
walk = lib.add_anim_state(ABP, 'Locomotion', 'Walk',  300,  0)
lib.set_anim_state_default(ABP, 'Locomotion', 'Idle')
lib.add_anim_transition(ABP, 'Locomotion', 'Idle', 'Walk')
lib.add_anim_transition(ABP, 'Locomotion', 'Walk', 'Idle')
lib.set_anim_transition_properties(ABP, 'Locomotion', 'Idle', 'Walk', 0.25, 0, False)
lib.set_anim_transition_const_rule(ABP, 'Locomotion', 'Walk', 'Idle', True)
```

**Recipe — author poses inside a state.** A state's inner anim graph is addressed by the state name, because `UAnimStateNode::GetStateName` == `BoundGraph->GetName()`:

```python
seq = lib.add_anim_graph_node_sequence_player(ABP, 'Idle', '/Game/Anim/AS_Idle', -300, 0)
# The state's StateResult node already exists — connect into its "Result" input:
for n in lib.get_anim_graph_nodes(ABP):  # note: this queries the top-level AnimGraph, not state internals
    pass
# For state internals, grab the StateResult via list_anim_graphs + a custom walk,
# or simply wire by querying the state's graph nodes through raw unreal.EditorAssetLibrary.
```

### Auto-layout

Mirrors the BP `auto_layout_graph` shape but tuned for anim graphs (pose-flow layering) and state machines (grid). Returns `FBridgeAnimLayoutResult { succeeded, nodes_positioned, layer_count, bounds_width, bounds_height, warnings }`.

| Function | Notes |
|----------|-------|
| `auto_layout_anim_graph(abp, graph_name, h_spacing, v_spacing)` | Sinks (Root / StateResult / TransitionResult) anchor the rightmost layer; layers grow leftward. Pass 0 for default spacing (100 H / 60 V). |
| `auto_layout_state_machine(abp, sm_graph_name, h_spacing, v_spacing)` | States laid out in a ceil(sqrt(N))-wide grid; transitions parked at endpoint midpoints. **Recurses into every state's inner anim graph and every transition's rule graph** — one call tidies the whole machine. Default spacing 300 H / 200 V. |

```python
r = lib.auto_layout_anim_graph(ABP, 'AnimGraph', 0, 0)
print(f'graph laid out: {r.nodes_positioned} nodes, {r.layer_count} layers')

r = lib.auto_layout_state_machine(ABP, 'Locomotion', 0, 0)
```

### Persistence

Write ops mark the ABP structurally modified; changes are visible immediately in the editor, but you still need to compile + save to persist:

```python
unreal.UnrealBridgeEditorLibrary.recompile_blueprint(ABP)
unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(ABP))
```

### Gotchas

- **Python bool field names.** `FBridgeAnimLayoutResult::bSucceeded` → `.succeeded` in Python (UE Python strips the `b` prefix from USTRUCT bool fields). Same for `FBridgeAnimState::bIsDefault` → `.is_default`, `bIsConduit` → `.is_conduit`, and `FBridgeAnimTransition::bBidirectional` → `.bidirectional`.
- **Transition property sentinels.** `set_anim_transition_properties` uses `crossfade < 0` to mean "leave unchanged" and `MIN_int32` for the same semantics on `priority` (since 0 is a valid priority).
- **State-machine "name" ambiguity.** `AddAnimGraphNodeStateMachine` returns the *outer* node's GUID (the one in `AnimGraph`). The *interior* graph — which every state / transition op targets — is a separate object named by the `sm_name` parameter. The outer GUID and the interior name address different objects.
- **State name vs. state node.** Passing `state_name` to `remove_anim_state`, `rename_anim_state`, `set_anim_state_default`, `add_anim_transition` etc. looks up the `UAnimStateNodeBase` by calling `GetStateName()` — which is just `BoundGraph->GetName()`. So "renaming a state" really means renaming the state's inner graph.
- **Transition rule authoring.** `set_anim_transition_const_rule` is the fast path for true/false rules. For conditional logic ("crouch && moving"), query the rule graph's name via `list_anim_graphs` — it'll be something like `"Transition"` or `"AnimationTransitionGraph_N"` — and author the condition nodes with the Blueprint library (`add_variable_node` / `add_function_call_node` / `connect_graph_pins` etc.) against that graph name, ending at the rule graph's `TransitionResult` node's `bCanEnterTransition` input.
- **No force-compile per op.** Each write op notifies the graph and marks the BP structurally modified but does not recompile. Batch your edits, then call `recompile_blueprint` once.

### End-to-end example: locomotion ABP with real variable-driven rules

Compact script that creates an ABP, authors a 3-state locomotion state machine with **real** `Speed > X` / `Speed < X` rules (not always-true shortcuts), and layers the SM output with an overlay anim in the outer graph. Verified on the UEFN Mannequin in GameAnimationSample 5.7:

```python
import unreal

SK   = '/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin'
IDLE = '/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop'
WALK = '/Game/Characters/UEFN_Mannequin/Animations/Walk/M_Neutral_Walk_Loop_LR.M_Neutral_Walk_Loop_LR'
RUN  = '/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F'
UPPR = '/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Break_v01.M_Neutral_Stand_Idle_Break_v01'
ABP  = '/Game/Demo/ABP_LocomotionDemo.ABP_LocomotionDemo'
MATH = '/Script/Engine.KismetMathLibrary'
alib = unreal.UnrealBridgeAnimLibrary
blib = unreal.UnrealBridgeBlueprintLibrary

# 1. Create ABP + drive variable
f = unreal.AnimBlueprintFactory()
f.set_editor_property('target_skeleton', unreal.load_asset(SK))
unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'ABP_LocomotionDemo', '/Game/Demo', unreal.AnimBlueprint, f)
blib.add_blueprint_variable(ABP, 'Speed', 'Float', '0.0')

# 2. State machine + states + default + transitions
sm = alib.add_anim_graph_node_state_machine(ABP, 'AnimGraph', 'Locomotion', 100, 0)
for name, x in [('Idle', 0), ('Walk', 400), ('Run', 800)]:
    alib.add_anim_state(ABP, 'Locomotion', name, x, 0)
alib.set_anim_state_default(ABP, 'Locomotion', 'Idle')
for a, b, cx in [('Idle','Walk',0.20),('Walk','Run',0.25),('Run','Walk',0.25),('Walk','Idle',0.20)]:
    alib.add_anim_transition(ABP, 'Locomotion', a, b)
    alib.set_anim_transition_properties(ABP, 'Locomotion', a, b, cx, 0, False)

# 3. Per-state anim
for state, seq in [('Idle', IDLE), ('Walk', WALK), ('Run', RUN)]:
    sp = alib.add_anim_graph_node_sequence_player(ABP, state, seq, -400, 0)
    result = alib.find_anim_graph_node_by_class(ABP, state, 'AnimGraphNode_StateResult')
    alib.connect_anim_graph_pins(ABP, state, sp, 'Pose', result, 'Result')

# 4. Real transition rules: Speed > X / Speed < X
def author_rule(fr, to, op, rhs):
    rg = alib.get_anim_transition_rule_graph_name(ABP, 'Locomotion', fr, to)
    var = blib.add_variable_node(ABP, rg, 'Speed', False, -400, 0)
    cmp = blib.add_call_function_node(ABP, rg, MATH, op, -150, 0)
    res = alib.find_anim_graph_node_by_class(ABP, rg, 'AnimGraphNode_TransitionResult')
    blib.connect_graph_pins(ABP, rg, var, 'Speed',       cmp, 'A')
    blib.set_pin_default_value(ABP, rg, cmp, 'B', str(rhs))
    blib.connect_graph_pins(ABP, rg, cmp, 'ReturnValue', res, 'bCanEnterTransition')

author_rule('Idle', 'Walk', 'Greater_DoubleDouble',  10.0)
author_rule('Walk', 'Run',  'Greater_DoubleDouble', 200.0)
author_rule('Run',  'Walk', 'Less_DoubleDouble',    200.0)
author_rule('Walk', 'Idle', 'Less_DoubleDouble',     10.0)

# 5. Outer graph: SM -> Slot -> LayeredBoneBlend (+ overlay) -> Root
slot = alib.add_anim_graph_node_slot(ABP, 'AnimGraph', 'UpperBody', 500, 0)
lbb  = alib.add_anim_graph_node_layered_bone_blend(ABP, 'AnimGraph', 2, 900, 0)
ov   = alib.add_anim_graph_node_sequence_player(ABP, 'AnimGraph', UPPR, 500, 300)
root = alib.find_anim_graph_node_by_class(ABP, 'AnimGraph', 'AnimGraphNode_Root')
for s, sp_n, t, tp_n in [
    (sm,   'Pose', slot, 'Source'),       # SM -> Slot.Source
    (slot, 'Pose', lbb,  'BasePose'),     # Slot -> LBB.BasePose
    (ov,   'Pose', lbb,  'BlendPoses_0'), # overlay -> LBB.BlendPoses_0   (plural!)
    (lbb,  'Pose', root, 'Result'),       # LBB -> Output Pose
]:
    alib.connect_anim_graph_pins(ABP, 'AnimGraph', s, sp_n, t, tp_n)

# 6. Tidy + compile + save
alib.auto_layout_anim_graph(ABP, 'AnimGraph', 0, 0)
alib.auto_layout_state_machine(ABP, 'Locomotion', 0, 0)
unreal.UnrealBridgeEditorLibrary.recompile_blueprint(ABP)
unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(ABP))
```

**Gotchas.**

- **Pin plurality.** `UAnimGraphNode_LayeredBoneBlend` uses `BlendPoses_0 / BlendPoses_1` (plural) and `BlendWeights_0`. If a pin-wire op returns `False`, query actual names via `blib.get_node_pins(abp, graph_name, node_guid)` — pin names vary across node classes.
- **UE 5 renamed `Greater_FloatFloat` / `Less_FloatFloat`** to `Greater_DoubleDouble` / `Less_DoubleDouble` (all float math went to double). Use the `_Double` variants in UE 5.x. For bool AND / OR / NOT use `BooleanAND` / `BooleanOR` / `Not_PreBool`.
- **Rule graph naming.** `add_anim_transition` auto-renames the rule graph to `Rule_<From>_to_<To>` so clients can address it unambiguously. Use `get_anim_transition_rule_graph_name(abp, sm, from, to)` instead of hard-coding that string — it's robust to future rename changes.
- **Rule authoring uses the Blueprint library.** `UnrealBridgeBlueprintLibrary.find_graph_by_name` was extended to walk into state-machine interiors + state / conduit / transition `BoundGraph`s + nested `SubGraphs`, so `add_variable_node` / `add_call_function_node` / `connect_graph_pins` / `set_pin_default_value` all work against `graph_name="Rule_Idle_to_Walk"` directly.
| `implemented_interfaces` | list[str] | AnimLayer interface class names |

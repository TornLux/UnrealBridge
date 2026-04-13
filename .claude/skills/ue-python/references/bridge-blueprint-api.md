# UnrealBridge Blueprint Library API

Module: `unreal.UnrealBridgeBlueprintLibrary`

## Class Hierarchy

### get_blueprint_parent_class(blueprint_path) -> (bool, FBridgeClassInfo)

Get the direct parent class of a Blueprint.

```python
success, parent = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_parent_class(
    '/Game/BP/MyBP'
)
print(parent.class_name, parent.class_path, parent.is_native)
```

### get_blueprint_class_hierarchy(blueprint_path) -> list[FBridgeClassInfo]

Full inheritance chain. Index 0 = BP itself, last = UObject.

```python
chain = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_class_hierarchy('/Game/BP/MyBP')
for cls in chain:
    tag = '[Native]' if cls.is_native else '[BP]'
    print(f'{tag} {cls.class_name} — {cls.class_path}')
```

### FBridgeClassInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `class_name` | str | e.g. "MyBP_C" |
| `class_path` | str | e.g. "/Script/Module.ClassName" |
| `is_native` | bool | True = C++ class |

---

## Variables

### get_blueprint_variables(blueprint_path, include_inherited=False) -> list[FBridgeVariableInfo]

List all variables defined in a Blueprint.

```python
vars = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_variables('/Game/BP/MyBP')
for v in vars:
    print(f'{v.name}: {v.type} = {v.default_value}  (editable={v.instance_editable})')

# Include inherited variables from parent classes
all_vars = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_variables('/Game/BP/MyBP', True)
```

### FBridgeVariableInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Variable name |
| `type` | str | "Float", "Vector", "Array of Int", "MyStruct", etc. |
| `category` | str | Editor category |
| `instance_editable` | bool | Visible in Details panel |
| `blueprint_read_only` | bool | Read-only in BP graphs |
| `default_value` | str | Best-effort string representation |
| `description` | str | Tooltip text |
| `replication_condition` | str | "None", "Replicated", "RepNotify" |

---

## Functions / Events

### get_blueprint_functions(blueprint_path, include_inherited=False) -> list[FBridgeFunctionInfo]

List all functions and events.

> ⚠️ **Token cost: MEDIUM–HIGH.** Returns full `FBridgeFunctionInfo` per function (params, description, category, access). A Character BP with 30+ functions × multi-param signatures × tooltips easily runs several KB. **Prefer `get_blueprint_overview`** (returns compact `FBridgeFunctionSummary` — name + kind + signature) when you only need a catalog. `include_inherited=True` multiplies the output by the inheritance chain depth.

```python
funcs = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_functions('/Game/BP/MyBP')
for f in funcs:
    params_str = ', '.join(f'{p.name}: {p.type}' for p in f.params if not p.is_output)
    returns_str = ', '.join(f'{p.name}: {p.type}' for p in f.params if p.is_output)
    print(f'[{f.kind}] {f.name}({params_str}) -> ({returns_str})')
```

### FBridgeFunctionInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Function name |
| `kind` | str | "Function", "Event", "Override" |
| `access` | str | "Public", "Protected", "Private" |
| `is_pure` | bool | No exec pin |
| `is_static` | bool | Static function |
| `category` | str | Editor category |
| `description` | str | Tooltip |
| `params` | list[FBridgeFunctionParam] | Input/output parameters |

### FBridgeFunctionParam fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Parameter name |
| `type` | str | Parameter type |
| `is_output` | bool | True for return/out params |

---

## Components

### get_blueprint_components(blueprint_path) -> list[FBridgeComponentInfo]

Get the component tree (own + inherited from parent CDO).

```python
comps = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_components('/Game/BP/MyActor')
for c in comps:
    prefix = '[Root]' if c.is_root else ('  [Inherited]' if c.is_inherited else '  ')
    print(f'{prefix} {c.name} ({c.component_class}) parent={c.parent_name}')
```

### FBridgeComponentInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Component variable name |
| `component_class` | str | e.g. "StaticMeshComponent" |
| `parent_name` | str | Parent component (empty for root) |
| `is_root` | bool | Scene root component |
| `is_inherited` | bool | From parent class |

---

## Interfaces

### get_blueprint_interfaces(blueprint_path) -> list[FBridgeInterfaceInfo]

List all implemented interfaces and their functions.

```python
ifaces = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_interfaces('/Game/BP/MyBP')
for i in ifaces:
    bp_tag = '[BP]' if i.is_blueprint_implemented else '[Native]'
    print(f'{bp_tag} {i.interface_name}: {", ".join(i.functions)}')
```

### FBridgeInterfaceInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `interface_name` | str | e.g. "BPI_Interactable" |
| `interface_path` | str | Full class path |
| `is_blueprint_implemented` | bool | BP interface vs C++ |
| `functions` | list[str] | Function names declared by this interface |

---

## Overview

### get_blueprint_overview(blueprint_path) -> (bool, FBridgeBlueprintOverview)

Get a compact overview of a Blueprint in a single call. Replaces separate calls to Variables + Functions + Components + Interfaces.

```python
ov = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_overview('/Game/BP/MyBP')
print(f'{ov.blueprint_name} ({ov.blueprint_type})')
print(f'Parent: {ov.parent_class.class_name}')
for v in ov.variables:
    print(f'  var {v.name}: {v.type}')
for f in ov.functions:
    print(f'  [{f.kind}] {f.name}{f.signature}')
for c in ov.components:
    print(f'  component {c.name} ({c.component_class})')
print(f'Interfaces: {list(ov.interfaces)}')
print(f'Dispatchers: {list(ov.event_dispatchers)}')
print(f'Graphs: {list(ov.graph_names)}')
```

### FBridgeBlueprintOverview fields

| Field | Type | Description |
|-------|------|-------------|
| `blueprint_name` | str | Blueprint asset name |
| `blueprint_type` | str | First native ancestor: "Actor", "Character", "AnimInstance", etc. |
| `parent_class` | FBridgeClassInfo | Direct parent class info |
| `variables` | list[FBridgeVariableSummary] | Compact variable list (name + type only) |
| `functions` | list[FBridgeFunctionSummary] | Compact function list (name + kind + signature) |
| `components` | list[FBridgeComponentSummary] | Compact component list (name + class + parent) |
| `interfaces` | list[str] | Interface class names |
| `event_dispatchers` | list[str] | Event dispatcher names |
| `graph_names` | list[str] | All graph names |

### FBridgeVariableSummary fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Variable name |
| `type` | str | Type string |

### FBridgeFunctionSummary fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Function name |
| `kind` | str | "Function", "Event", "Override" |
| `signature` | str | Compact signature: "(Int, Float) -> Bool" |

### FBridgeComponentSummary fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Component variable name |
| `component_class` | str | e.g. "StaticMeshComponent" |
| `parent_name` | str | Parent component name (empty for root) |

---

## Event Dispatchers

### get_event_dispatchers(blueprint_path) -> list[FBridgeEventDispatcherInfo]

Get all event dispatchers with their parameter signatures.

```python
dispatchers = unreal.UnrealBridgeBlueprintLibrary.get_event_dispatchers('/Game/BP/MyBP')
for d in dispatchers:
    params = ', '.join(f'{p.name}: {p.type}' for p in d.params)
    print(f'{d.name}({params})')
```

### FBridgeEventDispatcherInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Dispatcher name |
| `params` | list[FBridgeFunctionParam] | Delegate parameters |

---

## Graph Listing

### get_graph_names(blueprint_path) -> list[FBridgeGraphInfo]

Lightweight list of all graphs. Use before diving into node details.

```python
graphs = unreal.UnrealBridgeBlueprintLibrary.get_graph_names('/Game/BP/MyBP')
for g in graphs:
    print(f'[{g.graph_type}] {g.name}')
```

### FBridgeGraphInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Graph name |
| `graph_type` | str | "EventGraph", "Function", "Macro", "EventDispatcher" |

---

## Graph Analysis

### get_function_call_graph(blueprint_path, function_name) -> list[FBridgeCallEdge]

Return only the outgoing call edges of a function (what it calls). Lightweight — no node details, just the call relationships. Pass empty string for EventGraph.

```python
edges = unreal.UnrealBridgeBlueprintLibrary.get_function_call_graph('/Game/BP/MyBP', 'TakeDamage')
for e in edges:
    print(f'  -> [{e.target_kind}] {e.target_class}::{e.target_name}')
```

### FBridgeCallEdge fields

| Field | Type | Description |
|-------|------|-------------|
| `target_name` | str | Called function/event name |
| `target_class` | str | Target class or object ("KismetMathLibrary", "Self", etc.) |
| `target_kind` | str | "Function", "Event", "Macro" |

### get_function_nodes(blueprint_path, function_name, node_type_filter) -> list[FBridgeNodeInfo]

All nodes in a specific function graph. `function_name=""` = EventGraph. `node_type_filter` optional ("FunctionCall", "VariableGet", "VariableSet", "Branch", "Cast", "Macro", "Event", ...); empty = all nodes.

> ⚠️ **Token cost: HIGH on dense graphs.** No result cap. Complex EventGraphs (Lyra/ALS-style) can have 200–1000+ nodes. **Always pass `node_type_filter`** ("FunctionCall" alone usually shrinks output 3–5×), or prefer `get_function_execution_flow` (exec-only ordered walk) / `get_function_call_graph` (call edges only).

```python
nodes = unreal.UnrealBridgeBlueprintLibrary.get_function_nodes('/Game/BP/MyBP', '', 'FunctionCall')
for n in nodes:
    print(f'[{n.node_type}] {n.title}  target={n.target_class} var={n.variable_name}')
```

### FBridgeNodeInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `title` | str | Node title as shown in editor |
| `node_type` | str | "FunctionCall", "VariableGet", "VariableSet", "Branch", "ForEach", "Cast", "Event", "Macro", "Spawn", "Timeline", "Knot", "Other" |
| `target_class` | str | For function calls: the target class |
| `variable_name` | str | For variable nodes: the variable name |
| `comment` | str | Node comment if any |

---

## Execution Flow

### get_function_execution_flow(blueprint_path, function_name) -> list[FBridgeExecStep]

Walk exec pins to get an ordered execution flow. Much more compact than `get_function_nodes` — only includes nodes on exec wires, with branching info. Pass empty string for EventGraph.

```python
steps = unreal.UnrealBridgeBlueprintLibrary.get_function_execution_flow('/Game/BP/MyBP', '')
for s in steps:
    branches = ', '.join(f'{c.pin_name}->{c.target_step_index}' for c in s.exec_outputs)
    detail = f' [{s.detail}]' if s.detail else ''
    print(f'  {s.step_index}: {s.node_title}{detail}  -> {branches}')
```

### FBridgeExecStep fields

| Field | Type | Description |
|-------|------|-------------|
| `step_index` | int | Position in result array |
| `node_title` | str | Node title as shown in editor |
| `node_type` | str | "FunctionCall", "Branch", "Event", etc. |
| `detail` | str | Context: function name, variable, cast target |
| `exec_outputs` | list[FBridgeExecConnection] | Outgoing exec connections |

### FBridgeExecConnection fields

| Field | Type | Description |
|-------|------|-------------|
| `pin_name` | str | Exec output pin name ("then", "True", "False", etc.) |
| `target_step_index` | int | Target step index (-1 = unconnected) |

---

## Pin Connections

### get_node_pin_connections(blueprint_path, function_name) -> list[FBridgePinConnection]

Get all pin-to-pin connections (exec + data) in a function graph. Node indices match `get_function_nodes(path, func, "")` order. Pass empty string for EventGraph.

> ⚠️ **Token cost: HIGH on dense graphs.** Wire count scales ~2–4× node count; a 300-node graph can emit 1000+ connections. Call only after you've confirmed the graph is small (via `get_function_call_graph` or `get_function_execution_flow`), and only when you actually need pin-level wiring.

```python
connections = unreal.UnrealBridgeBlueprintLibrary.get_node_pin_connections('/Game/BP/MyBP', '')
for c in connections:
    kind = 'EXEC' if c.is_exec else 'DATA'
    print(f'  [{kind}] node[{c.source_node_index}].{c.source_pin_name} -> node[{c.target_node_index}].{c.target_pin_name}')
```

### FBridgePinConnection fields

| Field | Type | Description |
|-------|------|-------------|
| `source_node_index` | int | Source node index |
| `source_pin_name` | str | Source pin name |
| `target_node_index` | int | Target node index |
| `target_pin_name` | str | Target pin name |
| `is_exec` | bool | True = exec wire, False = data wire |

---

## Component Property Values

### get_component_property_values(blueprint_path, component_name) -> list[FBridgePropertyValue]

Get designer-configured (non-default) property values for a specific component. Only returns properties that differ from the component class CDO.

```python
props = unreal.UnrealBridgeBlueprintLibrary.get_component_property_values(
    '/Game/BP/MyActor', 'StaticMesh'
)
for p in props:
    print(f'{p.name} ({p.type}) = {p.value}')
```

### FBridgePropertyValue fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Property name |
| `type` | str | Property type (Float, Vector, etc.) |
| `value` | str | Exported text value |

---

## Node Search

### search_blueprint_nodes(blueprint_path, query, graph_name="") -> list[FBridgeNodeSearchResult]

Search all nodes in a Blueprint by title/type/detail substring. Empty `graph_name` searches all graphs.

> ⚠️ **Token cost: MEDIUM–HIGH for broad queries.** No result cap. A vague query ("Set", "Get") on a large BP can match hundreds of nodes across every graph. Use specific substrings and pass the `node_type_filter` parameter when possible; scope to a single `graph_name` when you already know where to look.

```python
results = unreal.UnrealBridgeBlueprintLibrary.search_blueprint_nodes(
    '/Game/BP/MyBP', 'SetActorLocation'
)
for r in results:
    print(f'[{r.graph_name}] {r.node_title} ({r.node_type}) detail={r.detail}')
```

### FBridgeNodeSearchResult fields

| Field | Type | Description |
|-------|------|-------------|
| `node_title` | str | Node title as shown in editor |
| `node_type` | str | "FunctionCall", "Branch", "Event", etc. |
| `detail` | str | Context (function name, variable, etc.) |
| `graph_name` | str | Which graph contains this node |

---

## Timeline Info

### get_timeline_info(blueprint_path) -> list[FBridgeTimelineInfo]

Get all Timeline components in a Blueprint with their tracks and settings.

```python
timelines = unreal.UnrealBridgeBlueprintLibrary.get_timeline_info('/Game/BP/MyActor')
for tl in timelines:
    print(f'{tl.name} len={tl.length} autoplay={tl.auto_play} loop={tl.loop}')
    for t in tl.tracks:
        print(f'  {t.track_name} ({t.track_type})')
```

### FBridgeTimelineInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Timeline name |
| `length` | float | Timeline length in seconds |
| `auto_play` | bool | Auto-play on begin |
| `loop` | bool | Looping |
| `replicated` | bool | Replicated timeline |
| `tracks` | list[FBridgeTimelineTrack] | Tracks in timeline |

### FBridgeTimelineTrack fields

| Field | Type | Description |
|-------|------|-------------|
| `track_name` | str | Track name |
| `track_type` | str | "Float", "Vector", "LinearColor", "Event" |

---

## Write Operations

### set_blueprint_variable_default(blueprint_path, variable_name, new_value) -> bool

Set the default value of a Blueprint variable. Value is parsed as text (same format as Details panel).

```python
ok = unreal.UnrealBridgeBlueprintLibrary.set_blueprint_variable_default(
    '/Game/BP/MyBP', 'Health', '100.0'
)
```

### set_component_property(blueprint_path, component_name, property_name, new_value) -> bool

Set a property on a Blueprint component by name. Value is parsed as text.

```python
ok = unreal.UnrealBridgeBlueprintLibrary.set_component_property(
    '/Game/BP/MyActor', 'StaticMesh', 'RelativeScale3D', '(X=2.0,Y=2.0,Z=2.0)'
)
```

### add_blueprint_variable(blueprint_path, variable_name, type_string, default_value="") -> bool

Add a new variable to a Blueprint. Type string supports: bool, byte, int, int64, float, double, Name, String, Text, Vector, Rotator, Transform, Color, LinearColor, Object, Class, SoftObject, SoftClass, and struct names (e.g. "MyStruct").

```python
ok = unreal.UnrealBridgeBlueprintLibrary.add_blueprint_variable(
    '/Game/BP/MyBP', 'Score', 'int', '0'
)
ok = unreal.UnrealBridgeBlueprintLibrary.add_blueprint_variable(
    '/Game/BP/MyBP', 'SpawnLocation', 'Vector', '(X=0,Y=0,Z=100)'
)
```

### remove_blueprint_variable(blueprint_path, variable_name) -> bool

Remove a member variable by name and recompile. Returns `False` if the variable doesn't exist.

```python
unreal.UnrealBridgeBlueprintLibrary.remove_blueprint_variable('/Game/BP/MyBP', 'Score')
```

### rename_blueprint_variable(blueprint_path, old_name, new_name) -> bool

Rename a member variable and recompile. Returns `False` if the old name is missing, the new name already exists, or `new_name` is empty.

```python
unreal.UnrealBridgeBlueprintLibrary.rename_blueprint_variable('/Game/BP/MyBP', 'Health', 'HP')
```

### add_blueprint_interface(blueprint_path, interface_path) -> bool

Add an interface implementation to a Blueprint and recompile. `interface_path` can be:

- a native interface class path: `/Script/GameplayAbilities.AbilitySystemInterface`
- a Blueprint interface asset path: `/Game/Interfaces/BPI_Interactable`
- a Blueprint interface class path: `/Game/Interfaces/BPI_Interactable.BPI_Interactable_C`

Returns `False` if the path doesn't resolve to a `UINTERFACE` (`CLASS_Interface` flag).

```python
unreal.UnrealBridgeBlueprintLibrary.add_blueprint_interface(
    '/Game/BP/MyBP',
    '/Script/GameplayAbilities.AbilitySystemInterface',
)
```

### remove_blueprint_interface(blueprint_path, interface_name_or_path) -> bool

Remove an interface implementation. Accepts a full path or the short class name (e.g. `AbilitySystemInterface`, `BPI_Interactable_C`). Returns `False` if the interface isn't implemented on the BP.

```python
unreal.UnrealBridgeBlueprintLibrary.remove_blueprint_interface('/Game/BP/MyBP', 'BPI_Interactable_C')
```

### add_blueprint_component(blueprint_path, component_class_path, component_name, parent_component_name="") -> bool

Add a new component node to an Actor Blueprint's `SimpleConstructionScript` and recompile.

- `component_class_path`: native class path (`/Script/Engine.StaticMeshComponent`) or Blueprint component class (`/Game/BP/BP_MyComp.BP_MyComp_C`).
- `component_name`: new variable name for the node (must be unique within the BP).
- `parent_component_name`: attach under this existing component. Empty string = attach under the current root, or become the root if no root exists yet.

Returns `False` if the class isn't a `UActorComponent` subclass, the name collides, or SCS creation fails.

```python
unreal.UnrealBridgeBlueprintLibrary.add_blueprint_component(
    '/Game/BP/MyBP',
    '/Script/Engine.StaticMeshComponent',
    'BodyMesh',
    '',          # attach under root
)

unreal.UnrealBridgeBlueprintLibrary.add_blueprint_component(
    '/Game/BP/MyBP',
    '/Script/Engine.PointLightComponent',
    'HeadLight',
    'BodyMesh',  # attach under BodyMesh
)
```

---

## Graph Node Write Ops

Low-level graph manipulation. **Token budget matters here** — these calls return only a GUID (create) or bool (connect/remove/move) so a multi-node build doesn't flood context. Do not re-read the whole graph after each op; track GUIDs in your script and only call `get_function_nodes` when you actually need to inspect.

**Layout convention**: caller owns positions. Default grid of **300px column × 150px row** keeps a graph readable — build exec flow left-to-right, keep data inputs above/below the consuming node. Always pass deliberate `(x, y)` so the graph opens tidy; overlapping nodes are the #1 reason a graph becomes unreadable.

**Compile**: none of these auto-compile. Run one `compile_blueprints([path])` after a batch of edits, not per-op.

### add_call_function_node(blueprint_path, graph_name, target_class_path, function_name, x, y) -> str

Create a Call-Function node. Pass `""` for `target_class_path` to call on self. Returns the node GUID (32-hex-char string) or `""` on failure.

```python
guid = unreal.UnrealBridgeBlueprintLibrary.add_call_function_node(
    '/Game/BP/BP_Hero', 'EventGraph',
    '/Script/Engine.KismetSystemLibrary', 'PrintString',
    400, 0)
```

### add_variable_node(blueprint_path, graph_name, variable_name, is_set, x, y) -> str

Create a Variable Get (`is_set=False`) or Set (`is_set=True`) node for a self-member variable (declared on the BP or inherited). Returns node GUID or `""` on failure.

```python
get_guid = lib.add_variable_node('/Game/BP/BP_Hero', 'EventGraph', 'Health', False, 0, 200)
set_guid = lib.add_variable_node('/Game/BP/BP_Hero', 'EventGraph', 'Health', True,  400, 200)
```

### connect_graph_pins(blueprint_path, graph_name, src_node_guid, src_pin_name, dst_node_guid, dst_pin_name) -> bool

Connect pins through the K2 schema. Handles type coercion (e.g. int → string auto-inserts a conversion where supported). Returns `False` when nodes/pins are missing or types are incompatible.

Use `get_node_pin_connections` or `get_function_nodes` to look up pin names when unsure.

```python
lib.connect_graph_pins('/Game/BP/BP_Hero', 'EventGraph',
    get_guid, 'Health',       # source: VarGet.Health output
    set_guid, 'Health')       # target: VarSet.Health input
```

### remove_graph_node(blueprint_path, graph_name, node_guid) -> bool

Remove a node by GUID; breaks all existing pin links first.

### set_graph_node_position(blueprint_path, graph_name, node_guid, x, y) -> bool

Reposition a node. Cheap layout cleanup after a batch of creations.

### add_event_node(blueprint_path, graph_name, parent_class_path, event_name, x, y) -> str

Add a `K2Node_Event` that overrides a parent-class `BlueprintImplementableEvent` / `BlueprintNativeEvent` (e.g. `ReceiveTick`, `ReceiveBeginPlay` on `AActor`). Pass `""` for `parent_class_path` to use the BP's own `ParentClass`.

Idempotent: if a matching event (including the default "ghost" `ReceiveBeginPlay` / `ReceiveTick` placeholder) is already on the graph, its existing GUID is returned and the node is re-enabled + repositioned instead of duplicated. Returns `""` if the parent class or function can't be resolved.

```python
tick_guid = lib.add_event_node('/Game/BP/BP_Hero', 'EventGraph',
    '', 'ReceiveTick', 0, 0)                 # uses BP.ParentClass (AActor)
bp_guid   = lib.add_event_node('/Game/BP/BP_Hero', 'EventGraph',
    '/Script/Engine.Actor', 'ReceiveBeginPlay', 0, 200)
```

### set_pin_default_value(blueprint_path, graph_name, node_guid, pin_name, new_default_value) -> bool

Set a pin's literal default via the K2 schema — the same text form the Details panel accepts: `"1.0"`, `"true"`, `"(X=1,Y=0,Z=0)"`, `"Hello"`, etc. Only affects unconnected pins (a connected pin ignores its default). Returns `False` if the node/pin is missing; the schema silently accepts malformed text, so verify with `get_node_pin_connections` if in doubt.

```python
print_g = lib.add_call_function_node(bp, g,
    '/Script/Engine.KismetSystemLibrary', 'PrintString', 900, 0)
lib.set_pin_default_value(bp, g, print_g, 'InString',  'Hello from bridge')
lib.set_pin_default_value(bp, g, print_g, 'Duration',  '5.0')
lib.set_pin_default_value(bp, g, print_g, 'TextColor', '(R=1,G=0,B=0,A=1)')
```

## P0 — Control-flow nodes

Essential graph primitives beyond Call-Function / Variable Get-Set. All take `(x, y)` and return a node GUID string; empty string on failure. None auto-compile.

### add_branch_node(blueprint_path, graph_name, x, y) -> str

Add `K2Node_IfThenElse` (Branch). Pin names: `Condition` (in), `then` (exec out true), `else` (exec out false).

```python
br = lib.add_branch_node(bp, g, 400, 0)
lib.connect_graph_pins(bp, g, some_bool_g, 'ReturnValue', br, 'Condition')
```

### add_sequence_node(blueprint_path, graph_name, pin_count, x, y) -> str

`K2Node_ExecutionSequence`. `pin_count` clamped to `[2, 16]`. Pins `Then 0`, `Then 1`, … are added in order.

### add_cast_node(blueprint_path, graph_name, target_class_path, pure, x, y) -> str

`K2Node_DynamicCast`. `pure=True` → no exec pins (implicit cast). Pins: `Object` (in), `As<Class>` (out), + exec pair when impure. `target_class_path` accepts native (`/Script/Engine.Pawn`) or BP (`/Game/Foo/BP_X`) paths.

### add_self_node(blueprint_path, graph_name, x, y) -> str

`K2Node_Self` — the `self` reference. Pin: `self` (out).

### add_custom_event_node(blueprint_path, graph_name, event_name, x, y) -> str

Add a `K2Node_CustomEvent`. `event_name` is auto-uniquified against the BP (adds `_0`, `_1`, …) — check `get_function_nodes` if you need the final name. Pins: `OutputDelegate` (delegate out, used for Bind-to-Event patterns), `then` (exec out).

## P0 — Function / event graph management

Graph-level ops. All compile implicitly where needed.

### create_function_graph(blueprint_path, function_name) -> bool

Create an empty user-defined function graph with default entry + result nodes. Returns `False` if the name is taken.

### remove_function_graph(blueprint_path, function_name) -> bool

Remove a user function. Recompiles.

### rename_function_graph(blueprint_path, old_name, new_name) -> bool

Rename. Updates references across the BP.

### add_function_parameter(blueprint_path, function_name, param_name, type_string, is_return) -> bool

Add an input (`is_return=False`) or output/return pin (`is_return=True`). `type_string` syntax matches `add_blueprint_variable`: `"Int"`, `"Float"`, `"Vector"`, `"Array of Int"`, `"BP_Foo"`, etc. When `is_return=True` and the function has no result node yet, one is auto-created.

```python
lib.create_function_graph(bp, 'Fn_Add')
lib.add_function_parameter(bp, 'Fn_Add', 'A',   'Int', False)
lib.add_function_parameter(bp, 'Fn_Add', 'B',   'Int', False)
lib.add_function_parameter(bp, 'Fn_Add', 'Sum', 'Int', True)
```

### set_function_metadata(blueprint_path, function_name, pure, const, category, access_specifier) -> bool

Toggle `BlueprintPure` / `Const`, set Category, and access (`"public"` / `"protected"` / `"private"`; empty string = leave unchanged). Empty `category` also leaves unchanged.

## P0 — Event Dispatcher write ops

Event dispatchers = multicast delegates exposed on a BP. Creating one generates both a member variable (of `MulticastDelegate` type) and a signature graph.

### add_event_dispatcher(blueprint_path, dispatcher_name) -> bool

Create a new no-parameter event dispatcher. Recompiles. Returns `False` if the name collides with an existing variable or dispatcher.

**Adding parameters**: after creation, open the signature graph via standard MyBlueprint UI, or use `add_function_parameter(bp, dispatcher_name, ...)` — the signature graph is indexed by the same name. (Params are not wired through this helper yet; manual edit is fine.)

### remove_event_dispatcher(blueprint_path, dispatcher_name) -> bool

Remove variable + signature graph.

### rename_event_dispatcher(blueprint_path, old_name, new_name) -> bool

Renames both the variable and the signature graph in sync.

### add_dispatcher_call_node(blueprint_path, graph_name, dispatcher_name, x, y) -> str

Add a **Call** (Broadcast) node. Pins: `execute` (in), `then` (out), plus any dispatcher params as inputs.

### add_dispatcher_bind_node(blueprint_path, graph_name, dispatcher_name, unbind, x, y) -> str

Add a **Bind** (`unbind=False`) or **Unbind** (`unbind=True`) node. Pins: `execute` (in), `then` (out), `Event` (delegate in) — wire this to a `CustomEvent`'s `OutputDelegate` to register a callback.

```python
ce   = lib.add_custom_event_node(bp, g, 'OnHealthChangedHandler', 0, 400)
bind = lib.add_dispatcher_bind_node(bp, g, 'OnHealthChanged', False, 400, 400)
lib.connect_graph_pins(bp, g, ce, 'OutputDelegate', bind, 'Event')
```

## P0 — Interface override

### implement_interface_function(blueprint_path, interface_path, function_name) -> bool

Materialize an interface function as an editable graph on this BP. Idempotent: returns `True` without duplicating if the function is already implemented.

**Rules**:
- Interface must already be added via `add_blueprint_interface`.
- **Event-type interface members** (no return / no out params, `BlueprintImplementableEvent`-style) return `False` — for those use `add_event_node` on the EventGraph instead.
- **Function-type members** get a new function graph with the interface's signature (auto-created by `add_blueprint_interface` already; this helper is the explicit path for cases where it wasn't).

### add_interface_message_node(blueprint_path, graph_name, interface_path, function_name, x, y) -> str

Add a `K2Node_Message` — the "Call Function (Message)" variant that dispatches against any object that may-or-may-not implement the interface. Pins include `Target` (object ref input) plus the interface function's params.

## P0 — Variable metadata / type

### set_variable_metadata(blueprint_path, variable_name, instance_editable, blueprint_read_only, expose_on_spawn, private, category, tooltip, replication_mode) -> bool

Bulk-set editor-panel flags. Pass empty string for `category`/`tooltip`/`replication_mode` to leave unchanged.

- `replication_mode`: `""` (leave) | `"None"` | `"Replicated"` | `"RepNotify"` (RepNotify function name must be wired separately — not covered here)
- `private` toggles the `BlueprintPrivate` metadata key

```python
lib.set_variable_metadata(bp, 'Health',
    instance_editable=True, blueprint_read_only=False,
    expose_on_spawn=True, private=False,
    category='Stats', tooltip='Current health', replication_mode='Replicated')
```

### set_variable_type(blueprint_path, variable_name, new_type_string) -> bool

Change the type of an existing member variable in-place. Accepts container syntax — `"Array of Int"`, `"Array of Vector"`, etc. Triggers a recompile. Nodes referencing the old type may error in the compile log — inspect with `get_compile_errors` after.

## P0 — Compile feedback

### get_compile_errors(blueprint_path) -> list[FBridgeCompileMessage]

Compile the BP and return all compiler messages (errors + warnings + notes + info). `FBridgeCompileMessage` fields:

- `severity: str` — `"Error"` | `"Warning"` | `"Note"` | `"Info"`
- `message: str` — flattened human-readable message
- `node_guid: str` — GUID of the first referenced graph node, or `""`

**Cost: medium-to-high** — a clean compile returns ~1 Info message, but a broken BP can easily return 20–100 messages with long descriptive text. Filter client-side by severity before printing if you only care about errors:

```python
msgs = lib.get_compile_errors(bp)
errs = [m for m in msgs if m.severity == 'Error']
for e in errs[:10]:
    print(f'{e.message[:200]}  node={e.node_guid}')
```

---

## P1 — Loops, Select, Literal

Macro-based nodes live in `/Engine/EditorBlueprintResources/StandardMacros`. These helpers spawn `K2Node_MacroInstance` pointed at the right macro graph.

### add_foreach_node(blueprint_path, graph_name, with_break, x, y) -> str
### add_for_loop_node(blueprint_path, graph_name, with_break, x, y) -> str
### add_while_loop_node(blueprint_path, graph_name, x, y) -> str

Loops. `with_break=True` yields the `WithBreak` variant with an extra Break exec pin.

```python
lib.add_foreach_node(bp, 'EventGraph', False, 800, 400)   # ForEachLoop
lib.add_for_loop_node(bp, 'EventGraph', True, 800, 600)   # ForLoopWithBreak
lib.add_while_loop_node(bp, 'EventGraph', 800, 800)
```

### add_select_node(blueprint_path, graph_name, x, y) -> str

Wildcard `K2Node_Select`. Pin type / option count auto-resolve once you wire the Index pin (Bool / Byte / Enum / Int).

### add_make_literal_node(blueprint_path, graph_name, type_string, value, x, y) -> str

Wraps `UKismetSystemLibrary::MakeLiteral<T>`. `type_string` is one of: `Int`, `Int64`, `Float` (→Double), `Double`, `Bool`, `Byte`, `Name`, `String`, `Text`. `value` is assigned to the `Value` pin default (pass empty string to leave default).

```python
lib.add_make_literal_node(bp, 'EventGraph', 'Int',    '42',    1000, 200)
lib.add_make_literal_node(bp, 'EventGraph', 'String', 'hello', 1000, 300)
```

## P1 — Layout ops

### align_nodes(blueprint_path, graph_name, guids, axis) -> bool

Align or distribute 2+ selected nodes. `axis` ∈ `Left`, `Right`, `Top`, `Bottom`, `CenterHorizontal`, `CenterVertical`, `DistributeHorizontal`, `DistributeVertical`. Matches the editor's Q/W/A/S/align-distribute shortcuts.

### add_comment_box(blueprint_path, graph_name, guids, text, x, y, width, height) -> str

Create a comment frame. If `guids` is non-empty, the frame auto-sizes to fit those nodes (x/y/width/height ignored); otherwise places at (x,y) with size (width,height) (defaults 400×200 if zero). Returns the comment's GUID.

### add_reroute_node(blueprint_path, graph_name, x, y) -> str

Insert a `K2Node_Knot` pass-through. Wire pins yourself with `connect_graph_pins` — Knot input pin is `0`, output pin is `1`.

### set_node_enabled(blueprint_path, graph_name, node_guid, state) -> bool

`state` ∈ `Enabled`, `Disabled`, `DevelopmentOnly`.

## P1 — Class settings

### reparent_blueprint(blueprint_path, new_parent_path) -> bool

Change the BP's parent class and recompile. **HIGH RISK** — reparenting outside the current hierarchy can drop components, variables, and function overrides that don't exist on the new parent. Returns `True` if already parented to the same class (noop) or after a successful reparent.

### set_blueprint_metadata(blueprint_path, display_name, description, category, namespace) -> bool

Set class-level metadata. Pass an empty string to leave a field unchanged.

```python
lib.set_blueprint_metadata(bp, 'My Hero', 'Player pawn', 'Bridge|Demo', '')
```

## P1 — Component tree

### reparent_component(blueprint_path, component_name, new_parent_name) -> bool

Detach the SCS node and reattach under `new_parent_name` (empty string → promote to scene root).

### reorder_component(blueprint_path, component_name, new_index) -> bool

Reorder within the current parent's child list (or the root list if the component is a root node). Index is clamped to valid range.

### remove_component(blueprint_path, component_name) -> bool

Delete an SCS node. Children are removed too — use `reparent_component` first if you want to keep them.

## P1 — Dispatcher event node

### add_dispatcher_event_node(blueprint_path, graph_name, dispatcher_name, x, y) -> str

Create a `K2Node_CustomEvent` whose signature (output pins) matches the dispatcher's delegate signature. Typically wired via an `add_dispatcher_bind_node(..., unbind=False)` + a `Self` → Target pin — the CustomEvent's Delegate output pin connects to the Bind node's Event pin.

```python
lib.add_event_dispatcher(bp, 'OnFired')
evt  = lib.add_dispatcher_event_node(bp, 'EventGraph', 'OnFired', 1400, 400)
bind = lib.add_dispatcher_bind_node(bp, 'EventGraph', 'OnFired', False, 1700, 400)
# wire:  evt.Delegate → bind.Event ;  Self node → bind.Target
```

---

### End-to-end example — one node-graph build, one compile

```python
lib = unreal.UnrealBridgeBlueprintLibrary
bp = '/Game/BP/BP_Hero'
g  = 'EventGraph'

get_g = lib.add_variable_node(bp, g, 'Health', False,   0, 200)
set_g = lib.add_variable_node(bp, g, 'Health', True,  600, 200)
print_g = lib.add_call_function_node(bp, g,
    '/Script/Engine.KismetSystemLibrary', 'PrintString',  900, 0)

lib.connect_graph_pins(bp, g, get_g, 'Health', set_g, 'Health')
lib.connect_graph_pins(bp, g, get_g, 'Health', print_g, 'InString')

unreal.UnrealBridgeEditorLibrary.compile_blueprints([bp])
```

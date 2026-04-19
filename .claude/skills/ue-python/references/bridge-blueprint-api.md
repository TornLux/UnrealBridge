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

---

## Function-scope local variables

Local variables live on a function graph's `UK2Node_FunctionEntry`. They're visible **inside the function body** but not as class members, so the class-level `get_blueprint_variables` won't surface them. These helpers operate on Function graphs only — `EventGraph`, macro graphs, and construction-script-style graphs reject the call.

### get_function_local_variables(blueprint_path, function_name) -> list[FBridgeVariableInfo]

List local variables on a function. Returns the same `FBridgeVariableInfo` fields as `get_blueprint_variables` — name, type, category, default_value, description, instance_editable, blueprint_read_only. `replication_condition` is always `"None"` (local vars can't be replicated).

```python
L = unreal.UnrealBridgeBlueprintLibrary
for v in L.get_function_local_variables('/Game/BP/MyBP', 'Attack'):
    print(f'{v.name:12s} {v.type:20s} = {v.default_value}')
# HitCount      Int                    = 99
# Velocity      Vector                 = (X=1.0,Y=2.0,Z=3.0)
# Tag           String                 = Hello
```

### add_function_local_variable(blueprint_path, function_name, variable_name, type_string, default_value="") -> bool

Add a local variable. Type grammar identical to `add_blueprint_variable` (`"Bool"`, `"Int"`, `"Float"`, `"Vector"`, class paths, `"Array of X"` prefix). Compiles on success so subsequent node spawns resolve the variable.

```python
L.add_function_local_variable('/Game/BP/MyBP', 'Attack', 'HitCount', 'Int', '0')
L.add_function_local_variable('/Game/BP/MyBP', 'Attack', 'Targets', 'Array of Actor', '')
```

Returns `False` if the function graph is missing, the name is taken, or the type string can't be parsed.

### remove_function_local_variable(blueprint_path, function_name, variable_name) -> bool

Remove a local variable. Variable-get/set nodes that still reference it become dangling after compile — delete or repoint them first if you care about a clean graph.

```python
L.remove_function_local_variable('/Game/BP/MyBP', 'Attack', 'HitCount')
```

### rename_function_local_variable(blueprint_path, function_name, old_name, new_name) -> bool

Rename a local variable. Variable-get/set nodes inside the function are updated to the new name. Returns `False` if `old_name` is missing, `new_name` is already taken, or the names are equal.

```python
L.rename_function_local_variable('/Game/BP/MyBP', 'Attack', 'Counter', 'HitCount')
```

### set_function_local_variable_default(blueprint_path, function_name, variable_name, value) -> bool

Set the default value. Uses the same export-text format as `set_blueprint_variable_default` (e.g. `"true"`, `"3.14"`, `"(X=1,Y=2,Z=3)"`, `'"some string"'` for text).

```python
L.set_function_local_variable_default('/Game/BP/MyBP', 'Attack', 'HitCount', '99')
```

### add_function_local_variable_node(blueprint_path, function_name, variable_name, is_set, node_pos_x, node_pos_y) -> str

Spawn a variable-get (`is_set=False`) or variable-set (`is_set=True`) node for a local variable **or a function parameter**. This is separate from `add_variable_node` because the underlying `VariableReference` must carry the **function scope**, not the self-member scope — `add_variable_node` can't spawn local-variable nodes correctly.

Resolution order: first looks in `FunctionEntry.LocalVariables` (true locals), then falls back to `FunctionEntry.UserDefinedPins` (function parameters). For parameters the variable reference is name-only (no FGuid in `FUserPinInfo` to pass), and UE's compile-time lookup resolves by name against the generated UFunction's parameter properties — same as the editor's drag-from-MyBlueprint path.

```python
# Local variable
get_hit  = L.add_function_local_variable_node(bp, 'Attack', 'HitCount',      False, 0, 0)
set_hit  = L.add_function_local_variable_node(bp, 'Attack', 'HitCount',      True,  400, 0)

# Function parameter (no redundant SET-into-local-mirror needed)
get_dmg  = L.add_function_local_variable_node(bp, 'Attack', 'IncomingDamage', False, 0, 100)
```

**Avoid the "fanout into local mirrors" antipattern.** A parameter used in N places does not need a matching `ParamL` local + `SET ParamL = Param` chain at the function head — that's authoring overhead the editor doesn't require. Drop Gets for the parameter directly near each consumer. Intermediate locals are still worth it for **reused computed values** (e.g. `DeltaL`, `DistanceL`, `FinalScoreL`) — those break deep pure chains into short hops and reduce `LayerDataBudget` in the layout.

Returns `""` if the name doesn't match any local variable or parameter, or if the graph isn't a function graph.

---

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

Remove a user function. Recompiles. The bridge automatically closes any
open Blueprint-editor tab for the graph before removal, so Slate widget
references are clean.

**⚠ Residual GC crash risk on heavy graphs.** Even after tab close, the
`UK2Node_FunctionEntry` node held type references via `LocalVariables`
and `UserDefinedPins`. Rebuilding a 100+ node function by calling
`remove_function_graph` → `create_function_graph` → re-spawn right
after can crash during the next GC pass at
`UK2Node_FunctionEntry::AddReferencedObjects` (access violation at a
dangling `UStruct*`, typically 0x70 offset). Reproduced on the
`ScoreTraversalCandidates` showcase rebuild after a prior
`auto_insert_reroutes` added 43 knots — total ~130 nodes to tear down.

**Prefer in-place edits over full rebuilds** when the graph has many
parameters / locals. If you only need to remove specific nodes (e.g.
reroute knots added by `auto_insert_reroutes`), use `remove_graph_node`
per-guid and splice wires with `connect_graph_pins`. Resolve knot
upstream/downstream via `get_node_pin_connections` + `get_function_nodes`
filter='Knot' (ordering matches `get_rendered_node_info` 1:1, so zip them
to get idx→guid).

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

Add a **Bind** (`unbind=False`) or **Unbind** (`unbind=True`) node. Pins: `execute` (in), `then` (out), `Delegate` (delegate in) — wire this to a `CustomEvent`'s `OutputDelegate` to register a callback.

```python
ce   = lib.add_custom_event_node(bp, g, 'OnHealthChangedHandler', 0, 400)
bind = lib.add_dispatcher_bind_node(bp, g, 'OnHealthChanged', False, 400, 400)
lib.connect_graph_pins(bp, g, ce, 'OutputDelegate', bind, 'Delegate')
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

### wrap_nodes_in_comment_box(blueprint_path, graph_name, node_guids, text) -> str

Declarative sibling of `add_comment_box`: always sizes to fit the given
nodes, no manual (x, y, width, height) fallback. `node_guids` must be
non-empty; returns `""` if none of the guids resolve. Prefer this when
the intent is "box around these nodes"; use `add_comment_box` when you
want a free-floating section header with manual placement.

```python
guids = [hdr_set_guid, cond_branch_guid, print_guid]
box = L.wrap_nodes_in_comment_box(bp, 'MyFunction', guids, '1. Validate inputs')
L.set_comment_box_color(bp, 'MyFunction', box, 'Validation')
```

### update_comment_box(blueprint_path, graph_name, comment_guid, node_guids, text) -> bool

Update an existing comment box in place. Both `node_guids` and `text`
are "optional":

- Empty `node_guids` → leave position/size alone.
- Empty `text`       → leave comment string alone.
- Non-empty both     → reshape AND rename in one call.

Returns `True` when the comment was found and at least one field was
changed. Useful when the set of nodes under a section grew/shrunk
after a refactor, or when you want to rename a section without
re-creating the box (preserves GUID, color, bubble-pinned state).

```python
# Section now covers two more nodes; tighten the frame.
L.update_comment_box(bp, g, section_guid, [*originals, new_a, new_b], '')
# Just rename; don't touch the frame.
L.update_comment_box(bp, g, section_guid, [], '2. Apply damage (v2)')
```

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
lib.connect_graph_pins(bp, 'EventGraph', evt, 'OutputDelegate', bind, 'Delegate')
```

---

## P2 — CallFunction convenience wrappers

### add_delay_node(blueprint_path, graph_name, duration_seconds, x, y) -> str

Insert a latent `Delay` node (`UKismetSystemLibrary::Delay`) and pre-fill the `Duration` pin. Returns the new node GUID.

```python
lib.add_delay_node(bp, 'EventGraph', 0.25, 600, 0)
```

### add_set_timer_by_function_name_node(blueprint_path, graph_name, function_name, time_seconds, looping, x, y) -> str

Insert a "Set Timer by Function Name" node (`UKismetSystemLibrary::K2_SetTimer`) with `FunctionName` / `Time` / `bLooping` pin defaults pre-filled.

```python
lib.add_set_timer_by_function_name_node(bp, 'EventGraph', 'TickHandler', 0.5, True, 800, 0)
```

### add_spawn_actor_from_class_node(blueprint_path, graph_name, actor_class_path, x, y) -> str

Insert a `K2Node_SpawnActorFromClass` node and default its `Class` pin to the resolved actor class. The class must be `AActor`-derived. After the class is set the node regenerates its exposed-spawn pins automatically.

```python
lib.add_spawn_actor_from_class_node(bp, 'EventGraph',
    '/Script/Engine.StaticMeshActor', 1000, 0)
```

---

## P2 — Struct Make / Break

### add_make_struct_node(blueprint_path, graph_name, struct_path, x, y) -> str

Insert a `K2Node_MakeStruct` for a `UScriptStruct`. Pass an asset path (`/Game/Foo/MyStruct`) for a Blueprint struct or a script path (`/Script/Engine.MaterialParameterInfo`) for a native one. Native structs that have a dedicated "Make X" function (e.g. `Vector`, `Rotator`, `Transform`) are still supported via the internal-use path; for those, prefer `add_call_function_node` against `MakeVector` / `MakeRotator` / `MakeTransform` for the standard editor look.

```python
lib.add_make_struct_node(bp, 'EventGraph',
    '/Script/Engine.MaterialParameterInfo', 0, 200)
```

### add_break_struct_node(blueprint_path, graph_name, struct_path, x, y) -> str

Insert a `K2Node_BreakStruct`. Same struct-path semantics as `add_make_struct_node`. Works with native-make structs (Vector etc.) since BreakStruct does not have the same restriction.

```python
lib.add_break_struct_node(bp, 'EventGraph', '/Script/CoreUObject.Vector', 400, 200)
```

---

## P2 — Graph extras

### create_macro_graph(blueprint_path, macro_name) -> bool

Create a new user-defined macro graph on the Blueprint. Returns `False` if a macro with that name already exists.

```python
lib.create_macro_graph(bp, 'IsTargetVisible')
```

### remove_macro_graph(blueprint_path, macro_name) -> bool

Remove a user-defined macro graph by name. Returns `False` if no macro with that name exists. Triggers a Blueprint recompile (via `EGraphRemoveFlags::Recompile`), so pair with an intentional batch rather than calling in a tight loop.

```python
lib.remove_macro_graph(bp, 'IsTargetVisible')
```

### add_breakpoint(blueprint_path, graph_name, node_guid, enabled) -> bool

Create or toggle a debug breakpoint on a node. If a breakpoint already exists the call is idempotent and just updates the enabled state. Useful for guiding the user when debugging a generated graph.

```python
node = lib.add_branch_node(bp, 'EventGraph', 0, 0)
lib.add_breakpoint(bp, 'EventGraph', node, True)
```

### remove_breakpoint(blueprint_path, graph_name, node_guid) -> bool

Remove a breakpoint previously set on a node. Returns `True` only if a breakpoint was actually found and removed; `False` is a no-op (not an error).

```python
lib.remove_breakpoint(bp, 'EventGraph', node)
```

### clear_all_breakpoints(blueprint_path) -> int

Remove every breakpoint on the Blueprint. Returns the count that were cleared. Use before generating a fresh debug layout or when bulk-cleaning after a scripted debug session.

```python
n = lib.clear_all_breakpoints(bp)
print(f'cleared {n} breakpoints')
```

### get_breakpoints(blueprint_path) -> list[FBridgeBreakpointInfo]

Enumerate all breakpoints on the Blueprint. Token footprint: **LOW** — four fields per breakpoint (graph name, node GUID, node title, enabled). Typical debug sessions have <20 breakpoints.

```python
for bp_info in lib.get_breakpoints(bp):
    flag = 'ON ' if bp_info.enabled else 'off'
    print(f'{flag} {bp_info.graph_name}/{bp_info.node_title} ({bp_info.node_guid})')
```

#### FBridgeBreakpointInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `graph_name` | str | Name of the graph the breakpoint's node lives in (`EventGraph`, or a function/macro name). Empty if the node was deleted but the breakpoint record lingers. |
| `node_guid` | str | NodeGuid in digits format, matches what `add_branch_node`/etc. return. |
| `node_title` | str | User-facing node title (best effort; can be empty for detached nodes). |
| `enabled` | bool | `True` if enabled by the user. Transient single-step state is filtered out. |

---

## P2 — Timeline

### add_timeline_node(blueprint_path, graph_name, timeline_template_name, x, y) -> str

Insert a `K2Node_Timeline`. If `timeline_template_name` is empty, a unique name is auto-generated. If a `UTimelineTemplate` with that name already exists on the Blueprint it is reused; otherwise a new template is created via `FBlueprintEditorUtils::AddNewTimeline`. Cost: medium-to-high — adds a new variable to the BP and triggers structural change.

The returned node has no tracks yet. The current bridge has no track-edit API; populate tracks via the Timeline editor in UE, or use the Python `unreal.TimelineTemplate` reflection if you need tracks set programmatically.

```python
tl = lib.add_timeline_node(bp, 'EventGraph', 'Fade', 200, 600)
```

### set_timeline_properties(blueprint_path, timeline_name, length, auto_play, loop, replicated, ignore_time_dilation) -> bool

Update a timeline's template-level settings. Pass `length = -1.0` to leave the existing length unchanged; bool flags are always applied. The call syncs the change into the owning `K2Node_Timeline` graph node (if any) so the in-graph display matches. Returns `False` only when no timeline template with that name is found.

```python
lib.set_timeline_properties(bp, 'Fade',
    length=2.5, auto_play=True, loop=True,
    replicated=False, ignore_time_dilation=False)
```

---

## P2 — Node utilities

Low-cost (~1 call each, small response). None auto-compile — call `compile_blueprints([bp])` after your batch.

### set_node_comment(blueprint_path, graph_name, node_guid, comment, comment_bubble_visible) -> bool

Set the inline "Node Comment" text shown above a graph node. Pass empty `comment` to clear. When `comment_bubble_visible` is `True` the bubble is also pinned open; when `False` the bubble auto-collapses regardless of content. Returns `False` only when the node is missing.

```python
lib.set_node_comment(bp, 'EventGraph', guid, 'TODO: rework', True)
```

### duplicate_graph_node(blueprint_path, graph_name, node_guid, x, y) -> str

Clone an existing node in the same graph at `(x, y)`. The duplicate gets a fresh GUID; all pin links are broken on the copy (rewire via `connect_graph_pins`). Useful for templating boilerplate patterns (e.g. repeated PrintString or math nodes). Returns new GUID, or `""` on failure.

```python
clone = lib.duplicate_graph_node(bp, g, call_guid, src_x + 320, src_y)
```

### disconnect_graph_pin(blueprint_path, graph_name, node_guid, pin_name) -> bool

Break every link on a single named pin. Returns `True` if the pin was found *and* had at least one link broken; `False` if the pin is missing, the node is missing, or the pin was already unlinked. Cheaper and more surgical than removing/re-adding the node when you only need to clear one connection.

```python
lib.disconnect_graph_pin(bp, g, branch_guid, 'execute')
```

### add_make_array_node(blueprint_path, graph_name, x, y) -> str

Insert a `Make Array` node. Element type is wildcard until the first pin is connected (UE infers the type from the connection). Use `connect_graph_pins` with pin names `"[0]"`, `"[1]"`, ... for inputs and `"Array"` for the output. Returns node GUID.

```python
arr = lib.add_make_array_node(bp, g, 400, 200)
lib.connect_graph_pins(bp, g, get_a, 'A', arr, '[0]')
lib.connect_graph_pins(bp, g, get_b, 'B', arr, '[1]')
```

### add_enum_literal_node(blueprint_path, graph_name, enum_path, value_name, x, y) -> str

Insert a `Literal <Enum>` node for the given `UEnum`. `enum_path` is a native class path (`/Script/Engine.EComponentMobility`) or user-defined enum asset path. `value_name` is the short entry name (e.g. `"Static"`); pass `""` to use the first entry. Returns `""` if the enum can't be loaded or the value isn't one of its entries.

```python
lit = lib.add_enum_literal_node(bp, g,
    '/Script/Engine.EComponentMobility', 'Movable', 200, 300)
```

---

## Semantic summary (understanding layer)

Agents reading an unfamiliar BP typically end up chaining 5–6 of the
granular queries above (overview + variables + functions + components +
interfaces + dispatchers) and stitching the results together. The three
calls below pre-digest that work into LLM-friendly output so "what does
this BP do?" becomes one round-trip.

### get_blueprint_summary(blueprint_path) -> FBridgeBlueprintSummary

One-call high-level digest — aggregates parent class, interfaces, events
handled, public functions, dispatchers, variable stats, component /
timeline / macro counts, total node count across every graph, the
variable categories in use, the most-called external classes (top 10
by call frequency, self-class excluded), and the top referenced assets.

```python
s = unreal.UnrealBridgeBlueprintLibrary.get_blueprint_summary(
    '/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC')

# High-level shape
print(s.parent_class, '(native:', s.blueprint_type, ')')   # Character (native: Character)
print('events:', list(s.events_handled))                   # ReceiveBeginPlay, ReceiveTick, OnJumped, …
print('public_funcs:', len(list(s.public_functions)))      # 49
print('dispatchers:', list(s.event_dispatchers))           # ['On Request Interact']

# State
print(s.variable_count, 'vars,',
      s.instance_editable_count, 'instance-editable,',
      s.replicated_variable_count, 'replicated')            # 19 / 19 / 1

# Complexity
print('total nodes:', s.total_node_count)                   # 427
print('categories:', list(s.variable_categories))           # ['Camera', 'Input', 'Movement', ...]

# Dependencies
print('calls classes:', list(s.key_referenced_classes))     # KismetMathLibrary, Character, KismetSystemLibrary, …
print('assets:', list(s.key_referenced_assets))             # IA_Sprint, IA_Walk, IA_Jump, …
```

**What persists in the output:** short names (`ParentClass`), full paths
where disambiguation matters (`ParentClassPath`, asset entries), and
aggregate counts. No node guids, no raw graph data — use the granular
queries for those.

### get_function_summary(blueprint_path, function_name) -> FBridgeFunctionSemantics

Per-function / per-event semantic digest. Walks the graph's exec chain
from its entry node to produce an indented human-readable outline, plus
aggregates: variables read / written, functions called, dispatchers
fired, classes spawned, loop / branch flags, comment text.

Accepts function names, event names (searches `UbergraphPages` for a
matching `K2Node_Event`), or macro names.

```python
s = unreal.UnrealBridgeBlueprintLibrary.get_function_summary(
    '/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC',
    'GetDesiredGait')

print(s.kind, s.access, 'pure=', s.is_pure)        # Event Public pure=True
print('params:', [(p.name, p.type) for p in s.params])   # [('ReturnValue', 'E_Gait')]
print('reads:', list(s.reads_variables))            # ['CharacterInputState', 'FullMovementInput', ...]
print('writes:', list(s.writes_variables))
print('calls:', list(s.calls_functions))            # ['KismetMathLibrary.BooleanOR', 'SKEL_SandboxCharacter_CMC_C.CanSprint', ...]
print('branches:', s.has_branches, 'loops:', s.has_loops)

for line in list(s.exec_outline):
    print(line)
# Entry
# Set FullMovementInput
# Branch
#   True →
#     Branch
#       True →
#         Return
#       False →
#         Return
#   False →
#     Branch
#       True →
#         Return
#       False →
#         Branch
#           True →
#             Return
#           False →
#             Return
```

**Outline conventions:**
- 2-space indent per nesting level
- Multi-output nodes (Branch, Sequence, Cast, ForEach) render each
  exec-output as a labelled child branch; single-output chains stay
  at the same indent
- "then" / "else" pin names are prettified to "True" / "False"
- Reroute knot nodes are followed through but not rendered
- Capped at 200 lines + depth 8 to bound large graphs
- Cycle-safe: a visited set prevents loops from recursing forever

**Aggregates are computed by full-graph walk** (not just exec chain) —
so a purely-data-flow variable read inside a loop body still shows up
in `reads_variables`.

### find_variable_references(blueprint_path, variable_name) -> list[FBridgeReference]
### find_function_call_sites(blueprint_path, function_name) -> list[FBridgeReference]
### find_event_handler_sites(blueprint_path, event_name) -> list[FBridgeReference]

Cross-reference queries that walk every graph (`UbergraphPages` +
`FunctionGraphs` + `MacroGraphs`) and return one entry per matched
node. Core refactor / impact-analysis tool: "where is this variable
written?", "which graph calls this function?", "where is this dispatcher
bound vs fired?".

```python
L = unreal.UnrealBridgeBlueprintLibrary
bp = '/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC'

for r in L.find_variable_references(bp, 'CharacterInputState'):
    print(f'  [{r.kind}] {r.graph_type}/{r.graph_name}: {r.node_title}')
# [read]  EventGraph/EventGraph: Get CharacterInputState
# [read]  EventGraph/EventGraph: Get CharacterInputState
# [write] EventGraph/EventGraph: Set CharacterInputState
# [read]  Function/UpdateRotation_PreCMC: Get CharacterInputState
# [read]  Function/GetDesiredGait: Get CharacterInputState
# [read]  Function/CanSprint: Get CharacterInputState

for r in L.find_function_call_sites(bp, 'CanSprint'):
    print(r.graph_name, r.node_guid)
# GetDesiredGait  <guid>

for r in L.find_event_handler_sites(bp, 'OnCharacterMovementUpdated'):
    print(r.kind, r.graph_name, r.node_title)
# bind  EventGraph  "Assign On Character Movement Updated"
```

**Kind field semantics:**
- `"read"` — `K2Node_VariableGet` for the named variable
- `"write"` — `K2Node_VariableSet`
- `"call"` — `K2Node_CallFunction` / `K2Node_MacroInstance` / `K2Node_Message`
- `"event"` — `K2Node_Event` / `K2Node_CustomEvent` entry
- `"bind"` / `"unbind"` — `K2Node_AddDelegate` / `K2Node_RemoveDelegate`

The returned `NodeGuid` matches what `get_function_nodes` reports, so
you can pipe Find* results into `remove_graph_node`, `set_node_enabled`,
`set_node_comment`, etc. for bulk edits.

---

## Pin introspection

Read any pin on any node — both its metadata (type, direction, flags)
and its default value. Closes the Set/Get symmetry gap: `set_pin_default_value`
existed already; these two are the read side. With both, an agent can
**fully reconstruct** the logic of any node — including Select defaults,
static-function CDO self pins, hidden pins, and unconnected literals.

### get_pin_default_value(blueprint_path, graph_name, node_guid, pin_name) -> str

Single-pin lookup. Returns the effective default as a string — object
path for object refs, text contents for text pins, raw string for
literals. Empty when the pin is connected (UE ignores the default in
that case) or has no default set.

```python
L = unreal.UnrealBridgeBlueprintLibrary
bp = '/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC'
fn = 'CalculateMaxAcceleration'
map_node = '459D8CE64B09A309F667D7B744FE5A9A'  # MapRangeClamped node guid

for p in ['InRangeA', 'InRangeB', 'OutRangeA', 'OutRangeB']:
    print(p, '=', L.get_pin_default_value(bp, fn, map_node, p))
# InRangeA  = '300.000000'
# InRangeB  = '700.000000'
# OutRangeA = '800.000000'
# OutRangeB = '300.000000'
```

### get_node_pins(blueprint_path, graph_name, node_guid) -> list[FBridgePinInfo]

Full pin surface — every pin including unconnected ones, hidden ones,
and static-function self pins. Each entry carries type, direction,
category, default, and connection state.

```python
select_node = 'C08E653441319F523B95C1B5D9A29EDE'   # Select node in CalculateMaxAcceleration
for p in L.get_node_pins(bp, fn, select_node):
    flags = []
    if p.is_exec:      flags.append('exec')
    if p.is_connected: flags.append(f'wired({p.link_count})')
    if p.is_hidden:    flags.append('hidden')
    if p.is_self_pin:  flags.append('self')
    if p.is_array:     flags.append('array')
    default = repr(p.default_value) if p.default_value else ''
    print(f'[{p.direction}] {p.name} : {p.type}  {" ".join(flags)}  {default}')
# [input]  NewEnumerator0 : Double              '800.000000'
# [input]  NewEnumerator1 : Double              '800.000000'
# [input]  NewEnumerator2 : Double   wired(1)   '0.0'
# [input]  Index          : Enum<E_Gait>  wired(1)  'NewEnumerator0'
# [output] ReturnValue    : Double   wired(1)
```

#### FBridgePinInfo fields

| Field | Meaning |
|-------|---------|
| `name` | internal pin name (use with `connect_graph_pins`, `set_pin_default_value`, …) |
| `display_name` | user-facing label shown in the editor (sometimes localised) |
| `type` | human type — `Exec`, `Bool`, `Int`, `Double`, `Vector`, `Actor`, `Array of Int`, `Enum<E_Gait>`, `Class<PrimitiveComponent>`, `Map<Name, Int>` |
| `direction` | `"input"` \| `"output"` |
| `category` | raw `FEdGraphPinType::PinCategory` (e.g. `"exec"`, `"object"`) |
| `sub_category` / `sub_category_object_path` | raw sub-category + path of the class / struct / enum object (empty for primitives) |
| `default_value` | effective default as string (object path for object pins) |
| `has_default_object` | true for object-ref defaults (distinct from literal defaults) |
| `is_exec`, `is_connected`, `link_count` | exec wire flag + connection state |
| `is_array` / `is_set` / `is_map` | container type flags |
| `is_reference`, `is_const`, `is_hidden` | UE pin-type modifiers |
| `is_self_pin` | true for the implicit target / self pin static-function calls carry (usually hidden) |

**Interpreting `default_value` on a connected pin.** UE stores the last
literal even after a wire is connected (it just ignores it at
evaluation time). Treat `default_value` as meaningful **only when
`is_connected == false`**.

**Reading Select defaults.** The `Select` node (K2Node_Select) exposes
`NewEnumerator0` / `NewEnumerator1` / ... as input pins — one per enum
entry. Unconnected inputs keep their literal default; connected ones
take precedence at runtime.

**Reading static-function CDO self pins.** `KismetMathLibrary`,
`KismetSystemLibrary`, `GameplayStatics` etc. are all static libraries.
Their `self` pin is hidden by default and points to the class's CDO
(e.g. `/Script/Engine.Default__KismetMathLibrary`) — `get_node_pins`
surfaces this; `get_node_pin_connections` skips it because there's no
wire.

---

### describe_node(blueprint_path, graph_name, node_guid) -> FBridgeNodeDescription

One-shot description of a single node. Combines title, position, size,
classification, K2Node-subclass-specific fields (target class / function,
variable name + scope + type, cast target, event name, macro graph,
struct type, delegate name, literal value), AND every pin with types,
defaults, and **link targets** — all in a single bridge round-trip.
Replaces the common pattern of chaining `get_function_nodes` +
`get_node_pins` + `get_node_pin_connections` + `get_pin_default_value`
for a single node.

> **Prefer this** when you want to understand *one* specific node fully
> (about to edit it, reason about its wiring, copy its signature). Use
> `get_function_nodes` when you want a catalog of many nodes; use
> `get_node_pin_connections` when you want the full wire graph.

```python
desc = unreal.UnrealBridgeBlueprintLibrary.describe_node(bp, fn, guid)
print(f'{desc.title} ({desc.node_class}) at ({desc.pos_x}, {desc.pos_y})')
if desc.node_type == 'FunctionCall':
    print(f'  calls {desc.target_class}::{desc.target_name}  pure={desc.is_pure}')
elif desc.node_type in ('VariableGet', 'VariableSet'):
    print(f'  {desc.variable_scope} variable {desc.variable_name}: {desc.variable_type}')
for p in desc.pins:
    links = ' -> ' + ', '.join(p.linked_to) if p.linked_to else ''
    print(f'  [{p.direction}] {p.name} : {p.type}{links}')
```

#### FBridgeNodeDescription fields

| Field | Meaning |
|-------|---------|
| `node_guid` | 32-hex digits form |
| `title` | palette-style display title |
| `node_type` | coarse classification — `"FunctionCall"`, `"VariableGet"`, `"Branch"`, `"Cast"`, `"Event"`, `"CustomEvent"`, `"Macro"`, `"Spawn"`, `"Timeline"`, `"Knot"`, `"MakeStruct"`, `"BreakStruct"`, ... |
| `node_class` | exact UE class name, e.g. `"K2Node_CallFunction"`, `"K2Node_IfThenElse"` — use this when dispatching on node type |
| `pos_x`, `pos_y` | node origin in graph units |
| `width`, `height` | stored size if Slate arranged it, else estimate |
| `comment` | user-authored floating comment above the node (if any) |
| `enabled_state` | `"Enabled"` / `"Disabled"` / `"DevelopmentOnly"` |
| `target_class` | function call: target class short name. Cast: target class |
| `target_name` | function call: function internal name. Event/CustomEvent: event name |
| `variable_name` | VariableGet/Set: variable internal name |
| `variable_scope` | `"member"` \| `"local"` \| `"external"` |
| `variable_type` | resolved variable type string |
| `is_pure` | function call pure flag / pure cast |
| `is_const` | function call: target UFunction is const |
| `struct_type` | MakeStruct/BreakStruct: struct class path |
| `literal_value` | literal nodes (`MakeLiteral*`, `EnumLiteral`): current value string |
| `macro_graph` | macro instance: referenced macro graph name |
| `delegate_name` | Add/Remove/Call delegate nodes: delegate property name |
| `exec_out_count` | wired-or-unwired exec-output pin count (fanout detection) |
| `pins` | list of `FBridgePinInfo` with `linked_to` populated |

#### Pin `linked_to`

Each pin's `linked_to` is a list of strings `"<node_guid>:<pin_name>"`
identifying the other end of every wire on that pin. Parse with
`guid, pin = entry.split(':', 1)`; resolve via a subsequent
`describe_node(bp, fn, guid)` or by looking up from a
`get_function_nodes` result you already have cached.

---

### get_function_signature(class_path, function_name) -> FBridgeFunctionSignature

Look up a `UFunction`'s full signature — parameter names, types,
declared default values (read from `CPP_Default_<ParamName>` metadata
on the UFunction), ref / const / out flags, and blueprint-facing flags
(pure / const / static / latent / callable / native).

**Use before spawning a CallFunction node** so the caller knows exactly
which pins to wire and which defaults to leave untouched.

```python
sig = unreal.UnrealBridgeBlueprintLibrary.get_function_signature(
    'KismetSystemLibrary', 'PrintString'
)
if sig.found:
    print(f'{sig.owning_class}::{sig.function_name}  pure={sig.is_pure} static={sig.is_static}')
    for p in sig.parameters:
        tag = '<-' if p.is_output else '->'
        default = f' = {p.default_value}' if p.default_value else ''
        print(f'  {tag} {p.name}: {p.type}{default}  ref={p.is_reference}')
# KismetSystemLibrary::PrintString  pure=False static=True
#   -> WorldContextObject: Object<Object>  ref=False
#   -> InString: String = Hello  ref=False
#   -> bPrintToScreen: Bool = true  ref=False
#   -> bPrintToLog: Bool = true  ref=False
#   -> TextColor: LinearColor = (R=0.0,G=0.66,B=1.0,A=1.0)  ref=False
#   -> Duration: Float = 2.0  ref=False
#   -> Key: Name = None  ref=False
```

`class_path` accepts either a full path (`/Script/Engine.KismetSystemLibrary`)
or a short class name (`KismetSystemLibrary`) — short names are resolved
via `FindFirstObject<UClass>` so they work for any loaded class. For
Blueprint-defined functions, pass the generated class path
(`/Game/.../BP_X.BP_X_C`).

`function_name` is the internal `FName` of the UFunction, not the
display name. Use `get_blueprint_functions` or `list_spawnable_actions`
to discover internal names when unsure.

#### FBridgeFunctionSignature fields

| Field | Meaning |
|-------|---------|
| `found` | false when class/function didn't resolve |
| `function_name` | canonical `Func->GetName()` |
| `owning_class` / `owning_class_path` | short name + full path |
| `is_pure` | mirrors `FUNC_BlueprintPure` |
| `is_const` | `FUNC_Const` |
| `is_static` | `FUNC_Static` |
| `is_latent` | has `"Latent"` metadata |
| `is_blueprint_callable` | `FUNC_BlueprintCallable` |
| `is_blueprint_pure` | `FUNC_BlueprintPure` |
| `is_native` | `FUNC_Native` |
| `category` | `"Category"` metadata |
| `tooltip` | `"ToolTip"` metadata |
| `parameters` | list of `FBridgeFunctionParam` (see below) |

#### FBridgeFunctionParam fields (extended)

| Field | Meaning |
|-------|---------|
| `name` | parameter name |
| `type` | human type string |
| `is_output` | true for out-params and return values |
| `default_value` | stored `CPP_Default_<name>` metadata; empty when caller must wire |
| `is_reference` | passed by ref — BP caller must wire a variable, not a literal |
| `is_const` | declared const |

---

## Node layout (position / size / corners / pin coordinates)

Read node geometry for layout purposes — placing new nodes adjacent to
existing ones, wrapping regions with comment boxes, positioning reroute
knots mid-wire.

⚠️ **Size accuracy caveat.** `UEdGraphNode::NodeWidth/NodeHeight` is
only authoritative for **Comment nodes** (user-authored, always
correct). For regular K2 nodes the stored size stays 0 because Slate
caches its own desired-size on the widget and doesn't write back. We
fall back to an estimate from title length + visible pin count, good
enough for relative placement but not pixel-perfect. The returned
struct tells you which path was taken via `size_is_authoritative`.

### get_node_layout(blueprint_path, graph_name, node_guid) -> FBridgeNodeLayout

Origin, stored / estimated / effective dimensions, four corners, and
centre in graph coordinates.

```python
L = unreal.UnrealBridgeBlueprintLibrary
bp = '/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC'
fn = 'CalculateMaxAcceleration'
select = 'C08E653441319F523B95C1B5D9A29EDE'

lay = L.get_node_layout(bp, fn, select)
print(f'({lay.pos_x}, {lay.pos_y})  {lay.effective_width}×{lay.effective_height}  auth={lay.size_is_authoritative}')
# (896, 64)  180×140  auth=False    ← estimated (no Slate pass)

print('TL', lay.top_left, 'BR', lay.bottom_right, 'centre', lay.center)
```

#### FBridgeNodeLayout fields

| Field | Meaning |
|-------|---------|
| `pos_x`, `pos_y` | Node origin in graph coordinates (always accurate) |
| `stored_width`, `stored_height` | Raw `NodeWidth/Height` from the UEdGraphNode. 0 for most nodes |
| `estimated_width`, `estimated_height` | Synthesised from title + visible pins |
| `effective_width`, `effective_height` | `stored_*` if nonzero, else `estimated_*` |
| `top_left` / `top_right` / `bottom_left` / `bottom_right` / `center` | Corner + centre points, computed from `effective_*` |
| `is_comment_box` | True for `UEdGraphNode_Comment` |
| `size_is_authoritative` | True when Effective came from Stored (i.e. comment box or a node that has been rendered) |

**Comment boxes are reliable.** `add_comment_box` sets Stored* explicitly,
so `get_node_layout` on a comment immediately returns authoritative
sizes. Use comments as "layout anchors" if you need exact dimensions.

### get_node_pin_layouts(blueprint_path, graph_name, node_guid) -> list[FBridgePinLayout]

Estimated coordinates for every visible pin on a node. Input pins sit
~10 px inset from the left edge; outputs sit the same distance from
the right edge; pin Y = header (40) + `direction_index` × row height
(22). Hidden pins are surfaced with `direction_index = -1` and zero
offset.

```python
for p in L.get_node_pin_layouts(bp, fn, select):
    x, y = int(p.position.x), int(p.position.y)
    print(f'[{p.direction}#{p.direction_index}] {p.name}  →  ({x}, {y})')
# [input#0]  NewEnumerator0  →  (906, 104)
# [input#1]  NewEnumerator1  →  (906, 126)
# [input#2]  NewEnumerator2  →  (906, 148)
# [input#3]  Index           →  (906, 170)
# [output#0] ReturnValue     →  (1066, 104)
```

#### FBridgePinLayout fields

| Field | Meaning |
|-------|---------|
| `name` | pin internal name |
| `direction` | `"input"` / `"output"` |
| `direction_index` | index among pins of same direction (0-based, hidden skipped; -1 for hidden pins) |
| `local_offset` | position relative to node top-left origin |
| `position` | absolute graph position (node pos + local offset) |
| `is_exec` | exec-pin flag |
| `is_hidden` | hidden pin (still surfaced; has `direction_index = -1`) |
| `is_estimated` | always True for v1 — real Slate-computed coords aren't exposed without the graph panel active |

**Layout recipes.**
- **Midpoint of a wire**: `(src_pin.position + dst_pin.position) / 2` →
  drop a reroute knot there with `add_reroute_node`.
- **Place a node to the right of another**: `new_x = lay.top_right.x +
  40; new_y = lay.pos_y` → call `add_call_function_node(..., new_x,
  new_y)`.
- **Wrap a region in a comment**: find min top-left / max bottom-right
  across the target nodes' `get_node_layout` results, pad by 30, pass
  as `add_comment_box(x, y, width, height)`.

**Accuracy caveat.** `local_offset.y` uses the generic formula
`HeaderHeight=40 + row × PinRowHeight=22`. This is **wrong for compact
nodes** (`>=`, `OR`, `+`, variable getters) and for standard nodes
whose actual header isn't 40 px. For pixel-accurate pin positions,
open the function graph first (`open_function_graph_for_render`, wait
a Slate tick) and use **`get_rendered_node_info`** — it reads the
live SGraphPin widgets.

### open_function_graph_for_render(blueprint_path, graph_name) -> bool

Open the named function/event/macro graph in the Blueprint editor and
bring its tab to the front. This is what forces UE to construct the
`SGraphNode`/`SGraphPin` widgets for the graph — without it, the
function graph's widgets don't exist and `get_rendered_node_info`
returns nothing usable.

Must be called in one bridge exec ON ITS OWN (Slate can't tick while
an exec holds the game thread). A typical flow is three execs:

```python
# Exec 1
L.open_function_graph_for_render(bp, 'MyFunction')
```
```python
# Client-side sleep — Slate ticks in the meantime
import time; time.sleep(4)
```
```python
# Exec 2: now widgets exist and have been laid out
infos = L.get_rendered_node_info(bp, 'MyFunction')
# ...or run auto_layout_graph in pin_aligned mode, which calls
# get_rendered_node_info internally.
```

Returns `True` if the graph was found and opened, `False` if the
Blueprint / graph couldn't be resolved.

**⚠ Prerequisite: the Blueprint editor must already be open for this
asset.** Internally this calls `UAssetEditorSubsystem::OpenEditorForAsset`
then immediately `FindEditorForAsset(bp, /*bFocusIfOpen*/false)`. The
open is async — same-exec `FindEditorForAsset` returns null and the
function gives back `False` on a fresh editor launch.

**Do NOT poll this in a tight loop** — polling alone does not make the
asset editor register any faster, it just burns round-trips. Instead:

```python
# Exec A: force the BP editor tab to exist
unreal.UnrealBridgeEditorLibrary.open_asset(bp)
```
```python
# Client-side sleep so the asset editor registers
import time; time.sleep(2)
```
```python
# Exec B: now open_function_graph_for_render returns True
L.open_function_graph_for_render(bp, 'MyFunction')
```

### get_rendered_node_info(blueprint_path, graph_name) -> list[FBridgeRenderedNode]

Pixel-accurate node geometry and pin positions, queried from the live
SGraphNode/SGraphPin Slate widgets. Use this (not `get_node_layout` /
`get_node_pin_layouts`) whenever you need ACTUAL rendered coordinates
— e.g. to drive a pin-accurate layout, to measure the real gap
between two wired pins, or to check why a wire renders diagonal.

Prerequisite: the graph must already be open + ticked. Call
`open_function_graph_for_render` in a prior exec and wait a moment.

```python
# Measure the real Y of every pin on a >=
for r in L.get_rendered_node_info(bp, g):
    if r.title != '>= 布尔':
        continue
    print(f'size={int(r.size.x)}x{int(r.size.y)}')
    for p in r.pins:
        if p.is_hidden: continue
        print(f'  {p.direction:6s} #{p.direction_index} Y={p.graph_position.y:.0f}  {p.name!r}')
# size=147x66
#   input  #0  Y=190  'A'
#   input  #1  Y=220  'B'
#   output #0  Y=205  'ReturnValue'       ← output centered between inputs!
```

Each node returned has `is_live=True` when a widget was found, or
`is_live=False` (with zero size and empty pins) for nodes whose
widget isn't rendered yet. Compare against `get_node_pin_layouts`
output for the same node to see where the generic formula is wrong.

#### FBridgeRenderedNode fields

| Field | Meaning |
|-------|---------|
| `node_guid` | The node's GUID (digits format) |
| `title` | `GetNodeTitle(ListView)` |
| `graph_position` | Node's NodePosX/Y (matches `get_node_layout.pos_*`) |
| `size` | Actual rendered size from `SGraphNode::GetDesiredSize` |
| `pins` | Array of `FBridgeRenderedPin` (see below) |
| `is_live` | True if the widget was found and measured |

#### FBridgeRenderedPin fields

| Field | Meaning |
|-------|---------|
| `name` | Pin internal name |
| `direction` | `"input"` / `"output"` |
| `direction_index` | Index among visible pins of the same direction |
| `node_offset` | Pin position relative to node top-left, from `SGraphPin::GetNodeOffset` |
| `graph_position` | Absolute graph position (node pos + node offset) |
| `is_exec` | True for exec-category pins |
| `is_hidden` | True for hidden pins (surfaced but no widget) |

**Knot caveat**: `K2Node_Knot` reports `node_offset = (0, 0)` from
`GetNodeOffset`. Treat knots specially: the actual pin center sits at
`NodePos + (0, 12)` for input and `(42, 12)` for output (knot body is
42×24, pins vertically centred).

### predict_node_size(kind, param_a, param_b, param_int) -> FBridgeNodeSizeEstimate

Predict a node's rendered size **before** spawning it, so you can
reserve space / avoid overlap without a spawn→measure round-trip. The
formula matches `FBridgeNodeLayout.estimated_*` exactly — prediction and
post-spawn estimate agree to the pixel for resolved kinds.

```python
L = unreal.UnrealBridgeBlueprintLibrary

# Reserve horizontal space for a PrintString call + a Sequence(3)
p1 = L.predict_node_size('function_call',
    '/Script/Engine.KismetSystemLibrary', 'PrintString', 0)
p2 = L.predict_node_size('sequence', '', '', 3)

x = 0
fn = L.add_call_function_node(bp, g,
    '/Script/Engine.KismetSystemLibrary', 'PrintString', x, 0)
x += p1.width + 80
seq = L.add_sequence_node(bp, g, 3, x, 0)
```

Supported kinds (arg meanings):

| Kind | `param_a` | `param_b` | `param_int` |
|------|-----------|-----------|-------------|
| `function_call` | class path (e.g. `/Script/Engine.KismetSystemLibrary`) | function name | — |
| `event`         | class path | event name | — |
| `variable_get`  | — | variable name | — |
| `variable_set`  | — | variable name | — |
| `custom_event`  | event name (for title width) | — | data-output count |
| `branch`        | — | — | — |
| `sequence`      | — | — | `then` pin count (≥2) |
| `cast`          | target class path (for title) | — | — |
| `self`          | — | — | — |
| `reroute`       | — | — | — |
| `delay`         | — | — | — |
| `foreach` / `forloop` / `whileloop` | — | — | — |
| `select`        | — | — | option count (≥2) |
| `make_array`    | — | — | element count (≥1) |
| `make_struct` / `break_struct` | struct path | — | — |
| `enum_literal`  | enum path | — | — |
| `make_literal`  | type string (e.g. `"Vector"`) | — | — |
| `spawn_actor`   | actor class path | — | — |
| `dispatcher_call` / `dispatcher_bind` / `dispatcher_event` | blueprint path | dispatcher name | — |

Unknown kind returns a 180×60 fallback with `resolved=False`.

#### FBridgeNodeSizeEstimate fields

| Field | Meaning |
|-------|---------|
| `width`, `height` | Predicted size in graph units |
| `input_pin_count`, `output_pin_count` | Visible pin counts (exec + data) |
| `kind` | Echoes the input kind |
| `resolved` | True if kind + params resolved cleanly; False on fallback |
| `notes` | Diagnostic: `"function not found"`, `"unknown kind"`, etc. |

### auto_layout_graph(blueprint_path, graph_name, strategy, anchor_node_guid, h_spacing, v_spacing) -> FBridgeLayoutResult

Re-flow a graph with an exec-flow-driven auto-layout. Choose a strategy
by passing its name:

- **`"exec_flow"`** (Sugiyama-lite): topologically layer nodes by exec
  dependency, place each layer left-to-right, stack vertically within
  a layer, then barycentrically re-order each layer to reduce wire
  crossings. Pure / data nodes attach to their first exec consumer's
  layer − 1. **Positions nodes at layer-center Y** — exec pins are
  not pixel-aligned; follow up with `straighten_exec_chain` for that.
  Fast, crossings-minimal; the right choice for bulk tidy on large
  graphs where pin-perfect alignment matters less.

- **`"pin_aligned"`** (recommended for human-readable output): places
  nodes using **DFS-ordered downstream alignment** — the same shape
  hand-authored BPs take. See the full details below; this is what
  you should use for polish.

**Only moves nodes — wires, comments, and pin defaults are untouched**
(aside from reroute knots the pass inserts/strips as described below).
Safe to call repeatedly; idempotent on stable topology. Doesn't compile
the Blueprint.

#### `"pin_aligned"` — the flow

Getting a pixel-accurate pin-aligned layout requires three bridge
execs in sequence, because Slate can't tick while an exec holds the
game thread:

```python
L = unreal.UnrealBridgeBlueprintLibrary
bp = '/Game/Blueprints/BP_MyActor.BP_MyActor'
g  = 'MyFunction'

# Exec 1: open the function graph in BP editor, forcing Slate to
#         construct the SGraphNode/SGraphPin widgets we'll read from.
L.open_function_graph_for_render(bp, g)
```

```python
# (short client-side sleep — ~3-4 s — to let Slate tick the widgets)
import time; time.sleep(4)
```

```python
# Exec 2: run the layout. It queries the live widgets for pixel-exact
#         node sizes and pin positions, then places everything.
r = L.auto_layout_graph(bp, g, 'pin_aligned', '', 100, 48)
print(f'bounds={r.bounds_width}×{r.bounds_height}')
for w in r.warnings: print(' !', w)
```

If you skip step 1 (or call step 2 in the same exec), the cache is
empty and the layout silently falls back to formula-based size/pin
estimates. Estimates are off by 10–50 px per node for UE 5.7's
compact/standard node mix — visible as mildly-diagonal wires even on
"aligned" pairs. Always run the three-exec flow for anything you care
about visually.

#### `"pin_aligned"` — the algorithm (why it matches hand-laid BPs)

1. **Strip prior reroutes**: collapse every existing knot into a
   direct src→dst link, so the algorithm always operates on the
   canonical topology. (Calling layout repeatedly doesn't accumulate
   reroutes.)

2. **Query live Slate geometry** for every node: real rendered size
   from `SGraphNode::GetDesiredSize`, real pin offsets from
   `SGraphPin::GetNodeOffset`. Required for compact nodes (`>=`,
   `OR`, variable getters) whose pin Ys differ from the generic
   `HeaderHeight + row × PinRowHeight` formula by 10–22 px.

3. **DFS the exec tree** from the function entry, visiting exec-out
   pins in pin-direction order (`.then` before `.else`). Push leaves
   (nodes with no exec successor — Returns, terminal calls) to a flat
   list in DFS-visit order. This matches how humans read top-to-bottom.

4. **Stack leaves** in that DFS order spanning layers: Return Sprint
   (Branch.True path) at the top, Return Run (Branch.False), Return
   Walk, Return Run (crouch.True), Return Walk (crouch.False) — all
   in one vertical column, regardless of exec layer depth.

5. **Place non-leaves backward** from the deepest exec layer. Each
   Branch / exec node pulls its Y from its primary successor (the
   `.then` path) so the `.then` wire reads as a perfect horizontal
   rail. `.else` branches fan out diagonally.

6. **Place data producers** per-consumer: each pure node sits
   `DataHSpace` px to the left of its primary consumer, pin-Y-aligned
   to the specific input pin it feeds. Chained producers cascade
   further left by the same rule. Siblings feeding different pins of
   the same consumer share a right edge so output pins align.

7. **Reroute diagonal exec wires** with a single knot per wire,
   placed near the destination at the dst pin Y. The first segment is
   a natural diagonal Bezier; the final segment is horizontal into
   the destination pin. Data wires are left as bare Beziers — hand-
   laid BPs don't knot mild data diagonals.

#### Spacing: user values are *loose upper bounds*, not exact gaps

`h_spacing` and `v_spacing` are hints, not strict widths. The
algorithm derives three tighter values from them:

| What                                    | Formula                     |
|-----------------------------------------|-----------------------------|
| `DataHSpace` — gap between data columns | `max(15, h_spacing / 3)`    |
| `ExecGap`    — gap between exec layers  | `max(30, h_spacing / 2)`    |
| `VSpace`     — gap between stacked rows | `v_spacing` (used as-is)    |

So `h_spacing=100` → `DataHSpace=33`, `ExecGap=50` inside. Pass 0 or
negative to get defaults (`h=80, v=40` → internally 26/40). Pass
smaller values (e.g. 40/32 → 15/30) for a denser pack.

#### Anchor node — keep one position fixed

```python
# Keep BeginPlay at its current spot; the rest of the graph flows
# out from it. Useful when merging layout into an active edit session.
r = L.auto_layout_graph(bp, 'EventGraph', 'pin_aligned', begin_guid, 100, 50)
```

#### Comment boxes

Skipped — moving a comment box breaks its "encloses these specific
nodes" intent. Comments stay at their authored XY; the nodes inside
may shift out of them. Re-enclose or resize the box afterward if it
matters.

#### Unavoidable diagonals

Two cases where pin-Y alignment is physically impossible:

- **Multi-input consumer with tall producers**: V2D (74 px tall) and
  Threshold (38 px tall) both feeding a `>=` whose A/B pins are only
  22 px apart. The producers have to sit above/below each other with
  VSpace between them, so at most one pin-aligns.
- **Single output feeding multiple pins at different Y**: OR's one
  `Return Value` out wired to Select's dir-1 and dir-3 pins (44 px
  apart on Select). Only one wire can be horizontal.

Both are rendered as Bezier curves without knots — matches the
reference look.

**Strategies**: `"exec_flow"`, `"pin_aligned"`. Reserved (not yet
implemented): `"data_flow"`, `"event_grouped"`.

#### FBridgeLayoutResult fields

| Field | Meaning |
|-------|---------|
| `succeeded` | True if the graph was processed (false = BP or graph missing / bad strategy) |
| `nodes_positioned` | Count of nodes whose `NodePosX/Y` was updated |
| `layer_count` | Depth of the final layout (layer 0 = leftmost sources) |
| `bounds_width`, `bounds_height` | Size of the laid-out region in graph units |
| `warnings` | Diagnostics: cycle breaks, unreachable anchors, unknown strategy |

**Cycle handling**: exec cycles break the longest-path layering. The
algorithm caps iterations and drops any still-unlayered nodes at layer 0
with a warning. The graph isn't mutated to break the cycle — fix the
wires first.

**Reading geometry same-exec after auto_layout**: `auto_layout_graph`
writes new `NodePosX / NodePosY` on the node model, but the live
`SGraphNode` Slate widgets don't pick up the change until they tick —
which can't happen while the bridge exec is still holding the
GameThread. So `get_rendered_node_info` returns the *pre-layout* pin
coordinates if you call it right after `auto_layout_graph` in the same
exec.

For post-layout geometric analysis (crossing detection, wire-length
measurement, overlap checks, bounds auditing), read from the node model
instead: `get_node_layout(bp, fn, guid).pos_x / pos_y /
effective_width / effective_height`. Pin positions can be estimated
from `NodePosY + 40 + 22 × dir_index` (see `get_node_pin_layouts` for
the same formula), which is authoritative right after layout. Only
reach for `get_rendered_node_info` when you specifically need
Slate-accurate pin coords, and only after the 3-exec
open-graph → sleep → query dance.

---

## Quality / review toolkit

The following five APIs turn a freshly-authored BP from "works" into "a human wants to maintain this". They're meant to be called together as a post-build review loop — see the workflow block at the top of `SKILL.md`.

### lint_blueprint(blueprint_path, severity_filter, oversized_fn_threshold, long_exec_chain_threshold, large_graph_threshold) -> list[FBridgeLintIssue]

Run a structural / stylistic review and return findings. Pass `-1` for any threshold to use the default. Pass `""` for the severity filter to get everything.

```python
L = unreal.UnrealBridgeBlueprintLibrary
for i in L.lint_blueprint(bp, '', -1, -1, -1):
    print(f'[{i.severity}] {i.code}: {i.message}')
```

**Check codes (v1)**:

| Code | Severity | Triggered when |
|------|----------|----------------|
| `OrphanNode` | warning | Every pin on a node has zero links (dead code / left-over spawn) |
| `OversizedFunction` | warning | Function graph exceeds node threshold (default 20) |
| `UnnamedCustomEvent` | warning | `CustomEvent_N` / `Event_N` pattern — rename to describe intent |
| `UnnamedFunction` | warning | `NewFunction_N` pattern |
| `InstanceEditableNoCategory` | info | Editable var with `Default` / empty / = var-name category |
| `InstanceEditableNoTooltip` | info | Editable var with no tooltip — designers won't know what it does |
| `UnusedVariable` | info | Non-editable class variable with zero references |
| `UnusedLocalVariable` | info | Local variable with zero references |
| `LargeUncommentedGraph` | info | Graph has ≥ threshold nodes and no comment boxes |
| `LongExecChain` | warning | Unbroken linear exec chain ≥ threshold — consider extraction |

**FBridgeLintIssue fields**: `severity`, `code`, `message`, `graph_name`, `node_guid`, `variable_name`, `function_name` (location fields are `""` when not meaningful).

**Caveat (locale)**: `InstanceEditableNoCategory` uses English-source + var-name matching. In non-English locales UE may store `Default` as a localized FText (e.g. `默认`) which slips past the check — ignore the false negative or set an explicit category to silence it.

### collapse_nodes_to_function(blueprint_path, source_graph, node_guids, new_function_name) -> (str, str)

Extract a selection of nodes into a new Function graph and replace the selection in the source graph with a CallFunction gateway node wired to the same external pins. Returns `(gateway_guid, new_graph_name)`. On failure, gateway is `""`.

```python
# After lint flags a LongExecChain, extract the offending range
gateway, new_name = L.collapse_nodes_to_function(bp, 'EventGraph',
    [p1, p2, p3, p4, p5], 'HandlePrints')
print(f'extracted into {new_name}, call-site {gateway[:8]}')
```

**Algorithm** (mirrors `FBlueprintEditor::CollapseNodesIntoGraph`):
1. Create a new function graph (unique name derived from `new_function_name`)
2. Move the selected nodes into it
3. For each pin that crosses the selection boundary, create matching pins on `FunctionEntry` (inputs) / `FunctionResult` (outputs) and on the gateway CallFunction
4. Re-wire: external side ↔ gateway, internal side ↔ entry/result

**Constraints**:
- The selection must all live in the same graph.
- `FunctionEntry` / `FunctionResult` / tunnel nodes are rejected (can't be extracted).
- An empty-output function discards its result node automatically.

### straighten_exec_chain(blueprint_path, graph_name, start_node_guid, start_exec_pin_name) -> int

Walk the linear exec chain downstream from `start_node_guid.start_exec_pin_name` and align each downstream node's `NodePosY` so its exec-input pin sits at *exactly* the same Y as the upstream exec-output pin. Returns the number of nodes adjusted.

Pass `""` for `start_exec_pin_name` to pick the first exec-output pin with a single link.

```python
# After auto_layout_graph, straighten the main event's exec rail
L.straighten_exec_chain(bp, 'EventGraph', begin_play_guid, 'then')
```

**Stops at**:
- A branch (multiple exec-out pins with links) — call again on each branch
- A merge (exec-in pin with > 1 links)
- A node with 0 exec-out links (end of chain)

This is the missing piece for the "clean straight rail" look — `auto_layout_graph` operates on layer-center Y, not pin-level Y, so exec pins don't line up pixel-perfect until this runs.

### set_comment_box_color(blueprint_path, graph_name, node_guid, color_or_preset) -> bool

Set a comment box's background color. Accepts a hex string (`"#RRGGBB"` / `"#RRGGBBAA"`, `#` optional) or one of these preset names:

| Preset | Color | Use for |
|--------|-------|---------|
| `Section` | neutral grey | generic grouping / section headers |
| `Validation` | yellow | input validation / guard clauses |
| `Danger` | red | destructive / irreversible actions |
| `Network` | purple | multiplayer / replication |
| `UI` | teal | HUD / UMG / viewport interactions |
| `Debug` | green | temporary debug prints, breakpoint zones |
| `Setup` | blue | initialization / construction |

```python
comment = L.add_comment_box(bp, g, [n1, n2, n3], '1. Validate inputs', 0, 0, 0, 0)
L.set_comment_box_color(bp, g, comment, 'Validation')
```

Returns `False` if the node isn't a Comment, or the color/preset didn't parse.

### set_node_color(blueprint_path, graph_name, node_guid, color_or_preset) -> bool

Tag an individual node with a visible color cue. **UE doesn't expose a true per-node background tint at the base class level**, so this helper sets a labelled comment bubble on the node with the color/preset name as the label — enough for a human or an agent re-reading the graph to see which functional group a node belongs to.

For real visual grouping, prefer `add_comment_box` + `set_comment_box_color` over per-node tagging.

```python
L.set_node_color(bp, g, damage_call_guid, 'Danger')  # labels the node
```

### auto_insert_reroutes(blueprint_path, graph_name) -> int

Walk every wire; if a wire's straight line from source-pin to destination-pin crosses any other node's bounding box, insert a reroute knot at a clear Y above the obstruction and re-route the wire through it. Returns the number of knots inserted.

```python
# Run after auto_layout_graph (which can still leave wires passing through
# nodes when a layer's vertical stack is shorter than the one it feeds)
knots = L.auto_insert_reroutes(bp, 'EventGraph')
```

Safe to call multiple times — previously-routed wires don't get re-processed.

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

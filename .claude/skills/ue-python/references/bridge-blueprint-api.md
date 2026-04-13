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

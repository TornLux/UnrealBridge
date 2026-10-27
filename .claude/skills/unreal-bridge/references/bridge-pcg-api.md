# bridge-pcg-api

`unreal.UnrealBridgePCGLibrary` provides inspection, bounded native graph
authoring, component overrides and editor generation. Use the kwargs-only
`unreal_bridge.PCG` wrapper. Graph topology is created through native PCG
methods; never edit private node/edge arrays through Property.

## Native authoring

- `create_pcg_graph(asset_path, save=False)` creates a new project/plugin
  graph. Existing packages and packages outside this project are rejected.
- `add_pcg_graph_node(graph_path, settings_class_path, title, position_x,
  position_y)` adds a concrete `UPCGSettings` class. The result contains
  `object_path` for the node and `settings_path` for its settings object.
- `get_pcg_graph_nodes(graph_path)` returns settings classes, paths, exact
  pin labels and layout, including the input/output nodes.
- `connect_pcg_graph_nodes(graph_path, source_node_path, source_pin,
  target_node_path, target_pin)` adds a native edge. Cross-graph nodes,
  missing/wrong-direction pins, self edges and cycles are rejected;
  an existing identical edge succeeds without duplication. It does not
  replace a connection on an already occupied single-connection input.
- `configure_pcg_static_mesh_spawner(graph_path, node_path, mesh_paths,
  weights, material_overrides, collision_profile_name, cast_shadow=True)`
  validates all assets, weights and the collision profile before replacing
  that spawner's weighted mesh entries. Empty weights use 1 per mesh;
  empty material overrides retain mesh defaults. Mesh count is 1..256.
- `create_pcg_spline_actor(graph_path, actor_label, points, seed=0)` creates
  one editor actor with a registered root spline and PCG component. Points
  are finite world-space cm, 2..10000. Labels must be unique. Native spline
  point types are Linear. Generation is on demand, with no automatic run.
- `set_pcg_spline_points(actor_label, component_name, points,
  closed_loop=False)` changes the input through native spline APIs and
  refreshes spline caches. Empty component name selects the first spline.
  PCG input is dirtied, but no cleanup or generation is performed.
- `set_pcg_component_graph(actor_label, component_name, graph_path)` uses
  native SetGraph so parameter layouts and delegates are initialized.
  Empty component name selects the first PCG component. Do not assign the
  nested Graph pointer through Level/Property as a substitute.
- `get_pcg_actor_mesh_outputs(actor_label)` reads meshes, material paths,
  collision profiles, bounds and actual instance counts owned by the actor.
  This includes its own authored components, excludes attached actors and
  is not a claim of managed-resource ownership. A Generated flag alone
  does not prove that any meshes were emitted.

Authoring results expose `success`, `error`, `object_path`, `settings_path`,
`actor_label` and `component_name`; check success before consuming paths.
Settings can be inspected/configured through the Property wrapper using the
returned settings path after checking their reflected fields and native
semantics. Save finished graph assets with `Editor.save_asset`.

`Editor.save_current_level_as(asset_path, overwrite=False)` saves the current
editor level to a project/plugin package path without saving unrelated dirty
assets. The path has no object suffix. Existing targets are rejected unless
overwrite is explicitly requested; use `save_current_level` for later saves.

Graph and spline changes are undoable. Creation/spline editing require an
editor world without PIE. Engine/plugin reference graphs outside this project
are available for inspection/binding, not topology edits.

Hard contract:
- **UE 5.7+ only.** Older engines compile to logged-warning stubs.
- **Editor-world only.** PIE/runtime PCG is the PCG plugin's own concern.
- ⚠ **`wait_for_pcg_generate` cannot be used to actually wait.** Its 50ms poll
  loop runs on the GameThread, so PCG can't tick during the wait — a freshly-
  triggered generation will sit at `generating=True` for the full timeout. The
  function only works for the "is it done already?" use case (where generation
  completed before the call). For genuine waits, **poll across separate
  bridge execs** from Python — the GT drains between calls. See "wait
  patterns" section below.
- `set_pcg_component_override` uses `Property->ImportText`. Empty inputs are
  rejected (UE's numeric-property ImportText silently writes 0 on garbage,
  so the bridge enforces non-empty + snapshot-restores the prior value when
  the parser refuses to consume any chars).

## Inspection and generation

- `list_pcg_graph_assets(filter, max=200) -> list[str]` (L3-1)
- `list_pcg_components_in_level(level_filter, max=200) -> list[FBridgePCGComponentEntry]` (L3-2)
- `get_pcg_component_state(actor_label, component_name) -> FBridgePCGComponentState` (L3-3)
- `get_pcg_component_overrides(actor_label, component_name) -> list[FBridgePCGOverrideEntry]` (L3-4)
- `set_pcg_component_override(actor_label, component_name, name, exported_value) -> bool` (L3-5)
- `trigger_pcg_generate(actor_label, component_name, b_force=False) -> bool` (L3-6)
- `wait_for_pcg_generate(actor_label, component_name, timeout_sec=60.0) -> FBridgePCGWaitResult` (L3-7)
- `cleanup_pcg_component(actor_label, component_name, b_remove_components=False) -> bool` (L3-8)

## USTRUCTs

### FBridgePCGComponentEntry
| Field | Type | Source |
|---|---|---|
| `actor_label` | str | `AActor::GetActorLabel()` |
| `component_name` | str | `UPCGComponent::GetName()` |
| `graph_path` | str | `GetGraph()->GetPathName()` |
| `b_generated` | bool | `bGenerated` |
| `b_generating` | bool | `IsGenerating()` |

### FBridgePCGComponentState
| Field | Type | Source |
|---|---|---|
| `graph_path` | str | as above |
| `b_generated` | bool | as above |
| `b_dirty` | bool | (always False — `bDirtyGenerated` is private; expose if PCG ships a getter) |
| `b_generating` | bool | as above |
| `generated_bounds` | Box | `GetLastGeneratedBounds()` |
| `last_generation_iso` | str | wall-clock at the moment of query (ISO-8601) — a **call timestamp**, not the actual last gen time (PCG doesn't expose that yet) |

### FBridgePCGOverrideEntry
| Field | Type | Source |
|---|---|---|
| `name` | str | property bag entry name |
| `type_str` | str | `Property->GetCPPType()` |
| `value_str` | str | `Property->ExportText_Direct(...)` |

### FBridgePCGWaitResult
| Field | Type | Notes |
|---|---|---|
| `b_success` | bool | True iff `IsGenerating()` reported false before `timeout_sec` |
| `elapsed_ms` | float | total wall-time spent polling |
| `note` | str | `"generated"` / `"not generated (no work to do?)"` / `"timeout"` / `"component not found"` |

---

## Per-op contract notes (verified by sub-agent test sweep 2026-05-08)

### list_pcg_graph_assets(filter, max=200)
- Filter is **case-insensitive substring match** against both the asset's
  short name AND its full object path (FString::Contains default behavior).
  `_Default` and `_DEFAULT` return identical results.
- Returns full object paths (`/PCG/.../Foo.Foo`), not short names.
- `max ≤ 0` clamps to 1 (FMath::Max). UE 5.7 ships ~46 graph assets under `/PCG/`.

### list_pcg_components_in_level(level_filter, max=200)
- `level_filter` matches against the level's **package path** (e.g.
  `/Game/Levels/DefaultLevel`), NOT against `Level->GetName()`. Using
  `'PersistentLevel'` returns 0 results despite the level being the
  persistent level. Use `/Game/Levels/<MapName>` form, or pass empty
  string to disable the filter.
- Returns one entry per `UPCGComponent` per actor — actors with multiple
  PCG components produce multiple entries.

### Component name conventions
- `unreal.PCGVolume` actors auto-attach a component named **`'PCG Component'`**
  (with the space — that's `UPCGComponent::GetName()` for the embedded
  component). Pass that string to op calls; the bridge resolves both
  `GetFName()` and `GetName()` matches.
- Pass `component_name=''` to fall back to the first PCG component on the actor.

### cleanup_pcg_component(b_remove_components)
- `b_remove_components=False` (default): "soft cleanup" — purges generated
  outputs, but `state.generated` stays `True` and `state.generated_bounds`
  retains the cached value. This is PCG's documented behavior, not a bridge
  bug.
- `b_remove_components=True`: "hard reset" — flips `state.generated` to
  False. Use this when you want a fresh `bGenerated` flag for repeated
  trigger/cleanup cycles.

### set_pcg_component_override
- Empty `exported_value` is **always rejected** (returns False) regardless
  of the property's type. To set an FString to empty, pass the literal
  exported form `'""'` (Python string of two quote characters).
- On parse failure (no characters consumed by ImportText), the bridge
  restores the pre-call value via a snapshot, then returns False. So:
  **a False return means the property is unchanged**, never silently zeroed.
- Property paths supported (verified): `int32`, `int64`, `float`, `double`,
  `bool`, `FString`, `FName`, `FVector`, `FVector2D`, `FRotator`, enums
  (e.g. `'ECC_WorldStatic'` for `ECollisionChannel`), `TArray<...>` (use
  the `(Item1, Item2)` exported form).

### get_pcg_component_state — `b_dirty` always False
- `UPCGComponent::bDirtyGenerated` is private with no public getter in
  UE 5.7. The field is wired but always reads False. File an upstream
  request if you need it.
- `last_generation_iso` is a **query-time wall-clock marker**, not the
  actual last generation timestamp (PCG doesn't expose that). Empty for
  unknown actors (early return).

## Wait patterns

`wait_for_pcg_generate` is reliable ONLY for "check if already done". For
actual waits, poll from Python across multiple bridge execs:

```python
import time, unreal as u
P = u.UnrealBridgePCGLibrary
P.trigger_pcg_generate('MyActor', 'PCG Component', True)
# subsequent execs — GT is freed between them, so PCG can tick
deadline = time.time() + 60.0
while time.time() < deadline:
    state = P.get_pcg_component_state('MyActor', 'PCG Component')
    if not state.generating:
        break
    time.sleep(0.5)
```

(Each `P.get_pcg_component_state` call is its own bridge exec from your
Python script — between calls, the editor's GameThread runs PCG ticks.)

## End-to-end pattern

```python
import unreal as u
P = u.UnrealBridgePCGLibrary

# Discover PCG content
print(P.list_pcg_graph_assets(filter='', max=20))
comps = P.list_pcg_components_in_level(level_filter='', max=20)
for c in comps:
    print(c.actor_label, c.component_name, c.graph_path, c.b_generated)

# Inspect overrides on the first component
if comps:
    e = comps[0]
    overrides = P.get_pcg_component_overrides(e.actor_label, e.component_name)
    for o in overrides:
        print(o.name, '=', o.value_str, '(', o.type_str, ')')

    # Set one override + regenerate
    P.set_pcg_component_override(e.actor_label, e.component_name, 'Density', '0.5')
    P.trigger_pcg_generate(e.actor_label, e.component_name, b_force=True)
    result = P.wait_for_pcg_generate(e.actor_label, e.component_name, timeout_sec=60.0)
    print(result.b_success, result.elapsed_ms, result.note)

    # Tear down generated content
    P.cleanup_pcg_component(e.actor_label, e.component_name, b_remove_components=True)
```

---

## Pitfalls

- **Property-bag values are strict.** `set_pcg_component_override` wraps
  `Property->ImportText`. For `int` pass `"42"`; for `FVector` pass
  `"(X=1.0, Y=2.0, Z=3.0)"`; for `FString` pass `"\"hello\""`. Wrong
  format → ImportText silently consumes nothing and returns false.
- **`b_dirty` is always false in the current build.** `bDirtyGenerated`
  is a private field on `UPCGComponent` with no public getter; we'd need
  a friend class or a PR upstream. Not blocking for the read+trigger
  use case.
- **Big PCG generations block the bridge for as long as
  `wait_for_pcg_generate` is held.** Fire-and-forget is `trigger` only;
  use a poll loop in Python if you don't want to block.
- **`cleanup_pcg_component(b_remove_components=True)`** purges any
  component the PCG generation registered as managed — this can include
  things outside the immediate output (foliage components, ISMs spawned
  by sub-graphs). Use False unless you know you want a hard reset.

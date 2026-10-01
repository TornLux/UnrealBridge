# UnrealBridge Niagara / VFX API

Module: `unreal.UnrealBridgeNiagaraLibrary`

Preferred kwargs-only wrapper: `from unreal_bridge import Niagara`

This library is the authoring and delivery surface for Niagara Systems and
Emitters, stack modules and inputs, User parameters, renderer configuration,
compile diagnostics, quality audits, reusable VFX presets, and transient
runtime previews. It is intended for complete effects such as weapon ribbons
and beams, directional or radial sparks, layered explosions, and
dissolve/disintegration particle layers.

> **Version gate:** the functional implementation requires UE 5.7+. UE
> 5.3-5.6 retain the reflected library and kwargs wrapper through generated
> safe stubs. Check `Niagara.is_niagara_api_available()` first and read
> `Niagara.get_last_niagara_error()` if it is false. Lower-version calls log
> the actionable `requires UE 5.7+` reason and return safe empty values without
> blocking the plugin build.

> **Discover before writing:** template assets, module scripts, dynamic-input
> scripts, reflected renderer properties, and data-interface properties vary
> by engine version and installed plugins. Obtain their exact paths and names
> from `list_niagara_templates`, `list_niagara_scripts`, and the relevant
> `list_*_properties` call. Never reconstruct a display label as a content path.

> **Compile and simulate:** a successful mutation or saved package is not a
> deliverable effect. Require a clean `compile_niagara_system`, a passing
> `validate_niagara_system`, and a transient preview with the expected active
> emitters and non-zero particle count. Visual review remains necessary for
> timing, silhouette, color, readability, overdraw, and gameplay scale.

## Mandatory delivery workflow

1. Confirm `is_niagara_api_available()` and discover the exact System/Emitter
   templates and module scripts needed by the effect.
2. Work in one dedicated `/Game/...` folder. Record every created asset path
   so validation content can be removed exactly.
3. Create from a suitable template or a `BridgeNiagaraSystemRecipe`. Re-list
   Emitters, modules, inputs, renderers, and User parameters after structural
   edits; stable IDs are returned by the bridge and must not be guessed.
4. Expose art/gameplay controls as `User.*` parameters and prove each intended
   module input reads back as `mode == "Linked"` with the correct
   `value == "User.Name"`.
5. Configure materials, renderer bindings, system/emitter bounds, CPU/GPU
   target, deterministic seed, warmup, and Effect Type deliberately.
6. Require `compile_niagara_system(..., save=True)` to report `success`,
   `valid`, and `ready_to_run`, with zero errors. Review every warning.
7. Require `validate_niagara_system` to pass. Resolve null materials, missing
   GPU bounds, disabled content, empty systems, and budget warnings.
8. Spawn a transient preview and advance it. Check total and per-emitter
   execution state, particle count, memory, and CPU timing. For trails, move
   the component with `set_niagara_preview_transform(..., teleport=False)`
   between advances so the ribbon samples a real path.
9. Perform visual review from the intended camera/gameplay distance. A
   particle-count check proves simulation, not artistic acceptance.
10. Remove all previews, delete validation assets with
    `delete_niagara_asset`, and prove the dedicated Asset Registry path and
    backing directory are empty. Immediately after deletion, a loaded UObject
    may remain visible to an in-memory existence query until GC/restart; the
    authoritative cleanup checks are registry enumeration and disk presence.

## Capability and discovery

| Function | Contract |
|---|---|
| `is_niagara_api_available()` | `True` only when the UE 5.7+ implementation is compiled. |
| `get_last_niagara_error()` | Most recent semantic/path/version diagnostic. Read it immediately after an unexpected `False`, empty list, or failed result. |
| `list_niagara_templates(asset_type, query, max_results)` | Lists discoverable `System` and/or `Emitter` templates with exact asset path, category, description, and tags. `asset_type` accepts `All`, `System`, or `Emitter`. |
| `list_niagara_scripts(usage, query, max_results)` | Lists module, dynamic-input, or function scripts and their version/library metadata. Use the returned path verbatim. |
| `get_niagara_script_info(script_path)` | Reads one script's usage, category, version, visibility, deprecation, and experimental state. |

Engine Niagara templates live under plugin mounts such as `/Niagara/...`, not
`/Game`. Discovery searches all loaded assets, so do not replace it with a
project-only Asset Registry walk.

## Asset lifecycle and recipes

| Function | Notes |
|---|---|
| `create_niagara_system(asset_path, template_system_path, save)` | Creates an initialized blank System or duplicates a discovered System template. `asset_path` is `/Game/Folder/AssetName`, without `.uasset`. |
| `create_niagara_emitter(asset_path, template_emitter_path, add_default_modules_and_renderer, save)` | Creates a standalone Emitter asset or duplicates a template. |
| `create_niagara_system_from_recipe(asset_path, recipe, compile, save)` | Applies warmup, bounds, Effect Type, User parameters, Emitters, modules/inputs, renderers/properties/bindings, then optionally compiles and saves as one operation. |
| `get_niagara_system_info(system_path)` | Valid/ready/dirty state, bounds, warmup, Effect Type, and aggregate Emitter/module/renderer/User-parameter counts. |
| `delete_niagara_asset(asset_path)` | Deletes one exact `/Game` Niagara System or Emitter and destroys previews that reference that System. It refuses non-Niagara assets and non-project paths. |

`BridgeNiagaraSystemRecipe` contains optional `template_path`, warmup, fixed
bounds, `effect_type_path`, `user_parameters`, and `emitters`. Each
`BridgeNiagaraEmitterSpec` contains template path, enabled/local-space/CPU-GPU
settings, deterministic seed, bounds, modules, and renderers. Module inputs use
`BridgeNiagaraInputValue.mode`:

- `Local`: `value` is Unreal export text.
- `Linked`: `value` is a Niagara parameter such as `User.Color`.
- `Dynamic`: `source_path` is a discovered dynamic-input script.
- `Object`: `value` is an object asset path.
- `DataInterface`: `source_path` is a DI class path and `properties` contains
  reflected name/value entries.

Renderer `properties` use reflected export text. Renderer `bindings` map the
exact binding-property name to a Niagara variable. Recipes stop on a failed
step and return the failing semantic stage in `message`; inspect and delete a
partially created validation asset before retrying with corrected input.

## Emitters

| Function | Notes |
|---|---|
| `list_niagara_emitters(system_path)` | Returns stable handle ID, name, source, enabled/local/deterministic state, seed, simulation target, bounds, and stack counts. |
| `add_niagara_emitter(system_path, name, emitter_asset_or_template_path, save)` | Adds a blank initialized Emitter or a discovered template through NiagaraEditor topology synchronization. Returns the new handle ID. |
| `duplicate_niagara_emitter(system_path, emitter_id_or_name, new_name, save)` | Duplicates the selected versioned Emitter and returns the new handle ID. |
| `remove_niagara_emitter(...)` | Removes the handle and its System graph topology. Re-list after removal. |
| `rename_niagara_emitter(...)` | Renames the exact handle and synchronizes overview data. |
| `set_niagara_emitter_enabled(...)` | Enables/disables a handle without deleting it. |
| `set_niagara_emitter_properties(...)` | Sets local space, `CPU`/`GPU`, deterministic seed, and fixed/dynamic bounds. A valid `unreal.Box` is required when fixed bounds are enabled. |

Structural Emitter operations rebuild or remove the corresponding System graph
nodes. This is essential: adding only a handle can compile yet execute as
`Disabled` at runtime.

## Stack modules and inputs

| Function | Notes |
|---|---|
| `list_niagara_modules(system_path, emitter_id_or_name, usage)` | Returns stack order, stable node GUID, exact usage/usage ID, function name/path/version, enabled/deprecated/assignment flags, and input count. Empty Emitter ID includes System and every Emitter. |
| `list_niagara_module_inputs(system_path, module_id, include_hidden)` | Returns unaliased input name, Niagara type, `Default`/`Local`/`Linked`/`Dynamic`/`DataInterface`/`Object` mode, readback value, static/hidden state, and variable GUID. |
| `add_niagara_module(...)` | Adds a discovered module script to `SystemSpawn`, `SystemUpdate`, `EmitterSpawn`, `EmitterUpdate`, `ParticleSpawn`, `ParticleUpdate`, or another supported usage. Negative index appends before the output. |
| `remove_niagara_module(...)` / `set_niagara_module_enabled(...)` | Removes or toggles the exact module node GUID. Re-list after structural changes. |
| `set_niagara_module_input(...)` | Writes a local value using Unreal/Niagara export text. |
| `link_niagara_module_input(...)` | Replaces the override with a linked parameter; readback returns the parameter name, for example `User.BridgeColor`. |
| `set_niagara_module_dynamic_input(...)` | Replaces the input with a discovered dynamic-input script and returns the dynamic node ID. |
| `set_niagara_module_object_input(...)` | Assigns a compatible UObject asset. |
| `set_niagara_module_data_interface_input(...)` | Creates a compatible DI and applies reflected properties atomically. |
| `list_niagara_module_input_object_properties(...)` | Lists editable DI/object properties with current export text. |
| `set_niagara_module_input_object_property(...)` | Changes one reflected property on the existing input object/DI. |
| `reset_niagara_module_input(...)` | Removes the override and restores the module default. |
| `add_niagara_parameter_assignment(...)` | Adds a Set Variables/assignment module for an explicit parameter such as `Particles.Color`. Returns its module GUID. |

Module display names are not stable identities. Always carry the returned
`id`. An input may be hidden because another module switch selects a different
variant; linking a hidden input does not automatically change that switch.
Inspect both the active template behavior and compile result.

Common type aliases accepted by writers include `Float`, `Int`, `Bool`,
`Vec2`, `Vec3`, `Position`, `Vec4`, `Color`, `Quat`, `Material`, `StaticMesh`,
`Texture`, and `Object`. Readback uses engine names such as `NiagaraFloat`,
`NiagaraInt32`, `Vector3f`, and `LinearColor`; the bridge treats the aliases as
the same Niagara type.

## User parameters

Module identifiers returned by module listing/creation and dynamic-input creation
are opaque scoped strings (`emitter-guid/node-guid`, or `system/node-guid`). Pass
them back unchanged. Duplicated emitters can have identical node GUIDs; legacy
bare GUIDs remain supported only when unique and are rejected when ambiguous.
Nested dynamic-input IDs support the same input list/set/link operations as stack
modules. Static switches are included with `static=True`; use their discovered
export-text enum/bool values with `set_niagara_module_input`, then re-list inputs
because changing a switch can expose/hide other inputs. Module insertion rejects
disconnected parameter-map stacks with a diagnostic instead of asserting.

模块 ID 带发射器/系统作用域，必须原样传回；重复的裸 GUID 会被拒绝。动态输入可继续
嵌套配置；静态开关使用列表返回的枚举/布尔导出值，修改后重新查询有效输入。

| Function | Notes |
|---|---|
| `list_niagara_user_parameters(system_path)` | Returns full `User.*` name, type, exported default, and object/DI flags. |
| `add_niagara_user_parameter(system_path, name, type, default_value, save)` | Adds and initializes one exposed parameter. `User.` is added when omitted. |
| `set_niagara_user_parameter_default(...)` | Changes the System default with type-aware parsing. |
| `rename_niagara_user_parameter(...)` | Renames the exposed variable and preserves the parameter store entry. Re-list links and compile afterward. |
| `remove_niagara_user_parameter(...)` | Removes the exposed variable. Resolve or remove links that still target it. |

Exposing a parameter is not enough. The delivery gate is a matching module
input whose readback is `Linked -> User.Name`, followed by a clean compile.

## Renderers, materials, and bindings

| Function | Notes |
|---|---|
| `list_niagara_renderers(system_path, emitter_id_or_name)` | Returns renderer UObject ID, Emitter ID, type, enabled state, material(s), mesh(es), and binding count. |
| `add_niagara_renderer(...)` | Adds `Sprite`, `Ribbon`, `Mesh`, `Light`, `Decal`, or `Component`. Optional material/mesh paths are type checked. |
| `remove_niagara_renderer(...)` / `set_niagara_renderer_enabled(...)` | Removes or toggles the exact renderer ID. |
| `list_niagara_renderer_properties(...)` | Discovers reflected editable properties and export-text values. |
| `get_niagara_renderer_property(...)` / `set_niagara_renderer_property(...)` | Reads/writes one exact reflected property. List first. |
| `set_niagara_renderer_material(...)` | Assigns a compatible material at an explicit material index. |
| `set_niagara_renderer_binding(...)` | Binds a renderer property to a Niagara variable with source mode such as `Particles`. |

Sprite, Ribbon, and Decal renderers need a material. A renderer can compile
with a null material, so `validate_niagara_system(check_materials=True)` is the
required delivery gate. Material authoring itself is handled by the Material
library; compile the material before assigning it to Niagara.

## System settings, compile, and audit

| Function | Notes |
|---|---|
| `set_niagara_system_warmup(system_path, warmup_time, tick_delta, save)` | Sets warmup time/delta and derived tick count. |
| `set_niagara_system_fixed_bounds(system_path, enabled, bounds, save)` | Enables/disables fixed System bounds. Required for reliable GPU culling. |
| `set_niagara_system_effect_type(system_path, effect_type_path, save)` | Assigns or clears a `UNiagaraEffectType`. |
| `compile_niagara_system(system_path, force, wait_for_gpu_shaders, save)` | Requests compile, waits, and returns System/Emitter script diagnostics. `success` requires zero errors, valid System, and ready-to-run state. |
| `get_niagara_compile_diagnostics(system_path)` | Reads current diagnostics without forcing a new compile. Immediately after editor startup, background Niagara compilation may still be pending. |
| `validate_niagara_system(...)` | Audits empty/disabled content, renderer/material state, fixed GPU bounds, compile readiness, and configurable Emitter/module/renderer budgets. |

`BridgeNiagaraCompileMessage` includes severity, Emitter ID, script usage,
node ID, and pin ID. Use those identities to locate the exact stack item. Do
not reduce validation to `System.is_valid`: an effect may be structurally valid
while not ready, over budget, unbounded on GPU, or missing a material.

## Deliverable presets

| Preset | Result and exposed controls |
|---|---|
| `create_weapon_trail_effect(...)` | `Ribbon` creates a looping position-sampled Ribbon with motion/gravity modules removed and default Ribbon material. `Beam` and `StaticBeam` use the engine templates. Exposes `BridgeColor`, `BridgeWidth`, `BridgeLifetime`, and `BridgeSpawnRate`; Beam styles spawn per activation, so SpawnRate remains available for custom spawning. |
| `create_spark_effect(...)` | `Directional` creates a cone burst plus Ribbon follower; `Radial` creates omnidirectional leaders/followers. Exposes and links color, count, velocity strength, lifetime, and gravity. Optional collision is added when the template lacks it. |
| `create_explosion_effect(...)` | Duplicates the multi-layer SimpleExplosion System, optionally adds a real `ShockwaveRibbon` Emitter and Light renderer, and enables fixed bounds. Exposes core color, debris count, duration, mesh scale, and derived `BridgeShockwaveRadius = 250 * scale`. |
| `create_dissolve_effect(...)` | Builds a looping BlowingParticles breakup layer with color, spawn rate (`BridgeCount` is a float rate), lifetime, sphere radius, and wind direction links. `DissolveAmount` is exposed for synchronization with a dissolve-capable source material. |

Preset style tokens are case-insensitive. The presets intentionally use engine
template materials when `material_path` is empty so compile and audit are
immediately valid. Supplying a custom material applies it to compatible
Sprite/Ribbon/Mesh/Decal renderers; the material must support the intended
Niagara usage and visual domain.

The dissolve preset is the particle breakup layer, not a complete source-mesh
shader by itself. A production dissolve also needs a Material parameter driven
in sync with `User.DissolveAmount`, and may need a mesh/skeletal-mesh DI for
surface sampling. Use generic module/DI APIs for project-specific sampling.

## Transient preview and runtime readback

| Function | Notes |
|---|---|
| `spawn_niagara_preview(system_path, transform, auto_activate, reset_on_change)` | Spawns a transient editor-world component and returns a session-local handle. If the System is not ready after cold load, the bridge compiles and waits before spawning. |
| `list_niagara_previews()` / `get_niagara_preview_info(handle)` | Returns transform, active/complete state, total particles/bytes, and per-emitter execution state, particles, bytes, and CPU time. |
| `advance_niagara_preview(handle, seconds, tick_delta)` | Deterministically advances simulation; never use `time.sleep` on the GameThread. |
| `set_niagara_preview_transform(handle, transform, teleport)` | Moves the component. Use `teleport=False` between short advances for weapon trails and movement-dependent modules. |
| `set_niagara_preview_variable(handle, name, type, value)` | Overrides supported scalar/vector/color/quat/object User variables on the component. Generic DI replacement is intentionally rejected; configure DI inputs before spawn. |
| `control_niagara_preview(handle, action)` | `Activate`, `Deactivate`, `StopImmediate`, `Reset`, `Reinitialize`, `Pause`, or `Resume`. |
| `remove_niagara_preview(handle)` / `remove_all_niagara_previews()` | Destroys transient components and releases session handles. Always call before deleting validation assets. |

Preview handles are session-local and disappear on editor restart. Visibility
and distance scalability can complete an off-camera effect; position the
editor camera before spawning when validating culling-sensitive templates.

### Moving Ribbon example

```python
from unreal_bridge import Niagara
import unreal

result = Niagara.create_weapon_trail_effect(
    asset_path="/Game/VFX/Weapons/NS_SwordTrail",
    style="Ribbon",
    color=unreal.LinearColor(1.0, 0.2, 0.02, 1.0),
    width=16.0,
    lifetime=0.4,
    spawn_rate=120.0,
    save=True,
)
assert result.success, result.message

compiled = Niagara.compile_niagara_system(
    system_path=result.asset_path,
    force=True,
    wait_for_gpu_shaders=True,
    save=True,
)
assert compiled.success, compiled.error

audit = Niagara.validate_niagara_system(system_path=result.asset_path)
assert audit.passed, [issue.message for issue in audit.issues]

preview = Niagara.spawn_niagara_preview(
    system_path=result.asset_path,
    transform=unreal.Transform(location=unreal.Vector(-100, 0, 100)),
)
assert preview.success, preview.message

for x in (0.0, 100.0, 200.0):
    Niagara.set_niagara_preview_transform(
        handle=preview.id,
        transform=unreal.Transform(location=unreal.Vector(x, 0, 100)),
        teleport=False,
    )
    Niagara.advance_niagara_preview(
        handle=preview.id,
        seconds=0.05,
        tick_delta=1.0 / 60.0,
    )

runtime = Niagara.get_niagara_preview_info(handle=preview.id)
assert runtime.active and runtime.total_particle_count > 0
Niagara.remove_niagara_preview(handle=preview.id)
```

## Common failures

| Symptom | Cause and fix |
|---|---|
| Added Emitter compiles but runtime state is `Disabled` | The System topology was not rebuilt, or the source Emitter lifecycle is inactive. Use `add_niagara_emitter`; do not mutate handle arrays through raw properties. Re-list and compile. |
| `Linked` input readback shows a node name | Use the current library build. Linked ParameterMapGet outputs read back the actual `User.*` pin name. |
| Preset exposes parameters but they do not affect the effect | Verify every intended module input is `Linked` to the User parameter. Engine type names such as `NiagaraFloat` are aliases of bridge `Float`, but the selected input still has to be semantically active. |
| Preview succeeds with zero particles just after editor startup | Cold-loaded Niagara compilation may still be pending. Current `spawn_niagara_preview` automatically compiles and waits; otherwise call `compile_niagara_system` explicitly and inspect diagnostics. |
| Ribbon has particles but no visible length | The component did not move. Alternate `set_niagara_preview_transform(..., teleport=False)` and short `advance_niagara_preview` calls, or attach the System to the moving weapon/socket in gameplay. |
| Audit reports null material | Assign a valid material with `set_niagara_renderer_material` or recreate the renderer/preset with a material. |
| GPU effect disappears | Set valid fixed System or Emitter bounds and review scalability/Effect Type. |
| Validation folder is registry-empty but `does_asset_exist` is still true in the same call | The deleted UObject can remain loaded until GC/restart. Confirm `list_assets_under_path(...) == []` and `does_asset_exist_on_disk(...) == False`; remove the empty backing folder after those checks. |

# UnrealBridge Material Library API

Module: `unreal.UnrealBridgeMaterialLibrary`

Covers Material / Material Instance / Material Parameter Collection / Material Function introspection. All calls are read-only in this milestone (M1); writes land in M2.

Path conventions:
- Full object paths (`/Game/Foo/MyMat.MyMat`) or package paths (`/Game/Foo/MyMat`) both work — `LoadObject<UMaterialInterface>` resolves either.
- Engine content lives under `/Engine/` and is read-only in the project but safe to introspect.

---

## get_material_instance_parameters(material_path) -> FBridgeMaterialInstanceInfo

Get parameter **overrides** stored directly on a Material Instance (not inherited defaults). Use `get_material_info` if you need the full effective parameter set including master defaults.

```python
info = unreal.UnrealBridgeMaterialLibrary.get_material_instance_parameters(
    '/Engine/EngineMaterials/Widget3DPassThrough_Opaque')
print(f'{info.name} (parent: {info.parent_name})')
for p in info.parameters:
    print(f'  [{p.param_type}] {p.name} = {p.value}')
```

### FBridgeMaterialInstanceInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Material instance name |
| `parent_name` | str | Parent material name |
| `parent_path` | str | Parent material full path |
| `parameters` | list[FBridgeMaterialParam] | Parameter overrides on the MI |

### FBridgeMaterialParam fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Parameter name |
| `param_type` | str | "Scalar" / "Vector" / "Texture" / "DoubleVector" / "RuntimeVirtualTexture" |
| `value` | str | String representation of the value |

---

## get_material_info(material_path) -> FBridgeMaterialInfo

**M1-1.** Full metadata for a `UMaterial` or `UMaterialInstance`. Resolves MI to the underlying base material for master-only fields (domain / `use_material_attributes` / usage flags / expression counts). Parameter lists reflect the **effective defaults** for this asset (master defaults for masters; inherited defaults for MI — use `get_material_instance_parameters` for MI override diff).

Use this as the "what is this material" baseline before any optimization / editing task — catches domain mismatches (Post-Process material treated as Surface) / shading model surprises / usage flag drift / parameter count drift.

```python
info = unreal.UnrealBridgeMaterialLibrary.get_material_info(
    '/Engine/EngineMaterials/WorldGridMaterial')
if not info.found:
    raise RuntimeError(f'material not loadable: {info.path}')

print(f'{info.name}  domain={info.material_domain}  blend={info.blend_mode}')
print(f'  shading_models={list(info.shading_models)}')
print(f'  usage_flags={list(info.usage_flags)}')
print(f'  params: {len(info.scalar_parameters)} scalar, '
      f'{len(info.vector_parameters)} vector, '
      f'{len(info.texture_parameters)} texture, '
      f'{len(info.static_switch_parameters)} static-switch')
print(f'  graph: {info.num_expressions} expressions, '
      f'{info.num_function_calls} MF calls')
```

### FBridgeMaterialInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | `False` if the path could not be loaded. Caller should check this first |
| `name` | str | Asset name (short, no path) |
| `path` | str | Full object path (`...Name.Name`) |
| `is_material_instance` | bool | `True` for `UMaterialInstance`, `False` for `UMaterial` |
| `parent_path` | str | Immediate parent — only set for MI |
| `base_path` | str | Ultimate base `UMaterial` path (same as `path` for masters) |
| `material_domain` | str | `"Surface"` / `"DeferredDecal"` / `"LightFunction"` / `"Volume"` / `"PostProcess"` / `"UI"` / `"RuntimeVirtualTexture"` |
| `blend_mode` | str | `"Opaque"` / `"Masked"` / `"Translucent"` / `"Additive"` / `"Modulate"` / `"AlphaComposite"` / `"AlphaHoldout"` |
| `shading_models` | list[str] | One entry per shading model present on the material. Values: `"Unlit"` / `"DefaultLit"` / `"Subsurface"` / `"PreintegratedSkin"` / `"ClearCoat"` / `"SubsurfaceProfile"` / `"TwoSidedFoliage"` / `"Hair"` / `"Cloth"` / `"Eye"` / `"SingleLayerWater"` / `"ThinTranslucent"` / `"Strata"` / `"FromMaterialExpression"` |
| `two_sided` | bool | Resolved `IsTwoSided()` — MI overrides are honored |
| `use_material_attributes` | bool | Base material uses MaterialAttributes pin (vs. per-output pins) |
| `usage_flags` | list[str] | Enabled usages on the base material. Values match `UMaterial` `bUsedWith*` properties minus the `bUsedWith` prefix: `"SkeletalMesh"` / `"StaticMesh"` / `"ParticleSprites"` / `"BeamTrails"` / `"MeshParticles"` / `"NiagaraSprites"` / `"NiagaraRibbons"` / `"NiagaraMeshParticles"` / `"StaticLighting"` / `"MorphTargets"` / `"SplineMesh"` / `"InstancedStaticMeshes"` / `"GeometryCollections"` / `"Clothing"` / `"GeometryCache"` / `"Water"` / `"HairStrands"` / `"LidarPointCloud"` / `"VirtualHeightfieldMesh"` / `"Nanite"` / `"Voxels"` / `"VolumetricCloud"` / `"HeterogeneousVolumes"` |
| `subsurface_profile` | str | Full path to `USubsurfaceProfile` asset, empty if not set |
| `scalar_parameters` | list[FBridgeMaterialParamDefault] | All scalar params declared on the master, with default values |
| `vector_parameters` | list[FBridgeMaterialParamDefault] | All vector params, default as `"(R=,G=,B=,A=)"` |
| `texture_parameters` | list[FBridgeMaterialParamDefault] | All texture params, default as asset path |
| `static_switch_parameters` | list[FBridgeMaterialParamDefault] | All static-switch params, default as `"True"` / `"False"` |
| `num_expressions` | int | Count of `UMaterialExpression` nodes on the base material graph |
| `num_function_calls` | int | Subset of `num_expressions` that are `MaterialFunctionCall` nodes |

### FBridgeMaterialParamDefault fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Parameter name |
| `param_type` | str | `"Scalar"` / `"Vector"` / `"Texture"` / `"StaticSwitch"` |
| `value` | str | Default value as string (see per-type encoding above) |
| `guid` | FGuid | Parameter expression GUID (stable across renames) |

### Notes

- **Python bool field names** — UE Python strips the `b` prefix from `bool bFoo` USTRUCT fields. Use `.two_sided` not `.b_two_sided`, `.found` not `.b_found`, etc.
- **MI parameter values** — this function returns *defaults*. For MI override values, use `get_material_instance_parameters`. The two are intentionally split so you can diff override vs. inherited cleanly.
- **`shading_models` plural** — a material can expose multiple shading models when the ShadingModel pin is driven by a `ShadingModel` expression; the list contains every one reachable. Static materials list a single entry.
- **`usage_flags`** — only flags set to `True` appear. Empty list ≠ error.
- **Engine default materials** — `/Engine/EngineMaterials/WorldGridMaterial`, `DefaultPostProcessMaterial`, `DefaultDeferredDecalMaterial`, `Widget3DPassThrough_Opaque` are reliable smoke-test targets.

---

## list_material_functions(path_prefix, max_results) -> list[FBridgeMaterialFunctionSummary]

**M1-8.** Enumerate `UMaterialFunction` assets via AssetRegistry. Fast — reads asset tags and only loads the asset when a library category is requested on an exposed function.

```python
# All MFs under /Game, cap 50
mfs = unreal.UnrealBridgeMaterialLibrary.list_material_functions("/Game", 50)
# All Engine MFs, no cap
mfs = unreal.UnrealBridgeMaterialLibrary.list_material_functions("/Engine", 0)
# Whole project, cap 20
mfs = unreal.UnrealBridgeMaterialLibrary.list_material_functions("", 20)
for s in mfs:
    print(f"{s.path}  exposed={s.expose_to_library}  cat={s.library_category}")
```

Parameters:
- `path_prefix` — package path prefix to scope the search (e.g. `"/Game/Materials"`). Pass `""` for all assets.
- `max_results` — cap on the returned list (`0` or negative = no cap).

### FBridgeMaterialFunctionSummary fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Short asset name |
| `path` | str | Full object path |
| `description` | str | Tooltip text (from `UMaterialFunction::Description`) |
| `expose_to_library` | bool | Whether the MF shows up in the Material Function library panel |
| `library_category` | str | First category in `LibraryCategoriesText` if exposed; empty otherwise |

---

## get_material_function(function_path) -> FBridgeMaterialFunctionInfo

**M1-8.** Full metadata for a single `UMaterialFunction`. Walks the MF's expression list, extracts `UMaterialExpressionFunctionInput` / `UMaterialExpressionFunctionOutput` nodes, sorts by `SortPriority`.

```python
info = unreal.UnrealBridgeMaterialLibrary.get_material_function(
    '/Engine/Functions/Engine_MaterialFunctions01/Opacity/CameraDepthFade.CameraDepthFade')
print(f"{info.name} — {info.num_expressions} expressions")
for p in info.inputs:
    print(f"  in [{p.port_type}] {p.name} default={p.default_value} (usePreview={p.use_preview_value_as_default})")
for p in info.outputs:
    print(f"  out {p.name}")
```

### FBridgeMaterialFunctionInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | `False` if the path could not be loaded |
| `name` / `path` / `description` | str | As above |
| `expose_to_library` | bool | — |
| `library_category` | str | First of `LibraryCategoriesText` |
| `inputs` | list[FBridgeMaterialFunctionPort] | Sorted by `sort_priority` ascending |
| `outputs` | list[FBridgeMaterialFunctionPort] | Sorted by `sort_priority` ascending |
| `num_expressions` | int | Total `UMaterialExpression` count in the MF graph |

### FBridgeMaterialFunctionPort fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Input / output pin name |
| `description` | str | Tooltip text |
| `port_type` | str | For inputs: `"Scalar"` / `"Vector2"` / `"Vector3"` / `"Vector4"` / `"Texture2D"` / `"TextureCube"` / `"Texture2DArray"` / `"VolumeTexture"` / `"StaticBool"` / `"MaterialAttributes"` / `"TextureExternal"`. For outputs: empty (the type is determined by whatever is connected upstream in the graph — requires a graph walk, not available at this milestone). |
| `sort_priority` | int | Pin order within the MF (0 = top) |
| `default_value` | str | Inputs only: stringified `PreviewValue` (Scalar → float; Vector2/3/4 → `(X,Y,...)`; StaticBool → `"True"`/`"False"`; texture types → empty). This is the preview value the MF uses for its own preview scene; it is used as the default only when `use_preview_value_as_default` is True. |
| `use_preview_value_as_default` | bool | Inputs only: whether a caller that leaves the pin unconnected gets the preview value |

### Notes

- **FText category display** — `library_category` comes from localized `FText`. In non-English UE editor locales the string may render as mojibake in Git Bash / cmd; the value itself is correct. Inspect in Python with `repr()` or write to a file if needed.
- **Output port types** — intentionally empty in this milestone. M1-2's `get_material_graph` will provide enough graph visibility to resolve output types via upstream tracing; adding type inference here would duplicate work.

---

## list_material_instance_chain(material_path) -> FBridgeMaterialInstanceChain

**M1-5.** Walk MI → parent → ... → UMaterial base. Each layer lists the parameter overrides contributed by *that* layer — useful for debugging which ancestor actually set a value, and for diffing two MIs that share a base.

Accepts either a `UMaterial` (returns a single-element chain) or a `UMaterialInstance` (returns `[MI, ..., parent MIs ..., base UMaterial]`).

```python
chain = unreal.UnrealBridgeMaterialLibrary.list_material_instance_chain(
    '/Engine/EngineMaterials/Widget3DPassThrough_Opaque')
for i, layer in enumerate(chain.layers):
    tag = 'BASE' if layer.is_base_material else 'MI'
    print(f"L{i} [{tag}] {layer.name} — {len(layer.override_parameters)} overrides")
    for p in layer.override_parameters:
        print(f"    [{p.param_type}] {p.name} = {p.value}")
```

### FBridgeMaterialInstanceChain fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | `False` if the path could not be loaded |
| `path` | str | Full path of the leaf asset that was passed in |
| `layers` | list[FBridgeMaterialInstanceLayer] | Ordered leaf → base. Element 0 is the input asset; last element is the ultimate `UMaterial` |

### FBridgeMaterialInstanceLayer fields

| Field | Type | Description |
|-------|------|-------------|
| `name` / `path` | str | This layer's asset |
| `is_base_material` | bool | `True` for the final `UMaterial` leaf of the chain |
| `override_parameters` | list[FBridgeMaterialParam] | Parameters *explicitly set on this layer*. Always empty for the base `UMaterial` (defaults for master parameters are reported by `get_material_info`, not here). Covers all 5 override tables: Scalar / Vector / DoubleVector / Texture / RuntimeVirtualTexture. |

### Notes

- **Depth cap** — chain walks up to 64 layers deep; any deeper is treated as a pathological cycle and truncated. Real MI chains rarely exceed 3-4 layers.
- **Use vs. `get_material_instance_parameters`** — that function reports *only the leaf MI's overrides*. This one reports overrides per layer, useful when you need to answer "which ancestor set this value?"

---

## get_material_parameter_collection(collection_path) -> FBridgeMaterialParameterCollectionInfo

**M1-9.** Read scalar + vector parameters from a `UMaterialParameterCollection` with their default values and stable GUIDs.

```python
info = unreal.UnrealBridgeMaterialLibrary.get_material_parameter_collection(
    '/Landmass/Landscape/BlueprintBrushes/MPC/MPC_Landscape.MPC_Landscape')
for sp in info.scalar_parameters:
    print(f"scalar {sp.name} = {sp.default_value}")
for vp in info.vector_parameters:
    c = vp.default_value
    print(f"vector {vp.name} = ({c.r:.3f}, {c.g:.3f}, {c.b:.3f}, {c.a:.3f})")
```

### FBridgeMaterialParameterCollectionInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | `False` if the path could not be loaded |
| `name` / `path` | str | Asset name / full object path |
| `scalar_parameters` | list[FBridgeMPCScalarParam] | All scalar params |
| `vector_parameters` | list[FBridgeMPCVectorParam] | All vector params |

### FBridgeMPCScalarParam fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Parameter name (as referenced in material graphs) |
| `default_value` | float | |
| `id` | FGuid | Stable across renames; what UE uses internally to resolve params |

### FBridgeMPCVectorParam fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | |
| `default_value` | FLinearColor | Access channels via `.r .g .b .a` |
| `id` | FGuid | |

### Notes

- **MPC at runtime** — these defaults are the *authored* values. At runtime / PIE, the effective values live in `UWorld::GetParameterCollectionInstance(MPC)`. MPC write APIs (design-time default edit + runtime instance mutation) land in M6.

---

## get_material_graph(material_path) -> FBridgeMaterialGraph

**M1-2.** The full expression graph for a `UMaterial` or `UMaterialFunction`: every node with its class, position, caption, pin names, and key properties, plus every wire between expressions, plus — for `UMaterial` — the main-property wiring (BaseColor / Metallic / Normal / WorldPositionOffset / ...).

This is the primary "read the shader" function. Use it before any optimization / edit task to know what you're modifying; use it after an edit to diff what changed.

Connections reference expressions by `MaterialExpressionGuid` and pins by **name** (not index). Output connections reference the material property by enum name (`"BaseColor"` / `"Metallic"` / etc.).

```python
g = unreal.UnrealBridgeMaterialLibrary.get_material_graph('/Game/Materials/M_MyMaster')

# Per-node summary
for n in g.nodes:
    print(f"[{n.class_name}] guid={n.guid}  pos=({n.x},{n.y})")
    print(f"  caption: {n.caption}")
    if n.key_properties:
        print(f"  props: {n.key_properties}")
    print(f"  in={list(n.input_names)}  out={list(n.output_names)}")

# Where the main outputs come from
for c in g.output_connections:
    print(f"{c.dst_property_name} <- src_guid={c.src_guid} ({c.src_output_name or 'default'})")

# All expression→expression edges
for c in g.connections:
    print(f"{c.src_guid}[{c.src_output_name or 'default'}] --> "
          f"{c.dst_guid}[{c.dst_input_name}]")
```

### FBridgeMaterialGraph fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | `False` if the path could not be loaded or the asset is neither `UMaterial` nor `UMaterialFunction` |
| `path` | str | Full object path |
| `is_material_function` | bool | `True` if the asset is a `UMaterialFunction` — in that case `output_connections` will be empty |
| `nodes` | list[FBridgeMaterialGraphNode] | All expression nodes, in declaration order |
| `connections` | list[FBridgeMaterialGraphConnection] | Expression → expression wires |
| `output_connections` | list[FBridgeMaterialGraphConnection] | For `UMaterial` only: wires into `BaseColor` / `Metallic` / etc. `dst_guid` is the invalid (all-zero) GUID; `dst_property_name` is the enum name without the `MP_` prefix |

### FBridgeMaterialGraphNode fields

| Field | Type | Description |
|-------|------|-------------|
| `guid` | FGuid | `MaterialExpressionGuid` — stable across reloads, what connections reference |
| `class_name` | str | UE class name with `UMaterialExpression` / `MaterialExpression` prefix stripped (e.g. `"Constant3Vector"`, `"TextureSampleParameter2D"`) |
| `x` / `y` | int | Editor-space position (`MaterialExpressionEditorX` / `Y`) |
| `caption` | str | First line of `UMaterialExpression::GetCaption()` — what the node displays as its title (class-dependent; subclasses override) |
| `desc` | str | The comment set on the node (`UMaterialExpression::Desc`) |
| `input_names` | list[str] | Input pin names. `"None"` (string) = anonymous default input |
| `output_names` | list[str] | Output pin names. Single-output nodes typically list `["None"]`. Multi-output nodes like `TextureSample` list `["RGB", "R", "G", "B", "A", "RGBA"]` |
| `key_properties` | str | Class-specific k=v pairs (semicolon-separated) covering the most relevant fields. See "Supported key_properties" below |

### FBridgeMaterialGraphConnection fields

| Field | Type | Description |
|-------|------|-------------|
| `src_guid` | FGuid | Source expression `MaterialExpressionGuid` |
| `src_output_name` | str | Source pin name (empty or `"None"` if the default output) |
| `src_output_index` | int | Source pin index (secondary — prefer name) |
| `dst_guid` | FGuid | Destination expression guid. **Invalid (all-zero) GUID** for entries in `output_connections` — the destination is the material property itself |
| `dst_input_name` | str | Destination pin name |
| `dst_input_index` | int | Destination pin index (secondary — prefer name) |
| `dst_property_name` | str | For `output_connections` only: `"BaseColor"` / `"Metallic"` / `"Roughness"` / `"Normal"` / `"EmissiveColor"` / `"Opacity"` / `"OpacityMask"` / `"WorldPositionOffset"` / `"AmbientOcclusion"` / `"Refraction"` / `"PixelDepthOffset"` / `"SubsurfaceColor"` / `"Tangent"` / `"Anisotropy"` / `"CustomizedUVs0..7"` / `"MaterialAttributes"` / etc. Empty for expression→expression edges |

### Supported `key_properties`

| Expression class | Fields extracted |
|---|---|
| `ScalarParameter` | `ParameterName`, `DefaultValue`, `Group`, `SortPriority` |
| `VectorParameter` | `ParameterName`, `DefaultValue` as `(R,G,B,A)`, `Group` |
| `StaticSwitchParameter` | `ParameterName`, `DefaultValue` as `True`/`False` |
| `StaticBoolParameter` | `ParameterName`, `DefaultValue` |
| `Constant` | `R` |
| `Constant2Vector` | `R`, `G` |
| `Constant3Vector` | `Constant` as `(R,G,B)` |
| `Constant4Vector` | `Constant` as `(R,G,B,A)` |
| `MaterialFunctionCall` | `MaterialFunction` asset path |
| `TextureSample` | `Texture` asset path, `SamplerType` |
| `TextureSampleParameter2D` (and subclasses) | `ParameterName`, `Texture`, `SamplerType` |
| `Comment` | `Text` (truncated 80 chars), `Size` as `WxH` |
| `Custom` | `Description`, `CodeLen`, `OutputType`, `NumInputs` (HLSL code body itself not dumped — use a dedicated future call when that is needed) |
| `FunctionInput` | `InputName`, `InputType`, `SortPriority` |
| `FunctionOutput` | `OutputName`, `SortPriority` |
| (other classes) | empty — rely on `caption` |

### Notes

- **Name vs index for pins** — connections always carry both. Prefer name when diffing/applying since reordering pins on a node would shift indices but keep names.
- **`dst_guid` validity** — for `output_connections` the destination is the material itself, not an expression, so `dst_guid` is the invalid GUID `00000000-0000-0000-0000-000000000000`. Python check: `str(c.dst_guid) == '00000000-0000-0000-0000-000000000000'` or `not c.dst_guid.is_valid()` (if available). In practice callers distinguish by looking at `dst_property_name` being non-empty vs. empty.
- **Reroute nodes** — appear in `nodes` as class `Reroute`. Their single input/output preserves type; walking through them is the consumer's responsibility.
- **Material functions inside** — `MaterialFunctionCall` nodes appear once in the parent graph; the function's internal expressions are *not* inlined. Call `get_material_function` / `get_material_graph` on the referenced path for the inner graph.
- **Main-output properties** — the full enum covers masters' properties (`BaseColor` / `Metallic` / `Roughness` / `Normal` / `EmissiveColor` / `Opacity` / `OpacityMask` / `WorldPositionOffset` / `Refraction` / `AmbientOcclusion` / `PixelDepthOffset` / `SubsurfaceColor` / `Tangent` / `Anisotropy` / `FrontMaterial` / `MaterialAttributes` / `CustomizedUVs0..7` / `CustomData0..1`). Only entries that are actually wired up appear in `output_connections`.

---

## get_material_stats(material_path, feature_level, quality) -> FBridgeMaterialStats

**M1-3 (+ M1-4).** Returns shader-level statistics (per-variant instruction counts + VT stack count) **and** the most recent compile errors for a given feature level + quality combination. Requires the material's shader map to be compiled — returns `shader_map_ready=False` if not.

```python
s = unreal.UnrealBridgeMaterialLibrary.get_material_stats(
    '/Game/Materials/M_MyMaster', 'SM5', 'High')
print(f"FL={s.feature_level} Q={s.quality_level} ready={s.shader_map_ready}")
for sh in s.shaders:
    print(f"  [{sh.shader_type}] {sh.shader_description} = {sh.instruction_count} instructions")
for e in s.compile_errors:
    print(f"  ERR: {e}")
```

Parameters:
- `feature_level` — `"SM5"` / `"SM6"` / `"ES3_1"` / `"Default"` (uses `GMaxRHIFeatureLevel` — the editor's current).
- `quality` — `"Low"` / `"Medium"` / `"High"` / `"Epic"` / `"Default"` (== `"High"`).

### FBridgeMaterialStats fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | `False` if the path did not load |
| `path` | str | Full asset path |
| `feature_level` | str | Resolved feature level (useful to confirm what `"Default"` mapped to) |
| `quality_level` | str | Resolved quality level |
| `shader_map_ready` | bool | `False` while the material is still compiling. If you hit this, wait for `unreal.SystemLibrary.is_material_shader_compiling()` to return `False` and retry |
| `shaders` | list[FBridgeMaterialShaderStat] | One entry per representative shader variant (de-duplicated). Empty for MI whose parent's shader map isn't materialized under this feature level, and for materials still compiling |
| `virtual_texture_stack_count` | int | `FMaterialResource::GetNumVirtualTextureStacks()` — how many VT sample stacks the material compiled into |
| `compile_errors` | list[str] | Errors reported by the shader map. Empty = clean compile |

### FBridgeMaterialShaderStat fields

| Field | Type | Description |
|-------|------|-------------|
| `shader_type` | str | Variant bucket: `"Stationary surface"` / `"Stationary surface + CSM"` / `"Stationary surface + Point Lights"` / `"Dynamically lit object"` / `"Static Mesh"` / `"Skeletal Mesh"` / `"Skinned Cloth"` / `"Nanite Mesh"` / `"UI Pixel Shader"` / `"UI Vertex Shader"` / `"UI Instanced Vertex Shader"` / `"Runtime Virtual Texture Output"` |
| `shader_description` | str | Longer human description — typically `"Base pass shader"`, `"Base pass vertex shader"`, etc. |
| `instruction_count` | int | `FMaterialShaderMap::GetMaxNumInstructionsForShader` — the representative instruction count UE's Material Editor Stats window displays |
| `extra_statistics` | str | Reserved for future platform-specific counters (currently always empty) |

### Notes

- **MI stats** — Material Instances that don't have their own static-switch permutation compiled will return `shaders=[]`. Resolve via `list_material_instance_chain` and call on the parent, or set a PIE cook platform that materializes the MI's permutation.
- **Default quality** — `"Default"` maps to `High`; this matches what the Material Editor Stats window uses.
- **Consistency with Material Editor** — the numbers match the Stats panel in the UE Material Editor (same `GetRepresentativeInstructionCounts` path reimplemented locally since the original is not ENGINE_API).
- **Budget check pattern** — for per-template budget enforcement:
  ```python
  stats = unreal.UnrealBridgeMaterialLibrary.get_material_stats(path, 'SM5', 'High')
  base_pass = [s for s in stats.shaders if 'Base pass shader' == s.shader_description.strip()]
  if base_pass and base_pass[0].instruction_count > 250:
      raise RuntimeError(f"over PS budget: {base_pass[0].instruction_count} > 250")
  ```

---

## get_material_compile_errors(material_path, feature_level, quality) -> list[str]

**M1-4.** Just the compile errors — cheaper than `get_material_stats` when you only need a yes/no on whether the material is broken.

```python
errors = unreal.UnrealBridgeMaterialLibrary.get_material_compile_errors(
    '/Game/Materials/M_MyMaster', 'Default', 'Default')
if errors:
    print('material has compile errors:')
    for e in errors:
        print(f'  {e}')
```

Parameters match `get_material_stats`. Returns:
- Empty list — material compiled cleanly (or no shader map yet)
- Non-empty — each entry is a compile error as reported by the shader map
- On load failure — a single synthetic entry starting with `"UnrealBridge: could not load material"`

---

## preview_material(material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path) -> bool

**M1-6.** Render a preview of a material on a standard mesh in an isolated `FPreviewScene` (no dependency on the active level, PIE, or any actors) and save to PNG. Uses `ASceneCapture2D` with `SCS_FinalColorLDR`, so the PNG is what you'd see in the viewport — full PBR shading, post-processing, tonemapping.

```python
import os, unreal
out = os.path.join(unreal.Paths.project_dir(), "temp/previews/my_mat.png").replace("\\", "/")
ok = unreal.UnrealBridgeMaterialLibrary.preview_material(
    "/Game/Materials/M_Armor",
    mesh="sphere", lighting="studio",
    resolution=512,
    camera_yaw_deg=30.0, camera_pitch_deg=15.0,
    camera_distance=0.0,        # 0 = auto-fit to mesh bounds
    out_png_path=out)
assert ok, "render failed"
```

Parameters:
- `mesh` — `"sphere"` / `"plane"` / `"cube"` / `"cylinder"` / `"cone"` (engine basic shapes), or `"shaderball"` = UE's `/Engine/EngineMeshes/SM_MatPreviewMesh_01` — the canonical material-preview mesh with rounded box + cutouts + flat region + curved surface. **Strongly recommended for metallic / glossy / anisotropic materials** — a plain sphere can't show the geometric variation that makes PBR readable.
- `lighting` — `"studio"` (3-point directional, default), `"hdri"` (uses `/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1` — the cubemap UE's own Material Editor viewport uses, gives proper HDR reflections for metals/mirrors), `"night"` (dim). Unknown name falls back to studio.

**Metal preview recipe.** `mesh="shaderball", lighting="hdri"` is the closest approximation to how the material looks in UE's own Material Editor preview viewport. Flat-only meshes + studio lighting tend to make metallic materials look plasticy because there's nothing interesting in the reflection.
- `resolution` — pixel square edge. Clamped to `[32, 4096]`. Default effective = 512.
- `camera_yaw_deg` — orbit azimuth (Z rotation). 0 = front, 90 = right.
- `camera_pitch_deg` — orbit elevation. 0 = horizontal, +20 typical for slight tilt-down, +90 top-down, -20 slight tilt-up.
- `camera_distance` — cm from mesh center. **0 = auto**: Radius / tan(FOV/2) at a 35° FOV, which frames the mesh tightly.
- `out_png_path` — relative paths resolve against the project root; absolute paths are used as-is.

Returns `True` on success, `False` on any failure (logs via `LogTemp` with details).

### Notes

- **PreviewScene isolation** — creating the scene, rendering, and destroying all happen inside one GameThread call. Previous renders don't linger. Does *not* touch the editor viewport or any opened level.
- **Throughput** — about 50-100 ms per 512² render on typical hardware. Batching many is fine but each render is a synchronous GameThread stall (expected; you called a sync preview).
- **Auto distance** — `camera_distance=0` frames the mesh radius at 35° FOV. For MI variants you want at identical framing, pass a fixed distance.
- **Material instance compile** — when previewing an MI with a static switch or permutation that hasn't been cooked for the preview feature level, the first render triggers a compile; call once to warm it up, then batch subsequent renders.
- **Expected usage** — pairs with `get_material_stats` for numerical cost and `preview_material_complexity` for heat-map style.

---

## preview_material_complexity(...) -> bool

**M1-7.** Same signature as `preview_material`, but toggles the `ShaderComplexity` show flag on the capture. Renders the shader-complexity visualization rather than the normal shaded output.

**Known limitation (UE 5.7).** `SceneCapture2D`'s simplified renderer only partially honors the `ShaderComplexity` view mode — the flag takes effect (output clearly differs from `preview_material`) but the full green→red heatmap may not render for all material configurations. The quantitative complexity signal lives in `get_material_stats` (instruction counts) and `get_material_graph` (node / edge / sampler inventory). Treat this as a qualitative hint, not a definitive metric.

```python
ok = unreal.UnrealBridgeMaterialLibrary.preview_material_complexity(
    "/Game/Materials/M_Armor",
    "sphere", "studio", 512, 30.0, 15.0, 0.0, out_path)
```

Returns same contract as `preview_material`.

---

# Write operations (M2)

All write ops mutate the asset in memory and mark its package dirty. Call `compile_material(path, save=True)` to flush the changes to disk and trigger shader compilation.

Connections between expressions reference **guids and pin names** (not indices). Pin names come from `get_material_graph` → `input_names` / `output_names`. `""` and `"None"` both mean the default anonymous pin — if an expression has a single output / input, pass empty and it resolves to index 0.

## create_material(path, domain, shading_model, blend_mode, two_sided, use_material_attributes) -> FBridgeCreateAssetResult

**M2-1.** Create a new `UMaterial` asset.

```python
r = unreal.UnrealBridgeMaterialLibrary.create_material(
    "/Game/Materials/M_NewMaster",
    domain="Surface",
    shading_model="DefaultLit",
    blend_mode="Opaque",
    two_sided=False,
    use_material_attributes=False)
if not r.success:
    raise RuntimeError(r.error)
print("created:", r.path)
```

| Param | Accepted values |
|---|---|
| `domain` | `"Surface"` / `"DeferredDecal"` / `"LightFunction"` / `"Volume"` / `"PostProcess"` / `"UI"` / `"RuntimeVirtualTexture"` |
| `shading_model` | `"DefaultLit"` / `"Unlit"` / `"Subsurface"` / `"PreintegratedSkin"` / `"ClearCoat"` / `"SubsurfaceProfile"` / `"TwoSidedFoliage"` / `"Hair"` / `"Cloth"` / `"Eye"` / `"SingleLayerWater"` / `"ThinTranslucent"` / `"FromMaterialExpression"` |
| `blend_mode` | `"Opaque"` / `"Masked"` / `"Translucent"` / `"Additive"` / `"Modulate"` / `"AlphaComposite"` / `"AlphaHoldout"` |

### FBridgeCreateAssetResult fields
| Field | Type | Description |
|---|---|---|
| `success` | bool | True on successful creation |
| `path` | str | Full object path of the new asset (populated on success) |
| `error` | str | Human-readable error message on failure |

Fails if the path is already occupied — delete the existing asset first (`unreal.EditorAssetLibrary.delete_asset(path)`) or pick a fresh name.

---

## create_material_instance(parent_path, instance_path) -> FBridgeCreateAssetResult

**M2-2.** Create a `UMaterialInstanceConstant` with the given parent (itself a master or another MI).

```python
unreal.UnrealBridgeMaterialLibrary.create_material_instance(
    "/Game/Materials/M_NewMaster",
    "/Game/Materials/MI_Variant_Red")
```

Same `FBridgeCreateAssetResult` contract as `create_material`.

---

## add_material_expression(material_path, expression_class, x, y) -> FBridgeAddExpressionResult

**M2-4.** Add a single expression node.

`expression_class` accepts three forms (all equivalent):
- Short name: `"Constant3Vector"`, `"ScalarParameter"`, `"TextureSampleParameter2D"` (prefix auto-added)
- Prefixed: `"MaterialExpressionConstant3Vector"` / `"UMaterialExpressionConstant3Vector"`
- Full path: `"/Script/Engine.MaterialExpressionConstant3Vector"`

```python
r = unreal.UnrealBridgeMaterialLibrary.add_material_expression(
    mat_path, "Constant3Vector", x=-300, y=-100)
print(r.guid)     # stable MaterialExpressionGuid — pass this to connect ops
```

### FBridgeAddExpressionResult fields
| Field | Type | Description |
|---|---|---|
| `success` | bool | |
| `guid` | FGuid | `MaterialExpressionGuid` of the new node — stable across save/load |
| `resolved_class` | str | The actual UE class that was instantiated (confirms the name resolved correctly) |
| `error` | str | |

---

## connect_material_expressions(material_path, src_guid, src_output, dst_guid, dst_input) -> bool

**M2-5.** Connect output pin `src_output` of one expression to input pin `dst_input` of another. Pin names are from `get_material_graph`. Empty string or `"None"` = default.

```python
L.connect_material_expressions(mat, tex.guid, "RGB", lerp.guid, "A")
```

Returns False if either guid is missing or the pin names don't resolve.

---

## disconnect_material_input(material_path, dst_guid, dst_input) -> bool

**M2-5.** Clear one input wire on an expression. Returns True if a connection existed and was cleared.

---

## connect_material_output(material_path, src_guid, src_output, property_name) -> bool

**M2-6.** Wire an expression into one of the main material outputs.

`property_name`: `"BaseColor"` / `"Metallic"` / `"Roughness"` / `"Normal"` / `"EmissiveColor"` / `"Opacity"` / `"OpacityMask"` / `"WorldPositionOffset"` / `"Refraction"` / `"AmbientOcclusion"` / `"PixelDepthOffset"` / `"SubsurfaceColor"` / `"Tangent"` / `"Anisotropy"` / `"FrontMaterial"` / `"MaterialAttributes"` / `"CustomizedUVs0..7"` / `"CustomData0..1"` — same set that shows up in `get_material_graph` → `output_connections` → `dst_property_name`.

```python
L.connect_material_output(mat, basecolor.guid, "", "BaseColor")
```

---

## disconnect_material_output(material_path, property_name) -> bool

**M2-6.** Clear the wire to a main output. Returns True if a connection existed.

---

## delete_material_expression(material_path, guid) -> bool

**M2-4 companion.** Remove a node by guid. Any wires to/from it go away with it (including main-output wires that sourced from the deleted node).

---

## compile_material(material_path, save_after) -> bool

**M2-11.** Force a recompile of the material's shader map and optionally save the asset.

```python
L.compile_material(mat, save_after=True)
errs = L.get_material_compile_errors(mat, "Default", "Default")
assert not errs, errs
```

The call blocks until asset compilation is complete (`FAssetCompilingManager::FinishAllCompilation`). On completion, `get_material_stats` returns the new instruction counts.

---

### End-to-end example: new PBR master from scratch

```python
L = unreal.UnrealBridgeMaterialLibrary
path = "/Game/Materials/M_Metal_Test"

L.create_material(path, "Surface", "DefaultLit", "Opaque", False, False)

base = L.add_material_expression(path, "Constant3Vector", -400, -100)
rough = L.add_material_expression(path, "ScalarParameter", -400, 50)
metal = L.add_material_expression(path, "ScalarParameter", -400, 200)

L.connect_material_output(path, base.guid,  "", "BaseColor")
L.connect_material_output(path, rough.guid, "", "Roughness")
L.connect_material_output(path, metal.guid, "", "Metallic")

L.compile_material(path, save_after=True)
```

Note: `Constant3Vector` defaults to `(0,0,0)`; `ScalarParameter` defaults to `0`. Use `set_material_expression_property` (M2-7) to set meaningful defaults — otherwise you'll get a black roughness-0 / metallic-0 material (mirror-black). `ScalarParameter` / `VectorParameter` also expose their values as MI overrides after compile, so you can tune per-instance without re-editing the master.

---

## set_material_expression_property(material_path, guid, property_name, value) -> bool

**M2-7.** Set a single UPROPERTY on an expression, by name, using UE's ImportText format (the same format UE uses for `.uasset` text export).

```python
L.set_material_expression_property(mat, bc.guid, "Constant",
    "(R=0.78,G=0.45,B=0.20,A=1.0)")
L.set_material_expression_property(mat, rough.guid, "ParameterName", "Roughness")
L.set_material_expression_property(mat, rough.guid, "DefaultValue", "0.35")
L.set_material_expression_property(mat, tex.guid,  "Texture",
    "/Game/Textures/T_Brick_D.T_Brick_D")
```

### Value format cheat sheet

| Property type | String form |
|---|---|
| `float` / `int32` | `"1.5"` / `"42"` |
| `bool` | `"true"` / `"false"` |
| `FString` / `FName` | `"MyParam"` (no surrounding quotes) |
| `FLinearColor` | `"(R=1.0,G=0.5,B=0.2,A=1.0)"` |
| `FVector` / `FVector2D` | `"(X=1.0,Y=0.5,Z=0.2)"` / `"(X=1.0,Y=0.5)"` |
| Object reference (`UTexture*`, `UMaterialFunction*`, etc.) | Full content path: `"/Game/Textures/T_Base.T_Base"` |
| Enum | Full name: `"SAMPLERTYPE_Color"` (or just `"Color"` when unambiguous) |

### Common target properties by expression class

| Class | Properties |
|---|---|
| `Constant` | `R` |
| `Constant2Vector` | `R`, `G` |
| `Constant3Vector` / `Constant4Vector` | `Constant` (FLinearColor) |
| `ScalarParameter` | `ParameterName`, `DefaultValue`, `Group`, `SortPriority`, `SliderMin`, `SliderMax` |
| `VectorParameter` | `ParameterName`, `DefaultValue` (FLinearColor), `Group` |
| `StaticSwitchParameter` / `StaticBoolParameter` | `ParameterName`, `DefaultValue` |
| `TextureSample` | `Texture`, `SamplerType`, `SamplerSource`, `MipValueMode` |
| `TextureSampleParameter2D` | above + `ParameterName`, `Group` |
| `MaterialFunctionCall` | `MaterialFunction` |
| `Comment` | `Text`, `SizeX`, `SizeY`, `CommentColor` |
| `Custom` | `Code`, `Description`, `OutputType` |
| *any* | `Desc`, `MaterialExpressionEditorX`, `MaterialExpressionEditorY` |

Returns False if the property doesn't exist on the expression class or the value string doesn't parse.

---

## set_material_expression_properties(material_path, guid, properties) -> int

**M2-7.** Batched form — set many properties on one node in one call with a single `PostEditChange` broadcast. More efficient than many single-property calls.

```python
n = L.set_material_expression_properties(mat, scalar.guid, [
    unreal.BridgeExpressionPropSet(name="ParameterName", value="Roughness"),
    unreal.BridgeExpressionPropSet(name="DefaultValue", value="0.35"),
    unreal.BridgeExpressionPropSet(name="Group", value="PBR"),
    unreal.BridgeExpressionPropSet(name="SortPriority", value="1"),
])
assert n == 4
```

### FBridgeExpressionPropSet fields
| Field | Type | Description |
|---|---|---|
| `name` | str | UPROPERTY name |
| `value` | str | ImportText-format value string |

Returns the count of properties that applied successfully. Each property is independent — failures in one don't stop the rest.

---

## add_material_comment(material_path, x, y, width, height, text, color) -> FGuid

**M2-8.** Add a `MaterialExpressionComment` — a resizable framed rectangle that groups / annotates nodes.

```python
L.add_material_comment(mat,
    x=-450, y=-140, width=260, height=430,
    text="PBR core params",
    color=unreal.LinearColor(0.2, 0.7, 1.0, 0.4))
```

Returns the new comment's `MaterialExpressionGuid`, invalid GUID (all-zero) on failure.

---

## add_material_reroute(material_path, x, y) -> FGuid

**M2-8.** Add a `MaterialExpressionReroute` — a transparent single-input / single-output knot used to clean up wire routing (especially useful alongside `auto_layout_material_graph`, M2-9).

Returns the new reroute's `MaterialExpressionGuid`.

---

## create_material_function(path, description, expose_to_library, library_category) -> FBridgeCreateAssetResult

**M2-3.** Create an empty `UMaterialFunction` asset. Populate it later with function-input / function-output / other expression nodes.

```python
r = L.create_material_function(
    "/Game/Materials/MF_BlendNormals",
    description="Detail normal blend helper",
    expose_to_library=True,
    library_category="Bridge/Normals")
```

Same `FBridgeCreateAssetResult` contract as `create_material`.

---

## apply_material_graph_ops(material_path, ops, compile) -> FBridgeMaterialGraphOpResult

**M2-10.** Apply an ordered batch of graph ops in a single call with `$N` back-references to previously-created nodes. Dramatically reduces round-trips when generating template graphs — a 30-node PBR master can ship in one call.

```python
def op(kind, **kw):
    o = unreal.BridgeMaterialGraphOp()
    o.op = kind
    # UE Python renames UPROPERTY 'Property' to attr 'property_' (Python builtin clash).
    rename = {"property": "property_"}
    for k, v in kw.items():
        setattr(o, rename.get(k, k), v)
    return o

ops = [
    op("add", class_name="Constant3Vector", x=-600, y=-100),           # → $0
    op("set_prop", dst_ref="$0", property="Constant",
       value="(R=0.78,G=0.45,B=0.20,A=1.0)"),
    op("add", class_name="ScalarParameter", x=-600, y=60),             # → $2
    op("set_prop", dst_ref="$2", property="ParameterName", value="Roughness"),
    op("set_prop", dst_ref="$2", property="DefaultValue", value="0.35"),
    op("connect_out", src_ref="$0", src_output="", property="BaseColor"),
    op("connect_out", src_ref="$2", src_output="", property="Roughness"),
    op("comment", x=-650, y=-150, width=260, height=260,
       text="PBR core", color=unreal.LinearColor(0.2,0.7,1.0,0.4)),
]
r = L.apply_material_graph_ops(mat, ops, compile=True)
if not r.success:
    raise RuntimeError(f"op {r.failed_at_index}: {r.error}")
```

Supported ops:

| Op | Required fields | Result |
|---|---|---|
| `"add"` | `class_name`, `x`, `y` | new node guid |
| `"comment"` | `x`, `y`, `width`, `height`, `text`, `color` | new comment guid |
| `"reroute"` | `x`, `y` | new reroute guid |
| `"connect"` | `src_ref`, `src_output`, `dst_ref`, `dst_input` | — |
| `"connect_out"` | `src_ref`, `src_output`, `property` | — |
| `"disconnect_in"` | `dst_ref`, `dst_input` | — |
| `"disconnect_out"` | `property` | — |
| `"set_prop"` | `dst_ref`, `property`, `value` (ImportText fmt) | — |
| `"delete"` | `dst_ref` | — |

References in `src_ref` / `dst_ref`:
- `"$N"` — back-reference to the guid produced by op N (0-based index into the same batch)
- A literal guid string — targets an existing node not created in this batch

If `compile=True`, runs `compile_material` (sync, blocks on shader compile) after all ops succeed.

### FBridgeMaterialGraphOpResult fields
| Field | Type | Description |
|---|---|---|
| `success` | bool | All ops applied |
| `ops_applied` | int | Number of ops that ran before success or failure |
| `guids` | list[FGuid] | Same length as input. Each index holds the node guid for `add`/`comment`/`reroute` ops; invalid (all-zero) guid otherwise |
| `failed_at_index` | int | Index of the first failed op, or -1 on success |
| `error` | str | Human-readable failure message |

Ops are executed in order, and failure at index N leaves ops 0..N-1 applied. Re-run the batch after fixing the offending op — existing nodes persist (apply_ops is not transactional).

### Python attribute name gotcha
UE's Python bindings rename UPROPERTY `Property` → attribute `property_` (trailing underscore) because `property` is a Python builtin. When setting ops programmatically, use `o.property_ = "BaseColor"`, not `o.property`.

---

## auto_layout_material_graph(material_path, column_spacing, row_spacing) -> int

**M2-9.** Topologically arrange all expressions: column = BFS distance from the nearest main-output wire, row = ordered within the column. Comments are left alone (their bounds are user-authored). Disconnected / dangling expressions go to a far-left "limbo" column.

```python
moved = L.auto_layout_material_graph(mat, column_spacing=260, row_spacing=140)
print(f"{moved} nodes repositioned")
```

Defaults: column 260 px, row 140 px if either arg is ≤0. Returns the count of repositioned expressions.

### Notes
- **Does not recompile** — positions are editor-only metadata; the material resource is *not* invalidated (this is intentional — `PostEditChange` would force a recompile and temporarily fall back to DefaultMaterial during rendering).
- **Output-nearest = column 0** — the main outputs are conceptually at column `-1`; column-0 expressions are directly wired to `BaseColor` / `Metallic` / etc.; column-1 expressions feed column-0; and so on.
- **Limbo column** — unreachable nodes (no path to a main output) end up at `MaxDepth + 2` so they don't overlap the main flow.

---

## snapshot_material_graph_json(material_path) -> str

**M2-12.** Produce a deterministic JSON snapshot — nodes sorted by guid, connections sorted by source/destination keys, main outputs sorted by property — suitable for stable diffing across edits.

```python
snap_before = L.snapshot_material_graph_json(mat)
# ... make edits ...
snap_after = L.snapshot_material_graph_json(mat)
report = L.diff_material_graph_snapshots(snap_before, snap_after)
print(report)
```

Schema:
```json
{
  "nodes": [ {"guid":..., "class":..., "x":..., "y":..., "desc":..., "key_props":..., "inputs":[...], "outputs":[...]}, ...],
  "connections": [ {"src":..., "src_out":..., "dst":..., "dst_in":...}, ... ],
  "outputs": [ {"property":..., "src":..., "src_out":...}, ... ]
}
```

---

## diff_material_graph_snapshots(before_json, after_json) -> str

**M2-12.** Compare two snapshots and return a human-readable text diff.

Line prefixes:
- `+ node / wire / out` — added
- `- node / wire / out` — removed
- `* node` — same node, key_props changed (parameter default edited, texture swapped, etc.)
- `~ node` — same node, position moved
- `(no changes)` — snapshots identical

---

# HLSL hybrid programming (M2.5)

The bridge ships a shared HLSL snippet library at
`Plugin/UnrealBridge/Shaders/Private/BridgeSnippets.ush`, registered under the virtual
path `/Plugin/UnrealBridge/` at plugin startup via `AddShaderSourceDirectoryMapping`.

From a `UMaterialExpressionCustom` node, `#include` the snippets via
`/Plugin/UnrealBridge/BridgeSnippets.ush` (the `IncludePaths` array) and call any
snippet function by name from the Code body.

### When to use a Custom node (vs. regular graph)

| Scenario | Use |
|---|---|
| Parameter flow that needs MI override | Node graph |
| `StaticSwitchParameter` permutation | Node graph |
| Texture sampling (dependency tracking) | Node graph (`TextureSample`) |
| Material property / ShadingModel wiring | Node graph |
| Noise / SDF / ACES / TBN math | Custom (snippet library) |
| `ddx`/`ddy` / `[branch]` / `[unroll]` | Custom |
| Normal blending, 8+ node algorithm | Custom |

Custom nodes disable UE's per-node constant folding / CSE / DCE for everything
inside them — `set_material_expression_property` with trivial values goes in the
graph, not the Custom body.

---

## add_custom_expression(material_path, x, y, input_names, output_type, code, include_paths, description) -> FBridgeAddExpressionResult

**M2.5-2.** Create a `UMaterialExpressionCustom` node fully configured — named inputs,
HLSL body, output type, include-file list.

```python
r = L.add_custom_expression(
    mat, x=-400, y=-100,
    input_names=["BaseN", "DetailN", "Strength"],
    output_type="Float3",
    code="return BridgeBlendAngleCorrectedNormals(BaseN, DetailN, Strength);",
    include_paths=["/Plugin/UnrealBridge/BridgeSnippets.ush"],
    description="Detail normal blend (RNM)")
# Wire inputs by InputName via the standard connect_material_expressions:
L.connect_material_expressions(mat, base_n.guid,   "", r.guid, "BaseN")
L.connect_material_expressions(mat, detail_n.guid, "", r.guid, "DetailN")
L.connect_material_expressions(mat, strength.guid, "", r.guid, "Strength")
L.connect_material_output(mat, r.guid, "", "Normal")
```

Parameters:

| Name | Type | Description |
|---|---|---|
| `input_names` | list[str] | Names of input pins on the Custom node. Order is preserved. Each name becomes the HLSL variable you reference in `code` |
| `output_type` | str | `"Float1"` / `"Float2"` / `"Float3"` / `"Float4"` / `"MaterialAttributes"` |
| `code` | str | HLSL body — typically a single `return BridgeFoo(In0, In1, ...);` call into a snippet, or an inline expression |
| `include_paths` | list[str] | Virtual paths to `.ush` files to `#include`. Use `"/Plugin/UnrealBridge/BridgeSnippets.ush"` for the shared library |
| `description` | str | Shown in the node title / docstring |

Returns the standard `FBridgeAddExpressionResult` (same as `add_material_expression`).

---

## list_shared_snippets() -> list[FBridgeShaderSnippet]

**M2.5-4.** Enumerate snippets declared in `BridgeSnippets.ush`. Fast — reads the file
and parses `//@snippet` headers without loading snippet bodies.

```python
for s in unreal.UnrealBridgeMaterialLibrary.list_shared_snippets():
    print(f"{s.name:<40}  {s.instruction_estimate:<5}  {s.description}")
    print(f"    {s.signature}")
```

### FBridgeShaderSnippet fields

| Field | Type | Description |
|---|---|---|
| `name` | str | e.g. `"BridgeACESTonemap"` |
| `description` | str | `@desc` line |
| `signature` | str | Full HLSL function signature |
| `min_feature_level` | str | `@fl` tag — `"SM5"` / `"SM6"` / `"ES3_1"` |
| `instruction_estimate` | str | `@inst` tag — rough instruction count as authored |
| `source` | str | Function body. Empty in `list_*` results; populated by `get_shared_snippet` |

---

## get_shared_snippet(name) -> FBridgeShaderSnippet

**M2.5-4.** Same struct as above but with `source` populated with the function body
(useful when an agent wants to read the implementation before deciding to call it).
Returns a struct with empty `name` if the snippet does not exist.

---

## First-wave snippets (BridgeSnippets.ush)

| Name | Signature | Instr. |
|---|---|---|
| `BridgeLuminance` | `float BridgeLuminance(float3 Color)` | ~3 |
| `BridgeUnpackORM` | `float3 BridgeUnpackORM(float3 Packed)` | ~0 |
| `BridgePackORM` | `float3 BridgePackORM(float O, float R, float M)` | ~0 |
| `BridgeACESTonemap` | `float3 BridgeACESTonemap(float3 Color)` | ~10 |
| `BridgeBlendAngleCorrectedNormals` | `float3 BridgeBlendAngleCorrectedNormals(float3 BaseN, float3 DetailN, float Strength)` | ~14 |
| `BridgeDepthFade` | `float BridgeDepthFade(float PixelDepth, float SceneDepth, float FadeDistance)` | ~4 |
| `BridgeDitherLODTransition` | `float BridgeDitherLODTransition(float Opacity, float2 PixelPos)` | ~10 |
| `BridgeHash21` / `BridgeHash31` | `float BridgeHash21(float2 P)` / `BridgeHash31(float3 P)` | ~6 / ~8 |
| `BridgeValueNoise3D` | `float BridgeValueNoise3D(float3 Pos)` | ~60 |
| `BridgeVoronoi2D` | `float BridgeVoronoi2D(float2 UV, float Cells)` | ~70 |

All snippets use `Bridge*` prefix to avoid name collisions with engine or user code
inlined into generated material shaders. To add a new snippet, edit
`Plugin/UnrealBridge/Shaders/Private/BridgeSnippets.ush` following the header-tag
format — the parser picks up the new entry automatically on next
`list_shared_snippets` call (no editor restart needed since the file is read
on demand).

---

# Parameter iteration loop (M6)

All M6 calls work on **Material Instances** (`UMaterialInstanceConstant`). If you need to tweak a Master material's defaults, edit the underlying expressions via M2-7 `set_material_expression_property`; M6 is strictly for MI overrides + MPC defaults.

**Asset-modal deadlock gotcha.** UE shows a "completing asset references" progress modal on some `IAssetTools::CreateAsset` / `save_loaded_asset` calls, and because bridge exec serializes through the GameThread, a modal from one op will deadlock subsequent queued ops. Rule: **one asset-write op per bridge exec** — create, save, and multi-render operations each go in their own `bridge.exec-file` call.

---

## set_mi_params(material_instance_path, params) -> FBridgeMIParamResult

**M6-1.** Batch-write scalar / vector / texture / static-switch override values onto a Material Instance Constant. Each entry validates against the parent material — params that don't exist upstream are reported in `skipped` rather than silently failing.

```python
def ps(name, type, value):
    p = unreal.BridgeMIParamSet(); p.name = name; p.type = type; p.value = value
    return p

r = unreal.UnrealBridgeMaterialLibrary.set_mi_params(
    "/Game/Materials/MI_Bronze_Scratched",
    [ps("Roughness", "Scalar", "0.65"),
     ps("BaseColor", "Vector", "(R=0.78,G=0.45,B=0.2,A=1)"),
     ps("WearMask",  "Texture", "/Game/Textures/T_Wear.T_Wear"),
     ps("UseDetail", "StaticSwitch", "true")])

print(f"applied {r.applied}, skipped {len(r.skipped)}")
for s in r.skipped: print(f"  {s}")
```

### FBridgeMIParamSet fields

| Field | Type | Values |
|---|---|---|
| `name` | str | Parameter name on the parent |
| `type` | str | `"Scalar"` / `"Vector"` / `"Texture"` / `"StaticSwitch"` (aliases: `"bool"`, `"switch"`) |
| `value` | str | ImportText-format: floats as `"0.5"`, colors as `"(R=,G=,B=,A=)"`, texture as full asset path, bool as `"true"`/`"false"` |

### FBridgeMIParamResult fields

| Field | Type | Description |
|---|---|---|
| `success` | bool | True only if every param applied |
| `applied` | int | Count of params that wrote successfully |
| `skipped` | list[str] | Diagnostic messages for failures ("not found on parent" / "vector unparseable" / etc.) |

### UE 5.7 gotcha — ignore the engine's bool return

`UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue` has a documented engine bug where `bResult = false` is returned unconditionally — the value *does* get applied via `SetScalarParameterValueEditorOnly`. This wrapper works around that by probing the parent first and always treating the setter call as advisory; clients don't see the raw bool.

---

## set_mi_and_preview(mi_path, params, mesh, lighting, resolution, yaw, pitch, distance, out_png) -> bool

**M6-2.** `set_mi_params` followed by `preview_material` in one atomic call. The MI is saved after applying (persist the override values), then the preview renders.

```python
ok = unreal.UnrealBridgeMaterialLibrary.set_mi_and_preview(
    mi_path,
    [ps("Roughness", "Scalar", "0.35")],
    "shaderball", "hdri", 768, 30, 15, 0,
    "G:/Claude/UnrealBridge/temp/previews/tweak.png")
```

Useful for interactive parameter tuning — each change is one call, outputs the new image path, ready to display or commit.

---

## sweep_mi_params(mi_path, param_name, values, mesh, lighting, resolution, yaw, pitch, distance, grid_cols, out_grid_path) -> list[str]

**M6-3.** Scan a single Scalar or Vector parameter over a list of values, rendering each into a cell then compositing a grid image. The MI's original override is restored after the sweep so the MI's on-disk state is unchanged.

```python
paths = unreal.UnrealBridgeMaterialLibrary.sweep_mi_params(
    mi_path, "Roughness",
    ["0.05", "0.2", "0.4", "0.6", "0.8", "1.0"],
    mesh="shaderball", lighting="hdri",
    resolution=320,
    camera_yaw_deg=30, camera_pitch_deg=15, camera_distance=0,
    grid_cols=3,      # 2 rows × 3 cols; 0 = auto sqrt(N)
    out_grid_path="G:/.../sweep_roughness.png")

# paths[0] = grid image; paths[1..] = individual cells
```

Parameters:

| Name | Type | Description |
|---|---|---|
| `param_name` | str | Must be a Scalar or Vector parameter on the MI's parent chain |
| `values` | list[str] | ImportText-format values. Scalar: `"0.5"`; Vector: `"(R=,G=,B=,A=)"` |
| `grid_cols` | int | Column count. `0` = auto (ceil of sqrt(N)). Rows inferred |
| `out_grid_path` | str | The composite grid goes here. Per-cell PNGs land alongside with `_N_<value>` suffix |

Returns: `[grid_path, cell_0_path, cell_1_path, ...]`. Empty list on failure.

Non-destructive — the MI's override of `param_name` is snapshot before the sweep and restored after.

---

## set_material_parameter_collection(collection_path, params) -> FBridgeMIParamResult

**M6-4.** Write Scalar / Vector default values on a `UMaterialParameterCollection`. Same `FBridgeMIParamSet` payload as `set_mi_params`, but `type` must be `"Scalar"` or `"Vector"` (the two MPC value kinds).

```python
L.set_material_parameter_collection("/Game/MPC/MPC_Weather", [
    ps("WindStrength", "Scalar", "0.4"),
    ps("SunColor", "Vector", "(R=1,G=0.92,B=0.78,A=1)"),
])
```

---

## diff_mi_params(path_a, path_b) -> str

**M6-5.** Text diff of MI override values. Line prefixes:

- `+ Type Name = value` — override in B that A lacks
- `- Type Name = value` — override in A that B lacks
- `~ Type Name: oldValue -> newValue` — same param, different value
- `(no override differences)` — identical overrides

Walks Scalar / Vector / Texture tables. Does not walk parent chains — only each MI's own override table is compared.

```python
print(L.diff_mi_params(mi_bronze_a, mi_bronze_b))
# ~ Scalar Roughness: 0.5 -> 0.75
# ~ Vector BaseColor: (R=1.0,G=0.2,B=0.2,A=1.0) -> (R=0.3,G=0.3,B=0.35,A=1.0)
```

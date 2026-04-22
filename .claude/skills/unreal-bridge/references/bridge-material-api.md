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
- `mesh` — `"sphere"` / `"plane"` / `"cube"` / `"cylinder"` / `"cone"`. Engine basic shapes from `/Engine/BasicShapes/`. Unknown name falls back to sphere.
- `lighting` — `"studio"` (3-point directional, default) / `"hdri"` (sky + single key, better for reflective PBR) / `"night"` (dim). Unknown name falls back to studio.
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

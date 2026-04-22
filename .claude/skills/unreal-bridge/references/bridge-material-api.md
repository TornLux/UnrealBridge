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

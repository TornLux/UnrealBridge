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

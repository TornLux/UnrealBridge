# UnrealBridge Material Library API

Module: `unreal.UnrealBridgeMaterialLibrary`

## Get Material Instance Parameters

### get_material_instance_parameters(material_path) -> FBridgeMaterialInstanceInfo

Get all parameter overrides on a Material Instance (scalar, vector, texture, etc.).

```python
info = unreal.UnrealBridgeMaterialLibrary.get_material_instance_parameters('/Game/Mat/MI_Character')
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
| `parameters` | list[FBridgeMaterialParam] | Parameter overrides |

### FBridgeMaterialParam fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Parameter name |
| `param_type` | str | "Scalar", "Vector", "Texture", "DoubleVector", "RuntimeVirtualTexture" |
| `value` | str | String representation of the value |

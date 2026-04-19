# UE Python API — Material Operations

## Create Material Instance

```python
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.MaterialInstanceConstantFactoryNew()
mi = tools.create_asset('MI_New', '/Game/Materials', unreal.MaterialInstanceConstant, factory)

# Set parent material
mi.set_editor_property('parent', unreal.load_asset('/Game/Materials/M_Base'))
```

## Scalar / Vector Parameters

```python
import unreal

mi = unreal.load_asset('/Game/Materials/MI_MyInst')

# Set scalar parameter
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.5)

# Set vector parameter
color = unreal.LinearColor(1.0, 0.0, 0.0, 1.0)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, 'BaseColor', color)

# Set texture parameter
tex = unreal.load_asset('/Game/Textures/T_MyTex')
unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, 'DiffuseMap', tex)
```

## Query Material Info

```python
import unreal

mat = unreal.load_asset('/Game/Materials/M_Base')

# List scalar parameter names
params = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
print(params)

# List texture parameter names
tex_params = unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)
print(tex_params)
```

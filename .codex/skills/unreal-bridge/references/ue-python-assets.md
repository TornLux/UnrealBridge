# UE Python API — Asset Operations

## Asset Registry

```python
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()

# List assets in a directory
assets = registry.get_assets_by_path('/Game/MyFolder', recursive=True)
for a in assets:
    print(f"{a.asset_name} ({a.asset_class_path.asset_name})")

# Get asset by path
asset_data = registry.get_asset_by_object_path('/Game/MyFolder/MyAsset.MyAsset')
```

## EditorAssetLibrary / EditorAssetSubsystem

```python
import unreal

# Load
asset = unreal.EditorAssetLibrary.load_asset('/Game/Props/MyMesh')

# Check existence
exists = unreal.EditorAssetLibrary.does_asset_exist('/Game/Props/MyMesh')

# Duplicate
unreal.EditorAssetLibrary.duplicate_asset('/Game/Source', '/Game/Dest')

# Delete (DESTRUCTIVE)
unreal.EditorAssetLibrary.delete_asset('/Game/ToDelete')

# List directory
assets = unreal.EditorAssetLibrary.list_assets('/Game/MyFolder', recursive=True)
```

## AssetTools

```python
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()

# Create asset with factory
factory = unreal.MaterialInstanceConstantFactoryNew()
mi = tools.create_asset('MI_New', '/Game/Materials', unreal.MaterialInstanceConstant, factory)

# Rename
tools.rename_assets([unreal.AssetRenameData('/Game/Old', '/Game/New')])
```

## DataAsset properties

```python
asset = unreal.load_asset('/Game/Data/DA_MyData')
# Read property
val = asset.get_editor_property('my_property')
# Write property
asset.set_editor_property('my_property', new_value)
# List available properties
print(dir(asset))
```

## File search (disk-level)

```python
import unreal, os

target = 'my_asset_name'
for root, dirs, files in os.walk(unreal.Paths.project_content_dir()):
    for f in files:
        if target in f.lower() and f.endswith('.uasset'):
            print(os.path.join(root, f))
```

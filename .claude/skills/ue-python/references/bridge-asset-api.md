# UnrealBridge Asset Library API

Module: `unreal.UnrealBridgeAssetLibrary`

## Asset Search

### search_assets_in_all_content(query, max_results) -> (list[SoftObjectPath], list[str])

Simplified keyword search across all content roots (including plugins). Case-insensitive, supports include/exclude tokens.

Query syntax: `"hero !enemy"` = must contain "hero", must NOT contain "enemy". `"hero &Type=Blueprint"` = also filter by asset class.

```python
paths, tokens = unreal.UnrealBridgeAssetLibrary.search_assets_in_all_content('Interaction', 20)
for p in paths:
    print(str(p))
```

### search_assets(query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path) -> (list[SoftObjectPath], list[str])

Full-featured keyword search with all options.

```python
paths, tokens = unreal.UnrealBridgeAssetLibrary.search_assets(
    'MyAsset',                                          # query
    unreal.BridgeAssetSearchScope.ALL_ASSETS,           # scope
    '',                                                 # class_filter (empty = all)
    False,                                              # case_sensitive
    False,                                              # whole_word
    50,                                                 # max_results
    1,                                                  # min_characters
    ''                                                  # custom_package_path (only for CustomPackagePath scope)
)
```

### search_assets_under_path(content_folder_path, query, max_results) -> (list[SoftObjectPath], list[str])

Search under a specific content folder path.

```python
paths, _ = unreal.UnrealBridgeAssetLibrary.search_assets_under_path('/Game/Characters', 'hero', 20)
```

### EBridgeAssetSearchScope

| Value | Description |
|-------|-------------|
| `ALL_ASSETS` | All content roots including plugins |
| `PROJECT` | `/Game` only |
| `CUSTOM_PACKAGE_PATH` | Custom root (pass via `custom_package_path`) |

---

## Derived Classes

### get_derived_classes(base_classes, excluded_classes) -> set[UClass]

Get all classes derived from the given base classes. Skips hidden, deprecated, abstract, SKEL_, REINST_ classes.

```python
base = [unreal.Actor.static_class()]
excluded = set()
derived = unreal.UnrealBridgeAssetLibrary.get_derived_classes(base, excluded)
for cls in derived:
    print(cls.get_name())
```

### get_derived_classes_by_blueprint_path(blueprint_class_path) -> list[UClass]

Get all classes derived from a Blueprint (by asset path). Accepts content path, object path, or export-text with quotes.

```python
derived = unreal.UnrealBridgeAssetLibrary.get_derived_classes_by_blueprint_path(
    '/Game/BP/BP_BaseEnemy'
)
for cls in derived:
    print(cls.get_name())
```

---

## Asset References

### get_asset_references(asset_path) -> (list[SoftObjectPath], list[SoftObjectPath])

Get all dependencies and referencers of an asset in one call.

```python
deps, refs = unreal.UnrealBridgeAssetLibrary.get_asset_references('/Game/BP/MyActor')
print(f'Dependencies ({len(deps)}):')
for d in deps:
    print(f'  {d}')
print(f'Referencers ({len(refs)}):')
for r in refs:
    print(f'  {r}')
```

---

## DataAsset Queries

### get_data_assets_by_base_class(base_class) -> list[AssetData]

Get all DataAssets of a given base class (recursive).

```python
assets = unreal.UnrealBridgeAssetLibrary.get_data_assets_by_base_class(unreal.MyDataAsset)
for a in assets:
    print(f'{a.asset_name} @ {a.package_name}')
```

Also available: `get_data_assets_by_asset_path(path)`, `get_data_asset_soft_paths_by_base_class(cls)`, `get_data_asset_soft_paths_by_asset_path(path)` — same queries returning different formats.

**Known limitation**: `get_data_asset_soft_paths_by_asset_path` resolves the base class via AssetRegistry tags (without loading the asset). For native C++ DataAsset subclasses (non-Blueprint), the `GeneratedClass` tag does not exist, and the fallback `AssetClassPath` may not match recursive class queries correctly, resulting in 0 results. Use `get_data_assets_by_asset_path` instead — it loads the asset to resolve the class and works reliably for all DataAsset types.

---

## Folder / Path Queries

### list_assets_under_path(folder_path, include_subfolders) -> list[SoftObjectPath]

List all asset soft paths under a content folder.

```python
paths = unreal.UnrealBridgeAssetLibrary.list_assets_under_path('/Game/Characters', True)
```

Also: `list_assets_under_path_simple(path)` — always recursive.

### get_sub_folder_paths(folder_path) -> list[str]

Get immediate sub-folder paths.

```python
subs = unreal.UnrealBridgeAssetLibrary.get_sub_folder_paths('/Game')
```

Also: `get_sub_folder_names(folder_path)` — returns FName instead of FString.

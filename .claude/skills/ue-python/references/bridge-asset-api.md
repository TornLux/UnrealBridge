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

---

## Registry Metadata (no load)

These queries hit the AssetRegistry only — no assets are loaded. They are cheap and safe to call on large sweeps.

### does_asset_exist(asset_path) -> bool

Check whether the AssetRegistry knows about a path. Accepts content path (`/Game/Foo/Bar`), object path (`/Game/Foo/Bar.Bar`), or export-text with quotes.

```python
unreal.UnrealBridgeAssetLibrary.does_asset_exist('/Game/BP/BP_MyActor')  # True/False
```

### get_asset_info(asset_path) -> FBridgeAssetInfo

Read registry-backed metadata: class path, redirector flag, disk size, and every tag key/value pair.

```python
info = unreal.UnrealBridgeAssetLibrary.get_asset_info('/Game/BP/BP_MyActor')
if info.found:
    print(info.package_name, info.asset_name, info.class_path)
    print('Disk size:', info.disk_size, 'bytes')
    for tag in info.tags:
        print(f'  {tag.key} = {tag.value}')
```

#### FBridgeAssetInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `found` | bool | True if the registry returned a valid entry |
| `package_name` | str | e.g. `/Game/BP/BP_MyActor` |
| `asset_name` | str | Leaf name |
| `class_path` | str | `TopLevelAssetPath` string, e.g. `/Script/Engine.Blueprint` |
| `is_redirector` | bool | True if class is `ObjectRedirector` |
| `disk_size` | int | `.uasset`/`.umap` size in bytes, or `-1` if unresolved |
| `tags` | list[FBridgeAssetTag] | All AssetRegistry tag key/value pairs |

#### FBridgeAssetTag fields

| Field | Type |
|-------|------|
| `key` | str |
| `value` | str |

### get_assets_by_class(class_path, search_sub_classes) -> list[SoftObjectPath]

Wraps `IAssetRegistry::GetAssetsByClass` with a plain string `class_path`. Accepts any `TopLevelAssetPath`: engine class (`/Script/Engine.Texture2D`), or a Blueprint generated class (`/Game/BP/BP_MyActor.BP_MyActor_C`).

```python
# All Texture2D assets (no subclasses)
tex = unreal.UnrealBridgeAssetLibrary.get_assets_by_class('/Script/Engine.Texture2D', False)

# All actors derived from a specific Blueprint
derived = unreal.UnrealBridgeAssetLibrary.get_assets_by_class(
    '/Game/BP/BP_BaseEnemy.BP_BaseEnemy_C', True)
```

### get_assets_by_tag_value(tag_name, tag_value, optional_class_path) -> list[SoftObjectPath]

Find every asset whose AssetRegistry tag matches `(tag_name, tag_value)`. Optionally narrow the sweep to a class path (`''` = all classes, recursive).

```python
# All Blueprints whose ParentClass tag is DamageType
same_parent = unreal.UnrealBridgeAssetLibrary.get_assets_by_tag_value(
    'ParentClass',
    "/Script/CoreUObject.Class'/Script/Engine.DamageType'",
    '/Script/Engine.Blueprint')
```

Common registry tags: `ParentClass`, `GeneratedClass`, `NativeParentClass`, `BlueprintType`, plus any `AssetRegistrySearchable` property the asset exposes.

### resolve_redirector(asset_path) -> str

Follow a single redirector hop. Returns the destination object path, or an empty string when the path isn't a redirector.

```python
target = unreal.UnrealBridgeAssetLibrary.resolve_redirector('/Paper2D/DummySpriteTexture')
if target:
    print('Redirects to:', target)
```

For batch redirector cleanup, pair this with `get_assets_by_class('/Script/CoreUObject.ObjectRedirector', False)` and `unreal.UnrealBridgeEditorLibrary.fixup_redirectors(...)`.

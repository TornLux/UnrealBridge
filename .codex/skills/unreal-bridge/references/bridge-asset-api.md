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

> ⚠️ **Token cost: MEDIUM–HIGH for broad bases.** Passing `UObject` / `UActorComponent` / `AActor` will walk the entire class tree. Narrow to the most specific base you care about.

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

> ⚠️ **Token cost: HIGH on broad paths.** No result cap. `list_assets_under_path('/Game', True)` on a production project returns tens of thousands of paths (~60 chars each). **Always scope to a subfolder**, or use `search_assets_under_path(folder, query, max_results)` when you're looking for specific assets.

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

> ⚠️ **Token cost: HIGH for broad base classes.** `search_sub_classes=True` on `/Script/Engine.Object` or `/Script/Engine.Actor` sweeps the whole registry and can return 10k+ paths. Pick the narrowest class you can, and prefer `search_assets` when a name-keyword narrows the set.

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

For batch redirector cleanup, prefer `find_redirectors_under_path(folder, recursive)` (below) plus `unreal.UnrealBridgeEditorLibrary.fixup_redirectors(...)`.

---

## Cheap scalar / batch registry queries

Focused helpers for callers processing many assets. All are registry-only (no load).

### get_asset_class_path(asset_path) -> str

Just the class path of an asset. Cheaper than `get_asset_info` when you only need "what kind of asset is this?". Returns `""` when the registry doesn't know the asset.

> Token footprint: ~1 line/call, ~40 chars. One TCP round-trip per call — batch via `get_assets_by_class` or `get_assets_of_classes` if you're classifying many assets.

```python
unreal.UnrealBridgeAssetLibrary.get_asset_class_path('/Game/BP/BP_MyActor')
# '/Script/Engine.Blueprint'
```

### get_asset_tag_value(asset_path, tag_name) -> str

Read one AssetRegistry tag value from one asset, no load. Returns `""` when the asset is unknown or the tag is absent. Much cheaper than `get_asset_info` when you only want a single tag (e.g. `ParentClass`, `NativeParentClass`, `BlueprintType`).

> Token footprint: ~1 short string. One round-trip per call.

```python
parent = unreal.UnrealBridgeAssetLibrary.get_asset_tag_value(
    '/Game/BP/BP_MyActor', 'ParentClass')
# "/Script/CoreUObject.Class'/Script/Engine.Actor'"
```

### get_assets_by_package_paths(folder_paths, class_filter, recursive) -> list[SoftObjectPath]

Batch list every asset under any of the given content folder paths in a **single** registry sweep. Pass `class_filter=''` for any class, otherwise a TopLevelAssetPath string. `recursive` controls both subfolder descent and recursive class matching.

> Token footprint: HIGH on broad folders. 1 soft path per asset (~60 chars). Prefer this over multiple `list_assets_under_path` calls when you need to OR several disjoint folders — one round-trip instead of N.

```python
AL = unreal.UnrealBridgeAssetLibrary

# All Blueprints under two disjoint folders:
out = AL.get_assets_by_package_paths(
    ['/Game/Characters', '/Game/Weapons'],
    '/Script/Engine.Blueprint',
    True)
```

### get_assets_of_classes(class_paths, search_sub_classes) -> list[SoftObjectPath]

One registry pass returning every asset whose class matches **any** entry in `class_paths`. Replaces N separate `get_assets_by_class` calls when you need multiple types together.

> Token footprint: HIGH for broad bases like `Actor` / `Object`. Narrow the class list aggressively.

```python
# Textures OR Materials in one pass
tex_and_mat = unreal.UnrealBridgeAssetLibrary.get_assets_of_classes(
    ['/Script/Engine.Texture2D', '/Script/Engine.Material'], False)
```

### find_redirectors_under_path(folder_path, recursive) -> list[SoftObjectPath]

Find every `UObjectRedirector` under a folder. Pair with `unreal.UnrealBridgeEditorLibrary.fixup_redirectors(...)` for batch cleanup.

> Token footprint: proportional to redirector count — usually small on healthy projects, can spike after large moves/renames.

```python
AL = unreal.UnrealBridgeAssetLibrary
redirectors = AL.find_redirectors_under_path('/Game', True)
print(f'{len(redirectors)} redirectors to fix up')
# package-level string paths for fixup_redirectors:
pkg_paths = [r.export_text().split('.')[0] for r in redirectors]
```

> Note on `SoftObjectPath`: `str(p)` returns `"<Struct 'SoftObjectPath' ... {}>"` in UE 5.7. Use `p.export_text()` to get the `"/Game/Foo/Bar.Bar"` object path; strip on `'.'` for the package path.

---

## Cheap counts & batched per-asset queries

Follow-up helpers for callers processing many assets. All registry-only, no load.

### get_asset_count_under_path(folder_path, class_filter, recursive) -> int

One registry sweep, returns only the count. Cheap scoping check before deciding whether a folder is small enough to list. `class_filter=''` counts any class; otherwise pass a TopLevelAssetPath.

> Token footprint: 1 integer. 1 round-trip.

```python
AL = unreal.UnrealBridgeAssetLibrary
n_all = AL.get_asset_count_under_path('/Game', '', True)           # e.g. 775
n_bp  = AL.get_asset_count_under_path('/Game', '/Script/Engine.Blueprint', True)  # e.g. 101
```

### get_package_dependencies(package_name, hard_only) -> list[str]

Package-level dependencies as string package names. Pass a package path like `"/Game/BP/BP_MyActor"` (no `.AssetName` suffix). When `hard_only=True`, only `Hard|Game` deps (cooker-relevant); otherwise all categories.

Cheaper and coarser than `get_asset_references` — works at package granularity and returns strings, not soft paths.

> Token footprint: 1 string per dep. 1 round-trip.

```python
deps = unreal.UnrealBridgeAssetLibrary.get_package_dependencies(
    '/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky', False)
print(len(deps), 'deps')
```

### get_package_referencers(package_name, hard_only) -> list[str]

Mirror of `get_package_dependencies`. Who references this package? Useful before rename/delete to gauge breakage.

```python
refs = unreal.UnrealBridgeAssetLibrary.get_package_referencers(
    '/Game/BP/BP_MyActor', False)
```

### get_asset_tag_values_batch(asset_paths, tag_name) -> list[str]

Read a single tag from many assets in one call. Output is aligned 1:1 with input — unknown asset or missing tag yields `""` at that index. Replaces N `get_asset_tag_value` round-trips.

> Token footprint: 1 tag string per asset. 1 round-trip.

```python
AL = unreal.UnrealBridgeAssetLibrary
paths = AL.get_assets_by_class('/Script/Engine.Blueprint', False)
obj_paths = [p.export_text() for p in paths[:100]]
parents = AL.get_asset_tag_values_batch(obj_paths, 'ParentClass')
for path, pc in zip(obj_paths, parents):
    if pc:
        print(path, '->', pc)
```

### get_asset_disk_sizes_batch(asset_paths) -> list[int]

Disk size in bytes for many assets. Output aligned 1:1 with input; unresolved entries are `-1`. Use when you want sizes only and want to avoid N `get_asset_info` calls (each of which returns all tags).

> Token footprint: 1 integer per asset. 1 round-trip. Still O(N) FileManager calls on the GameThread, so keep batches reasonable (thousands is fine; hundreds of thousands is not).

```python
AL = unreal.UnrealBridgeAssetLibrary
paths = AL.list_assets_under_path_simple('/Game/Textures')
obj_paths = [p.export_text() for p in paths]
sizes = AL.get_asset_disk_sizes_batch(obj_paths)
total = sum(s for s in sizes if s > 0)
print('Total texture bytes:', total)
```

---

## Structural / aggregate helpers

Cheap whole-project / whole-folder queries. Useful for scoping, sizing, and deciding whether a folder is worth listing in detail.

### get_content_roots() -> list[str]

Every mounted writable content root visible to the AssetRegistry — `/Game/`, `/Engine/`, and every plugin root. Trailing slashes preserved (UE convention).

> Token footprint: ~1 short string per root. ~94 roots on a typical project with engine plugins.

```python
roots = unreal.UnrealBridgeAssetLibrary.get_content_roots()
project_roots = [r for r in roots if not r.startswith('/Engine')]
```

### does_folder_exist(folder_path) -> bool

True when the AssetRegistry knows the path and it indexes at least one asset (recursively). Cheap pre-flight before listing or sweeping. Accepts trailing slash or not.

> Token footprint: 1 boolean. 1 round-trip.

```python
AL = unreal.UnrealBridgeAssetLibrary
if AL.does_folder_exist('/Game/Characters'):
    paths = AL.list_assets_under_path_simple('/Game/Characters')
```

### get_total_disk_size_under_path(folder_path, class_filter, recursive) -> (int, int)

Aggregate `.uasset`/`.umap` bytes under a folder in one registry sweep. Returns `(total_bytes, asset_count)`. `class_filter=''` sums any class; otherwise pass a TopLevelAssetPath. Filtering also applies `recursive` to subclass matching. Files that can't be sized are skipped silently.

> Token footprint: 2 integers. 1 round-trip + N FileManager stats. Cheaper than `get_asset_disk_sizes_batch` when you only care about the total — no per-asset path traffic.

```python
AL = unreal.UnrealBridgeAssetLibrary
total, count = AL.get_total_disk_size_under_path('/Game/UltraDynamicSky', '', True)
print(f'{total/1e6:.1f} MB across {count} assets')

# Just textures under /Game
tex_total, tex_count = AL.get_total_disk_size_under_path(
    '/Game', '/Script/Engine.Texture2D', True)
```

### get_asset_class_paths_batch(asset_paths) -> list[str]

Batch class-path lookup, 1:1 with input. Unknown assets yield `""`. One registry pass — replaces N `get_asset_class_path` round-trips when classifying many assets.

> Token footprint: 1 short string per asset (e.g. `"/Script/Engine.Blueprint"`). 1 round-trip.

```python
AL = unreal.UnrealBridgeAssetLibrary
paths = AL.list_assets_under_path_simple('/Game/Mixed')
obj_paths = [p.export_text() for p in paths]
classes = AL.get_asset_class_paths_batch(obj_paths)

from collections import Counter
print(Counter(classes).most_common(10))
```

### get_package_dependencies_recursive(package_name, hard_only, max_depth) -> list[str]

Transitive package-dependency closure, bounded by `max_depth` (use `0` for unlimited). Returns the union of all reachable packages excluding the seed. When `hard_only=True`, only `Hard|Game` edges are traversed (cooker-relevant closure).

Use for impact analysis: "what would I have to ship to keep this asset working?" or "how deep does this content tree branch?".

> Token footprint: 1 string per reachable package. **Can explode on broad seeds** — a hub asset with 800-deep transitive closure returns 800 strings. Start with `max_depth=2` or `3` to bound the blast radius before going unlimited.

```python
AL = unreal.UnrealBridgeAssetLibrary
pkg = '/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky'
direct = AL.get_package_dependencies(pkg, False)        # 174
d3     = AL.get_package_dependencies_recursive(pkg, False, 3)  # 439
all_   = AL.get_package_dependencies_recursive(pkg, False, 0)  # 864 (full closure)
```

---

## Asset introspection

Per-asset metadata queries. Answer "what's in this mesh / texture / sound?"
without loading the asset into Python and poking at raw `unreal.*`
properties. Each returns `bFound = false` (Python: `.found`) when the
path can't be resolved, otherwise a fully-populated struct.

### get_static_mesh_info(asset_path) -> FBridgeStaticMeshInfo

Per-LOD vertex / triangle counts + material-slot indices, mesh bounds,
material slot names + bound material asset paths, socket list, UV
channel count (LOD 0), Nanite-enabled flag, collision presence.

```python
sm = AL.get_static_mesh_info('/Game/Meshes/SM_Cylinder.SM_Cylinder')
print(f'LODs={sm.num_lo_ds} UVs={sm.num_uv_channels} nanite={sm.has_nanite_data}')
for lod in sm.lod_stats:
    print(f'  LOD{lod.lod_index}: {lod.vertex_count} verts, {lod.triangle_count} tris')
# LODs=1 UVs=2 nanite=False
#   LOD0: 132 verts, 128 tris
```

> **Python naming quirk.** UE's reflection converts `NumLODs` →
> `num_lo_ds` (the consecutive uppercase splits each char). All other
> `num_*` fields (`num_sockets`, `num_uv_channels`, …) follow the
> normal snake-case rule.

**Fields:** `asset_path`, `found`, `bounds_origin`, `bounds_extent`,
`bounds_sphere_radius`, `num_lo_ds`, `lod_stats[].{lod_index,
vertex_count, triangle_count, material_indices}`, `material_slot_names`,
`material_asset_paths`, `num_sockets`, `socket_names`, `has_collision`,
`num_uv_channels`, `has_nanite_data`.

### get_skeletal_mesh_info(asset_path) -> FBridgeSkeletalMeshInfo

Same LOD-stats pattern as static meshes, plus skeleton binding, bone
count, socket + morph-target counts, and the bound PhysicsAsset path.

```python
sk = AL.get_skeletal_mesh_info('/Game/Characters/MyChar/SK_Hero.SK_Hero')
print(f'LODs={sk.num_lo_ds} bones={sk.num_bones} morphs={sk.num_morph_targets}')
print(f'skeleton={sk.skeleton_path}  physics={sk.physics_asset_path}')
# LODs=4 bones=354 morphs=0
# skeleton=/Game/Characters/MyChar/Hero_Skeleton.Hero_Skeleton
```

**Fields:** same LOD / material / bounds fields as StaticMeshInfo,
plus `skeleton_path`, `num_bones`, `num_morph_targets`,
`physics_asset_path`, `num_sockets`, `socket_names`. Missing:
`has_collision`, `has_nanite_data`, `num_uv_channels`.

### get_texture_info(asset_path) -> FBridgeTextureInfo

Dimensions, pixel format (EPixelFormat name), compression settings,
LOD group, sRGB flag, mip count, never-stream flag, resident memory
bytes.

```python
tx = AL.get_texture_info('/Game/Textures/T_Grid.T_Grid')
print(f'{tx.width}x{tx.height} mips={tx.num_mips} fmt={tx.pixel_format}')
print(f'compression={tx.compression_settings} group={tx.lod_group}')
print(f'sRGB={tx.srgb} never_stream={tx.never_stream} bytes={tx.resource_size_bytes}')
# 1024x1024 mips=11 fmt=PF_DXT1
# compression=TC_Default group=TEXTUREGROUP_World
# sRGB=True never_stream=False bytes=722288
```

**Fields:** `asset_path`, `found`, `width`, `height`, `num_mips`,
`pixel_format` (e.g. `PF_DXT1`, `PF_BC7`, `PF_FloatRGBA`),
`compression_settings` (e.g. `TC_Default`, `TC_NormalMap`),
`lod_group` (e.g. `TEXTUREGROUP_World`, `TEXTUREGROUP_UI`), `srgb`,
`never_stream`, `resource_size_bytes`.

### get_sound_info(asset_path) -> FBridgeSoundInfo

Duration, sample rate, channel count, loop flag, compressed bytes.
Accepts `USoundWave`, `USoundCue`, or any `USoundBase` subclass.
SampleRate / channels are only populated for `SoundWave`; cue / meta-
sound return the computed total duration only.

```python
snd = AL.get_sound_info('/Game/Audio/Ambient/Birds.Birds')
print(f'kind={snd.sound_kind} dur={snd.duration_seconds:.2f}s '
      f'sr={snd.sample_rate}Hz ch={snd.num_channels} loop={snd.looping}')
# kind=SoundWave dur=10000.00s sr=44100Hz ch=2 loop=True
```

**Duration caveat.** A `SoundWave` with `bLooping = true` reports a
sentinel duration (typically `INDEFINITELY_LOOPING_DURATION`) that
looks like 10000 s. Check the `looping` flag before trusting the
duration.

**Fields:** `asset_path`, `found`, `sound_kind` (`"SoundWave"` /
`"SoundCue"` / `"MetaSound"` / other), `duration_seconds`,
`sample_rate`, `num_channels`, `looping`, `compressed_data_bytes`.

# UnrealBridge Editor Library API

Module: `unreal.UnrealBridgeEditorLibrary`

Editor session control: state query, asset open/save, Content Browser, viewport, PIE, undo/redo, console/CVars, redirector fixup, Blueprint compile.

## Editor State

### get_editor_state() -> FBridgeEditorState

Snapshot of the current editor session.

```python
s = unreal.UnrealBridgeEditorLibrary.get_editor_state()
print(f'{s.project_name} on UE {s.engine_version}')
print(f'PIE={s.b_is_pie} paused={s.b_is_paused} level={s.current_level_path}')
print(f'opened={s.num_opened_assets} selectedActors={s.num_selected_actors} cbSel={s.num_content_browser_selection}')
```

### FBridgeEditorState fields

| Field | Type | Description |
|-------|------|-------------|
| `engine_version` | str | Engine version string |
| `project_name` | str | Project name |
| `b_is_pie` | bool | PIE is active |
| `b_is_paused` | bool | PIE is paused |
| `current_level_path` | str | Persistent level package path |
| `num_opened_assets` | int | Count of open asset editors |
| `num_selected_actors` | int | Count of selected actors in the world |
| `num_content_browser_selection` | int | Count of assets selected in Content Browser |

### get_engine_version() -> str

### is_in_pie() -> bool

### is_play_in_editor_paused() -> bool

### get_opened_assets() -> list[FBridgeOpenedAsset]

```python
for a in unreal.UnrealBridgeEditorLibrary.get_opened_assets():
    dirty = '*' if a.b_is_dirty else ''
    print(f'[{a.class_name}] {a.path}{dirty}')
```

### FBridgeOpenedAsset fields

| Field | Type | Description |
|-------|------|-------------|
| `path` | str | Asset object path |
| `class_name` | str | Asset class short name |
| `b_is_dirty` | bool | Has unsaved changes |

---

## Content Browser

### get_content_browser_selection() -> list[str]

Return object paths of assets currently selected in the Content Browser.

### get_content_browser_path() -> str

Currently-focused folder path (e.g. `/Game/MyFolder`). Empty if unavailable.

### set_content_browser_selection(asset_paths) -> bool

Select a set of assets in the Content Browser.

### sync_content_browser_to_asset(asset_path) -> bool

Navigate the Content Browser to the asset and highlight it.

### set_content_browser_path(folder_path) -> bool

Navigate the Content Browser to a folder (e.g. `/Game/Maps`). Returns False if the `ContentBrowser` module isn't loaded.

```python
unreal.UnrealBridgeEditorLibrary.set_content_browser_path('/Game/Maps')
```

---

## Viewport

### get_editor_viewport_camera() -> FBridgeViewportCamera

Current perspective viewport camera.

### FBridgeViewportCamera fields

| Field | Type | Description |
|-------|------|-------------|
| `location` | Vector | Camera location |
| `rotation` | Rotator | Camera rotation |
| `fov` | float | Field of view (degrees) |

### set_editor_viewport_camera(location, rotation, fov) -> bool

### focus_viewport_on_selection() -> bool

Frame the viewport on the currently selected actor(s).

### take_high_res_screenshot(resolution_multiplier) -> bool

Queue a high-res screenshot of the active level viewport. `resolution_multiplier` scales viewport size (1.0 = native). Output is written to `<Project>/Saved/Screenshots/WindowsEditor/` (engine-named). Returns True if the request was queued.

```python
unreal.UnrealBridgeEditorLibrary.take_high_res_screenshot(2.0)  # 2x native
```

### set_viewport_realtime(realtime) -> bool

Toggle realtime rendering for the active level viewport. Returns False if no viewport.

Note: the editor can stack temporary *realtime overrides* (e.g. during PIE,
Sequencer playback, or while a modal tool runs) that win over the value set
here. If `is_viewport_realtime()` disagrees with what you just set, an
override is active — see `FEditorViewportClient::AddRealtimeOverride`.

### is_viewport_realtime() -> bool

### get_viewport_size() -> Vector2D

Pixel size of the active viewport (`x` = width, `y` = height). `(0,0)` if no viewport.

### set_viewport_view_mode(mode) -> bool

Set the active viewport's view mode by name. Accepted values (case-insensitive):
`Lit`, `Unlit`, `Wireframe` (alias for `BrushWireframe`), `CSGWireframe`,
`DetailLighting`, `LightingOnly`, `LightComplexity`, `ShaderComplexity`,
`LightmapDensity`, `ReflectionOverride`, `CollisionPawn`, `CollisionVisibility`,
`LODColoration`, `QuadOverdraw`. Returns False on unknown name.

```python
unreal.UnrealBridgeEditorLibrary.set_viewport_view_mode('Unlit')
```

### get_viewport_view_mode() -> str

Current view mode name (e.g. `"Lit"`). Empty if no viewport. Unknown numeric
modes return `"VMI_<n>"`.

### set_viewport_show_flag(flag_name, enabled) -> bool

Toggle a named engine show flag on the active viewport. `flag_name` matches
`FEngineShowFlags::FindIndexByName` — common names: `Grid`, `Bounds`,
`Collision`, `Navigation`, `Landscape`, `StaticMeshes`, `SkeletalMeshes`,
`Particles`, `Fog`, `PostProcessing`, `Lighting`. Returns False on unknown flag.

```python
unreal.UnrealBridgeEditorLibrary.set_viewport_show_flag('Grid', False)
```

### get_viewport_show_flag(flag_name) -> bool

Read a named show flag. Returns False if the flag name is unknown (ambiguous
with a legitimate off state — prefer checking via `set_viewport_show_flag`'s
return before trusting `False`).

### set_viewport_type(viewport_type) -> bool

Set the active viewport's projection. Accepted (case-insensitive):
`Perspective`, `Top` (alias `OrthoXY`), `Front` (alias `OrthoXZ`),
`Side` (alias `OrthoYZ`), `OrthoFreelook`. Returns False on unknown name.

```python
unreal.UnrealBridgeEditorLibrary.set_viewport_type('Top')
```

### get_viewport_type() -> str

Returns `"Perspective"`, `"Top"`, `"Front"`, `"Side"`, or `"OrthoFreelook"`.

**Call cost & token footprint** — viewport render/display calls are all
synchronous single-value returns; each round-trip is `~150B` of JSON. Prefer
batching via `exec-file` if toggling many flags.

---

## Asset Control

### open_asset(asset_path) -> bool

Open the appropriate asset editor. Accepts either a full object path (`/Game/Foo/Bar.Bar`) or a bare package path (`/Game/Foo/Bar`).

Path handling: bare package paths are auto-normalized to `<path>.<leaf>` before `LoadObject`, so the inner asset is loaded instead of the `UPackage` wrapper. If a `UPackage` is still returned (edge cases), the library scans its inner objects and returns the one whose name matches the package leaf, else the first `IsAsset()` child.

> **Historical bug (fixed):** prior to the normalization pass, passing a bare package path (e.g. `/Game/Foo/Bar`) loaded the `UPackage` itself, and the Asset Editor Subsystem opened the Generic "Package" editor instead of the asset's dedicated editor (Curve Editor, BP Editor, etc.). Now both forms open the correct editor.

Applies to every `BridgeEditorImpl::LoadAssetFromPath` caller (`open_asset`, `save_asset`, `reload_asset`, `sync_content_browser_to_assets`, etc.).

Cost: single `FindObject`/`LoadObject` + at most one `ForEachObjectWithOuter` scan of the package (typically 1–5 inners). GameThread.

Output footprint: tiny — single bool.

### close_all_asset_editors() -> bool

### save_asset(asset_path) -> bool

### save_all_dirty_assets(include_maps) -> bool

Save all dirty packages. Returns True if the save attempt finished without user cancel.

### save_current_level() -> bool

### reload_asset(asset_path) -> bool

---

## Level / Map Control

### load_level(level_path, prompt_save_changes) -> bool

Load a map into the editor. `level_path` is a package path like `/Game/Maps/MyLevel`. If `prompt_save_changes` is True and there are unsaved map changes, the user is prompted; if False, unsaved changes are discarded silently.

```python
unreal.UnrealBridgeEditorLibrary.load_level('/Game/Maps/TestLevel', True)
```

### create_new_level(save_existing) -> bool

Create a new empty level and make it the current editor world. If `save_existing` is True, unsaved map changes prompt for save first; cancel returns False.

```python
unreal.UnrealBridgeEditorLibrary.create_new_level(True)
```

---

## PIE

### start_pie() -> bool

### stop_pie() -> bool

### pause_pie(paused) -> bool

---

## Undo / Redo

### undo() -> bool

### redo() -> bool

---

## Console / CVar

### execute_console_command(command) -> str

Run a console command. Returns captured `GLog` output (best-effort — some commands print only to the viewport HUD).

```python
out = unreal.UnrealBridgeEditorLibrary.execute_console_command('stat fps')
print(out)
```

### get_cvar(name) -> str

### set_cvar(name, value) -> bool

### list_cvars(keyword) -> list[str]

Search CVars by substring. Returns `"Name = Value"` entries.

---

## Utility

### fixup_redirectors(paths) -> int

Fix up object redirectors under the given content paths (e.g. `/Game/Foo`). Re-saves referencers to point at the destination and deletes the redirector. Returns the number of redirectors processed.

```python
n = unreal.UnrealBridgeEditorLibrary.fixup_redirectors(['/Game'])
```

### get_dirty_package_names() -> list[str]

Package names of all currently-dirty packages in `/Game/` and plugin content mounts (`/Script/` and `/Temp/` are skipped). Useful for "what will be saved next?" style prompts.

```python
dirty = unreal.UnrealBridgeEditorLibrary.get_dirty_package_names()
print('\n'.join(dirty))
```

### is_asset_dirty(asset_path) -> bool

True if the asset's package has unsaved modifications.

### mark_asset_dirty(asset_path) -> bool

Mark the asset's package dirty — useful after direct C++/Python mutations that didn't already notify the package.

### is_asset_editor_open(asset_path) -> bool

True if an asset editor tab is currently open for this asset.

```python
if unreal.UnrealBridgeEditorLibrary.is_asset_editor_open('/Game/BP/BP_Hero'):
    unreal.UnrealBridgeEditorLibrary.close_all_asset_editors()
```

### is_asset_loaded(asset_path) -> bool

True if the asset's package is already loaded in memory. Does NOT force-load the asset — useful to probe load state before deciding whether to trigger an expensive load.

```python
if not unreal.UnrealBridgeEditorLibrary.is_asset_loaded('/Game/BP/BP_Hero.BP_Hero'):
    print('not loaded — skipping cheap probe')
```

### close_asset_editor(asset_path) -> bool

Close the editor tab for a single asset. Returns False if no editor was open for that asset.

```python
unreal.UnrealBridgeEditorLibrary.close_asset_editor('/Game/BP/BP_Hero')
```

### save_assets(asset_paths) -> int

Save the listed assets silently (no save dialog). Returns the number of packages successfully written. Assets that couldn't be resolved are skipped.

```python
saved = unreal.UnrealBridgeEditorLibrary.save_assets([
    '/Game/Data/DT_Weapons',
    '/Game/BP/BP_Hero',
])
```

### compile_blueprints(blueprint_paths) -> list[FBridgeCompileResult]

Compile the listed Blueprints. Returns per-BP success + error summary.

```python
results = unreal.UnrealBridgeEditorLibrary.compile_blueprints(['/Game/BP/BP_Hero', '/Game/BP/BP_Enemy'])
for r in results:
    ok = 'OK' if r.b_success else 'FAIL'
    print(f'[{ok}] {r.path} {r.error_message}')
```

### FBridgeCompileResult fields

| Field | Type | Description |
|-------|------|-------------|
| `path` | str | Blueprint path |
| `b_success` | bool | Compile succeeded |
| `error_message` | str | Error summary (empty on success) |

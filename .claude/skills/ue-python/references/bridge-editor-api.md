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

---

## Asset Control

### open_asset(asset_path) -> bool

Open the appropriate asset editor. Accepts object path or package path.

### close_all_asset_editors() -> bool

### save_asset(asset_path) -> bool

### save_all_dirty_assets(include_maps) -> bool

Save all dirty packages. Returns True if the save attempt finished without user cancel.

### save_current_level() -> bool

### reload_asset(asset_path) -> bool

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

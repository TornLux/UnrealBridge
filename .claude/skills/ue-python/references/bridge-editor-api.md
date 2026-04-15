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

## Editor UX + plugin introspection

### show_editor_notification(message, duration_seconds=4.0, b_success=True) -> bool

Show a Slate toast (lower-right of the editor window) for long-running
automation scripts. `duration_seconds` is clamped to `[1, 60]`. The
`b_success` flag selects the icon — green checkmark when `True`, red X
when `False`; use the failure style sparingly, it's loud.

```python
unreal.UnrealBridgeEditorLibrary.show_editor_notification(
    'Import finished — 42 assets', 5.0, True)
```

Returns False only when `message` is empty or the Slate notification
manager rejects the add (rare).

### get_enabled_plugins() -> list[str]

Alphabetically-sorted list of plugin names currently enabled for the
project (e.g. `['UnrealBridge', 'EnhancedInput', 'GameplayAbilities', ...]`).
Typical projects return 150–300 names — the list is small-ish but you
probably want to filter client-side.

### is_plugin_enabled(plugin_name) -> bool

Case-insensitive match against the enabled-plugin set. Much cheaper
than scanning the `get_enabled_plugins()` result when you only need a
single boolean.

```python
if not unreal.UnrealBridgeEditorLibrary.is_plugin_enabled('PythonScriptPlugin'):
    raise RuntimeError('PythonScriptPlugin disabled — bridge is broken')
```

### get_editor_build_config() -> str

Build configuration the running editor was compiled in:
`"Debug"` | `"DebugGame"` | `"Development"` | `"Shipping"` | `"Test"` |
`"Unknown"`. Use to gate expensive debug-only automation.

### write_log_message(message, severity="Log") -> bool

Emit a message to `GLog` under the dedicated `LogUnrealBridgePy`
category. Lands in the editor's Output Log and the project's `.log`
file so Python automation output is captured alongside native UE logs.

`severity` is case-insensitive, one of: `"Verbose"`, `"Log"` (default),
`"Warning"`, `"Error"`. Unknown values fall back to `Log`.

```python
unreal.UnrealBridgeEditorLibrary.write_log_message('import finished', 'Log')
unreal.UnrealBridgeEditorLibrary.write_log_message('asset missing', 'Warning')
```

Returns False only when `message` is empty.

### get_log_file_path() -> str

Absolute path to the current editor log file (typically
`<Project>/Saved/Logs/<Project>.log`). Useful after long operations to
`tail` the log from Python without guessing the path.

### get_screenshot_directory() -> str

Absolute path to `<Project>/Saved/Screenshots/<Platform>Editor/`. Pair
with `take_high_res_screenshot` — UE picks the filename automatically,
so scan this directory afterwards to find the newly-written file.

### bring_editor_to_front() -> bool

Raise and activate the editor's top-level window. Good at the end of a
long background task that toasted a notification — without raising, the
toast is hidden when the user alt-tabbed to another app.

Returns False when no Slate application / no visible top-level window
(e.g. commandlet / headless editor run).

### get_frame_rate() -> float

Instantaneous FPS from the last `FApp::GetDeltaTime()` sample.
Per-frame jittery — average 10–30 samples client-side for anything a
human reads. Returns 0 when the delta is near-zero (paused tick).

### get_memory_usage_mb() -> float

Physical memory used by the editor process, in MB. Sampled via
`FPlatformMemory::GetStats().UsedPhysical`. On Windows this matches
Task Manager's "Working set" value.

### get_engine_uptime() -> float

Seconds since `GStartTime` (engine init complete). Useful for gating
early automation — e.g. skip expensive work during the first 30 s
while the editor is still loading shaders.

### trigger_garbage_collection(b_full_purge=False) -> bool

Force a GC pass. `b_full_purge=False` runs an incremental collection
(fast, default). `True` runs the full purge — slow, compacts the pool,
blocks the game thread for up to several seconds on a large project.

Use after destroying a batch of actors / unloading many assets when
you want `get_memory_usage_mb` to reflect the drop immediately rather
than waiting for the next engine-scheduled GC.

```python
unreal.UnrealBridgeLevelLibrary.destroy_actors(many_actor_names)
unreal.UnrealBridgeEditorLibrary.trigger_garbage_collection(False)
print(unreal.UnrealBridgeEditorLibrary.get_memory_usage_mb())
```

Returns True once the collection pass has run.

### get_project_version() -> str

Reads `ProjectVersion` from
`[/Script/EngineSettings.GeneralProjectSettings]` in DefaultGame.ini.
Empty string when unset.

### get_project_company_name() -> str

`CompanyName` from the project's general settings. Empty if unset.

### get_project_id() -> str

Stable per-project identifier. Current implementation returns the
project short name (e.g. `"GameplayLocomotion"`) — sufficient as a
cache key. If you specifically need the .uproject GUID, parse the
`.uproject` JSON yourself via `unreal.SystemLibrary.get_project_directory()`.

### get_auto_save_directory() -> str

Absolute path to `<Project>/Saved/Autosaves`. Use to locate `.auto.umap`
files UE drops there during long editing sessions.

### open_editor_tab(tab_name) -> bool

Invoke a docked-tab spawner by tab id via `FGlobalTabmanager::TryInvokeTab`.
Opens the tab if registered and not already open; refocuses it
otherwise. Returns False when the id isn't a registered spawner.

Common ids: `"OutputLog"`, `"ContentBrowserTab1"`, `"StatsViewer"`,
`"MessageLog"`, `"LevelEditorToolBox"`, `"LevelEditorStatsViewer"`.

### close_editor_tab(tab_name) -> bool

Close a live docked tab. Returns False if no live tab with that id
exists.

### is_editor_tab_open(tab_name) -> bool

Tab-liveness check via `FGlobalTabmanager::FindExistingLiveTab`.

**Pitfall:** `FindExistingLiveTab` is stricter than `TryInvokeTab`'s
spawner registry — freshly-invoked tabs may need a frame to register
as "live", so immediately-following `is_editor_tab_open` can still
return False even after `open_editor_tab` succeeded. Wait a tick, or
rely on `open_editor_tab`'s return value directly.

### get_main_window_title() -> str

Title text of the editor's main frame window (e.g. `"MyProject -
Unreal Editor"`). Empty string when the MainFrame module isn't
loaded yet (very early boot).

### get_os_version() -> str

Human-readable OS version, e.g. `"Windows 10 (22H2) [10.0.19045.6466]"`.

### get_cpu_brand() -> str

CPU brand string from `FPlatformMisc::GetCPUBrand` (e.g. `"AMD Ryzen
9 7950X 16-Core Processor"`). May include trailing spaces on some
platforms — strip client-side if displaying.

### get_cpu_core_count() -> int

Logical core count (physical cores × hyperthreads) available to the
editor process. Useful as a cap for parallel-work heuristics.

### get_total_physical_memory_mb() -> float

Total physical RAM on the host in MB. Pair with
`get_memory_usage_mb()` to compute headroom ratios.

### get_shader_compile_job_count() -> int

Pending shader-compile jobs across `GShaderCompilingManager`. Non-zero
right after opening a new level, editing a material, or reimporting
a texture. Zero when the editor is idle.

### get_asset_compile_job_count() -> int

Pending async asset-compile count (materials, textures, meshes) via
`FAssetCompilingManager`.

### is_compiling() -> bool

True while either shader or asset compilation is in-flight. Cheap
gate for "wait until editor is idle" loops.

### flush_compilation() -> bool

Block the game thread until both queues drain via
`FinishAllCompilation`. Can take tens of seconds after a big import —
callers should show progress UI or raise their own watchdog.

```python
if unreal.UnrealBridgeEditorLibrary.is_compiling():
    unreal.UnrealBridgeEditorLibrary.flush_compilation()
# safe to take screenshots / run PIE now
```

### get_recent_log_lines(num_lines=50, min_severity="") -> list[str]

Lazily installs a thread-safe `FOutputDevice` ring buffer on first
call (capacity 500 lines). Each entry is preformatted as
`"[Category][Severity] Message"`, ordered oldest → newest.

`min_severity` (case-insensitive) is one of: `"Verbose"` | `"Log"` |
`"Display"` | `"Warning"` | `"Error"` | `"Fatal"`. Empty string = no
filter. Entries are returned at or above that severity.

`num_lines=0` returns everything currently buffered; a positive value
returns the last N after filtering.

```python
# Run an expensive op, then surface any warnings/errors.
unreal.UnrealBridgeEditorLibrary.clear_log_buffer()
run_big_thing()
for l in unreal.UnrealBridgeEditorLibrary.get_recent_log_lines(0, 'Warning'):
    print(l)
```

### get_log_buffer_size() -> int

Current buffered line count (0..500). Maxes out once the ring fills;
further writes overwrite the oldest entries.

### get_log_buffer_capacity() -> int

Fixed ring capacity (currently 500). Useful for clients that want to
gauge potential loss before a big log-heavy operation.

### clear_log_buffer() -> int

Empty the ring; returns the count of lines dropped. The `FOutputDevice`
stays registered — subsequent log output continues to accumulate.

**Pitfalls**

- The ring buffer is *append-only* across editor sessions; there's no
  persistence. Log lines from before the first `get_recent_log_lines`
  call of a session are lost — the device installs lazily.
- At 500 lines capacity, a verbose operation (shader compile, asset
  import) can overflow the buffer and drop earlier lines. Call
  `clear_log_buffer()` right before the operation to make sure its
  output fits.

### is_module_loaded(module_name) -> bool

True when an engine module is currently loaded. Complements
`is_plugin_enabled` — plugins own modules, but modules can also exist
standalone as engine built-ins.

### get_registered_module_names() -> list[str]

Alphabetically-sorted names of every module the `FModuleManager`
knows about (loaded or not). Typical editor count: 900–1100. Large
list — filter client-side.

### load_module(module_name) -> bool

Force-load a module if it isn't already loaded. Returns True only if
the module is loaded after the call.

**Pitfall:** loading game-side modules mid-session introduces new
UObjects and can complicate GC / asset-registry snapshots. Prefer for
editor-only tooling modules; cold-loading gameplay modules is risky.

### get_module_binary_path(module_name) -> str

Filesystem path to a loaded module's compiled binary. Empty string if
the module isn't loaded or has no backing file (script-only modules).

```python
p = unreal.UnrealBridgeEditorLibrary.get_module_binary_path('UnrealBridge')
# p → ".../Plugins/UnrealBridge/Binaries/Win64/UnrealEditor-UnrealBridge.dll"
```

### get_widget_mode() -> str

Active transform-gizmo mode in the level viewport. One of:
`"Translate"` | `"Rotate"` | `"Scale"` | `"TranslateRotateZ"` |
`"2D"` | `"None"`. Empty on editor-mode-tools unavailable.

### set_widget_mode(mode) -> bool

Switch gizmo mode. Accepts the strings returned by `get_widget_mode`
(case-insensitive). Returns False on unknown mode.

### get_coord_system() -> str

Current coordinate system used by the transform gizmo: `"World"` or
`"Local"`.

### set_coord_system(system) -> bool

Toggle between `"World"` and `"Local"` (case-insensitive). Mirrors the
`~` hotkey in the viewport toolbar. Returns False on unknown value.

```python
# Temporarily switch to rotate gizmo in local space for a scripted
# placement, then restore.
prev_mode = unreal.UnrealBridgeEditorLibrary.get_widget_mode()
unreal.UnrealBridgeEditorLibrary.set_widget_mode('Rotate')
unreal.UnrealBridgeEditorLibrary.set_coord_system('Local')
# ... user interacts ...
unreal.UnrealBridgeEditorLibrary.set_widget_mode(prev_mode)
```

### get_location_grid_size() -> float

Active location-grid snap size in cm. Sourced from
`ULevelEditorViewportSettings` — reads `Pow2GridSizes` when "use
power-of-two snap size" is on, else `DecimalGridSizes`, indexed by
`CurrentPosGridSize`.

### get_rotation_grid_size() -> float

Active rotation-grid snap size in degrees. Reads from
`CommonRotGridSizes` (default) or `DivisionsOf360RotGridSizes`
depending on the current grid mode.

### is_grid_snap_enabled() -> bool

True when viewport location-grid snapping is enabled. Mirrors the
grid-snap toggle in the viewport toolbar.

### set_grid_snap_enabled(enabled) -> bool

Toggle location-grid snapping. Persists to editor config via
`SaveConfig`. Returns True on success.

**Pitfall:** setters are provided only for the enable toggle — the
active grid size is index-based and changing it requires matching a
stored preset. For custom sizes, edit the `*GridSizes` arrays directly
via UE Python (`unreal.get_mutable_default_object(...)`).

### is_auto_save_enabled() -> bool

True if the editor autosave timer is enabled (matches the Editor
Preferences → Loading & Saving → Auto Save checkbox).

### set_auto_save_enabled(enabled) -> bool

Toggle autosave. Persists to editor config.

### get_auto_save_interval_minutes() -> int

Autosave interval in minutes. Default `10`. Returns `-1` if the
settings object is unavailable.

### set_auto_save_interval_minutes(minutes) -> bool

Set the interval. Accepts `1..120`; values outside that range are
rejected. Persists to editor config.

```python
# Silence autosave while running a long scripted capture.
prev = unreal.UnrealBridgeEditorLibrary.is_auto_save_enabled()
unreal.UnrealBridgeEditorLibrary.set_auto_save_enabled(False)
run_capture()
unreal.UnrealBridgeEditorLibrary.set_auto_save_enabled(prev)
```

### does_asset_exist_on_disk(asset_path) -> bool

Pure filesystem check — True when the package's `.uasset` or `.umap`
file exists. Does not load the asset or consult the Asset Registry.
Accepts package paths (`/Game/Foo/Bar`) or full object paths
(`/Game/Foo/Bar.Bar` — the object suffix is stripped).

### get_asset_disk_path(asset_path) -> str

Absolute filesystem path to the package's backing file. Empty on
unresolvable paths.

### get_asset_file_size(asset_path) -> int

File size in bytes. Returns `-1` when the file is missing.

### get_asset_last_modified_time(asset_path) -> str

ISO-8601 UTC timestamp of the file's last modification. Empty when
the file is missing or the timestamp can't be read.

```python
# Detect recently-edited assets.
import datetime
p = '/Game/Maps/MyLevel'
mtime = unreal.UnrealBridgeEditorLibrary.get_asset_last_modified_time(p)
if mtime:
    delta = datetime.datetime.utcnow() - datetime.datetime.fromisoformat(mtime.rstrip('Z'))
    if delta.total_seconds() < 3600:
        print(f'{p} modified within last hour')
```

### get_os_user_name() -> str

Logged-in OS user name via `FPlatformProcess::UserName`.

### get_machine_name() -> str

Host computer name via `FPlatformProcess::ComputerName`.

### get_now_utc() -> str

ISO-8601 UTC timestamp for "now" (e.g. `"2026-04-15T12:25:18.723Z"`).
Useful as a timestamp prefix for automation logs.

### get_editor_process_id() -> int

OS process ID of the running editor. Handy for external tooling that
wants to attach a debugger or killswitch against this session.

### is_source_control_enabled() -> bool

True when a source-control provider is registered AND currently
available (configured + reachable).

### get_source_control_provider_name() -> str

Active provider name, e.g. `"Perforce"`, `"Git"`, `"Plastic"`.
Empty when the module isn't loaded.

### get_asset_source_control_state(asset_path) -> str

One of: `"CheckedOut"` | `"NotCheckedOut"` | `"CheckedOutOther"` |
`"Added"` | `"Deleted"` | `"Ignored"` | `"NotControlled"` |
`"Unknown"`. Empty if the asset can't be resolved or SCC is disabled.

Cached state via `EStateCacheUsage::Use` — no network round-trip.
Call `execute_console_command('Source Control Refresh')` to force a
fresh query if stale state is suspected.

### check_out_asset(asset_path) -> bool

Synchronous check-out via `FCheckOut`. Returns True only when the
provider reports success. Blocks briefly on network round-trip for
remote providers — use sparingly in tight loops.

### get_engine_directory() -> str

Absolute path to the engine installation's `Engine/` directory
(e.g. `G:/UnrealEngine/UE_5.7/Engine/`).

### get_project_content_directory() -> str

Absolute path to the project's `Content/` directory.

### get_project_intermediate_directory() -> str

Absolute path to `Intermediate/` — transient build outputs, asset
cache, shader cache live here.

### get_project_plugins_directory() -> str

Absolute path to the project's `Plugins/` directory. Engine-side
plugins (e.g. those bundled with UE) live under
`get_engine_directory() + 'Plugins/'`, not here.

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

### start_simulate() -> bool

Start "Simulate in Editor" — spins up the play world but skips player
controller spawn / pawn possession. Useful for observing AI, Sequencer,
or physics without stealing input focus. Fails (returns False) if a
play session is already running; stop it first.

```python
unreal.UnrealBridgeEditorLibrary.start_simulate()
# ... observe ...
unreal.UnrealBridgeEditorLibrary.stop_pie()   # same StopPIE works
```

### is_simulating() -> bool

True only during a Simulate-in-Editor session. Note that `is_in_pie()`
also returns True during Simulate — use `is_simulating()` to distinguish.

### get_pie_net_mode() -> str

Network mode of the current PIE/Simulate world, one of:
`"Standalone"` | `"DedicatedServer"` | `"ListenServer"` | `"Client"`.
Returns `""` when no play session is running.

### get_pie_world_time() -> float

Seconds since BeginPlay on the PIE/Simulate world. Returns `-1.0` when
no play session is running. Time freezes while PIE is paused (mirrors
`UWorld::GetTimeSeconds`), so it's suitable for scripting "wait 2
seconds of in-game time" delays that respect pause.

```python
t0 = unreal.UnrealBridgeEditorLibrary.get_pie_world_time()
# ... run some gameplay ...
elapsed = unreal.UnrealBridgeEditorLibrary.get_pie_world_time() - t0
```

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

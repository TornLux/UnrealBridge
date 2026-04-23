# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UnrealBridge is a TCP bridge between external tools (Claude Code) and Unreal Engine 5.7. It consists of:
- A UE Editor plugin (`Plugin/UnrealBridge/`) that runs a TCP server inside the editor
- A Python CLI client (`.claude/skills/unreal-bridge/scripts/bridge.py`) used by the `unreal-bridge` skill
- API reference docs and helper scripts for querying/manipulating UE assets via Python

## Key Commands

**Sync plugin to UE project and compile:**
```bash
sync_plugin.bat
```
Mirrors `Plugin/UnrealBridge/` into the target project's `Plugins/UnrealBridge/` (excluding `Binaries`/`Intermediate`). The DST path lives in `sync_plugin.bat` itself — do not hardcode it anywhere else.

**Test bridge connection:**
```bash
python .claude/skills/unreal-bridge/scripts/bridge.py ping
```

**Execute Python in UE:**
```bash
python .claude/skills/unreal-bridge/scripts/bridge.py exec "print('hello')"
python .claude/skills/unreal-bridge/scripts/bridge.py exec-file script.py
```

## Architecture

### TCP Protocol
Length-prefixed JSON over TCP on an OS-assigned port (default bind `127.0.0.1`, port auto-allocated at startup). Clients find the editor via the UDP multicast discovery service (see next section) — port 9876 is no longer hardcoded on the TCP data channel.
- Request: `[4 bytes big-endian length][JSON: {"id":"...", "script":"...", "timeout":30, "token":"..." (optional)}]`
- Response: `[4 bytes big-endian length][JSON: {"id":"...", "success":bool, "output":"...", "error":"..."}]`
- Token auth: required when the server binds non-loopback. The token is written to `<Project>/Saved/UnrealBridge/token.txt`; clients read it and add `"token":"<value>"` to every request. Constant-time compared on the server.
- Special commands (handled inline on the worker thread, bypass the Python exec queue):
  - `{"id":"...", "command":"ping"}` → `pong` (TCP-only liveness)
  - `{"id":"...", "command":"gamethread_ping", "timeout":2.0}` → `alive`/`unresponsive` + `latency_ms` (GT liveness)
  - `{"id":"...", "command":"debug_resume"}` → unsticks a paused BP breakpoint via `FKismetDebugUtilities::RequestAbortingExecution`

### Discovery Protocol
UDP multicast on `239.255.42.99:9876` (the one constant the whole system shares). Multiple editors can bind via `SO_REUSEADDR`.
- Probe (client → group): `{"v":1, "type":"probe", "request_id":"<uuid>", "filter":{"project":"<name|path|*>"}}`
- Response (server → probe source, unicast): `{"v":1, "type":"response", "request_id":"<uuid>", "pid":..., "project":"...", "project_path":"...", "engine_version":"...", "tcp_bind":"...", "tcp_port":..., "token_fingerprint":"<sha1(token)[:16]>"}`
- Client loop: probe → collect for `--discovery-timeout` ms (default 800) → filter by `--project=...` → connect TCP. Empty `token_fingerprint` means no token required.

### Server configuration (CLI / env / editor ini)
Priority CLI > env > `EditorPerProjectUserSettings.ini [UnrealBridge]` > default.

| CLI | Env | INI key | Default | Effect |
|---|---|---|---|---|
| `-UnrealBridgeBind=` | `UNREAL_BRIDGE_BIND` | `Bind` | `127.0.0.1` | Interface to bind TCP to |
| `-UnrealBridgePort=` | `UNREAL_BRIDGE_PORT` | `Port` | `0` | TCP port — `0` = OS-assigned ephemeral |
| `-UnrealBridgeToken=` | `UNREAL_BRIDGE_TOKEN` | `Token` | *(empty)* | Required when bind is not loopback |
| `-UnrealBridgeDiscoveryGroup=` | `UNREAL_BRIDGE_DISCOVERY_GROUP` | `DiscoveryGroup` | `239.255.42.99:9876` | Multicast group + port |
| `-UnrealBridgeDiscoveryEnabled=` | `UNREAL_BRIDGE_DISCOVERY` | `DiscoveryEnabled` | `1` | `0` = disable discovery responder |
| `-UnrealBridgeNoDiscovery` *(flag)* | — | — | — | Shorthand for `DiscoveryEnabled=0` |

### Plugin Module Structure
- **UnrealBridgeModule** — Module entry point at PostEngineInit. Parses config, starts TCP server + discovery responder, maps `/Plugin/UnrealBridge/` → Shaders dir
- **UnrealBridgeServer** — TCP listener, accepts clients on background threads, dispatches Python execution to GameThread via `IPythonScriptPlugin::ExecPythonCommandEx`. Uses `__UB_ERR__` sentinel to separate stdout from stderr in captured output
- **UnrealBridgeBlueprintLibrary** — Blueprint introspection: class hierarchy, variables, functions, components, interfaces, graph analysis (call graph, execution flow, node inspection, pin connections), timelines, event dispatchers, cross-graph search, write ops (set variable defaults, component properties, add variables)
- **UnrealBridgeAssetLibrary** — Asset search (keyword with include/exclude tokens), derived class queries, asset references/dependencies, DataAsset queries, folder listing
- **UnrealBridgeAnimLibrary** — AnimBlueprint introspection: state machines, AnimGraph nodes, linked layers, slots, curves, anim sequence/montage/blend space info, skeleton bone tree
- **UnrealBridgeDataTableLibrary** — DataTable row inspection
- **UnrealBridgeMaterialLibrary** — Material instance parameter queries
- **UnrealBridgeUMGLibrary** — Widget Blueprint introspection: widget tree, properties, animations, bindings, events, search, property write
- **UnrealBridgeLevelLibrary** — Level/actor introspection and editing on the editor world: summary, actor listing with class/tag/name filters, actor info/transform/components, class/tag/radius queries, streaming levels, selection; write ops spawn/destroy/move/attach/detach/duplicate/label/hide + nested property get/set (e.g. `RootComponent.RelativeLocation`). All writes wrapped in `FScopedTransaction` for Ctrl+Z
- **UnrealBridgeEditorLibrary** — Editor session control: state query (engine version, PIE status, opened assets, CB selection/path, viewport camera), asset open/close/save/reload, Content Browser sync, viewport camera set/focus, PIE start/stop/pause, undo/redo, console command execution, CVar get/set/list, redirector fixup, Blueprint compile
- **UnrealBridgeGameplayAbilityLibrary** — GameplayAbilitySystem introspection (scaffold): GameplayAbility Blueprint CDO metadata — name, parent, instancing/net policy, asset tags, cost/cooldown GE class. Depends on the `GameplayAbilities` engine plugin (auto-enabled via `.uplugin`)
- **UnrealBridgePerfLibrary** — Structured perf snapshots: frame timing (FPS, GT/RT/GPU/RHI ms) from viewport `FStatUnitData`, draw calls / primitives from RHI globals, process memory via `FPlatformMemory::GetStats`, UObject class histogram via `TObjectIterator`. Replaces parsing `stat unit` text output

### Python Side
- `Content/Python/unreal_bridge_helpers.py` — Helper functions auto-loaded in UE Python env (list_assets, get_selected_actors, find_actors_by_class, set_actor_transform, get_world_info)
- `.claude/skills/unreal-bridge/` — Claude Code skill with bridge CLI, API reference docs, and safety rules

## Development Workflow

Edit C++ source in `Plugin/UnrealBridge/Source/`. Two canonical loops, picked by whether the edit touches reflection metadata:

### Hot reload (editor stays up) — body-only edits

```bash
python .claude/skills/unreal-bridge/scripts/hot_reload.py
```

Syncs plugin source then triggers Live Coding via the bridge. Works when the edit only changed function bodies (no new `UFUNCTION` / `UCLASS` / `UPROPERTY` / `USTRUCT` members). LC patches the running editor in place — PIE, open assets, viewport camera all survive. Takes ~10–60s depending on how many TUs changed. On `Status="Failure"` the script tails recent `LogLiveCoding` entries; the actual MSVC error text only lives in the external LiveCodingConsole GUI window (see `bridge-editor-api.md` Live Coding section).

### Full rebuild + relaunch — any reflection change

```bash
python .claude/skills/unreal-bridge/scripts/rebuild_relaunch.py
```

Quits the editor → runs `sync_plugin.bat` → runs the target project's `Build.bat` → launches the editor detached → polls `bridge.py ping` until ready. Use when adding/removing `UFUNCTION` / `UCLASS` / `UPROPERTY`, changing struct layouts, or recovering from a failed LC compile. Build.bat's stdout captures full compiler output (this is the only way to surface MSVC errors when hot reload reports Failure). Takes ~2–5 minutes.

The script resolves the editor exe from `--editor-exe` CLI arg → `UNREAL_EDITOR_EXE` env var → `UE_ROOT` env var. No hardcoded paths. Set one of those env vars before first use.

### Verifying new functionality

After either loop finishes:
- `python .claude/skills/unreal-bridge/scripts/bridge.py ping` — confirm the bridge is up.
- `bridge.py exec "import unreal; print(unreal.SystemLibrary.get_project_directory())"` — confirm Python is live.
- Exercise the feature via `bridge.py exec` or `exec-file` (call the new `unreal.<Library>.<method>()`). Check return values and `LogUnrealBridge` output.

### Clean shutdown (if needed)

```bash
python .claude/skills/unreal-bridge/scripts/bridge.py exec "import unreal; unreal.SystemLibrary.quit_editor()"
```

Verify with `tasklist //FI "IMAGENAME eq UnrealEditor.exe"`. Only fall back to `taskkill` if `quit_editor` doesn't return.

## Important Notes

- Plugin is Editor-only (`"Type": "Editor"`) — depends on PythonScriptPlugin
- Python execution happens on GameThread (async dispatch from worker thread with sync wait)
- All C++ library classes are `UBlueprintFunctionLibrary` subclasses with static UFUNCTIONs, callable from both Blueprint and Python via `unreal.<ClassName>.<method_name>()`
- Asset paths in API calls use content paths (e.g. `/Game/MyFolder/BP_MyActor`)
- Safety: destructive operations (delete/modify assets) require user confirmation; use `unreal.ScopedEditorTransaction` for undoable changes

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UnrealBridge is a TCP bridge between external tools (Claude Code) and Unreal Engine 5.7. It consists of:
- A UE Editor plugin (`Plugin/UnrealBridge/`) that runs a TCP server inside the editor
- A Python CLI client (`.claude/skills/ue-python/scripts/bridge.py`) used by the `ue-python` skill
- API reference docs and helper scripts for querying/manipulating UE assets via Python

## Key Commands

**Sync plugin to UE project and compile:**
```bash
sync_plugin.bat
```
This mirrors `Plugin/UnrealBridge/` to `G:\UEProjects\GameplayLocomotion\Plugins\UnrealBridge\` (excluding Binaries/Intermediate), then UE recompiles on next editor launch or hot-reload.

**Test bridge connection:**
```bash
python .claude/skills/ue-python/scripts/bridge.py ping
```

**Execute Python in UE:**
```bash
python .claude/skills/ue-python/scripts/bridge.py exec "print('hello')"
python .claude/skills/ue-python/scripts/bridge.py exec-file script.py
```

## Architecture

### TCP Protocol
Length-prefixed JSON over TCP on port 9876 (localhost only).
- Request: `[4 bytes big-endian length][JSON: {"id":"...", "script":"...", "timeout":30}]`
- Response: `[4 bytes big-endian length][JSON: {"id":"...", "success":bool, "output":"...", "error":"..."}]`
- Special command: `{"id":"...", "command":"ping"}` returns pong

### Plugin Module Structure
- **UnrealBridgeModule** — Module entry point; starts TCP server on port 9876 at PostEngineInit
- **UnrealBridgeServer** — TCP listener, accepts clients on background threads, dispatches Python execution to GameThread via `IPythonScriptPlugin::ExecPythonCommandEx`. Uses `__UB_ERR__` sentinel to separate stdout from stderr in captured output
- **UnrealBridgeBlueprintLibrary** — Blueprint introspection: class hierarchy, variables, functions, components, interfaces, graph analysis (call graph, execution flow, node inspection, pin connections), timelines, event dispatchers, cross-graph search, write ops (set variable defaults, component properties, add variables)
- **UnrealBridgeAssetLibrary** — Asset search (keyword with include/exclude tokens), derived class queries, asset references/dependencies, DataAsset queries, folder listing
- **UnrealBridgeAnimLibrary** — AnimBlueprint introspection: state machines, AnimGraph nodes, linked layers, slots, curves, anim sequence/montage/blend space info, skeleton bone tree
- **UnrealBridgeDataTableLibrary** — DataTable row inspection
- **UnrealBridgeMaterialLibrary** — Material instance parameter queries
- **UnrealBridgeUMGLibrary** — Widget Blueprint introspection: widget tree, properties, animations, bindings, events, search, property write
- **UnrealBridgeLevelLibrary** — Level/actor introspection and editing on the editor world: summary, actor listing with class/tag/name filters, actor info/transform/components, class/tag/radius queries, streaming levels, selection; write ops spawn/destroy/move/attach/detach/duplicate/label/hide + nested property get/set (e.g. `RootComponent.RelativeLocation`). All writes wrapped in `FScopedTransaction` for Ctrl+Z
- **UnrealBridgeEditorLibrary** — Editor session control: state query (engine version, PIE status, opened assets, CB selection/path, viewport camera), asset open/close/save/reload, Content Browser sync, viewport camera set/focus, PIE start/stop/pause, undo/redo, console command execution, CVar get/set/list, redirector fixup, Blueprint compile

### Python Side
- `Content/Python/unreal_bridge_helpers.py` — Helper functions auto-loaded in UE Python env (list_assets, get_selected_actors, find_actors_by_class, set_actor_transform, get_world_info)
- `.claude/skills/ue-python/` — Claude Code skill with bridge CLI, API reference docs, and safety rules

## Development Workflow

1. Edit C++ source in `Plugin/UnrealBridge/Source/`
2. Run `sync_plugin.bat` to copy plugin to the UE project
3. Open/reload UE Editor to compile; check Output Log for `LogUnrealBridge` messages
4. Test with `bridge.py ping` then run test scripts (`test_final.py`, etc.) via bridge

## Important Notes

- Plugin is Editor-only (`"Type": "Editor"`) — depends on PythonScriptPlugin
- Python execution happens on GameThread (async dispatch from worker thread with sync wait)
- All C++ library classes are `UBlueprintFunctionLibrary` subclasses with static UFUNCTIONs, callable from both Blueprint and Python via `unreal.<ClassName>.<method_name>()`
- Asset paths in API calls use content paths (e.g. `/Game/MyFolder/BP_MyActor`)
- Safety: destructive operations (delete/modify assets) require user confirmation; use `unreal.ScopedEditorTransaction` for undoable changes

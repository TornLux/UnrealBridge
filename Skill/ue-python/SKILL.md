---
name: ue-python
description: Execute Python scripts inside a running Unreal Engine 5.7 editor via TCP bridge. Use when the user asks to interact with UE, manipulate assets, query scenes, automate workflows, or run Python in Unreal.
allowed-tools: Bash Read Write Edit Glob Grep
---

# UE Python Bridge

Execute Python code directly inside a running Unreal Engine 5.7 editor. The bridge communicates over TCP with the UnrealBridge plugin (default port 9876).

## Bridge CLI

The bridge script is bundled with this skill:

```
${CLAUDE_SKILL_DIR}/scripts/bridge.py
```

All commands use this pattern:

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" [options] <command> [args]
```

Options: `--host HOST` (default 127.0.0.1), `--port PORT` (default 9876), `--timeout SECS` (default 30), `--json` (machine-readable output)

### Commands

| Command | Usage | Purpose |
|---------|-------|---------|
| `ping` | `python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" ping` | Check UE connection |
| `exec` | `python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec "code"` | Execute inline Python |
| `exec-file` | `python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec-file path.py` | Execute a .py file |

### Exit codes

- `0` — success
- `1` — error (connection refused, script exception, timeout)

## Workflow

1. **Always ping first** to verify the editor is reachable
2. Use `exec` for short queries (one-liners, simple lookups)
3. For multi-line scripts (>3 lines), write a temp `.py` file and use `exec-file`
4. Always use `--json` when you need to parse structured results programmatically
5. stdout = script output, stderr = errors/tracebacks

## Helper Library

The UE plugin ships `unreal_bridge_helpers` (auto-loaded in UE's Python path):

```python
from unreal_bridge_helpers import *

list_assets('/Game/Props', class_filter='StaticMesh')   # -> [str]
get_selected_actors()                                    # -> [dict]
find_actors_by_class('PointLight')                       # -> [str]
get_actor_properties('MyActor')                          # -> dict | None
set_actor_transform('MyActor', location={"x":1,"y":2,"z":3})
get_world_info()                                         # -> dict
```

## UE Python API Quick Reference

### Assets
```python
import unreal
registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path('/Game/MyFolder', recursive=True)
for a in assets:
    print(f"{a.asset_name} ({a.asset_class_path.asset_name})")

unreal.EditorAssetLibrary.load_asset('/Game/Props/Mesh')
unreal.EditorAssetLibrary.duplicate_asset('/Game/Src', '/Game/Dst')
unreal.EditorAssetLibrary.delete_asset('/Game/Old')  # DESTRUCTIVE
```

### Actors
```python
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
actor = sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0,0,0))
actor.set_actor_location(unreal.Vector(100,200,300), False, False)
```

### Materials
```python
import unreal
tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.MaterialInstanceConstantFactoryNew()
mi = tools.create_asset('MI_New', '/Game/Materials', unreal.MaterialInstanceConstant, factory)
```

### Transactions (undo support)
```python
with unreal.ScopedEditorTransaction('Move actors') as trans:
    actor.set_actor_location(unreal.Vector(0,0,100), False, False)
```

## Safety Rules

- **NEVER** delete assets, actors, or files without explicit user confirmation
- **NEVER** modify or overwrite assets without describing exactly what will change first
- For bulk operations, print a preview of affected items and wait for approval before executing
- Wrap state-changing operations in `unreal.ScopedEditorTransaction` so they are undoable
- If an operation fails, show the full traceback — do not silently retry destructive operations

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `ping` returns connection refused | Check UE Editor is running and UnrealBridge plugin is enabled in Plugins menu |
| Script timeout | Long operation — retry with `--timeout 120` |
| `PythonScriptPlugin not available` | Enable "Python Editor Script Plugin" in UE's Plugins |
| Import errors | Check that the module exists in UE's Python path; use `print(sys.path)` to debug |

## Notes

- The Python interpreter is **persistent** — variables from one `exec` carry over to the next
- Use `print()` to return data to the bridge; `unreal.log()` writes to UE's Output Log only
- For structured data, use `import json; print(json.dumps(data))` with `--json` flag

---
name: ue-python
description: Execute Python scripts inside a running Unreal Engine 5.7 editor via TCP bridge. Use when the user asks to interact with UE, manipulate assets, query scenes, automate workflows, or run Python in Unreal.
allowed-tools: Bash Read Write Edit Glob Grep
---

# UE Python Bridge

Execute Python code directly inside a running Unreal Engine 5.7 editor. The bridge communicates over TCP with the UnrealBridge plugin (default port 9876).

## Bridge CLI

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" [options] <command> [args]
```

| Command | Example | Purpose |
|---------|---------|---------|
| `ping` | `bridge.py ping` | Check UE connection |
| `exec` | `bridge.py exec "print('hi')"` | Execute inline Python |
| `exec-file` | `bridge.py exec-file script.py` | Execute a .py file |

Options: `--host`, `--port` (default 9876), `--timeout` (default 30), `--json`

## Workflow

1. **Always ping first** to verify the editor is reachable
2. Use `exec` for short queries, write a temp `.py` + `exec-file` for multi-line scripts
3. Use `--json` when you need to parse structured results
4. stdout = output, stderr = errors. Exit code 0 = success, 1 = error

## API References

**Before writing UE Python code, Read the relevant reference file to find available APIs.** Do not guess API signatures — look them up first.

| Topic | Reference file | When to read |
|-------|---------------|--------------|
| Blueprint queries | `${CLAUDE_SKILL_DIR}/references/bridge-blueprint-api.md` | Getting parent class, class hierarchy, variables, functions, components, overview, execution flow, node search, write operations |
| Asset queries | `${CLAUDE_SKILL_DIR}/references/bridge-asset-api.md` | Using UnrealBridge asset functions |
| UMG / Widget queries | `${CLAUDE_SKILL_DIR}/references/bridge-umg-api.md` | Widget Blueprint hierarchy, widget tree |
| Animation queries | `${CLAUDE_SKILL_DIR}/references/bridge-anim-api.md` | Animation Blueprint state machines, graph nodes, layers, slots, sequences, montages, blend spaces, skeleton |
| DataTable queries | `${CLAUDE_SKILL_DIR}/references/bridge-datatable-api.md` | DataTable schema/rows/fields/columns, search, add/remove/rename/reorder, CSV import/export |
| Material queries | `${CLAUDE_SKILL_DIR}/references/bridge-material-api.md` | Material instance parameters |
| Level / Actor queries | `${CLAUDE_SKILL_DIR}/references/bridge-level-api.md` | Level summary, actor listing/info/transform/components, spawn/destroy/move/attach/duplicate, property get/set, selection, visibility |
| Editor session control | `${CLAUDE_SKILL_DIR}/references/bridge-editor-api.md` | Editor state, opened assets, Content Browser, viewport camera, PIE start/stop, asset open/save/reload, undo/redo, console/CVars, redirector fixup, Blueprint compile |
| GameplayAbilitySystem | `${CLAUDE_SKILL_DIR}/references/bridge-gameplayability-api.md` | GameplayAbility Blueprint metadata (instancing/net policy, tags, cost/cooldown GE); scaffold, extended over time |
| UE Asset API | `${CLAUDE_SKILL_DIR}/references/ue-python-assets.md` | Loading, listing, duplicating, deleting assets; file search |
| UE Actor API | `${CLAUDE_SKILL_DIR}/references/ue-python-actors.md` | Spawning, querying, transforming actors; level loading |
| UE Material API | `${CLAUDE_SKILL_DIR}/references/ue-python-materials.md` | Creating material instances, setting parameters |

## Safety Rules

- **NEVER** delete assets, actors, or files without explicit user confirmation
- **NEVER** modify or overwrite assets without describing what will change first
- Wrap state-changing operations in `unreal.ScopedEditorTransaction` so they are undoable
- If an operation fails, show the full traceback — do not silently retry destructive operations

## Notes

- The Python interpreter is **persistent** — variables carry over between `exec` calls
- `print()` returns data to bridge; `unreal.log()` writes to UE Output Log only
- For structured data: `import json; print(json.dumps(data))` with `--json`
- If `ping` fails: check UE Editor is running with UnrealBridge plugin enabled
- If timeout: retry with `--timeout 120`

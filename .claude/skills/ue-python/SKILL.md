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
5. **All temporary files (`.py` scripts, intermediate data, etc.) MUST be placed in the project root's `temp/` folder (`F:/Claude/ClaudeAgentPythonBase/temp/`), NOT inside the skill directory.**

## API References

**Before writing UE Python code, Read the relevant reference file to find available APIs.** Do not guess API signatures — look them up first.

### Signature verification (mandatory)

Hallucinated parameter lists waste large numbers of bridge round-trips. Enforce these rules for every `unreal.UnrealBridge*Library.*` call:

1. **Look up before you call.** For any bridge function with >2 parameters, or any function you have not called in this session, `Grep` / `Read` the relevant `references/bridge-*-api.md` for its signature before issuing the `exec`.
2. **If the reference doesn't list it,** read the `UFUNCTION` declaration in `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridge*Library.h` — that is the ground truth. Do not infer from the function name.
3. **On the first `TypeError` / `AttributeError`, stop.** Do not append another positional arg and retry. Re-read the signature, fix the call, then retry once. Repeated blind retries are the failure mode this rule exists to prevent.
4. **Prefer one well-formed script over speculative probes.** Write the full sequence in a temp `.py` and use `exec-file` rather than chaining guessy `exec` calls.
5. **If a reference doc contradicts the header,** trust the header and update the doc in the same change.

### Execution discipline (mandatory)

Every task executed through this skill MUST follow these principles — no exceptions:

1. **`exec-file` by default.** Whenever a task needs more than one bridge call, write a `.py` file and use `exec-file`. A single `exec-file` round-trip replaces N chained `exec` calls and collapses N result blobs into one. Only use inline `exec` for a genuinely single one-liner (e.g. `ping`, a single property read).
2. **Minimum tokens, maximum efficiency.** Before any bridge call, ask: "Can I batch this with the next call?" If yes, batch. Do not print large intermediate dumps — print only what you need to make the next decision (GUIDs, booleans, counts). Use `json.dumps` to keep structured output compact.
3. **Read the reference first.** Always `Read`/`Grep` the relevant `references/bridge-*-api.md` (or the C++ header) before invoking a function whose signature is not already confirmed in this session. Reading a doc is cheaper than a failed round-trip.
4. **No API hallucination, no parameter guessing.** Never invent function names, parameter names, parameter order, or parameter counts. If you are not certain of a signature, look it up. A guessed call that errors burns a full round-trip and often leaks a long traceback into context.
5. **Stop on the first signature error.** `TypeError` / `AttributeError` means your assumption is wrong. Do not retry with another guess — re-read the signature, then issue one corrected call.

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
| Reactive handlers | `${CLAUDE_SKILL_DIR}/references/bridge-reactive.md` | Register Python scripts that fire on UE events (GameplayEvent, AnimNotify, MovementMode, Attribute, ActorLifecycle, InputAction). Read before using any `UnrealBridgeReactiveLibrary.*` call. |
| Navigation | `${CLAUDE_SKILL_DIR}/references/bridge-navigation-api.md` | Export the current level's NavMesh as Wavefront OBJ |
| Agent / Gameplay | `${CLAUDE_SKILL_DIR}/references/bridge-gameplay-api.md` | PIE player-pawn observation + navmesh path planning for agent loops |
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

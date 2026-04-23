---
name: unreal-bridge
description: Execute Python scripts inside a running Unreal Engine 5.7 editor via TCP bridge. Use when the user asks to interact with UE, manipulate assets, query scenes, automate workflows, or run Python in Unreal.
allowed-tools: Bash Read Write Edit Glob Grep
---

# UnrealBridge

Execute Python code directly inside a running Unreal Engine 5.7 editor. The bridge auto-discovers the editor via UDP multicast (`239.255.42.99:9876`) so the TCP data port is picked by the OS at runtime — zero port-conflict across multiple editors.

## Bridge CLI

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" [options] <command> [args]
```

| Command | Example | Purpose |
|---------|---------|---------|
| `ping` | `bridge.py ping` | Check UE connection (TCP-only, doesn't touch GameThread) |
| `exec` | `bridge.py exec "print('hi')"` | Execute inline Python |
| `exec-file` | `bridge.py exec-file script.py` | Execute a .py file |
| `gamethread-ping` | `bridge.py gamethread-ping` | Probe GameThread liveness (bypasses exec queue) |
| `resume` | `bridge.py resume` | Unstick a paused BP breakpoint (bypasses exec queue) |
| `list-editors` | `bridge.py list-editors` | Send a discovery probe; print every editor that responded |

Discovery is automatic — the common case needs no flags. Overrides (all optional):

| Flag | Env fallback | Purpose |
|---|---|---|
| `--project=<name\|path>` | `UNREAL_BRIDGE_PROJECT` | Disambiguate when >1 editors are running |
| `--endpoint=host:port` | `UNREAL_BRIDGE_ENDPOINT` | Skip discovery, connect directly |
| `--token=<secret>` | `UNREAL_BRIDGE_TOKEN` | Required only when the server binds non-loopback |
| `--discovery-timeout=<ms>` | — | Probe wait window (default: 800ms) |
| `--discovery-group=host:port` | `UNREAL_BRIDGE_DISCOVERY_GROUP` | Override the multicast group |
| `--timeout=<s>` | — | Per-request TCP timeout (default: 30s) |
| `--json` | — | Machine-readable output |

### Multi-editor disambiguation

If two or more editors are running, the first `exec` / `ping` fails with a list — re-run with `--project=<name>` or set `UNREAL_BRIDGE_PROJECT`. Filter matches against the project's short name, its full `.uproject` path, a path suffix, or a case-insensitive name substring.

### When discovery fails

Discovery uses UDP multicast; some firewalls / VPNs / tightly-segmented networks block it. Symptoms: `discovery: no UnrealBridge editors found on the LAN`. Work around with an explicit `--endpoint=127.0.0.1:<port>` — read the port from the editor's `LogUnrealBridgeModule: Server up on 127.0.0.1:XXXXX` startup line, or from the editor's Output Log.

### Diagnosing a stuck `exec`

`exec` is serialized through a single GameThread ticker queue, so when a script blocks the GT (modal dialog, pure-Python loop, deadlock), all further `exec` calls just sit in the queue and the client eventually sees a recv timeout with no diagnostic. Use these **non-exec** commands from a *separate terminal* to find out what state the editor is in:

- `bridge.py ping` — TCP works → server process is alive
- `bridge.py gamethread-ping` — also dispatches to GT but with its own short timeout (default 2s, `--probe-timeout` to override). Returns `alive (X ms)` or `unresponsive`. High latency on `alive` means the GT is mid-exec but pumping TaskGraph (asset load / BP compile inside Python) — the queue will drain when that exec finishes.
- `bridge.py resume` — recovery for the specific case where a Blueprint breakpoint has paused execution inside the editor's nested Slate debug loop.

## Workflow

1. **Always ping first** to verify the editor is reachable
2. Use `exec` for short queries, write a temp `.py` + `exec-file` for multi-line scripts
3. Use `--json` when you need to parse structured results
4. stdout = output, stderr = errors. Exit code 0 = success, 1 = error
5. **All temporary files (`.py` scripts, intermediate data, etc.) MUST be placed in the project root's `temp/` folder, NOT inside the skill directory.**

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
6. **Never bail to raw `unreal.*` when an `UnrealBridge*Library` wrapper exists.** If a bridge call returns empty / errors / "feels wrong", the failure is almost always in your **parameters** (wrong scope, wrong path, wrong filter), not in the wrapper. Re-read the reference and fix the call. Falling back to `unreal.AssetRegistryHelpers` / `unreal.EditorAssetLibrary` / `unreal.EditorUtilityLibrary` / `unreal.SystemLibrary` to "do it yourself" bypasses the documented surface, hides the real bug, and burns far more round-trips walking large registries than the bridge call would have. If you genuinely believe no bridge function covers your case, **ask the user before reaching for raw `unreal.*`** — the answer is often "use this other bridge function I forgot to list."

### Common antipatterns (each cost real round-trips in practice)

| Symptom | Wrong reflex | Right move |
|---------|--------------|------------|
| `TypeError: required argument 'X' (pos N) not found` | Append `False` / `0` / `''` and retry | Stop, `Read` the `references/bridge-*-api.md` row, write **one** corrected call |
| `AttributeError: type object 'BridgeXxxScope' has no attribute 'YYY'` | Try another guessed enum name | Read the enum table in the reference doc — never invent enum members |
| Bridge call returns `[]` / `None` for an asset/actor/BP you know exists | Drop to `unreal.AssetRegistryHelpers.get_assets_by_path` and walk it | Suspect your scope/filter first. For asset name lookup the default is `search_assets_in_all_content(name, max_results)` with `ALL_ASSETS` scope (plugins live outside `/Game`) |
| Want to do a multi-step BP edit (spawn + connect + position) | Chain 3 `exec` calls inline | Write a `.py` in the project's `temp/` and use `exec-file` once |

| Topic | Reference file | When to read |
|-------|---------------|--------------|
| Blueprint queries | `${CLAUDE_SKILL_DIR}/references/bridge-blueprint-api.md` | Getting parent class, class hierarchy, variables, functions, components, overview, execution flow, node search, write operations, cross-BP call-site search, function invocation (verify behavior without PIE) |
| Asset queries | `${CLAUDE_SKILL_DIR}/references/bridge-asset-api.md` | Using UnrealBridge asset functions |
| UMG / Widget queries | `${CLAUDE_SKILL_DIR}/references/bridge-umg-api.md` | Widget Blueprint hierarchy, widget tree |
| Animation queries | `${CLAUDE_SKILL_DIR}/references/bridge-anim-api.md` | Animation Blueprint state machines, graph nodes, layers, slots, sequences, montages, blend spaces, skeleton. **Authoring / modifying an ABP? Read the "Authoring an Animation Blueprint (agent workflow)" section top-to-bottom first** — it's the workflow + verification checklist that keeps AI-generated ABPs from compiling with "transition will never be taken" or rendering as T-pose. |
| DataTable queries | `${CLAUDE_SKILL_DIR}/references/bridge-datatable-api.md` | DataTable schema/rows/fields/columns, search, add/remove/rename/reorder, CSV import/export |
| Material queries | `${CLAUDE_SKILL_DIR}/references/bridge-material-api.md` | Material instance parameters |
| Level / Actor queries | `${CLAUDE_SKILL_DIR}/references/bridge-level-api.md` | Level summary, actor listing/info/transform/components, spawn/destroy/move/attach/duplicate, property get/set, selection, visibility |
| Editor session control | `${CLAUDE_SKILL_DIR}/references/bridge-editor-api.md` | Editor state, opened assets, Content Browser, viewport camera, PIE start/stop, asset open/save/reload, undo/redo, console/CVars, redirector fixup, Blueprint compile |
| GameplayAbilitySystem | `${CLAUDE_SKILL_DIR}/references/bridge-gameplayability-api.md` | GameplayAbility Blueprint metadata (instancing/net policy, tags, cost/cooldown GE); scaffold, extended over time |
| Reactive handlers | `${CLAUDE_SKILL_DIR}/references/bridge-reactive.md` | Register Python scripts that fire on UE events (GameplayEvent, AnimNotify, MovementMode, Attribute, ActorLifecycle, InputAction). Read before using any `UnrealBridgeReactiveLibrary.*` call. |
| Navigation | `${CLAUDE_SKILL_DIR}/references/bridge-navigation-api.md` | Export the current level's NavMesh as Wavefront OBJ |
| Perf snapshots | `${CLAUDE_SKILL_DIR}/references/bridge-perf-api.md` | Structured FPS / GT / RT / GPU / draw calls / memory / UObject class histogram. Replaces parsing `stat unit` text. |
| Agent / Gameplay | `${CLAUDE_SKILL_DIR}/references/bridge-gameplay-api.md` | **Mandatory before driving the player pawn** (movement, camera steering, IA injection, "go to a target", sticky inputs). PIE observation + navmesh path planning for agent loops also live here. |
| UE Asset API | `${CLAUDE_SKILL_DIR}/references/ue-python-assets.md` | Loading, listing, duplicating, deleting assets; file search |
| UE Actor API | `${CLAUDE_SKILL_DIR}/references/ue-python-actors.md` | Spawning, querying, transforming actors; level loading |
| UE Material API | `${CLAUDE_SKILL_DIR}/references/ue-python-materials.md` | Creating material instances, setting parameters |

> **Pawn control is non-obvious — read `bridge-gameplay-api.md` first.** Driving the player (sticky `IA_Move`, `apply_look_input`, navigating to a moving target, holding an input over time) has hard constraints that a fresh read of the API will not reveal: `bridge.exec` runs on the GameThread so any `time.sleep` inside an exec freezes the engine and stops the sticky ticker; continuous steering must run from a reactive `register_runtime_timer` callback, not a Python `while` loop; `IA_Move` is camera-relative and the forward-axis convention varies per project. The "Pattern: chase a (possibly moving) target and stop on arrival" section has the working template — start there before writing your own loop.

> **Asset lookup by name defaults to `search_assets_in_all_content(name, max_results)` — read `bridge-asset-api.md` before searching.** When the user names an asset without giving a path, do **not** call `unreal.AssetRegistryHelpers.get_assets_by_path('/Game', recursive=True)` and filter in Python — that walks 100k–2M+ entries and times out. The full `search_assets` form needs `BridgeAssetSearchScope.ALL_ASSETS` (not `PROJECT`) when the asset might live in a plugin mount (`/PluginName/...`). `PROJECT` scope only covers `/Game`; using it for a plugin asset returns `[]` silently. The valid scope members are exactly `ALL_ASSETS`, `PROJECT`, `CUSTOM_PACKAGE_PATH` — there is no `GAME_FOLDER`.

## Blueprint review loop (mandatory after any BP authoring)

When you **author or modify a Blueprint graph** (spawn / connect / remove nodes, add variables, create functions), you MUST run this loop before calling the task done. AI-generated BPs default to a visually chaotic, maintainability-light shape — the review loop is what turns them into code a human will want to maintain.

**Auto-layout is not optional.** Every BP edit — even a single `add_call_function_node` or `connect_graph_pins` — leaves nodes at spawn-time XY (often stacked at origin or wherever a caller guessed). Before any reply claims "done", `auto_layout_graph` must have run on every function / event graph you touched in that session. If you edited three graphs, you run auto-layout three times. No exceptions for "just one tiny change".

```
1. plan        — list events, functions, local vars as text before touching the bridge
2. build       — add_*_node / connect_graph_pins / add_blueprint_variable …
3. auto_layout — MANDATORY after ANY graph mutation. Flow for pin_aligned:
                   exec A:  open_asset(bp)                       # if BP editor not open
                   ~2 s  :  client-side time.sleep
                   exec B:  open_function_graph_for_render(bp, graph)
                   ~3 s  :  client-side time.sleep so Slate ticks the new widgets
                   exec C:  auto_layout_graph(bp, graph, 'pin_aligned', '', 100, 48)
                 Skip exec A if the BP editor is already open (typical mid-session).
                 open_function_graph_for_render returns False on a fresh editor
                 launch until open_asset registers the BP editor tab — do NOT
                 poll it in a loop, polling alone won't make it register faster.
                 Repeat exec B + exec C for every graph you mutated this session.
                 # Single-exec 'exec_flow' is acceptable ONLY for huge graphs where
                 # pixel-accurate alignment doesn't matter; always follow it with
                 # straighten_exec_chain on the main rail.
4. lint        — lint_blueprint(bp, '', -1, -1, -1) and resolve every finding
5. collapse    — for any LongExecChain finding, collapse_nodes_to_function
6. straighten  — straighten_exec_chain for each main exec rail (only needed after 'exec_flow')
7. comment     — add_comment_box + set_comment_box_color for each section
8. compile     — confirm compile_blueprints returns clean
# auto_insert_reroutes is intentionally NOT in the loop. Empirically on
# dense math-heavy graphs it inserts dozens of knots to avoid straight-line
# wire-through-node violations, but the resulting many-knot routing reads
# worse than the original. Opt in per-graph if a specific case needs it.
```

- **Lint is non-optional.** If `lint_blueprint` returns any `warning` or `error`, fix or explicitly accept each one (write a justification in a comment box). Do not hand the BP back with unresolved warnings.
- **Name things.** `UnnamedCustomEvent` / `UnnamedFunction` findings mean the agent left a placeholder — always rename to describe intent (`OnHealthChanged`, not `CustomEvent_0`).
- **Comment boxes are section titles.** Any graph > 10 nodes must have at least one comment box with a meaningful title (e.g. `"1. Validate inputs"`, `"2. Apply damage"`) and an appropriate preset color via `set_comment_box_color` (`Section`, `Validation`, `Danger`, `Network`, `UI`, `Debug`, `Setup`).
- **Pick the right layout strategy.** `auto_layout_graph` with `'pin_aligned'` uses DFS-ordered leaves + downstream-driven Y alignment + live Slate geometry — matches hand-laid BP shape (`.then` rails horizontal, siblings stacked in a single column across exec layers). Requires the three-exec flow (open graph, sleep, layout) to read the live widgets. Use `'exec_flow'` (Sugiyama-lite, layer-center Y) for bulk tidy on large graphs where pixel-accurate alignment matters less — then follow with `straighten_exec_chain` on the main rail.
- **Compact by spacing, not by node.** `pin_aligned` treats the user's `h_spacing` as a loose upper bound and internally uses `DataHSpace = max(15, h/3)` and `ExecGap = max(30, h/2)`. So `h_spacing=100` renders tighter than you'd expect from the raw number. Pass smaller values (e.g. `40, 32`) when you want maximum density.
- **Size predict before spawning.** If you're placing several nodes in a row by hand, call `predict_node_size` for each kind first so your X offsets don't overlap.
- **Post-layout geometry reads.** After `auto_layout_graph` runs, the Slate widgets don't refresh `NodePosX/Y` until they tick — so `get_rendered_node_info` returns *pre-layout* pin coords in the same exec. For crossing-detection, wire-length audits, or overlap checks right after layout, read `get_node_layout(bp, fn, guid).pos_*` (authoritative from the node model) and estimate pin Y as `pos_y + 40 + 22 × dir_index`. Only pay the open-graph + sleep cost when you specifically need Slate-accurate coords.

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

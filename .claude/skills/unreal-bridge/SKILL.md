---
name: unreal-bridge
description: Execute Python scripts inside a running Unreal Engine 5.7 editor via TCP bridge. Use when the user asks to interact with UE, manipulate assets, query scenes, automate workflows, or run Python in Unreal.
allowed-tools: Bash Read Write Edit Glob Grep
---

# UnrealBridge

Execute Python code directly inside a running Unreal Engine 5.7 editor. The bridge auto-discovers the editor via UDP multicast (`239.255.42.99:9876`) so the TCP data port is picked by the OS at runtime — zero port-conflict across multiple editors.

## Preconditions (check before first use)

Three things must all be true for `bridge.py` to reach the editor. If any `bridge.py` call returns `discovery: no UnrealBridge editors found`, walk this list with the user in order — don't start troubleshooting Python or firewalls first.

1. **Plugin is installed in the target UE project.** The plugin source lives in `Plugin/UnrealBridge/` in the repo that ships this skill. It must be copied into the user's `<UEProject>/Plugins/UnrealBridge/` (the repo ships `sync_plugin.bat` for this; the user has to edit its `DST=` line once). `ls <UEProject>/Plugins/UnrealBridge/UnrealBridge.uplugin` should exist.
2. **Plugin is enabled in the .uproject.** The `EnabledByDefault: true` in `UnrealBridge.uplugin` covers this on a fresh plugin install, but a previously-disabled plugin stays disabled. Check the `"Plugins"` block of `<UEProject>/<Project>.uproject` for an `{"Name":"UnrealBridge", "Enabled":false}` override and flip it to `true` if present.
3. **Editor is running and past MainFrame init.** `bridge.py ping` returns `"ready": true` in the JSON when the editor's window is fully up. `"ready": false` means the editor is still loading — wait 10–60s and retry. The editor also has to be the one built against a project that has the plugin — if the user launched a different `.uproject`, discovery will find nothing.

Python requirements: **Python 3.7+** with the stdlib — no third-party packages. The skill scripts use only `socket`, `json`, `struct`, `uuid`, `hashlib`, `subprocess`, `pathlib`.

If you see `discovery: no UnrealBridge editors found on the LAN`, work the list above. Only fall back to `--endpoint=127.0.0.1:<port>` (reading the port from the editor log line `LogUnrealBridge: Listening on 127.0.0.1:<port>`) if you've confirmed the plugin is loaded but multicast is somehow being dropped (rare — corporate VPNs, virtual network interfaces).

## Bridge CLI

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" [options] <command> [args]
```

| Command | Example | Purpose |
|---------|---------|---------|
| `ping` | `bridge.py ping` | Check UE connection (TCP-only, doesn't touch GameThread) |
| `exec` | `bridge.py exec "print('hi')"` | Execute a single inline statement |
| `exec --stdin` | `bridge.py exec --stdin <<EOF ... EOF` | Execute multi-line script from stdin (no temp file) |
| `exec-file` | `bridge.py exec-file script.py` | Execute a .py file (use when you'll iterate or keep) |
| `preflight` | `bridge.py preflight script.py` | Lint a script for bridge-call errors WITHOUT sending to UE |
| `suggest` | `bridge.py suggest AssetRegistry` | Look up the bridge equivalent for a raw `unreal.*` fallback pattern |
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
| `--no-preflight` | — | Disable AST preflight before send (rare; preflight is on by default) |

### Multi-editor disambiguation

If two or more editors are running, the first `exec` / `ping` fails with a list — re-run with `--project=<name>` or set `UNREAL_BRIDGE_PROJECT`. Filter matches against the project's short name, its full `.uproject` path, a path suffix, or a case-insensitive name substring.

### When discovery fails

Discovery uses UDP multicast; some firewalls / VPNs / tightly-segmented networks block it. Symptoms: `discovery: no UnrealBridge editors found on the LAN`. First walk the **Preconditions** section above — the vast majority of discovery failures are "editor not running" or "plugin not enabled", not networking. If those all check out, fall back to an explicit `--endpoint=127.0.0.1:<port>` — read `<port>` from the editor's startup log (`LogUnrealBridge: Listening on 127.0.0.1:<port>`).

### Diagnosing a stuck `exec`

`exec` is serialized through a single GameThread ticker queue, so when a script blocks the GT (modal dialog, pure-Python loop, deadlock), all further `exec` calls just sit in the queue and the client eventually sees a recv timeout with no diagnostic. Use these **non-exec** commands from a *separate terminal* to find out what state the editor is in:

- `bridge.py ping` — TCP works → server process is alive
- `bridge.py gamethread-ping` — also dispatches to GT but with its own short timeout (default 2s, `--probe-timeout` to override). Returns `alive (X ms)` or `unresponsive`. High latency on `alive` means the GT is mid-exec but pumping TaskGraph (asset load / BP compile inside Python) — the queue will drain when that exec finishes.
- `bridge.py resume` — recovery for the specific case where a Blueprint breakpoint has paused execution inside the editor's nested Slate debug loop.

## Workflow

1. **Always ping first** to verify the editor is reachable.
2. Pick the right execution mode (see decision tree below) — `exec --stdin` is the default for anything multi-line.
3. Use `--json` when you need to parse structured results.
4. stdout = output, stderr = errors. Exit codes: `0`=success, `1`=transport/runtime error, `2`=bad CLI args, `3`=AST preflight rejected the script before send.

### Execution modes — decision tree

There are four distinct surfaces. Pick by **how the script is shaped** and **whether you'll run it more than once**:

```
Want to execute Python in UE? Answer in order:

1. Single statement / single API call?
   → bridge.py exec "<code>"

2. Multi-step logic, one-shot, throwaway?
   → bridge.py exec --stdin <<'EOF' ... EOF       ← DEFAULT for any >1-line script
     (or: bridge.py exec - <<'EOF' ... EOF — `-` is shorthand for --stdin)

3. Multi-step logic, you'll iterate / debug / rerun multiple times?
   → Write to temp/<name>.py + bridge.py exec-file temp/<name>.py
     (temp/ is NOT auto-cleaned; tidy when you finish a debugging session)

4. Deliverable script (demo reproduction, long-term asset, user wants to keep)?
   → Write to a project location (NOT temp/ — temp/ is conventionally ephemeral),
     then bridge.py exec-file <path>
```

Mode 2 (`--stdin`) is the **new default for anything multi-line**. It eliminates the per-script temp-file accumulation that the previous "always exec-file" guidance produced.

#### Paradigm: Multi-step one-shot (the 80% case)

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec --stdin <<'EOF'
import unreal
from unreal_bridge import Asset, Level
paths, _ = Asset.search_assets_in_all_content(query="Hero", max_results=5)
for p in paths:
    print(str(p))
EOF
```

#### Paradigm: Iterate-and-rerun (when you'll edit + run again)

```bash
# Write temp/inspect_hero.py first, then:
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec-file temp/inspect_hero.py
# Edit the file & rerun freely. Clean it up when you're done.
```

#### Paradigm: Deliverable (kept long-term)

```bash
# Write to a project-appropriate location, e.g. scripts/build_demo.py — NOT temp/.
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec-file scripts/build_demo.py
```

## API surface — use the wrapper module first

Two ways to call bridge functions:

**Preferred: `unreal_bridge` wrapper module** (auto-loaded inside UE Python, kwargs-only):

```python
from unreal_bridge import Asset, Level, Blueprint, Editor, Anim, Material, ...
paths, _ = Asset.search_assets_in_all_content(query="Hero", max_results=20)
info = Level.get_actor_info(actor_path="/Persistent/Player")
```

The wrapper has 14 classes (one per `UnrealBridge*Library`) with kwargs-only signatures — calling with positional args raises a `TypeError` immediately at the Python layer, before any bridge round-trip. **This is the structural fix for positional-arg-order hallucinations.** Regenerate after C++ header changes via `python tools/gen_manifest.py`.

**Fallback: raw `unreal.UnrealBridge*Library.foo(...)`** (still supported; AST preflight catches errors before send).

## AST preflight (automatic, on by default)

Every `exec` / `exec --stdin` / `exec-file` call runs an AST preflight in the bridge client BEFORE sending the script to UE. Preflight reads `scripts/bridge_manifest.json` (auto-generated from UE reflection) and rejects the call locally — exit code 3, no UE round-trip — when:

- Function name doesn't exist on the named library (with did-you-mean candidates)
- Library name unknown (with did-you-mean)
- Required args missing (with full signature shown)
- Unknown kwarg name (with did-you-mean + signature)
- Same param given both positionally and by kwarg
- `unreal.BridgeXxx.YYY` enum member doesn't exist (with valid members listed)

What this means for you: **structural call errors are caught before they cost a round-trip, and the rejection message contains the right signature**. There's no "guess and retry" loop for these cases — you get the answer in the first error.

To preview without sending: `bridge.py preflight script.py`. To bypass (rare; debugging the preflight itself, or running deliberately weird code): `bridge.py --no-preflight exec ...`.

Type validation is NOT done by preflight (the manifest doesn't carry param types). Wrong asset paths, wrong enum scopes for context, etc. still surface only at runtime — references prose still matters for those.

## Reference docs (for semantics, not signatures)

Signatures are now mechanically enforced. **References still carry the things preflight can't**: token-cost warnings, scope traps (PROJECT vs ALL_ASSETS for plugin assets), workflow patterns (auto_layout three-step), known limitations.

Use references to answer "**how should I use this?**" — not "**what's the signature?**" (the wrapper / preflight has the signature).

### Common semantic traps (preflight does NOT catch these)

| Symptom | What's actually wrong | Right move |
|---------|----------------------|------------|
| Bridge call returns `[]` / `None` for an asset you know exists | Wrong scope: `PROJECT` only covers `/Game`, plugin assets need `ALL_ASSETS` | Use `Asset.search_assets_in_all_content(query, max_results)` (defaults to `ALL_ASSETS`) |
| `get_derived_classes` is hanging / returning massive results | Passed `UObject` / `AActor` / `UActorComponent` as base — walks the entire class tree | Narrow to the most specific base you care about |
| Multi-step BP edit feels chatty | Chaining 3 inline `exec` calls instead of batching | Write to `temp/X.py` + `exec-file` once (Mode 3) or use `--stdin` heredoc (Mode 2) |
| Pawn movement script freezes the editor | Used `time.sleep` inside a single `exec` (blocks GameThread) | See `references/bridge-gameplay-api.md` "Pattern: chase a target" — use `register_runtime_timer` instead |

### Never silently bail to raw `unreal.*` when a bridge wrapper exists

If a bridge call returns empty / errors / "feels wrong", suspect **your parameters** (wrong scope, path, filter), not the wrapper. Falling back to `unreal.AssetRegistryHelpers` / `unreal.EditorAssetLibrary` / `unreal.SystemLibrary` to "do it yourself" bypasses the documented surface, hides the real bug, and often walks 100k–2M registry entries the bridge call would have skipped.

**The bridge enforces this automatically:** AST preflight prints a `[WARN]` whenever it spots a known raw-fallback pattern (e.g. `unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(...)`) and shows the bridge equivalent inline. The script still runs (warning, not error), but if you see one of these warnings on a call you wrote, switch to the suggested bridge form on the next iteration.

**To look up a replacement before writing a call:**

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" suggest AssetRegistry      # filter
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" suggest                    # list all
```

Each result includes the exact bridge call with kwargs and a one-line "why". Patterns currently mapped (auto-extends as new fallbacks surface in `Saved/UnrealBridge/exec.log`):

| Raw pattern | Bridge equivalent |
|---|---|
| `ar = unreal.AssetRegistryHelpers.get_asset_registry(); ar.get_assets_by_path(...)` | `Asset.list_assets_under_path(folder_path=..., include_subfolders=...)` |
| `ar.get_referencers(...)` / `unreal.AssetRegistryHelpers.get_referencers(...)` | `Asset.get_package_referencers(package_name=..., hard_only=...)` |
| `ar.get_dependencies(...)` / `unreal.AssetRegistryHelpers.get_dependencies(...)` | `Asset.get_package_dependencies(package_name=..., hard_only=...)` |
| `unreal.GameplayStatics.get_all_actors_of_class(...)` | `Level.find_actors_by_class(class_path=..., max_results=-1)` |
| `unreal.GameplayStatics.get_all_actors_with_tag(...)` | `Level.find_actors_by_tag(tag=...)` |

If you genuinely believe no bridge function covers your case, **ask the user before reaching for raw `unreal.*`** — the answer is often "use this other bridge function I forgot to list."

### Reference index — read these for usage / traps / workflow patterns

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

## Blueprint authoring policy (MUST read before touching BP write ops)

**非必要不写蓝图 Node 和 Graph。** Even with `auto_layout_graph` + lint + the review loop below, agent-authored BPs are visibly worse than human-authored or C++-equivalent implementations: mechanical layout without project-aesthetic judgment, bland function/event naming, weak section grouping, no sense for when to collapse vs. inline, BP-VM performance overhead on hot paths.

**When the user says "用蓝图实现 X"**, the default reply is NOT "ok, spawning nodes." It is a short exchange:

1. **State the tradeoff up-front.** Acknowledge that the bridge has full graph-write primitives (spawn / connect / auto-layout / lint / compile), but agent-generated Graphs are measurably weaker than a C++ equivalent on readability, maintainability, and performance.
2. **Offer the alternative.** Propose implementing the logic in C++ (a new `UCLASS` / `UFUNCTION(BlueprintCallable)`) with a thin BP wrapper for exposure; or using another bridge-authoring surface (Material / Anim / UMG / DataTable / Level) when the task fits there — those are areas the agent does handle well.
3. **Re-confirm explicitly.** Ask: "你仍然希望我用蓝图 Node/Graph 实现，还是改走 C++ / 其他方式？" Do NOT decide unilaterally.
4. **Only proceed on explicit user insistence.** If the user says "就用蓝图写" or equivalent, then run the review loop below. Otherwise, redirect to C++ or the suggested alternative.

**Exceptions** (skip the confirmation dance — just do it):
- Pure data writes: `set_blueprint_variable_default` / `set_component_property` / `add_blueprint_variable` without any new graph nodes.
- Bulk / automation scenarios the user explicitly framed as automation (e.g. "扫 100 个 BP 改某变量默认值").
- Standing authorization in `CLAUDE.md` or in the current conversation ("this session you can edit BPs freely").

The hard line: **any op that spawns / connects / removes graph nodes, adds events, or creates functions** needs the confirmation-first dance unless one of the three exceptions above applies.

## Blueprint review loop (mandatory after any BP authoring)

Applies once the user has confirmed they want a BP graph written (see policy above). When you **author or modify a Blueprint graph** (spawn / connect / remove nodes, add variables, create functions), you MUST run this loop before calling the task done. AI-generated BPs default to a visually chaotic, maintainability-light shape — the review loop is what turns them into code a human will want to maintain.

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

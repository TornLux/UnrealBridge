---
name: unreal-bridge
description: Execute Python scripts inside a running Unreal Engine 5.7 editor via TCP bridge. Use when the user asks to interact with UE, manipulate assets, query scenes, automate workflows, or run Python in Unreal.
---

# UnrealBridge

Execute Python directly inside a running UE 5.7 editor. Auto-discovers via UDP multicast (`239.255.42.99:9876`); the TCP data port is OS-assigned per editor.

## Preconditions

If `bridge.py` returns `discovery: no UnrealBridge editors found`, walk these in order 鈥?don't troubleshoot Python or firewalls first:

1. **Plugin installed** in `<UEProject>/Plugins/UnrealBridge/` (use `sync_plugin.bat` from this repo; user must edit its `DST=` once). Check: `<UEProject>/Plugins/UnrealBridge/UnrealBridge.uplugin` exists.
2. **Plugin enabled** 鈥?check `<UEProject>/<Project>.uproject` `"Plugins"` block for `{"Name":"UnrealBridge", "Enabled":false}` and flip if present.
3. **Editor up and ready** 鈥?`bridge.py ping` returns `"ready": true`. `false` means MainFrame still loading; wait 10鈥?0s.

Last resort if multicast is blocked (corp VPN, virtual NIC): `--endpoint=127.0.0.1:<port>` from the editor log line `LogUnrealBridge: Listening on 127.0.0.1:<port>`. Python 3.7+ stdlib only.

## Bridge CLI

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" [options] <command> [args]
```

| Command | Purpose |
|---------|---------|
| `ping` | Check UE connection (TCP-only, doesn't touch GameThread) |
| `exec "<code>"` | Execute single inline statement |
| `exec --stdin <<'EOF' ... EOF` | Multi-line script from stdin (default for >1 line; `-` is shorthand for `--stdin`) |
| `exec-file <path>` | Execute a .py file (use when iterating, debugging, or keeping the script) |
| `preflight <path>` | Lint a script for bridge-call errors WITHOUT sending to UE |
| `suggest [pattern]` | Look up the bridge equivalent for a raw `unreal.*` fallback |
| `gamethread-ping` | Probe GameThread liveness (bypasses exec queue; use when `exec` hangs) |
| `resume` | Unstick a paused BP breakpoint |
| `list-editors` | Print every editor that responded to a discovery probe |
| `wait-compile <material>` / `wait-pose-index <psd>` | Client-side polling helpers |

Optional flags: `--project=<name|path>` (disambiguate when >1 editors run; or env `UNREAL_BRIDGE_PROJECT`), `--endpoint=host:port`, `--token=<secret>`, `--timeout=<s>`, `--json`, `--no-preflight`.

## Workflow

1. **Always ping first.**
2. Pick the execution mode by shape (table above): `exec` for one-liners, `exec --stdin` for multi-line one-shots (the **80% case**), `exec-file` only when you'll iterate / keep.
3. `--json` for parseable output.
4. Exit codes: `0` success 路 `1` runtime/transport 路 `2` bad CLI args 路 `3` AST preflight rejected.
5. **If `exec` hangs**: from a separate terminal try `gamethread-ping` (high latency = GT mid-exec, queue will drain) or `resume` (BP breakpoint).

Multi-line example:

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec --stdin <<'EOF'
from unreal_bridge import Asset
paths, _ = Asset.search_assets_in_all_content(query="Hero", max_results=5)
for p in paths:
    print(p.export_text())
EOF
```

## API surface 鈥?use the wrapper module first

```python
from unreal_bridge import Asset, Level, Blueprint, Editor, Anim, Material, PoseSearch, Chooser, ...
paths, _ = Asset.search_assets_in_all_content(query="Hero", max_results=20)
```

The wrapper has 16 classes (one per `UnrealBridge*Library`) with **kwargs-only signatures** 鈥?positional args raise `TypeError` immediately, no UE round-trip. This is the structural fix for positional-arg-order hallucinations. Regenerate after C++ header changes via `python tools/gen_manifest.py`.

Fallback: raw `unreal.UnrealBridge*Library.foo(...)` works (preflight catches errors), but prefer the wrapper.

## AST preflight (automatic)

Every `exec*` call runs an AST preflight in the bridge client BEFORE sending. Reads `scripts/bridge_manifest.json` and rejects locally (exit 3, no UE round-trip) when:

- Function/library name doesn't exist (with did-you-mean)
- Required args missing / unknown kwarg / param given twice
- `unreal.BridgeXxx.YYY` enum member doesn't exist

Type validation is NOT done by preflight. Wrong asset paths / wrong scope still surface at runtime 鈥?references prose still matters for those.

Bypass with `--no-preflight` (rare). Preview with `bridge.py preflight <path>`.

## Common semantic traps (preflight does NOT catch)

| Symptom | Fix |
|---------|-----|
| Bridge call returns `[]` for an asset you know exists | Wrong scope: `PROJECT` covers `/Game` only; plugin assets need `ALL_ASSETS`. Use `Asset.search_assets_in_all_content(...)`. |
| `get_derived_classes` hangs / huge results | Don't pass `UObject` / `AActor` 鈥?narrow to most specific base. |
| Multi-step BP edit feels chatty | Batch with `exec --stdin` heredoc or `exec-file`, not 3 inline `exec` calls. |
| Pawn movement script freezes the editor | `time.sleep` inside `exec` blocks GameThread 鈥?see `bridge-gameplay-api.md` "chase a target" pattern (use `register_runtime_timer`). |

## Reading UE object attributes 鈥?never `<obj>.<attr>`

Guessing attribute names on a UE object reference is the **#1 remaining hallucination source**. The bridge has a generic property pipeline:

```python
from unreal_bridge import Level
props = Level.list_actor_properties(actor_name='/Persistent/MyActor')
# OR before spawning:
props = Level.list_class_properties(class_path='/Script/Engine.SkyAtmosphere')
# Read by name (dotted paths supported):
loc = Level.get_actor_property(actor_name='/Persistent/MyActor',
                               property_path='RootComponent.RelativeLocation')
```

If you find yourself typing `<some_var>.attribute_name` on a UE object 鈥?**stop**, list properties first.

## Never silently bail to raw `unreal.*`

If a bridge call returns empty / errors, suspect **your parameters** (wrong scope / path / filter), not the wrapper. Falling back to `unreal.AssetRegistryHelpers` etc. bypasses preflight, audit logging, and often walks 100k鈥?M registry entries.

Preflight emits `[WARN]` on known raw-fallback patterns and shows the bridge equivalent inline. To look up before writing: `bridge.py suggest [pattern]`. Patterns currently mapped:

| Raw pattern | Bridge equivalent |
|---|---|
| `ar.get_assets_by_path(...)` | `Asset.list_assets_under_path(folder_path, include_subfolders)` |
| `ar.get_referencers(...)` | `Asset.get_package_referencers(package_name, hard_only)` |
| `ar.get_dependencies(...)` | `Asset.get_package_dependencies(package_name, hard_only)` |
| `unreal.GameplayStatics.get_all_actors_of_class(...)` | `Level.find_actors_by_class(class_path, max_results)` |
| `unreal.GameplayStatics.get_all_actors_with_tag(...)` | `Level.find_actors_by_tag(tag)` |

If you genuinely believe no bridge function covers your case, **ask the user before reaching for raw `unreal.*`**.

## Reference index 鈥?read these for usage / traps / workflow patterns

Signatures are now mechanically enforced (preflight). References carry semantic traps, scope warnings, and workflow patterns preflight can't catch.

| Topic | File | When to read |
|-------|------|------|
| Blueprint queries + authoring | `references/bridge-blueprint-api.md` | Class hierarchy, variables/functions/components, node search, write ops, auto-layout flow, lint loop |
| Asset queries | `references/bridge-asset-api.md` | Asset lookup, search, references/dependencies, **`SoftObjectPath` stringification** (top-of-file block) |
| UMG / Widget | `references/bridge-umg-api.md` | Widget Blueprint hierarchy/tree |
| Animation | `references/bridge-anim-api.md` | ABP state machines, slots, sequences, montages, blend spaces. **Authoring an ABP? Read the "Authoring an Animation Blueprint (agent workflow)" section first.** |
| DataTable | `references/bridge-datatable-api.md` | Schema, rows, fields, search, CSV |
| Material | `references/bridge-material-api.md` | Material instance parameters |
| Level / Actor | `references/bridge-level-api.md` | Level queries, spawn/destroy/move, property get/set, selection |
| Editor session | `references/bridge-editor-api.md` | Asset open/save, viewport camera, PIE start/stop, console/CVars, BP compile |
| GameplayAbility | `references/bridge-gameplayability-api.md` | GA Blueprint metadata |
| **Motion Matching 鈥?PoseSearch** | `references/bridge-pose-search-api.md` | **Read before any PSS / PSD read or write** 鈥?`DatabaseAnimationAssets` / `Channels` are `private:` and unreachable via `get_editor_property`; this lib is the only path. Includes `wait-pose-index` CLI. |
| **Motion Matching 鈥?Chooser** | `references/bridge-chooser-api.md` | **Read before any CHT read or write** 鈥?`ResultsStructs` / `DisabledRows` etc. are `private:`; this lib is the only path. Covers NestedChooser `:Name` paths, `matched_row=-1` caveat, and the auto Compile+PostEditChange contract. |
| Reactive handlers | `references/bridge-reactive.md` | Register Python on UE events (GameplayEvent / AnimNotify / MovementMode / Attribute / ActorLifecycle / InputAction). |
| Navigation | `references/bridge-navigation-api.md` | NavMesh OBJ export |
| Perf snapshots | `references/bridge-perf-api.md` | Structured FPS / GT / RT / GPU / draw calls / mem / UObject histogram |
| Agent / Gameplay | `references/bridge-gameplay-api.md` | **Mandatory before driving the player pawn** 鈥?sticky inputs, camera steering, navmesh path planning. |
| UE Asset / Actor / Material | `references/ue-python-*.md` | Stdlib UE Python helpers (load, list, duplicate, spawn) |

> **Pawn control is non-obvious 鈥?read `bridge-gameplay-api.md` first.** Driving the player (sticky `IA_Move`, `apply_look_input`, navigating to a moving target, holding an input over time) has hard constraints a fresh API read won't reveal: `bridge.exec` runs on GameThread so `time.sleep` inside one `exec` freezes the engine and stops the sticky ticker; continuous steering must run from a reactive `register_runtime_timer` callback, not a Python `while` loop; `IA_Move` is camera-relative and the forward-axis convention varies per project. The "Pattern: chase a (possibly moving) target and stop on arrival" section has the working template.

> **Asset lookup by name defaults to `search_assets_in_all_content(name, max_results)`.** When the user names an asset without a path, do **not** call `unreal.AssetRegistryHelpers.get_assets_by_path('/Game', recursive=True)` and filter 鈥?that walks 100k鈥?M+ entries and times out. The full `search_assets` form needs `BridgeAssetSearchScope.ALL_ASSETS` (not `PROJECT`) when the asset might live in a plugin mount (`/PluginName/...`). `PROJECT` covers `/Game` only; using it for a plugin asset returns `[]` silently. Valid scope members: `ALL_ASSETS`, `PROJECT`, `CUSTOM_PACKAGE_PATH` 鈥?there is no `GAME_FOLDER`.

## Blueprint authoring policy (MUST read before BP write ops)

**闈炲繀瑕佷笉鍐欒摑鍥?Node 鍜?Graph.** Agent-authored BPs are visibly weaker than C++ on readability/maintainability/perf. Default response to "鐢ㄨ摑鍥惧疄鐜?X":

1. State the tradeoff. Bridge has full graph-write primitives but agent-Graphs are weaker than C++.
2. Offer alternative: a C++ `UFUNCTION(BlueprintCallable)` with thin BP wrapper, or another bridge surface (Material / Anim / UMG / DataTable / Level).
3. **Re-confirm explicitly**: "浣犱粛鐒跺笇鏈涙垜鐢ㄨ摑鍥?Node/Graph 瀹炵幇锛岃繕鏄敼璧?C++锛?
4. Only proceed on explicit insistence ("灏辩敤钃濆浘鍐?). Otherwise redirect.

**Exceptions** (skip the dance):
- Pure data writes (`set_blueprint_variable_default` etc.) without new nodes
- Bulk automation explicitly framed as such ("鎵?100 涓?BP 鏀归粯璁ゅ€?)
- Standing authorization in `CLAUDE.md` or current conversation

Hard line: any op spawning/connecting/removing nodes, adding events, or creating functions needs the confirmation dance.

## Blueprint review loop (mandatory after BP authoring)

After authoring/modifying a BP graph (and the user confirmed they want it):

```
1. plan        鈥?list events / functions / local vars in text first
2. build       鈥?add_*_node / connect_graph_pins / add_blueprint_variable 鈥?3. auto_layout 鈥?MANDATORY after ANY graph mutation; use 'pin_aligned' (3-exec flow) or 'exec_flow' (bulk).
                 See bridge-blueprint-api.md for the exact open鈫抯leep鈫抣ayout sequence.
4. lint        鈥?lint_blueprint(...) and resolve every warning/error
5. collapse    鈥?collapse_nodes_to_function on any LongExecChain finding
6. straighten  鈥?straighten_exec_chain on main rails (after 'exec_flow' only)
7. comment     鈥?add_comment_box + set_comment_box_color per section (>10 nodes = mandatory)
8. compile     鈥?compile_blueprints clean
```

- **Lint is non-optional.** Fix every warning/error or comment-box-justify it.
- **Name things.** Rename `UnnamedCustomEvent` / `CustomEvent_0` to intent (`OnHealthChanged`).
- **Comment boxes are section titles.** Any graph >10 nodes needs at least one comment box with a meaningful title (`"1. Validate inputs"`, `"2. Apply damage"`) and an appropriate preset color (`Section`, `Validation`, `Danger`, `Network`, `UI`, `Debug`, `Setup`).
- **Pick the right layout strategy.** `auto_layout_graph(... 'pin_aligned' ...)` uses DFS-ordered leaves + downstream-driven Y alignment + live Slate geometry 鈥?matches hand-laid BP shape (`.then` rails horizontal, siblings stacked in one column). Requires the three-exec flow (open graph 鈫?sleep 鈫?layout) to read live widgets. Use `'exec_flow'` (Sugiyama-lite, layer-center Y) for bulk tidy on large graphs where pixel-accuracy matters less, then follow with `straighten_exec_chain` on the main rail.
- **Compact by spacing, not by node.** `pin_aligned` treats `h_spacing` as a loose upper bound and internally uses `DataHSpace = max(15, h/3)` and `ExecGap = max(30, h/2)` 鈥?so `h_spacing=100` renders tighter than the raw number suggests. Pass smaller values (`40, 32`) for max density.
- **Size predict before spawning.** If placing several nodes in a row by hand, call `predict_node_size` for each kind first so X offsets don't overlap.
- **Post-layout geometry reads.** After `auto_layout_graph` runs, Slate widgets don't refresh `NodePosX/Y` until they tick 鈥?so `get_rendered_node_info` returns *pre-layout* pin coords in the same exec. For crossing-detection / wire-length audits right after layout, read `get_node_layout(bp, fn, guid).pos_*` (authoritative from node model) and estimate pin Y as `pos_y + 40 + 22 脳 dir_index`. Only pay the open-graph + sleep cost when you specifically need Slate-accurate coords.
- `auto_insert_reroutes` is intentionally NOT in the loop 鈥?empirically it produces many-knot routing that reads worse than the original. Opt in per-graph if a specific case needs it.

## Safety Rules

- **NEVER** delete assets / actors / files without explicit confirmation.
- **NEVER** modify or overwrite assets without describing the change first.
- Wrap state-changing ops in `unreal.ScopedEditorTransaction`.
- On failure: show full traceback. Don't silently retry destructive ops.

## Notes

- Python interpreter is **persistent** 鈥?variables carry between `exec` calls.
- `print()` returns to bridge; `unreal.log()` only to UE Output Log.
- Structured data: `import json; print(json.dumps(...))` with `--json`.
- If `ping` fails: editor not running, plugin not enabled, or wrong project (preconditions above).
- Timeout: retry with `--timeout 120`.

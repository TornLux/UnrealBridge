---
name: unreal-bridge
description: Execute Python scripts inside a running Unreal Engine 5.3+ editor via TCP bridge. Use when the user asks to interact with UE, manipulate assets, query scenes, automate workflows, or run Python in Unreal.
allowed-tools: Bash Read Write Edit Glob Grep Monitor
---

# UnrealBridge

Execute Python directly inside a running UE 5.3+ editor. Protocol-v2 auto-discovery defaults to paired host-local UDP probes: multicast `239.255.42.99:9876` with TTL 0 plus loopback `127.0.0.1:9876`. Use `--discovery-scope=lan` only when another host must be discovered; it raises multicast TTL to 1 while retaining loopback. Malformed replies and Windows UDP reset notifications are skipped independently, valid responses are de-duplicated by Server-start UUID, and wildcard binds use the UDP response source IP. The six pre-existing exact commands remain the minimum capability set; `exact_editor_status` is advertised and negotiated separately, so an older protocol-v2 endpoint remains usable for its supported base commands. Unique future capabilities remain forward-compatible. Every bounded TCP response is rechecked against the frozen identity. The TCP data port is OS-assigned per editor.

## Preconditions

If `bridge.py` returns `discovery: no UnrealBridge editors found`, walk these in order — don't troubleshoot Python or firewalls first:

1. **Plugin and matching skill installed** with `sync_project.bat <UEProjectRoot>`. Check: `<UEProject>/Plugins/UnrealBridge/UnrealBridge.uplugin` exists. The legacy `sync_plugin.bat` command name accepts the same project-root argument.
2. **Plugin enabled** — check `<UEProject>/<Project>.uproject` `"Plugins"` block for `{"Name":"UnrealBridge", "Enabled":false}` and flip if present.
3. **Editor up and ready** — `bridge.py ping` returns `"ready": true`. `false` means MainFrame still loading; wait 10–60s.

The loopback probe keeps local discovery working when multicast is blocked by a VPN, virtual NIC, or Windows firewall policy. The discovery group must be an IPv4 multicast address; direct unicast mode is intentionally fail-closed: `--endpoint`, `--instance-id`, `--expected-pid`, and `--expected-project-path` (or matching `UNREAL_BRIDGE_*` variables) are one inseparable tuple. Copy the identity values verbatim from one discovery response or Server startup line; `project_path` is case- and representation-sensitive on every OS. A host/port alone can be stale and is never used as a legacy fallback. Python 3.7+ stdlib only.

## Waiting for the editor to become ready (post-launch / post-relaunch)

After launching/relaunching the editor, poll readiness with **`Monitor`**
(streams progress events, recommended) or **`Bash` with `run_in_background:
true`** (one completion event). Don't write a foreground `for/sleep`
countdown — the harness blocks long leading sleeps and you'll miss readiness.

Paste this verbatim as the `command` for either tool:

```bash
end=$(( $(date +%s) + 300 ))
i=0
until python .claude/skills/unreal-bridge/scripts/bridge.py --json --timeout 3 ping 2>/dev/null | grep -q '"ready": *true'; do
  now=$(date +%s)
  [ "$now" -ge "$end" ] && { echo "[wait] TIMEOUT after 300s"; exit 1; }
  i=$((i + 1))
  [ $((i % 3)) -eq 0 ] && echo "[wait] still booting ($((end - now))s left)"
  sleep 10
done
echo "[wait] READY"
```

Then call one of:

```
Monitor(description="UE editor ready-poll", command=<block above>,
        timeout_ms=360000, persistent=false)

Bash(description="Wait for UE editor ready", command=<block above>,
     run_in_background=true, timeout=360000)
```

**Locks (don't change without reading)**:

- Grep `"ready": *true` — TCP-up ≠ MainFrame-ready. `success:true, ready:false`
  means `exec` calls will be rejected; ping success alone is not enough.
- Ping FIRST then sleep — no leading sleep (harness blocks long ones).
- `i % 3` echo gate — caps notifications at ~10 over 5 min so Monitor doesn't
  trip its event-flood auto-stop.
- `end` deadline must stay **below** the tool `timeout_ms`/`timeout` so you
  get `[wait] TIMEOUT` rather than a silent harness kill.
- Path is relative to repo root; `${CLAUDE_SKILL_DIR}` is **not** reliably set
  in Monitor/Bash subshells — don't substitute it here.
- After the loop returns, do **one foreground** `bridge.py ping` before real
  work — bg success is past tense.
- Multi-editor host: append `--project=<name>` to the ping inside the loop.
- Skip this entirely when invoking `rebuild_relaunch.py` — it polls
  internally and prints `[rebuild] bridge is ready.` on success.

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
| `status` | Read cached Engine/Slate tick ages, readiness and modal attention without fresh GameThread dispatch |
| `gamethread-ping` | Probe GameThread liveness (bypasses exec queue; use when `exec` hangs) |
| `resume` | Unstick a paused BP breakpoint |
| `modal-status` | Inspect a blocking Slate dialog: title, body, buttons, inputs and checkboxes |
| `modal-click <snapshot> <button>` | Click one reviewed button; stale snapshots are rejected |
| `modal-set-text <snapshot> <input> <value>` | Fill a reviewed text input (password values are never returned) |
| `modal-set-checkbox <snapshot> <checkbox> <checked\|unchecked>` | Change a reviewed checkbox |
| `list-editors` | Print every editor that responded in the selected discovery scope (host-local by default) |
| `wait-compile <material>` / `wait-pose-index <psd>` | Client-side polling helpers |

Discovery-mode optional flags: `--project=<name|path>` (disambiguate when >1 editors run; or env `UNREAL_BRIDGE_PROJECT`), `--discovery-scope=local|lan` (`local` default; or env `UNREAL_BRIDGE_DISCOVERY_SCOPE`), `--token=<secret>`, `--timeout=<s>`, `--json`, `--no-preflight`. Global flags precede the subcommand, for example `bridge.py --discovery-scope=lan list-editors`. Direct mode requires the complete `--endpoint=host:port --instance-id=<uuid> --expected-pid=<pid> --expected-project-path=<uproject>` tuple; none of its four fields is optional.

## Workflow

1. **Always ping first.**
2. **Default to `exec --stdin` heredoc, NOT `exec-file`.** A heredoc is the right mode for ~95% of one-shot work — no temp file to name, no cleanup, prompt stays self-contained, no risk of dangling scripts in `$TEMP` / `.tmp` / project root. **Only reach for `exec-file` when you genuinely intend to re-run the same script multiple times** (iterating on a fix, comparing runs). One-shots like "find X, list Y, build a report" → heredoc. If you find yourself writing `with open("/tmp/foo.py", "w")` followed by `bridge.py exec-file /tmp/foo.py`, stop and rewrite as a heredoc.
3. `--json` for parseable output.
4. Exit codes: `0` success · `1` runtime/transport · `2` bad CLI args · `3` AST preflight rejected.
5. **If `exec` hangs**: read the timeout response first. `blocked_by_modal:true`
   includes the active window and its controls. Otherwise use `status`
   first: it reads cached Engine/Slate ages without dispatching fresh GameThread
   work. Follow with `gamethread-ping` only when a fresh liveness probe is useful,
   or `resume` for a BP breakpoint.

## Blocking modal dialogs — inspect, decide, then act

Slate dialogs run a nested event loop that blocks the normal Python exec queue.
UnrealBridge's modal commands use a separate GameThread task path, so they keep
working while the original call is suspended.

1. Run `modal-status` (an `exec*` timeout does this automatically).
2. Read the **title and full body**, then decide whether an action is safe and
   consistent with the user's request. Never click the first/default/affirmative
   button merely to unblock the editor.
3. Use the exact `snapshot_id` and control id returned by that inspection.
4. After `modal-set-text` or `modal-set-checkbox`, use the new snapshot returned
   by that command for the next action. A stale id is deliberately rejected.
5. If the dialog is destructive, ambiguous, requests credentials/consent, or
   expands the user's authorized scope, leave it open and ask the user.

Example:

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" --json modal-status
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" modal-click 8f1a2b3c4d5e6f70 0
```

Password input contents are reported only as `<redacted>`. These commands cover
UE Slate modals. A platform-native file picker or crash reporter may stop the
GameThread before it can pump this bypass; treat a modal-status GameThread
timeout as a distinct native-window/deadlock diagnosis rather than blind retry.

Multi-line example:

```bash
python "${CLAUDE_SKILL_DIR}/scripts/bridge.py" exec --stdin <<'EOF'
from unreal_bridge import Asset
paths, _ = Asset.search_assets_in_all_content(query="Hero", max_results=5)
for p in paths:
    print(p.export_text())
EOF
```

**Windows path gotcha (Bash tool only):** unquoted backslashes get eaten by bash word-splitting — `python G:\Claude\UnrealBridge\.claude\skills\unreal-bridge\scripts\bridge.py exec ...` arrives as `G:ClaudeUnrealBridge.claudeskillsunreal-bridgescriptsbridge.py` and fails with "No such file or directory". Use forward slashes (`G:/Claude/...`) or single-quote the path (`'G:\Claude\...'`). The body inside `<<'EOF' ... EOF` is literal and unaffected — only the arg before the heredoc matters. PowerShell tool is unaffected.

## API surface — use the wrapper module first

```python
from unreal_bridge import Asset, Level, Blueprint, Editor, Anim, Rig, Niagara, Material, PoseSearch, Chooser, StateTree, SmartObject, ...
paths, _ = Asset.search_assets_in_all_content(query="Hero", max_results=20)
```

The wrapper has 26 classes (one per `UnrealBridge*Library`) with **kwargs-only signatures** — positional args raise `TypeError` immediately, no UE round-trip. This is the structural fix for positional-arg-order hallucinations. Regenerate after C++ header changes via `python tools/gen_manifest.py`.

Fallback: raw `unreal.UnrealBridge*Library.foo(...)` works (preflight catches errors), but prefer the wrapper.

## AST preflight (automatic — safety net only)

Every `exec*` call runs an AST preflight in the bridge client BEFORE sending. Reads `scripts/bridge_manifest.json` and rejects locally (exit 3, no UE round-trip) when:

- Function/library name doesn't exist (with did-you-mean)
- Required args missing / unknown kwarg / param given twice
- `unreal.BridgeXxx.YYY` enum member doesn't exist

Type validation is NOT done by preflight. Wrong asset paths / wrong scope still surface at runtime — references prose still matters for those.

**Treat preflight as a backstop, not a planning tool.** If you write code expecting preflight to autocorrect your guesses, you'll produce noisy output and still hit runtime tracebacks for the half it can't catch (USTRUCT field names, Python attribute access, EditDefaultsOnly violations). See "Verify before you call" below.

Bypass with `--no-preflight` (rare). Preview with `bridge.py preflight <path>`.

## Common semantic traps (preflight does NOT catch)

| Symptom | Fix |
|---------|-----|
| Bridge call returns `[]` for an asset you know exists | Wrong scope: `PROJECT` covers `/Game` only; plugin assets need `ALL_ASSETS`. Use `Asset.search_assets_in_all_content(...)`. |
| `get_derived_classes` hangs / huge results | Don't pass `UObject` / `AActor` — narrow to most specific base. |
| Multi-step BP edit feels chatty | Batch with `exec --stdin` heredoc or `exec-file`, not 3 inline `exec` calls. |
| Pawn movement script freezes the editor | `time.sleep` inside `exec` blocks GameThread — see `bridge-gameplay-api.md` "chase a target" pattern (use `register_runtime_timer`). |
| `print('中文' / '한글' / '日本語')` shows `���` or `涓枃` mojibake | Almost always **display-only** — the wire is byte-perfect UTF-8. See "Non-ASCII output (CJK / Greek / emoji)" below. |
| Need "where is this GameplayTag used?" / Find References on a tag | `unreal.UnrealBridgeGameplayTagLibrary.find_assets_referencing_tag(tag, include_children, ...)`. Mutations: `add_gameplay_tag` / `rename_gameplay_tag` (auto-redirect) / `remove_gameplay_tag`; pick the target ini via `list_tag_source_inis(...)`. For `PrimaryAssetId` / other named-value structs use the generic `UnrealBridgeAssetLibrary.find_assets_referencing_searchable_name(struct_type, value, ...)`. See `bridge-gameplaytag-api.md`. |
| Need to change default materials on a StaticMesh / SkeletalMesh asset | Use `Asset.get_mesh_material_slots(...)` then `set_mesh_material`, `set_mesh_material_by_slot_name`, or atomic `set_mesh_materials`. Pass `save=False` for an undoable in-memory comparison. Do not mutate the raw `Materials` UPROPERTY. |

## Verify before you call — don't lean on preflight

Preflight is a **safety net**, not a planning tool. Writing code from imagined API / field / attribute names and waiting for preflight to tell you the right one produces noisy output (4-8 `[WARN]` per script, often still ending in a runtime traceback) and wastes round-trips. **Verify FIRST, then write the script.** Four axes:

1. **Bridge function names** — `grep scripts/bridge_manifest.json` or read the relevant `references/bridge-*-api.md`. Don't translate the C++ name into snake_case in your head and assume.
2. **Bridge USTRUCT return-value fields** — return types like `BridgeGameplayEffectInfo` keep a *minimum* set of fields (not exhaustive); UE 5.x evolves struct shape between versions. Look up the fields in `bridge_manifest.json` `structs` section, OR run a one-off probe `print([a for a in dir(obj) if not a.startswith('_')])` BEFORE writing the consumer code.
3. **UE object attributes** — never `<obj>.<attr>`. Always `get_editor_property` (and respect `protected:` / `EditDefaultsOnly` rejection). Use `Level.list_actor_properties(actor_name=...)` or `Level.list_class_properties(class_path=...)` to list first.
4. **Function signatures** — when unsure: `unreal.UnrealBridgeXxx.fn.__doc__` first line is the signature. Cheaper than a guess + correction round-trip.

**Symptom you're in the bad loop**: your last few exec attempts produced `[WARN]` lines from preflight followed by `AttributeError` tracebacks. **Stop**, run one heredoc with `dir()` / manifest grep, then write the real script.

## Never silently bail to raw `unreal.*`

If a bridge call returns empty / errors, suspect **your parameters** (wrong scope / path / filter), not the wrapper. Falling back to `unreal.AssetRegistryHelpers` etc. bypasses preflight, audit logging, and often walks 100k–2M registry entries.

Preflight emits `[WARN]` on known raw-fallback patterns and shows the bridge equivalent inline. To look up before writing: `bridge.py suggest [pattern]`. Patterns currently mapped:

| Raw pattern | Bridge equivalent |
|---|---|
| `ar.get_assets_by_path(...)` | `Asset.list_assets_under_path(folder_path, include_subfolders)` |
| `ar.get_referencers(...)` | `Asset.get_package_referencers(package_name, hard_only)` |
| `ar.get_dependencies(...)` | `Asset.get_package_dependencies(package_name, hard_only)` |
| `unreal.GameplayStatics.get_all_actors_of_class(...)` | `Level.find_actors_by_class(class_path, max_results)` |
| `unreal.GameplayStatics.get_all_actors_with_tag(...)` | `Level.find_actors_by_tag(tag)` |

If you genuinely believe no bridge function covers your case, **ask the user before reaching for raw `unreal.*`**.

## Reference index — read these for usage / traps / workflow patterns

Signatures are now mechanically enforced (preflight). References carry semantic traps, scope warnings, and workflow patterns preflight can't catch.

| Topic | File | When to read |
|-------|------|------|
| Blueprint queries + authoring | `references/bridge-blueprint-api.md` | Class hierarchy, variables/functions/components, node search, write ops, auto-layout flow, lint loop |
| Asset queries | `references/bridge-asset-api.md` | Asset lookup, search, references/dependencies, **`SoftObjectPath` stringification** (top-of-file block) |
| UMG / Widget | `references/bridge-umg-api.md` | **Read before any Widget Blueprint write.** Asset/tree/layout/style authoring, UI-material brushes, batched widget animations, UE 5.7+ MVVM, compile validation, and PIE functional verification. |
| Animation | `references/bridge-anim-api.md` | ABP state machines, slots, sequences, montages, blend spaces. **Authoring an ABP? Read the "Authoring an Animation Blueprint (agent workflow)" section first.** |
| **Control Rig / IK Rig / IK Retargeter** | `references/bridge-rig-api.md` | **Read before any rig or retarget asset write.** Type discovery, hierarchy and RigVM authoring, solver/goal/chain setup, retarget ops/mapping/poses/profiles, batch retargeting, compile/processor validation, transient evaluation, animation-quality review, and cleanup. Functional on UE 5.7+. |
| **Niagara / VFX** | `references/bridge-niagara-api.md` | **Read before any Niagara asset write.** Template/script discovery, System/Emitter recipes, module inputs, User parameters, renderers/materials/bindings, compile/audit gates, Trail/Sparks/Explosion/Dissolve presets, moving transient previews, and cleanup. Functional on UE 5.7+. |
| DataTable | `references/bridge-datatable-api.md` | Schema, rows, fields, search, CSV |
| Material | `references/bridge-material-api.md` | Material/master/function inspection and delivery: dependency-complete local graph reads, fail-closed/preflighted graph mutation, compile diagnostics, instances, layers, textures, templates, and previews |
| Level / Actor | `references/bridge-level-api.md` | Level queries, spawn/destroy/move, property get/set, selection |
| Editor session | `references/bridge-editor-api.md` | Asset open/save, viewport camera, PIE start/stop, console/CVars, BP compile |
| GameplayAbility | `references/bridge-gameplayability-api.md` | GA Blueprint metadata |
| **Generic UPROPERTY read/write (privileged)** | `references/bridge-property-api.md` | When `get_editor_property` rejects with "protected" / `set_editor_property` rejects EditDefaultsOnly sub-fields / you need to read `private:` / bare `UPROPERTY()` / write nested struct values via export-text. `list_u_properties` / `get_u_property_as_export_text(path, "Modifiers[0].Foo.Bar")` / `set_u_property_from_export_text` (with `fire_change_notify` for editor refresh) / `array_append_u_property` (auto-handles FGameplayTagContainer ParentTags) / `array_remove_u_property` / `array_clear_u_property` / `get_asset_cdo_path`. **Privileged** — bypasses UE editor safety net. Use dedicated APIs (`add_gameplay_tag`, `add_ge_modifier_scalable`) when they exist. |
| **GameplayTag references / sources / mutations** | `references/bridge-gameplaytag-api.md` | Read: `find_assets_referencing_tag` (with child-tag expansion), `list_all_registered_tags`, `get_tag_source_info`. Write: `add_gameplay_tag(tag, source_ini='', comment='')`, `rename_gameplay_tag(old, new, rename_children=True)` (auto-inserts redirect, redirect lifetime hardened — survives subsequent mutations + cold restart), `remove_gameplay_tag(tag)`. Source enum: `list_tag_source_inis(filter_type='')`. Redirect ops: `list_gameplay_tag_redirects(source='', old_prefix='')` + `remove_gameplay_tag_redirect(old, new)` for enumerate-then-sweep. Built on the AssetRegistry SearchableName index + IGameplayTagsEditorModule. `bridge-asset-api.md` "SearchableName Index" has the generic read version (PrimaryAssetId, custom named-value structs). |
| **Motion Matching — PoseSearch** | `references/bridge-pose-search-api.md` | **Read before any PSS / PSD read or write** — `DatabaseAnimationAssets` / `Channels` are `private:` and unreachable via `get_editor_property`; this lib is the only path. Includes `wait-pose-index` CLI. |
| **Motion Matching — Chooser** | `references/bridge-chooser-api.md` | **Read before any CHT read or write** — `ResultsStructs` / `DisabledRows` etc. are `private:`; this lib is the only path. Covers NestedChooser `:Name` paths, `matched_row=-1` caveat, and the auto Compile+PostEditChange contract. |
| **StateTree authoring + runtime** | `references/bridge-statetree-api.md` | **Read before any StateTree write.** GUID-first hierarchy/node/transition workflows, schema-filtered type discovery, reflected node-property discovery, bindings, parameters, compile diagnostics, transient breakpoints, and PIE component control. Functional on UE 5.7+. |
| **Smart Object authoring + runtime** | `references/bridge-smartobject-api.md` | **Read before any Smart Object write or claim.** Definition/slot/behavior/data/annotation editing, World Conditions, parameters/bindings, world components and persistent collections, runtime query/claim/occupy/release, tags/events, and entrance validation. Functional on UE 5.7+; claim tokens are session-local and must be released. |
| Reactive handlers | `references/bridge-reactive.md` | Register Python on UE events (GameplayEvent / AnimNotify / MovementMode / Attribute / ActorLifecycle / InputAction). |
| Navigation | `references/bridge-navigation-api.md` | NavMesh OBJ export |
| Perf snapshots | `references/bridge-perf-api.md` | Structured FPS / GT / RT / GPU / draw calls / mem / UObject histogram |
| Agent / Gameplay | `references/bridge-gameplay-api.md` | **Mandatory before driving the player pawn** — sticky inputs, camera steering, navmesh path planning. |
| UE Asset / Actor / Material | `references/ue-python-*.md` | Stdlib UE Python helpers (load, list, duplicate, spawn) |

## UMG deliverable loop (mandatory after UMG authoring)

Creating a WidgetTree is not completion. For every agent-authored screen:

1. Build one valid panel root, set responsive anchors/slots, and make only the widgets needed by logic/bindings variables.
2. Create UI-domain shaders through the Material library, compile them cleanly, then assign them to `FSlateBrush` properties with the UMG library.
3. On UE 5.7+, keep state in a FieldNotify ViewModel and prefer MVVM bindings over legacy binding functions or event-graph state plumbing. On UE 5.3-5.6, MVVM calls are safe logged no-ops; choose an explicitly documented fallback instead of failing the build.
4. Author animation keys in batches, then require `compile_and_validate_widget_blueprint(..., save=True)` to succeed with no errors.
5. Start PIE in a separate exec, spawn the widget, and prove live geometry, state propagation, semantic interactions, animation intermediate values, and dynamic material parameters by readback.
6. Always call `remove_widget_instance` / `remove_all_widget_instances` before stopping PIE. Delete validation-only assets from their exact dedicated folder and verify both Asset Registry and backing files are empty.

The exact API sequence, MVVM modes, animation key structs, runtime semantics, and cleanup checklist are in `references/bridge-umg-api.md`.

## Rig deliverable loop (mandatory after Control Rig / IK authoring)

Creating assets or obtaining a clean compile is not completion. For every
agent-authored Control Rig or retarget setup:

1. Discover exact unit, template, solver, and retarget-op type paths before
   writing; never guess reflected type names or RigVM pin paths.
2. Build and review the Control Rig hierarchy, then author a connected RigVM
   graph with explicit execution flow, meaningful control shapes, stable
   naming, comments, and readable layout.
3. Require `validate_control_rig(..., save=True)` to succeed, then call
   `evaluate_control_rig` on a transient instance and verify expected control
   and bone transforms for at least one non-default input.
4. For IK Rig, validate the preview skeleton, retarget root, every chain, goal,
   solver connection, and any reflected solver/goal/bone settings.
5. For IK Retargeter, validate both rigs and meshes, review chain mappings and
   op settings, initialize the processor, test representative retarget poses
   and profiles, then batch-retarget at least one animation.
6. Run `analyze_animation_quality` on representative output and review root
   spikes, planted-foot sliding/penetration, and joint angular discontinuities;
   visual playback remains required for final artistic acceptance.
7. Validation-only work must live in one unique folder. Delete every generated
   Control Rig, IK Rig, Retargeter, pose/profile carrier, and retargeted output,
   then prove both Asset Registry and backing files contain no residue.

The exact type-discovery calls, export-text property rules, RigVM addressing,
retarget workflow, validation gates, and cleanup checklist are in
`references/bridge-rig-api.md`.

## Niagara deliverable loop (mandatory after VFX authoring)

Creating or cleanly compiling a Niagara System is not completion. For every
agent-authored effect:

1. Discover exact System/Emitter templates and module/dynamic-input script
   paths before writing; never guess plugin-mount paths or stack input names.
2. Re-list Emitters, modules, inputs, renderers, and User parameters after
   structural changes. Use returned handle/node/renderer IDs for mutations.
3. Prove each intended gameplay/art control is linked to a `User.*` parameter
   by input readback, then require `compile_niagara_system(..., save=True)` to
   report valid, ready, zero-error output.
4. Require `validate_niagara_system` to pass; resolve null materials, missing
   GPU bounds, disabled/empty content, and budget findings.
5. Spawn and advance a transient preview, then check active per-emitter state,
   particle counts, memory, and timing. For weapon ribbons, alternate
   `set_niagara_preview_transform(..., teleport=False)` with short simulation
   advances so the component samples a real path.
6. Review the effect visually at gameplay scale for timing, silhouette,
   readability, color, overdraw, and culling. Runtime counts prove simulation,
   not artistic acceptance.
7. Remove every preview before deleting validation assets. Delete the exact
   dedicated folder and prove both Asset Registry and backing files are empty;
   an in-memory deleted UObject can persist until GC/restart and is not a disk
   cleanup failure.

The exact recipe structs, stack/input modes, renderer/material rules, preset
semantics, cold-load compile behavior, moving-preview pattern, and cleanup
checklist are in `references/bridge-niagara-api.md`.

> **Pawn control is non-obvious — read `bridge-gameplay-api.md` first.** Driving the player (sticky `IA_Move`, `apply_look_input`, navigating to a moving target, holding an input over time) has hard constraints a fresh API read won't reveal: `bridge.exec` runs on GameThread so `time.sleep` inside one `exec` freezes the engine and stops the sticky ticker; continuous steering must run from a reactive `register_runtime_timer` callback, not a Python `while` loop; `IA_Move` is camera-relative and the forward-axis convention varies per project. The "Pattern: chase a (possibly moving) target and stop on arrival" section has the working template.

> **Asset lookup by name defaults to `search_assets_in_all_content(name, max_results)`.** When the user names an asset without a path, do **not** call `unreal.AssetRegistryHelpers.get_assets_by_path('/Game', recursive=True)` and filter — that walks 100k–2M+ entries and times out. The full `search_assets` form needs `BridgeAssetSearchScope.ALL_ASSETS` (not `PROJECT`) when the asset might live in a plugin mount (`/PluginName/...`). `PROJECT` covers `/Game` only; using it for a plugin asset returns `[]` silently. Valid scope members: `ALL_ASSETS`, `PROJECT`, `CUSTOM_PACKAGE_PATH` — there is no `GAME_FOLDER`.

## Blueprint authoring policy (MUST read before BP write ops)

**非必要不写蓝图 Node 和 Graph.** Agent-authored BPs are visibly weaker than C++ on readability/maintainability/perf. Default response to "用蓝图实现 X":

1. State the tradeoff. Bridge has full graph-write primitives but agent-Graphs are weaker than C++.
2. Offer alternative: a C++ `UFUNCTION(BlueprintCallable)` with thin BP wrapper, or another bridge surface (Material / Anim / UMG / DataTable / Level).
3. **Re-confirm explicitly**: "你仍然希望我用蓝图 Node/Graph 实现，还是改走 C++？"
4. Only proceed on explicit insistence ("就用蓝图写"). Otherwise redirect.

**Exceptions** (skip the dance):
- Pure data writes (`set_blueprint_variable_default` etc.) without new nodes
- Bulk automation explicitly framed as such ("扫 100 个 BP 改默认值")
- Standing authorization in `AGENTS.md`, `CLAUDE.md`, or current conversation

Hard line: any op spawning/connecting/removing nodes, adding events, or creating functions needs the confirmation dance.

## Blueprint review loop (mandatory after BP authoring)

After authoring/modifying a BP graph (and the user confirmed they want it):

```
1. plan        — list events / functions / local vars in text first
2. build       — add_*_node / connect_graph_pins / add_blueprint_variable …
3. auto_layout — MANDATORY after ANY graph mutation; use 'pin_aligned' (3-exec flow) or 'exec_flow' (bulk).
                 See bridge-blueprint-api.md for the exact open→sleep→layout sequence.
4. lint        — lint_blueprint(...) and resolve every warning/error
5. collapse    — collapse_nodes_to_function on any LongExecChain finding
6. straighten  — straighten_exec_chain on main rails (after 'exec_flow' only)
7. comment     — add_comment_box + set_comment_box_color per section (>10 nodes = mandatory)
8. compile     — compile_blueprints clean
```

- **Lint is non-optional.** Fix every warning/error or comment-box-justify it.
- **Name things.** Rename `UnnamedCustomEvent` / `CustomEvent_0` to intent (`OnHealthChanged`).
- **Comment boxes are section titles.** Any graph >10 nodes needs at least one comment box with a meaningful title (`"1. Validate inputs"`, `"2. Apply damage"`) and an appropriate preset color (`Section`, `Validation`, `Danger`, `Network`, `UI`, `Debug`, `Setup`).
- **Pick the right layout strategy.** `auto_layout_graph(... 'pin_aligned' ...)` uses DFS-ordered leaves + downstream-driven Y alignment + live Slate geometry — matches hand-laid BP shape (`.then` rails horizontal, siblings stacked in one column). Requires the three-exec flow (open graph → sleep → layout) to read live widgets. Use `'exec_flow'` (Sugiyama-lite, layer-center Y) for bulk tidy on large graphs where pixel-accuracy matters less, then follow with `straighten_exec_chain` on the main rail.
- **Compact by spacing, not by node.** `pin_aligned` treats `h_spacing` as a loose upper bound and internally uses `DataHSpace = max(15, h/3)` and `ExecGap = max(30, h/2)` — so `h_spacing=100` renders tighter than the raw number suggests. Pass smaller values (`40, 32`) for max density.
- **Size predict before spawning.** If placing several nodes in a row by hand, call `predict_node_size` for each kind first so X offsets don't overlap.
- **Post-layout geometry reads.** After `auto_layout_graph` runs, Slate widgets don't refresh `NodePosX/Y` until they tick — so `get_rendered_node_info` returns *pre-layout* pin coords in the same exec. For crossing-detection / wire-length audits right after layout, read `get_node_layout(bp, fn, guid).pos_*` (authoritative from node model) and estimate pin Y as `pos_y + 40 + 22 × dir_index`. Only pay the open-graph + sleep cost when you specifically need Slate-accurate coords.
- `auto_insert_reroutes` is intentionally NOT in the loop — empirically it produces many-knot routing that reads worse than the original. Opt in per-graph if a specific case needs it.

## Non-ASCII output (CJK / Greek / emoji)

Bridge is UTF-8 byte-perfect end-to-end since 2026-05-04. **Mojibake = display, not data.** Confirm with hex:

```python
print(got.encode('utf-8').hex(), '==', '测试'.encode('utf-8').hex())
```

If hex matches, it's a Windows cp936/cp1252 console issue — fix the *display*, not the bridge:

| Situation | Fix |
|---|---|
| Piping `bridge.py ... > out.txt` writes cp936 | prefix `PYTHONIOENCODING=utf-8` (bash) / `$env:PYTHONIOENCODING='utf-8'` + `\| Out-File -Encoding utf8` (PS) |
| Reading saved file back | `open(p, encoding='utf-8')` (or `utf-8-sig` for PS BOM) |
| Calling bridge.py from another Python process | set `PYTHONIOENCODING=utf-8` in subprocess env |

Verified: identifiers + literals in script source, exception messages/tracebacks, DataTable FName keys, asset paths with Chinese folders, save→restart→read persistence. Korean/Japanese use the same UTF-8 path — verify with hex if anything looks off.

**Never write temp files to `C:\` root** — use `$env:TEMP\…` or `/tmp/…`.

## Safety Rules

- **NEVER** delete assets / actors / files without explicit confirmation.
- **NEVER** modify or overwrite assets without describing the change first.
- Wrap state-changing ops in `unreal.ScopedEditorTransaction`.
- On failure: show full traceback. Don't silently retry destructive ops.

## Notes

- Python interpreter is **persistent** — variables carry between `exec` calls.
- `print()` returns to bridge; `unreal.log()` only to UE Output Log.
- Structured data: `import json; print(json.dumps(...))` with `--json`.
- If `ping` fails: editor not running, plugin not enabled, or wrong project (preconditions above).
- Timeout: retry with `--timeout 120`.

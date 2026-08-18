# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UnrealBridge is a TCP bridge between external tools (Claude Code) and Unreal Engine 5.3+ (verified clean BuildPlugin against 5.3.2 / 5.4.4 / 5.5.4 / 5.6.1 / 5.7.1 / 5.8.0 via `tools/build_matrix.py`; some libraries are gated to 5.7+ and a few inline shims handle 5.3 and 5.8 — see `docs/version-compatibility.md`). UE 5.2 and earlier are not supported. It consists of:
- A UE Editor plugin (`Plugin/UnrealBridge/`) that runs a TCP server inside the editor
- A Python CLI client (`.claude/skills/unreal-bridge/scripts/bridge.py`) used by the `unreal-bridge` skill
- API reference docs and helper scripts for querying/manipulating UE assets via Python

## Key Commands

**Sync plugin and matching skills to a UE project:**
```bash
sync_project.bat D:\Path\To\YourProject
```
Mirrors `Plugin/UnrealBridge/` plus `.claude/skills/unreal-bridge/` into the target project's plugin, `.agents`, and `.claude` destinations. Local plugin build outputs are retained. `sync_plugin.bat` is a legacy command-name alias with the same project-root argument.

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
- Request: `[4 bytes big-endian length][JSON: {"id":"...", "command":"exact_exec", "expected":{"protocol_version":2,"instance_id":"...","pid":...,"project_path":"..."}, "request":{"script":"...","timeout":30}, "token":"..." (optional)}]`
- Response: `[4 bytes big-endian length][JSON: {"id":"...", "success":bool, "output":"...", "error":"...", "protocol_version":2, "instance_id":"...", "pid":..., "project_path":"..."}]`
- Token auth: required when the server binds non-loopback. The token is written to `<Project>/Saved/UnrealBridge/token.txt`; clients read it and add `"token":"<value>"` to every request. Constant-time compared on the server.
- Every request uses an `exact_*` command with frozen discovery identity. Payload fields live under `request`, so a legacy server cannot mistake `exact_exec` for an ordinary script request. The production dispatcher rejects missing/malformed identity, request objects, and unknown/legacy commands before any command body, work admission, or GameThread dispatch; the client rejects response identity drift. `project_path` is one wire-canonical string and must match discovery/startup output exactly on every OS.
- Client response frames are bounded to `MAX_RESPONSE_FRAME_BYTES` (10 MiB), must be non-empty, valid UTF-8, and decode to a JSON object before identity fields are read.
- Special exact commands (handled inline on the worker thread, bypass the Python exec queue):
  - `exact_ping` → `pong` (TCP-only liveness)
  - `exact_editor_status` → cached Engine/Slate tick ages, readiness, staleness and modal-attention summary (no fresh GameThread dispatch)
  - `exact_gamethread_ping` → `alive`/`unresponsive` + `latency_ms` (GT liveness)
  - `exact_debug_resume` → unsticks a paused BP breakpoint via `FKismetDebugUtilities::RequestAbortingExecution`
  - `exact_modal_status` → structured active-Slate-modal snapshot (title, body, buttons, redacted inputs, checkboxes)
  - `exact_modal_action` → guarded click/input/checkbox action; rejects stale snapshots

`bridge.py exec*` automatically calls `modal_status` after an exec timeout and
adds `blocked_by_modal` plus the snapshot to its response. It never selects an
action automatically. The caller must read the dialog semantics and act with
the returned `snapshot_id`; this prevents both blind confirmation and a stale
action landing on a different dialog.

Exec and modal GameThread work share a cancellable lifecycle. If the server-side
deadline expires while work is still queued, the response reports `cancelled before execution`
and a later ticker/task-graph consumer cannot run the body. If the GameThread already claimed
the work, it is not safely retractable; the response reports `already started and outcome is unknown`
instead of claiming a successful cancellation. Shutdown first closes a shared admission gate,
uses the same queued cancellation path, and drains tracked worker/GameThread closures before
module unload; each result/event therefore has exactly one terminal publisher.

### Discovery Protocol
UDP discovery uses LAN multicast on `239.255.42.99:9876` plus a parallel local-loopback probe to `127.0.0.1:9876`. Both carry the same request id and responses are de-duplicated by Server-start UUID. This preserves LAN discovery while avoiding dependence on Windows multicast loopback. Multiple editors can bind via `SO_REUSEADDR`.
- Probe (client → group): `{"v":2, "type":"probe", "request_id":"<uuid>", "filter":{"project":"<name|path|*>"}}`
- Response (server → probe source, unicast): `{"v":2, "protocol_version":2, "type":"response", "request_id":"<uuid>", "instance_id":"<uuid>", "pid":..., "project":"...", "project_path":"...", "engine_version":"...", "tcp_bind":"...", "tcp_port":..., "token_fingerprint":"<sha1(token)[:16]>", "capabilities":["exact_exec",...]}`. The six pre-existing exact commands are the minimum required capability set; `exact_editor_status` is an advertised optional capability required only by the status command, and unique forward-compatible extras are allowed.
- Client loop: send the same probe to multicast + loopback → collect for `--discovery-timeout` ms (default 800) → discard each malformed/incomplete/legacy datagram without aborting collection → de-duplicate by instance UUID → filter by `--project=...` → freeze identity → connect TCP. A wildcard advertised bind resolves to that response's source IP. Empty `token_fingerprint` means no token required. Direct mode requires the inseparable `--endpoint`, `--instance-id`, `--expected-pid`, and `--expected-project-path` tuple, copied verbatim from discovery/startup output.

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
- **UnrealBridgeAssetLibrary** — Asset search (keyword with include/exclude tokens), derived class queries, asset references/dependencies, DataAsset queries, folder listing, and transactional StaticMesh/SkeletalMesh default-material authoring (index, slot name, atomic batch, optional save)
- **UnrealBridgeAnimLibrary** — AnimBlueprint introspection: state machines, AnimGraph nodes, linked layers, slots, curves, anim sequence/montage/blend space info, skeleton bone tree
- **UnrealBridgeRigLibrary** — UE 5.7+ Control Rig hierarchy/RigVM authoring and transient evaluation; IK Rig solver/goal/chain setup; IK Retargeter ops, mappings, poses, profiles, processor validation and batch retargeting; sampled animation-quality diagnostics. UE 5.3-5.6 expose safe logged stubs
- **UnrealBridgeNiagaraLibrary** — UE 5.7+ Niagara/VFX authoring and delivery: System/Emitter lifecycle and recipes, stack modules and inputs, user parameters, renderers/materials/bindings, compiler diagnostics and audits, weapon Trail/Beam, Sparks, layered Explosion/shockwave/light and Dissolve presets, plus transient preview simulation/transform/runtime metrics. UE 5.3-5.6 expose safe logged stubs
- **UnrealBridgeDataTableLibrary** — DataTable row inspection
- **UnrealBridgeMaterialLibrary** — Material instance parameter queries
- **UnrealBridgeUMGLibrary** — Widget Blueprint introspection: widget tree, properties, animations, bindings, events, search, property write
- **UnrealBridgeLevelLibrary** — Level/actor introspection and editing on the editor world: summary, actor listing with class/tag/name filters, actor info/transform/components, class/tag/radius queries, streaming levels, selection; write ops spawn/destroy/move/attach/detach/duplicate/label/hide + nested property get/set (e.g. `RootComponent.RelativeLocation`). All writes wrapped in `FScopedTransaction` for Ctrl+Z
- **UnrealBridgeEditorLibrary** — Editor session control: state query (engine version, PIE status, opened assets, CB selection/path, viewport camera), asset open/close/save/reload, Content Browser sync, viewport camera set/focus, PIE start/stop/pause, undo/redo, console command execution, CVar get/set/list, redirector fixup, Blueprint compile
- **UnrealBridgeGameplayAbilityLibrary** — GameplayAbilitySystem introspection (scaffold): GameplayAbility Blueprint CDO metadata — name, parent, instancing/net policy, asset tags, cost/cooldown GE class. Depends on the `GameplayAbilities` engine plugin (auto-enabled via `.uplugin`)
- **UnrealBridgeStateTreeLibrary** — UE 5.7+ StateTree asset lifecycle and full authoring: schema-filtered state/node/transition CRUD, generic node/state/transition properties, property bindings, root/state parameters, compiler diagnostics, transient debugger breakpoints, and live `UStateTreeComponent` inspection/control. Older engines expose safe stubs
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

Quits the editor → runs the version-locked plugin + skill sync → runs the target project's `Build.bat` → launches the editor detached → polls `bridge.py ping` until ready. Use when adding/removing `UFUNCTION` / `UCLASS` / `UPROPERTY`, changing struct layouts, or recovering from a failed LC compile. Build.bat's stdout captures full compiler output (this is the only way to surface MSVC errors when hot reload reports Failure). Takes ~2–5 minutes.

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

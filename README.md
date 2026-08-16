<p align="center">
  <h1 align="center">UnrealBridge</h1>
  <p align="center">
    <strong>Give your AI Agent the ability to control and edit Unreal Engine.</strong>
  </p>
  <p align="center">
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License"></a>
    <a href="https://www.unrealengine.com/"><img src="https://img.shields.io/badge/Unreal%20Engine-5.3%2B-313131?logo=unrealengine" alt="UE5.3+"></a>
    <a href="https://www.python.org/"><img src="https://img.shields.io/badge/-Python-3776AB?logo=python&logoColor=white" alt="Python"></a>
    <img src="https://img.shields.io/badge/-C%2B%2B-00599C?logo=cplusplus&logoColor=white" alt="C++">
    <img src="https://img.shields.io/badge/platform-Windows-0078D6?logo=windows" alt="Windows">
    <a href="https://claude.ai/code"><img src="https://img.shields.io/badge/Claude%20Code-skill-D97757" alt="Claude Code"></a>
    <a href="README.zh-CN.md"><img src="https://img.shields.io/badge/lang-%E4%B8%AD%E6%96%87-red" alt="中文"></a>
  </p>
</p>

<p align="center">
  <img src="docs/images/en/01-hook.png" alt="UNREAL ENGINE 5.4+ — FULL CONTROL for your AI Agent">
</p>

---

UnrealBridge is an Unreal Engine editor bridging layer built for AI Agents. It provides a typed operation surface for core scenarios such as animation-asset introspection, reactive event subscription, asset search and reference analysis, and automatic layout of Blueprint graphs. The Agent issues queries and modifications against a locally running editor instance; every change takes effect in real time, is bounded by the transaction system, and is undoable.

> **Runtime boundary:** The current bridge is supported only in interactive editor sessions. Its module is excluded whenever Unreal Engine is running a commandlet. `UnrealEditor-Cmd.exe` is not itself the commandlet boundary; whether the exclusion applies depends on the launch mode. Supporting the bridge during commandlets or other genuinely headless sessions requires separating commandlet-safe services from interactive-editor and Slate-dependent code.

## Highlights

- **AST-based hallucination defense.** Before any script reaches UE, `bridge_preflight.py` parses it as Python AST and validates every `unreal.UnrealBridge*Library.fn(...)` call against an auto-generated manifest (26 libraries × 1382 UFUNCTIONs) — catching unknown function/library names (with did-you-mean), wrong positional arg counts, unknown kwargs, and non-existent bridge-enum members **without ever round-tripping to the editor**. A second layer redirects raw `AssetRegistry` / `GameplayStatics` usage patterns to their bridge equivalents and tracks each returned value's type so attribute access on a `str` or `SoftObjectPath` doesn't silently misbehave; on a real `AttributeError` from a UE object the bridge calls back into UE Python, lists that live class's reflected `UPROPERTY`s, and emits a paste-ready correction (auto-handles `snake_case` ↔ `PascalCase` mismatches). A third layer ships a kwargs-only Python wrapper module so positional-arg-order errors are structurally impossible. Together these dropped a fresh-context agent's bridge-call failure rate from **24% → 16%** across A/B validation runs — protection that prompt-only "look-up-before-call" rules in `SKILL.md` had failed to deliver.

  <p align="center">
    <img src="docs/images/en/02-preflight.png" alt="Local AST preflight — stop hallucinations before they reach UE">
  </p>

- **Deep asset-structure introspection + author-level write ops.** `UnrealBridgeAnimLibrary` covers full queries over AnimBP state machines, AnimGraph nodes, linked layers, slots, curves, Sequence / Montage / BlendSpace, and the skeleton tree — paired with a full suite of write ops: building an ABP from scratch, adding / removing states / transitions / condition rules, creating and wiring AnimGraph nodes, auto-layout of both the state machine and AnimGraph. `UnrealBridgeAssetLibrary` goes beyond keyword search with forward-dependency and reverse-reference analysis, surfacing a complete dependency view to the Agent. Compared with basic CRUD wrappers or schemes that require hand-assembled reflection calls, this level of structured capability is available out of the box.
- **Deliverable Control Rig and retargeting workflows.** `UnrealBridgeRigLibrary` authors Control Rig hierarchies and connected RigVM graphs, evaluates them on transient instances, configures IK Rig solvers/goals/chains, builds IK Retargeter op stacks/mappings/poses/profiles, initializes the real retarget processor, batch-retargets animation assets, and samples output for root spikes, foot sliding/penetration, and joint discontinuities. Type/property discovery and compile/processor diagnostics keep these workflows on official editor-controller paths rather than private-data mutation.
- **Deliverable Niagara VFX workflows.** `UnrealBridgeNiagaraLibrary` discovers engine templates and scripts; authors Systems, Emitters, stack modules/inputs, User parameters, renderers, materials and bindings; compiles and audits the result; and simulates transient previews with per-emitter particle/memory readback. Production presets cover moving Ribbon/Dynamic Beam/Static Beam weapon trails, directional and radial sparks, layered explosions with a real shockwave/light layer, and material-synchronized dissolve/disintegration particles.
- **Reactive event subscription.** The Agent can subscribe to GAS events, attribute changes, actor lifecycle, AnimNotify, input, timers, and editor-side asset-change events. When the specified event fires, the bridge calls back proactively — no polling needed. This is a scenario that a pure request / response protocol cannot cover.
- **Agent control surface at PIE runtime.** `UnrealBridgeGameplayLibrary` provides aggregated world observation, navigation pathfinding, and input operations for movement / look / jump — suitable for AI-behavior validation, automated testing, and in-game NPC prototyping.
- **Blueprint graph quality toolchain.** More than just auto-layout: `auto_layout_graph`'s `pin_aligned` strategy reads live Slate geometry to align exec rails, `straighten_exec_chain` snaps the main rail, `collapse_nodes_to_function` extracts subgraphs, `lint_blueprint` scans by fixed rules for orphans / unnamed nodes / oversized functions / uncommented large graphs, and `add_comment_box` + preset palette (Section / Validation / Danger / Network / UI / Debug / Setup) partition graphs for readability. AnimGraph and state machines get dedicated `auto_layout_anim_graph` / `auto_layout_state_machine` (the latter recurses into each state's inner graph + every transition rule graph).
- **Native Python execution.** 26 `UnrealBridge*Library` surfaces expose 1382 `UFUNCTION`s in total, covering common subsystems; un-wrapped capabilities are reachable directly through the native `unreal.*` API. Compared to fixed-tool-list MCP schemes or reflection protocols that expose only a single `call` command, this design strikes a balance between flexibility and structure. Every level write op is wrapped in `FScopedTransaction` and supports standard Undo / Redo.

## Architecture

```mermaid
flowchart LR
    Agent["AI Agent"]

    subgraph Host["Agent host"]
      CLI["bridge.py"]
      Pre["AST preflight<br/>(local — rejects bad calls<br/>before TCP send)"]
      Mani[("bridge_manifest.json<br/>26 libs · 1382 UFUNCTIONs")]
    end

    Gen["tools/gen_manifest.py<br/>scans C++ headers"]

    subgraph UE["Unreal Editor 5.3+"]
      Disc["FUnrealBridgeDiscovery<br/>UDP responder"]
      Server["FUnrealBridgeServer<br/>TCP · length-prefixed JSON"]
      Reactive["UnrealBridgeReactiveSubsystem<br/>+ 10 event adapters"]
      Exec["IPythonScriptPlugin::<br/>ExecPythonCommandEx<br/>(GameThread)"]
      Wrap["unreal_bridge<br/>kwargs-only wrapper<br/>(optional safer surface)"]
      Libs["26× UnrealBridge*Library"]
      Engine["UEditor · UWorld · Assets"]
    end

    Agent --> CLI
    CLI -- "AST gate" --> Pre
    Pre -. "lookup" .-> Mani

    Gen -- "writes" --> Mani
    Gen -- "writes" --> Wrap

    CLI -- "UDP probes<br/>multicast + loopback" --> Disc
    CLI -- "TCP / JSON<br/>(port from discovery)" --> Server
    Server -- "RPC script" --> Exec
    Engine -. "delegate fires" .-> Reactive
    Reactive -- "handler script" --> Exec

    Exec -- "user code calls" --> Libs
    Exec -. "or via" .-> Wrap
    Wrap --> Libs
    Libs --> Engine
```

## Quick Start

### 1. Clone the repo

```bash
git clone https://github.com/<your-fork>/UnrealBridge.git
cd UnrealBridge
```

### 2. 🚨 Run `link_agents_skills.bat` (one-time)

**Required for Codex / Gemini CLI / OpenCode / Cursor.** Skip if you only use Claude Code.

The skill source of truth lives at `.claude/skills/`. This script creates an NTFS junction at `.agents/skills/` so every Agent runtime following the [Agent Skills open standard](https://www.agensi.io/learn/agent-skills-open-standard) sees the same content. Junctions can't be committed (Windows git limitation), so each clone has to materialize it locally — **once**.

```bat
link_agents_skills.bat
```

Mac / Linux equivalent: `ln -sfn .claude/skills .agents/skills` (run from repo root).

### 3. Install the plugin and matching skills

Pass the UE project root to the canonical sync command:

```bat
sync_project.bat D:\Path\To\YourProject
```

One invocation mirrors `Plugin/UnrealBridge/` and the matching
`.claude/skills/unreal-bridge/` into the project's plugin, `.agents`, and
`.claude` destinations. Local build outputs are retained. Each destination is
stamped with the source commit. The local source-repository path is stored under
the project's ignored `Saved/UnrealBridge/` directory, allowing the reload
helpers to repeat the same version-locked sync without publishing a machine-local
path. `sync_plugin.bat` is retained as a legacy command-name alias and accepts
the same project-root argument.

### 4. Build & launch

Open the `.uproject` and let UE rebuild the plugin automatically, or run the project's `Build.bat` from the command line. Launch the editor — the plugin starts the server at `PostEngineInit`. You're good once `LogUnrealBridge: Listening on 127.0.0.1:<port>` shows up in the log (the port is OS-assigned; the client finds it via paired multicast + loopback probes — no manual config needed).

### 5. Verify

```bash
python .claude/skills/unreal-bridge/scripts/bridge.py ping
# → pong
python .claude/skills/unreal-bridge/scripts/bridge.py exec \
  "import unreal; print(unreal.UnrealBridgeLevelLibrary.get_level_summary())"
```

### Claude Code integration (optional)

Copy the skill somewhere Claude Code can discover it:

```bash
cp -r .claude/skills/unreal-bridge ~/.claude/skills/            # user-wide
# or into the target project's own .claude/skills/
```

For `rebuild_relaunch.py` to auto-relaunch the editor, set one of:

```bash
setx UNREAL_EDITOR_EXE "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
setx UE_ROOT            "C:\Program Files\Epic Games\UE_5.7"
```

### Quick usage

Once the skill is installed, drop any of these into a Claude Code session:

- *"List every PointLight in the current level."*
- *"Move the PlayerStart up by 200 units."*
- *"Compile `/Game/Blueprints/BP_Character` and tell me whether it has errors."*
- *"Show me the state machines inside `/Game/Animations/ABP_Hero`."*
- *"Create an ABP on `SK_Mannequin` with an Idle / Walk / Run state machine driven by a `Speed` variable (>10 enters Walk, >200 enters Run), then layer a Slot + LayeredBoneBlend in the outer graph for an upper-body overlay."*

The Agent reads `SKILL.md`, picks the right `UnrealBridge*Library` function, calls it through `bridge.py`, and reports back.

## Usage

### CLI

```bash
bridge.py ping
bridge.py exec "print('hello from UE')"
bridge.py exec-file my_script.py
```

Flags (all optional — the common case works with no flags):

- `--project=<name|path>` — disambiguate when >1 editors are running
- `--endpoint=host:port` — skip discovery, connect directly (also env `UNREAL_BRIDGE_ENDPOINT`)
- `--token=<secret>` — only when the server binds non-loopback (also env `UNREAL_BRIDGE_TOKEN`)
- `--timeout` (default 30s), `--json`, `--discovery-timeout=<ms>` (default 800)

`bridge.py list-editors` sends a probe and lists every editor that answered — handy for multi-editor setups.

### From Python inside UE

```python
import unreal

summary = unreal.UnrealBridgeLevelLibrary.get_level_summary()
print(summary)

lights = unreal.UnrealBridgeLevelLibrary.find_actors_by_class(
    "/Script/Engine.PointLight", 50
)
print(len(lights), "point lights")
```

### Two reload loops

```bash
python .claude/skills/unreal-bridge/scripts/hot_reload.py        # body-only edits
python .claude/skills/unreal-bridge/scripts/rebuild_relaunch.py  # reflection changes
```

Installed skill copies auto-detect their containing UE project and the source
repository recorded by `sync_project.bat`. From a source checkout, pass
`--project-dir D:\Path\To\YourProject`; use `--sync-source` only when moving the
source checkout invalidated the recorded path.

## Bridge libraries

| Library | Purpose |
|---|---|
| `UnrealBridgeServer` | TCP listener, length-prefixed JSON framing, GameThread dispatch |
| `UnrealBridgeRigLibrary` | Complete UE 5.7+ Control Rig / IK delivery surface: Control Rig asset/hierarchy/control authoring; discoverable RigVM units/templates, member variables, nodes, pins, links, comments and auto-layout; compiler diagnostics and transient event evaluation; IK Rig solver/goal/chain/excluded-bone setup with reflected settings and humanoid/FBIK auto-characterization; IK Retargeter op stacks, chain mapping, poses, profiles, real processor initialization, and saved batch retargeting; sampled root-motion, foot-contact, and joint-discontinuity quality reports. UE 5.3-5.6 retain the same reflected kwargs API as safe logged stubs. |
| `UnrealBridgeNiagaraLibrary` | Complete UE 5.7+ Niagara/VFX delivery surface: template and script discovery; System/Emitter lifecycle and declarative recipes; stack modules with local/linked/dynamic/object/data-interface inputs; User parameters; Sprite/Ribbon/Mesh/Light/Decal/Component renderers, materials and bindings; warmup/bounds/Effect Type; compiler diagnostics and delivery audits; Trail/Beam, directional/radial Sparks, layered Explosion/shockwave/light, and Dissolve presets; transient preview variables/control/transform movement with per-emitter particle, memory, and CPU readback. UE 5.3-5.6 retain the same reflected kwargs API as safe logged stubs. |
| `UnrealBridgeBlueprintLibrary` | Full-stack Blueprint read / write: class hierarchy / variables / functions / components / interfaces / event dispatchers; graph call relationships, exec flow, pin connections, node search; 20+ node-type insertion (Branch, Cast, loops, Delay, Timer, SpawnActor, MakeStruct, …), pin connect, node-coordinate read / write, alignment, comment boxes, AutoLayoutGraph; runtime debug — set/list/clear breakpoints, `get_last_breakpoint_hit` captures function locals/params/return **plus** the executing object's BP-class instance variables (tagged with `OwnerClass`), PIE node coverage; compile-error query |
| `UnrealBridgeAssetLibrary` | Asset keyword search (include / exclude tokens); derived-class query; forward-dependency and reverse-reference analysis (recursive); DataAsset / StaticMesh / SkeletalMesh / Texture / Sound metadata; transactional StaticMesh/SkeletalMesh default-material authoring by index, slot name, or atomic batch (`save=false` previews supported); folder tree, redirector resolution, batch tag and disk-size query; **SearchableName index queries** (`find_assets_referencing_searchable_name` / `get_searchable_names_used_by_asset` / `list_searchable_name_values`) — the data backing the editor's right-click "Find References" on a `GameplayTag` / `PrimaryAssetId` / any USTRUCT-keyed named value |
| `UnrealBridgeAnimLibrary` | AnimBP deep introspection: state machines, AnimGraph nodes, linked layers, slots, curves; Sequence / Montage / BlendSpace asset info; skeleton tree, Sockets, VirtualBone, BlendProfile. **Write ops**: ABP creation and variables, state machine / state / conduit / transition add / remove / modify, transition properties (crossfade, priority, bidirectional), const-rule shortcut and real variable-driven rules (paired with the BP library to author `KismetMathLibrary` comparator nodes), 9 typed AnimGraph node factories + `add_anim_graph_node_by_class_name` fallback, pin connect / disconnect / reorder, auto-layout of AnimGraph and state-machine interiors; AnimNotify, sync marker, Montage Section, Socket CRUD |
| `UnrealBridgePoseSearchLibrary` | Motion Matching — `UPoseSearchSchema` / `UPoseSearchDatabase` introspection: schema channels and weights, database animation entries, sampling / branch sampling, indexing status (`wait-pose-index` CLI helper); pose evaluation against a runtime pose vector. The `DatabaseAnimationAssets` / `Channels` arrays are `private:` in C++ and unreachable via `get_editor_property` — this library is the only path |
| `UnrealBridgeChooserLibrary` | Motion Matching — `UChooserTable` introspection and authoring: columns, rows (with disabled flag and resolved result), context objects, NestedChooser drill-down (`:Name` paths). Write ops: column add / remove, row add / remove, set context object class with auto-Compile + PostEditChange so the editor refreshes. The `ResultsStructs` / `DisabledRows` arrays are `private:` in C++ — this library is the only path |
| `UnrealBridgeStateTreeLibrary` | StateTree read/write/debug/runtime surface (UE 5.7+): asset creation and schema introspection; GUID-stable state hierarchy, node, consideration, condition, and transition CRUD; schema-filtered native/Blueprint node discovery; discoverable export-text property editing; accessible property bindings; root/state parameters; compiler diagnostics; transient breakpoints; live `UStateTreeComponent` inspection, lifecycle control, and event delivery. Safe stubs are exposed on UE 5.3-5.6 |
| `UnrealBridgeSmartObjectLibrary` | Complete Smart Object authoring/world/runtime surface (UE 5.7+): Definition creation/validation; GUID-stable slot, behavior, definition-data and annotation editing; tag queries/policies; schema-filtered World Conditions; parameters and property bindings; world components and persistent collections; spatial query, dynamic instances, claim/occupy/release, runtime tags/enablement/events, and entrance lookup/offline validation. UE 5.3-5.6 expose safe stubs |
| `UnrealBridgeStructLibrary` | `UUserDefinedStruct` (UDS) author-time surface — create the asset, add / remove / rename / retype / reorder fields, write defaults, set tooltips and per-field "edit on instance" flag. Goes through `FStructureEditorUtils` so writes trigger recompile + downstream BP propagation. The `UserDefinedStructureFactory` and `StructureEditorUtils` are not exposed to native Python; this library is the only way to programmatically build a UDS that DataTable / Blueprint variable types can reference |
| `UnrealBridgeDataTableLibrary` | DataTable row-level read / write with conditional filters; CSV / JSON import / export — file-path variants plus in-memory text variants for LLM-generated content, and `create_data_table_from_csv` / `create_data_table_from_json` to build a fresh DataTable with row-struct resolution by content path or short name; cross-table row copy, row-diff compare; reverse lookup by RowStruct to find every table referencing that struct |
| `UnrealBridgeCurveLibrary` | Curve assets (`UCurveFloat` / `UCurveVector` / `UCurveLinearColor`) and `UCurveTable` rows: asset info, key CRUD (batch-safe + atomic tangent writes), pre / post-infinity extrapolation, auto-tangent recompute, batch sampling (N time points in one round-trip), uniform sampling; curve-table row add / remove / rename / replace. Write ops broadcast `OnCurveChanged` so open Curve Editor tabs refresh instantly |
| `UnrealBridgeMaterialLibrary` | Material instance parameter queries |
| `UnrealBridgeUMGLibrary` | Complete Widget Blueprint delivery surface: asset creation; widget class discovery and hierarchy CRUD; reflected widget/slot properties, Canvas layout and SlateBrush assignment; batched opacity/color/2D-transform animation authoring; compile, accessibility and stale-reference validation; UE 5.7+ FieldNotify ViewModel creation, source configuration, one-way/two-way MVVM bindings and diagnostics (safe logged no-ops on 5.3-5.6); PIE instance spawning, live geometry/state readback, semantic Button/Text/Slider/CheckBox/focus actions, animation playback, dynamic UI-material parameters, and live MVVM writes |
| `UnrealBridgeLevelLibrary` | Actor query (name / Class / Tag / Folder / radius / Box / ray) and edit (spawn / destroy / transform / attach / visibility / Mobility, nested property read / write, function invocation); terrain height profile and Trace probing; in-editor custom NavGraph (nodes, edges, shortest path, JSON persistence); orthographic top-down view plus animation Pose / Montage timeline screenshots; every write op runs in a transaction |
| `UnrealBridgeEditorLibrary` | Editor session control: asset open / close / save / load; Content Browser and viewport; PIE start / stop / simulate / pause; undo / redo, console commands, CVars; batch Blueprint compile, redirector fixup; Live Coding trigger; raw viewport plus WYSIWYG presented-window screenshots (including one-frame debug overlays), GBuffer channels (Depth / DeviceDepth / Normal / BaseColor) and HitProxy ID pass; tabs, notifications, diagnostics. Bridge self-observation: call log (ring-buffered request id, latency, endpoint, output size), latency stats, signature-registry JSON dump (one shot returns metadata for all 1384 reflected `UFUNCTION`s) |
| `UnrealBridgeGameplayAbilityLibrary` | GameplayAbility / GameplayEffect / AttributeSet Blueprint metadata; tag hierarchy and matching; list abilities and effects by tag; actor ASC state (attribute values, active abilities / effects, cooldown checks); runtime `SendGameplayEvent` and attribute mutation; GA / GE / GC Blueprint authoring (CDO edit, GA graph nodes, GE magnitude / component / inherited tags, GC tag set) |
| `UnrealBridgeGameplayTagLibrary` | GameplayTag refactoring: `find_assets_referencing_tag` (with child-tag expansion), `list_all_registered_tags`, `get_tag_source_info`. Mutations `add_gameplay_tag` / `rename_gameplay_tag` (auto-redirect, redirect persistence hardened against UE 5.7's silent-drop quirk) / `remove_gameplay_tag`. Source enumeration via `list_tag_source_inis`; redirect ledger via `list_gameplay_tag_redirects` + `remove_gameplay_tag_redirect` for enumerate-then-sweep cleanup |
| `UnrealBridgePerfLibrary` | AAA-grade performance instrumentation across eight dimensions. **Point-in-time**: frame timing (FPS / GT / RT / GPU / RHI ms via `FStatUnitData` + RHI globals), render counters, process memory, `TObjectIterator` class histogram, ISO-8601-stamped aggregate snapshot. **Memory & asset breakdown**: texture / mesh / audio / UObject grouped by folder / LOD group / compression format / class — disk or runtime mode; top-N largest assets across any UClass; world-actor breakdown by class × level (World Partition partial). **Time series**: opt-in periodic sampling with ring buffer, always-on frame-time histogram, hitch log via `OnEndFrame` hook, CSV export, **`get_frame_time_percentiles([50,90,95,99])`** for AAA-grade tail-latency investigation. **Render breakdown**: per-actor render cost, LOD distribution, primitives-by-material, shadow casters, Lumen / Nanite diagnostics; **`get_texture_streaming_residency`** (per-texture resident vs wanted mip + pool over-budget), **`get_render_target_memory`** (per-subclass RT byte totals), **`get_per_pass_gpu_timings`** (BasePass / Lumen / Translucency averages from `FRealtimeGPUProfiler`; falls back gracefully on UE 5.7's new RHI profiler), **`analyze_all_materials`** (cross-library complexity heuristic to surface heaviest masters). **Live trace control**: `start_trace_capture` / `stop_trace_capture` / `list_trace_channels` / `get_trace_state` wrapping `FTraceAuxiliary`. **Trace summary parsers** (5.7+): `parse_trace_to_summary` returns CPU + GPU hot scopes + per-thread hot scopes + counters + load-time breakdown + frame stats from a `.utrace` file in one call; specialised `parse_alloc_trace_to_summary` (peak commit + tag inventory + alloc/free delta), `parse_net_trace_to_summary` (per-game-instance + per-connection traffic totals), `parse_cook_trace_to_summary` (top-N packages by `BeginCacheCookedPlatformData` for 4-hour cook attribution). **Regression workflow**: `compare_perf_snapshots(before, after, threshold)` returns per-field deltas + flagged regressions list; `begin_auto_hitch_capture` / `end_auto_hitch_capture` ring-buffer rich snapshots on every frame ≥ threshold; `begin_insights_for_trace` shells out UnrealInsights.exe for human handoff |
| `UnrealBridgeGameplayLibrary` | PIE-runtime Agent control: aggregated world observation, navigation pathfinding; movement / look / jump / teleport / sticky input, Enhanced Input **runtime injection plus IA / IMC enumeration and IMC mapping authoring** (`list_input_actions` / `list_input_mapping_contexts` / `get_input_mapping_context_mappings` / `add_ia_mapping_to_imc` / `remove_ia_mapping_from_imc` — the binding side that previously required raw `unreal.*`); pawn velocity, ability, jump-arc simulation; camera ray, screen ↔ world, NavMesh projection; damage, physics impulse, time dilation, sound, camera shake; debug draw; AI-controller probing |
| `UnrealBridgeNavigationLibrary` | Export NavMesh as OBJ for external visualization and geometry analysis |
| `UnrealBridgeProceduralLibrary` | Procedural content authoring primitives — point-list-in / point-list-out sampling + filters + instancing on the editor world. Deterministic given `(params, seed)`: `FRandomStream(Seed)` + `ECC_Visibility` + `bTraceComplex=true` for surface trace; Poisson-2D / grid / radial / spline / mesh-surface samplers; slope / min-distance / mask filters; ISM / HISM batch spawn; Landscape grid + project-to-surface (callable as plain Python arrays — intentionally NOT a PCG-graph wrapper) |
| `UnrealBridgeGeometryLibrary` | Geometry Script wrapper — `UDynamicMesh` handle pool + cross-engine asset I/O (`copy_mesh_from_static_mesh` / `create_new_static_mesh_asset_from_mesh`) + 25+ ops covering primitives / boolean / smooth / decimate / displace / voxel-merge / uv-unwrap / bake normals + occlusion / extrude / sweep-along-spline / selection. Field names follow standard UE Python snake_case (`bHasNormals` → `.has_normals`) |
| `UnrealBridgePCGLibrary` | PCG (Procedural Content Generation) read + trigger only — NO graph editing (PCG's territory; agents write code not visual graphs). Component override get / set, generate / cleanup, asset graph introspection. Whole library gated to UE 5.7+ with stub bodies on 5.3-5.6 |
| `UnrealBridgeReactive*` | Event subscription framework with 10 adapters: runtime (GameplayEvent, AttributeChanged, ActorLifecycle, MovementMode, AnimNotify, InputAction, Timer) and editor (AssetEvent, PieState, BpCompiled); handler register / list / pause / resume / stats; cross-session JSON persistence. Replaces polling |
| `UnrealBridgePropertyLibrary` | **Privileged generic UPROPERTY surface.** Read / write any reflected property by dotted path with `[N]` array indexing — bypasses UE Python's binding-layer access checks (the "is protected and cannot be read" rejection, the EditDefaultsOnly-on-struct-copy rejection that blocks nested writes like `Modifiers[0].ModifierMagnitude.ScalableFloatMagnitude.Value`). `list_u_properties` returns full reflection (private/protected/bare UPROPERTY + decoded EPropertyFlags + metadata map); `array_append_u_property` auto-detects FGameplayTagContainer to maintain ParentTags cache; `get_asset_cdo_path` resolves the CDO path correctly. Wraps writes in `FScopedTransaction` + optional `PostEditChangeChainProperty` for editor-window refresh. |

## Protocol

Two channels:

1. **UDP discovery** on port `9876`. The client sends the same request-id-bearing `probe` to LAN multicast `239.255.42.99` and local loopback `127.0.0.1`, then de-duplicates replies by editor PID. Multicast preserves LAN/multi-editor discovery; loopback keeps local discovery reliable when Windows, a VPN, or a virtual NIC drops multicast loopback. Every editor replies with its bound TCP address + port + token fingerprint. Multiple editors coexist via `SO_REUSEADDR`.

2. **TCP data** on the port the editor reports in its discovery response (OS-assigned; `127.0.0.1` by default). Length-prefixed JSON:

```
Request :  [4-byte big-endian length][{"id","script","timeout","token?"}]
Response:  [4-byte big-endian length][{"id","success","output","error"}]
Ping    :  {"id","command":"ping"}  →  pong
```

Token auth kicks in automatically when the server binds non-loopback; the client reads the token from `<Project>/Saved/UnrealBridge/token.txt` and includes it in every request.

Scripts run on the GameThread; captured stdout and stderr are separated by the special `__UB_ERR__` sentinel.

### Server config (CLI / env / `EditorPerProjectUserSettings.ini [UnrealBridge]`)

| CLI | Env | Default |
|---|---|---|
| `-UnrealBridgeBind=` | `UNREAL_BRIDGE_BIND` | `127.0.0.1` |
| `-UnrealBridgePort=` | `UNREAL_BRIDGE_PORT` | `0` (OS-assigned) |
| `-UnrealBridgeToken=` | `UNREAL_BRIDGE_TOKEN` | empty (required when bind ≠ loopback) |
| `-UnrealBridgeDiscoveryGroup=` | `UNREAL_BRIDGE_DISCOVERY_GROUP` | `239.255.42.99:9876` |
| `-UnrealBridgeNoDiscovery` *(flag)* | `UNREAL_BRIDGE_DISCOVERY=0` | discovery on |

## Repository layout

```
UnrealBridge/
├── Plugin/UnrealBridge/         # UE 5.3+ Editor plugin (C++)
│   ├── Source/UnrealBridge/     #   TCP server + bridge libraries
│   └── Content/Python/          #   Helpers auto-loaded into UE's Python env
├── .claude/skills/unreal-bridge/
│   ├── scripts/                 # bridge.py, hot_reload.py, rebuild_relaunch.py
│   └── references/              # Per-library API docs
├── docs/                        # Design notes and plans
├── tools/                       # Standalone helpers
├── sync_project.bat             # Mirror plugin + matching skills into a UE project
└── sync_plugin.bat              # Legacy alias for sync_project.bat
```

## Requirements

- **Unreal Engine 5.3+** with `PythonScriptPlugin` and `GameplayAbilities` (both ship with the engine). The matrix at `tools/build_matrix.py` verifies clean BuildPlugin against 5.3.2 / 5.4.4 / 5.5.4 / 5.6.1 / 5.7.1 / 5.8.0; some libraries (Chooser / PoseSearch / Material / Navigation / StateTree / Smart Object / Control Rig, IK, and Niagara), UMG MVVM, and a few standalone UFUNCTIONs require 5.7+, while lower versions retain safe StateTree/Smart Object/Rig/Niagara stubs and UMG MVVM no-ops; a handful of inline shims cover 5.3 and 5.8 — see [docs/version-compatibility.md](docs/version-compatibility.md). UE 5.2 and earlier are not supported.
- **Windows 10/11** — the plugin itself is portable, but paths inside the helper scripts are hard-coded Windows-style
- **Python 3.9+** on PATH
- **Visual Studio 2022** with the UE workload — for plugin compilation. **Toolchain notes:**
  - **5.5 / 5.6 / 5.7 / 5.8** work with current MSVC (verified on **14.44.35207**, VS 17.14).
  - **5.3 / 5.4** require MSVC with `_MSC_VER ≤ 1939` (verified on **14.38.33130**, VS 17.8). Newer MSVCs hit an engine-side `C4668: '__has_feature' is not defined` in `ConcurrentLinearAllocator.h` that 5.3 / 5.4's UBT promotes to a hard error via `/we4668`; 5.5+ guarded the macro and dropped the promotion. To verify 5.3 / 5.4 on a machine with both toolchains installed: temporarily pin `<CompilerVersion>14.38.33130</CompilerVersion>` in `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`, run the affected matrix entries, then restore the prior value. Do not patch engine source.
  - **5.8 source build** (vs. Launcher install) may need the UBA executor disabled if the engine snapshot is missing the `UbaDetours.dll` content normally fetched by `Setup.bat`. Configure via `engines.local.json`'s per-engine `env` block: `"env": { "UnrealBuildTool_BuildConfiguration__bAllowUBAExecutor": "false" }`. UBT falls back to the local ParallelExecutor.
- **Claude Code CLI** — optional, only if you use the bundled skill

## Safety

- Every level-edit op is wrapped in `FScopedTransaction` — Ctrl+Z in the editor reverts anything the bridge did.
- The TCP server binds to `127.0.0.1` only; it is not reachable from the network.

## License

MIT — see [LICENSE](LICENSE).

---

<p align="center">
  <img src="docs/images/en/03-outro.png" alt="UnrealBridge — Turn the Unreal Editor into a programmable surface for AI Agents">
</p>

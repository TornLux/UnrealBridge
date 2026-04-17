# Reactive Python handlers — unified event-to-script framework

Status: **P1–P6 all done** (as of 2026-04-18). Ten adapters: seven
runtime (GameplayEvent, AttributeChanged, ActorLifecycle,
MovementModeChanged, AnimNotify, InputAction, Timer) + three editor
(AssetEvent, PieEvent, BpCompiled). Editor-scope handlers get
`ed_<seq>_<rand4>` ids via `register_editor_*`. All edge paths
verified including `WhileSubjectAlive` (global-dispatch cleanup) and
`defer_to_next_tick` stress (50-fire FIFO). **P6.B3 JSON persistence
shipped**: registry auto-saves to `<ProjectSaved>/UnrealBridge/
reactive-handlers.json` with 100 ms debounce and auto-loads at editor
start; HandlerIds preserved across restart; PIE-tied subjects deferred
and re-bound on PostPIEStarted. `bridge-reactive.md` reference doc
covers every trigger + persistence; `bridge-anim-api.md` +
`bridge-gameplayability-api.md` cross-link back. Still build-only at
runtime: AnimNotify + InputAction (fixture-blocked — no AnimBP w/
notify or IA asset in the project).

Let an agent register Python scripts that fire when UE events occur
(GameplayEvent, AnimNotify, MovementMode change, attribute crossed
threshold, …), so reactive gameplay/tooling logic doesn't require the
agent to be online polling. The bridge exec model stays request/
response; reactive scripts live in C++ and execute on the GameThread the
moment the underlying delegate fires.

## Why a unified framework, not one-off per event

At least eight UE event surfaces want the same shape: bind a C++
delegate, run a stored Python script with event payload as context.
Building a per-feature solution each time means re-implementing
lifetime, cleanup, re-entrancy guards, stats, and error policy 8×.
A single registry + per-source adapter pays back after the second
trigger type.

Confirmed event sources to cover:

| Domain   | Trigger                                 | Example use                            |
| -------- | --------------------------------------- | -------------------------------------- |
| GAS      | `GenericGameplayEventCallbacks`         | hit reaction, combo windows            |
| GAS      | `GetGameplayAttributeValueChangeDelegate` | HP threshold → music swap            |
| GAS      | `AbilityActivated/Ended/Cancelled`      | combo continuation                     |
| GAS      | `OnActiveGameplayEffectAddedDelegate…`  | buff on/off VFX                        |
| GAS      | GameplayTag count changed               | state-machine-style conditions         |
| Anim     | AnimNotify (by name)                    | attack-frame SFX/VFX, hit windows      |
| Anim     | Montage Started/Ended/Interrupted       | recovery cancel                        |
| Anim     | StateMachine state enter/exit           | landed/sprint reactions                |
| Movement | `MovementModeChangedDelegate`, Landed   | jump apex, water entry                 |
| Input    | EnhancedInput Triggered/Started/…       | reactive (player presses) ≠ existing sticky-inject (we press) |
| Actor    | BeginPlay / EndPlay / Destroyed / Possessed | re-bind on pawn swap, follow new spawns |
| Physics  | OnHit / OnOverlap                       | trigger volume scripting               |
| AI       | Perception updated                      | sight/hearing reactions                |
| Timer    | Periodic / N seconds delay              | autonomous loops, deferred reactions   |
| Editor   | AssetAdded/Renamed/Saved, BP compiled, PIE start/stop, CVar changed | tooling agents (P5 — lower priority) |

## Lifetime split: ONE subsystem, TWO registration surfaces

**Refinement during P1 implementation:** the original plan called for two
subsystems (`UBridgeReactiveRuntimeSubsystem` as
`UGameInstanceSubsystem` + `UBridgeReactiveEditorSubsystem` as
`UEditorSubsystem`), but a `UGameInstanceSubsystem` only exists during
PIE — which means an agent could not `register_runtime_*` before
starting PIE. That contradicts the agent's mental model of "register at
any time, it'll fire when the event happens." Collapsed to one
`UEditorSubsystem` (`UBridgeReactiveSubsystem`) that holds both
runtime-scope and editor-scope handlers.

- **Single registry:** `UBridgeReactiveSubsystem` (`UEditorSubsystem`).
  Lives for the editor session. Adapters bind and run inside it.
- **API still splits:** `register_runtime_*` vs `register_editor_*`
  (editor surface coming in P5). Scope stored on each record.
- **HandlerId prefix encodes scope:** `rt_NNNN_xxxx` / `ed_NNNN_xxxx`.
- **Cleanup triggers:**
  - `FWorldDelegates::OnWorldCleanup` purges handlers whose Subject's
    World matches the cleaned world (PIE-tied actors die → handlers
    drop). Handlers with null Subject (global) survive.
  - `FEditorDelegates::EndPIE` purges `WhilePIE` handlers on every PIE
    stop regardless of Subject.
  - Subsystem `Deinitialize` unbinds every `FDelegateHandle` via each
    adapter's `Shutdown()`.

## Core data model

```cpp
struct FBridgeHandlerRecord
{
    FString HandlerId;          // system-issued, e.g. "rt_0007_b3a1"
    EBridgeTrigger TriggerType; // GameplayEvent / AnimNotify / ...
    TWeakObjectPtr<UObject> Subject;  // ASC, Mesh, Pawn, ... or null = global
    FName Selector;             // GameplayTag name / Notify name / Attribute name / ...

    FString Script;             // inline source (always populated)
    FString ScriptPath;         // optional: source file, for agent traceability

    // --- Agent-facing metadata (REQUIRED at register, returned by list/get) ---
    FString TaskName;           // short label, e.g. "命中反应"
    FString Description;        // long text, what this handler does and why
    TArray<FString> Tags;       // free-form labels for filtering

    EBridgeLifetime Lifetime;   // Permanent | Once | Count(N) | WhilePIE | WhileSubjectAlive
    int32 RemainingCalls;       // for Count(N); -1 = unlimited
    EBridgeErrorPolicy ErrorPolicy; // LogContinue (default) | LogUnregister | Throw
    int32 ThrottleMs;           // 0 = unlimited fire rate
    bool  bPaused;

    FDateTime CreatedAt;
    FBridgeHandlerStats Stats;  // calls, total_us, max_us, last_error, last_fire_time
};
```

**HandlerId issuance.** System-generated. Format: `<scope>_<seq>_<rand4>`,
e.g. `rt_0042_b3a1`, `ed_0007_91ce`. Sequence is per-subsystem, rand4
prevents accidental collisions across subsystem reloads. Returned to
Python on register; agent stores it. Repeated registration with the
same metadata `TaskName` does **not** auto-replace — agent must
explicitly `unregister(id)` first, to keep semantics predictable.

**Required metadata at registration.** `task_name` and `description`
are mandatory — these are what `list_handlers()` shows so an agent
landing in a fresh session can immediately understand "what is each
handler doing and why". `script_path` is optional but encouraged when
the script lives in a file (agent can grep, diff, reload).

**Agent introspection contract:**

```python
unreal.UnrealBridgeReactiveLibrary.list_all_handlers()
# → [
#     { "id": "rt_0042_b3a1",
#       "scope": "runtime",
#       "task_name": "命中反应",
#       "description": "Player takes Event.Combat.Hit → trigger flinch + screen shake",
#       "trigger": "gameplay_event:Event.Combat.Hit",
#       "subject": "/Game/.../BP_Player.BP_Player_C",
#       "script_path": "scripts/handlers/hit_react.py",
#       "lifetime": "Permanent",
#       "paused": false,
#       "stats": { "calls": 17, "avg_us": 412, "max_us": 1830 } },
#     ...
#   ]
```

## Adapter interface

```cpp
class IBridgeReactiveAdapter
{
public:
    virtual EBridgeTrigger GetTriggerType() const = 0;
    virtual void OnHandlerAdded  (const FBridgeHandlerRecord&) = 0; // bind delegate if first
    virtual void OnHandlerRemoved(const FBridgeHandlerRecord&) = 0; // unbind if last
    virtual void Shutdown() = 0;        // unbind all on subsystem deinit
    virtual TMap<FString,FString> DescribeContext() const = 0; // doc-only: what ctx keys this trigger injects
};
```

Each adapter owns its `FDelegateHandle`s and a reverse map keyed by
`(Subject, Selector)`. When an event fires, adapter builds a context
dict (UObject paths, primitive values) and calls
`Subsystem->Dispatch(TriggerKey, ContextDict)`. All cross-cutting
concerns (depth guard, stats, throttle, lifetime decrement, error
policy) live in `Dispatch`, written once.

## Script execution and context injection

No private-header coupling to PythonScriptPlugin. Build a preamble in
C++, prepend to the user script, hand the whole string to
`IPythonScriptPlugin::ExecPythonCommandEx`:

```python
# === bridge reactive preamble (auto-generated) ===
import unreal

# Per-trigger context payload
ctx = {
    'trigger':    'gameplay_event',
    'tag':        'Event.Combat.Hit',
    'instigator': unreal.load_object(None, '/Game/.../BP_Player_C_0'),
    'target':     unreal.load_object(None, '/Game/.../BP_Enemy_C_3'),
    'magnitude':  12.5,
    'time':       1234.5,
}
# Convenience aliases for the most-used keys
event_tag, event_instigator, event_target, event_magnitude = (
    ctx['tag'], ctx['instigator'], ctx['target'], ctx['magnitude'])

# Handler identity + metadata accessible from inside the script
handler_id = 'rt_0042_b3a1'
handler_task_name = '命中反应'

# Shared state across all handlers (user decision: enable cross-handler coordination)
import sys
_mod = sys.modules.setdefault('_bridge_reactive_state', type(sys)('_bridge_reactive_state'))
if not hasattr(_mod, 'shared'):    _mod.shared = {}
if not hasattr(_mod, 'private'):   _mod.private = {}
bridge_state = _mod.shared                              # cross-handler dict
state        = _mod.private.setdefault(handler_id, {})  # per-handler dict, persists across calls

# Helpers
def defer_to_next_tick(script_str):
    unreal.UnrealBridgeReactiveLibrary.defer_to_next_tick(script_str)
def log(msg):
    unreal.log(f'[{handler_task_name}|{handler_id}] {msg}')

# === user script ===
<USER_SCRIPT>
```

**Shared state design (user decision).** Two scopes coexist:
- `bridge_state` — single dict shared across **all** handlers. Use for
  cross-handler coordination (e.g. one handler sets `bridge_state['in_combo'] = True`,
  another reads it).
- `state` — per-handler dict, isolated per `handler_id`. Use for
  handler-private memory (counters, last-fire timestamps, cached refs).

Both survive across handler invocations because they live on a module
attached to `sys.modules`. They survive even across bridge client
reconnects because the Python interpreter outlives the TCP server.
They are wiped on editor restart only.

## Lifecycle and cleanup (highest-bug-density area)

- **Subject death.** `TWeakObjectPtr` null-checked at dispatch entry;
  null subject ⇒ remove handler, fire `LogUnrealBridge` line so agent
  can detect the disappearance via stats query.
- **WorldCleanup.** Both subsystems subscribe to
  `FWorldDelegates::OnWorldCleanup` to mass-purge handlers whose
  Subject's outer-world matches the cleaned world. Handlers with
  `Subject == nullptr` (global) survive PIE cycles by design.
- **PIE stop.** Runtime subsystem's `Deinitialize` unbinds every
  adapter delegate. Editor subsystem keeps running.
- **Replacement.** Same `HandlerId` cannot be re-registered (system
  rejects with explanatory error). Agent must `unregister(id)` first;
  this is intentional — keeps "what's running" knowable.
- **Module shutdown.** Both subsystems' `Deinitialize` calls every
  adapter's `Shutdown()`, which unbinds every `FDelegateHandle`.
  Without this, plugin reload during editor session crashes.

## Re-entrancy, throttling, error policy

- **Depth guard.** Thread-local int incremented around each dispatch.
  Warn at depth > 8, abort at depth > 16 with `LogUnrealBridge` error.
  Prevents handler-A-fires-event-B-which-fires-handler-A loops.
- **Snapshot dispatch.** Before iterating handlers for a trigger,
  copy the matching IDs into a local array. Lets handler scripts
  register/unregister without invalidating iteration.
- **Throttle.** Per-handler `ThrottleMs`. Adapter checks
  `now - LastFireTime >= ThrottleMs` before dispatching. Useful for
  high-frequency events (Tick, AttributeChanged on regen attribute).
- **Error policy.**
  - `LogContinue` (default): `ExecPythonCommandEx` returned false ⇒
    log error, increment `Stats.last_error`, keep handler active.
  - `LogUnregister`: same but auto-removes the handler. For "noisy
    handler hit during testing — disable until I look".
  - `Throw`: re-raises into UE log as `Error` severity (calls
    `UE_LOG(... Error ...)`). Debug only.

## Python API shape

New library `UnrealBridgeReactiveLibrary` (cross-cutting; doesn't fit
existing per-domain libraries).

```python
# --- Runtime registration (PIE/game world) ---
unreal.UnrealBridgeReactiveLibrary.register_runtime_gameplay_event(
    task_name="命中反应",
    description="Player ASC: Event.Combat.Hit → flinch + screen shake",
    target_actor=player,
    event_tag="Event.Combat.Hit",
    script="...",                  # OR script_path="scripts/handlers/hit_react.py"
    script_path="",                # optional, recorded for introspection
    tags=["combat", "feedback"],   # optional, for filter queries
    lifetime="Permanent",          # Once | Count:5 | WhilePIE | WhileSubjectAlive
    error_policy="LogContinue",    # LogUnregister | Throw
    throttle_ms=0,
) -> str  # returns HandlerId

unreal.UnrealBridgeReactiveLibrary.register_runtime_attribute_changed(...)
unreal.UnrealBridgeReactiveLibrary.register_runtime_anim_notify(...)
unreal.UnrealBridgeReactiveLibrary.register_runtime_movement_mode_changed(...)
unreal.UnrealBridgeReactiveLibrary.register_runtime_input_action(...)   # reactive, distinct from sticky-inject
unreal.UnrealBridgeReactiveLibrary.register_runtime_actor_lifecycle(...) # BeginPlay/EndPlay/Destroyed
unreal.UnrealBridgeReactiveLibrary.register_runtime_timer(interval_seconds=..., ...)

# --- Editor registration (P5; small surface) ---
unreal.UnrealBridgeReactiveLibrary.register_editor_asset_event(...)   # added/renamed/saved
unreal.UnrealBridgeReactiveLibrary.register_editor_pie_event(...)     # start/stop/pause
unreal.UnrealBridgeReactiveLibrary.register_editor_bp_compiled(...)
unreal.UnrealBridgeReactiveLibrary.register_editor_timer(...)         # editor-tick interval

# --- Common management (works on either scope; id encodes scope) ---
unreal.UnrealBridgeReactiveLibrary.unregister(handler_id) -> bool
unreal.UnrealBridgeReactiveLibrary.list_all_handlers(filter_scope=None,
                                                    filter_trigger=None,
                                                    filter_tag=None) -> List[Dict]
unreal.UnrealBridgeReactiveLibrary.get_handler(handler_id) -> Dict     # full record incl. script source
unreal.UnrealBridgeReactiveLibrary.get_handler_stats(handler_id) -> Dict
unreal.UnrealBridgeReactiveLibrary.pause(handler_id)  -> bool
unreal.UnrealBridgeReactiveLibrary.resume(handler_id) -> bool
unreal.UnrealBridgeReactiveLibrary.clear_all(scope="runtime"|"editor"|"all") -> int  # returns count cleared

# --- Helpers usable from inside handler scripts (also injected to preamble) ---
unreal.UnrealBridgeReactiveLibrary.defer_to_next_tick(script_str)
unreal.UnrealBridgeReactiveLibrary.describe_trigger_context(trigger_name) -> Dict
    # → {"tag": "FName", "instigator": "AActor*", ...} so agent knows what ctx keys exist
```

## Connection points to existing code

- **`UnrealBridgeServer.cpp` ticker queue** — unchanged. Reactive
  handlers run inline on UE's event-callback thread (= GameThread),
  do not enter the bridge exec queue, do not contend with synchronous
  bridge `exec` calls.
- **`UnrealBridgeGameplayLibrary.cpp:38` sticky-input ticker** —
  user decision: do **not** migrate. Re-evaluate after the framework
  has been in use for ~2 weeks.
- **`bridge-*.md` references** — add `bridge-reactive.md` with two
  worked examples: GameplayEvent hit-reaction; AnimNotify attack-frame
  SFX. Cross-link from `bridge-gameplayability-api.md` and
  `bridge-anim-api.md`.

## Phasing

| Phase | Status | Scope                                                                                                                                                  | Acceptance                                                                                            |
| ----- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------- |
| **P1** | ✅ done | Single `UBridgeReactiveSubsystem` (refined from dual-subsystem plan) + `FBridgeHandlerRecord` + system HandlerId issuance + reverse index + Dispatch (snapshot, depth guard, stats, throttle, error policy, Once/Count lifetime) + preamble builder + shared/private state setup + WorldCleanup wiring | Subsystem deinit unbinds cleanly; foundation built and proven via P2 tests. |
| **P2** | ✅ done | `FGameplayEventAdapter` + `register_runtime_gameplay_event` + unregister/list/get/stats/pause/resume/clear_all + `defer_to_next_tick`, `describe_trigger_context`. Added `EnsureAbilitySystemComponent` + `SendGameplayEventByName` test helpers to GameplayAbilityLibrary. ASC walker now traverses Pawn→PlayerState→Controller→PC.PlayerState. | 15 assertions green: register→list→fire(3x)→stats→pause→resume→Once auto-unregister→unregister. **ASC walker path not yet exercised** (test used direct `FindComponentByClass`). |
| **P3** | ✅ done | Adapters: `AttributeChanged`, `ActorLifecycle` (Destroyed+EndPlay), `MovementModeChanged`, `AnimNotify`, `InputAction`. All 5 `register_runtime_*` entry points. `AdapterPayload` field added to `FBridgeHandlerRecord` so InputAction can pass UInputAction path without a fragile side-channel. Listener UClasses (`UBridgeMovementModeListener`, `UBridgeAnimNotifyListener`, `UBridgeActorLifecycleListener`, `UBridgeInputActionListener`) for the 4 dynamic-delegate triggers. | ActorLifecycle (Destroyed + EndPlay-Once), MovementModeChanged runtime-verified on player pawn (Walking→Flying→Falling→Walking, 3 fires). describe_trigger_context returns proper specs for all 6 triggers. list_all_handlers filter_trigger works. **AttributeChanged/AnimNotify/InputAction runtime-untested** (need UAttributeSet / AnimBP / possessed pawn fixtures). |
| **P4** | ✅ done | `defer_to_next_tick` 50-fire stress (FIFO preserved), `WhileSubjectAlive` cleanup verified via null-subject dispatch path, re-entrancy depth guard capped at 16, LogUnregister + throttle already verified 2026-04-16, `bridge-reactive.md` shipped with Timer / GameplayEvent / cross-handler examples, bridge-anim-api.md + bridge-gameplayability-api.md cross-link back. | Agent can read `bridge-reactive.md` cold and register a working handler in one exec. ✓ |
| **P5** | ✅ done | 3 editor adapters (`AssetEvent` refcount + 4 AR delegates; `PieState` refcount + 8 FEditorDelegates; `BpCompiled` per-subject `UBlueprint::OnCompiled()` + global `GEditor->OnBlueprintCompiled()`). 3 `register_editor_*` entry points in `UnrealBridgeReactiveLibrary` with scope='editor' so HandlerIds prefix `ed_`. | Renamed material captured with correct old/new path + class. PIE start+stop produces 5 phases in correct order (PreBegin→Begin→PostStarted, then PrePIE→End). BP compile fires both per-subject (with payload) and global (empty payload). ✓ |
| P6 | ✅ done | B1 ✅ tag wildcard matching (2026-04-17). B2 ✅ global GameplayEvent listener (2026-04-17). **B3 ✅ JSON persistence (2026-04-18)**: every register_* fills `RegistrationContext`; subsystem debounces save 100 ms after Register/Unregister/Clear; Initialize auto-loads; `ResolveForRestore` in the library rebuilds Subject/Selector/AdapterPayload per-trigger; PIE-tied handlers defer to `PostPIEStarted`. `HandleWorldCleanup` moves non-WhilePIE records to `DeferredHandlers` (preserving their JSON entry) rather than deleting. | Restart round-trip verified: register 4 handlers (GE/Timer/AssetEvent/BpCompiled) → save → full editor quit + relaunch → 3 live with preserved ids, 1 deferred → start PIE → GE re-binds with same id → fire event → handler runs. ✓ |

## Untested paths (carried forward from P1–P3)

These code branches exist but no runtime test has exercised them yet.
Flagged so a future session can close the gap:

- **ASC walker** — ✅ verified 2026-04-16 via `EnsureAbilitySystemComponent(pawn, "Controller")` + `register_runtime_gameplay_event` on pawn. Handler fired, proving Pawn→Controller branch works.
- **AttributeChanged adapter end-to-end** — ✅ verified 2026-04-16 via `UBridgeTestAttributeSet`: 3 Health modifications (100→80→50→100) produced 3 handler fires with correct new/old/delta; Mana change on same ASC did NOT fire the Health handler (selector filtering confirmed).
- **AnimNotify adapter end-to-end.** Still build-only. Needs an actor with `USkeletalMeshComponent` + AnimBP that plays a montage emitting a notify — no such fixture in the current DefaultMap.
- **InputAction adapter end-to-end.** Still build-only. Needs a possessed pawn with a bound `UEnhancedInputComponent` and a real `UInputAction` asset. AssetRegistry scan for `InputAction` class returned zero results in this project; may require a synthetic IA or authoring one.
- **WorldCleanup + WhilePIE on PIE stop** — ✅ verified 2026-04-16. Both Permanent-with-PIE-subject and WhilePIE handlers are removed when PIE ends.
- **Re-entrancy depth guard** — ✅ verified 2026-04-16. Handler self-firing GameplayEvents caps at exactly 16 dispatches (matches `MaxDispatchDepth`).
- **LogUnregister error policy** — ✅ verified 2026-04-16. Handler raising an exception under `LogUnregister` is auto-removed from the registry.
- **Throttle** — ✅ verified 2026-04-16. `throttle_ms=200` suppresses 2 of 3 rapid fires; a post-window fire passes.
- **WhileSubjectAlive** — ✅ verified 2026-04-17 via global-dispatch cleanup. Setup: spawn temp actor A + ASC, register WSA handler on A (trigger=GameplayEvent, tag T) plus a *global* handler on tag T, fire once on A (both fire), destroy A, fire tag T on player (different ASC). Adapter's null-subject dispatch pass matches the global handler AND the stale WSA handler (R.Subject.Get() now returns nullptr → pointer-equal to the event's null subject). `ExecuteHandlerOnce` runs the liveness check (`!IsExplicitlyNull && !IsValid`) → WSA handler auto-removed. Key insight: WSA-only cleanup REQUIRES a companion global handler on the same trigger+selector so a null-subject dispatch fires — without that, the handler just lingers until PIE end / editor restart.
- **defer_to_next_tick stress** — ✅ verified 2026-04-17. 50 rapid GameplayEvents each queue a deferred script via `defer_to_next_tick`; after a couple editor ticks the DeferTicker drained all 50 with FIFO ordering preserved (order = [1..50]).

## Open questions resolved

- ✅ Unified framework, not per-event one-offs.
- ✅ Lifetime split: separate `register_runtime_*` and `register_editor_*` entry points; editor scope is small/lower-priority.
- ✅ HandlerId system-issued; agent stores it. Re-registration of same id rejected — agent must `unregister` first.
- ✅ Required metadata: `task_name` + `description` (mandatory), `script_path` + `tags` (optional). Surfaced by `list_all_handlers` so a fresh agent can introspect what's running and why.
- ✅ Sticky-input ticker not migrated for now.
- ✅ Cross-handler shared state via `bridge_state`; per-handler private state via `state`. Both injected to preamble.

## P5 implementation strategy (shipped 2026-04-17)

> **Status: ✅ done.** Three adapters implemented as designed below,
> three `RegisterEditor*` library entry points, smoke-tested end-to-end.
> Notable deviation from the sketch: `BpCompiled` global mode has to
> tolerate a **no-payload** broadcast (`GEditor->OnBlueprintCompiled()`
> is param-less) — ctx reports `blueprint_path=''` + `parent_class=''`
> for global fires. Per-subject mode via `UBlueprint::OnCompiled()`
> retains the full payload.


Three new adapters; all three trigger sources are non-dynamic
multicasts, so **no listener UClasses needed** — adapters can bind
lambdas directly. Wiring effort: one `RegisterAdapter` line in
`UBridgeReactiveSubsystem::Initialize` per adapter; one new
`RegisterEditor*` UFUNCTION per adapter on
`UnrealBridgeReactiveLibrary`. `EBridgeTrigger::AssetEvent | PieEvent
| BpCompiled` enum slots already reserved in
`UnrealBridgeReactiveTypes.h`. Build deps already cover everything
needed (`AssetRegistry`, `UnrealEd`, `Kismet`).

### A1. `BridgeReactiveAdapter_AssetEvent`

- **File:** `Plugin/UnrealBridge/Source/UnrealBridge/Private/BridgeReactiveAdapter_AssetEvent.cpp`
- **Delegates bound (one-shot, refcounted by handler count):**
  - `IAssetRegistry::OnAssetAdded()` → dispatch with `Selector="Added"`
  - `IAssetRegistry::OnAssetRemoved()` → `Selector="Removed"`
  - `IAssetRegistry::OnAssetRenamed()` → `Selector="Renamed"`
  - `IAssetRegistry::OnAssetUpdated()` → `Selector="Updated"`
- **Filtering:** `Selector=NAME_None` matches any event (existing
  `DispatchLocked` logic at `UnrealBridgeReactiveSubsystem.cpp:556`
  treats none-selector handlers as "match all"). No special code.
- **Subject:** always null (global). Per-handler class filter happens
  inside the script via `ctx['asset_class']`.
- **ctx keys:** `trigger='asset_event'`, `event` (Added/Removed/Renamed/
  Updated), `asset_path` (str), `asset_class` (str), `package_name`
  (str), `old_path` (str — empty unless `event=='Renamed'`).
- **Library entry point:**
  `RegisterEditorAssetEvent(task_name, description, event_filter,
  script, script_path, tags, lifetime, error_policy, throttle_ms)`.
  `event_filter=""` → Selector=NAME_None (match all). Otherwise must be
  one of the four event names; reject with warning otherwise.
- **Refcounting model:** copy `FBridgeAnimNotifyAdapter` shape — keep
  one `int32 HandlerCount`; on first add, bind all four delegates +
  store `FDelegateHandle`s; on last remove, unbind. Skip the
  per-Subject `FBinding` array (no per-subject grouping needed).
- **Initial-scan gating:** AssetRegistry fires `OnAssetAdded` for every
  asset during its discovery scan at editor startup. Agents register
  long after that, so v1 doesn't gate. If startup-time registration
  becomes a use case, gate with
  `IAssetRegistry::IsLoadingAssets()`.

### A2. `BridgeReactiveAdapter_PieState`

- **File:** `Plugin/UnrealBridge/Source/UnrealBridge/Private/BridgeReactiveAdapter_PieState.cpp`
- **Delegates bound (refcount as above):**
  - `FEditorDelegates::PreBeginPIE` → `Selector="PreBeginPIE"`
  - `FEditorDelegates::BeginPIE` → `Selector="BeginPIE"`
  - `FEditorDelegates::PostPIEStarted` → `Selector="PostPIEStarted"`
  - `FEditorDelegates::PrePIEEnded` → `Selector="PrePIEEnded"`
  - `FEditorDelegates::EndPIE` → `Selector="EndPIE"`
  - `FEditorDelegates::PausePIE` → `Selector="PausePIE"`
  - `FEditorDelegates::ResumePIE` → `Selector="ResumePIE"`
  - `FEditorDelegates::SingleStepPIE` → `Selector="SingleStepPIE"`
- **Subject:** null. **ctx keys:** `trigger='pie_event'`, `phase` (str),
  `is_simulating` (bool — from delegate payload, where applicable).
- **Coordination with existing `HandlePieEnded`.** The subsystem
  already binds `FEditorDelegates::EndPIE` to purge `WhilePIE`
  handlers (`UnrealBridgeReactiveSubsystem.cpp:173`). The adapter's
  EndPIE handler must run **after** the purge so handlers can
  observe their own removal. Subscribe later via `AddLambda` —
  delegate fire order matches subscription order in UE — and don't
  rely on it; instead, document that `EndPIE` handlers should not
  expect any other PIE-scoped handler to still exist.
- **Library entry point:** `RegisterEditorPieEvent(task_name,
  description, phase_filter, script, ...)`. `phase_filter=""` → Any.

### A3. `BridgeReactiveAdapter_BpCompiled`

- **File:** `Plugin/UnrealBridge/Source/UnrealBridge/Private/BridgeReactiveAdapter_BpCompiled.cpp`
- **Two binding modes:**
  1. **Per-blueprint (Subject = UBlueprint).** Bind to `UBlueprint::
     OnCompiled()` (a `FOnBlueprintCompiledMulticast` —
     non-dynamic). Maintain `TMap<TWeakObjectPtr<UBlueprint>,
     FBindingState>` keyed by Subject for per-BP refcount.
  2. **Global (Subject = null).** Bind once to
     `GEditor->OnBlueprintCompiled()` (broadcast for any compile).
     Use the same one-binding-refcount pattern as AssetEvent for the
     global path.
- **Subject = UBlueprint** (per-BP) or null (global). **Selector** =
  always NAME_None (no sub-types).
- **ctx keys:** `trigger='bp_compiled'`, `blueprint_path` (str),
  `parent_class` (str), `bytecode_only` (bool — pull from compile
  context if reachable; otherwise omit and document gap).
- **Library entry point:** `RegisterEditorBpCompiled(task_name,
  description, blueprint_path_filter, script, ...)`. Empty path =
  global, otherwise resolve via `LoadObject<UBlueprint>` and store
  Subject. Reject with warning if path doesn't load.

### Wiring (A4)

- `UnrealBridgeReactiveSubsystem.cpp:15-23`: add three forward decls
  (`MakeAssetEventAdapter` / `MakePieStateAdapter` /
  `MakeBpCompiledAdapter`).
- `UnrealBridgeReactiveSubsystem.cpp:197-203`: add three
  `RegisterAdapter` calls.
- `UnrealBridgeReactiveLibrary.h/cpp`: add three `RegisterEditor*`
  UFUNCTIONs, mirroring the runtime-entry-point shape (TaskName /
  Description / filter / Script / ScriptPath / Tags / Lifetime /
  ErrorPolicy / ThrottleMs). Set `Record.Scope = "editor"` so
  `IssueHandlerId` produces `ed_<seq>_<rand4>` ids.

### Verify (A5)

Each end-to-end smoke test is a single bridge `exec-file`:

- **AssetEvent:** register handler with `event_filter='Renamed'`;
  duplicate a temp Material to a new path via
  `unreal.EditorAssetLibrary.duplicate_asset`; rename via
  `rename_asset`; assert handler `call_count == 1` with correct
  `old_path` / new path in stats `LastError` field is empty.
- **PieEvent:** register with `phase_filter='BeginPIE'`; call
  `unreal.UnrealBridgeEditorLibrary.start_pie` then `stop_pie`;
  assert one fire (BeginPIE only).
- **BpCompiled:** register with empty `blueprint_path_filter` (global);
  load a BP via `unreal.EditorAssetLibrary.load_blueprint_class`;
  call `unreal.UnrealBridgeEditorLibrary.compile_blueprints`; assert
  fire with correct `blueprint_path`.

Cleanup between runs via `clear_all('editor')`.

### Docs (A6)

- `bridge-reactive.md`: add three new sections (one per adapter)
  with a worked register example. Update the table of contents
  / overview block.
- `reactive-handlers.md`: change P5 row to ✅ done, append
  implementation notes if anything diverged.

---

## P6 implementation strategy (drafted 2026-04-17)

Three independent sub-features. **B1 is cheap** (single function
update); **B2 is medium** (new dispatch path + ASC discovery hook);
**B3 is the big one** (schema design + persistence + subject re-resolution).
Land in B1 → B2 → B3 order so each is independently verifiable.

### B1. Tag wildcard matching

- **Surface:** `list_all_handlers(filter_tag="combat.*")` returns
  handlers whose tags match the wildcard pattern.
- **Implementation:** in `UBridgeReactiveSubsystem::ListAllHandlers`
  (`UnrealBridgeReactiveSubsystem.cpp:404` area), replace exact
  `Tags.Contains(FilterTag)` with `Tags.ContainsByPredicate([&](const
  FString& T){ return T.MatchesWildcard(FilterTag); })`. Use UE's
  built-in `FString::MatchesWildcard` — handles `*` and `?` correctly,
  no regex dep.
- **Backwards compatible:** patterns without wildcards still match
  exactly via `MatchesWildcard`.
- **Verify:** register handlers with tags `["combat.hit"]`,
  `["combat.dodge"]`, `["movement"]`; `list_all_handlers(filter_tag=
  "combat.*")` returns the first two only.

### B2. Global GameplayEvent listener (no-subject)

- **Surface:** `register_runtime_gameplay_event(target_actor_name=
  "", event_tag="Event.Combat.Hit", ...)` — empty target means
  "fire whenever any live ASC receives this tag".
- **Discovery model:** at register time, snapshot all live ASCs in
  the PIE world via `TActorIterator<AActor>` + `ResolveActorASC`,
  bind to each. Subscribe to `UWorld::OnActorSpawned` to catch
  ASCs created mid-PIE, and to `UWorld::OnActorDestroyed` to drop
  bindings cleanly. PIE start/stop already handled by existing
  WorldCleanup wiring.
- **Adapter changes:** `BridgeReactiveAdapter_GameplayEvent`
  needs a "global handler" bucket alongside the existing
  per-Subject map. Single bound delegate per ASC; on fire,
  dispatch to both per-subject handlers (existing path) AND
  global handlers whose tag matches.
- **Subject storage in record:** keep `Subject = nullptr`,
  `Selector = tag`, and add `AdapterPayload = "global"` as a
  marker so the adapter can route correctly on `OnHandlerAdded`.
  Alternative: a new `bIsGlobal` field on the record, but
  `AdapterPayload` keeps the schema flat.
- **Edge cases to design out:**
  - PIE not running at register time → defer ASC discovery until
    `FEditorDelegates::PostPIEStarted`. Park record in pending list.
  - Late-spawned actors with ASC on PlayerState — `OnActorSpawned`
    fires before PlayerState assigns; need a one-tick delayed
    re-resolve.
  - Editor world ASCs (asset preview) — exclude unless
    `WorldType == EWorldType::PIE`.
- **Verify:** spawn a fresh ASC mid-PIE, send GameplayEvent on it,
  assert global handler fires; destroy actor, assert binding
  dropped (no zombie fires next time tag is sent).

### B3. JSON persistence

- **What persists:** all handler records — task_name, description,
  tags, script (inline or path), lifetime, error_policy,
  throttle_ms, trigger_type, subject path string (if any),
  selector, adapter_payload. Stats deliberately NOT persisted
  (counts reset on reload — load is "first run after restart").
- **What does NOT persist by default:**
  - `WhilePIE` lifetime (purpose-bound to one session).
  - Handlers with `bPersist=false` flag (new optional field on
    every register entry point, default `true`).
- **File layout:** `<ProjectDir>/Saved/UnrealBridge/
  reactive-handlers.json`. One file, top-level array of records.
  Schema versioned via top-level `{ "version": 1, "handlers":
  [...] }` so future migrations have somewhere to hook.
- **Save triggers:** on `RegisterHandler` success, `UnregisterHandler`,
  `PauseHandler`, `ResumeHandler`, `ClearHandlers`. Debounce with a
  100 ms `FTSTicker` coalescer to avoid hammering disk during
  burst registers (e.g. agent re-creating its 20 handlers on
  reconnect).
- **Load:** in `UBridgeReactiveSubsystem::Initialize`, after
  adapters are registered, parse JSON and replay each record via
  the same code path as runtime register, **except**:
  - Re-issue HandlerId? Or preserve? **Preserve** — agents stash
    HandlerIds across sessions. Add an `internal` overload of
    `RegisterHandler` that accepts a pre-existing id and skips
    `IssueHandlerId`.
  - On subject re-resolve failure, push to a `DeferredHandlers`
    list that retries on `PostPIEStarted` (since most subjects
    are PIE-tied actors).
- **Subject re-resolution:**
  - GameplayEvent subject = ASC. Stored path is the ASC's path,
    not the actor's. On reload: `LoadObject<UAbilitySystemComponent>`,
    fall back to `FindActorByName(<actor-path-derived>)` →
    `ResolveActorASC`.
  - AnimNotify subject = AnimInstance. Same path-load + fallback.
  - PerActor adapters (ActorLifecycle, MovementMode, InputAction):
    `LoadObject<AActor>`.
  - Per-BP BpCompiled subject: `LoadObject<UBlueprint>` (editor
    world, no PIE issues).
- **Failure modes & UX:**
  - Subject not found after one PIE start cycle → log warning,
    leave in `DeferredHandlers`. `list_all_handlers()` should
    surface deferred records with `subject_path = "<unresolved>"`
    so agents see them.
  - Schema version mismatch → log + skip the file (don't drop user
    data).
  - Corrupt JSON → rename to `.json.bak.<timestamp>`, start fresh.
- **Library surface additions:**
  - `RegisterEditor*` / `RegisterRuntime*` gain optional
    `bool bPersist = true` parameter (default true; agents pass
    false for ephemeral throwaway handlers).
  - New `SaveAllHandlers()` and `LoadAllHandlers()` for explicit
    control (e.g. forced reload after editing the JSON externally).
- **Verify:** register 5 handlers across multiple trigger types,
  restart editor, check `list_all_handlers` returns the same 5
  with the same HandlerIds; fire each event source and confirm
  they still respond.

### Open question (defer until building)

- Should persistence be per-project (`Saved/UnrealBridge/`) or
  per-user (`%APPDATA%/UnrealBridge/`)? Per-project is more useful
  (handlers tied to project content), per-user is friendlier when
  multiple projects share an agent setup. Default to per-project
  with override env var.

---

## Tradeoffs / gotchas to revisit during implementation

- **Heavy handler scripts hitch the frame.** No way around this in
  v1 — the contract is "handler decides, defers heavy work via
  `defer_to_next_tick`". Stats surface offenders.
- **Context with non-trivial UObjects.** Pass paths and let the
  preamble call `unreal.load_object`; avoid C++↔Python wrapper
  marshalling. Cost: one `LoadObject` per handler fire — fine for
  combat-frequency events; reconsider if AttributeChanged on a
  fast-regen attribute becomes a hotspot.
- **Per-frame events (Tick).** Not in v1 surface. If needed later,
  give it a separate adapter with mandatory throttle.
- **Editor reactive surface really is small.** Confirmed user
  intuition. Worth building because the same registry handles it for
  free, but only 2–3 trigger types ship in P5.

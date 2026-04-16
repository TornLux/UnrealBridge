# Reactive Python handlers — unified event-to-script framework

Status: **P1, P2, P3, most of P4 landed** (as of 2026-04-16). Seven
adapters total: GameplayEvent, AttributeChanged, ActorLifecycle,
MovementModeChanged, AnimNotify, InputAction, **Timer** (P4 — runtime-
verified 9 fires in 3s at ~3Hz editor tick when unfocused). All edge
paths verified: WorldCleanup on PIE stop, re-entrancy depth guard,
LogUnregister, throttle, WhilePIE, ASC walker. Sticky-input's FTSTicker
migrated to `UBridgeReactiveSubsystem::RegisterPersistentTicker` —
unified lifecycle, same API for callers. `bridge-reactive.md` reference
doc shipped with Timer section. Still build-only: AnimNotify + InputAction
adapters (fixture-blocked), WhileSubjectAlive lifetime. P5 editor
adapters + P6 persistence remain.

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
| **P4** | pending | `defer_to_next_tick` stress test, `WhilePIE`/`WhileSubjectAlive` runtime verification, re-entrancy depth guard verification, write `bridge-reactive.md` with 2 worked examples, link from existing API docs. | Agent can read `bridge-reactive.md` cold and register a working handler in one exec. |
| **P5** | pending | 2–3 editor adapters (asset event, PIE state, BP compiled). Editor-scope `register_editor_*` entry points. | Editor-only: rename an asset, watch a registered handler fire. |
| P6 (optional) | pending | Persist registrations to JSON for cross-session restore; tag wildcard matching; global (no-subject) GameplayEvent listener that walks live ASCs. | — |

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
- **WhileSubjectAlive** — still build-only. The check runs only during Dispatch — a dead subject is removed the next time the adapter tries to fire on it. If no event fires after the subject dies, the handler lingers until PIE ends / editor closes.
- **WorldCleanup removing PIE-tied handlers on PIE stop.** P1 logic
  exists; not yet verified with a stop-PIE → inspect-registry test.
- **Re-entrancy depth guard (warn at 8, abort at 16).** Not exercised.
  Would need a pair of handlers that each fire a GameplayEvent to the
  other.
- **LogUnregister error policy.** Not exercised. Needs a handler that
  raises an exception.
- **Throttle.** Not exercised. Needs a high-frequency event.

## Open questions resolved

- ✅ Unified framework, not per-event one-offs.
- ✅ Lifetime split: separate `register_runtime_*` and `register_editor_*` entry points; editor scope is small/lower-priority.
- ✅ HandlerId system-issued; agent stores it. Re-registration of same id rejected — agent must `unregister` first.
- ✅ Required metadata: `task_name` + `description` (mandatory), `script_path` + `tags` (optional). Surfaced by `list_all_handlers` so a fresh agent can introspect what's running and why.
- ✅ Sticky-input ticker not migrated for now.
- ✅ Cross-handler shared state via `bridge_state`; per-handler private state via `state`. Both injected to preamble.

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

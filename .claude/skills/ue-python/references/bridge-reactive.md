# Reactive handlers API

`unreal.UnrealBridgeReactiveLibrary` — register Python scripts that fire when UE events happen (GameplayEvent, AnimNotify, MovementMode change, attribute threshold, actor destroyed, …). The bridge request/response exec model stays unchanged; reactive scripts live in C++ and run on the GameThread the moment the underlying delegate fires.

Design doc: `docs/plans/reactive-handlers.md`. For the plan's current status (which adapters are runtime-verified vs build-only), read the status table there.

---

## When to use this instead of an exec loop

Use reactive handlers when:
- You want logic to run **in response to an in-game event** (a tag fires, an actor gets destroyed, the player lands) without the agent having to poll.
- You want **persistent behaviour** that outlives a single bridge call.
- You want **multiple scripts to coordinate** via shared state (combos, interrupts).

Do NOT use this for:
- One-off introspection ("what's the player's HP right now") — use the relevant library function.
- Heavy per-frame work. Handlers run inside UE's event dispatch; a slow script hitches the frame. Defer heavy work with `defer_to_next_tick`.

---

## Core concepts

### HandlerId (system-issued)

`register_runtime_*` returns a string like `rt_0042_b3a1`. Agents MUST store this to later pause, resume, unregister, or inspect. Same id cannot be re-registered — call `unregister(id)` first.

| Prefix | Scope |
|--------|-------|
| `rt_`  | runtime — fires during PIE |
| `ed_`  | editor — fires on editor events (P5, not yet implemented) |

### Mandatory metadata

Every registration MUST supply `task_name` (short label) and `description` (why this handler exists). Both show up in `list_all_handlers()` so a fresh agent can understand what's running without reading source.

Optional: `script_path` (points to a file on disk), `tags` (free-form filter labels).

### The preamble — what's available inside handler scripts

Before your script runs, the bridge injects a preamble that provides:

```python
# Identity
handler_id          # str — this handler's id
handler_task_name   # str — the task_name passed at register

# Event context (keys depend on trigger type — see adapter sections below)
ctx                 # dict[str, any]
event_tag           # convenience alias when ctx has 'tag' (GameplayEvent only)
event_instigator    # alias for ctx['event_instigator'] (GameplayEvent only)
# ... etc — any key in ctx is also hoisted to a top-level name for terser scripts

# State dicts — survive across handler invocations, wiped on editor restart
bridge_state        # dict shared across ALL handlers — use for cross-handler coordination
state               # dict private to THIS handler (keyed by handler_id internally)

# Helpers
log(msg)            # -> '[reactive:rt_0042_b3a1|task_name] <msg>' in LogUnrealBridge
defer_to_next_tick(script_str)  # queue a Python snippet for next GameThread tick
```

### Lifetime

| Value | Behaviour |
|-------|-----------|
| `"Permanent"` (default) | Never auto-deregister |
| `"Once"` | Fire once, auto-deregister |
| `"Count:N"` | Fire N times (e.g. `"Count:5"`), auto-deregister |
| `"WhilePIE"` | Auto-deregister on PIE stop |
| `"WhileSubjectAlive"` | Auto-deregister when Subject becomes GC'd |

### Error policy

| Value | Behaviour |
|-------|-----------|
| `"LogContinue"` (default) | Script exception → log + keep handler active |
| `"LogUnregister"` | Script exception → log + auto-deregister (debug aid) |
| `"Throw"` | Script exception → log at Error severity + keep active |

---

## Trigger reference

### register_runtime_gameplay_event

Fire when `TargetActor`'s ASC receives a `GameplayEvent` matching `event_tag`. Binds `UAbilitySystemComponent::GenericGameplayEventCallbacks[Tag]`.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_gameplay_event(
    task_name="hit_reaction",
    description="Player takes Event.Combat.Hit → screen shake + flinch montage",
    target_actor_name="BP_UnitCharacterBase_C_0",
    event_tag="Event.Combat.Hit",
    script="""
        bridge_state.setdefault('hit_count', 0)
        bridge_state['hit_count'] += 1
        log('took a hit, total=' + str(bridge_state['hit_count']))
    """,
    script_path="",
    tags=["combat"],
    lifetime="Permanent",
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('gameplay_event'), `tag`, `source_asc` (the ASC that received the event — primary signal for global handlers to identify which ASC fired), `event_instigator`, `event_target`, `event_optional_object`, `event_optional_object2`, `event_magnitude`, `event_instigator_tags`, `event_target_tags`.

> **ASC resolution.** The target actor doesn't need a direct ASC — the bridge walks `Actor → Pawn.PlayerState → Pawn.Controller → PC.PlayerState`. Fires `SendGameplayEventByName` through the same walker.

#### Global mode — pass `target_actor_name=""`

Empty `target_actor_name` registers a **global** handler that fires whenever
*any* live ASC in the PIE world receives `event_tag`. Use for cross-actor
listeners (combat broadcaster, ML reward signal, debug HUD) where you don't
want to per-bind every ASC by name.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_gameplay_event(
    task_name="hit_logger",
    description="Log every Event.Combat.Hit on any ASC in PIE",
    target_actor_name="",                    # ← empty = global
    event_tag="Event.Combat.Hit",
    script="""
        owner = source_asc.get_owner() if source_asc else None
        log('hit on asc=' + str(source_asc) + ' owner=' + str(owner))
    """,
    script_path="", tags=["combat", "telemetry"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)
```

How it works internally:

- At register, the adapter walks live PIE-world actors and binds the ASC
  delegate on every one with an ASC. A subsequent per-subject handler on
  the same `(ASC, tag)` shares that binding (refcounted).
- New actors that spawn after register are caught via
  `UWorld::OnActorSpawned`. If the actor doesn't have an ASC at spawn time
  (e.g. PlayerState ASCs assigned one tick later), a one-tick deferred
  re-resolve catches it.
- On `EndPIE`, dead bindings are swept; the global record persists and
  re-snapshots ASCs on the next `PostPIEStarted` so the same registration
  survives PIE restarts.

`subject_path` reads as `""` for global handlers in `list_all_handlers`
output — distinguishes them from `<invalid>` (subject was a real object
that has since been GC'd).

**Caveats:**

- ASCs that you attach to actors *after* registration via Python (e.g.
  `EnsureAbilitySystemComponent` called post-register) are not auto-bound
  unless the actor was just spawned. The spawn-watcher only triggers on
  the original `OnActorSpawned`. If you bolt ASCs onto pre-existing
  actors mid-session, unregister and re-register the global handler to
  re-snapshot.
- Each fire dispatches twice internally — once with `Subject=ASC` (per-
  subject handlers match), once with `Subject=null` (globals match). No
  double-firing because the matcher uses pointer equality.

### register_runtime_attribute_changed

Fire when an `FGameplayAttributeData` field on the target's ASC changes value. Binds `UAbilitySystemComponent::GetGameplayAttributeValueChangeDelegate(Attribute)` (non-dynamic, cheap).

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_attribute_changed(
    task_name="low_health_warning",
    description="Play warning sound when Health crosses below 25",
    target_actor_name="BP_UnitCharacterBase_C_0",
    attribute_name="Health",   # or "BridgeTestAttributeSet.Health" for disambiguation
    script="""
        if ctx['old_value'] >= 25 and ctx['new_value'] < 25:
            log('CRITICAL: health dropped under 25')
            bridge_state['warn_played'] = True
    """,
    script_path="",
    tags=["combat", "feedback"],
    lifetime="Permanent",
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('attribute_changed'), `attribute_name`, `new_value`, `old_value`, `delta`.

### register_runtime_actor_lifecycle

Fire on `AActor::OnDestroyed` or `AActor::OnEndPlay`. One registration per event type; pass `event_type` = `"Destroyed"` or `"EndPlay"`.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_actor_lifecycle(
    task_name="enemy_killed",
    description="Grant XP when Enemy_42 dies",
    target_actor_name="BP_Enemy_C_3",
    event_type="Destroyed",
    script="bridge_state.setdefault('kills', 0); bridge_state['kills'] += 1",
    script_path="", tags=[],
    lifetime="Once",   # enemy dies once
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('actor_lifecycle'), `event` ('Destroyed' or 'EndPlay'), `actor`, `end_play_reason` (empty string for Destroyed; otherwise 'Destroyed' / 'LevelTransition' / 'EndPlayInEditor' / 'RemovedFromWorld' / 'Quit').

### register_runtime_movement_mode_changed

Fire when `TargetActor` (must be an `ACharacter`) transitions between `EMovementMode` values. Handler script filters on specific transitions via `ctx`.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_movement_mode_changed(
    task_name="landed_detector",
    description="Play dust VFX when pawn transitions Falling → Walking",
    target_actor_name="BP_UnitCharacterBase_C_0",
    script="""
        if ctx['prev_mode_name'] == 'Falling' and ctx['current_mode_name'] == 'Walking':
            log('landed!')
            bridge_state['last_landing_time'] = unreal.SystemLibrary.get_game_time_in_seconds(ctx['character'])
    """,
    script_path="", tags=["movement"],
    lifetime="Permanent",
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('movement_mode_changed'), `character`, `prev_mode` (int), `prev_mode_name` ('Walking' | 'NavWalking' | 'Falling' | 'Swimming' | 'Flying' | 'Custom' | 'None'), `prev_custom_mode` (int), `current_mode`, `current_mode_name`, `current_custom_mode`.

### register_runtime_anim_notify

Fire when an AnimNotify named `notify_name` plays on target's skeletal mesh. Binds `UAnimInstance::OnPlayMontageNotifyBegin`.

> **Note:** runtime-untested at the time of writing — the project's current DefaultMap lacks an AnimBP + montage fixture. Build-verified; API shape is stable. See `docs/plans/reactive-handlers.md` "Untested paths".

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_anim_notify(
    task_name="attack_hit_frame",
    description="Apply damage on Attack_Hit notify during combo montage",
    target_actor_name="BP_UnitCharacterBase_C_0",
    notify_name="Attack_Hit",
    script="log('notify fired on ' + str(ctx['montage']))",
    script_path="", tags=["combat"],
    lifetime="Permanent",
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('anim_notify'), `notify_name`, `anim_instance`, `mesh_component`, `owner_actor`, `montage` (UAnimMontage or None), `source_asset` (raw SequenceAsset from payload).

### register_runtime_timer

Fire a Python script periodically at a configured interval. No UE event source — the Timer adapter drives itself via an FTSTicker registered with the reactive subsystem. Use for self-polling loops, periodic state sync, agent idle behaviour, or "every N seconds do X" patterns.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_timer(
    task_name="hp_regen",
    description="Every 0.5s: add 1 HP if below max",
    interval_seconds=0.5,
    script="""
        import unreal as u
        info = u.UnrealBridgeGameplayAbilityLibrary.get_actor_attributes('BP_UnitCharacterBase_C_0')
        # ... logic that reads/writes attributes ...
        log('tick ' + str(ctx['fire_count']) + ' world_time=' + str(round(ctx['world_time'], 2)))
    """,
    script_path="", tags=["regen"],
    lifetime="Permanent",     # WhilePIE is also common for in-game timers
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('timer'), `interval_seconds` (the configured interval), `elapsed_since_last` (wall-clock seconds since previous fire), `fire_count` (int, starts at 1), `world_time` (PIE world `GetTimeSeconds()`, 0 if no PIE), `editor_time` (`FPlatformTime::Seconds()`).

**Editor tick rate caveat.** The timer's real resolution is bound by the editor's FTSTicker tick rate. When the editor window is **focused** during PIE, this is ~60 Hz, so intervals as low as ~16ms are observable. When the editor is **unfocused**, UE throttles to ~3 Hz — a requested `interval_seconds=0.2` will fire roughly every 0.33s. This matters if you're stress-testing timers while the client window has focus. For in-game reactive logic, the focused-editor rate applies and the timer is as precise as one frame.

**Subject:** Timer handlers have no Subject, so the WorldCleanup purge doesn't apply. Use `Lifetime="WhilePIE"` when you want the timer removed on PIE stop; otherwise it persists until explicitly unregistered or the editor closes.

### register_runtime_input_action

Fire on EnhancedInput action triggers. Binds via `UEnhancedInputComponent::BindAction` on the target actor's (or its controller's) input component.

> **Note:** runtime-untested at the time of writing — the project's DefaultMap doesn't expose a convenient `UInputAction` asset for testing. Build-verified; API shape is stable.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_input_action(
    task_name="jump_log",
    description="Log every time the player presses Jump",
    target_actor_name="BP_UnitCharacterBase_C_0",
    input_action_path="/Game/Input/IA_Jump",
    trigger_event="Triggered",   # Started|Ongoing|Triggered|Completed|Canceled
    script="log('jump ' + ctx['trigger_event'] + ' value=' + str(ctx['value_bool']))",
    script_path="", tags=["input"],
    lifetime="Permanent",
    error_policy="LogContinue",
    throttle_ms=0,
)
```

**ctx keys:** `trigger` ('input_action'), `action_path`, `action_name`, `trigger_event`, `value_type` ('Boolean' | 'Axis1D' | 'Axis2D' | 'Axis3D'), `value_bool`, `value_axis1d`, `value_axis2d_x`, `value_axis2d_y`, `value_axis3d_x`, `value_axis3d_y`, `value_axis3d_z`, `elapsed_seconds`.

---

## Editor-domain triggers (P5)

These fire on editor-only events — asset changes, PIE state transitions, Blueprint compiles — and return handler ids prefixed `ed_<seq>_<rand4>`. Use `register_editor_*` (not `register_runtime_*`). Filtering works the same way the runtime surface does.

### register_editor_asset_event

Fire when the AssetRegistry reports a lifecycle event.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_editor_asset_event(
    task_name="material_rename_tracker",
    description="Log every material rename so downstream references can be updated",
    event_filter="Renamed",            # "", "Added", "Removed", "Renamed", "Updated"
    script="""
        if ctx['asset_class'] != 'Material':
            return
        log(f"material {ctx['old_path']} → {ctx['asset_path']}")
        bridge_state.setdefault('renames', []).append(
            (ctx['old_path'], ctx['asset_path']))
    """,
    script_path="", tags=["pipeline"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)
```

**ctx keys:** `trigger` ('asset_event'), `event` ('Added' | 'Removed' | 'Renamed' | 'Updated'), `asset_path` (object path like `/Game/Foo/Bar.Bar`), `asset_class` (short class name), `package_name`, `old_path` (empty unless `event=='Renamed'`).

**Subject is always null** (global). Per-asset filtering is done inside the script via `ctx['asset_class']` / `ctx['package_name']`.

**Startup scan caveat:** during editor boot AssetRegistry fires `OnAssetAdded` for every asset in the project. Agent sessions start long after this scan completes, so v1 doesn't gate. If you register during startup, expect a flood of Added events and either ignore them or gate on `unreal.AssetRegistryHelpers.get_asset_registry().is_loading_assets() == False`.

### register_editor_pie_event

Fire on Play-in-Editor phase transitions.

```python
hid = unreal.UnrealBridgeReactiveLibrary.register_editor_pie_event(
    task_name="pie_begin_probe",
    description="Spawn debug observers every time PIE starts",
    phase_filter="BeginPIE",           # "" for any phase
    script="""
        log(f"PIE begin (simulating={ctx['is_simulating']}) — spawn observers")
        # ... spawn your observers here ...
    """,
    script_path="", tags=["debug"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)
```

**Phases** (use one of these in `phase_filter` or pass `""` for all): `PreBeginPIE` → `BeginPIE` → `PostPIEStarted` → … → `PrePIEEnded` → `EndPIE`. `PausePIE` / `ResumePIE` / `SingleStepPIE` also fire when applicable.

**ctx keys:** `trigger` ('pie_event'), `phase` (one of the names above), `is_simulating` (bool — True for Simulate-in-Editor, False for normal Play).

**Subject is always null.** Selector is the phase name — the adapter filters per handler automatically.

⚠️ **Ordering gotcha with WhilePIE lifetime.** The subsystem's core EndPIE hook purges `WhilePIE` handlers BEFORE this adapter fires its `EndPIE` dispatch. So a handler registered with `phase_filter="EndPIE"` + `lifetime="WhilePIE"` gets removed before its own event fires. If you need to observe EndPIE, use `lifetime="Permanent"` — or register for `PrePIEEnded`, which fires before the purge.

### register_editor_bp_compiled

Fire when a Blueprint is compiled.

```python
# Mode A: per-blueprint — binds to a specific UBlueprint's OnCompiled()
hid = unreal.UnrealBridgeReactiveLibrary.register_editor_bp_compiled(
    task_name="enemy_bp_recompile",
    description="Re-validate enemy BP references after every compile",
    blueprint_path_filter="/Game/AI/BP_Enemy",
    script="""
        log(f"compiled {ctx['blueprint_path']} (parent={ctx['parent_class']})")
        # revalidate downstream data-assets that reference this BP
    """,
    script_path="", tags=["pipeline"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)

# Mode B: global — hooks GEditor->OnBlueprintCompiled() for any BP compile
hid_any = unreal.UnrealBridgeReactiveLibrary.register_editor_bp_compiled(
    task_name="bp_compile_counter",
    description="Count total compiles across the session",
    blueprint_path_filter="",          # empty → global
    script="bridge_state['n_compiles'] = bridge_state.get('n_compiles', 0) + 1",
    script_path="", tags=["telemetry"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)
```

**ctx keys:** `trigger` ('bp_compiled'), `blueprint_path` (object path like `/Game/Foo/BP_Bar.BP_Bar`; **empty** for global-mode fires), `parent_class` (short class name; **empty** for global-mode fires).

**Mode difference.** Per-subject mode binds `UBlueprint::OnCompiled()`, which carries the compiled `UBlueprint*` — so ctx gets the full path + parent class. Global mode binds `GEditor->OnBlueprintCompiled()`, which is a no-param broadcast — ctx has `blueprint_path=''` / `parent_class=''`. If you need per-BP payload for multiple BPs, register one per-subject handler per target rather than one global one.

**Selector is always NAME_None** — no sub-events. Agents scope by Subject (path filter) or in-script checks.

---

## Management

```python
# List all handlers with metadata. Optional filters.
summaries = unreal.UnrealBridgeReactiveLibrary.list_all_handlers(
    filter_scope="",            # "", "runtime", "editor"
    filter_trigger_type="",     # "", "GameplayEvent", "AttributeChanged", ...
    filter_tag="")              # "", exact tag, or wildcard pattern

# filter_tag wildcard support:
#   "combat.hit"  → exact match (any handler with that exact tag)
#   "combat.*"    → prefix wildcard (combat.hit, combat.dodge, …)
#   "*.alert"     → suffix wildcard
#   "combat.???"  → '?' matches a single char
# Plain text without '*' or '?' uses fast exact-equality. Wildcards use
# UE's FString::MatchesWildcard.

for s in summaries:
    print(f"{s.handler_id}  {s.task_name:32}  {s.trigger_summary}  paused={s.paused}  calls={s.stats.calls}")

# Full record with script source.
detail = unreal.UnrealBridgeReactiveLibrary.get_handler(handler_id)
print(detail.script)

# Runtime stats.
stats = unreal.UnrealBridgeReactiveLibrary.get_handler_stats(handler_id)
print(stats.calls, stats.total_microseconds, stats.max_microseconds, stats.error_count, stats.last_error)

# Pause / resume without unregistering.
unreal.UnrealBridgeReactiveLibrary.pause(handler_id)
unreal.UnrealBridgeReactiveLibrary.resume(handler_id)

# Unregister a single handler.
unreal.UnrealBridgeReactiveLibrary.unregister(handler_id)

# Nuke everything.
n = unreal.UnrealBridgeReactiveLibrary.clear_all("runtime")   # or "editor" or "all"

# Describe what ctx keys a given trigger injects.
ctx_spec = unreal.UnrealBridgeReactiveLibrary.describe_trigger_context("MovementModeChanged")
for k, doc in ctx_spec.items():
    print(k, '—', doc)
```

---

## Worked example 1 — "landed dust VFX"

Goal: every time the player lands, register the landing time and log it. If two landings happen within 0.2s (edge walking / ledge grabs), only log the first.

```python
import unreal

pawn_name = unreal.UnrealBridgeGameplayLibrary.get_all_pawns()[0]

hid = unreal.UnrealBridgeReactiveLibrary.register_runtime_movement_mode_changed(
    task_name="landed_throttle",
    description="First landing transition wins within a 200ms window",
    target_actor_name=pawn_name,
    script="""
        import unreal as u
        if not (ctx['prev_mode_name'] == 'Falling' and ctx['current_mode_name'] == 'Walking'):
            return
        now = u.SystemLibrary.get_game_time_in_seconds(ctx['character'])
        last = bridge_state.get('last_land', -99.0)
        if now - last < 0.2:
            return
        bridge_state['last_land'] = now
        bridge_state.setdefault('landings', []).append(now)
        log('landed at t=' + ('%.2f' % now))
    """,
    script_path="", tags=["movement", "feedback"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)
print(f"registered {hid}")
```

After some PIE playtime:
```python
# Inspect what the handler collected.
import _bridge_reactive_state as bs
print(bs.shared.get('landings', []))
```

---

## Worked example 2 — "combo window via two coordinating handlers"

Goal: within 0.8s of an Event.Combat.Hit, if the player presses Jump, trigger a finisher ability. Two handlers share state via `bridge_state`.

```python
pawn_name = unreal.UnrealBridgeGameplayLibrary.get_all_pawns()[0]

# 1) Hit opens a combo window.
hid_hit = unreal.UnrealBridgeReactiveLibrary.register_runtime_gameplay_event(
    task_name="combo_window_open",
    description="Event.Combat.Hit opens combo_window until t+0.8s",
    target_actor_name=pawn_name,
    event_tag="Event.Combat.Hit",
    script="""
        import unreal as u
        bridge_state['combo_until'] = u.SystemLibrary.get_game_time_in_seconds(ctx['event_target']) + 0.8
        log('combo window opened')
    """,
    script_path="", tags=["combat"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)

# 2) Jump during the window triggers the finisher.
hid_jump = unreal.UnrealBridgeReactiveLibrary.register_runtime_input_action(
    task_name="combo_finisher",
    description="IA_Jump Triggered during combo_window → SendGameplayEvent finisher",
    target_actor_name=pawn_name,
    input_action_path="/Game/Input/IA_Jump",
    trigger_event="Triggered",
    script="""
        import unreal as u
        now = u.SystemLibrary.get_game_time_in_seconds(None)
        if now < bridge_state.get('combo_until', 0):
            u.UnrealBridgeGameplayAbilityLibrary.send_gameplay_event_by_name(
                handler_task_name_target := '""" + pawn_name + """',
                'Event.Combat.Finisher', 1.0)
            log('FINISHER triggered')
            bridge_state['combo_until'] = 0
    """,
    script_path="", tags=["combat"],
    lifetime="Permanent", error_policy="LogContinue", throttle_ms=0,
)
```

Note: the two handlers are independent registrations, but share `bridge_state['combo_until']` implicitly — that's what the shared-state design exists for.

---

## Persistence (P6.B3)

Handlers **survive editor restarts automatically.** The subsystem auto-saves the registry to `<ProjectSaved>/UnrealBridge/reactive-handlers.json` ~100 ms after any Register/Unregister/Clear, and auto-loads on editor startup. HandlerIds are preserved so agents can stash an id in their context and `unregister(id)` / `get_handler_stats(id)` after a restart.

**What persists:**
- Task name, description, tags, script source, lifetime, error policy, throttle, `RegistrationContext` (user-provided `target_actor_name` / `event_tag` / `interval_seconds` / etc.).
- Seq counters so new post-restart ids don't collide with old ones.

**What doesn't persist:**
- `WhilePIE` lifetime handlers (session-bound by definition).
- Stats (`calls`, `max_microseconds`, …) — reset on load.
- Pause state — restored handlers are unpaused.
- Live `Subject` weak ptr (it's ephemeral). Subject gets re-resolved from `RegistrationContext` at load.

**Deferred restoration.** Editor-world subjects (a `UBlueprint` path, Timer, global triggers) resolve immediately at startup. **PIE-world subjects** (`ACharacter`, `UAbilitySystemComponent`, `UAnimInstance`, the player pawn, etc.) can't resolve until PIE starts — those records go into `DeferredHandlers` and retry on `FEditorDelegates::PostPIEStarted`. Check the queue size via `get_deferred_handler_count()`; inspect entries via `list_all_handlers()` (they surface with `subject_path = "<unresolved>"` until resolved).

**WorldCleanup behaviour.** When PIE stops, handlers whose Subject lived in the PIE world are moved to `DeferredHandlers` (not deleted), so they survive the session gap and re-bind on next `start_pie`. Only `WhilePIE`-lifetime handlers are fully deleted on PIE end (matching their declared semantics).

```python
# Explicit flush (e.g. right before a known-risky action):
unreal.UnrealBridgeReactiveLibrary.save_all_handlers()

# Force reload from disk (e.g. after hand-editing the JSON):
n = unreal.UnrealBridgeReactiveLibrary.load_all_handlers()

# Where does the JSON live?
path = unreal.UnrealBridgeReactiveLibrary.get_persistence_path()

# How many handlers are waiting for PIE to restore their subject?
pending = unreal.UnrealBridgeReactiveLibrary.get_deferred_handler_count()
```

**Schema version = 1.** Files with a different version are logged-and-skipped (don't destroy user data). Corrupt JSON is logged and skipped likewise — if that happens, delete the file manually to start fresh.

---

## Gotchas

- **Handler scripts run on the GameThread inside UE's event dispatch.** Slow scripts hitch the frame. For heavy work (asset loads, wide queries), call `defer_to_next_tick("<your script>")` and do it next tick.
- **`Once` / `Count:N` removal is deferred to after the current dispatch.** So `get_handler(id).summary.handler_id == ''` is the test that a Once handler already fired.
- **Re-registration with the same `handler_id` is not supported** (the system issues ids; don't try to reuse them). Agent-level uniqueness is enforced by the server.
- **`bridge_state` and `state` are Python dicts in the UE process** — the Python module survives bridge reconnects but NOT editor restart. If you need session-survivable data outside the handler registry, write it to a file yourself.
- **Handler depth guard.** If a handler fires a GameplayEvent that triggers another handler, nested execution is allowed up to depth 8 (warn) / 16 (abort). Recursive chains get logged as Warning / Error.

---

## Related APIs

- `unreal.UnrealBridgeGameplayAbilityLibrary.send_gameplay_event_by_name(actor_name, tag, magnitude)` — fire a GameplayEvent on an actor's ASC from Python. Uses the same walker as the reactive framework.
- `unreal.UnrealBridgeGameplayAbilityLibrary.ensure_ability_system_component(actor_name, location="Actor"|"Controller"|"PlayerState")` — test scaffolding: attach a fresh ASC to a bare actor.
- `unreal.UnrealBridgeGameplayAbilityLibrary.ensure_bridge_test_attribute_set(actor_name)` — test scaffolding: attach `UBridgeTestAttributeSet` (Health + Mana, each initialised to 100) to the actor's ASC.
- `unreal.UnrealBridgeGameplayAbilityLibrary.set_actor_attribute_value(actor_name, attr_name, value)` — write a numeric attribute base value, triggering the change delegate.

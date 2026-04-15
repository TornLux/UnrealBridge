# bridge-gameplay-api

`unreal.UnrealBridgeGameplayLibrary` — agent sensors + navigation for PIE automation.

MVP scope: **perception + path planning only**. Actuators (`apply_movement_input`, `press_action`, etc.) are a separate library addition planned after this path is validated.

Requires PIE to be running for `get_agent_observation`. `find_nav_path` falls back to the editor world when PIE is off — useful for precomputing routes.

---

## get_agent_observation(max_actor_distance, require_line_of_sight, class_filter) -> FAgentObservation or None

Assemble a full snapshot of the player pawn's perceptible world in one call.

**Parameters**
- `max_actor_distance` (float, default 3000.0 cm): filter radius; actors farther than this are dropped from `visible_actors`.
- `require_line_of_sight` (bool, default True): if True, line-trace camera→actor and drop occluded actors (visibility channel).
- `class_filter` (str, default ""): case-insensitive substring match on the actor's class name. Empty = no class filter.

**Returns** — `FAgentObservation` struct:
- `b_valid` (bool): False if PIE isn't running or there's no player pawn yet.
- `pawn_location` (Vector), `pawn_rotation` (Rotator), `pawn_velocity` (Vector)
- `b_on_ground` (bool): False when the character is falling (via UCharacterMovementComponent::IsFalling); True for non-Character pawns.
- `camera_location` (Vector), `camera_forward` (Vector)
- `visible_actors` (array of `FAgentVisibleActor`): sorted by distance ascending. Each entry carries `actor_name` (FName), `class_name` (str), `location` (Vector), `distance` (float cm), `tags` (array of FName).

**Cost**
- O(N) scan of all actors in the PIE world; one line trace per candidate when `require_line_of_sight=True`.
- Single GameThread call. Typical <1 ms for small scenes, a few ms for actor-heavy levels with LoS filtering.

**Output footprint** — small. Size scales linearly with actor count within the radius. For a 30 Hz agent loop this is ~30 struct copies/second, negligible on the wire.

**Example**
```python
import unreal
obs = unreal.UnrealBridgeGameplayLibrary.get_agent_observation(
    max_actor_distance=2500.0,
    require_line_of_sight=True,
    class_filter='Character',
)
if obs.b_valid:
    print(f'self@{obs.pawn_location} hp=ground={obs.b_on_ground}')
    for a in obs.visible_actors:
        print(f'  {a.class_name} {a.actor_name} dist={a.distance:.0f}')
```

**Pitfalls**
- Outside PIE returns `b_valid=False` (won't raise).
- `b_on_ground` is always True for non-ACharacter pawns — the `IsFalling()` check doesn't apply to raw APawn / APawn-derived vehicles.
- `class_name` is the UClass short name (e.g. `BP_PlayerCharacter_C`), not a path. Use a prefix like `class_filter='zombie'` to scope.

---

## find_nav_path(start_location, end_location) -> (success, waypoints, path_length)

Synchronously compute a navmesh path between two world-space points.

**Parameters**
- `start_location` (Vector): world-space source.
- `end_location` (Vector): world-space target.

**Returns** — tuple of:
- `success` (bool): True if a path was found.
- `out_waypoints` (array of Vector): `FNavPathPoint.Location` list, `waypoints[0]` = start, last entry = end-nearest navmesh point. Empty when success is False.
- `out_path_length` (float): sum of segment lengths in cm. 0.0 when success is False.

**World selection** — PIE world if PIE is active, otherwise the editor world. Both must have a built NavMesh (see `bridge-navigation-api.md` or call `execute_console_command('RebuildNavigation')` first).

**Cost**
- One synchronous `UNavigationSystemV1::FindPathToLocationSynchronously` call on the GameThread.
- Fast for local pathfinding (<5 ms typical); scales with corridor complexity.

**Output footprint** — small. Waypoint count is usually 2–20. At 30 Hz re-planning that's ~600 Vectors/s, negligible.

**Example**
```python
import unreal
ok, waypoints, length = unreal.UnrealBridgeGameplayLibrary.find_nav_path(
    start_location=obs.pawn_location,
    end_location=unreal.Vector(1000, 500, 100),
)
print(f'path ok={ok} length={length:.0f}cm points={len(waypoints)}')
```

**Pitfalls**
- Path planning requires a built RecastNavMesh. If the level has no navmesh actor or it hasn't been built, this returns `(False, [], 0.0)`.
- Points far outside the navmesh are snapped to the nearest navmesh region by the query — the returned `waypoints[-1]` may not equal `end_location` exactly.
- Partial paths (end not reachable) currently return False. If you need partial routing, call `NavSys::FindPathToLocationSynchronously` directly.

---

## Navmesh utilities

All four queries use the PIE world's navmesh when PIE is running and
fall back to the editor world's navmesh otherwise — same behaviour as
`find_nav_path`. Output is `None` when no world / no `UNavigationSystemV1`
/ the query itself fails.

### project_point_to_navmesh(point, search_extent) -> Vector or None

Clamp `point` onto the nearest walkable navmesh surface within an
axis-aligned half-extent box `search_extent` (cm). Use this before
`find_nav_path` when the caller has an arbitrary world point (e.g. a
camera hit on a wall) that isn't itself on the navmesh.

```python
origin = unreal.UnrealBridgeGameplayLibrary.get_camera_hit_location(10000.0)
if origin is not None:
    target = unreal.UnrealBridgeGameplayLibrary.project_point_to_navmesh(
        origin, unreal.Vector(200, 200, 400))
```

### is_point_on_navmesh(point, tolerance=50.0) -> bool

Boolean variant of `project_point_to_navmesh` — True if any navmesh
surface exists within `(tolerance, tolerance, tolerance)` cm of the
query point.

### get_nav_mesh_bounds() -> (Vector, Vector) or None

World-space AABB of the navigable world. Returned as `(min, max)`
Vector tuple on success, `None` when no navmesh exists. Use to seed
random sampling or to sanity-check that a planned path origin lies
within the navigable region.

### get_random_reachable_point_in_radius(origin, radius) -> Vector or None

Pick a random navmesh point within `radius` cm of `origin` that is
**path-reachable from `origin`** — not just "on a navmesh nearby".
Different navmesh islands are rejected. Returns `None` when the radius
contains no reachable points.

```python
obs = unreal.UnrealBridgeGameplayLibrary.get_agent_observation()
if obs.b_valid:
    wander = unreal.UnrealBridgeGameplayLibrary.get_random_reachable_point_in_radius(
        obs.pawn_location, 1500.0)
```

**Pitfalls**

- All four queries return None outside a navmesh-enabled level, even if
  `find_nav_path` was previously working — the level's navmesh may have
  been invalidated. Call `execute_console_command('RebuildNavigation')`
  from `bridge-editor-api` and retry.
- `search_extent` / `tolerance` must be positive — internally clamped
  to a minimum of 1 cm.

---

## Screen-space perception

All four helpers require PIE to be running with a valid player
controller — outside PIE they return `None` / `False`. Normalized
coordinates use `[0,1]` with origin at the top-left of the viewport.

### get_pie_viewport_size() -> Vector2D or None

PIE viewport pixel size (`x = width`, `y = height`). Use to denormalize
the [0,1] coordinates that project/deproject accept, or to reason about
aspect ratio.

### deproject_screen_to_world(normalized_x, normalized_y) -> (Vector, Vector) or None

Convert a normalized viewport position to a world-space ray. Returns a
`(origin, direction)` tuple on success; `direction` is unit-length.

```python
origin, direction = unreal.UnrealBridgeGameplayLibrary.deproject_screen_to_world(0.5, 0.5)
```

### project_world_to_screen(world_location) -> Vector2D or None

Project a world point to the PIE viewport. Returns `Vector2D` with
normalized `[0,1]` coordinates when the point is on-screen, `None`
otherwise (behind camera, or outside the viewport rectangle).

```python
ui_pos = unreal.UnrealBridgeGameplayLibrary.project_world_to_screen(
    unreal.Vector(1000, 0, 0))
```

### get_actor_at_screen_position(normalized_x, normalized_y, max_distance=10000.0) -> str

Convenience wrapper: deproject the screen position into a ray, line-trace
it (visibility channel, pawn ignored), return the first hit actor's
`FName` as string. Empty string on miss / no PIE.

```python
name = unreal.UnrealBridgeGameplayLibrary.get_actor_at_screen_position(0.5, 0.5)
```

**Pitfalls**

- Results flicker during the first few PIE frames while the camera and
  viewport are still resolving — always gate on `is_in_pie()`.
- Off-screen points return `None` from `project_world_to_screen`, *not*
  a clamped coordinate — callers that need the projected-but-clipped
  pixel value should call `ProjectWorldLocationToScreen` in Python
  directly.

---

## Actuators

All actuators target the PIE world's first player pawn/controller and
return `bool` (maps to `True`/`False` in Python — there are no out-params
so the return is a plain bool, not a struct-or-None tuple).

Input is accumulated per-frame by UE: each `apply_*_input` call contributes
to the current tick's input and is then consumed by the movement component.
A Python agent should call these once per tick for continuous behaviour. If
the agent stops calling them, the pawn decelerates on the next frame.

### apply_movement_input(world_direction, scale_value=1.0, b_force=False) -> bool

Wraps `APawn::AddMovementInput`. Direction is world-space; magnitude is
ignored (UE normalises). Scale in `[-1, 1]` flips and scales the input.

```python
import unreal
# Walk forward along the pawn's current facing.
obs = unreal.UnrealBridgeGameplayLibrary.get_agent_observation()
forward = obs.pawn_rotation.rotate_vector(unreal.Vector(1, 0, 0))
unreal.UnrealBridgeGameplayLibrary.apply_movement_input(forward, 1.0)
```

### apply_look_input(yaw_delta, pitch_delta) -> bool

Wraps `APlayerController::AddYawInput` / `AddPitchInput`. Values are in
"input units" — the same units the mouse delta would deliver through the
input mapping context, typically ~1 unit per degree.

### set_control_rotation(new_rotation) -> bool

Instantly snaps the controller's rotation; skips input smoothing. Useful
for "face this actor" commands or test teleports where continuous look
input would add unwanted visual ramp-up.

### jump() -> bool  /  stop_jumping() -> bool

Mirror of `ACharacter::Jump` / `StopJumping`. Returns False if the pawn
isn't a Character. For a tap jump, call `jump()` one tick and
`stop_jumping()` the next; for a variable-height jump hold `jump()`
across N ticks then release.

### Pitfalls

- `apply_movement_input` is a one-shot per tick. If the Python loop
  pauses longer than ~33 ms (one frame at 30 FPS), the pawn visibly
  stutters to a stop. Run the agent loop at or above the UE tick rate.
- `apply_look_input`'s scale depends on the project's mapping; for an
  unconfigured controller it may feel rotated by 0 degrees (no input
  binding). Prefer `set_control_rotation` for deterministic aiming.
- Actions/fire/interact (EnhancedInput IA triggers) are a separate
  actuator batch and not covered here yet.

---

## EnhancedInput injection (for projects that bypass AddMovementInput)

Many gameplay projects route WASD → `UInputAction` → GAS ability instead
of the classic `APawn::AddMovementInput` path. For those, the direct
actuators above look like no-ops because the game never reads the pawn's
input vector. Two injection flavours:

### inject_enhanced_input_axis(input_action_path, axis_value) -> bool

One-tick injection. Good for discrete press actions (`IA_Jump`,
`IA_Interact`) — single call fires the IA's `Pressed` / `Triggered`
phase for the frame.

**Not suitable for held axes.** A continuously-held axis like `IA_Move`
or `IA_Look` needs a value every UE frame; the Python bridge can't
reliably match 60 FPS, so use sticky inject instead.

**Value coercion**: the FVector you pass is automatically converted to
the IA's declared `EInputActionValueType` (Bool/Axis1D/Axis2D/Axis3D).
Pass `(1, 0, 0)` for a bool "pressed"; for Axis2D WASD pass `(x, y, 0)`.

### set_sticky_input(input_action_path, axis_value) -> bool  /  clear_sticky_input(input_action_path="") -> bool

Register a sticky injection: a server-side FTSTicker re-injects the
registered IA/value **every GameThread frame** until cleared. Set once
from Python, input persists at engine frame rate.

- `set_sticky_input(path, value)` — register or overwrite one entry.
- `clear_sticky_input(path)` — remove one entry by path.
- `clear_sticky_input("")` — clear all entries.

Multiple IAs can be sticky at the same time (e.g. move + look + sprint
all held). Passing a zero vector does NOT clear — it injects zero
every frame (useful to keep the binding alive while temporarily
suspending input without losing the subscription).

Sticky entries are auto-dropped when PIE ends, so each fresh PIE
session starts with no sticky input. The server-side ticker stops
running when the registry is empty.

**Example — walk forward for 2s:**
```python
import unreal, time
IA = '/LocomotionDriver/Input/IA_Move'
unreal.UnrealBridgeGameplayLibrary.set_sticky_input(IA, unreal.Vector(1, 0, 0))
time.sleep(2.0)
unreal.UnrealBridgeGameplayLibrary.clear_sticky_input(IA)
```

Observed behaviour in a GAS/Mover-style project (GameplayLocomotion):
pawn reaches ~300 cm/s within one frame, stays at walk speed for
the sticky duration, decelerates on clear.

### Debug fields on FAgentObservation

Added to the existing observation struct to help diagnose "why isn't
my input doing anything":

- `pawn_class_name`, `movement_component_class_name`, `movement_mode`
  (see `EMovementMode`: 0=None, 1=Walking, 3=Falling, ...) — identify
  whether the pawn even has a standard `CharacterMovementComponent`.
- `last_control_input_vector` — `APawn::GetLastMovementInputVector`.
  Zero despite calling `apply_movement_input` means the input never
  reached the pawn's accumulator (probably because the project uses
  EnhancedInput-only input routing).
- `current_acceleration` — `UCharacterMovementComponent::GetCurrentAcceleration`.
  Non-zero means input got through and the movement comp is reacting;
  zero despite a non-zero `last_control_input_vector` points at a
  root-motion / custom locomotion override.

---

## State inspection + reset

Small surface for "is the agent loop even running?" and "put the pawn
back to a known state" scenarios, without round-tripping a full
observation.

### is_in_pie() -> bool

True when a PIE world is currently playing **and** a player pawn has
spawned. Cheap gate for Python loops that should no-op outside PIE.

```python
import unreal
if not unreal.UnrealBridgeGameplayLibrary.is_in_pie():
    return  # agent loop waits for PIE to start
```

### get_control_rotation() -> FRotator or None

Read the current `APlayerController::GetControlRotation`. Pairs with
`set_control_rotation` — use this to remember the pre-teleport camera
direction so you can restore it after a scenario ends.

Returns `None` when PIE is not running (UE Python bool-plus-outparam
convention). On success returns the `FRotator` directly.

```python
rot = unreal.UnrealBridgeGameplayLibrary.get_control_rotation()
if rot is not None:
    print(f'camera yaw={rot.yaw:.1f}')
```

### get_sticky_inputs() -> (count, paths, values)

Enumerate active sticky EnhancedInput entries. Returned as a 3-tuple:
- `count` (int): number of entries.
- `paths` (array of str): each entry's asset path, e.g.
  `/LocomotionDriver/Input/IA_Move`.
- `values` (array of Vector): per-entry injected value, same index as `paths`.

Useful for asserting "no input is being held" between scenarios, or
inspecting a live input-replay script.

```python
count, paths, values = unreal.UnrealBridgeGameplayLibrary.get_sticky_inputs()
for p, v in zip(paths, values):
    print(f'  {p} = ({v.x}, {v.y}, {v.z})')
```

### get_camera_hit_actor(max_distance=10000.0) -> str

Line-trace forward from the player camera for up to `max_distance` cm
against the visibility channel. Returns the FIRST actor's `FName` as
string (matches `FAgentVisibleActor.actor_name`, *not* the display
label), or empty string on miss / no PIE. The pawn is auto-ignored so
self-hits never occur.

```python
target = unreal.UnrealBridgeGameplayLibrary.get_camera_hit_actor(5000.0)
if target:
    print(f'aiming at {target}')
```

### get_camera_hit_location(max_distance, out_hit_location) -> bool

UE Python convention: returns `FVector` when the ray hits, `None` when
it misses or PIE isn't running. The hit point is `FHitResult.ImpactPoint`.

```python
loc = unreal.UnrealBridgeGameplayLibrary.get_camera_hit_location(10000.0)
if loc is not None:
    print(f'aim point = {loc}')
```

### is_actor_visible_from_camera(actor_name, max_distance=10000.0) -> bool

Bounds-sampling visibility test: traces from the camera to nine sample
points on the target's bounds (center + 8 corners). Returns True as
soon as ANY ray reaches the target or its endpoint unobstructed; False
if every sample is blocked by another actor first.

`actor_name` is matched first by `FName`, then by display label. Pass
the value returned by `get_camera_hit_actor()` directly.

**Pitfalls**

- Bounds are evaluated via `AActor::GetActorBounds(bOnlyCollidingComponents=false)`.
  Actors without any primitive components return zero bounds — all 9
  samples collapse to the pivot, which may be occluded.
- When the camera is inside another actor's collision (e.g. a default
  PIE spectator spawned inside geometry), every outbound ray hits that
  actor first and this check returns False even for visually-unobstructed
  targets. Fix by possessing a proper pawn before querying.
- The pawn is auto-ignored, but sibling occluders (smoke volumes,
  trigger boxes on Visibility channel) are not — they'll register as
  blockers.

### get_pawn_ground_height(max_distance=5000.0) -> float

Downward line-trace from the pawn's pivot for up to `max_distance` cm.
Returns the distance to the first hit surface in cm, or `-1.0` on miss
/ no PIE. Useful for stairs/drop detection without parsing the full
`CharacterMovement` state.

```python
h = unreal.UnrealBridgeGameplayLibrary.get_pawn_ground_height()
if h < 0:
    print('pawn is airborne or over a bottomless pit')
elif h > 200:
    print(f'drop below is {h:.0f} cm')
```

---

### teleport_pawn(new_location, new_rotation, b_snap_controller=True, b_stop_velocity=True) -> bool

Hard-reset the pawn's pose for scenario setup. Wraps
`SetActorLocationAndRotation(..., ETeleportType::TeleportPhysics)` and
optionally stops movement + snaps the controller rotation to match.

- `b_snap_controller=True` (default): also set the player controller's
  rotation. Keeps `apply_movement_input(forward)` intuitive post-teleport.
  Set False if the camera/controller should retain its existing aim.
- `b_stop_velocity=True` (default): call `StopMovementImmediately` on
  the movement component so the character doesn't slide past the target
  on the next physics tick. Set False to preserve momentum (e.g. to
  launch from a moving state into a new pose).

```python
import unreal
ok = unreal.UnrealBridgeGameplayLibrary.teleport_pawn(
    new_location=unreal.Vector(500, 0, 100),
    new_rotation=unreal.Rotator(0, 0, 0),
    b_snap_controller=True,
    b_stop_velocity=True,
)
```

**Pitfalls**
- Returns False outside PIE (no pawn to teleport).
- The pawn's collision shape is swept at `TeleportPhysics` — if the
  target location is inside a wall, physics will still resolve it on the
  next tick (may nudge the pawn out). For pure geometric test setups,
  pick a location in clear space.
- Non-Character pawns skip `StopMovementImmediately` gracefully (their
  `UPawnMovementComponent::StopMovementImmediately` is a no-op).


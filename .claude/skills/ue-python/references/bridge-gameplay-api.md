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

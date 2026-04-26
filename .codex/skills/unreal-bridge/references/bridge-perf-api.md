# bridge-perf-api

`unreal.UnrealBridgePerfLibrary` — structured performance snapshots. Replaces parsing `stat unit` text output with typed UPROPERTY structs.

Values come from engine globals on the GameThread:
- `FStatUnitData` from the active level viewport (smoothed running averages) → `GetFrameTiming`
- `GNumDrawCallsRHI` / `GNumPrimitivesDrawnRHI` (RHI, summed across `MAX_NUM_GPUS`) → `GetRenderCounters`
- `FPlatformMemory::GetStats()` + `FPlatformMemory::GetConstants()` → `GetMemoryStats`
- `TObjectIterator<UObject>` aggregated by `UClass` → `GetUObjectStats`

All functions are cheap except `GetUObjectStats` — see the cost note per function.

---

## get_frame_timing() -> FBridgeFrameTiming

Frame-time breakdown (FPS, game/render/GPU ms). Always pulls from the raw `GGameThreadTime` / `GRenderThreadTime` / `RHIGetGPUFrameCycles` globals (updated every frame by `FViewport::Draw`, independent of any stat display). When the active level viewport has `stat unit` enabled — detected by checking `FStatUnitData::FrameTime > 0` — the smoothed running averages override the raw values for stability.

| Field | Type | Notes |
|---|---|---|
| `fps` | float | `GAverageFPS`. 0 until the first full frame. |
| `frame_ms` | float | `FStatUnitData::FrameTime` when smoothed; else `1000/fps` (or `GAverageMS` if FPS is 0). |
| `game_thread_ms` | float | `GGameThreadTime` in ms; overridden by `FStatUnitData::GameThreadTime` when smoothed. |
| `render_thread_ms` | float | `GRenderThreadTime` in ms; overridden by `FStatUnitData::RenderThreadTime` when smoothed. |
| `gpu_ms` | float | `RHIGetGPUFrameCycles` summed across `GNumExplicitGPUsForRendering`; overridden by `FStatUnitData::GPUFrameTime` (summed) when smoothed. |
| `rhi_ms` | float | RHI translation time. 0 on the raw path — only populated when smoothed=true. |
| `delta_seconds` | float | `FApp::GetDeltaTime()` for the most recent frame. |
| `frame_number` | int64 | `GFrameCounter`. |
| `smoothed` | bool | True when `stat unit` is active on a viewport and values came from `FStatUnitData`. False = raw per-frame globals (no running average). |

**Cost** — O(1). Safe to poll per frame.

**Example**
```python
import unreal
t = unreal.UnrealBridgePerfLibrary.get_frame_timing()
print(f"FPS={t.fps:.1f} GT={t.game_thread_ms:.2f}ms RT={t.render_thread_ms:.2f}ms GPU={t.gpu_ms:.2f}ms smoothed={t.smoothed}")
```

**Pitfalls**
- When the editor is unfocused / minimised, UE throttles the tick to ~3 FPS by default (`Editor Preferences → Use Less CPU when in Background`). Timings aren't wrong — the editor really is running that slow. Focus the window or disable the throttle to measure real performance.
- `render_thread_ms` can be 0 on the raw path when the RT spent all its time idle. Not a bug.
- `rhi_ms` on the raw path is always 0. To read actual RHI time, turn on `stat unit` in the active viewport — then `smoothed=true` and `rhi_ms` is populated.
- PIE drives its own rendering — timings inside PIE reflect the PIE viewport, not the editor viewport.

---

## get_render_counters() -> FBridgeRenderCounters

Draw call and primitive counts for the most recently rendered frame.

| Field | Type | Notes |
|---|---|---|
| `draw_calls` | int32 | `GNumDrawCallsRHI` summed across `MAX_NUM_GPUS`. |
| `primitives_drawn` | int32 | `GNumPrimitivesDrawnRHI` summed across `MAX_NUM_GPUS`. |
| `num_gpus` | int32 | `GNumExplicitGPUsForRendering`. 1 on most desktop builds. |

**Cost** — O(1). Safe to poll per frame.

**Example**
```python
c = unreal.UnrealBridgePerfLibrary.get_render_counters()
print(f"draws={c.draw_calls} prims={c.primitives_drawn}")
```

**Pitfalls**
- These are counters for the most recently *submitted* frame. If the editor hasn't rendered (minimised), values are stale.
- Capped at `MAX_int32` in the unlikely event of overflow — use the aggregate snapshot's `frame_number` to detect counter saturation across a long run.

---

## get_memory_stats() -> FBridgeMemoryStats

Process + system memory in mebibytes (MiB = 1024 × 1024 bytes).

| Field | Type | Notes |
|---|---|---|
| `used_physical_mb` | int64 | Process working set. |
| `used_virtual_mb` | int64 | Process virtual commit. |
| `peak_used_physical_mb` | int64 | Peak working set observed this session. |
| `peak_used_virtual_mb` | int64 | Peak virtual commit observed this session. |
| `available_physical_mb` | int64 | System-wide available physical RAM. |
| `available_virtual_mb` | int64 | System-wide available virtual. |
| `total_physical_mb` | int64 | Total physical RAM on the machine. |

**Cost** — O(1). Safe to poll per second.

**Example**
```python
m = unreal.UnrealBridgePerfLibrary.get_memory_stats()
print(f"editor using {m.used_physical_mb} MiB (peak {m.peak_used_physical_mb}) / {m.total_physical_mb} total")
```

---

## get_u_object_stats(top_n=20) -> FBridgeUObjectStats

Top-N `UClass` types ranked by live UObject count, plus totals.

| Field | Type | Notes |
|---|---|---|
| `total_objects` | int32 | Every live UObject walked. |
| `unique_classes` | int32 | Distinct `UClass` types seen. |
| `top_classes` | array of `FBridgeUObjectClassCount` | `{class_name, count}`, descending by `count`. |

**Cost** — O(N) in live UObjects. A typical mid-sized editor session has 300 k – 2 M UObjects, so this call costs **10 – 200 ms** on the GameThread. **Don't poll per frame.** Use for "is memory climbing?" diagnostics, memory leak hunting, or baseline snapshots between operations.

**Parameters**
- `top_n` (int32): number of classes to return. Clamped to `[1, 200]`. Default 20.

**Example**
```python
u = unreal.UnrealBridgePerfLibrary.get_u_object_stats(10)
print(f"{u.total_objects} UObjects across {u.unique_classes} classes")
for row in u.top_classes:
    print(f"  {row.count:>8}  {row.class_name}")
```

**Pitfalls**
- The walk is not atomic — during a GC pass UObject count can drop mid-iteration. For stable baselines, call before any PIE and avoid right after `CollectGarbage`.
- Reported classes are the *leaf* class of each object, not the nearest `UClass` in a hierarchy you care about. To bucket "all BP-derived Actors" you need to post-process `top_classes` yourself.

---

## get_perf_snapshot(include_uobject_stats=False, uobject_top_n=20) -> FBridgePerfSnapshot

One-call aggregate for regression tests / CI-style sampling.

| Field | Type | Notes |
|---|---|---|
| `timing` | `FBridgeFrameTiming` | See `get_frame_timing`. |
| `render` | `FBridgeRenderCounters` | See `get_render_counters`. |
| `memory` | `FBridgeMemoryStats` | See `get_memory_stats`. |
| `u_objects` | `FBridgeUObjectStats` | Zero-filled when `include_uobject_stats=False` (default). |
| `capture_time_utc` | str | ISO-8601 UTC timestamp — handy for delta-comparison across snapshots. |
| `engine_version` | str | e.g. `"5.7.0-0+UE5"`. |
| `was_in_pie` | bool | True when `GEditor->PlayWorld` was non-null at capture time. |

**Parameters**
- `include_uobject_stats` (bool): when true, also runs `get_u_object_stats(uobject_top_n)` (slow — see above). Default false.
- `uobject_top_n` (int32): forwarded to `get_u_object_stats` when enabled.

**Cost**
- With `include_uobject_stats=False`: O(1), microseconds. OK to poll per second.
- With `include_uobject_stats=True`: 10-200 ms. Use for keyframed baselines, not hot-loop polling.

**Example — cheap snapshot for regression comparison**
```python
import unreal, json
s = unreal.UnrealBridgePerfLibrary.get_perf_snapshot(False)
print(f"[{s.capture_time_utc}] FPS={s.timing.fps:.1f} draws={s.render.draw_calls} mem={s.memory.used_physical_mb}MiB pie={s.was_in_pie}")
```

**Example — full snapshot with UObject histogram**
```python
s = unreal.UnrealBridgePerfLibrary.get_perf_snapshot(True, 15)
print(f"[{s.capture_time_utc}] UObjects={s.u_objects.total_objects} across {s.u_objects.unique_classes} classes")
for row in s.u_objects.top_classes[:5]:
    print(f"  {row.count:>8}  {row.class_name}")
```

---

## Cookbook

### Compare two points in time
```python
import unreal, time
before = unreal.UnrealBridgePerfLibrary.get_perf_snapshot(True)
# ... do some work ...
after = unreal.UnrealBridgePerfLibrary.get_perf_snapshot(True)

delta_mem_mb = after.memory.used_physical_mb - before.memory.used_physical_mb
delta_objs   = after.u_objects.total_objects   - before.u_objects.total_objects
print(f"Δmem = {delta_mem_mb:+d} MiB   Δobjs = {delta_objs:+d}")
```

### Watch the editor for 10 seconds
Run two bridge execs — the first starts a viewport camera flight / PIE / etc., the second samples. Bridge `exec` holds the GameThread, so *don't* loop sleep-and-sample inside a single exec (see `feedback_bridge_exec_holds_gamethread` in memory).

```python
# exec 1: kick off the thing you want to measure (PIE start / cinematic / etc.)
# exec 2:
import unreal, time, json
samples = []
end = time.time() + 10.0
while time.time() < end:
    t = unreal.UnrealBridgePerfLibrary.get_frame_timing()
    samples.append({"frame": t.frame_number, "fps": t.fps, "gt_ms": t.game_thread_ms, "rt_ms": t.render_thread_ms, "gpu_ms": t.gpu_ms})
    time.sleep(0.1)
print(json.dumps(samples))
```

Note: the loop runs on the GameThread (blocking). During the 10s sample window editor ticking is blocked — use short windows, or set up a reactive handler instead for longer captures.

# UnrealBridge Curve Library API

Module: `unreal.UnrealBridgeCurveLibrary`

Read / write float curves (`UCurveFloat`, `UCurveVector`, `UCurveLinearColor`) and
curve tables (`UCurveTable`) with batch-safe semantics.

## When to use this vs. raw Python reflection

For **`UCurveFloat` single-key edits** you can usually skip this library — `curve.float_curve.add_key(t, v)` / `get_float_value(t)` / `EditorAssetLibrary.save_asset(path)` work via reflection alone.

This library earns its keep when:

1. **Batch evaluating** a channel at N points — one bridge round-trip instead of N.
2. **Editing `UCurveTable` rows** — `FRealCurve` / `FSimpleCurve` dispatch isn't clean in Python.
3. **Atomic tangent writes** — setting `TangentMode` + `TangentWeightMode` + the four weight scalars in one call, under a single `FScopedTransaction`, with `OnCurveChanged` fired exactly once at the end.
4. **Refreshing an open Curve Editor tab** — writes broadcast the curve-owner delegate so open editors redraw immediately.

## Enum strings

Enum fields are strings on the wire (case-insensitive reads). Canonical spellings:

| Enum | Values |
|---|---|
| `InterpMode` | `"Linear"` / `"Constant"` / `"Cubic"` / `"None"` |
| `TangentMode` | `"Auto"` / `"User"` / `"Break"` / `"None"` / `"SmartAuto"` |
| `TangentWeightMode` | `"None"` / `"Arrive"` / `"Leave"` / `"Both"` |
| `InfinityExtrap` | `"Cycle"` / `"CycleWithOffset"` / `"Oscillate"` / `"Linear"` / `"Constant"` / `"None"` |

## Channel indexing

| Asset class | Channels | `channel_index` |
|---|---|---|
| `UCurveFloat` | 1 | `0` → "Value" |
| `UCurveVector` | 3 | `0` → X, `1` → Y, `2` → Z |
| `UCurveLinearColor` | 4 | `0` → R, `1` → G, `2` → B, `3` → A |

## FBridgeRichCurveKey fields

| Field | Type | Notes |
|---|---|---|
| `time` | float | |
| `value` | float | |
| `interp_mode` | str | See table above |
| `tangent_mode` | str | See table above |
| `tangent_weight_mode` | str | See table above |
| `arrive_tangent` | float | Cubic mode only |
| `leave_tangent` | float | Cubic mode only |
| `arrive_tangent_weight` | float | Weighted modes only |
| `leave_tangent_weight` | float | Weighted modes only |

---

## Read

### get_curve_info(curve_path) -> FBridgeCurveInfo

Identify + per-channel key counts + infinity extrap. Cheap.

```python
info = unreal.UnrealBridgeCurveLibrary.get_curve_info('/Game/Curves/C_Damage')
print(info.class_name, list(info.channel_names), list(info.num_keys_per_channel))
```

### get_curve_keys(curve_path, channel_index) -> list[FBridgeRichCurveKey]

All keys on one channel.

### get_curve_as_json_string(curve_path) -> str

Full asset dump as compact JSON: `{name, class, channels:[{name, pre_infinity, post_infinity, keys:[...]}, ...]}`. Cheaper to serialize + transport than a USTRUCT array for the whole asset.

---

## Write (curve assets)

All writes run inside `FScopedTransaction` (Ctrl+Z restores the entire call as one unit), mark the package dirty, and fire `OnCurveChanged` so an open Curve Editor tab redraws.

### set_curve_keys(curve_path, channel_index, keys) -> bool

Replace the whole key list on one channel. Keys don't need to be sorted — the library sorts by `time` before `FRichCurve::SetKeys`.

```python
def mk(t, v, interp="Cubic", tan="Auto"):
    k = unreal.BridgeRichCurveKey(); k.time = t; k.value = v
    k.interp_mode = interp; k.tangent_mode = tan; k.tangent_weight_mode = "None"
    return k

keys = [mk(0, 0), mk(1, 10), mk(2, 5)]
unreal.UnrealBridgeCurveLibrary.set_curve_keys('/Game/Curves/C_Damage', 0, keys)
```

### add_curve_key(curve_path, channel_index, key) -> int

Append one key; returns the inserted index in the (sorted) key array, or `-1` on failure. If a key already exists within `UE_KINDA_SMALL_NUMBER` of `time`, its value is updated and its index returned.

### remove_curve_key_by_index(curve_path, channel_index, index) -> bool

### clear_curve_keys(curve_path, channel_index) -> bool

### set_curve_key_tangents(curve_path, channel_index, index, tangent_mode, tangent_weight_mode, arrive_tangent, leave_tangent, arrive_tangent_weight, leave_tangent_weight) -> bool

Atomic tangent write — all four tangent scalars + both enum fields set under one transaction.

- Pass `""` for `tangent_mode` / `tangent_weight_mode` to leave them untouched.
- Pass `float("nan")` for any scalar to leave it untouched.

```python
# Set arrive + leave tangents but keep the existing weights
import math
unreal.UnrealBridgeCurveLibrary.set_curve_key_tangents(
    '/Game/Curves/C_Damage', 0, 0,
    "User", "None",
    5.0, 5.0,
    math.nan, math.nan,
)
```

### set_curve_infinity_extrap(curve_path, pre, post) -> bool

Set pre / post-infinity extrapolation on **every channel** of the asset. Pass `""` on either side to leave it alone.

### auto_set_curve_tangents(curve_path, tension) -> bool

Recompute cubic tangents for every `Auto`-mode key on every channel. `tension = 0.0` = default; negative tightens, positive slackens.

---

## Eval (batch)

### evaluate_curve(curve_path, channel_index, times) -> list[float]

Evaluate one channel at N input times; one bridge round-trip.

```python
ts = [i * 0.1 for i in range(101)]    # 0.0 .. 10.0 step 0.1
ys = unreal.UnrealBridgeCurveLibrary.evaluate_curve('/Game/Curves/C_Damage', 0, ts)
```

### sample_curve_uniform(curve_path, channel_index, start_time, end_time, num_samples) -> list[float]

Evenly-spaced samples in `[start_time, end_time]` (inclusive). For `num_samples <= 1` returns a single sample at `start_time`. Convenience wrapper — just `evaluate_curve` with a pre-built time list.

---

## Curve Table

### get_curve_table_info(curve_table_path) -> FBridgeCurveTableInfo

| Field | Type | Description |
|---|---|---|
| `name` | str | Asset name |
| `curve_table_mode` | str | `"Empty"` / `"SimpleCurves"` / `"RichCurves"` |
| `row_names` | list[str] | |
| `num_keys_per_row` | list[int] | Parallel to `row_names` |

### get_curve_table_row_keys(curve_table_path, row_name) -> list[FBridgeRichCurveKey]

Read one row. For `SimpleCurves` tables, the simple-curve row-wide interp is copied into each returned key's `interp_mode`; tangent fields default to `0` / `"None"`.

### set_curve_table_row_keys(curve_table_path, row_name, keys) -> bool

Replace the keys on an **existing** row. Does not create new rows.

- `RichCurves` table: all bridge key fields are applied.
- `SimpleCurves` table: only `time`, `value`, and the first key's `interp_mode` are used — tangent fields and per-key interp overrides are ignored because simple curves don't store them.

### add_curve_table_row(curve_table_path, row_name, keys) -> bool

Create a new row. Fails if the row already exists.

- `Empty` table → defaults to `RichCurves` mode on first add.
- `RichCurves` / `SimpleCurves` table → matches the existing mode.

### remove_curve_table_row(curve_table_path, row_name) -> bool

### rename_curve_table_row(curve_table_path, old_row_name, new_row_name) -> bool

Fails if `old` doesn't exist, or `new` already exists.

### evaluate_curve_table_row(curve_table_path, row_name, times) -> list[float]

Batch-eval one row at N times. Works on both `SimpleCurves` and `RichCurves` tables.

---

## Save + refresh

The library **marks packages dirty and broadcasts `OnCurveChanged` / `OnCurveTableChanged`** so open Curve Editor tabs redraw. It does **not save to disk** — the caller decides when to persist:

```python
unreal.EditorAssetLibrary.save_asset('/Game/Curves/C_Damage')
```

## Typical workflows

### Plot a damage curve over level 1–60

```python
lib = unreal.UnrealBridgeCurveLibrary
ts = list(range(1, 61))
ys = lib.evaluate_curve('/Game/Curves/C_DamagePerLevel', 0, ts)
# → hand ys to LLM for plotting
```

### Rescale a DT_Weapon damage curve table row by 1.5x

```python
lib = unreal.UnrealBridgeCurveLibrary
row = lib.get_curve_table_row_keys('/Game/Data/CT_Weapons', 'Sword_01')
scaled = []
for k in row:
    new_k = unreal.BridgeRichCurveKey()
    new_k.time = k.time
    new_k.value = k.value * 1.5
    new_k.interp_mode = k.interp_mode
    new_k.tangent_mode = k.tangent_mode
    new_k.tangent_weight_mode = k.tangent_weight_mode
    new_k.arrive_tangent = k.arrive_tangent * 1.5
    new_k.leave_tangent = k.leave_tangent * 1.5
    new_k.arrive_tangent_weight = k.arrive_tangent_weight
    new_k.leave_tangent_weight = k.leave_tangent_weight
    scaled.append(new_k)
lib.set_curve_table_row_keys('/Game/Data/CT_Weapons', 'Sword_01', scaled)
unreal.EditorAssetLibrary.save_asset('/Game/Data/CT_Weapons')
```

### Convert a `linear` XP curve to `ease-out`

```python
lib = unreal.UnrealBridgeCurveLibrary
info = lib.get_curve_info('/Game/Curves/C_XpPerLevel')
keys = lib.get_curve_keys('/Game/Curves/C_XpPerLevel', 0)
# Flip all to cubic + Auto tangents
for k in keys:
    k.interp_mode = "Cubic"; k.tangent_mode = "Auto"
lib.set_curve_keys('/Game/Curves/C_XpPerLevel', 0, keys)
lib.auto_set_curve_tangents('/Game/Curves/C_XpPerLevel', 0.0)
```

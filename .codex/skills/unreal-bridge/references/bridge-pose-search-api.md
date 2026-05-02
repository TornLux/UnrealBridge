# UnrealBridge PoseSearch Library API

Module: `unreal.UnrealBridgePoseSearchLibrary`

Read+write coverage of the three Motion Matching asset families:

| Asset | Class (UE) | This library covers |
|---|---|---|
| **PSS** — PoseSearchSchema | `UPoseSearchSchema` | info, channel listing |
| **PSD** — PoseSearchDatabase | `UPoseSearchDatabase` | info, animation entries (CRUD), index lifecycle |
| Normalization Set | `UPoseSearchNormalizationSet` | (read via `get_database_info().normalization_set_path`; no write ops) |

> **What this library does NOT cover:** the Chooser tables that *select* a PSD at runtime live in `bridge-chooser-api.md`. The MotionMatching anim node and its Database pin live in `bridge-anim-api.md` (use the existing AnimGraph write ops to wire it).

> **Index rebuilds are async.** Every PSD write op (add/remove/edit) automatically schedules a fresh DDC rebuild via `FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(NewRequest)`. The CLI `bridge.py wait-pose-index <psd_path>` polls until the rebuild lands. Don't `wait` inside an `exec` — the GameThread is held; rebuild can't progress.

> **MirrorOption is a string, not an enum.** Pass one of `"UnmirroredOnly"` / `"MirroredOnly"` / `"UnmirroredAndMirrored"` (case-insensitive; `"original"` / `"mirror"` / `"both"` are also accepted aliases). Empty string defaults to `UnmirroredOnly`.

---

## Schema (PSS) — read

### `get_schema_info(schema_path) → FBridgePSSInfo`

```python
info = unreal.UnrealBridgePoseSearchLibrary.get_schema_info(
    schema_path='/Game/.../PSS_Default')
```

Returned struct fields:

| Field | Meaning |
|---|---|
| `sample_rate` | Hz at which entries get indexed (`SampleRate` UPROPERTY). |
| `schema_cardinality` | **Final feature-vector dimension** after `Finalize()`. Drives KDTree size + PCA upper bound; this is the number to watch when tuning a schema. Cached `Transient` field, only populated after the schema has been used. |
| `channel_count` | Authored top-level `Channels` count. |
| `finalized_channel_count` | Post-Finalize channel count (dependent debug channels can inflate this when `bInjectAdditionalDebugChannels` is on). |
| `total_channel_weight` | Sum of all top-level channel `Weight` values — sanity check: should be ~ `channel_count` if you haven't deviated from defaults. |
| `data_preprocessor` | `"None"` / `"Normalize"` / `"NormalizeOnlyByDeviation"` / `"NormalizeWithCommonSchema"`. The last one is what GASP's PSS_Default uses — required for cross-database normalization. |
| `number_of_permutations`, `permutations_sample_rate`, `permutations_time_offset` | Permutation-based indexing knobs (default 1 / 30 / 0). |
| `add_data_padding`, `inject_additional_debug_channels` | Performance / debug toggles. |
| `skeletons[]` | One entry per role: `skeleton_path`, `mirror_data_table_path`, `role`. |

### `list_schema_channels(schema_path) → TArray<FBridgePSSChannel>`

Top-level (authored) channel summary. Sub-channels (e.g. inside a `Group`) are NOT flattened — the row's `sub_channel_count` tells you how many it has, but their structure isn't reachable from this library yet (read manually via `get_editor_property('sub_channels')` for now).

| Field | Meaning |
|---|---|
| `kind` | C++ class short name, e.g. `"PoseSearchFeatureChannel_Trajectory"`. |
| `weight` | Authored channel weight (1.0 default). |
| `bone_name` | For Position / Velocity / Heading: the bone string. Empty for Group / Trajectory. |
| `sample_time_offset` | Seconds of past/future the channel samples (negative = past). |
| `sub_channel_count` | Immediate children — Group containers and Trajectory both have these. |

---

## Database (PSD) — read

### `get_database_info(database_path) → FBridgePSDInfo`

```python
psd = unreal.UnrealBridgePoseSearchLibrary.get_database_info(
    database_path='/Game/.../PSD_Sparse_Stand_Walk_Loops')
```

| Field | Meaning |
|---|---|
| `schema_path`, `normalization_set_path` | Refs to the PSS / PSN driving this database. |
| `entry_count` | Number of animation entries (= `len(list_database_animations(...))`). |
| `indexed_pose_count` | How many poses the index actually contains. 0 means "not yet indexed". |
| `index_status` | `"NotIndexed"` / `"Indexing"` / `"Indexed"` / `"Failed"`. Same source as `get_index_status`. |
| `indexed_memory_bytes` | Approx feature-vector + metadata bytes. Useful for "did this database explode" audits when chained schema/normalization changes blow up cardinality. |
| `pose_search_mode` | `"BruteForce"` / `"PCAKDTree"` / `"VPTree"` / `"EventOnly"`. PCAKDTree is the default. |
| `number_of_principal_components`, `k_d_tree_query_num_neighbors` | PCA / KDTree tuning. |
| `continuing_pose_cost_bias`, `base_cost_bias`, `looping_cost_bias` | Cost adjustments — negative = more sticky, positive = less likely to pick. |
| `tags` | Authored FName tags. |

### `list_database_animations(database_path) → TArray<FBridgePSDAnimEntry>`

Each entry exposes the same fields as the editor inspector:

| Field | Meaning |
|---|---|
| `kind` | `"AnimSequence"` / `"BlendSpace"` / `"AnimComposite"` / `"AnimMontage"`. |
| `asset_path` | Path of the underlying anim asset. |
| `enabled`, `disable_reselection`, `mirror_option` | Per-entry runtime flags. |
| `sampling_range_min`, `sampling_range_max` | Trim range. `[0, 0]` means "use entire animation". |
| `blend_space_horizontal_samples`, `blend_space_vertical_samples`, `b_blend_space_use_grid_for_sampling`, `b_blend_space_use_single_sample`, `blend_space_param_x/y` | BlendSpace-specific. `-1` H/V samples means "not a BlendSpace". |

### `find_databases_using_animation(animation_asset_path) → TArray<FString>`

Reverse lookup: which PSDs reference an anim asset? **Use this instead of `Asset.get_package_referencers`** — PSD entries are `FInstancedStruct`, asset registry doesn't see them.

---

## Database (PSD) — write

> Every write below dirties the package + schedules an async re-index. Save with `unreal.EditorAssetLibrary.save_loaded_asset(psd)` when you're ready; the bridge does not auto-save.

### `add_animation_to_database(...) → FBridgePSDAddResult`

```python
r = unreal.UnrealBridgePoseSearchLibrary.add_animation_to_database(
    database_path='/Game/.../PSD_Foo',
    animation_asset_path='/Game/.../A_MyClip',
    sampling_range_min=0.0,
    sampling_range_max=0.0,        # 0/0 = entire animation
    mirror_option='UnmirroredOnly',
    enabled=True,
)
if r.index < 0:
    raise RuntimeError(r.error)    # human-readable: "cannot load …", "is a … not Anim*", etc.
```

Accepts `AnimSequence`, `BlendSpace`, `AnimComposite`, `AnimMontage`. **Failure is reported via `result.error`, not via UE_LOG** (UE_LOG is invisible to bridge clients).

> **Common typo trap:** GASP's walk-loops are at `/Game/Characters/UEFN_Mannequin/Animations/Walk/` not `/Locomotion/Walks/`. When the editor was on a different version the path layout drifted — always verify with `Asset.search_assets_in_all_content(name=...)` before adding.

### `add_blend_space_to_database(...) → FBridgePSDAddResult`

Same shape, plus BlendSpace knobs:

```python
r = unreal.UnrealBridgePoseSearchLibrary.add_blend_space_to_database(
    database_path='...',
    blend_space_path='/Game/.../BS_Locomotion',
    h_samples=9, v_samples=2,
    b_use_grid_for_sampling=False,    # True overrides h/v with the BS's authored grid
    b_use_single_sample=False,        # True treats the BS as one segment at (param_x, param_y)
    blend_param_x=0.0, blend_param_y=0.0,
    sampling_range_min=0.0, sampling_range_max=0.0,
    mirror_option='UnmirroredOnly', enabled=True,
)
```

### `remove_database_animation_at(database_path, index) → bool`

Drop an entry by zero-based index. Triggers re-index.

### `remove_database_animation_by_asset(database_path, animation_asset_path) → int`

Drop the **first** entry referencing that asset. Returns the removed index, or `-1` if not found. If the same asset appears twice (e.g. once mirrored, once not), this only removes the first — call again to remove the rest.

### `clear_database_animations(database_path) → int`

Empty the entry list. Returns the count removed.

### Per-entry edits

```python
unreal.UnrealBridgePoseSearchLibrary.set_database_animation_enabled(
    database_path='...', index=3, enabled=False)
unreal.UnrealBridgePoseSearchLibrary.set_database_animation_sampling_range(
    database_path='...', index=3, sampling_range_min=0.5, sampling_range_max=2.5)
unreal.UnrealBridgePoseSearchLibrary.set_database_animation_mirror_option(
    database_path='...', index=3, mirror_option='UnmirroredAndMirrored')
unreal.UnrealBridgePoseSearchLibrary.set_database_blend_space_sampling(
    database_path='...', index=3,
    h_samples=11, v_samples=3,
    b_use_grid_for_sampling=False, b_use_single_sample=False,
    blend_param_x=0.0, blend_param_y=0.0)
```

All return `bool`. `set_database_blend_space_sampling` returns `False` when the entry is not a BlendSpace.

---

## Index lifecycle

PSD writes schedule rebuilds; these probe / wait / control the resulting state.

| Function | Returns | Notes |
|---|---|---|
| `request_async_build_index(database_path)` | `"InProgress"` / `"Success"` / `"Failed"` / `"Error"` | Forces a fresh build (NewRequest). Already covered by every write op — only call manually if you skipped the writes and *just* want to rebuild. |
| `is_index_ready(database_path)` | `bool` | `True` only when status is "Indexed". |
| `get_index_status(database_path)` | `"NotIndexed"` / `"Indexing"` / `"Indexed"` / `"Failed"` | Cheap probe — uses `ContinueRequest`, doesn't kick a build. |
| `invalidate_index(database_path)` | `bool` | Equivalent to `request_async_build_index` but explicit about intent. |

### CLI: wait for an index

```bash
python bridge.py wait-pose-index '/Game/.../PSD_Foo' --wait-timeout 300 --poll-interval 1.0
```

Mirrors `wait-compile`: GameThread releases between polls so the async indexer can finish on its own ticks. Don't poll inside an `exec` — same deadlock pattern as shader compile.

Exit codes: `0` ready · `1` timeout · `2` database not loadable / build failed · `3` transport error.

---

## Pitfalls

- **Never bundle add-and-immediately-search in one exec.** The index isn't ready until the async rebuild finishes ticks; running a Search call right after Add returns stale results. Pattern: split into `exec1: add` → CLI `wait-pose-index` → `exec2: search/verify`.
- **Channel sub-structure beyond top-level isn't surfaced** by `list_schema_channels` — only `sub_channel_count` is. To see what's inside a Group, you currently need `unreal.load_object(...).get_editor_property('channels')[i].get_editor_property('sub_channels')`. (Could be lifted into a future bridge call.)
- **`schema_cardinality` is 0 until the schema is finalized.** Loading an unused schema returns `0`; trigger any indexed PSD against it to force `Finalize`.
- **`list_database_animations` is the only way to see entries.** `unreal.PoseSearchDatabase.get_editor_property('database_animation_assets')` raises — the field is `private:` without `EditAnywhere`. The library wraps `GetMutableDatabaseAnimationAsset(idx)` to reach it from C++.
- **Saving is not automatic.** Writes mark the package dirty but don't save. `unreal.EditorAssetLibrary.save_loaded_asset(psd)` to commit; otherwise the editor will prompt on close.

---

## Schema authoring is partial

This library exposes schema **reads** (info + channel list) but no write ops yet. To author a new schema or add a channel, fall back to:

```python
pss = unreal.PoseSearchSchema.cast(unreal.load_object(None, '/Game/.../PSS_Foo'))
# Channels is private but EditAnywhere — reachable via reflection
```

The C++ `UPoseSearchSchema::AddChannel(UPoseSearchFeatureChannel*)` and `AddSkeleton(...)` are public — straightforward to wrap into bridge functions when needed. **Open an issue / ask before reaching for raw reflection** — usually the right answer is "extend this library."

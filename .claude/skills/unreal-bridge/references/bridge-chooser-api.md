# UnrealBridge Chooser Library API

Module: `unreal.UnrealBridgeChooserLibrary`

Read+write coverage of `UChooserTable` (`.uasset` extension: `ChooserTable` / `CHT_*` by convention). Choosers map a row of input columns → a result asset/class/sub-chooser.

> **Where this fits:** ChooserTables are the dispatch layer between AnimBP variables and PoseSearchDatabase / Montage / asset selection. To inspect a PSD that a chooser returns, see `bridge-pose-search-api.md`. To drive choosers from an AnimBP at runtime, the AnimBP wires a `Chooser Player` node — see `bridge-anim-api.md`.

> **Mental model:** A `UChooserTable` is **two parallel arrays of `FInstancedStruct`**:
> - `ColumnsStructs[col]` — one per column. Each holds an `InputValue` (property binding) + a `RowValues[row]` array of cells.
> - `ResultsStructs[row]` — one per row. Each holds an `IObjectChooser` impl: `FAssetChooser` / `FClassChooser` / `FNestedChooser` / `FEvaluateChooser` / `FSoftAssetChooser`.
> 
> Plus `DisabledRows[row]` (bool) and `FallbackResult` (used when no row passes filters). Row count = `len(ResultsStructs)`. **Adding/removing a row must keep all three arrays in lockstep** — this library does that for you; never poke the arrays directly.

---

## Read

### `get_chooser_info(chooser_table_path) → FBridgeCHTInfo`

```python
info = unreal.UnrealBridgeChooserLibrary.get_chooser_info(
    chooser_table_path='/Game/.../CHT_PoseSearchDatabases')
```

| Field | Meaning |
|---|---|
| `row_count`, `column_count` | Sizes. `row_count == 0` is legal (table only has fallback / static result). |
| `nested_chooser_count` | Embedded sub-CHTs (subobjects, not separate assets). |
| `output_class_path` | Result class (e.g. `/Script/PoseSearch.PoseSearchDatabase`). |
| `result_type` | `"ObjectResult"` / `"ClassResult"` / `"NoPrimaryResult"`. |
| `has_fallback`, `fallback` | `fallback.kind` + `fallback.result_path` if set. |

### `list_chooser_columns(chooser_table_path) → TArray<FBridgeCHTColumn>`

| Field | Meaning |
|---|---|
| `kind` | C++ struct short name: `"FloatRangeColumn"`, `"EnumColumn"`, `"BoolColumn"`, `"ObjectColumn"`, `"GameplayTagColumn"`, `"RandomizeColumn"`, `"OutputFloatColumn"`, `"OutputObjectColumn"`, …. |
| `binding_path` | Dot-joined property chain, e.g. `"MovementMode"` or `"Player.Stats.Health"`. Empty when bound-to-root or column type doesn't bind (e.g. RandomizeColumn). |
| `display_name` | Editor display string from the binding (often friendlier than `binding_path`). |
| `is_output` | `True` for the `Output*` family — these *write* to the eval context, they don't filter. |
| `disabled` | Authored disabled flag. |

### `list_chooser_rows(chooser_table_path) → TArray<FBridgeCHTRow>`

| Field | Meaning |
|---|---|
| `disabled` | From `DisabledRows[row]`. |
| `result.kind` | `"Asset"` / `"SoftAsset"` / `"Class"` / `"NestedChooser"` / `"EvaluateChooser"` / `"None"` / `"<other>"`. |
| `result.result_path` | Asset / class / chooser path. **Empty** for `"None"`. |

> **NestedChooser path convention:** `:Name` suffix means subobject. `/Game/.../CHT_X.CHT_X:Stand Walks` is a `UChooserTable` *subobject* embedded inside `CHT_X`. They are NOT separate assets — they live in the parent's package. To inspect one, use the full path including the subobject suffix in any of the read calls.

### `get_chooser_row_result(chooser_table_path, row_index) → FBridgeCHTRowResult`

Single-row variant of the above.

### `get_chooser_cell_raw(chooser_table_path, column_index, row_index) → FString`

T3D export of one cell — useful when you need to read a `FloatRange` or `Enum` value programmatically. Format matches `unreal.<StructName>` export, e.g. `"(Min=0.0,Max=10.0,bNoMin=False,bNoMax=False)"`.

The library uses each column's `RowValuesPropertyName()` virtual to find the right array — most columns use `"RowValues"` but Output / Randomize variants use different names. You don't need to care; the read call uses the right one.

### `list_possible_results(chooser_table_path) → TArray<FBridgeCHTRowResult>`

Every row's result + fallback (kind prefixed `"Fallback:"`). **Useful for "what could this CHT possibly return?"** audits without simulating evaluation. No filter inputs are considered.

---

## Row writes

> Every row write keeps the three parallel arrays (`ColumnsStructs[*].RowValues`, `ResultsStructs`, `DisabledRows`) in sync via the column base virtuals (`SetNumRows` / `InsertRows` / `DeleteRows`). Don't hand-edit.

### `add_chooser_row(chooser_table_path) → int`

Append at end. Returns new row index.

### `insert_chooser_row(chooser_table_path, before_row) → int`

`before_row = -1` or `>= row_count` ⇒ append.

### `remove_chooser_row(chooser_table_path, row_index) → bool`

### `set_chooser_row_disabled(chooser_table_path, row_index, disabled) → bool`

### Set the result of a row

```python
# Hard reference to an asset (FAssetChooser)
unreal.UnrealBridgeChooserLibrary.set_chooser_row_result_asset(
    chooser_table_path='...', row_index=2,
    asset_path='/Game/.../PSD_Foo')

# Class output (FClassChooser; only valid when result_type=ClassResult)
unreal.UnrealBridgeChooserLibrary.set_chooser_row_result_class(
    chooser_table_path='...', row_index=2,
    class_path='/Game/.../BP_Enemy.BP_Enemy_C')

# Delegate to another CHT asset (FEvaluateChooser)
unreal.UnrealBridgeChooserLibrary.set_chooser_row_result_evaluate_chooser(
    chooser_table_path='...', row_index=2,
    sub_chooser_path='/Game/.../CHT_SubTable')

# Clear (returns null at runtime, falls back to FallbackResult if set)
unreal.UnrealBridgeChooserLibrary.clear_chooser_row_result(
    chooser_table_path='...', row_index=2)
```

> **There's no `set_row_result_nested_chooser` for *embedded* subobjects yet.** Embedded `FNestedChooser` rows reference subobject `UChooserTable` instances created by the editor's "+ Nested Chooser" UI. To author those programmatically you'd need an additional UFUNCTION wrapping `UChooserTable::AddNestedChooser(NewObject<UChooserTable>(...))`. Today, the only way to write a NestedChooser row is to create the subobject in the editor first, then point a row at it via `set_chooser_row_result_evaluate_chooser` (FEvaluateChooser, not FNestedChooser — same effect for hard-referenced sub-CHT, different storage).

### Fallback

```python
unreal.UnrealBridgeChooserLibrary.set_chooser_fallback_asset(chooser_table_path='...', asset_path='/Game/.../FallbackAsset')
unreal.UnrealBridgeChooserLibrary.clear_chooser_fallback(chooser_table_path='...')
```

---

## Column writes

Typed adds for the common 8 + a generic by-struct-path fallback. `binding_property_chain` is dot-joined (`"Foo.Bar"`); `context_index` is the slot in `ContextData` to bind against (almost always `0`).

| UFUNCTION | Column class | When to use |
|---|---|---|
| `add_chooser_column_float_range` | `FFloatRangeColumn` | Float input, row matches if value ∈ `[Min, Max]`. |
| `add_chooser_column_enum` | `FEnumColumn` | Enum input. **Pass `enum_path`** (`"/Script/Engine.E…"` for native, `"/Game/.../E_Foo.E_Foo"` for UserDefinedEnum). |
| `add_chooser_column_bool` | `FBoolColumn` | Bool input, exact match. |
| `add_chooser_column_object` | `FObjectColumn` | UObject input, identity match. |
| `add_chooser_column_gameplay_tag` | `FGameplayTagColumn` | GameplayTag input, hierarchy match. |
| `add_chooser_column_randomize` | `FRandomizeColumn` | Adds a randomization weight column (always last in editor by convention). |
| `add_chooser_column_output_float` | `FOutputFloatColumn` | Writes a float into the eval context when the row passes. |
| `add_chooser_column_output_object` | `FOutputObjectColumn` | Writes a UObject into the eval context. |
| `add_chooser_column_by_struct_path` | any `FChooserColumnBase` | Generic — pass `column_struct_path="/Script/Chooser.MultiEnumColumn"` etc. |

All return the new column index, or `-1` on failure.

```python
col = unreal.UnrealBridgeChooserLibrary.add_chooser_column_enum(
    chooser_table_path='...',
    binding_property_chain='Stance',
    enum_path='/Game/Blueprints/Data/E_Stance.E_Stance',
    context_index=0,
)
```

### `remove_chooser_column(chooser_table_path, column_index) → bool`

### `set_chooser_column_disabled(chooser_table_path, column_index, disabled) → bool`

### `set_chooser_cell_raw(chooser_table_path, column_index, row_index, t3d_value) → bool`

The **only** way to write a single cell. Pass T3D text matching the column's per-cell struct:

| Column | Cell struct | Example T3D |
|---|---|---|
| FloatRange | `FChooserFloatRangeRowData` | `"(Min=0.0,Max=10.0)"` (omit defaults; `bNoMin`/`bNoMax` only when unbounded) |
| Enum | `FChooserEnumRowData` | `"(ValueName=\"E_Gait::NewEnumerator2\",Value=2,Comparison=MatchEqual)"` for a hard match; `"(Comparison=MatchAny)"` to wildcard |
| Bool | (enum) | **NOT a struct** — bare enum text: `"MatchTrue"` / `"MatchFalse"` / `"MatchAny"`. Default-init cells return `"MatchFalse"`. Setting `(Value=True)` does NOT work. |
| Object | `FChooserObjectRowData` | `"(Value=Asset'/Game/…/Foo.Foo')"` |

> **Two non-obvious gotchas, both empirically verified:**
> - **Bool cells are bare enum text, not a struct.** Pass `"MatchTrue"` / `"MatchFalse"` / `"MatchAny"` directly. The cell's Python type is `EChooserBoolColumnCellValueComparison`, not a struct with a `Value` field.
> - **Enum `MatchAny` cells need explicit `(Comparison=MatchAny)`** — leaving the cell at default (empty `()`) compares against int `0` (whatever the first enum value is), NOT "skip this column". This is the single most common authoring bug. Always write the wildcard explicitly.

To get the exact T3D format for a cell shape, read an existing one with `get_chooser_cell_raw` and edit the string. Round-trip works.

---

## Evaluation

### `evaluate_chooser_with_context_object(chooser_table_path, context_object_path) → FBridgeCHTEvaluation`

```python
out = unreal.UnrealBridgeChooserLibrary.evaluate_chooser_with_context_object(
    chooser_table_path='/Game/.../CHT_PoseSearchDatabases',
    context_object_path='',  # empty → no context object, fires default rows only
)
print(out.succeeded, out.result_path, out.matched_row, out.used_fallback)
```

Loads `context_object_path` (if non-empty) and adds it as the only `FChooserEvaluationInputObject`. The chooser's column bindings are resolved against that object's properties. Pass an empty string to test "what happens when nothing is bound" — the chooser falls through to whatever default-row each column matches.

| Field | Meaning |
|---|---|
| `succeeded` | True iff the eval picked an asset (fallback counts as success). |
| `result_path` | Picked asset's path. |
| `result_kind` | UClass short name of the picked object. |
| `matched_row` | **Only meaningful when the CHT has `bEnableDebugTesting=True`** (editor-only flag). Otherwise `-1` even on success — trust `result_path` to know what fired. |
| `used_fallback` | Heuristic: true when matched_row is -1 and a fallback exists. |

> **`matched_row=-1` doesn't mean failure.** It means "I didn't record which row" — happens by default. To get authoritative row tracking you'd need to flip `bEnableDebugTesting` on the chooser asset (no UFUNCTION yet, requires editor inspector or a follow-up bridge call).

> **Choosers with property-bag context can't be fully tested via context-object alone.** A CHT bound to a property bag wants `FChooserEvaluationContext.Params` populated with `FStructView`s. The current `evaluate_chooser_with_context_object` only handles the single-UObject case. For property-bag choosers, the agent's pragmatic option today is: read `list_chooser_columns` (binding paths), construct a small UObject *whose property names match* those bindings, and pass that object's path. If that's not feasible, the chooser can only be audited statically via `list_chooser_rows` + `list_possible_results`.

---

## Pitfalls

- **NestedChooser row paths use `:` not `/`.** `CHT_X:Stand Walks` is a subobject inside `CHT_X`, not a separate asset. They show up in `list_chooser_rows` results with `kind="NestedChooser"` and a `:Name` path — pass that exact string back to other chooser calls.
- **`ResultsStructs` / `DisabledRows` / `NestedChoosers` are `private:` and unreachable from Python `get_editor_property`.** Any code that tries to read them via reflection will fail silently (returns None) — always go through this library.
- **Column adds default-init the cell array.** `set_chooser_cell_raw` is mandatory if you want anything other than zero/empty per row. Forgetting this is the #1 way to ship a chooser that "always picks row 0".
- **Saving is not automatic.** Mark the asset dirty? Yes. Save? No. `unreal.EditorAssetLibrary.save_loaded_asset(cht)` to commit.
- **Don't write column or row indexes that came from a stale read.** Insert/remove ops shift indexes — re-fetch `list_chooser_rows` between mutations. The library doesn't track invalidation across calls.
- **Property bindings are case- and path-sensitive.** `add_chooser_column_enum(binding_property_chain="movementMode", ...)` won't bind to `MovementMode`. Match the casing of the UPROPERTY exactly.

---

## Audit recipes

### Reverse lookup: which CHTs reference asset X

`Asset.get_package_referencers(package_name='/Game/.../X')` from `bridge-asset-api.md` works fine — chooser column row values store soft refs that the asset registry sees. Filter the result by `class_path == "/Script/Chooser.ChooserTable"`.

### Diff two CHTs

There's no built-in diff. Recipe:

```python
from unreal_bridge import Chooser
def snapshot(p):
    info = Chooser.get_chooser_info(chooser_table_path=p)
    cols = Chooser.list_chooser_columns(chooser_table_path=p)
    rows = Chooser.list_chooser_rows(chooser_table_path=p)
    return info, cols, rows

a, b = snapshot('/Game/A'), snapshot('/Game/B')
# Compare sizes, kinds, binding paths, row results
```

### Visualize the LOD chooser fan-out

For GASP-style "entry CHT routes by LOD into per-density sub-CHTs":

```python
# Top-level
top = Chooser.list_chooser_rows(chooser_table_path='/Game/.../CHT_PoseSearchDatabases')
for r in top:
    if r.result.kind in ('EvaluateChooser', 'NestedChooser'):
        sub_rows = Chooser.list_chooser_rows(chooser_table_path=r.result.result_path)
        # ... recurse
```

`list_possible_results` flattens this one level for quick "what does this CHT route to" overviews.

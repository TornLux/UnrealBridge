# UnrealBridge DataTable Library API

Module: `unreal.UnrealBridgeDataTableLibrary`

## Cheap reads (prefer these first)

Before dumping full rows, use the cheap variants — `get_data_table_rows` and `get_data_table_rows_filtered` (with both filters empty) can be **very expensive** on tables with nested structs.

### get_data_table_summary(data_table_path) -> FBridgeDataTableInfo

Schema only: name, row struct, num rows, column names. No row data.

```python
s = unreal.UnrealBridgeDataTableLibrary.get_data_table_summary('/Game/Data/DT_Weapons')
print(f'{s.name} ({s.row_struct_name}) rows={s.num_rows} cols={list(s.column_names)}')
```

### get_data_table_row_names(data_table_path) -> list[str]

Just the row keys.

### get_data_table_row(data_table_path, row_name) -> FBridgeDataTableRow

A single row (empty `fields` if not found).

### get_data_table_row_field(data_table_path, row_name, field_name) -> str

A single field value as export-text (empty if not found).

### get_data_table_column(data_table_path, field_name) -> list[str]

One column across all rows, as `"RowName = Value"` entries.

### search_data_table_rows(data_table_path, keyword, column_filter) -> list[str]

Case-insensitive substring search. `column_filter=[]` searches all columns. Returns matching row names — use `get_data_table_row` after to fetch details.

```python
hits = unreal.UnrealBridgeDataTableLibrary.search_data_table_rows(
    '/Game/Data/DT_Weapons', 'sword', ['Name', 'Description']
)
```

---

## Full table reads (⚠️ high token cost)

### get_data_table_rows(data_table_path) -> FBridgeDataTableInfo

Dumps the entire table including all nested struct text. Avoid unless needed.

```python
info = unreal.UnrealBridgeDataTableLibrary.get_data_table_rows('/Game/Data/DT_Weapons')
for row in info.rows:
    print(f'  {row.row_name}: {list(row.fields)}')
```

### get_data_table_rows_filtered(data_table_path, row_filter, column_filter) -> FBridgeDataTableInfo

Filtered view. `row_filter=[]` = all rows; `column_filter=[]` = all columns. With **both** empty this equals `get_data_table_rows` — always pass at least one filter.

```python
view = unreal.UnrealBridgeDataTableLibrary.get_data_table_rows_filtered(
    '/Game/Data/DT_Weapons', ['Sword_01','Sword_02'], ['Damage','Range']
)
```

### FBridgeDataTableInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | DataTable asset name |
| `row_struct_name` | str | Row struct type name |
| `num_rows` | int | Number of rows |
| `column_names` | list[str] | Property names of the row struct |
| `rows` | list[FBridgeDataTableRow] | Row data (empty for summary) |

### FBridgeDataTableRow fields

| Field | Type | Description |
|-------|------|-------------|
| `row_name` | str | Row key name |
| `fields` | list[str] | Values as `"PropertyName = Value"` strings |

---

## Cross-asset lookup

### get_data_tables_using_struct(row_struct_name) -> list[str]

Find all DataTable asset paths that use the given row struct (by struct name).

```python
tables = unreal.UnrealBridgeDataTableLibrary.get_data_tables_using_struct('WeaponData')
```

---

## Write Operations

All writes are undoable (wrapped in a scoped transaction).

### set_data_table_row_field(data_table_path, row_name, field_name, exported_value) -> bool

Set a single field from an export-text value.

```python
unreal.UnrealBridgeDataTableLibrary.set_data_table_row_field(
    '/Game/Data/DT_Weapons', 'Sword_01', 'Damage', '42.0'
)
```

### add_data_table_row(data_table_path, row_name, field_values) -> bool

Add a new row. `field_values` is a dict of `FieldName -> exported-text`. Unspecified fields keep struct defaults.

```python
unreal.UnrealBridgeDataTableLibrary.add_data_table_row(
    '/Game/Data/DT_Weapons', 'Sword_03',
    {'Damage': '25.0', 'Name': '"Iron Sword"'}
)
```

### remove_data_table_row(data_table_path, row_name) -> bool

### duplicate_data_table_row(data_table_path, source_row_name, new_row_name) -> bool

### rename_data_table_row(data_table_path, old_row_name, new_row_name) -> bool

### reorder_data_table_rows(data_table_path, ordered_names) -> bool

Reorder rows to match `ordered_names`. Names not listed are left in their current order at the end.

---

## CSV Import / Export

### export_data_table_to_csv(data_table_path, out_csv_file_path) -> bool

Export to a CSV file (absolute filesystem path).

### import_data_table_from_csv(data_table_path, csv_file_path) -> bool

Import from a CSV file (absolute path). **Replaces** existing rows.

---

## Extra reads / bulk write / JSON IO

### does_data_table_row_exist(data_table_path, row_name) -> bool

Cheap existence check — no field data returned.

```python
if unreal.UnrealBridgeDataTableLibrary.does_data_table_row_exist('/Game/Data/DT_Weapons', 'Sword_01'):
    ...
```

### set_data_table_row_fields(data_table_path, row_name, field_values) -> bool

Atomically set multiple fields on an existing row from an export-text dict. Unknown fields are ignored. One scoped transaction covers the whole change. Returns `True` if the row existed **and** at least one field was updated.

```python
unreal.UnrealBridgeDataTableLibrary.set_data_table_row_fields(
    '/Game/Data/DT_Weapons', 'Sword_01',
    {'Damage': '50.0', 'Range': '120.0', 'Name': '"Steel Sword"'}
)
```

### get_data_table_as_json_string(data_table_path) -> str

Return the full DataTable as a pretty-printed JSON string. Empty string on failure.

### export_data_table_to_json(data_table_path, out_json_file_path) -> bool

Export to a JSON file (absolute filesystem path).

### import_data_table_from_json(data_table_path, json_file_path) -> bool

Import rows from a JSON file (absolute path). **Replaces** existing rows. Returns `False` if the file is unreadable or the importer reports errors.

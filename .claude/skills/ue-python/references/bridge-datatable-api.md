# UnrealBridge DataTable Library API

Module: `unreal.UnrealBridgeDataTableLibrary`

## Get DataTable Rows

### get_data_table_rows(data_table_path) -> FBridgeDataTableInfo

Get all rows from a DataTable with their field values.

```python
info = unreal.UnrealBridgeDataTableLibrary.get_data_table_rows('/Game/Data/DT_Weapons')
print(f'{info.name} ({info.row_struct_name}): {info.num_rows} rows')
print(f'Columns: {list(info.column_names)}')
for row in info.rows:
    print(f'  {row.row_name}:')
    for field in row.fields:
        print(f'    {field}')
```

### FBridgeDataTableInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | DataTable asset name |
| `row_struct_name` | str | Row struct type name |
| `num_rows` | int | Number of rows |
| `column_names` | list[str] | Property names of the row struct |
| `rows` | list[FBridgeDataTableRow] | Row data |

### FBridgeDataTableRow fields

| Field | Type | Description |
|-------|------|-------------|
| `row_name` | str | Row key name |
| `fields` | list[str] | Values as "PropertyName = Value" strings |

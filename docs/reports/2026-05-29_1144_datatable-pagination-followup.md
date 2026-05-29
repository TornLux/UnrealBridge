# DataTable pagination follow-up validation

时间：2026-05-29 11:44

分支：`codex/mcp-pagination-followups`

## 变更

- 新增 `bridge_datatable_row_names_page`。
- 新增 `bridge_datatable_search_rows_page`。
- 两个工具均只返回 row names / search hits，不鼓励整表导出。
- 复用现有 stdio MCP wrapper、`bridge.py exec --stdin`、cursor / stale cursor / page envelope 机制。
- 未新增 UE 侧 transport，未新增 C++ 代码。

## 验证

```powershell
python tools\smoke_mcp_stdio.py
# MCP stdio smoke passed

python tools\run_mcp_stdio_fixture.py
# MCP stdio fixture passed

python tools\smoke_mcp_pagination.py
# ok: DataTable pagination tools are listed and handler smoke passes

python tools\smoke_output_spill.py
# Output spill smoke passed

python -m py_compile ...
# passed

git diff --check
# passed
```

Live read-only smoke against `TEST_57` / UE 5.7.4:

```json
{
  "has_get_data_table_row_names": true,
  "has_search_data_table_rows": true,
  "data_tables": []
}
```

MCP stdio live tool smoke against a missing DataTable path:

```json
{
  "init_ok": true,
  "tool_is_error": false,
  "ok": true,
  "returned": 0,
  "has_more": false
}
```

## 备注

- `TEST_57` 当前没有 DataTable 资产，因此本轮 live smoke 验证的是 API 暴露和安全空结果路径。
- 有真实大表后，应补第一页 / 第二页 / stale cursor 的 live smoke。
- 当时该 follow-up 仍处于本地分支阶段；当前状态见下方更新。

## 2026-05-29 状态更新

- 该 follow-up 已不再作为单独分支提交，当前已并入 `TornLux/UnrealBridge#2` 的最终合并 PR 范围。
- 当前提交分支：`codex/mcp-stdio-wrapper-pr`。
- 备份分支：`codex/mcp-pagination-followups`。

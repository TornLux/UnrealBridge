# SearchableName pagination follow-up validation

时间：2026-05-29 11:40

分支：`codex/mcp-pagination-followups`

## 变更

- 新增 `bridge_searchable_name_values_page`。
- 新增 `bridge_assets_referencing_searchable_name_page`。
- 两个工具均复用现有 stdio MCP wrapper、`bridge.py exec --stdin`、cursor / stale cursor / page envelope 机制。
- 未新增 UE 侧 transport，未新增 C++ 代码。

## 验证

```powershell
python tools\smoke_mcp_stdio.py
# MCP stdio smoke passed

python tools\run_mcp_stdio_fixture.py
# MCP stdio fixture passed

python tools\smoke_mcp_pagination.py
# ok: new SearchableName pagination tools are listed and handler smoke passes

python tools\smoke_output_spill.py
# Output spill smoke passed

python -m py_compile ...
# passed

git diff --check
# passed
```

Live read-only smoke against `TEST_57` / UE 5.7.4:

```powershell
python .claude\skills\unreal-bridge\scripts\bridge.py --json ping
# success: true, output: pong

python .claude\skills\unreal-bridge\scripts\bridge.py --json list-editors
# project: TEST_57, engine_version: 5.7.4-51494982+++UE5+Release-5.7
```

Unreal Python API smoke:

```json
{
  "has_list_searchable_name_values": true,
  "has_find_assets_referencing_searchable_name": true,
  "values_sample": [],
  "refs_sample": []
}
```

MCP stdio live tool smoke:

```json
{
  "init_ok": true,
  "tool_is_error": false,
  "ok": true,
  "returned": 0
}
```

## 备注

- `TEST_57` 当前没有返回 GameplayTag SearchableName 样本，但函数存在且 MCP handler 到 UnrealBridge 的只读调用链成功。
- 该 follow-up 分支尚未推送，不影响 `TornLux/UnrealBridge#2`。

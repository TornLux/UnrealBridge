# Blueprint audit pagination follow-up validation

时间：2026-05-29 11:48

分支：`codex/mcp-pagination-followups`

## 变更

- 新增 `bridge_blueprint_call_sites_page`。
- 新增 `bridge_blueprint_debug_prints_page`。
- 两个工具用于全项目 / 子目录 Blueprint 审计浏览，避免一次性返回大量 call sites 或 debug print sites。
- 复用现有 stdio MCP wrapper、`bridge.py exec --stdin`、cursor / stale cursor / page envelope 机制。
- 未新增 UE 侧 transport，未新增 C++ 代码。

## 验证

```powershell
python tools\smoke_mcp_stdio.py
# MCP stdio smoke passed

python tools\run_mcp_stdio_fixture.py
# MCP stdio fixture passed

python tools\smoke_mcp_pagination.py
# ok: Blueprint audit pagination tools are listed and handler smoke passes

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
  "has_find_function_call_sites_global": true,
  "has_find_blueprint_debug_prints": true,
  "call_sites_sample_count": 0,
  "debug_prints_sample_count": 0
}
```

MCP stdio live tool smoke:

```json
{
  "init_ok": true,
  "call_sites_error": false,
  "call_sites_ok": true,
  "call_sites_returned": 0,
  "debug_prints_error": false,
  "debug_prints_ok": true,
  "debug_prints_returned": 0
}
```

## 备注

- `TEST_57` 当前没有匹配的 `PrintString` / debug print audit 结果，因此本轮 live smoke 验证的是 API 暴露和安全空结果路径。
- 有真实 Blueprint audit 命中后，应补第一页 / 第二页 / stale cursor 的 live smoke。
- 该 follow-up 分支尚未推送，不影响 `TornLux/UnrealBridge#2`。

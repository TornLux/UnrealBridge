# UnrealBridge MCP stdio fixture 验证记录

时间：2026-05-29 11:03:42 +08:00

## 变更范围

- 新增 `tools/fixtures/mcp_stdio_common_probes.json`。
- 新增 `tools/run_mcp_stdio_fixture.py`。
- fixture 覆盖无需 Unreal Editor 的客户端探针：
  - `initialize`
  - `notifications/initialized`
  - `notifications/cancelled`
  - `ping`
  - `tools/list`
  - `bridge_preflight`
  - `resources/list`
  - `resources/templates/list`
  - `prompts/list`
  - `logging/setLevel`
  - JSON-RPC batch

## 验证

```powershell
python tools\run_mcp_stdio_fixture.py
python tools\smoke_mcp_stdio.py
```

结果：均通过。

备注：fixture 是 data-driven，后续如果某个 MCP 客户端新增标准探针或兼容性期望，只需要扩展 JSON fixture，runner 可复用。

# UnrealBridge output spill 验证记录

时间：2026-05-29 10:24:27 +08:00

## 变更范围

- `bridge.py exec` 支持 `--max-output-bytes` / `UNREAL_BRIDGE_MAX_OUTPUT_BYTES`。
- 超过上限的 `output` / `error` 字段会写入 spill 文件，inline 响应保留预览和 `spills` 元数据。
- MCP stdio wrapper 默认向 `bridge.py` 传递 `UNREAL_BRIDGE_MAX_OUTPUT_BYTES=262144`，并对最终 MCP text content 增加兜底上限。

## 验证

```powershell
python tools\smoke_output_spill.py
python tools\smoke_mcp_stdio.py
```

结果：均通过。

Live 样本：

```powershell
"print('x' * 200)" | python .claude\skills\unreal-bridge\scripts\bridge.py --json --max-output-bytes 64 --spill-dir C:\tmp\UnrealBridgeOutputSpillLive_1019 exec --stdin
```

结果：通过。inline `output` 保留 64 字节预览，完整 200 字节输出写入
`C:\tmp\UnrealBridgeOutputSpillLive_1019\...\*_output.txt`，返回 JSON 包含
`spills.output.path` / `bytes` / `shown_bytes` / `truncated`。

MCP live 样本：

通过 `unrealbridge_mcp_server.py` 调用 `bridge_exec`，参数为
`max_output_bytes=64`、`spill_dir=C:\tmp\UnrealBridgeMcpOutputSpillLive_1025`，
脚本内容为 `print('y' * 200)`。

结果：通过。MCP tool 返回 `isError=false`，内部 bridge JSON 包含
`spills.output`，spill 文件存在，`bytes=200`，`shown_bytes=64`。

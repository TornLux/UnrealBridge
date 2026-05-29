# UnrealBridge MCP cursor pagination 验证记录

时间：2026-05-29 10:44:58 +08:00

## 变更范围

- `unrealbridge_mcp_server.py` 新增 cursor helper：参数哈希、opaque cursor、页大小上限、stale cursor 错误。
- 新增 MCP tools：
  - `bridge_search_assets_page`
  - `bridge_list_actors_page`
- 分页工具复用现有 UnrealBridge UFUNCTION，不新增 Unreal 侧传输协议，不改 TCP bridge。

## 无引擎验证

```powershell
python tools\smoke_mcp_pagination.py
python tools\smoke_mcp_stdio.py
```

结果：均通过。

覆盖点：

- cursor 可解码并恢复 offset。
- `page_size` 被限制在允许范围内。
- 查询参数变化时复用旧 cursor 返回 `STALE_CURSOR`。
- MCP `tools/list` 包含两个分页工具。

## Live 验证

通过 `unrealbridge_mcp_server.py` 调用：

- `bridge_list_actors_page`，`page_size=2`
- 使用返回的 `next_cursor` 再取第二页
- `bridge_search_assets_page`，`query=M_`，`scope=PROJECT`，`page_size=2`
- 修改 query 后复用旧 asset cursor

结果：通过。

摘要：

```json
{
  "actor_is_error": false,
  "actor_returned": 2,
  "actor_has_more": true,
  "actor_second_is_error": false,
  "actor_second_offset": 2,
  "asset_is_error": false,
  "asset_returned": 2,
  "asset_has_more": true,
  "stale_is_error": true,
  "stale_code": "STALE_CURSOR"
}
```

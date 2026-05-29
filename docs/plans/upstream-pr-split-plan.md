# UnrealBridge upstream PR 拆分方案

更新：2026-05-29

本文件用于把当前工作树整理成作者容易 review 的提交顺序。目标是保留完整证据链，同时避免把 MCP wrapper、exec timeout、输出保护、分页工具和 StrictIncludes 混成一个难审的大提交。

## 当前本地提交

当前分支：`codex/mcp-stdio-wrapper-pr`

实际已拆成 6 个本地提交：

1. `9289668 fix(cli): keep preflight JSON warnings separate`
2. `3a6fa99 fix(server): cancel queued exec after client timeout`
3. `67a11f0 feat(cli): spill oversized bridge output to files`
4. `077b5e1 feat(mcp): add stdio wrapper and paginated bridge tools`
5. `cc02854 build: satisfy UE 5.7 StrictIncludes`
6. `docs: record MCP wrapper validation and PR plan`

说明：MCP wrapper 文件本身已经包含 MCP content cap、bridge output cap 透传和分页工具，因此实际提交中把 MCP wrapper 与分页工具合并成一个可审单元，避免为了形式拆分而制造中间不可用状态。

当前 Draft PR：`https://github.com/TornLux/UnrealBridge/pull/2`

## 建议提交顺序

### 1. fix(cli): keep preflight JSON warnings separate

范围：

- `.claude/skills/unreal-bridge/scripts/bridge.py`
- `tools/smoke_mcp_stdio.py` 中依赖该行为的断言

内容：

- 修复 `bridge.py --json preflight` 返回结构，确保 `errors` 与 `warnings` 都是独立数组。
- 保持 stdout 为可解析 JSON，错误仍通过非零 exit code 表达。

验证：

```powershell
"print('ok')" | python .claude\skills\unreal-bridge\scripts\bridge.py --json preflight -
python tools\smoke_mcp_stdio.py
```

### 2. feat(mcp): add stdio MCP wrapper around bridge.py

范围：

- `.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py`
- `tools/smoke_mcp_stdio.py`
- `tools/fixtures/mcp_stdio_common_probes.json`
- `tools/run_mcp_stdio_fixture.py`
- `.claude/skills/unreal-bridge/SKILL.md`
- `README.md`
- `README.zh-CN.md`
- `docs/mcp-stdio-wrapper.md`
- `docs/plans/mcp-stdio-wrapper-roadmap.md`

内容：

- 新增 client-neutral stdio MCP wrapper。
- wrapper 只调用 `bridge.py`，不新增 Unreal 侧 HTTP/server/transport。
- token 只走环境变量，不作为 tool 参数暴露。
- 支持常见 MCP probes：`initialize`、`ping`、`tools/list`、`resources/list`、`resources/templates/list`、`prompts/list`、`logging/setLevel`、batch。

验证：

```powershell
python tools\smoke_mcp_stdio.py
python tools\run_mcp_stdio_fixture.py
```

### 3. fix(server): cancel queued exec after client timeout

范围：

- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeServer.h`
- `Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeServer.cpp`

内容：

- 当客户端 timeout 时标记尚未执行的 pending exec。
- GameThread ticker 在 Python 执行前跳过已取消的 pending exec。
- 已经开始执行的 Python 不尝试中断，仍保持 client-side timeout 语义。

验证：

```powershell
RunUAT.bat BuildPlugin -Plugin=Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeBuild -TargetPlatforms=Win64
```

live 验证见：

- `docs/reports/2026-05-29_0947_mcp-stdio-wrapper-pr-live-test.md`

### 4. feat(cli): spill oversized bridge output to files

范围：

- `.claude/skills/unreal-bridge/scripts/bridge.py`
- `.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py`
- `tools/smoke_output_spill.py`
- `.claude/skills/unreal-bridge/SKILL.md`
- `README.md`
- `README.zh-CN.md`
- `docs/mcp-stdio-wrapper.md`
- `docs/plans/mcp-stdio-wrapper-roadmap.md`

内容：

- `bridge.py exec` 增加 `--max-output-bytes` / `--spill-dir`。
- 超大 `output` / `error` 写入 spill 文件，inline 响应保留预览和 `spills` 元数据。
- MCP wrapper 默认设置保守输出上限，并对最终 MCP text content 做兜底保护。

验证：

```powershell
python tools\smoke_output_spill.py
python tools\smoke_mcp_stdio.py
```

live 验证见：

- `docs/reports/2026-05-29_1024_output-spill-validation.md`

### 5. feat(mcp): add cursor-paginated asset and actor tools

范围：

- `.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py`
- `tools/smoke_mcp_pagination.py`
- `tools/smoke_mcp_stdio.py`
- `.claude/skills/unreal-bridge/SKILL.md`
- `README.md`
- `README.zh-CN.md`
- `docs/mcp-stdio-wrapper.md`
- `docs/plans/mcp-stdio-wrapper-roadmap.md`

内容：

- 新增 opaque cursor helper。
- 新增 MCP tools：
  - `bridge_search_assets_page`
  - `bridge_list_actors_page`
- cursor 与原始查询参数绑定，参数变化复用旧 cursor 返回 `STALE_CURSOR`。

验证：

```powershell
python tools\smoke_mcp_pagination.py
python tools\smoke_mcp_stdio.py
```

live 验证见：

- `docs/reports/2026-05-29_1045_mcp-pagination-validation.md`

### 6. build: satisfy UE 5.7 StrictIncludes

范围：

- `Plugin/UnrealBridge/UnrealBridge.uplugin`
- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeServer.h`
- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeReactiveSubsystem.h`
- `Plugin/UnrealBridge/Source/UnrealBridge/Private/*.cpp` 中 StrictIncludes 暴露的 include 补齐

内容：

- 显式声明 `StructUtils` 插件依赖。
- 补齐直接使用类型所需 include。
- 不改变运行时行为。

验证：

```powershell
RunUAT.bat BuildPlugin -Plugin=Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeBuild57Loose -TargetPlatforms=Win64
RunUAT.bat BuildPlugin -Plugin=Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeBuild57Strict -TargetPlatforms=Win64 -StrictIncludes
```

验证记录：

- `docs/reports/2026-05-29_1100_strict-includes-validation.md`

## PR 描述草稿

### Summary

This PR adds a thin stdio MCP wrapper for UnrealBridge while keeping the existing `bridge.py` client and Unreal-side TCP bridge as the single source of truth. It also fixes `preflight --json` shape, prevents queued exec requests from running after client timeout, adds output spill protection, introduces cursor-paginated MCP tools for broad asset/actor queries, and keeps UE 5.7 StrictIncludes green.

### Design notes

- No HTTP server is added.
- No second Unreal-side transport is added.
- MCP tools delegate to `bridge.py`, preserving UDP discovery, token handling, AST preflight, audit logging, and length-prefixed TCP execution.
- Secrets stay in the MCP server environment; `UNREAL_BRIDGE_TOKEN` is not exposed as a tool argument.
- The wrapper keeps a small tool surface and adds paginated coarse-grained tools only where broad result sets are common.

### Validation

```powershell
python tools\smoke_mcp_stdio.py
python tools\run_mcp_stdio_fixture.py
python tools\smoke_output_spill.py
python tools\smoke_mcp_pagination.py
RunUAT.bat BuildPlugin -Plugin=Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeBuild57Loose -TargetPlatforms=Win64
RunUAT.bat BuildPlugin -Plugin=Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeBuild57Strict -TargetPlatforms=Win64 -StrictIncludes
```

Live validation reports are under `docs/reports/`.

## 注意事项

- `StructUtils` 在 UE 5.5+ 会输出 deprecation 提示；本 PR 只显式声明现有依赖，不处理未来替代迁移。
- UE 5.8 final validation 保持低优先级，继续放在路线图 P2。
- 如果维护者偏好小 PR，可按上面的 6 个提交拆成 2 到 4 个 PR。

# UnrealBridge upstream PR 拆分方案

更新：2026-05-29

本文件用于把当前工作树整理成作者容易 review 的提交顺序。目标是保留完整证据链，同时避免把 MCP wrapper、exec timeout、输出保护、分页工具、JSON-RPC 兼容性、客户端配置生成和 StrictIncludes 混成一个难审的大提交。

## 当前状态

- 当前分支：`codex/mcp-stdio-wrapper-pr`
- 当前 PR：`https://github.com/TornLux/UnrealBridge/pull/2`
- PR 状态：Ready for review
- 当前 review 粒度：8 个可读提交
- PR 描述正文以 `docs/plans/upstream-pr-description.md` 为准。本文件只维护拆分和审阅分组，避免重复维护两份 PR body。

当前提交序列：

```text
cd32e47 fix(bridge): harden exec transport and UE builds
a9517a4 feat(mcp): add stdio wrapper and no-editor smoke
67e1ecf docs: record MCP roadmap and validation
3a58070 ci(mcp): include wrapper docs in workflow scope
aa0ac04 test(mcp): support combined scope validation
5e91226 fix(mcp): reject empty JSON-RPC batches
df99e7d fix(mcp): isolate tool handler failures
HEAD feat(mcp): add client config generator
```

说明：早期拆分方案曾按 6 个主题组织。当前 PR 已演进为 8 个 review 提交，新增了 CI scope、combined scope checker、JSON-RPC batch / handler isolation、`bridge.py mcp-config` 等后续修正。它们仍然按逻辑单元组织，不追求固定提交数量。

## 建议审阅顺序

### 1. Bridge / CLI reliability baseline

对应提交：

- `fix(bridge): harden exec transport and UE builds`

范围：

- `.claude/skills/unreal-bridge/scripts/bridge.py`
- `Plugin/UnrealBridge/Source/UnrealBridge/**`
- `Plugin/UnrealBridge/UnrealBridge.uplugin`

内容：

- 修复 `bridge.py --json preflight` 返回结构，确保 `errors` 与 `warnings` 都是独立数组。
- 当客户端 timeout 时标记尚未执行的 pending exec，GameThread ticker 在 Python 执行前跳过已取消请求。
- 增加 oversized output spill，避免超大 `output` / `error` 直接撑爆 inline 响应。
- 显式声明 UE 5.7 StrictIncludes 暴露的直接依赖和 includes。

主要验证：

```powershell
python tools\smoke_output_spill.py
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Loose_112427 -TargetPlatforms=Win64
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Strict_112624 -TargetPlatforms=Win64 -StrictIncludes
```

### 2. Stdio MCP wrapper and no-editor smoke

对应提交：

- `feat(mcp): add stdio wrapper and no-editor smoke`

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
- 支持常见 MCP probes：`initialize`、`ping`、`tools/list`、`resources/list`、`resources/templates/list`、`prompts/list`、`logging/setLevel`、batch、notification。
- 新增 cursor pagination 工具，覆盖 asset、actor、SearchableName / GameplayTag、DataTable、Blueprint audit 等大结果集读取场景。

主要验证：

```powershell
python tools\smoke_mcp_stdio.py
python tools\run_mcp_stdio_fixture.py
python tools\smoke_mcp_pagination.py
```

### 3. Roadmap, reports, and client-facing docs

对应提交：

- `docs: record MCP roadmap and validation`

范围：

- `docs/reports/**`
- `docs/plans/**`
- `README.md`
- `README.zh-CN.md`
- `.claude/skills/unreal-bridge/SKILL.md`

内容：

- 记录 live validation、BuildPlugin / StrictIncludes、pagination follow-up 和 final readiness。
- 明确当前 PR 使用现有 Unreal-side TCP bridge + Python stdio MCP wrapper，不引入 HTTP。
- 保留 UE 5.8 / EDA ToolsetRegistry 路线图，但不作为当前阻塞项。
- 记录 P1 pagination 已并入 PR #2，P2 Perf / render pagination 继续延期。

### 4. CI and scope guardrails

对应提交：

- `ci(mcp): include wrapper docs in workflow scope`
- `test(mcp): support combined scope validation`

范围：

- `.github/workflows/mcp-no-editor.yml`
- `tools/check_mcp_workflow.py`
- `tools/check_mcp_followup_scope.py`
- 相关测试和 allowlist

内容：

- 让 wrapper-facing docs 和 PR body draft 变化触发 no-editor MCP smoke。
- 增加 `combined` / `followup` 两种 scope checker 模式。
- 当前 #2 使用 `combined`，因为它有意包含 bridge/plugin reliability fixes 与 MCP wrapper。
- 后续 pagination-only 分支继续使用默认 `followup` guard，避免误引入 `Plugin/` 变化。

主要验证：

```powershell
python tools\check_mcp_workflow.py
python tools\test_check_mcp_workflow.py
python tools\check_mcp_followup_scope.py --mode combined --base origin/main
python tools\test_check_mcp_followup_scope.py
```

### 5. JSON-RPC compatibility hardening

对应提交：

- `fix(mcp): reject empty JSON-RPC batches`
- `fix(mcp): isolate tool handler failures`

范围：

- `.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py`
- `tools/fixtures/mcp_stdio_common_probes.json`
- `tools/test_mcp_server_protocol.py`
- `tools/smoke_mcp_stdio.py`

内容：

- 显式 `"id": null` 作为 request 处理，缺失 `id` 才作为 notification。
- 空 batch `[]` 返回 `-32600 Invalid Request`。
- notification-like methods 如果带 request id，则返回 `{}`，避免客户端挂起。
- 缺失 / 错误 `jsonrpc`、缺失 / 非字符串 `method`、非对象 `params` 都返回标准错误。
- `tools/call` handler 异常被隔离，保留原 request id，batch 后续请求继续响应。

主要验证：

```powershell
python tools\run_mcp_stdio_fixture.py
python tools\test_mcp_server_protocol.py
python tools\smoke_mcp_stdio.py
```

### 6. MCP client config generator

对应提交：

- `feat(mcp): add client config generator`

范围：

- `.claude/skills/unreal-bridge/scripts/bridge.py`
- `tools/test_bridge_mcp_config.py`
- README / docs / roadmap / reports

内容：

- 新增 `bridge.py mcp-config`。
- 输出 `generic`、`claude-desktop`、`cursor`、`codex`、`openclaw`、`hermes`
  对应的配置片段形态，并用 no-editor 测试覆盖 JSON / TOML / YAML 输出；不声明真实客户端端到端验收。
- 输出 project / endpoint / discovery-group 等 MCP server 环境变量。
- 拒绝打印 `--token`，保持 `UNREAL_BRIDGE_TOKEN` 留在 client env 或 secret store。

主要验证：

```powershell
python tools\test_bridge_mcp_config.py
python tools\smoke_mcp_all.py
```

## 总体验证入口

当前 no-editor 总入口：

```powershell
python tools\smoke_mcp_all.py
python tools\check_mcp_followup_scope.py --mode combined --base origin/main
git diff --check
```

UE 5.7 BuildPlugin / StrictIncludes 验证记录：

- `docs/reports/2026-05-29_1100_strict-includes-validation.md`
- `docs/reports/2026-05-29_1129_pr-final-readiness.md`

## 注意事项

- 本 PR 不新增 HTTP server。
- 本 PR 不新增第二套 Unreal-side transport。
- `StructUtils` 在 UE 5.5+ 会输出 deprecation 提示；本 PR 只显式声明现有依赖，不处理未来替代迁移。
- UE 5.8 final validation 保持低优先级，继续放在路线图 P2 / EDA future work。
- 如果维护者偏好更小 PR，可按上述审阅顺序拆成多 PR；当前提交序列已经按可读逻辑粒度整理，不追求固定提交数量。

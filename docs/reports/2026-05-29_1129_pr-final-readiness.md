# PR final readiness check

时间：2026-05-29 14:57

PR：`https://github.com/TornLux/UnrealBridge/pull/2`

## 状态

- 当前 PR 为 Ready for review，base 为 `TornLux/UnrealBridge:main`，head 为 `whysoslow/UnrealBridge:codex/mcp-stdio-wrapper-pr`。
- GitHub 重新计算后显示 `MERGEABLE`。
- 仓库当前没有 GitHub status checks。
- 本地工作树在最终复核时为干净状态；验证命令覆盖当前 PR 文件树。
- PR is kept as 7 readable review commits after the handler isolation follow-up.

## 当前提交序列

```text
cd32e47 fix(bridge): harden exec transport and UE builds
a9517a4 feat(mcp): add stdio wrapper and no-editor smoke
67e1ecf docs: record MCP roadmap and validation
3a58070 ci(mcp): include wrapper docs in workflow scope
aa0ac04 test(mcp): support combined scope validation
5e91226 fix(mcp): reject empty JSON-RPC batches
HEAD fix(mcp): isolate tool handler failures
```

## 验证

```powershell
python tools\smoke_mcp_all.py
# All no-editor MCP checks passed

python tools\check_mcp_tool_docs.py
# MCP tool docs match server registry (18 tools)

python tools\check_mcp_workflow.py
# MCP workflow check passed

git diff --check
# passed
```

`python tools\smoke_mcp_all.py` 覆盖：

- Python compile checks；
- MCP stdio smoke，包括 tool input schema shape；
- data-driven stdio client probe fixture；
- JSON-RPC error responses / response envelopes / notification no-response；
- required / unknown tool argument validation；
- clean shutdown / empty stderr；
- cursor pagination smoke；
- pagination helper failure-path tests；
- output spill smoke；
- exposed tool docs consistency and negative tests；
- no-editor workflow path-filter checks and negative tests；
- follow-up scope checker tests；
- all-smoke runner self-tests。

UE 5.7 BuildPlugin：

```powershell
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Loose_112427 -TargetPlatforms=Win64
# BUILD SUCCESSFUL
```

UE 5.7 StrictIncludes：

```powershell
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Strict_112624 -TargetPlatforms=Win64 -StrictIncludes
# BUILD SUCCESSFUL
```

Note: `cd32e47` is the only current PR commit that changes `Plugin/UnrealBridge` C++ / `.uplugin` files. Later commits only cover the MCP Python wrapper, docs, smoke tools, and CI guardrails, so the UE 5.7 BuildPlugin / StrictIncludes results still cover the current Unreal plugin source state.

## 备注

- 第一次普通 BuildPlugin 命令中 `-Package=$out` 被 UAT 当成字面量，误生成了 `C:\Program Files\Epic Games\UE_5.7\$out`；该临时目录已删除，随后用明确的 `-Package=C:\tmp\...` 参数重跑并通过。
- 构建仍提示 `StructUtils` 在 UE 5.5+ deprecated。这是既有依赖的未来迁移问题，不影响 UE 5.7 packaging / StrictIncludes 结果。
- UE 5.8 final 验证仍保持低优先级，不作为当前 PR 阻塞项。

## 2026-05-29 self-review update

- Fixed JSON-RPC request handling so a missing `id` is treated as a notification, while an explicit `"id": null` request still receives a response. This avoids hanging generic JSON-RPC clients that send a nullable request id.
- Added the explicit null-id request to `tools/fixtures/mcp_stdio_common_probes.json`.
- Re-ran `python tools\smoke_mcp_all.py` and `git diff --check`; both passed.
- Re-pushed `codex/mcp-stdio-wrapper-pr` and backup branch `codex/mcp-pagination-followups` with readable review commits.

## 2026-05-29 CI scope update

- Added `README.md`, `README.zh-CN.md`, and
  `docs/plans/upstream-pr-description.md` to the no-editor workflow path
  filters, so wrapper-facing docs and the PR body draft trigger the same MCP
  smoke guardrails after merge.
- Re-ran `python tools\check_mcp_workflow.py`,
  `python tools\test_check_mcp_workflow.py`, `python tools\smoke_mcp_all.py`,
  and `git diff --check`; all passed.

## 2026-05-29 scope-checker update

- Updated `tools/check_mcp_followup_scope.py` with explicit `combined` and
  `followup` modes. The current #2 PR uses `combined` because it intentionally
  includes bridge/plugin reliability fixes with the MCP wrapper; later
  pagination-only branches keep the default `followup` guard that forbids
  `Plugin/` changes.
- Re-ran `python tools\check_mcp_followup_scope.py --mode combined --base
  origin/main`, `python tools\check_mcp_followup_scope.py --base
  codex/mcp-stdio-wrapper-pr`, `python tools\test_check_mcp_followup_scope.py`,
  `python tools\smoke_mcp_all.py`, `python tools\check_mcp_workflow.py`, and
  `git diff --check`; all passed.

## 2026-05-29 JSON-RPC batch update

- Fixed empty JSON-RPC batch handling so `[]` returns `-32600 Invalid Request`
  instead of producing no response and potentially hanging generic clients.
- Added an empty-batch probe to
  `tools/fixtures/mcp_stdio_common_probes.json`.
- Fixed known notification-like methods sent with a request id so they return
  `{}` instead of producing no response.
- Added a notification-like request probe to
  `tools/fixtures/mcp_stdio_common_probes.json`.
- Fixed request `jsonrpc` validation so requests without `jsonrpc: "2.0"`
  return `-32600 Invalid Request`.
- Added a missing-jsonrpc probe to
  `tools/fixtures/mcp_stdio_common_probes.json`.
- Fixed request `method` validation so missing or non-string methods return
  `-32600 Invalid Request` instead of `method not found`.
- Added a missing-method probe to
  `tools/fixtures/mcp_stdio_common_probes.json`.
- Fixed request `params` validation so arrays are rejected instead of being
  silently treated as empty objects.
- Added a non-object params probe to
  `tools/fixtures/mcp_stdio_common_probes.json`.
- Re-ran `python tools\run_mcp_stdio_fixture.py`,
  `python tools\smoke_mcp_stdio.py`, `python tools\smoke_mcp_all.py`,
  `python tools\check_mcp_followup_scope.py --mode combined --base
  origin/main`, and `git diff --check`; all passed.

## 2026-05-29 handler isolation update

- Wrapped `tools/call` handler execution so unexpected handler exceptions
  return `-32603` with the original JSON-RPC request id instead of falling
  through to the outer process loop as an id-less error.
- Added `tools/test_mcp_server_protocol.py`, including a batch case that proves
  a failing tool reply does not prevent later batch items from receiving
  responses.
- Added the new protocol test to `python tools\smoke_mcp_all.py`, workflow path
  filters, and scope allowlists.
- Re-ran `python tools\test_mcp_server_protocol.py`,
  `python tools\check_mcp_workflow.py`,
  `python tools\test_check_mcp_workflow.py`,
  `python tools\check_mcp_followup_scope.py --mode combined --base
  origin/main`, `python tools\smoke_mcp_all.py`, and `git diff --check`; all
  passed.

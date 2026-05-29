# PR final readiness check

时间：2026-05-29 14:57

PR：`https://github.com/TornLux/UnrealBridge/pull/2`

## 状态

- 当前 PR 为 Ready for review，base 为 `TornLux/UnrealBridge:main`，head 为 `whysoslow/UnrealBridge:codex/mcp-stdio-wrapper-pr`。
- GitHub 重新计算后显示 `MERGEABLE`。
- 仓库当前没有 GitHub status checks。
- 本地工作树在最终复核时为干净状态；验证命令覆盖当前 PR 文件树。
- PR is kept as 8 readable review commits after the mcp-config follow-up.

## 当前提交序列

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

## 2026-05-29 mcp-config update

- Added `bridge.py mcp-config` for copyable stdio MCP client snippets:
  `generic`, `claude-desktop`, `cursor`, `codex`, `openclaw`, and `hermes`.
- The generator emits project / endpoint / discovery-group settings as MCP
  server environment variables and refuses to print `--token`, keeping
  `UNREAL_BRIDGE_TOKEN` in the client environment or secret store.
- Added `tools/test_bridge_mcp_config.py` to cover generated generic /
  Claude Desktop / Cursor / OpenClaw JSON shapes, Codex TOML shape, Hermes YAML
  shape, and token refusal behavior. This is no-editor snippet-shape coverage,
  not real external client launch coverage.
- Added the new test to `python tools\smoke_mcp_all.py`, workflow path filters,
  and scope allowlists.
- Re-ran `python tools\test_bridge_mcp_config.py`,
  `python tools\smoke_mcp_all.py`,
  `python tools\check_mcp_followup_scope.py --mode combined --base
  origin/main`, and `git diff --check`; all passed.

## 2026-05-29 roadmap sync update

- Updated `docs/plans/eda-integration-roadmap.md` so it no longer marks the MCP
  client config generator and 5.7 MCP shell path as unstarted. The roadmap now
  records that #2 covers the Python stdio wrapper and `bridge.py mcp-config`,
  while UE 5.8 EDA ToolsetRegistry integration remains low-priority future work.
- Removed stale `gen-config` naming and clarified the deferred UE-side MCP shell
  entries so the roadmap matches the current stdio wrapper + existing TCP bridge
  design.
- Added the EDA roadmap to the no-editor workflow path filters and the combined
  scope checker allowlist so future roadmap changes still run the same MCP
  guardrails.
- Updated the three pagination live-smoke reports so their earlier "branch not
  pushed" notes are explicitly historical and point to the current #2 folded PR
  status.
- Updated `docs/plans/mcp-pagination-followups.md` so its opening section no
  longer describes the P1 tools as a future separate follow-up after #2. The
  file now records that #2 includes all P1 pagination tools and keeps only P2
  Perf / render pagination as deferred future work.
- Reworded the individual P1 pagination tool entries from "implemented on the
  follow-up branch" to "folded into #2" so the plan no longer reads like a
  separate branch is still active.

## 2026-05-29 split-plan sync update

- Updated `docs/plans/upstream-pr-split-plan.md` from the early 6-commit Draft
  PR snapshot to the current #2 Ready-for-review state with 8 readable review
  commits.
- Reframed that file as the review-order / split-plan document and pointed the
  PR body to `docs/plans/upstream-pr-description.md` as the canonical source.
- Marked the older `docs/reports/2026-05-29_1107_upstream-pr-split-status.md`
  snapshot as historical so it no longer appears to contradict the current PR
  status.

## 2026-05-29 mcp-config coverage update

- Extended `tools/test_bridge_mcp_config.py` so every advertised
  `bridge.py mcp-config --client` shape is covered: `generic`,
  `claude-desktop`, `cursor`, `codex`, `openclaw`, and `hermes`.
- Added checks for discovery-group / project / endpoint environment propagation
  in the generated snippets and verified the refused `--token` value is not
  echoed to stdout or stderr.
- Clarified that this coverage validates emitted config shapes and secret
  handling only; OpenClaw, Hermes, Cursor, and Claude Desktop are not launched
  by the no-editor suite.
- Fixed the subcommand form `bridge.py mcp-config --token ...` so it reaches
  the same sanitized refusal path as the global `--token ... mcp-config` form
  instead of letting argparse echo the secret value as an unknown argument.
- Quoted Codex TOML server-name keys so names containing spaces or dots still
  parse as one `mcp_servers` entry instead of invalid TOML or unintended nested
  tables.
- Quoted Hermes YAML server-name keys so names containing YAML-sensitive
  characters such as `:` or `#` are emitted as one literal `mcp_servers` entry.
- Updated the Hermes example in `docs/mcp-stdio-wrapper.md` to match the quoted
  server-name key emitted by `bridge.py mcp-config --client hermes`.
- Added no-editor coverage for both global and subcommand `--token=...` forms
  so the token-refusal path stays sanitized for space-separated and equals
  syntax.

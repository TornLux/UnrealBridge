# Upstream PR split status

时间：2026-05-29 11:07

## 状态

- 已新增 `docs/plans/upstream-pr-split-plan.md`，把当前工作树拆成 6 个建议提交。
- 已在 roadmap 中把 PR 拆分方案标记为完成，并把该文件设为当前 review / submit 顺序。
- 已新增 `docs/plans/upstream-pr-description.md`，作为上游 PR body 草稿。
- 已创建本地分支 `codex/mcp-stdio-wrapper-pr`。
- 已实际拆成 6 个本地提交，见下方提交序列。
- 已创建上游 Draft PR：`https://github.com/TornLux/UnrealBridge/pull/2`。
- 已将提交作者修正为 GitHub noreply：`whysoslow <21312801+whysoslow@users.noreply.github.com>`。
- UE 5.8 final 验证继续保持低优先级，不进入当前 PR 的阻塞项。

## 当前建议顺序

1. `fix(cli): keep preflight JSON warnings separate`
2. `feat(mcp): add stdio MCP wrapper around bridge.py`
3. `fix(server): cancel queued exec after client timeout`
4. `feat(cli): spill oversized bridge output to files`
5. `feat(mcp): add cursor-paginated asset and actor tools`
6. `build: satisfy UE 5.7 StrictIncludes`

## 实际本地提交

```text
9289668 fix(cli): keep preflight JSON warnings separate
3a6fa99 fix(server): cancel queued exec after client timeout
67a11f0 feat(cli): spill oversized bridge output to files
077b5e1 feat(mcp): add stdio wrapper and paginated bridge tools
cc02854 build: satisfy UE 5.7 StrictIncludes
docs: record MCP wrapper validation and PR plan
```

MCP wrapper 与分页工具实际合并到同一提交，因为 wrapper 文件本身已经包含分页、MCP content cap 和 bridge output cap 透传；强拆会制造中间不可用状态。

## 后续检查

- [x] 运行 no-editor MCP / spill / pagination smoke。
- [x] 运行 Python compile check。
- [x] 用 `git status --short` 和 `git diff --stat` 复核工作树范围。

## 验证结果

```powershell
python tools\smoke_mcp_stdio.py
# MCP stdio smoke passed

python tools\run_mcp_stdio_fixture.py
# MCP stdio fixture passed: tools\fixtures\mcp_stdio_common_probes.json

python tools\smoke_output_spill.py
# Output spill smoke passed

python tools\smoke_mcp_pagination.py
# ok: bridge_search_assets_page / bridge_list_actors_page are listed

python -m py_compile ...
# passed
```

提交后复跑结果：

```powershell
git status --short --branch
# ## codex/mcp-stdio-wrapper-pr

python tools\smoke_mcp_stdio.py
# MCP stdio smoke passed

python tools\run_mcp_stdio_fixture.py
# MCP stdio fixture passed: tools\fixtures\mcp_stdio_common_probes.json

python tools\smoke_output_spill.py
# Output spill smoke passed

python tools\smoke_mcp_pagination.py
# ok: bridge_search_assets_page / bridge_list_actors_page are listed

python -m py_compile ...
# passed
```

## 工作树复核

- 已修改文件集中在 UnrealBridge skill / wrapper、plugin source、README 与 docs。
- `git diff --stat` 当前显示 17 个已跟踪文件变更：210 insertions, 7 deletions。
- 新增文件包括 MCP wrapper、smoke tests、fixture、docs 与 reports。
- `AGENTS.md` 是本地恢复说明，已加入 `.git/info/exclude`，保留在 PR 外。
- Git 输出了 LF/CRLF 提示；当前未作为阻塞项处理。

## PR 状态

- URL: `https://github.com/TornLux/UnrealBridge/pull/2`
- 类型：Ready for review
- Base: `TornLux/UnrealBridge:main`
- Head: `whysoslow/UnrealBridge:codex/mcp-stdio-wrapper-pr`
- 本地分支 tracking: `fork/codex/mcp-stdio-wrapper-pr`
- PR 当前无 GitHub status checks。
- 2026-05-29 11:29 已完成最终可审验证，见 `docs/reports/2026-05-29_1129_pr-final-readiness.md`。

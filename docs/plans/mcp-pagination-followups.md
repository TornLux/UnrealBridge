# MCP pagination follow-up candidates

更新：2026-05-29

当前分支：`codex/mcp-pagination-followups`

状态：P1 分页工具已完成并通过 UE 5.7.4 live smoke、pagination smoke、output spill、py_compile、diff whitespace，以及当前 no-editor MCP guardrail 套件验证。no-editor 套件覆盖 stdio schema shape、常见客户端探针、错误响应、response envelope、notification no-response、required / unknown tool argument validation、clean shutdown、二页 pagination script shape、pagination helper failure path、tool docs、workflow scope、follow-up scope 和 smoke runner 自检。根据当前提交策略，本 follow-up 内容并入 `TornLux/UnrealBridge#2` 最终提交版，不再等待单独 follow-up PR。

本文件记录 `TornLux/UnrealBridge#2` 之后的分页工具候选。当前 PR 已经覆盖两类最基础高基数查询：

- `bridge_search_assets_page`
- `bridge_list_actors_page`

本 follow-up 分支已开始实现第一组 SearchableName 工具：

- `bridge_searchable_name_values_page`
- `bridge_assets_referencing_searchable_name_page`

第二组 DataTable 工具也已在本分支实现：

- `bridge_datatable_row_names_page`
- `bridge_datatable_search_rows_page`

第三组 Blueprint audit 工具也已在本分支实现：

- `bridge_blueprint_call_sites_page`
- `bridge_blueprint_debug_prints_page`

后续扩展应保持同一原则：不把 UnrealBridge 改成几百个细碎 MCP tools，只给“容易一次性爆输出、且底层已经有可控筛选或排序”的查询补粗粒度分页入口。

## 选择标准

优先加入 MCP 分页工具的函数应满足：

- 返回 `TArray` 或聚合列表，且在真实项目中可能达到数百到数千行。
- 已有 `MaxResults` / `TopN` / filter 参数，或底层可以稳定排序后切片。
- 输出适合作为“浏览列表”，后续再用现有 `bridge_exec` 或领域库函数读取详情。
- 不需要新增 UE 侧 transport，不绕开 `bridge.py`。

暂不优先加入的函数：

- 单资产 / 单 actor / 单 Blueprint 的详情读取，例如 `GetActorInfo`、`GetBlueprintVariables`。
- 已经天然有 TopN 且结果较短的诊断工具，除非用户明确需要游标浏览。
- 写操作、批量变更操作、需要复杂事务或 rollback 的操作。

## P1 候选

P1 状态：已完成。当前实现覆盖 SearchableName / GameplayTag、DataTable row browser / row search、Blueprint global audit 三组高基数读取场景，并已抽出共享的 bridge JSON 输出解析 helper，避免后续分页工具重复处理 `bridge.py --json` 输出。

### 1. `bridge_searchable_name_values_page`

底层函数：

- `UUnrealBridgeAssetLibrary::ListSearchableNameValues(StructType, FilterPrefix, MaxResults)`
- `UUnrealBridgeAssetLibrary::FindAssetsReferencingSearchableName(StructType, ValueName, PackagePathFilter, MaxResults)`

价值：

- GameplayTag / DataRegistry / SearchableName 索引在大项目中很容易很大。
- 适合先分页浏览 tag/value，再查“谁引用了这个值”。

建议工具：

- `bridge_searchable_name_values_page`（已在 follow-up 分支实现）
- `bridge_assets_referencing_searchable_name_page`（已在 follow-up 分支实现）

### 2. `bridge_datatable_rows_page`

底层函数：

- `UUnrealBridgeDataTableLibrary::GetDataTableRowNames(DataTablePath)`
- `UUnrealBridgeDataTableLibrary::SearchDataTableRows(DataTablePath, Keyword, ColumnFilter)`
- `UUnrealBridgeDataTableLibrary::GetDataTableColumn(DataTablePath, FieldName)`

价值：

- DataTable 经常是大表，一次吐完整行数据 token 成本高。
- 先分页 row names / search hits，再按 row 读取详情，符合现有 API 设计。

建议工具：

- `bridge_datatable_row_names_page`（已在 follow-up 分支实现）
- `bridge_datatable_search_rows_page`（已在 follow-up 分支实现）
- 暂不分页 `GetDataTableRows`，避免鼓励整表导出。

### 3. `bridge_blueprint_call_sites_page`

底层函数：

- `UUnrealBridgeBlueprintLibrary::FindFunctionCallSitesGlobal(FunctionName, OwningClassFilter, PackagePath, MaxResults)`
- `UUnrealBridgeBlueprintLibrary::FindBlueprintDebugPrints(PackagePath, MaxResults)`

价值：

- 全项目 Blueprint 扫描结果可能很大。
- 这类工具通常用于 tech debt / refactor audit，分页能避免一次性塞满 MCP client context。

建议工具：

- `bridge_blueprint_call_sites_page`（已在 follow-up 分支实现）
- `bridge_blueprint_debug_prints_page`（已在 follow-up 分支实现）

## P2 候选

P2 状态：暂缓。等待 #2 反馈或真实项目中出现明确的高基数 Perf / render 浏览需求后再推进。

### 4. Perf / render TopN pages

底层函数：

- `UUnrealBridgePerfLibrary::GetAssetSizeTopN(ClassFilter, TopN)`
- `UUnrealBridgePerfLibrary::GetVisiblePrimitivesByMaterial(MaterialPath, TopN)`
- `UUnrealBridgePerfLibrary::GetShadowCasterBreakdown(TopN)`
- `UUnrealBridgePerfLibrary::AnalyzeAllMaterials(TopN)`

价值：

- 已经有 TopN 控制，当前风险低。
- 更像“诊断榜单”而不是完整浏览列表，分页收益不如 DataTable / Blueprint audit 高。

建议：

- 先保留为 `bridge_exec` 调用模式。
- 如果用户需要在大项目中浏览 Top 1000，可再做 `*_page` wrapper。

## 建议实现顺序

1. SearchableName / GameplayTag index pagination。（已完成）
2. DataTable row-name / row-search pagination。（已完成）
3. Blueprint global audit pagination。（已完成）
4. Perf TopN pagination only if 真实项目反馈需要。（暂缓）

每个新增工具都应补：

- no-editor MCP fixture entry；
- stale cursor test；
- page size clamp test；
- live smoke，至少覆盖第一页、第二页、参数变化后的 stale cursor；
- README / skill 的工具清单更新。

## 发布策略

- 当前上游 PR：`https://github.com/TornLux/UnrealBridge/pull/2`
- 当前最终提交分支：`codex/mcp-stdio-wrapper-pr`
- 备份分支：`codex/mcp-pagination-followups`
- PR 描述草稿：`docs/plans/upstream-pr-description.md`
- 提交 checklist：`docs/plans/pagination-followup-submit-checklist.md` 仅保留为历史拆分方案参考；当前不再单独开启 follow-up PR。
- 范围边界：本 PR 只做 stdio MCP wrapper、分页 wrapper、文档和 smoke 覆盖，不引入新的 transport，不把 UnrealBridge 拆成几百个细碎 MCP tools。
- 验证入口：当前 combined PR 提交前以 `python tools\smoke_mcp_all.py`、`python tools\check_mcp_followup_scope.py --mode combined --base <submit-base>` 和 `git diff --check` 为准；后续 pagination-only follow-up 分支使用默认 `followup` 模式。详细步骤见提交 checklist。

## 验证来源

本计划基于当前源码签名和注释：

- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeAssetLibrary.h`
- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeDataTableLibrary.h`
- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeBlueprintLibrary.h`
- `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgePerfLibrary.h`
- `.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py`

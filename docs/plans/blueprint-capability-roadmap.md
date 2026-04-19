# Blueprint 读/写/理解/操作能力路线图

盘点 UnrealBridge 在蓝图全生命周期（读取、理解、编辑、执行验证）上相对"AI 从自然语言自主生成蓝图"目标的能力缺口，按优先级排序。

最后更新：2026-04-19（#5 `invoke_blueprint_function` + #7 `find_function_call_sites_global` 落地后）

---

## 当前能力基准（2026-04-19）

### 读 / 理解
- 类层次、变量、函数、组件、接口、dispatcher 枚举
- Blueprint / 函数级 summary（紧凑）
- 执行流程 walk（`get_function_execution_flow`）
- 单 BP 内引用追踪：`find_variable_references` / `find_function_call_sites` / `find_event_handler_sites`
- 跨 BP 调用查询：**`find_function_call_sites_global`** 按函数名 + 可选 owner class + 路径 scope + MaxResults
- 节点搜索：`search_blueprint_nodes` 按 title/type/detail 子串
- 节点详细：**`describe_node`** 单次调用返回 pos/size/class/K2Node 子类字段/所有 pin（含类型/默认值/`linked_to`）
- 函数签名：**`get_function_signature`** 参数名/类型/默认值/ref/const/out + pure/static/latent/native + tooltip + category
- Lint（11 种检查）
- 可生成节点发现：`list_spawnable_actions`
- 活 Slate 几何：`get_rendered_node_info`
- 行为验证：**`invoke_blueprint_function`** 在 transient 实例上直接 ProcessEvent（非 Actor = NewObject；Actor = 编辑器世界 SpawnActor），JSON 入参 / JSON 出参（`_return` + out-params），拒绝 latent/非 BlueprintCallable

### 写
- 变量/函数/Macro/接口/组件/dispatcher 全 CRUD + metadata 编辑
- 22+ 种节点创建（Function/Variable/Branch/Sequence/Cast/Event/CustomEvent/Reroute/Delay/Timer/Spawn/Loop/Select/MakeLiteral/MakeStruct/MakeArray/Timeline/Dispatcher/Interface/Comment 等）
- 通用兜底：`spawn_node_by_action_key` 任意节点
- 引脚：连接/断开/默认值
- 布局：位置、对齐、auto_layout（pin_aligned / exec_flow）、straighten、reroute、per-row 列宽、delegate 聚合
- 节点编辑：颜色、enabled、comment、复制、删除、collapse-to-function
- 调试：断点
- BP 层：reparent、metadata、编译

---

## 缺口 — 按优先级分档

### P0 高优先级：阻塞大类功能

| # | 项目 | 影响 |
|---|---|---|
| 1 | **Timeline 轨道 CRUD** | 现只能改 length/autoplay/loop；无法增删 Float/Vector/Event/Color 轨道与关键帧。所有动画/渐变/延时过渡类 BP 做不出来。 |
| 2 | **AnimGraph + 状态机写** | `UnrealBridgeAnimLibrary` 只读。无法建状态、改转换、改 BlendSpace 采样、改 LinkedLayer。角色 BP 整类做不出来。 |
| 3 | **GameplayAbility 图编辑** | 只读 CDO 元数据。无法编辑 GA 激活图/GameplayEffect/GameplayCue。所有 GAS 项目卡死。 |
| 4 | **Enhanced Input 绑定** | 无 IA/IMC 辅助。现代 UE 输入层只能走 raw `unreal.*`。 |
| 5 | ~~**invoke_blueprint_function(bp, func, args) → result**~~ | ✅ 2026-04-19 落地。transient 实例 ProcessEvent；支持 Actor（SpawnActor）+ 普通 UObject；拒绝 latent / 非 BlueprintCallable；JSON 入参 + 出参。 |
| 6 | **运行时 BP 变量/参数快照** | 断点 API 已有，但命中后无法看局部变量值、参数值、返回值。调试循环残缺。 |

### P1 中优先级：常见重构与模板

| # | 项目 | 影响 |
|---|---|---|
| 7 | ~~**find_function_call_sites_global(func, class, max_results)**~~ | ✅ 2026-04-19 落地。AssetRegistry 枚举 BP → 遍历 UbergraphPages/FunctionGraphs/MacroGraphs，支持 owner class 过滤（短名或 `U*` 前缀名）+ PackagePath scope + MaxResults。 |
| 8 | **find_usage_examples(class, func, n)** | 跨 BP 取 N 个真实调用点 + 上下游 2 层，让 AI 照样学。`get_function_signature` 只给文档不给实例。 |
| 9 | **Promote-to-Variable** | 编辑器常见操作，Bridge 无对应。 |
| 10 | **Collapse-to-Macro** | 有 Collapse-to-Function，缺 Macro 版本。 |
| 11 | **DataTable / DataAsset pin 辅助** | `FDataTableRowHandle` 类型需同时指定 DataTable + RowName。无专用 helper，`set_pin_default_value` 需手写 exported text。 |
| 12 | **结构体 pin 原地展开/收起** | Vector/Rotator 等可以"拆分结构体引脚"直接拿 X/Y/Z。现必须 Make/Break 节点，节点数翻倍。 |

### P2 中优先级：写能力边角

| # | 项目 | 影响 |
|---|---|---|
| 13 | **异步节点 K2Node_AsyncAction 创建** | `WaitGameplayEvent` / `OnlineAsyncTask` 等有 delegate 输出 + exec 续接。`SpawnNodeByActionKey` 可兜底但 action key 跨会话不稳。需专用 `add_async_action_node(factory_class, factory_function, x, y)`。 |
| 14 | **修改已有函数签名** | 有 `add_function_parameter` 但无"删除参数 2""交换 A B 顺序"。签名微调做不到。 |
| 15 | **变量类型变更 + 引用修复** | `set_variable_type` 改类型后，现有 Get/Set 节点和 wire 不自动修复，易编译错。 |
| 16 | **跨 BP 重命名** | 单 BP 内 `rename_*` 存在；跨 BP（调用方、子类）不会自动更新。 |

### P3 低优先级：便利性

| # | 项目 | 影响 |
|---|---|---|
| 17 | **当前编辑器状态查询** | "用户选中了哪些节点？焦点在哪个 graph？"——可做 AI 辅助编辑（"把我选中的打包成函数"）。 |
| 18 | **Dry-run / 预览变更** | 一组 add/connect 调用不实际写，返回"会发生什么"。现失败只能 undo 回滚。 |
| 19 | **自定义 K2Node 命名创建** | 项目私有 K2Node（如 `QueryDataTable` 自定义节点）只能走 `SpawnNodeByActionKey`；action key 跨会话不稳。需稳定命名入口。 |

---

## 推荐 Top-5 优先级（工程量 × 频次）

| 排名 | 项目 | 工程量 | 频次 | 理由 |
|---|---|---|---|---|
| ~~1~~ | ~~**#5 invoke_blueprint_function**~~ | ~~中~~ | ~~高~~ | ✅ 已落地 |
| 1 | **#1 Timeline 轨道 CRUD** | 中-大 | 高 | 一大类 BP 写不了 |
| ~~2~~ | ~~**#7 find_function_call_sites_global**~~ | ~~小~~ | ~~中-高~~ | ✅ 已落地 |
| 2 | **#4 Enhanced Input 绑定** | 中 | 高 | 现代 UE 输入必经 |
| 3 | **#2 AnimGraph 状态机写** | 大 | 高 | 角色 BP 品类解锁 |
| 4 | **#6 运行时 BP 变量/参数快照** | 中 | 高 | 断点 + 快照配合才是完整调试环 |
| 5 | **#3 GameplayAbility 图编辑** | 大 | 中-高 | GAS 项目解锁 |

下一最性价比起点：**#6（运行时变量/参数快照）** 与 **#4（Enhanced Input）** — #6 直接配合已有的断点 API 补齐调试循环，#4 是现代 UE 项目的刚需。

---

## 排除项（非 Bridge 职责）

这些由 LLM 侧处理，不需要 Bridge API：
- 自然语言 → 节点选择（LLM 做）
- 控制流自然语言摘要（LLM 做）
- 视觉化 SVG / mermaid 图输出（LLM 做）
- 自动完成 / 代码建议（LLM 做）

---

## 相关历史文档
- `docs/blueprint-edit-gaps.md`（2026-04-14 版，仅写操作视角，比本文件窄但更早）
- `docs/plans/anim-pose-capture.md`（pose 捕获特性）
- `docs/plans/reactive-handlers.md`（reactive 事件框架）

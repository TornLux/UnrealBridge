# Blueprint 读/写/理解/操作能力路线图

盘点 UnrealBridge 在蓝图全生命周期（读取、理解、编辑、执行验证）上相对"AI 从自然语言自主生成蓝图"目标的能力缺口，按优先级排序。

最后更新：2026-04-19（反思清单 Top-5：batch_ops / insert-on-wire / graph-diff / 签名摩擦修复 / CDO override 查询 一并落地）

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
- 编辑器焦点快照：**`get_editor_focus_state`** 当前焦点 BP / 活动 graph / 选中节点 / 所有打开的 BP 编辑器
- 图指纹 / 快照 / 语义 diff：**`get_graph_fingerprint`** + **`snapshot_graph_json`** + **`diff_graph_snapshots`** — 低成本检测 AI 编辑产生的变化

### 写
- 变量/函数/Macro/接口/组件/dispatcher 全 CRUD + metadata 编辑
- 22+ 种节点创建（Function/Variable/Branch/Sequence/Cast/Event/CustomEvent/Reroute/Delay/Timer/Spawn/Loop/Select/MakeLiteral/MakeStruct/MakeArray/Timeline/Dispatcher/Interface/Comment 等）
- 通用兜底：`spawn_node_by_action_key` 任意节点
- 引脚：连接/断开/默认值
- 布局：位置、对齐、auto_layout（pin_aligned / exec_flow）、straighten、reroute、per-row 列宽、delegate 聚合
- 节点编辑：颜色、enabled、comment、复制、删除、collapse-to-function、**collapse-to-macro**
- 调试：断点
- BP 层：reparent、metadata、编译
- 函数签名：add/remove/reorder 参数，set metadata，rename graph
- Pin 高级：struct pin **split / recombine**、**promote-to-variable**（member / local）
- 变量类型：**change_variable_type_with_report**（自动抑制"是否搜索引用"对话框）
- 跨 BP 重构：**rename_member_variable_global** / **rename_function_global** 按 owner class 过滤
- 节点落点：任意 K2Node 按类名创建（**add_node_by_class_name**）、异步节点（**add_async_action_node**）、DataTableRowHandle 默认值助手（**set_data_table_row_handle_pin**）
- 重构原语：**`insert_node_on_wire`**（A→B 中间插入第三个节点）+ **`replace_node_preserving_connections`**（换节点类同时保留同名 pin 连线）
- 批量编辑：**`apply_graph_ops`** 单次 round-trip 跑多个 add/connect/set_default，支持 `$N` 回引用、结尾统一 compile
- 入口摩擦修复：`get_function_signature` 现在接受 BP 路径（无需 `_C` 后缀）；**`ensure_function_exec_wired`** 把 Entry.then → Result.execute 自动接上
- CDO 查询：**`find_cdo_variable_overrides`** 列出子 BP 中覆盖了父类默认值的变量（如 MaxHealth 被哪些子类改过）

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
| 9 | ~~**Promote-to-Variable**~~ | ✅ 2026-04-19 落地 `promote_pin_to_variable`。支持 member / local variable；data pin → Get/Set 节点自动连线；唯一化变量名。 |
| 10 | ~~**Collapse-to-Macro**~~ | ✅ 2026-04-19 落地 `collapse_nodes_to_macro`。对称 `collapse_nodes_to_function` 的 Tunnel/Tunnel 版本。 |
| 11 | ~~**DataTable / DataAsset pin 辅助**~~ | ✅ 2026-04-19 落地 `set_data_table_row_handle_pin`。自动格式化 `(DataTable="...",RowName="...")` 导出文本。 |
| 12 | ~~**结构体 pin 原地展开/收起**~~ | ✅ 2026-04-19 落地 `split_struct_pin` / `recombine_struct_pin`（通过 `UEdGraphSchema_K2::SplitPin`/`RecombinePin`）。 |

### P2 中优先级：写能力边角

| # | 项目 | 影响 |
|---|---|---|
| 13 | ~~**异步节点 K2Node_AsyncAction 创建**~~ | ✅ 2026-04-19 落地 `add_async_action_node(factory_class_path, factory_function_name, x, y)`。直接 `InitializeProxyFromFunction`。 |
| 14 | ~~**修改已有函数签名**~~ | ✅ 2026-04-19 落地 `remove_function_parameter` + `reorder_function_parameter`（`UserDefinedPins` 重排 + `ReconstructNode`）。 |
| 15 | ~~**变量类型变更 + 引用修复**~~ | ✅ 2026-04-19 落地 `change_variable_type_with_report`。抑制 `ChangeVariableType_Warning` 对话框 + 扫描 Get/Set 节点 pin 类型不一致的 guid 列表。 |
| 16 | ~~**跨 BP 重命名**~~ | ✅ 2026-04-19 落地 `rename_member_variable_global` / `rename_function_global`。AssetRegistry 枚举 → 按 owner class 过滤重写 K2Node + recompile。 |

### P3 低优先级：便利性

| # | 项目 | 影响 |
|---|---|---|
| 17 | ~~**当前编辑器状态查询**~~ | ✅ 2026-04-19 落地 `get_editor_focus_state`。返回焦点 BP / 活动 graph / 选中节点 / 所有已打开 BP 列表。 |
| 18 | **Dry-run / 预览变更** | 一组 add/connect 调用不实际写，返回"会发生什么"。现失败只能 undo 回滚。 |
| 19 | ~~**自定义 K2Node 命名创建**~~ | ✅ 2026-04-19 落地 `add_node_by_class_name(class_path, x, y)`。`FindFirstObject<UClass>` 解析短/长类名；拒绝非 UK2Node 或 abstract 类。 |

---

## 当前剩余 TODO（2026-04-19 后）

只剩 6 项。按工程量 × 频次排序：

| 排名 | # | 项目 | 工程量 | 频次 | 理由 |
|---|---|---|---|---|---|
| 1 | #6  | 运行时 BP 变量/参数快照 | 中 | 高 | 断点 + 快照才是完整调试环 |
| 2 | #4  | Enhanced Input 绑定 | 中 | 高 | 现代 UE 输入必经 |
| 3 | #1  | Timeline 轨道 CRUD | 中-大 | 高 | 渐变/延时/过渡一大类 BP |
| 4 | #8  | find_usage_examples | 小-中 | 中 | 跨 BP 真实例子 → AI 照样学 |
| 5 | #2  | AnimGraph 状态机写 | 大 | 高 | 角色 BP 品类 |
| 6 | #3  | GameplayAbility 图编辑 | 大 | 中-高 | GAS 项目 |
| 7 | #18 | Dry-run / 预览变更 | 中 | 低 | 便利，非刚需 |

下一最性价比起点：**#6（运行时变量/参数快照）** + **#8（find_usage_examples）** — 两者都是中小工程量，前者补完调试循环，后者把 `get_function_signature` 的"文档级"信息扩展成"实战样例"。

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

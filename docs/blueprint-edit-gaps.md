# Blueprint 编辑能力缺口与路线图

本文件盘点 `UnrealBridgeBlueprintLibrary` 在蓝图**写操作**上相对日常开发需求的缺口，按使用频率 × 实现难度排序，作为后续扩展依据。

最后更新：2026-04-14

---

## 当前已有的写操作（基准线）

变量/组件/接口层：
- `set_blueprint_variable_default` / `add_blueprint_variable` / `remove_blueprint_variable` / `rename_blueprint_variable`
- `set_component_property` / `add_blueprint_component`
- `add_blueprint_interface` / `remove_blueprint_interface`（仅声明，不落函数图）

图节点层：
- `add_call_function_node` / `add_variable_node` / `add_event_node`
- `connect_graph_pins` / `remove_graph_node` / `set_graph_node_position`
- `set_pin_default_value`

编译：
- `UnrealBridgeEditorLibrary.compile_blueprints`（只返回 bool，无 error/warning 列表）

---

## 缺口清单

### P0 — 高频 / 必须补

日常写蓝图几乎每次都用；缺了这些，纯 Python 构建稍微复杂一点的图就走不通。

#### 函数/事件图管理
- `create_function_graph(bp, name, ...)` — 新建函数图
- `add_function_parameter(bp, fn_name, param_name, type_string)` / `add_function_return(...)`
- `set_function_metadata(bp, fn_name, pure=False, const=False, category="", access="public")`
- `create_custom_event(bp, graph, name, params, x, y)` — `K2Node_CustomEvent`，区别于 override event
- `rename_function` / `remove_function`
- `create_macro_graph` / 删除宏

#### 控制流节点
现在 Branch / Sequence / ForEachLoop / Cast 都没有，且这些不是普通 UFunction，`add_call_function_node` 覆盖不了：
- `add_branch_node(bp, graph, x, y)` — `K2Node_IfThenElse`
- `add_sequence_node(bp, graph, pin_count, x, y)` — `K2Node_ExecutionSequence`
- `add_foreach_node(bp, graph, x, y)` / `add_forloop_node` / `add_whileloop_node`
- `add_cast_node(bp, graph, target_class_path, pure, x, y)` — `K2Node_DynamicCast`
- `add_self_node(bp, graph, x, y)` — `K2Node_Self`
- `add_make_literal_node(bp, graph, type_string, value, x, y)`

#### Event Dispatcher 写侧
当前只能读 dispatcher，不能建/调用：
- `add_event_dispatcher(bp, name, params)` / `remove_event_dispatcher` / `rename_event_dispatcher`
- `add_dispatcher_call_node(bp, graph, dispatcher_name, x, y)` — Broadcast
- `add_dispatcher_bind_node` / `add_dispatcher_event_node` — 绑定回调用的 CustomEvent

#### Interface 继承与 Override
`add_blueprint_interface` 只写了声明，**没生成可 override 的函数图**，这是目前最明显的缺口：
- `implement_interface_function(bp, interface_path, fn_name)` — 把接口函数落成可编辑 graph；区分 event-type 和 function-type 接口成员
- `add_interface_message_node(bp, graph, interface_path, fn_name, x, y)` — `K2Node_Message`，即 UI 里 "Call Function (Message)"

#### 变量元数据
当前 `add_blueprint_variable` 只设了名字/类型/默认值，缺编辑器常用字段：
- `set_variable_metadata(bp, var_name, instance_editable=?, expose_on_spawn=?, replication=?, category="", tooltip="", access="public")`
- `set_variable_type(bp, var_name, new_type_string)` — 含容器（Array/Set/Map）切换
- 验证/扩展 `add_blueprint_variable.type_string` 对容器类型的支持

---

### P1 — 中频 / 补了很好用

#### 图布局
- `align_nodes(bp, graph, guids, axis)` — 左/顶/水平分布，对应编辑器 Q/W/A/S
- `add_comment_box(bp, graph, node_guids, text, x, y, w, h)`
- `add_reroute_node(bp, graph, src_pin, dst_pin, x, y)` — knot
- `set_node_enabled(bp, graph, guid, enabled)` — 禁用/启用节点

#### 类设置
- `reparent_blueprint(bp, new_parent_path)` — 改父类（高风险，需 transaction + 重编译）
- `set_blueprint_metadata(bp, tooltip="", category="", blueprint_type=?)`

#### 组件树补齐
现在只能 add，缺：
- `reparent_component(bp, component_name, new_parent_name)`
- `reorder_component(bp, component_name, new_index)`
- `remove_component(bp, component_name)`

#### 编译反馈
- `get_compile_errors(bp)` — 返回 `[{severity, message, node_guid?}]`；当前 `compile_blueprints` 只有 bool，调试生成图必需

---

### P2 — 低频 / 按需

已实现（2026-04-14）：

- `add_timeline_node(bp, graph, timeline_template_name, x, y)` — `K2Node_Timeline`，自动通过 `FBlueprintEditorUtils::AddNewTimeline` 落 `UTimelineTemplate`；空名时走 `FindUniqueTimelineName`。轨道（Float/Vector/Color/Event）目前没有写 API
- `add_delay_node` / `add_set_timer_by_function_name_node` — 包成 `UKismetSystemLibrary::Delay` / `K2_SetTimer`，预填 pin 默认值
- `add_spawn_actor_from_class_node` — `K2Node_SpawnActorFromClass`；为了避开 `PostPlacedNewNode` 的 `FindPinChecked(ScaleMethod)` 必须先 `AllocateDefaultPins` 再 `PostPlacedNewNode`，再设 ClassPin + `PinDefaultValueChanged` 触发暴露的 spawn vars 重新生成
- `add_make_struct_node` / `add_break_struct_node` — `K2Node_MakeStruct` / `BreakStruct`；MakeStruct 走 `bForInternalUse=true` 兼容带 native-make 的 struct，但 Vector/Rotator/Transform 这种依然只允许在 advanced 路径下使用，调用方建议直接用 `MakeVector` 等 CallFunction
- `add_breakpoint(bp, graph, node_guid, enabled)` — `FKismetDebugUtilities::CreateBreakpoint` + `SetBreakpointEnabled` 组合，调用幂等
- `create_macro_graph(bp, macro_name)` / `remove_macro_graph(bp, macro_name)` — `FBlueprintEditorUtils::AddMacroGraph` / `RemoveGraph(EGraphRemoveFlags::Recompile)`，与 `create_function_graph`/`remove_function_graph` 对称
- `remove_breakpoint(bp, graph, node_guid)` / `clear_all_breakpoints(bp)` / `get_breakpoints(bp)` — 补齐断点管理三件套；读侧返回 `FBridgeBreakpointInfo { graph_name, node_guid, node_title, enabled }`。`ClearBreakpoints` 走 `FKismetDebugUtilities::ClearBreakpoints`，单节点删走 `RemoveBreakpointFromNode`
- `set_timeline_properties(bp, timeline_name, length, auto_play, loop, replicated, ignore_time_dilation)` — 直接改 `UTimelineTemplate` 字段，再用 `FBlueprintEditorUtils::FindNodeForTimeline` 同步到活动的 `K2Node_Timeline`。`length < 0` 表示保持不变

#### Deferred — 暂不实现

- `convert_event_to_function(bp, event_name)` — 对应右键 "Collapse to Function"。`FKismetEditorUtilities::CreateNewBoundedGraphFromEventNode` 族 API 牵涉到 EventGraph→FunctionGraph 的节点迁移、CustomEvent 重命名、引用回填，UE 5.7 公开 API 没有一个干净入口；引擎自己也是在 `FBlueprintEditor` 里基于 SGraphPanel 选区做的。需要复刻几百行 `FBlueprintEditor::CollapseSelectionToFunction` 的逻辑或者改走拷贝 + 删除 + 重连接，复杂度远超 P2 收益，留待真有需求再做
- `add_function_return(...)` — 已经被 `AddFunctionParameter(..., bIsReturn=true)` 完整覆盖；不再单独加别名以避免 API 表面发胖

---

## 实现难度分档

| 难度 | 典型代表 | 实现套路 |
|------|---------|---------|
| **容易** | Branch / Sequence / Cast / Self / Literal / CustomEvent | 照 `AddEventNode` 套路：`NewObject<K2Node_XXX>` + `AllocateDefaultPins` + `MarkBlueprintAsModified` |
| **中等** | `create_function_graph`、`implement_interface_function`、Event Dispatcher 族 | 走 `FBlueprintEditorUtils::AddNewFunctionGraph` / `AddNewDelegateSignature` 等现成工具函数 |
| **硬** | `convert_event_to_function`、Timeline、`reparent_blueprint`、`get_compile_errors` | 多步重构或需要挂 `FCompilerResultsLog` 回调；必须 `FScopedTransaction` + 重编译 + 验证 |

---

## 建议实施顺序

分三批：

1. **Batch 1（P0 核心）**
   - `create_function_graph` + 参数/返回值
   - `create_custom_event`
   - Branch / Sequence / Cast
   - Event Dispatcher 写操作（add / call / bind / event）
   - `implement_interface_function`
   - `get_compile_errors`

2. **Batch 2（P1）**
   - ForEach/ForLoop/Select 等剩余控制流
   - `align_nodes` / comment box / reroute knot
   - `set_variable_metadata` / `set_variable_type`
   - `set_node_enabled`
   - 组件树 reparent/reorder/remove

3. **Batch 3（P2 / 高风险）**
   - `convert_event_to_function`
   - Timeline
   - `reparent_blueprint`
   - Make/Break Struct、断点、Delay/Timer

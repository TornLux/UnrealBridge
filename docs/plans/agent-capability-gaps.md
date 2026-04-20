# Agent 对 UE 的操作能力缺口与路线图

盘点 UnrealBridge 作为 agent ↔ UE 编辑器桥梁的能力缺口，按"能补 / 代价高但理论可行 / 根本补不了"三档划分。用于指导后续投资方向。

最后更新：2026-04-20（A1-#2 GBuffer 通道截图 + A1-#3 Perf 快照均已交付 — `capture_viewport_channel` / `capture_channel_from_pose` + HitProxy actor-ID pass；`UnrealBridgePerfLibrary.get_frame_timing` / `get_render_counters` / `get_memory_stats` / `get_u_object_stats` / `get_perf_snapshot`）

---

## 现状基线（2026-04-20）

Bridge 已覆盖的主要领域（详见各 `bridge-*-api.md`）：

- **Blueprint** — 全生命周期 CRUD：类层次 / 变量 / 函数 / 组件 / 接口 / dispatcher；节点创建 22+ 种 + 通用 `add_node_by_class_name` 兜底；图布局、lint、批量 ops、graph diff；运行时 debug（断点、PIE node coverage、FFrame.Locals 快照）
- **Asset** — 搜索、引用追踪、依赖链、DataTable、DataAsset、StaticMesh/SkeletalMesh/Texture/Sound 元数据
- **Level / Actor** — spawn / destroy / transform / property get-set（含嵌套）、selection、visibility、screenshot、`capture_ortho_top_down` / `capture_from_pose` / `capture_anim_pose_grid`
- **Editor** — state / viewport / CB / PIE / CVars / console / redirector fixup / BP compile / **screenshot 同步捕获** / **Live Coding 触发**
- **Animation** — 只读：state machine / AnimGraph / linked layer / slot / curve / sequence / montage / blendspace
- **Material** — 只读：MI 参数枚举
- **GAS** — 只读：GA CDO 元数据
- **Reactive handlers** — 10 个 adapter 的 Python-on-UE-event 注册框架（GameplayEvent / Anim / Input / Actor / Timer / PIE / Asset 等）
- **Navigation** — NavMesh OBJ 导出
- **C++ 迭代循环** — `hot_reload.py`（Live Coding）+ `rebuild_relaunch.py`（全量重编 + 重启）

---

## Tier A — 能做且收益明确（值得排期）

按 ROI × 工程量打分。

### A1. 感知类（让 agent 真的"看到"游戏）

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 1 | **PIE 视频 / 帧序列抓取** | 中 | 高 | `capture_active_viewport` 现在只出单帧。真的要看 agent 在玩、调试运动 / VFX / cutscene 的时候必须是序列。做法：加 `begin_frame_capture(fps)` + `end_frame_capture()`，内部 `FRenderTarget` + 按帧写 PNG 序列到 `<Saved>/Captures/`，返回路径清单；或直接 mux 成 MP4（需 `FFmpegCapture` 或第三方）。**→ 单一功能解锁最大新信息量**。 |
| 2 | **GBuffer / 深度 / 法线 / ID pass** ✅ | — | — | **已交付 2026-04-20**：`capture_viewport_channel` / `capture_channel_from_pose` 提供 Depth（linear cm, 16-bit PNG）/ DeviceDepth / Normal（world-space, 8-bit RGB）/ BaseColor 四通道，走 `ASceneCapture2D` + `UTextureRenderTarget2D`。ID pass 走 HitProxy（`capture_hit_proxy_map`），比原计划的 `SCS_ObjectID` 更精确 — 每像素直接映射回 `AActor*`。 |
| 3 | **Perf 快照结构化输出** ✅ | — | — | **已交付 2026-04-20**：`UnrealBridgePerfLibrary` 提供 `get_frame_timing`（FPS / GT / RT / GPU / RHI ms，raw + stat-unit 智能切换）/ `get_render_counters`（draw calls + primitives，summed across MAX_NUM_GPUS）/ `get_memory_stats`（working set / peak / available, MiB）/ `get_u_object_stats`（TObjectIterator class histogram，132k 对象 ~5ms）/ `get_perf_snapshot`（聚合 + ISO-8601 timestamp，两档成本选 uobject）。结构化 USTRUCT 而非字符串。 |
| 4 | **LC 编译错误文本捕获** | 中 | 高 | 2026-04-20 验证 UE 5.7 的 `ILiveCodingModule` 不返回编译器 stdout。要拿到只能 fork `LiveCodingConsole.exe` 走命名管道拦截，或改走"关 editor → UBT build → 读 stdout → 重启 editor" 的链路（已经是 `rebuild_relaunch.py`）。LC 这路暂时走不通，优先级降。 |

### A2. 内容生成类（最大空白，整类品类卡死）

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 5 | **Material Graph 写** | 大（3-5k 行 C++） | 高 | 只能改 MI 参数，不能造 Master Material。做法与 BP 同构：`UMaterialExpression*` 子类 + `UMaterialGraph` 编辑。按常用节点 20 个优先（Constant / Add / Multiply / TextureSample / Lerp / Panner / Normal / Emissive / Transform / PixelDepth …）就能覆盖 80% 案例。直接解锁整个 shader 领域 + Niagara 里很多 module（它们底层也是 material graph）。 |
| 6 | **Niagara 系统 + emitter + module 编辑** | 大 | 中-高 | 零覆盖。`FNiagaraSystemViewModel` + `FNiagaraEmitterHandle` + `UNiagaraScript`。VFX 整类无法 agent 化。先读后写，写的时候可以复用 Material Graph 的节点创建原语。 |
| 7 | **Sequencer / 过场** | 大 | 中 | 零覆盖。`UMovieScene` + `UMovieSceneTrack*` 子类。cutscene / 相机脚本 / trailer 都要人开 Sequencer 编辑器。API 结构清晰，主要工作量在各种 track 类型的封装。 |
| 8 | **AnimGraph / 状态机写** | 大 | 高 | 只读。`UAnimStateNodeBase` / `UAnimGraphNode_*`，做法与 BP 同构。角色 BP 品类需要。 |
| 9 | **GameplayAbility 图编辑** | 大 | 中-高 | 只读 CDO。GA activate graph / GE 链 / GC 触发写操作全缺。GAS 项目卡死。 |
| 10 | **Control Rig / IK Rig / IK Retargeter** | 大 | 中 | 零覆盖。跨骨架重定向每次人工。`URigBlueprint` + `FRigHierarchy` 编辑，UE 官方也在快速迭代，API 面偏不稳。 |

### A3. 运行时观测 / 调试

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 11 | **Behavior Tree / Blackboard 运行时读** | 中 | 中-高 | AI 代理完全黑盒。做法：hook `UBehaviorTreeComponent::StartTree/OnStart/OnTaskFinished`，dump 当前 active task + blackboard KV。 |
| 12 | **Callstack 快照** | 中 | 中 | `get_last_breakpoint_hit` 只给 Locals。加 `FFrame` 链遍历返回 N 层 callstack（fn name + class + pc）。 |
| 13 | **Crash dump 解析** | 中-大 | 低-中 | editor 崩 bridge 就断；`Saved/Crashes/` 堆 `.dmp` 没有 API 解析。做法：`rebuild_relaunch.py` 启动前检测最新 crash、读 `.log` + `dbghelp.dll` 符号化栈。 |
| 14 | **Shader 异步编译等待原语** | 小 | 中 | 已有 `is_compiling()` / `flush_compilation()`，但没有"等编译完再跑测试，超时 N 秒"的一次调用。包一层就完了。 |

### A4. 重构 / 调试基建（BP roadmap 里未完成项）

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 15 | **Dry-run 蓝图编辑** | 中 | 低-中 | 一组 add/connect 预览"会发生什么"而不落盘。错了不用 undo 链。 |
| 16 | **find_usage_examples(class, func, n)** | 小-中 | 中 | 跨 BP 抓 N 个真实调用样本 + 上下游 2 层，AI 照样学比读签名强 10 倍。 |
| 17 | **Timeline 轨道 CRUD** | 中-大 | 高 | 现只能改 length/autoplay/loop；无法增删 Float/Vector/Event/Color 轨道与关键帧。 |
| 18 | **Enhanced Input IA/IMC 辅助** | 中 | 高 | 现代 UE 输入层没有 bridge 封装，只能 raw `unreal.*`。 |

### A5. 打包 / 部署

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 19 | **Cook / Package / BuildServer** | 中 | 中 | 零。所有"能上线吗 / 打个 test build 给 QA"问题无法回答。封装 UAT (`Engine/Build/BatchFiles/RunUAT.bat BuildCookRun ...`) 的异步调用 + stdout 捕获即可。 |
| 20 | **Plugin 依赖声明扫描** | 小 | 中 | 构建警告里 `Plugin 'UnrealBridge' does not list plugin 'EnhancedInput' as a dependency` 这种应该自动扫 + 修 .uplugin。 |

### A6. 自动化测试 / 回归验证

Agent 最大的痛点是"改完不知道有没有坏东西"。这一组直接给它回归感知。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 21 | **UE Automation Framework 对接** | 中 | 高 | 封装 `Session Frontend` 的 test runner：`run_automation_tests(groups)` 返回结构化 `[{name, passed, duration_ms, stack}]`。CI 味道的"改完代码跑一轮"体验直接解锁。 |
| 22 | **Soak / 长跑测试** | 中 | 中-高 | 下一个脚本让 PIE 跑 N 分钟，持续抓 FPS / memory / error count / LogWarning 密度，返回 time series。回归时对比两次运行的统计差。 |
| 23 | **Golden-image 回归** | 中 | 高 | 同 pose / 同 level / 同 PIE 起始状态做 screenshot，和基线图像素 diff（带 ε tolerance + ignore-mask）。改渲染 / 材质 / BP 后一键确认没破东西。跟 `capture_anim_pose_grid` 那套 pipeline 天然契合。 |
| 24 | **Fuzz 输入 / 随机 pawn 驱动** | 中 | 中 | 在已有 IA 注入框架上加随机序列 + NaN / 碰撞穿墙 / 卡 geometry 检测。崩溃自动上报 + 种子化可复现。 |
| 25 | **Replay 录制 + 回放** | 中-大 | 中 | 录一段 PIE 的 input + spawn + 关键时间点，改完代码后能确定性重放。比 soak 更精细，适合 bug 复现。 |

### A7. 项目卫生 / 规模化重构

跨 BP + C++ 的大规模扫描类工具，人肉做太累而 agent 天生适合。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 26 | **命名 / 目录约定扫描器** | 小-中 | 中 | 按项目 style（如 "BP_* 必须在 /Content/Blueprints/"、"IA_* 是 InputAction"）扫全库，列违规 + 自动修复。规则用 JSON 配置。 |
| 27 | **死代码 / 孤立 asset 检测** | 小-中 | 中 | 无引用 asset、从未被调用的 BP 函数、从未被 broadcast 的 dispatcher、从未被 bind 的 event。基于 AssetRegistry + `find_function_call_sites_global` 已有原语。cleanup 前必跑。 |
| 28 | **TODO / debug print 审计** | 小 | 中 | 跨 BP + C++ 扫 `PrintString` / `UE_LOG(LogTemp, ...)` / `// TODO` / `// HACK`，发布前清单。 |
| 29 | **依赖图可视化** | 小 | 低-中 | 基于现有 reference tracking 生成 mermaid / DOT，定位循环依赖。Agent 能自己读图判断结构风险。 |
| 30 | **跨版本 BP 语义 diff** | 中 | 中 | git 看 `.uasset` 是二进制 diff 读不懂。已有 `snapshot_graph_json` + `diff_graph_snapshots`，扩成跨 git commit 的 BP diff（"这个 PR 加了哪些节点 / 改了哪个 pin 默认值 / 删了哪条连线"）直接用于 PR review。 |

### A8. Asset 流水线 / 批处理

数据驱动工作流的基石，单点 CRUD 已有但批量能力稀薄。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 31 | **CSV / JSON → DataTable 批量导入** | 小 | 高 | 现在 DataTable 是逐行 CRUD，真正数据驱动需要 `import_rows_from_csv(path, row_struct)` 一把梭。 |
| 32 | **FBX / GLTF / USD 批量导入带预设** | 中 | 中-高 | `AutomatedAssetImportData` + 每类资源（mesh / anim / texture / audio）一套 import options。美术交付一大堆资源的场景必备。 |
| 33 | **LOD / 碰撞 / physics body 自动生成** | 中 | 中 | 扫选中的 mesh，挨个生成 LOD chain / auto-convex / UCX 简单碰撞 / PhysicsAsset。 |
| 34 | **资源去重** | 中 | 低-中 | 找功能相同的 Texture（图像哈希）/ Material（参数完全相同的 MI）/ BP（节点结构哈希），一键 redirector 合并。 |

### A9. 程序化场景搭建

Houdini 风格的 placement 原语，让 agent 能批量组装关卡而不是逐 actor 摆。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 35 | **Scatter / grid 布景** | 中 | 中-高 | "把这 10 种 prop 随机撒在 navmesh 上 500 个"、"沿曲线摆路灯"、"在 volume 内等间距放"。输入 = 规则 + 目标区域，输出 = spawn 清单。 |
| 36 | **Snap-to-surface + 法线对齐** | 小 | 中 | 放 actor 自动投射到地面 + 对齐法线 + 随机偏移。Scatter 的原子。 |
| 37 | **Landscape 高度图导入 / 绘制** | 中 | 低-中 | 传 PNG 当 heightmap / weightmap，而不是 UI 画。procedural 地形管线入口。 |
| 38 | **Foliage type 批量配置** | 小 | 低-中 | density / LOD / cull distance / collision preset 的批处理。 |

### A10. 状态管理 / 持续性

让 agent 能"分支尝试 + 回滚"，而不是每条调用都是不可逆的线。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 39 | **Editor state 快照 / 恢复** | 中 | 高 | 记下当前开的 asset、选中的 actor、viewport 相机、CB 路径、各 BP 编辑器的 active graph → JSON；之后一键回到那个现场。"对比几个方案"的场景极好用。 |
| 40 | **PIE state 快照** | 中-大 | 中 | save game 之上加一层，dump 当前 world 所有 actor 的位置 / 属性 / component state → JSON，恢复时重建。不用每次从头玩一遍。 |
| 41 | **Named workflow / macro 录制** | 小-中 | 中 | 把一串 bridge 调用 + 参数打包成一个"macro"，下次一个 call 复用。Reactive handlers 框架已开了半个头。 |

### A11. 外部集成 / 触达

小工程量但体验差异巨大的"连通性"类。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 42 | **Webhook on completion** | 小 | 中 | Cook / build / 长跑测试完 POST 到 Slack / Discord / email / 自定义 URL。一行 HTTP 的事但解锁异步工作流。 |
| 43 | **JIRA / Linear / GitHub Issue 自动开票** | 小-中 | 中 | crash dump / test failure / lint violation 直接成 ticket 带 repro 步骤。减少"agent 跑完不知道去哪跟进"的断链。 |
| 44 | **Perforce / Git LFS 辅助** | 中 | 中-高 | check-out、submit、conflict 检测的 bridge 封装（现在的 SourceControl API 太薄，只有 state 查询 + checkout）。大项目必需。 |

### A12. Bridge 自观测 / 元工具

对 Bridge 本身的自动化，基础设施投入。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 45 | **Bridge 调用日志 + 性能** | 小 | 中 | 每次 `UnrealBridge*Library.*` 调用记 name / duration / success / payload size，本地 ring buffer + 可导出。"哪个 API 调得最多 / 最慢 / 最容易超时"的数据基础。 |
| 46 | **Signature registry dump** | 极小 | 高 | 启动时把所有 UFUNCTION 的参数名 / 类型 / 默认值 / tooltip 导出成 JSON。agent 一次拉走 ≈ 不再反复 Read 各 `bridge-*-api.md`。**消除"TypeError → 再读 → 再试"这一整类浪费**。 |
| 47 | **Bridge API changelog 自动生成** | 小 | 低-中 | 每次合并时 diff `UnrealBridge*Library.h` 之间的 UFUNCTION 列表，生成 Markdown changelog。不再靠记忆回忆"上周加了啥"。 |
| 48 | **单 call dry-run / trace** | 中 | 低-中 | "这个 bridge 调用会动哪些 asset / 改哪些 UObject"，真跑前预览。蓝图 dry-run（#15）的通用版本。 |

---

## Tier B — 理论可行但成本 / 风险过高

| 项目 | 阻塞因素 |
|---|---|
| **引擎源码修改 + 重编** | 要改 `<ue-install>/Engine/Source/...` 然后跑几十分钟全量重编。单个 agent turn 放不下，失败后用户整套编辑器起不来 — 风险不对称。只能让用户手动做。 |
| **DCC 联动（Blender / Maya / Houdini）** | 跨进程跨沙箱。需要独立 MCP 或单独的桥。UnrealBridge 够不着。 |
| **Visual Studio debugger attach** | 原生 C++ 断点、step-over、watch。UE 没有把这层暴露给 Blueprint/Python。我们只有 BP debugger + `FFrame.Locals`。要做只能反过来从 VS 自动化侧入手。 |
| **Quixel / Marketplace / Fab 下载** | 需要 Epic 账号授权 + Launcher RPC。没有干净的程序入口。 |
| **实机"玩游戏"做定性判断** | 量化 agent loop（走到点 A / HP 破线 / 帧数不降）能做。"这跳飘不飘、这关好不好玩"需要审美先验，要调外部视觉 LLM，而且标准本身主观。 |
| **自动新增游戏项目 C++ 模块** | 能改 plugin 自己，但往 `<ue-project>/Source/` 塞新 module + 改 .uproject + 让 UBT 识别，这套流程 bridge 没做过。 |

---

## Tier C — 根本做不了（约束而非工程量）

1. **听游戏音频** — 编辑器不把 `USoundSubmix` 的原始 buffer 暴露给 BP/Python。要改引擎或注入 NRT 录音插件，超出 bridge 范围。
2. **VR / 主机 / 手机实机测试** — 只能看 PC 编辑器。Quest / PS5 / iOS 全盲。需要远程设备的另一套 harness。
3. **真实网络 / 多人测试** — PIE 能开 ListenServer + N client，但 3 人 3 手柄 3 头显同时喊话 — agent 替代不了人。
4. **美术 / 音频 / UX 主观质量判断** — 技术正确 ≠ 好看 / 好听 / 好玩。视觉分类能给下限，上限要人。
5. **硬件反馈（rumble / haptic）** — 没有采样通道。
6. **用户未说出口的需求** — 没看过设计文档、没听过会议、没见过竞品 playtest。"老板觉得手感不对"这种得人做 proxy。
7. **跨项目 / 跨仓库知识迁移** — 看到的只是当前工作区。用户别的项目类似问题怎么解决，无从知晓。
8. **编辑器崩溃后的现场分析** — crash 后 bridge 断连，重启后进程现场全丢。只能读事后 .dmp。

---

## 给定新功能时，最可能卡住的几类场景

按出现频率排：

1. **需要改引擎源码** — 碰到 UE 官方 API 缺口（典型例子：2026-04-20 验证的 LC 不返回编译错误）。无解。
2. **需要调用外部进程并拿结果** — `ffmpeg`、`blender --python`、`git lfs`。`bridge.exec` 卡 GameThread，长命令死锁。解法：在 bridge 里加 `run_external_process(cmd, timeout)` 走 `FPlatformProcess::CreateProc` + 异步 pipe 读，不是不能做，只是每次都要临时起架子。
3. **需要新增项目 C++ 模块**（非我们 plugin） — UBT 识别流程没走通，需要先在桥这边做一套"生成新 module 模板 + 改 .uproject + 触发 rebuild_relaunch"。
4. **需要跨 editor-session 持久状态** — reactive handlers 框架有 JSON persistence 先例。非这框架的东西（如"上次 PIE 跑完的统计"）都得自己搞存档文件。
5. **需要 agent 判断用户意图是否正确** — 用户说"把角色变帅"，我只能问回去或瞎猜。

---

## 建议下一季度投入顺序

按 ROI × 解锁面积，分两波。

### 快速收益（工程量小，单周内落地，立竿见影）

优先做这批，基础设施投入，对后续所有特性都是乘数。

1. **Signature registry dump（A12-#46）** — 极小工程量，永久消除"读 doc → TypeError → 再读 → 再试"这一整类浪费。**性价比第一，先做**。
2. **CSV / JSON → DataTable 批量导入（A8-#31）** — 数据驱动项目的基石，工程量小。
3. **Webhook on completion（A11-#42）** — 一行 HTTP，长跑任务的异步体验全变。
4. **Editor state 快照 / 恢复（A10-#39）** — 让 agent 能"分支尝试"，不再每步不可逆。
5. **Snap-to-surface + 法线对齐（A9-#36）** — Scatter / level dressing 的原子，单独也很好用。
6. **TODO / debug print 审计（A7-#28）** — 发布前清单，小扫描大心安。

### 解锁新物种（中-大工程量，整类能力上线）

这批每个都能解锁一整个工作流，季度级别的大投入。

1. **PIE 视频 / 帧序列抓取（A1-#1）** — 感知从"单帧静态"升到"动作连续性"。所有调试 / 验证 / demo 场景受益。
2. ~~GBuffer 通道截图（A1-#2）~~ ✅ 已交付 2026-04-20。
3. **Golden-image 回归（A6-#23）** — 配合 A1-#1 和 `capture_anim_pose_grid`，agent 第一次有了"改完没破东西"的自证能力。
4. **UE Automation Framework 对接（A6-#21）** — CI 味道的测试一键跑。与 golden-image 互补（一个覆盖渲染，一个覆盖逻辑）。
5. **PIE state 快照（A10-#40）** — 让 agent 能"回到那个关键瞬间"调试 / 比较方案，不用每次从头玩。
6. **Behavior Tree / Blackboard 运行时读（A3-#11）** — AI 行为从黑盒变白盒。
7. **Cook / Package（A5-#19）** — 首次让 agent 能回答"能上线吗"。封装 UAT 即可。
8. **Material Graph 写（A2-#5）** — 打开 shader 领域；Niagara 里很多 module 是 material-like 图，算铺路。

**再之后**：Niagara / Sequencer / AnimGraph-write / GA-graph-write / Control Rig 这五个都是 3-5k 行级别的独立子系统，每个单独作为季度大项目；优先级看用户项目类型（GAS 重度项目优先 GA，过场密集项目优先 Sequencer，动画品类优先 AnimGraph）。

**明确推迟**：引擎源码改动、DCC 联动、marketplace 集成、主观质量判断 — 短期内不解。

---

## 排除项（非 Bridge 职责）

由 LLM 侧处理，不需要 Bridge API：

- 自然语言 → 节点选择（LLM 做）
- 控制流 / 图结构的自然语言摘要（LLM 做）
- 视觉化 SVG / mermaid 图输出（LLM 做）
- 代码风格 / 命名建议（LLM 做）
- 跨项目架构建议（LLM 做，但需要 agent 主动带上下文）

---

## 相关历史文档

- **`docs/plans/agent-capability-gaps-extended.md`**（2026-04-20，扩展模块 B1-B12，41 项）— 本文件之外另一批独立成类的模块：空间查询、UMG 运行时、事务沙箱、Headless / CI、代码生成、动画分析、关卡指标、NNE 集成、构建健康检查、Slate 扩展、perf 分解、replication 内视。与本文件互补。
- `docs/plans/blueprint-capability-roadmap.md`（2026-04-19 版，BP 读写视角，比本文件窄）
- `docs/plans/anim-pose-capture.md`（pose 捕获特性 — 本文件 A1-#2 的前身）
- `docs/plans/reactive-handlers.md`（reactive 事件框架 — 本文件 A3-#11 的同宗扩展）
- `docs/blueprint-edit-gaps.md`（2026-04-14 版，纯写操作视角，已被 blueprint-capability-roadmap 覆盖）

# Agent 对 UE 的操作能力缺口与路线图

盘点 UnrealBridge 作为 agent ↔ UE 编辑器桥梁的能力缺口，按"能补 / 代价高但理论可行 / 根本补不了"三档划分。用于指导后续投资方向。

最后更新：2026-04-20

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
| 2 | **GBuffer / 深度 / 法线 / ID pass** | 中 | 中-高 | 现在 screenshot 只给 final color。GBuffer 通道让 agent 量化判断："actor X 在画面上占多少像素"、"玩家到目标的深度距离"、"屏幕中心指着哪个 actor"。做法：`ASceneCapture2D` 用 `ESceneCaptureSource::SCS_DeviceDepth` / `SCS_WorldNormal` / `SCS_ObjectID`，读 RT → PNG / EXR。 |
| 3 | **Perf 快照结构化输出** | 小-中 | 中 | `stat unit` / GPU frame / draw call count / 内存分 class 分布。做法：封装 `FStatsData` + `FCsvProfiler` + `FMemory::GetStats`，返回 struct 而不是需要解析的字符串。性能回归测试必备。 |
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

按 ROI × 解锁面积：

1. **PIE 视频/帧序列抓取（A1-#1）** — 把 agent 的感知从"偶尔看静态图"升到"能看动作的连续性"。中等工程量，整类调试 / 验证 / demo 场景解锁。
2. **GBuffer 通道截图（A1-#2）** — 把 screenshot 从"能看"升到"能量化测量"。小-中工程量，永久收益。
3. **Material Graph 写（A2-#5）** — 打开整个 shader 领域；Niagara 里很多 module 是 material-like 图，算一并铺路。大工程量但进入门槛后摊薄。
4. **Behavior Tree / Blackboard 运行时读（A3-#11）** — AI 行为从黑盒变白盒。中工程量，所有 AI 项目受益。
5. **Cook / Package（A5-#19）** — 首次让 agent 能回答"能上线吗"。封装 UAT 即可，工程量不大但门槛效应明显。

**剩下**：Niagara / Sequencer / AnimGraph-write / GA-graph-write / Control Rig 这五个都是 3-5k 行级别的独立子系统，每个可作为单独季度的大项目；优先级看用户项目类型（GAS 重度项目优先 GA，过场密集项目优先 Sequencer，动画品类优先 AnimGraph）。

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

- `docs/plans/blueprint-capability-roadmap.md`（2026-04-19 版，BP 读写视角，比本文件窄）
- `docs/plans/anim-pose-capture.md`（pose 捕获特性 — 本文件 A1-#2 的前身）
- `docs/plans/reactive-handlers.md`（reactive 事件框架 — 本文件 A3-#11 的同宗扩展）
- `docs/blueprint-edit-gaps.md`（2026-04-14 版，纯写操作视角，已被 blueprint-capability-roadmap 覆盖）

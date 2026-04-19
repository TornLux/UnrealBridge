# Agent 能力缺口 — 扩展模块（B1-B12）

本文件是 [`agent-capability-gaps.md`](./agent-capability-gaps.md) 的延伸，收录主路线图之外另一批"独立成类、值得单独立项"的模块。

主文件的 Tier A (A1-A12) 列的 48 项是"对现有品类的直接补强"（加写操作、加感知通道、加调试原语等）。本文件的 B1-B12 是**以前完全没想到的独立类别**——它们不是对哪项现有能力的延伸，而是开辟新的维度。

继承主文件的编号系统：编号从 49 开始。

最后更新：2026-04-20

---

## B1. 空间 / 几何查询原语

感知的另一半 — 不是"看到"，是"算到"。现在 agent 要做 gameplay / AI / level 相关的几何判断，只能导出 OBJ 再外部算。bridge-side 查询能一个调用完成 90% 的问题。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 49 | **Line trace / sweep / overlap 查询** | 小-中 | 高 | `line_trace(start, end, channel)`、`sphere_sweep(start, end, radius, channel)`、`box_overlap(center, extent, channel)`。编辑器世界 + PIE 世界都要。返回 actor / component / hit location / normal。 |
| 50 | **Navmesh pathfinding 查询** | 中 | 高 | 现在能导出 OBJ 外部分析，缺的是原地查询：`find_path(start, end) → waypoints + length`、`is_reachable(from, to)`、`get_random_reachable_point(origin, radius)`。AI 脚本 / 关卡分析原子。 |
| 51 | **Bounding / 投影** | 小 | 中-高 | `get_actors_bounds([actors]) → AABB`、`project_world_to_screen(pos) → (px, py, visible)`、`project_screen_to_world(px, py) → ray`。与 GBuffer 截图配合做"actor 占画面多少像素"之类量化判断。 |
| 52 | **体素化 / 占据栅格** | 中-大 | 低-中 | 把关卡几何栅格化成 3D 体素或 2D 占据图，给 AI 规划、路径搜索算法、关卡拓扑分析用。 |

## B2. UMG 运行时状态 / 交互

现在 UMG 只能读静态 widget tree。PIE 中 widget 里装了什么、哪个按钮 enable、焦点在哪——完全黑盒。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 53 | **Runtime widget state 查询** | 中 | 中-高 | "这个 TextBlock 当前显示什么"、"Button X 当前是否 bIsEnabled"、"Focus 在哪个 widget"、"哪些 widget 当前可见"。hook 活的 `UUserWidget` 实例遍历 slot tree。 |
| 54 | **UMG 交互注入** | 中 | 中-高 | 程序化"点击按钮"、"输入字符到 EditableText"、"移动滑条到 0.7"。模拟事件流而不是直接调 OnClicked（要走完整 `FSlateApplication` 路径触发 widget 逻辑）。UI 自动化测试的基础。 |
| 55 | **Navigation graph 分析** | 小 | 低-中 | 手柄 / 键盘导航时焦点如何从 A 跳到 B。静态图可抓，帮菜单可玩性验证。 |
| 56 | **Accessibility probing** | 小-中 | 低 | 对比度、字号、alt text 检查。发布前 a11y 审计清单。 |

## B3. 事务沙箱 / 安全网

现在所有写都是单点 `FScopedTransaction`，跨操作的"保险"缺失。Agent 敢不敢尝试风险操作取决于能否一键回退。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 57 | **Named batch transaction** | 中 | 高 | `begin_batch('try-refactor') → ... → commit_batch() / rollback_batch()`。agent 尝试方案失败能一键回退整批，不依赖 undo 链的线性顺序。 |
| 58 | **Pre-op snapshot** | 小-中 | 中-高 | 改 N 个 BP 前自动 export 副本到 `<Saved>/AgentSnapshots/<timestamp>/`。出事精确还原单个文件，不牵动其它。 |
| 59 | **Quarantine** | 小 | 低 | 检测到 asset 损坏 / schema 不兼容时隔离到 `<Saved>/Quarantine/` 而不是就地覆盖。 |
| 60 | **Diff-before-commit** | 中 | 中-高 | 执行前打印"会改这些文件 / 节点 / 属性"的 preview，agent 或人确认后再落盘。和主文件 A12-#48 "单 call dry-run / trace" 互补：那个是单调用，这个是跨调用 batch。 |

## B4. Headless / Server / CI 模式

现在完全依赖 GUI 编辑器。CI 里跑不起来，团队共用一个编辑器也做不到。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 61 | **Headless editor 启动支持** | 中 | 中 | `UnrealEditor-Cmd.exe` 带 bridge 插件跑，纯命令行驱动。CI 里跑 automation tests / golden-image regression / cook validation 的前提。 |
| 62 | **共享 bridge 服务** | 中-大 | 低-中 | TCP 不再只绑 localhost。团队几个 agent 连一个编辑器实例（读写互斥 + 权限），或每人一个容器跑各自实例。 |
| 63 | **Job queue 模式** | 中 | 中 | "排 50 个 bridge 脚本，按依赖图跑完，每个有超时"。现在 GameThread 串行 exec，并发全卡。需要把重脚本拆成"快注册 + 慢 tick"，让 bridge 侧排队。 |

## B5. 代码生成（源码层）

不是 K2Node 层面，是 C++ 源码层面的脚手架。"给 agent 一个新 Actor 子类"现在得人工开 VS 写。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 64 | **UClass 脚手架生成器** | 中 | 中 | "给我新 AActor 子类 `AEnemySpawner`，带 `BeginPlay` 覆写 + `FSpawnEntry` 数组 + 一个 tick 逻辑骨架"。写 .h + .cpp + 更新 .Build.cs。涉及写项目 Source（主文件 Tier B "项目 C++ 模块" 的下探）。 |
| 65 | **USTRUCT 生成器** | 小-中 | 中 | 给数据形状 → emit C++ USTRUCT + UPROPERTY 标签 + Blueprint 暴露。和 DataTable / gameplay stat 定义搭配。 |
| 66 | **.uplugin / .uproject 编辑** | 小 | 中 | 启用 / 禁用 plugin、加 module 依赖、改项目元数据。现在得手编 JSON 风险高。 |

## B6. 动画 / 运动质量分析

主文件 A2-#8 是 AnimGraph 写，这里是对已有 anim 资产的数值分析。`capture_anim_pose_grid` 给了视觉预览，缺数值判断。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 67 | **骨骼 delta 尖峰检测** | 小 | 中 | 扫每根 bone 的 frame-to-frame 平移 / 旋转导数，找单帧跳跃（mocap 常见瑕疵）。返回 `[{bone, frame, delta}]`。 |
| 68 | **Foot lock / ground penetration 检测** | 中 | 中 | 脚底 socket 在地面以下多少 cm、持续多少帧。retarget 质量的第一指标。 |
| 69 | **Loop seam 检测** | 小 | 中 | 循环动画首尾 pose 差异（bone-space L2 + 速度连续性）。音游 / 待机循环的质量把关。 |
| 70 | **Retarget 质量报告** | 中 | 低-中 | 对比原骨骼和 retarget 目标骨骼每个关节角度偏差，标注问题关节。 |

## B7. 关卡 / gameplay 指标

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 71 | **Playspace 体积 / 面积测量** | 小-中 | 中 | navmesh 总面积、可达房间数（连通分量）、最长对角线、平均 cell 密度。关卡规模客观数据。 |
| 72 | **LOS heatmap** | 中 | 中 | 从 N 个 spawn 点批量 ray scan 可见体积，输出 2D 热力 PNG。FPS / 潜行关卡必做。 |
| 73 | **Cover point 自动提取** | 中 | 低-中 | 边缘几何采样 + 几何分析判断 AI 掩体适合度（高度 / 宽度 / 对面 LOS）。 |
| 74 | **Spawn 密度 / 距离分布** | 小 | 中 | 敌人 spawner 到玩家 spawn 的距离分布、clustering 分析。gameplay 平衡的客观数据。 |

## B8. UE NNE / ONNX 集成

UE 5 自带 Neural Network Engine 插件。bridge 连上就让 agent 在引擎内做视觉推理 / 合成数据管线。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 75 | **Bridge 暴露 NNE** | 中 | 中-高 | `run_onnx_model(path, input_tensor) → output_tensor`。配合 A1-#1 screenshot，agent 对当前画面跑 YOLO / SAM 得检测框 / 分割，不用走外部 vision API round-trip。 |
| 76 | **合成数据生成** | 中 | 低-中 | "在这 scene 扫 1000 个相机 pose，每个 dump 颜色 + depth + object ID mask"，直接得 ML 训练数据。和 B1-#51 投影 + A1-#2 GBuffer 组合。游戏 AI / CV 管线入口。 |

## B9. 构建环境健康检查

被动发现编译问题太慢。preflight 能提前阻止跑 10 分钟后才爆的操作。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 77 | **Preflight 检查器** | 小 | 中 | MSVC / Windows SDK / .NET / 磁盘空间 / GPU / VRAM 一把扫，跑 Cook / rebuild 前判定。之前看到 `MSVC 14.38.33130 required` 警告就是这类该提前抓的。 |
| 78 | **引擎版本漂移检测** | 小 | 低-中 | .uproject 的 `EngineAssociation` 和实际连接 bridge 编辑器版本是否匹配。跨引擎版本切换时 agent 第一件事该查这个。 |
| 79 | **编译产物完整性** | 小 | 中 | `Binaries/*.dll` 时间戳和 `Intermediate/**/*.obj` 是否脱节；判断是否需要 Rebuild 而不是 Build。避免 LC 假成功但 DLL 其实没更。 |

## B10. 编辑器内 Slate 扩展（反向通道）

反过来：让人类用户从编辑器 UI 触发 bridge 调用。很多团队成员不会装 bridge 但会点按钮。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 80 | **Custom editor tab / toolbar button** | 中 | 中 | 编辑器里注册一个"Agent"面板，常用 bridge 操作（hot_reload、lint、format BP、capture viewport）当按钮。`IEditorModule::StartupModule` 里 `FLevelEditorModule::GetToolBarExtensibilityManager()` 挂 hook。 |
| 81 | **Content browser 上下文菜单扩展** | 小-中 | 中 | 右键 asset → "Ask agent about this" / "Run lint" / "Generate test"。`FContentBrowserMenuExtender_SelectedAssets`。 |
| 82 | **Details panel inject** | 中 | 低-中 | 选中 actor 时在 Details 面板里显示 agent 建议（如 "这个 actor 未保存变更 / 引用了已删除 asset"）。`FPropertyEditorModule::RegisterCustomClassLayout`。 |

## B11. 性能 / 资源分解

主文件 A6-#21 Automation 测试 + 本文件 A 扩展里的 perf 快照给总数。这里是**按 owner 分解**——"谁负责这笔开销"。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 83 | **Draw call 按 material 分解** | 中 | 中-高 | 场景里每个 material 贡献多少 draw call / triangle / overdraw。`FPrimitiveSceneProxy` 统计聚合。优化必读。 |
| 84 | **Texture memory 按文件夹 / LOD group 分解** | 小-中 | 中 | `UTexture::GetResourceSizeBytes` 聚合，切 folder / LOD group / class。"UI 贴图 2GB 太多" 这类发现。 |
| 85 | **Actor count by class × level** | 小 | 中 | World partition / level streaming 的决策数据。哪个 sublevel 装了什么类的多少个。 |
| 86 | **Shader permutation 热度** | 中 | 低-中 | 哪些 permutation 真的被 render / cook，哪些可裁。shader cook 时间爆炸时必用。 |

## B12. Replication / 网络内视

多人 bug 定位现在完全看 `LogNet`。结构化视图能让 agent 读懂。

| # | 项目 | 工程量 | 频次 | 说明 |
|---|---|---|---|---|
| 87 | **RPC tracer** | 中 | 中 | hook `UFunction` 的 RPC 路径，记每次 `{sender, receiver, function, payload_size}`。Log 出结构化流，多人 desync / cheat 调查的底。 |
| 88 | **Replication graph 可视化** | 中-大 | 低-中 | 哪些 actor replicate 到哪些 client、每帧 byte cost、relevancy 结果。UE 有 `stat replication` 但不结构化。 |
| 89 | **Client-Server state diff** | 中 | 中 | PIE 多窗口各自 world state dump，结构化比较分歧点（哪个 actor 的哪个属性在 S/C 侧不同）。replication bug 第一把尺。 |

---

## 本文件里最"反直觉高价值"的子集

按"工程量不大但解锁面奇广"排，建议优先从扩展模块里做的：

1. **B1 空间查询原语（#49-#51）** — agent 做任何 gameplay / AI / level 判断的底座，目前全靠导出 OBJ 外部算。一层薄封装，用处无处不在。
2. **B3 事务沙箱 + diff-before-commit（#57 + #60）** — 对 agent "敢不敢尝试"的心理阈值降维打击。不怕改坏了，迭代速度变快。
3. **B10 编辑器 Slate 扩展（#80-#81）** — 反向把 agent 能力织进人类用户的工作流。不装 bridge 的团队成员也能受益。
4. **B4 Headless / CI 模式（#61）** — 让 bridge 进入 build system，PR 自动跑 automation + golden-image + lint。有 agent 的 CI 是完全不同级别的项目。
5. **B8 UE NNE 集成（#75）** — 不出引擎做视觉推理，成本正在变低，合成训练数据管线顺带打开。

---

## 和主路线图的关系

| 维度 | 主文件 (A1-A12) | 本文件 (B1-B12) |
|---|---|---|
| 定位 | 对现有品类的直接补强 | 开辟新维度的独立模块 |
| 优先级 | 用户项目日常直接受益 | 多数是基础设施 / 元能力 |
| 工程量 | 多数中-大（整类子系统） | 多数小-中（单点原语） |
| 决策门槛 | 每项都影响季度规划 | 可机会主义式插入（工作量小） |

**整合建议**：下次规划时，先从本文件的 Top-5（B1 / B3 / B10 / B4 / B8）里各抽一两项搭配主文件 Tier A 的"快速收益"清单，形成一个"基础设施季度"。之后再启动主文件里的大型子系统（Material Graph / Niagara / Sequencer / AnimGraph-write）。

---

## 相关历史文档

- `docs/plans/agent-capability-gaps.md`（主路线图，A1-A12，48 项）
- `docs/plans/blueprint-capability-roadmap.md`（BP 专项，2026-04-19）
- `docs/plans/reactive-handlers.md`（reactive 事件框架）

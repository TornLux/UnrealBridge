<p align="center">
  <h1 align="center">UnrealBridge</h1>
  <p align="center">
    <strong>让 AI Agent 具备控制、编辑 Unreal Engine 的能力。</strong>
  </p>
  <p align="center">
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License"></a>
    <a href="https://www.unrealengine.com/"><img src="https://img.shields.io/badge/Unreal%20Engine-5.3%2B-313131?logo=unrealengine" alt="UE5.3+"></a>
    <a href="https://www.python.org/"><img src="https://img.shields.io/badge/-Python-3776AB?logo=python&logoColor=white" alt="Python"></a>
    <img src="https://img.shields.io/badge/-C%2B%2B-00599C?logo=cplusplus&logoColor=white" alt="C++">
    <img src="https://img.shields.io/badge/platform-Windows-0078D6?logo=windows" alt="Windows">
    <a href="https://claude.ai/code"><img src="https://img.shields.io/badge/Claude%20Code-skill-D97757" alt="Claude Code"></a>
    <a href="README.md"><img src="https://img.shields.io/badge/lang-English-blue" alt="English"></a>
  </p>
</p>

<p align="center">
  <img src="docs/images/zh/01-hook.png" alt="UNREAL ENGINE 5.4+ 全部能力，交给你的 AI Agent">
</p>

---

UnrealBridge 是一个面向 AI Agent 的 Unreal Engine 编辑器桥接层，围绕动画资产内省、Reactive 事件订阅、资产搜索与引用分析、蓝图图谱自动布局等核心场景，提供一套类型化的操作接口。Agent 在本地正在运行的编辑器实例中发起查询与修改，所有变更实时生效，并受事务系统约束、可被撤销。

> **运行边界：** 当前桥接仅支持交互式编辑器会话；当 Unreal Engine 实际运行 Commandlet 时，该模块不会加载。`UnrealEditor-Cmd.exe` 本身并不等同于 Commandlet 边界，是否排除取决于具体启动模式。未来若要在 Commandlet 或其他真正的无头会话中支持 bridge，必须将 Commandlet-safe 服务与交互式编辑器及 Slate 依赖拆分。

## 亮点

- **基于 AST 的防幻觉契约层。** 用户脚本到达 UE 之前，`bridge_preflight.py` 先用 Python AST 解析，对照自动生成的清单（26 个库 × 1382 UFUNCTION）逐一校验每个 `unreal.UnrealBridge*Library.fn(...)` 调用——**不回到编辑器** 就能拦下不存在的库 / 函数名（带 did-you-mean）、错误的位置参数数量、未知关键字、不存在的桥接枚举成员。第二层把 `AssetRegistry` / `GameplayStatics` 的裸调用模式重定向到桥接等价物，并追踪每个返回值的实际类型，在对 `str` / `SoftObjectPath` 这类绑定类型做属性访问时给出警告；UE 对象抛出真正的 `AttributeError` 时则回查 UE Python，列出该类实际反射的 `UPROPERTY` 并给出可粘贴的修正代码（自动处理 `snake_case` ↔ `PascalCase` 的差异）。第三层 ship 一份纯关键字参数的 Python wrapper 模块，让"位置参数顺序写错"在语法层面就不可能发生。三层叠加把新会话 agent 的桥接调用失败率从 **24% 降到 16%**（A/B 验证）——这是先前仅靠 `SKILL.md` 的"调用前先查文档"提示规则一直没能稳定做到的。

  <p align="center">
    <img src="docs/images/zh/02-preflight.png" alt="本地 AST 预检 · 不让幻觉抵达编辑器">
  </p>

- **资产结构深度内省 + 作者级写操作。** `UnrealBridgeAnimLibrary` 覆盖 AnimBP 状态机、AnimGraph 节点、链接层、Slot、曲线、Sequence / Montage / BlendSpace 以及骨骼树的完整查询，并配套一整套写操作：从零搭建 ABP、增删状态 / 转移 / 条件规则、AnimGraph 节点创建与连线、状态机与 AnimGraph 的自动布局；`UnrealBridgeAssetLibrary` 在关键字搜索之外，支持资产的正向依赖与反向引用分析，可向 Agent 输出完整的依赖关系视图。相较于基础 CRUD 封装或需自行拼装反射调用的方案，该层次的结构化能力属于开箱即用。
- **可交付的 Control Rig 与动画重定向工作流。** `UnrealBridgeRigLibrary` 可制作 Control Rig Hierarchy 和完整连线的 RigVM Graph，并在瞬态实例上求值；配置 IK Rig Solver／Goal／Chain；构建 IK Retargeter 的 Op Stack、Mapping、Pose 与 Profile；初始化真实 Retarget Processor、批量生成重定向动画，并对 Root Spike、脚滑／穿地和关节跳变做采样质检。Type／Property 发现和编译／Processor 诊断确保全过程走官方编辑器 Controller，而不是直接改私有数据。
- **可交付的 Niagara 特效工作流。** `UnrealBridgeNiagaraLibrary` 可发现引擎模板与脚本，制作 System／Emitter、Stack Module 与输入、User Parameter、Renderer、材质与 Binding，并通过编译、交付审计和瞬态模拟做质量验收。生产预设覆盖可移动 Ribbon、动态 Beam、静态 Beam 武器 Trail，方向性／放射状火花，含真实冲击波和 Light 层的多层爆炸，以及材质同步的消散／解体粒子。
- **基于 Reactive 系统的事件订阅。** Agent 可订阅 GAS 事件、属性变化、Actor 生命周期、AnimNotify、输入、定时器，以及编辑器端的资产变更事件。在指定事件触发时由桥接层主动回调，无需 Agent 轮询——这是纯请求 / 响应式协议无法覆盖的场景。
- **PIE 运行时的 Agent 控制接口。** `UnrealBridgeGameplayLibrary` 提供聚合式世界观测、导航寻路，以及移动 / 视角 / 跳跃等操作输入，适用于 AI 行为验证、自动化测试、游戏内 NPC 原型等运行时工作流。
- **蓝图工具链。** 不仅仅是自动布局：`auto_layout_graph` 的 `pin_aligned` 策略读取 Slate 实时几何对齐 exec 轨道、`straighten_exec_chain` 把主干拉直、`collapse_nodes_to_function` 提取子图、`lint_blueprint` 按固定规则扫 orphan / 未命名节点 / 过大函数 / 无注释大图，`add_comment_box` + 预设配色（Section / Validation / Danger / Network / UI / Debug / Setup）让图谱分区可读；AnimGraph 与状态机还有专用的 `auto_layout_anim_graph` / `auto_layout_state_machine`（后者递归进入每个状态内部 + 规则图）。
- **Python 原生执行。** 26 个 `UnrealBridge*Library` 累计 1382 个 `UFUNCTION`，覆盖常见子系统；未封装的能力可直接通过 `unreal.*` 原生 API 调用。相较于固定工具列表的 MCP 方案与仅暴露单一 `call` 命令的反射协议，该设计在灵活性与结构性之间取得了折衷。所有关卡写操作均包裹于 `FScopedTransaction` 内，支持标准 Undo / Redo。

## 架构

```mermaid
flowchart LR
    Agent["AI Agent"]

    subgraph Host["Agent 主机"]
      CLI["bridge.py"]
      Pre["AST preflight<br/>（本地 — 调用前拦截，<br/>不发起 TCP）"]
      Mani[("bridge_manifest.json<br/>26 个库 · 1382 UFUNCTION")]
    end

    Gen["tools/gen_manifest.py<br/>扫 C++ 头文件"]

    subgraph UE["Unreal Editor 5.3+"]
      Disc["FUnrealBridgeDiscovery<br/>UDP 应答"]
      Server["FUnrealBridgeServer<br/>TCP · 长度前缀 JSON"]
      Reactive["UnrealBridgeReactiveSubsystem<br/>+ 10 个事件适配器"]
      Exec["IPythonScriptPlugin::<br/>ExecPythonCommandEx<br/>（GameThread）"]
      Wrap["unreal_bridge<br/>kwargs-only 包装<br/>（可选的更安全入口）"]
      Libs["26× UnrealBridge*Library"]
      Engine["UEditor · UWorld · Assets"]
    end

    Agent --> CLI
    CLI -- "AST 拦截" --> Pre
    Pre -. "查表" .-> Mani

    Gen -- "生成" --> Mani
    Gen -- "生成" --> Wrap

    CLI -- "UDP 双路探测<br/>多播 + 回环" --> Disc
    CLI -- "TCP / JSON<br/>（端口由发现得到）" --> Server
    Server -- "RPC 脚本" --> Exec
    Engine -. "委托触发" .-> Reactive
    Reactive -- "事件脚本" --> Exec

    Exec -- "用户代码调用" --> Libs
    Exec -. "或经由" .-> Wrap
    Wrap --> Libs
    Libs --> Engine
```

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/<your-fork>/UnrealBridge.git
cd UnrealBridge
```

### 2. 🚨 跑一次 `link_agents_skills.bat`(一次性)

**使用 Codex / Gemini CLI / OpenCode / Cursor 时必需。** 只用 Claude Code 可以跳过。

Skill 真源在 `.claude/skills/`,这个脚本会创建一个 NTFS junction `.agents/skills/`,让所有遵循 [Agent Skills 开放标准](https://www.agensi.io/learn/agent-skills-open-standard) 的 Agent 客户端都能看到同一份内容。Junction 在 Windows 下无法 commit 进 git,所以每次 clone 都需要在本地物化一次 —— **只需要一次**。

```bat
link_agents_skills.bat
```

Mac / Linux 等价命令:`ln -sfn .claude/skills .agents/skills`(在 repo 根目录跑)。

### 3. 安装插件及对应 Skill

把 UE 项目根目录传给统一同步命令：

```bat
sync_project.bat D:\Path\To\YourProject
```

一次调用会把 `Plugin/UnrealBridge/` 和同版本的
`.claude/skills/unreal-bridge/` 分别同步到项目的插件、`.agents` 与
`.claude` 目标目录，同时保留本地编译产物。每个目标都会记录源码提交和源码
提交。源码仓库的本机路径只记录在项目通常被忽略的
`Saved/UnrealBridge/` 目录下，重载脚本之后可重复执行同一套版本绑定同步，
同时不会把本机路径发布到仓库。`sync_plugin.bat` 仅作为旧命令名别名保留，
参数同样是项目根目录。

### 4. 构建并启动

用 UE 打开 `.uproject` 让它自动重建插件，或从命令行跑项目自带的 `Build.bat`。启动编辑器 —— 插件会在 `PostEngineInit` 拉起服务器，看到日志里出现 `LogUnrealBridge: Listening on 127.0.0.1:<端口>` 就算成功。端口由 OS 分配，客户端同时通过多播和本机回环探测自动找到它，不需要手动配置。

### 5. 验证

```bash
python .claude/skills/unreal-bridge/scripts/bridge.py ping
# → pong
python .claude/skills/unreal-bridge/scripts/bridge.py exec \
  "import unreal; print(unreal.UnrealBridgeLevelLibrary.get_level_summary())"
```

### Claude Code 集成（可选）

把 skill 拷到 Claude Code 能发现的位置：

```bash
cp -r .claude/skills/unreal-bridge ~/.claude/skills/            # 用户级
# 或拷进目标项目自己的 .claude/skills/
```

想让 `rebuild_relaunch.py` 自动重启编辑器，需设置其中之一：

```bash
setx UNREAL_EDITOR_EXE "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
setx UE_ROOT            "C:\Program Files\Epic Games\UE_5.7"
```

### 快速使用

skill 装好之后，把下面任意一句丢进 Claude Code 对话：

- *「列出当前关卡里所有的 PointLight。」*
- *「把 PlayerStart 向上移动 200 单位。」*
- *「编译 `/Game/Blueprints/BP_Character`，告诉我有没有报错。」*
- *「看看 `/Game/Animations/ABP_Hero` 里有哪些状态机。」*
- *「为 `SK_Mannequin` 创建一个 ABP，里面放一个 Idle / Walk / Run 状态机，转移规则用 `Speed` 变量（>10 进 Walk、>200 进 Run），外层再叠一个 Slot + LayeredBoneBlend 混入上半身覆盖动画。」*

Agent 会读 `SKILL.md`，挑出对应的 `UnrealBridge*Library` 函数，通过 `bridge.py` 发起调用，再把结果告诉你。

## 使用方式

### CLI

```bash
bridge.py ping
bridge.py exec "print('hello from UE')"
bridge.py exec-file my_script.py
```

参数（全部可选，常规使用可以一个都不传）：

- `--project=<名称|路径>` —— 同时跑多个编辑器时用来挑一个
- `--endpoint=host:port` —— 跳过发现直连（或环境变量 `UNREAL_BRIDGE_ENDPOINT`）
- `--token=<密钥>` —— 仅当 server 绑定非 loopback 时需要（或 `UNREAL_BRIDGE_TOKEN`）
- `--timeout`（默认 30 秒）、`--json`、`--discovery-timeout=<ms>`（默认 800）

`bridge.py list-editors` 发一次探测并列出所有响应的编辑器 —— 多编辑器场景的诊断快捷命令。

### 在 UE 的 Python 里调用

```python
import unreal

summary = unreal.UnrealBridgeLevelLibrary.get_level_summary()
print(summary)

lights = unreal.UnrealBridgeLevelLibrary.find_actors_by_class(
    "/Script/Engine.PointLight", 50
)
print(len(lights), "个点光源")
```

### 两种重载方式

```bash
python .claude/skills/unreal-bridge/scripts/hot_reload.py        # 只改函数体
python .claude/skills/unreal-bridge/scripts/rebuild_relaunch.py  # 动到反射
```

安装到项目中的 Skill 会自动识别所在 UE 项目，以及 `sync_project.bat`
记录的源码仓库。从源码仓库直接运行时需传
`--project-dir D:\Path\To\YourProject`；只有移动源码仓库导致记录路径失效时
才需要额外传 `--sync-source`。

## 桥接库

| 库 | 作用 |
|---|---|
| `UnrealBridgeServer` | TCP 监听、长度前缀 JSON 帧、派发到 GameThread |
| `UnrealBridgeRigLibrary` | UE 5.7+ 的完整 Control Rig／IK 交付能力：Control Rig 资产、Hierarchy 与 Control 制作；可发现的 RigVM Unit／Template、成员变量、节点、Pin、连线、注释与自动布局；编译诊断和瞬态事件求值；IK Rig Solver／Goal／Chain／排除骨骼、反射设置与 Humanoid/FBIK 自动配置；IK Retargeter Op Stack、Chain Mapping、Pose、Profile、真实 Processor 初始化与可保存的批量重定向；Root Motion、脚部接触和关节跳变采样质检。UE 5.3-5.6 保留同一反射与 kwargs API，调用时输出明确日志并安全返回。 |
| `UnrealBridgeNiagaraLibrary` | UE 5.7+ 的完整 Niagara／VFX 交付能力：模板与脚本发现；System／Emitter 生命周期和声明式 Recipe；含本地、Linked、Dynamic、Object、Data Interface 输入的 Stack Module；User Parameter；Sprite／Ribbon／Mesh／Light／Decal／Component Renderer、材质和 Binding；Warmup、Fixed Bounds、Effect Type；编译诊断与交付审计；Trail／Beam、方向性／放射状 Sparks、多层 Explosion／Shockwave／Light 和 Dissolve 预设；支持变量、控制与移动 Transform 的瞬态预览，以及逐 Emitter 粒子数、内存和 CPU 回读。UE 5.3-5.6 保留同一反射与 kwargs API，调用时输出明确日志并安全返回。 |
| `UnrealBridgeBlueprintLibrary` | 蓝图全栈读写：类层级 / 变量 / 函数 / 组件 / 接口 / 事件分发器；图谱的调用关系、执行流、引脚连接、节点搜索；20+ 类节点插入（Branch、Cast、循环、Delay、Timer、SpawnActor、MakeStruct 等）、引脚连接、节点坐标读写、对齐、注释框、AutoLayoutGraph；运行时调试 —— 断点增删查、`get_last_breakpoint_hit` 捕获函数 locals / params / return **加上**执行对象的 BP 类实例变量（带 `OwnerClass` 归属）、PIE node coverage；编译错误查询 |
| `UnrealBridgeAssetLibrary` | 资产关键字搜索（支持 include / exclude 词元）；派生类查询；正向依赖与反向引用分析（含递归）；DataAsset / StaticMesh / SkeletalMesh / Texture / Sound 元信息；支持按索引、槽名或原子批量修改 StaticMesh/SkeletalMesh 默认材质（可用 `save=false` 临时预览）；目录树、重定向解析、批量 tag 与磁盘大小查询；**SearchableName 索引查询**（`find_assets_referencing_searchable_name` / `get_searchable_names_used_by_asset` / `list_searchable_name_values`）—— 编辑器右键 "Find References" 在 `GameplayTag` / `PrimaryAssetId` / 任意 USTRUCT 索引命名值上拿到的就是这套数据 |
| `UnrealBridgeAnimLibrary` | AnimBP 深度内省：状态机、AnimGraph 节点、链接层、Slot、曲线；Sequence / Montage / BlendSpace 资产信息；骨骼树、Socket、VirtualBone、BlendProfile。**写操作**：ABP 创建与变量、状态机 / 状态 / 导管 / 转移的增删改、转移属性（crossfade、优先级、双向）、常量规则捷径与真实变量驱动规则（配合 BP 库写 `KismetMathLibrary` 比较节点）、9 类 AnimGraph 节点工厂 + `add_anim_graph_node_by_class_name` 兜底、引脚连线 / 断开 / 移位、AnimGraph 与状态机的自动布局；AnimNotify、同步标记、Montage Section、Socket 的增删配置 |
| `UnrealBridgePoseSearchLibrary` | Motion Matching —— `UPoseSearchSchema` / `UPoseSearchDatabase` 内省：schema 通道与权重、数据库动画条目、采样 / 分支采样、索引状态（`wait-pose-index` CLI 辅助）；针对运行时 pose 向量做匹配评估。`DatabaseAnimationAssets` / `Channels` 在 C++ 里是 `private:`，`get_editor_property` 拿不到 —— 这个库是唯一通路 |
| `UnrealBridgeChooserLibrary` | Motion Matching —— `UChooserTable` 内省与作者：列、行（含 disabled 标记与解析后的结果）、上下文对象、NestedChooser 下钻（`:Name` 路径）。写操作：增删列 / 行、设置上下文类型时自动 Compile + PostEditChange 让编辑器刷新。`ResultsStructs` / `DisabledRows` 在 C++ 是 `private:` —— 这个库是唯一通路 |
| `UnrealBridgeStateTreeLibrary` | StateTree 全流程读写、调试与运行时控制（UE 5.7+）：资产创建与 Schema 查询；以稳定 GUID 驱动状态、节点、条件、考量项和转移的 CRUD；按 Schema 发现原生/蓝图节点类型；可发现的 export-text 属性编辑；属性绑定、根/状态参数、完整编译诊断、临时断点，以及实时 `UStateTreeComponent` 查询、生命周期控制和事件投递。UE 5.3-5.6 暴露安全桩 |
| `UnrealBridgeSmartObjectLibrary` | Smart Object 完整作者、世界与运行时能力（UE 5.7+）：Definition 创建/校验；以稳定 GUID 驱动 Slot、Behavior、Definition Data 与 Annotation 编辑；Tag Query/策略；按 Schema 过滤的 World Condition；参数与属性绑定；世界组件和 Persistent Collection；空间查询、动态实例、Claim/Occupy/Release、运行时 Tag/启用/事件，以及入口查找与离线校验。UE 5.3-5.6 暴露安全桩 |
| `UnrealBridgeStructLibrary` | `UUserDefinedStruct`(UDS) 的作者级写接口 —— 创建 asset、字段 CRUD（增删改名 / 改类型 / 重排 / 改默认值 / tooltip / 每字段 edit on instance）。底层走 `FStructureEditorUtils`，所有写自动触发重编译 + 下游 BP 传播。`UserDefinedStructureFactory` 与 `StructureEditorUtils` 在 native Python 没暴露，这个库是程序化构造 DataTable / BP 变量可引用的 UDS 的唯一通路 |
| `UnrealBridgeDataTableLibrary` | DataTable 行级读写与条件过滤；CSV / JSON 导入导出 —— 文件路径变体之外，新增 in-memory text 变体（适合 LLM 生成的 CSV/JSON 内容），外加 `create_data_table_from_csv` / `create_data_table_from_json` 一把梭从 row struct 直接造新 DataTable（支持 `/Game/...` 路径或短类名）；表间行复制、行差异比对；按 RowStruct 反查引用该结构的所有表 |
| `UnrealBridgeCurveLibrary` | 曲线资产（`UCurveFloat` / `UCurveVector` / `UCurveLinearColor`）与 `UCurveTable` 行的读写：asset info、键 CRUD（批量 + 原子切线写）、前 / 后无穷外推模式、Auto 切线重算、批量采样（N 个时间点一次往返）、等距采样；曲线表行的增删改查与重命名。写操作广播 `OnCurveChanged` 让打开的 Curve Editor 即时刷新 |
| `UnrealBridgeMaterialLibrary` | 材质实例参数查询 |
| `UnrealBridgeUMGLibrary` | Widget Blueprint 完整交付能力：资产创建、控件类发现与层级 CRUD、控件/Slot 反射属性、Canvas 布局和 SlateBrush 资源；批量透明度/颜色/2D Transform 动画；编译、可访问性与陈旧引用校验；UE 5.7+ 的 FieldNotify ViewModel 创建、Source 配置、单向/双向 MVVM 绑定及诊断（5.3-5.6 为带明确日志的安全 no-op）；PIE 实例生成、实时几何/状态回读、Button/Text/Slider/CheckBox/焦点语义操作、动画播放、动态 UI 材质参数与实时 MVVM 写入 |
| `UnrealBridgeLevelLibrary` | Actor 查询（名称 / Class / Tag / Folder / 半径 / Box / 射线）与编辑（生成 / 销毁 / 变换 / 挂载 / 可见性 / Mobility、嵌套属性读写、函数调用）；地形高度剖面与 Trace 探测；编辑器内自定义 NavGraph（节点、边、最短路径、JSON 持久化）；正交俯视图与动画 Pose / Montage 时间轴截图；所有写操作走事务 |
| `UnrealBridgeEditorLibrary` | 编辑器会话控制：资产开关 / 保存 / 加载；Content Browser 与视口；PIE 启停 / 模拟 / 暂停；Undo / Redo、控制台命令、CVar；蓝图批量编译、重定向修复；Live Coding 触发；原始视口截图与包含单帧调试覆盖层的所见即所得已呈现窗口截图、GBuffer 通道（Depth / DeviceDepth / Normal / BaseColor）与 HitProxy ID pass；标签页、通知、诊断信息。Bridge 自观测：调用日志（请求 ID、耗时、端点、输出大小的环形缓冲）、性能统计、签名注册表 JSON dump（一次性输出全部 1384 个反射 `UFUNCTION` 的元信息） |
| `UnrealBridgeGameplayAbilityLibrary` | GameplayAbility / GameplayEffect / AttributeSet 蓝图元信息；Tag 层级与匹配；按 Tag 列出能力与效果；Actor 的 ASC 状态（属性值、激活 Ability / Effect、Cooldown 检查）；运行时发送 GameplayEvent、修改属性；GA / GE / GC 蓝图作者支持（CDO 编辑、GA 图节点、GE magnitude / component / 继承 Tag、GC Tag 设置） |
| `UnrealBridgeGameplayTagLibrary` | GameplayTag 重构工作流：`find_assets_referencing_tag`（支持子 tag 展开）、`list_all_registered_tags`、`get_tag_source_info`。Mutation：`add_gameplay_tag` / `rename_gameplay_tag`（自动写 redirect，并针对 UE 5.7 的"redirect 静默丢失"问题做了持久化加固） / `remove_gameplay_tag`。源枚举 `list_tag_source_inis`；redirect 管理 `list_gameplay_tag_redirects` + `remove_gameplay_tag_redirect`，支持 enumerate-then-sweep 清理 |
| `UnrealBridgePerfLibrary` | AAA 量级性能采集，八个维度。**Point-in-time**：帧时序（FPS / GT / RT / GPU / RHI ms，`FStatUnitData` + RHI globals 双源）、渲染计数器、进程内存、`TObjectIterator` 类直方图、ISO-8601 时间戳聚合快照。**内存 / 资产分解**：texture / mesh / audio / UObject 按 folder / LOD group / compression format / class 分组——支持 disk 或 runtime 两种模式；任意 UClass 下 top-N 最大资产；world-actor 按 class × level 分布（World Partition 部分支持）。**时间序列**：opt-in 周期采样配 ring buffer，常开的 frame-time 直方图与 hitch log（走 `OnEndFrame` hook），CSV 导出，**`get_frame_time_percentiles([50,90,95,99])`** 拿 AAA 项目真实长尾延迟。**渲染细分**：per-actor 渲染成本、LOD 分布、按 material 聚合 primitive、shadow caster、Lumen / Nanite 诊断；**`get_texture_streaming_residency`**（per-texture resident vs wanted mip + pool over-budget）、**`get_render_target_memory`**（per-subclass RT 字节总量）、**`get_per_pass_gpu_timings`**（BasePass / Lumen / Translucency 平均时长，源 `FRealtimeGPUProfiler`；UE 5.7 新 RHI profiler 上优雅降级）、**`analyze_all_materials`**（跨库结构复杂度启发式，找最重的 master）。**Live trace 控制**：`start_trace_capture` / `stop_trace_capture` / `list_trace_channels` / `get_trace_state` 封装 `FTraceAuxiliary`。**Trace summary 解析**（5.7+）：`parse_trace_to_summary` 一次调用从 `.utrace` 解出 CPU + GPU 热点 + per-thread 热点 + counters + load-time 分解 + 帧统计；专项 `parse_alloc_trace_to_summary`（peak commit + tag inventory + alloc/free delta）、`parse_net_trace_to_summary`（per-game-instance + per-connection 流量总量）、`parse_cook_trace_to_summary`（top-N 包按 `BeginCacheCookedPlatformData` 排序——给 4 小时 cook 归因）。**回归工作流**：`compare_perf_snapshots(before, after, threshold)` 返回 per-field delta + flagged regression 列表；`begin_auto_hitch_capture` / `end_auto_hitch_capture` 对每帧 ≥ 阈值的现场抓 rich snapshot 进 ring buffer；`begin_insights_for_trace` shell 出 UnrealInsights.exe 把人接进来 |
| `UnrealBridgeGameplayLibrary` | PIE 运行时 Agent 控制：聚合式世界观测、导航寻路；移动 / 视角 / 跳跃 / 传送 / 粘性输入、Enhanced Input **运行时注入加 IA / IMC 枚举与 IMC mapping 作者**（`list_input_actions` / `list_input_mapping_contexts` / `get_input_mapping_context_mappings` / `add_ia_mapping_to_imc` / `remove_ia_mapping_from_imc` —— binding 这一侧之前只能裸 `unreal.*` 调）；Pawn 速度、能力、跳跃轨迹模拟；相机射线、屏幕 ↔ 世界、NavMesh 投影；伤害、物理冲量、时间膨胀、音效、摄像机抖动；Debug 绘制；AI 控制器探测 |
| `UnrealBridgeNavigationLibrary` | NavMesh 导出为 OBJ，便于外部可视化与几何分析 |
| `UnrealBridgeProceduralLibrary` | 程序化内容作者原语 —— point-list-in / point-list-out 的采样 + 过滤 + instancing，跑在编辑器世界。给定 `(params, seed)` 确定可复现：`FRandomStream(Seed)` + `ECC_Visibility` + `bTraceComplex=true`；Poisson-2D / 网格 / 径向 / spline / 网格表面采样器；坡度 / 最近距离 / 蒙版过滤；ISM / HISM 批量生成；Landscape 网格 + project-to-surface（作为普通 Python 数组调用 —— 故意不是 PCG-graph 包装） |
| `UnrealBridgeGeometryLibrary` | Geometry Script 封装 —— `UDynamicMesh` 句柄池 + 跨引擎资产 I/O（`copy_mesh_from_static_mesh` / `create_new_static_mesh_asset_from_mesh`）+ 25+ 操作覆盖图元 / 布尔 / 平滑 / 减面 / 位移 / 体素合并 / UV 展开 / bake 法线 + 遮蔽 / 拉伸 / sweep-along-spline / 选择。字段名走标准 UE Python snake_case（`bHasNormals` → `.has_normals`） |
| `UnrealBridgePCGLibrary` | PCG（程序化内容生成）只读 + 触发 —— 不做图编辑（PCG 的领地；agent 写代码不画图）。组件 override get / set、generate / cleanup、资产图内省。整库 5.7+ gate；5.3-5.6 用 stub |
| `UnrealBridgeReactive*` | 事件订阅框架，10 个 adapter：运行时（GameplayEvent、AttributeChanged、ActorLifecycle、MovementMode、AnimNotify、InputAction、Timer）与编辑器（AssetEvent、PieState、BpCompiled）；Handler 的注册 / 列表 / 暂停 / 恢复 / 统计；跨会话 JSON 持久化。替代轮询 |
| `UnrealBridgePropertyLibrary` | **特权级通用 UPROPERTY 接口。** 用 `Foo.Bar[N].Baz` 点路径读写任意反射字段 —— 绕开 UE Python 绑定层的访问检查（"is protected and cannot be read" 报错、struct 副本上 EditDefaultsOnly 子字段写入被拒,这正是 GE `Modifiers[0].ModifierMagnitude.ScalableFloatMagnitude.Value` 之类嵌套写入卡死的根因）。`list_u_properties` 返回完整反射(private/protected/裸 UPROPERTY + 解码后的 EPropertyFlags + metadata 全表)；`array_append_u_property` 自动识别 FGameplayTagContainer 维护 ParentTags 缓存；`get_asset_cdo_path` 正确解析 CDO 路径。写操作包 `FScopedTransaction` + 可选 `PostEditChangeChainProperty` 让编辑器实时刷新。 |

## 协议

两个通道：

1. **UDP 发现**：端口 `9876`。客户端把带同一 request_id 的 `probe`（可带 project 过滤器）同时发往局域网多播 `239.255.42.99` 和本机回环 `127.0.0.1`，并按编辑器 PID 去重。多播保留局域网/多编辑器发现，回环探测则避免 Windows、VPN 或虚拟网卡丢弃多播回环时导致本机发现失效。每个编辑器单播回自己的 TCP 绑定地址 + 端口 + token 指纹，多编辑器通过 `SO_REUSEADDR` 共存。

2. **TCP 数据通道**：端口由编辑器在发现响应里给出（OS 分配，默认 `127.0.0.1`）。长度前缀 JSON：

```
请求:  [4 字节大端长度][{"id","script","timeout","token?"}]
响应:  [4 字节大端长度][{"id","success","output","error"}]
Ping:  {"id","command":"ping"}  →  pong
```

当 server 绑定非 loopback 时自动启用 token 鉴权；客户端从 `<Project>/Saved/UnrealBridge/token.txt` 读取并在每个请求里带上。

脚本在 GameThread 上执行；捕获的 stdout 与 stderr 通过特殊分隔符 `__UB_ERR__` 区分。

### 服务器配置（CLI / 环境变量 / `EditorPerProjectUserSettings.ini [UnrealBridge]`）

| CLI | 环境变量 | 默认 |
|---|---|---|
| `-UnrealBridgeBind=` | `UNREAL_BRIDGE_BIND` | `127.0.0.1` |
| `-UnrealBridgePort=` | `UNREAL_BRIDGE_PORT` | `0`（OS 分配） |
| `-UnrealBridgeToken=` | `UNREAL_BRIDGE_TOKEN` | 空（非 loopback 时必填） |
| `-UnrealBridgeDiscoveryGroup=` | `UNREAL_BRIDGE_DISCOVERY_GROUP` | `239.255.42.99:9876` |
| `-UnrealBridgeNoDiscovery` *(flag)* | `UNREAL_BRIDGE_DISCOVERY=0` | 默认开启 |

## 仓库结构

```
UnrealBridge/
├── Plugin/UnrealBridge/         # UE 5.3+ 编辑器插件(C++)
│   ├── Source/UnrealBridge/     #   TCP 服务器 + 桥接库
│   └── Content/Python/          #   UE Python 环境自动载入的辅助脚本
├── .claude/skills/unreal-bridge/
│   ├── scripts/                 # bridge.py、hot_reload.py、rebuild_relaunch.py
│   └── references/              # 各库 API 文档
├── docs/                        # 设计文档与规划
├── tools/                       # 独立小工具
├── sync_project.bat             # 把插件与对应 Skill 镜像进 UE 项目
└── sync_plugin.bat              # sync_project.bat 的旧命令名别名
```

## 系统要求

- **Unreal Engine 5.3+**,需启用 `PythonScriptPlugin` 与 `GameplayAbilities`(均为引擎自带)。`tools/build_matrix.py` 已对 5.3.2 / 5.4.4 / 5.5.4 / 5.6.1 / 5.7.1 / 5.8.0 验证 BuildPlugin 通过;部分库(Chooser / PoseSearch / Material / Navigation / StateTree / Smart Object / Control Rig、IK 与 Niagara)、UMG MVVM 及少量独立 UFUNCTION 需要 5.7+,低版本仍保留 StateTree／Smart Object／Rig／Niagara 安全桩和带明确日志的 UMG MVVM 安全 no-op,5.3 与 5.8 各有少量 inline shim,详见 [docs/version-compatibility.md](docs/version-compatibility.md)。UE 5.2 及更早版本不支持。
- **Windows 10/11** —— 插件本身可移植,但辅助脚本里的路径按 Windows 风格写死
- **Python 3.9+**,已加入 PATH
- **Visual Studio 2022** + UE 工作负载 —— 用于编译插件。**Toolchain 注意事项:**
  - **5.5 / 5.6 / 5.7 / 5.8** 在当前 MSVC 下能编(已在 **14.44.35207** / VS 17.14 上验证)。
  - **5.3 / 5.4** 需要 `_MSC_VER ≤ 1939` 的 MSVC(已在 **14.38.33130** / VS 17.8 上验证)。新 MSVC 在引擎自身的 `ConcurrentLinearAllocator.h` 触发 `C4668: '__has_feature' 未定义`,被 5.3 / 5.4 的 UBT 用 `/we4668` 升成硬错误;5.5+ 把 macro 用 `defined()` 包了一层并去掉了 `/we4668`。如果机器上两套工具链都装了,想验 5.3 / 5.4:在 `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` 把 `<CompilerVersion>` 临时改成 `14.38.33130`,跑完再改回去。
  - **5.8 源码自编引擎**(不是 Launcher 装的)如果缺 `Setup.bat` 通常下发的 `UbaDetours.dll`,需要关 UBA。在 `engines.local.json` 对应引擎条目里加 `"env": { "UnrealBuildTool_BuildConfiguration__bAllowUBAExecutor": "false" }`,UBT 会回落到本地 ParallelExecutor。
- **Claude Code CLI** —— 可选,只有使用自带 skill 时才需要

## 安全

- 所有关卡编辑操作都包在 `FScopedTransaction` 里 —— 编辑器内按 Ctrl+Z 可以撤销桥接做过的任何改动。
- TCP 服务器**默认绑到 `127.0.0.1`**，外网不可达。要开放 LAN 必须显式 `-UnrealBridgeBind=0.0.0.0 -UnrealBridgeToken=<密钥>`；非 loopback 绑定若未提供 token，server 会拒绝启动以避免 Python RCE 外漏。

## 许可证

MIT —— 见 [LICENSE](LICENSE)。

---

<p align="center">
  <img src="docs/images/zh/03-outro.png" alt="UnrealBridge — 把 Unreal Engine 编辑器变成 AI Agent 的可编程界面">
</p>

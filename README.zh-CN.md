<p align="center">
  <h1 align="center">UnrealBridge</h1>
  <p align="center">
    <strong>让 AI Agent 具备控制、编辑 Unreal Engine 的能力。</strong>
  </p>
  <p align="center">
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License"></a>
    <a href="https://www.unrealengine.com/"><img src="https://img.shields.io/badge/Unreal%20Engine-5.7-313131?logo=unrealengine" alt="UE5.7"></a>
    <a href="https://www.python.org/"><img src="https://img.shields.io/badge/-Python-3776AB?logo=python&logoColor=white" alt="Python"></a>
    <img src="https://img.shields.io/badge/-C%2B%2B-00599C?logo=cplusplus&logoColor=white" alt="C++">
    <img src="https://img.shields.io/badge/platform-Windows-0078D6?logo=windows" alt="Windows">
    <a href="https://claude.ai/code"><img src="https://img.shields.io/badge/Claude%20Code-skill-D97757" alt="Claude Code"></a>
    <a href="README.md"><img src="https://img.shields.io/badge/lang-English-blue" alt="English"></a>
  </p>
</p>

---

UnrealBridge 是一个面向 AI Agent 的 Unreal Engine 编辑器桥接层，围绕动画资产内省、Reactive 事件订阅、资产搜索与引用分析、蓝图图谱自动布局等核心场景，提供一套类型化的操作接口。Agent 在本地正在运行的编辑器实例中发起查询与修改，所有变更实时生效，并受事务系统约束、可被撤销。

## 亮点

- **资产结构深度内省 + 作者级写操作。** `UnrealBridgeAnimLibrary` 覆盖 AnimBP 状态机、AnimGraph 节点、链接层、Slot、曲线、Sequence / Montage / BlendSpace 以及骨骼树的完整查询，并配套一整套写操作：从零搭建 ABP、增删状态 / 转移 / 条件规则、AnimGraph 节点创建与连线、状态机与 AnimGraph 的自动布局；`UnrealBridgeAssetLibrary` 在关键字搜索之外，支持资产的正向依赖与反向引用分析，可向 Agent 输出完整的依赖关系视图。相较于基础 CRUD 封装或需自行拼装反射调用的方案，该层次的结构化能力属于开箱即用。
- **基于 Reactive 系统的事件订阅。** Agent 可订阅 GAS 事件、属性变化、Actor 生命周期、AnimNotify、输入、定时器，以及编辑器端的资产变更事件。在指定事件触发时由桥接层主动回调，无需 Agent 轮询——这是纯请求 / 响应式协议无法覆盖的场景。
- **PIE 运行时的 Agent 控制接口。** `UnrealBridgeGameplayLibrary` 提供聚合式世界观测、导航寻路，以及移动 / 视角 / 跳跃等操作输入，适用于 AI 行为验证、自动化测试、游戏内 NPC 原型等运行时工作流。
- **蓝图图谱质量工具链。** 不仅仅是自动布局：`auto_layout_graph` 的 `pin_aligned` 策略读取 Slate 实时几何对齐 exec 轨道、`straighten_exec_chain` 把主干拉直、`collapse_nodes_to_function` 提取子图、`lint_blueprint` 按固定规则扫 orphan / 未命名节点 / 过大函数 / 无注释大图，`add_comment_box` + 预设配色（Section / Validation / Danger / Network / UI / Debug / Setup）让图谱分区可读；AnimGraph 与状态机还有专用的 `auto_layout_anim_graph` / `auto_layout_state_machine`（后者递归进入每个状态内部 + 规则图）。
- **Python 原生执行。** 13 个 `UnrealBridge*Library` 累计约 990 个 `UFUNCTION`，覆盖常见子系统；未封装的能力可直接通过 `unreal.*` 原生 API 调用。相较于固定工具列表的 MCP 方案与仅暴露单一 `call` 命令的反射协议，该设计在灵活性与结构性之间取得了折衷。所有关卡写操作均包裹于 `FScopedTransaction` 内，支持标准 Undo / Redo。

## 架构

```mermaid
flowchart LR
    Agent["AI Agent"]
    CLI["bridge.py"]
    Server["FUnrealBridgeServer"]
    Libs["UnrealBridge*Library<br/>(13 个库, ~990 UFUNCTION)"]
    UE["Unreal Editor 5.7"]

    Agent -- "shell" --> CLI
    CLI -- "TCP / JSON<br/>127.0.0.1:9876" --> Server
    Server -- "GameThread<br/>Python 派发" --> Libs
    Libs --> UE
```

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/<your-fork>/UnrealBridge.git
cd UnrealBridge
```

### 2. 安装插件

修改 `sync_plugin.bat` 里的 `DST`，指向你 UE 项目的 `Plugins/` 目录：

```bat
set "DST=D:\Path\To\YourProject\Plugins\UnrealBridge"
```

运行 `sync_plugin.bat`，它会把 `Plugin/UnrealBridge/` 镜像进项目，并跳过 `Binaries/` 与 `Intermediate/`。

### 3. 构建并启动

用 UE 打开 `.uproject` 让它自动重建插件，或从命令行跑项目自带的 `Build.bat`。启动编辑器 —— 插件会在 `PostEngineInit` 拉起服务器，看到日志里出现 `LogUnrealBridge: Listening on 127.0.0.1:9876` 就算成功。

### 4. 验证

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

参数：`--host`、`--port`（默认 9876）、`--timeout`（默认 30 秒）、`--json`。

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

## 桥接库

| 库 | 作用 |
|---|---|
| `UnrealBridgeServer` | TCP 监听、长度前缀 JSON 帧、派发到 GameThread |
| `UnrealBridgeBlueprintLibrary` | 蓝图全栈读写：类层级 / 变量 / 函数 / 组件 / 接口 / 事件分发器；图谱的调用关系、执行流、引脚连接、节点搜索；20+ 类节点插入（Branch、Cast、循环、Delay、Timer、SpawnActor、MakeStruct 等）、引脚连接、节点坐标读写、对齐、注释框、AutoLayoutGraph；编译错误查询 |
| `UnrealBridgeAssetLibrary` | 资产关键字搜索（支持 include / exclude 词元）；派生类查询；正向依赖与反向引用分析（含递归）；DataAsset / StaticMesh / SkeletalMesh / Texture / Sound 元信息；目录树、重定向解析、批量 tag 与磁盘大小查询 |
| `UnrealBridgeAnimLibrary` | AnimBP 深度内省：状态机、AnimGraph 节点、链接层、Slot、曲线；Sequence / Montage / BlendSpace 资产信息；骨骼树、Socket、VirtualBone、BlendProfile。**写操作**：ABP 创建与变量、状态机 / 状态 / 导管 / 转移的增删改、转移属性（crossfade、优先级、双向）、常量规则捷径与真实变量驱动规则（配合 BP 库写 `KismetMathLibrary` 比较节点）、9 类 AnimGraph 节点工厂 + `add_anim_graph_node_by_class_name` 兜底、引脚连线 / 断开 / 移位、AnimGraph 与状态机的自动布局；AnimNotify、同步标记、Montage Section、Socket 的增删配置 |
| `UnrealBridgeDataTableLibrary` | DataTable 行级读写与条件过滤；CSV / JSON 导入导出；表间行复制、行差异比对；按 RowStruct 反查引用该结构的所有表 |
| `UnrealBridgeMaterialLibrary` | 材质实例参数查询 |
| `UnrealBridgeUMGLibrary` | UMG 控件树、属性、动画、绑定、事件查询；按名称 / 类搜索控件；属性写入 |
| `UnrealBridgeLevelLibrary` | Actor 查询（名称 / Class / Tag / Folder / 半径 / Box / 射线）与编辑（生成 / 销毁 / 变换 / 挂载 / 可见性 / Mobility、嵌套属性读写、函数调用）；地形高度剖面与 Trace 探测；编辑器内自定义 NavGraph（节点、边、最短路径、JSON 持久化）；正交俯视图与动画 Pose / Montage 时间轴截图；所有写操作走事务 |
| `UnrealBridgeEditorLibrary` | 编辑器会话控制：资产开关 / 保存 / 加载；Content Browser 与视口；PIE 启停 / 模拟 / 暂停；Undo / Redo、控制台命令、CVar；蓝图批量编译、重定向修复；Live Coding 触发；截图、GBuffer 通道（Depth / DeviceDepth / Normal / BaseColor）与 HitProxy ID pass；标签页、通知、诊断信息。Bridge 自观测：调用日志（请求 ID、耗时、端点、输出大小的环形缓冲）、性能统计、签名注册表 JSON dump（一次性输出全部 ~990 个 `UFUNCTION` 的元信息） |
| `UnrealBridgeGameplayAbilityLibrary` | GameplayAbility / GameplayEffect / AttributeSet 蓝图元信息；Tag 层级与匹配；按 Tag 列出能力与效果；Actor 的 ASC 状态（属性值、激活 Ability / Effect、Cooldown 检查）；运行时发送 GameplayEvent、修改属性；GA / GE / GC 蓝图作者支持（CDO 编辑、GA 图节点、GE magnitude / component / 继承 Tag、GC Tag 设置） |
| `UnrealBridgePerfLibrary` | 结构化性能快照：帧时序（FPS / GT / RT / GPU / RHI ms，支持 stat-unit 与 raw 两种模式）、渲染计数器（draw calls / primitives，跨 GPU 求和）、进程内存、`TObjectIterator` 类直方图、ISO-8601 时间戳聚合快照。USTRUCT 直出，无需解析 `stat unit` 文本 |
| `UnrealBridgeGameplayLibrary` | PIE 运行时 Agent 控制：聚合式世界观测、导航寻路；移动 / 视角 / 跳跃 / 传送 / 粘性输入、Enhanced Input 与 MappingContext；Pawn 速度、能力、跳跃轨迹模拟；相机射线、屏幕 ↔ 世界、NavMesh 投影；伤害、物理冲量、时间膨胀、音效、摄像机抖动；Debug 绘制；AI 控制器探测 |
| `UnrealBridgeNavigationLibrary` | NavMesh 导出为 OBJ，便于外部可视化与几何分析 |
| `UnrealBridgeReactive*` | 事件订阅框架，10 个 adapter：运行时（GameplayEvent、AttributeChanged、ActorLifecycle、MovementMode、AnimNotify、InputAction、Timer）与编辑器（AssetEvent、PieState、BpCompiled）；Handler 的注册 / 列表 / 暂停 / 恢复 / 统计；跨会话 JSON 持久化。替代轮询 |

## 协议

`127.0.0.1:9876` 上的长度前缀 JSON：

```
请求:  [4 字节大端长度][{"id","script","timeout"}]
响应:  [4 字节大端长度][{"id","success","output","error"}]
Ping:  {"id","command":"ping"}  →  pong
```

脚本在 GameThread 上执行；捕获的 stdout 与 stderr 通过特殊分隔符 `__UB_ERR__` 区分。

## 仓库结构

```
UnrealBridge/
├── Plugin/UnrealBridge/         # UE 5.7 编辑器插件(C++)
│   ├── Source/UnrealBridge/     #   TCP 服务器 + 桥接库
│   └── Content/Python/          #   UE Python 环境自动载入的辅助脚本
├── .claude/skills/unreal-bridge/
│   ├── scripts/                 # bridge.py、hot_reload.py、rebuild_relaunch.py
│   └── references/              # 各库 API 文档
├── docs/                        # 设计文档与规划
├── tools/                       # 独立小工具
└── sync_plugin.bat              # 把插件镜像进 UE 项目
```

## 系统要求

- **Unreal Engine 5.7**，需启用 `PythonScriptPlugin` 与 `GameplayAbilities`（均为引擎自带）
- **Windows 10/11** —— 插件本身可移植，但辅助脚本里的路径按 Windows 风格写死
- **Python 3.9+**，已加入 PATH
- **Visual Studio 2022** + UE 工作负载 —— 用于编译插件
- **Claude Code CLI** —— 可选，只有使用自带 skill 时才需要

## 安全

- 所有关卡编辑操作都包在 `FScopedTransaction` 里 —— 编辑器内按 Ctrl+Z 可以撤销桥接做过的任何改动。
- TCP 服务器只绑定到 `127.0.0.1`，外网不可达。

## 许可证

MIT —— 见 [LICENSE](LICENSE)。

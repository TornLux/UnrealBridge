# EDA 集成路线图

目标:让 UnrealBridge 在 UE 5.8+ 上**寄生** Epic 官方 EDA(Editor Development Assistant)框架,**白嫖**它的基础设施红利(Editor Slate 面板、MCP 协议暴露、客户端配置生成、analytics、official 背书),同时**保留** UnrealBridge 自身在 token 经济学、API 深度、AST 防幻觉、heredoc 多步合并方面的护城河。

最后更新:2026-05-29(v0.2 — 同步 `TornLux/UnrealBridge#2` 的 stdio MCP wrapper 与 `bridge.py mcp-config` 现状；UE 5.8 / EDA 正式集成仍保持低优先级)

---

## 战略定位

EDA 框架(`<ue-source>/Engine/Plugins/Experimental/ModelContextProtocol/` + `<ue-source>/Engine/Plugins/Experimental/Toolsets/*/` + `<ue-source>/Engine/Plugins/Experimental/AIAssistant/` + `<ue-source>/Engine/Plugins/Experimental/ToolsetRegistry/`)是 Epic 在 2026-Q1 至 2026-Q2 集中合入的 LLM Agent 接入官方框架,共 21 个 Experimental + NoRedist + EnabledByDefault=false 插件。架构大致:

- **`ModelContextProtocol`** — Anthropic MCP 服务器实现,把 Editor 暴露给外部 MCP 客户端(Claude Desktop/Cursor/Codex 等)
- **`ToolsetRegistry`** — 工具注册中枢,所有 toolset 把自己的 tool calls 注册到这里,MCP server 从这里查询可用工具
- **`AIAssistant`** — Editor 内 Slate 面板,Python 后端,UI 端的 AI 助手主入口
- **`MCPClientToolset`** — MCP 客户端适配器,让 UE 反过来连外部 MCP server
- **`AllToolsets`** — 伞形聚合插件,一键启用全部 16 个内建 toolset(AIModule / AnimationAssistant / AutomationTest / Conversation / DataflowAgent / GameFeatures / GameplayTags / GAS / LiveCoding / MCPClient / Niagara / Physics / SequencerAnimMixer / SlateInspector / StateTree / UMG / WorldConditions)

EDA 与 UnrealBridge 的关系**不是替代,是互补**。两者的目标用户和能力深度不同,可以共存于同一个项目,UnrealBridge 走"在 EDA 之上的深度 power-user 通道"路线。

---

## 战略原则(核心 — 偏离即停)

1. **加法不做减法** — 借 EDA 的基础设施,**绝不**为了像 EDA 而稀释 UnrealBridge 的现有差异化(1020 UFUNCTION 深度、AST preflight、kwargs wrapper、heredoc 多步、Reactive 推送、Pose Search/Chooser private 字段、Perf 8 维度、特权 UPROPERTY)。
2. **不破坏 token 经济学** — UnrealBridge 选 RPC 不选 MCP-everything 是基于 token 经济的有意决策(详见 `feedback_unrealbridge_rpc_over_mcp` memory)。1020 个 UFUNCTION 注册成 1020 个 MCP tool descriptor 会吃掉 20-40 万 tokens 常驻 system prompt;**正确做法是在 EDA 体系里只暴露 1 个 `exec_python` tool**,把 Python 当 schema。
3. **5.3-5.7 老用户不能丢** — 现有 TCP+UDP discovery 通道继续维护,EDA 集成只针对 5.8+ 增量启用。
4. **优先寄生不要自建** — EDA 已经做了的(Slate UI、MCP 协议、analytics、客户端配置生成),走它的接口蹭红利;EDA 没做的(Niagara/Sequencer/Dataflow 等领域 toolset),也优先 thin wrapper 路由进 EDA 而不是自己重写。
5. **5.8 适配先行** — 任何 EDA 集成的前置条件是 UnrealBridge 在 5.8 上跑通(API drift 审计 + build matrix 加 5.8.0)。

---

## 当前交付状态

| 阶段 | 状态 |
|---|---|
| P0 ToolsetRegistry 接入 | ⏸ 未开始 |
| P1 EDA toolset 兼容 shim | ⏸ 未开始 |
| P1 5.8 适配(前置) | ⏸ 未开始 |
| P2 MCP 客户端配置生成器 | ✅ stdio wrapper 路径已完成；EDA endpoint 自动生成待 P0 后再评估 |
| P3 独立 MCP 薄壳(退路) | ✅ Python stdio wrapper 已完成；UE 侧 MCP server 不再优先 |
| P4 三模块拆分(可选) | ⏸ 未开始 |

---

## 里程碑拆分

### P0 — 订阅 EDA `ToolsetRegistry`(最高优先,核心招)

订阅 UE 5.8 ships 的 `ToolsetRegistry` 接口,把整个 UnrealBridge 注册成**一个**(注意:不是 21 个、不是 1020 个)叫 "UnrealBridge" 的 toolset,内部声明**单一** tool — `exec_python(script: str, timeout?: float)`。这一招打通后,EDA 的 `AIAssistant` Slate 面板自动看到 UnrealBridge,EDA 的 MCP server 自动把 UnrealBridge 暴露给所有 MCP 客户端。零额外协议工作量,蹭到 EDA 的全部基础设施。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| P0-1 | 新增 `Plugin/UnrealBridge/Source/UnrealBridgeEDA/` 模块,只在 `Target.Version >= 5.8` 时编译(`*.Build.cs` 用 `EngineVersion.Major >= 5 && EngineVersion.Minor >= 8` 守门) | 小 | 模块加载阶段 `PostEngineInit`,声明软依赖 `ToolsetRegistry` plugin |
| P0-2 | 实现 `FUnrealBridgeToolsetProvider : public IToolsetProvider`,在 `RegisterTools()` 里只注册一个 tool:`unreal_bridge.exec_python` | 小 | input schema 单参数 `script: string` + 可选 `timeout: number`;output schema `{success, output, error}` 直接转发 TCP 路径已有结构 |
| P0-3 | tool handler 内部调用 `IPythonScriptPlugin::ExecPythonCommandEx`,**复用** `UnrealBridgeServer` 的 GameThread dispatch 路径 + `__UB_ERR__` stdout/stderr 分隔约定 | 小 | 不复制代码 — 抽出 `RunPythonOnGameThread(Script, Timeout) → FBridgeResult` 共用函数 |
| P0-4 | tool description 内嵌入"reference 索引"提示文本(~300 tokens):`"21 UnrealBridge*Library available via unreal.UnrealBridgeXxxLibrary.fn(...). See bridge_manifest.json for full surface; see references/bridge-*-api.md for usage patterns."` | 小 | 这是给 LLM 的 hint,告诉它怎么用 — 比注册 1020 个 tool descriptor 节省 20 万 tokens |
| P0-5 | 在 EDA `AIAssistant` 面板里手动验证 — 启用 UnrealBridge + ModelContextProtocol + AIAssistant 三个插件,看 UnrealBridge 是否出现在面板的 toolset 列表里 | 小 | 验证步骤,不是开发 |
| P0-6 | 验证用 Claude Desktop 通过 EDA 的 MCP server 调到 UnrealBridge 的 exec_python — 跑 `unreal.UnrealBridgeLevelLibrary.get_level_summary()` 通过 | 小 | 端到端 smoke test |
| P0-7 | 默认关闭(`EnabledByDefault: false`) — 用户主动启用,不破坏现有 5.3-5.7 用户的工作流 | 小 | uplugin 配置 |

**完成定义**:Claude Desktop 用户在 5.8 项目里启用 UnrealBridge + EDA 三件套后,**零额外配置**就能在面板里看到 UnrealBridge 出现并调用成功。

---

### P1a — 5.8 适配(P0/P1b 的前置)

UnrealBridge 当前 build matrix 验证到 5.7.1,5.8 主线已发布(2026-Q1+ 持续 commit)。在做任何 EDA 集成前必须先跑通 5.8。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| P1a-1 | 5.7→5.8 API drift 审计 — 拿 `tools/build_matrix.py` 加 5.8.0 编译,记录 deprecated / 改名 / 删除的 API | 中 | 重点关注 Anim / GAS / PoseSearch / Chooser / PCG 这些深度领域;PCG 模块在 5.8 可能有破坏性变化 |
| P1a-2 | 修复 BuildPlugin 报错;必要时加 `#if UE_VERSION_NEWER_THAN(5,8,0)` shim,优先用条件编译保持 5.3-5.7 兼容 | 中-大 | 视 drift 量级而定,预估 1-3 天 |
| P1a-3 | `tools/build_matrix.py` 加入 5.8.0,跑通 BuildPlugin clean | 小 | matrix 报告更新到 `build_matrix_report.md` |
| P1a-4 | `version-compatibility.md` 加 5.8 列,记录任何条件能力差异 | 小 | 已有文档框架 |
| P1a-5 | 验证 `MSVC 14.40+`(MSVC 14.38 在 5.8 主线触发 C7539,需 BuildConfiguration.xml 强制 `<Compiler>VisualStudio2022</Compiler>` + `<CompilerVersion>14.44.35207</CompilerVersion>` 之类) | 小 | 已有完整文档 — 把工具链要求加进 README 的 Requirements 段 |

**完成定义**:`tools/build_matrix.py` 在 5.3.2 / 5.4.4 / 5.5.4 / 5.6.1 / 5.7.1 / 5.8.0 全列 pass。

---

### P1b — EDA 独有 toolset 的兼容 shim

EDA 覆盖了 9 个 UnrealBridge 没有的领域(Niagara / SequencerAnimMixer / SlateInspector / GameFeatures / WorldConditions / Conversation / Dataflow / AutomationTest / PhysicsAsset / AIModule),自己重写性价比不高。直接 thin wrapper 把它们的 Python API 包成 `unreal.UnrealBridge*Library` 形态,让 UnrealBridge 用户在 5.8 上自动多出这些领域的能力。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| P1b-1 | 调研 EDA 9 个 toolset 各自的 Python API 签名(它们是 EditorOnly 模块,Python 端可能有 `unreal.<Module>` 直接调用) | 小 | 阅读 `<ue-source>/Engine/Plugins/Experimental/Toolsets/<ToolsetName>/Source/` 下的 .h |
| P1b-2 | `Plugin/UnrealBridge/Content/Python/unreal_bridge/eda_compat.py` — 9 个 thin wrapper class(Niagara / Sequencer / Dataflow / GameFeatures / WorldConditions / Conversation / AutomationTest / PhysicsAsset / AIModule),每个 class 转发到对应 EDA Python API | 中 | 5.8+ 才 import 加载,5.7 下提供 stub raise NotImplementedError |
| P1b-3 | 加进 wrapper module 的 `__all__`,生成器 `tools/gen_manifest.py` 扫描时识别 EDA shim 类(可能需要白名单标记) | 小 | manifest 里这些函数标记 `eda_shim=true` 让 preflight 在 5.7 下报清楚错 |
| P1b-4 | reference docs:`.claude/skills/unreal-bridge/references/bridge-eda-niagara.md` 等 9 篇,每篇 100-200 行,主要是 EDA toolset 文档的"翻译 + UnrealBridge 风格化" | 中 | 不重写,引用 EDA 的官方 README 段落 |
| P1b-5 | SKILL.md 加一段 "EDA-shim 模式" 说明 — 何时走 EDA shim、何时走 UnrealBridge 原生 | 小 | 默认:UnrealBridge 原生覆盖的领域用原生(深度更好);EDA 独有的领域走 shim |

**完成定义**:5.8 上 `from unreal_bridge.eda_compat import Niagara, Sequencer, Dataflow, ...` 能调用 9 个 EDA toolset 的核心能力。

---

### P2 — MCP 客户端配置生成器

EDA 在 2026-04-09 commit `[ModelContextProtocol] Add console command to generate MCP client configuration files` 加了一个 console 命令一键生成 MCP 客户端配置。UnrealBridge 抄一份。

2026-05-29 更新：当前 #2 PR 已用 `bridge.py mcp-config` 交付 stdio MCP wrapper 的配置片段生成器，可输出
`generic` / `claude-desktop` / `cursor` / `codex` / `openclaw` / `hermes` 对应的 JSON / TOML / YAML 配置形态。
当前测试覆盖的是 no-editor 配置片段形态与 token 不回显，不代表这些外部客户端都已安装并完成端到端连接。该实现不依赖 UE 5.8，也不会打印 `UNREAL_BRIDGE_TOKEN`。下表中 EDA endpoint 自动生成只在 P0
ToolsetRegistry 路径落地后再评估。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| P2-1 | `bridge.py mcp-config --client=claude-desktop\|cursor\|codex\|generic\|openclaw\|hermes` 子命令,输出可直接粘贴的配置片段 | 已完成 | `docs/mcp-stdio-wrapper.md` 与 `tools/test_bridge_mcp_config.py` 覆盖 JSON / TOML / YAML 输出形态；未声明真实客户端端到端验收 |
| P2-2 | 配置内容:当前先指向 stdio MCP wrapper；未来如 P0 落地,再补 EDA MCP endpoint 自动生成 | 部分完成 | 当前支持 `--project` / `--endpoint` / `--discovery-group` 写入 server env |
| P2-3 | `bridge.py mcp-config --output=<path>` 直接写文件;默认 stdout | 暂缓 | 当前只输出 stdout,避免直接改写用户客户端配置 |
| P2-4 | README 加"快速接入 Claude Desktop"段,3 行命令: `mcp-config` → 复制 → 重启 Claude Desktop | 部分完成 | README / docs 已给 `mcp-config` 示例；Claude Desktop 专门教程可等上游反馈后补 |

**当前完成定义**:用户可以用一行 `bridge.py mcp-config` 生成可粘贴的 MCP 客户端配置片段；是否进一步写入客户端文件,等待上游反馈后再决定。

---

### P3 — 独立 MCP 薄壳(退路,如 P0 充分则不必做)

P0 走通后,5.8 用户通过 EDA 蹭到 MCP 已经够。但如果用户想在 **5.7** 上也用 MCP 客户端(不能用 EDA,因为 EDA 是 5.8 only),需要 UnrealBridge 自带一个独立 MCP server。

2026-05-29 更新：当前 #2 PR 已经交付 Python stdio MCP wrapper，5.7 用户可以通过客户端启动
`.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py`，再由它委托 `bridge.py` 走现有
TCP bridge。这个路径满足“不新增 UE 侧 transport、不走 HTTP、不复制 bridge 逻辑”的约束，因此下表中的
UE 侧 MCP server 模块不再作为近期优先项。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| P3-1 | `Plugin/UnrealBridge/Source/UnrealBridgeMCPShell/` 独立模块,实现一个最小 UE 侧 MCP server | 不再优先 | 当前 Python stdio wrapper 已覆盖 5.7 客户端接入需求,且避免新增 UE 侧 transport |
| P3-2 | 只暴露 1-3 个 tool:`exec_python` / `preflight` / `list_libraries` | 小 | 同 P0 设计原则 |
| P3-3 | 每个 tool handler 转发到现有 TCP server | 小 | 复用 P0 抽出的共用函数 |
| P3-4 | `bridge.py mcp-server` 启动模式 | 不再优先 | 当前以 `unrealbridge_mcp_server.py` 作为 stdio 子进程入口；不规划 HTTP transport |
| P3-5 | `version-compatibility.md` 注明:5.8 用户**优先**走 P0(原生 EDA 寄生),5.7 及以下用户走 P3(独立薄壳) | 小 | 决策树 |

**当前完成定义**:5.7 用户可通过 stdio MCP wrapper 调用 UnrealBridge。UE 侧 MCP server 只有在上游明确要求“插件内置 MCP server”时再重新评估。

---

### P4 — 三模块拆分(可选,长期演进)

EDA 的 `ModelContextProtocol` 在 2026-04-07 commit `[ModelContextProtocol] Decouple MCP server from Engine, split into three-module architecture` 拆成 `ModelContextProtocol` + `ModelContextProtocolEngine` + `ModelContextProtocolEditor` + 各自 Tests 模块。UnrealBridge 当前所有 21 个 Library 都在一个 `UnrealBridge` 模块里。

借鉴价值:
- Cooked build 时 Editor-only 代码不进 runtime 模块
- Tests 模块独立,不污染主依赖图
- 模块间依赖关系清晰

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| P4-1 | 设计模块切分:`UnrealBridgeRuntime`(Server/Discovery/共用类型) + `UnrealBridgeEditor`(21 个 Library + Reactive)+ `UnrealBridgeTests` | 中 | 现在 21 个 Library 全是 EditorOnly,主要拆 Server 和 Library |
| P4-2 | 重构 `*.Build.cs` + 模块入口,处理跨模块的 UCLASS 引用 | 中 | 有 break-changes 风险 — UFUNCTION 全限定名变化可能影响 Python `unreal.UnrealBridgeXxxLibrary` 引用 |
| P4-3 | 验证 build matrix 全列仍然 pass + reference docs 不变 | 小 | 回归测试 |

**完成定义**:模块依赖图清晰,但**不动 Python 端 import 路径**。

**优先级**:P4 是长期演进,代码量翻倍前不必做。

---

## 不做(明确排除)

| 不做 | 原因 |
|---|---|
| **自建 Slate UI 面板** | P0 跑通后白嫖 EDA 的 `AIAssistant` 面板;重复造没有价值 |
| **把 1020 UFUNCTION 注册成 1020 个 MCP tool** | token 灾难(20-40 万 tokens 常驻 system prompt);违背 UnrealBridge 的核心设计哲学;详见 `feedback_unrealbridge_rpc_over_mcp` memory |
| **抛弃现有 TCP/UDP 通道,纯走 MCP** | 5.3-5.7 用户全部失联;heredoc 多步合并的 token 优势丢失;AST preflight 集成路径中断 |
| **把 UnrealBridge 改成 EDA toolset PR 提交给 Epic** | NoRedist + Experimental + Epic 风格收紧 + Reactive/Perf/Pose Search 这些深度活 Epic 短期不会接受;1020 个函数白扔 |
| **重写已有 Niagara / Sequencer / Dataflow 的能力** | EDA 已经做了,P1b shim 直接路由更经济 |

---

## 风险与权衡

### R1 — EDA 演进可能吃掉差异化

**风险**:Epic 投入 EDA 速度很快(2026-Q1 起每周新 commit),长期可能在 BP / Anim / GAS 这些 UnrealBridge 优势领域追上来。

**应对**:
- UnrealBridge 的护城河里,**Epic 没动机做的部分**(token 经济、AST 防幻觉、Reactive 推送、Pose Search private 字段、Perf AAA 级、特权 UPROPERTY)是真正的差异化。盯紧 EDA 是否在这些领域投入,如果是,差异化缩短;如果不是(更可能),反而拉大。
- 走 P0 + P1b 的策略本身是 hedge — 即便 EDA 把 BP/Anim 也吃了,UnrealBridge 仍然是"在 EDA 之上"的深度通道。

### R2 — `ToolsetRegistry` API 不稳定

**风险**:EDA 全部 `IsExperimentalVersion: true`,API 可能在 5.9 / 5.10 大改。

**应对**:
- P0 的 `UnrealBridgeEDA` 模块用条件编译守门,某个版本 API 破坏性改动时只需要适配该模块,不影响主体
- reference docs 注明 EDA 集成是 experimental 状态,跟随 Epic 版本

### R3 — 5.8 适配工作量超预期

**风险**:5.7→5.8 的 API drift 实际可能比预估大,P1a 拖慢 P0/P1b。

**应对**:
- P1a 每个领域(Anim / GAS / PoseSearch / PCG / Chooser)单独做条件能力 stub,先让 build matrix 过 5.8.0,再逐步恢复深度功能
- 接受短期内 5.8 上某些领域是 "scaffold" 状态,不阻塞 P0 ToolsetRegistry 接入

### R4 — token-efficient single-tool 模式不被 EDA 视为合规

**风险**:EDA 的 `IToolsetProvider` 接口可能假设 toolset 注册多个细粒度 tool,只注册一个粗粒度 `exec_python` 可能在 UI 层面表现不佳(EDA 的 AIAssistant 面板可能按 tool 数量做某种 grouping)。

**应对**:
- 在 P0-1 设计阶段先看 EDA 现有 toolset 的注册模式 — 看 EDA 自己的 16 个 toolset 平均注册多少 tool。如果 EDA 假设 N×多个 tool,UnrealBridge 可以**轻度妥协**注册 3-5 个粗粒度 tool(如 `exec_python` + `query_assets` + `query_blueprint` + `query_anim` + `query_perf`),每个 tool 仍然以自由文本 Python 为主参数,但在 UI 上看起来更像"多 toolset"。token 代价仍然 < 1000(对比 1020 tool 的 20-40 万 tokens)。

### R5 — 与 EDA `AIAssistant` 面板的 Python 命名冲突

**风险**:EDA 的 `AIAssistant` 也是基于 Python + EditorScripting,可能和 UnrealBridge 的 Python helper 共享命名空间冲突。

**应对**:
- UnrealBridge Python 包用 `unreal_bridge` 顶层命名空间,本身已经隔离
- 反射 API 调用走 `unreal.UnrealBridgeXxxLibrary` 形态,Epic 自己的 toolset 走 `unreal.<EDA_Toolset>` 形态,无 collision

---

## 执行顺序建议

按照 ROI 顺序:

```
1. P1a (5.8 适配)        ←─── 前置,必须先做
2. P0 (ToolsetRegistry)  ←─── 一招吃下大半 EDA 红利
3. P1b (EDA toolset shim) ←─── 9 个领域白嫖
4. P2 (MCP 配置生成器)    ←─── stdio wrapper 路径已交付;EDA endpoint 待 P0 后补
5. P3 (独立 MCP 薄壳)     ←─── Python stdio wrapper 已覆盖 5.7 接入;UE 侧 server 暂不做
6. P4 (三模块拆分)        ←─── 长期演进,不紧迫
```

**预计工作量**:
- P1a:2-5 天
- P0:2-3 天
- P1b:1-2 天
- P2:已交付 stdio wrapper 配置生成;EDA endpoint 自动生成待 P0 后评估
- P3:已交付 Python stdio wrapper;UE 侧内置 MCP server 暂不估时
- P4:2-3 天(可选)

**最小可见交付**(MVP):P1a + P0 = 4-8 天,UnrealBridge 在 5.8 上跑通 + 通过 EDA 暴露给所有 MCP 客户端。

---

## 决策记录

| 日期 | 决策 | 备选方案 | 选择理由 |
|---|---|---|---|
| 2026-05-10 | P0 选 ToolsetRegistry 寄生而不是自建 MCP server | 自建 MCP server(P3) | EDA 已经有 Slate UI + analytics + 客户端配置生成器等基础设施,寄生比自建省 1-2 周工作 |
| 2026-05-10 | 单一 `exec_python` tool 而不是按 21 个 Library 注册 21 个 tool | 21 个 tool(粗粒度)/ 1020 个 tool(细粒度) | token 经济学最优;1020 tool 直接 ban(详见 feedback memory);21 tool 在 EDA UI 上可能还是不够,因每个 tool 还是 Python free-form 入参 |
| 2026-05-10 | EDA 独有 9 个 toolset 走 shim 不重写 | 重写 / 不覆盖 | EDA 已 working,shim 1-2 天搞定;重写每个领域 1 周以上 |
| 2026-05-10 | 5.3-5.7 维持 TCP 通道,**不**强制走 MCP | 全切 MCP / 5.7 弃用 | 老用户基数仍然大;TCP 路径更便宜;EDA 是 5.8 only,不能强求 |
| 2026-05-29 | 5.7 MCP 接入先走 Python stdio wrapper + 现有 TCP bridge | UE 侧内置 MCP server | 满足 Codex、OpenClaw、Hermes 等 stdio MCP 客户端接入;不新增 UE 侧 transport,不走 HTTP,不复制 bridge 逻辑 |
| 2026-05-29 | `bridge.py mcp-config` 只输出片段,暂不直接写客户端配置文件 | 自动写 `config.toml` / `claude_desktop_config.json` | 避免误改用户全局配置和泄露 token;上游反馈需要安装器时再做写文件模式 |

---

## 引用

- `<repo-root>/CLAUDE.md` — UnrealBridge 架构总览
- `<repo-root>/README.md` — 协议、Library 列表、能力清单
- `<repo-root>/docs/version-compatibility.md` — UE 5.x 兼容性矩阵
- `<repo-root>/docs/plans/agent-capability-gaps.md` — 已识别的能力缺口(可能与 P1b 重叠)
- `<ue-source>/Engine/Plugins/Experimental/ModelContextProtocol/` — EDA MCP server 实现参考
- `<ue-source>/Engine/Plugins/Experimental/ToolsetRegistry/` — `IToolsetProvider` 接口源
- `<ue-source>/Engine/Plugins/Experimental/AIAssistant/` — AIAssistant Slate 面板,P0 的目标 UI 容器
- `<ue-source>/Engine/Plugins/Experimental/Toolsets/AllToolsets/AllToolsets.uplugin` — 16 个内建 toolset 列表
- 相关 EDA git commits(主线分支):
  - `2026-03-24` ModelContextProtocol 移入 Engine/Plugins/Experimental
  - `2026-04-01` AllToolsets 伞形插件创建,16 个 toolset 集中迁入
  - `2026-04-07` MCP 服务器拆三模块
  - `2026-04-09` console 命令生成 MCP 客户端配置(P2 借鉴)
  - `2026-04-13` AutomationTestToolset(LLM 触发自动化测试)
  - `2026-04-22` MCP analytics instrumentation
  - `2026-04-23` LiveCodingToolset 独立成插件

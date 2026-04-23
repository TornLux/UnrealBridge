# Material 能力拓展路线图

目标：把 Material 从目前"只能读 MI 参数"拓展到 agent 能独立完成完整材质任务。典型任务形态：

- **创建**：给一个自然语言描述（"角色铠甲母材质"、"植被主材质"、"素描风后处理"），agent 生成完整 Material graph 并编译通过
- **观察**：对任意 Material / MI，能"看到"它长什么样（当前外观 + PBR 正确性 + 着色复杂度）
- **优化**：识别 AAA + 性能问题（指令数 / sampler 槽位 / 冗余查表 / 静态分支遗漏），给出修正建议或自动修复
- **改参**：改 MI 或 Master 的参数 → 立刻拿到新截图 → 判断效果是否达标 → 迭代

硬约束：成品质量对齐 AAA 项目常见实践（SM5+ / Lumen / Nanite 就绪、正确的 ShadingModel / MaterialDomain / 纹理压缩 / sampler 复用 / 静态分支），性能口径按"不退化 GPU 时长、不超 sampler/ instruction 预算"衡量。

最后更新：2026-04-24（v0.2 — M1/M2/M2.5/M6 全部落地；M3 6/9 模板、M4 3/5 模板、M5 7 条规则 + auto_fix 已交付；下面的状态总览按 commit 同步）

---

## 当前交付状态（按里程碑）

| 里程碑 | 状态 | 备注 |
|---|---|---|
| M1 读 / 观察 | ✅ 全部交付 | get_material_info / graph / stats / compile_errors / preview / preview_complexity / list_functions / get_function / list_instance_chain / get_parameter_collection |
| M2 表达式工厂 + 图写原语 | ✅ 全部交付 | create_material / MI / MF + add_material_expression (35+ 类) + connect/disconnect (pin name 现在走 GetShortenPinName) + set_prop / add_comment / add_reroute / auto_layout / apply_material_graph_ops / compile_material / snapshot + diff |
| M2.5 HLSL 片段库 | ✅ 全部交付 | BridgeSnippets.ush + add_custom_expression + list / get 共享片段；现有 snippet：Luminance, Unpack/Pack ORM, ACES, BlendAngleCorrectedNormals (已修成 `-1..1` 约定), DepthFade, DitherLODTransition, Hash21/31, ValueNoise3D, ThinFilmInterference, FBM3D, IQFlow3D, SwirledNoise3D, Voronoi2D |
| M6 参数迭代闭环 | ✅ 全部交付 | set_mi_params / set_mi_and_preview / sweep / MPC setter / diff / golden snapshot+compare |
| M3 母材质模板 | 🟡 6 / 9 模板 | **已交付**：M3-2 Character_Armor、M3-3 Environment_Prop、M3-4 Foliage_Master、M3-6 Glass_Translucent、M3-8 UI_Unlit. **未交付**：M3-1 Character_PBR (可视作 M3-2 的精简子集，优先级低)、M3-5 Weapon_Hero (需 POM HLSL 片段)、M3-7 Layered (材质层系统)、M3-9 VFX (Unlit Additive / Translucent Soft) |
| M4 后处理材质 | 🟡 3 / 5 模板 + 全部 C++ 原语 | **已交付**：create_post_process_material / apply / remove / get_post_process_state + PP_Posterize、PP_Halftone、PP_Outline (4-neighbour depth gradient). **未交付**：PP_Sketch (需 BridgeSobelEdge snippet + 交叉线)、PP_ColorGradeLUT_Extended、PP_Film_Grain_AA |
| M5 Lint / 自动修复 | 🟡 7 / 13 规则 + auto_fix | **已交付**：analyze_material 聚合 + M5-2 预算、M5-3 不可达节点、M5-4 重复 TextureSample、M5-5 SamplerSource 混用、M5-8 ShadingModel↔wiring、M5-11 Custom body trivia、M5-12 Custom 内部 SceneTextureLookup 绕过分析；auto_fix_material 支持 `drop_unused` + `samplersource_share`. **未交付**：M5-6 静态开关误用、M5-7 feature level / quality switch 缺失、M5-9 MI chain depth、M5-10 texture compression 合规、M5-13 Custom SM-only intrinsic 缺 FeatureLevelSwitch、auto_fix 里的 `static_switch_conversion` / `inline_trivial_custom` |

### 顺带交付（不在原路线图但相关）

- **bridge 基础设施**：UDP 多播发现 (`239.255.42.99:9876`) + TCP 端口 `0` (OS 分配) + 可选 token 鉴权。`bridge.py` / `bridge_discovery.py` / SKILL.md / README.md 全部同步更新。
- **pin name 兼容层**：`NormalizePinName` + `get_material_graph` 现在都走 `UMaterialGraphNode::GetShortenPinName`，"Coordinates" / "AGreaterThanB" / "TextureObject" 等长名自动短化成 UI-可见的 "UVs" / "A > B" / "Tex"，读写对称。这条是 M4 开发时发现的隐性坑，已修。
- **模板共享基建**：`material_templates._common` — `OpList` 支持符号名 → `$N` 解析、未知名称早爆 KeyError；`ensure_master_material(rebuild=True)` 幂等重建；`guid_to_str()` 避开 UE Python `str(unreal.Guid)` 返回 `<Struct>` 的坑；`save_master()` 显式 asset.save (apply_ops compile=True 只编译不保存会在编辑器重启时丢失模板)。

---

## 现状基线（2026-04-22）

`UnrealBridgeMaterialLibrary` 只有一个函数：`get_material_instance_parameters(path)`，返回 MI 的参数覆盖列表。

更广阅能力分布：
- **读 Master material**：零（`unreal.MaterialEditingLibrary` 暴露了 `get_material_expressions` / `get_material_property_input_node` 等 Python-side，但 bridge 没有结构化封装，也没有统计类信息）
- **写 Master material**：零（Python 侧 `MaterialEditingLibrary.create_material_expression` / `connect_material_expressions` 可用，但只能覆盖常用 30+ 种 `UMaterialExpression` 中的一部分；Material Functions / Material Layers / 静态开关 / CustomHLSL / Feature Level Switch 覆盖不齐）
- **预览 / 截图**：`LevelLibrary.capture_*` 可以对场景 actor 截图，但没有"单独预览 Material 在标准球体 / 平面上"的一键能力（缺 `FPreviewScene` + 指定 mesh + 环境灯）
- **统计**：零（指令数 / sampler 数 / texture lookup 数 / 每 feature level 成本 / 编译错误 — 全部没暴露）
- **MI 创建 / 反向层级**：零
- **Material Parameter Collection**：零
- **后处理材质**：零（`MaterialDomain = PostProcess` + `PostProcessVolume` blendable wiring 未覆盖）

对比已有的 Blueprint / Anim / GA 图编辑，Material 是当前最大的"整类零覆盖"领域（`agent-capability-gaps.md` A2-#5 登记过，估工作量 3-5k 行）。

---

## 设计原则

1. **Python-first + C++ fallback**：`MaterialEditingLibrary` 能覆盖的调用走 Python；只在 Python 侧有阻塞（protected 成员、未导出字段、属性不支持 `set_editor_property`、需要批量 + 事务 + 统计）时加 C++。与 GameplayEffect/GameplayCue 的交付模式一致。
2. **USTRUCT 返回 + MI 语义**：读操作返回结构化数据（非字符串），字段名与 UE 内部一致（`shading_model` / `material_domain`）。写操作默认走 MI（轻量、可 runtime 切换），除非任务明确要 Master。
3. **模板即首公民**：AAA 项目里 Master material 数量少（通常 <30 个），覆盖 90% 资源；方案里"模板生成器"比"任意节点编辑"优先级更高。
4. **性能度量内建**：每次 Master material 写操作后，返回 instruction count / sampler count / texture lookup / 编译是否成功；避免 agent 写完才发现 "200 条指令超标"。
5. **幂等 + dry-run**：模板生成带 `--dry-run` 先返回"会建哪些 expression / 连哪些线"，用户确认后再落盘。重跑同名模板覆盖而不是报错。

---

## 里程碑拆分

### M1 — 读 / 观察基础设施（Introspection）

先让 agent 能"看到"材质现状。没有这层，后续优化类任务全是盲摸。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| M1-1 | `get_material_info(path)` — 返回 Material 全量元数据（domain / blend mode / shading model / two sided / subsurface profile / used usage flags / cast shadow / use material attributes / 参数 default 列表） | 小 | 基于 `UMaterial::GetShadingModels()` 等 getter，Python 侧零散 API 的结构化版 |
| M1-2 | `get_material_graph(path)` — 返回 expression 列表（class、GUID、位置、desc、主要属性）+ 连接列表（src expr + src output name → dst expr + dst input name） | 中 | 镜像 BP 的 `describe_node` 风格；必要时用 C++ 遍历 `UMaterial::Expressions` / `FunctionExpressions` |
| M1-3 | `get_material_stats(path, feature_level='SM5', quality='High')` — instruction count（Vertex/Pixel/Compute 分列）、sampler count、texture lookup count、texture dependency length、静态开关配置 | 中 | 走 `FMaterialResource::GetRepresentativeInstructionCounts` / `GetSamplerUsage`；各 feature level 单独编译取值 |
| M1-4 | `get_material_compile_errors(path)` — 最近一次编译的错误列表 + 每条对应的 expression GUID（便于 agent 定位） | 小-中 | `FMaterialResource::GetCompileErrors()` |
| M1-5 | `list_material_instance_chain(mi_path)` — 从 MI 往上走到 Master，每层列出 override 的参数 diff（哪个参数在哪层被改过） | 小 | 纯遍历 `UMaterialInstance::Parent` 递归 |
| M1-6 | `preview_material(path, mesh='sphere'\|'plane'\|'character'\|'cube'\|'cloth', resolution=512, lighting='studio'\|'outdoor'\|'night', camera_yaw, camera_pitch, camera_dist) → PNG 路径` | 中 | 建 `FPreviewScene`，放 `UStaticMeshComponent` + 挂 material，写 `UTextureRenderTarget2D`，保存 PNG。可复用 anim pose capture 的场景搭建代码 |
| M1-7 | `preview_material_complexity(path, ...)` — 同上但走 shader complexity view mode（绿→红渐变） | 小-中 | `EViewModeIndex::VMI_ShaderComplexity`；在 ViewFamily 参数里切 |
| M1-8 | `list_material_functions()` / `get_material_function(path)` — 枚举项目内 Material Function，返回 input/output 列表 | 小 | `UMaterialFunction` 本质就是个子图，复用 M1-2 逻辑 |
| M1-9 | `get_material_parameter_collection(path)` — MPC 内 Scalar/Vector 参数默认值列表 | 小 | `UMaterialParameterCollection` 直接枚举 |

里程碑产出验收：任给一个工程里的 MI，能一次调用拿到「外观 PNG + PBR 配置表 + 性能指标 + 完整 graph JSON + 编译错误（若有）」。

---

### M2 — 表达式工厂 + Graph 写原语

让 agent 能逐节点搭建 Material graph。覆盖面按"能否搭出 M3 的模板"倒推。

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| M2-1 | `create_material(path, domain='Surface', shading_model='DefaultLit', blend_mode='Opaque', two_sided=False, use_material_attributes=False) → asset_path` | 小 | `MaterialFactoryNew` |
| M2-2 | `create_material_instance(parent_path, instance_path) → asset_path` | 小 | `MaterialInstanceConstantFactoryNew` |
| M2-3 | `create_material_function(path, inputs, outputs) → asset_path` | 小-中 | `MaterialFunctionFactoryNew` + `FunctionInput`/`FunctionOutput` 建好 |
| M2-4 | `add_material_expression(material_path, class_name, x, y, properties_json) → expr_guid` — 覆盖 35+ 常用 expression（下表） | 中 | 基于 `MaterialEditingLibrary.create_material_expression`；不支持的类走 C++ `NewObject<UMaterialExpressionXxx>` + `Expressions.Add` |
| M2-5 | `connect_material_expressions(material_path, src_guid, src_output, dst_guid, dst_input)` / `disconnect_*` | 小 | `MaterialEditingLibrary.connect_material_expressions`；按 output/input **名字**而非 index，防止 reorder 后断连 |
| M2-6 | `connect_to_material_output(material_path, src_guid, src_output, mp_slot)` — 连到 BaseColor/Metallic/Roughness/Normal/EmissiveColor/Opacity/WorldPositionOffset/AmbientOcclusion/Refraction/Displacement 等主输出 | 小 | `MaterialEditingLibrary.connect_material_property` |
| M2-7 | `set_material_expression_property(material_path, expr_guid, property_name, value_json)` — 统一属性写（Constant 值 / Texture 引用 / SamplerType / SamplerSource / ParameterName / Group / SortPriority / DefaultValue） | 中 | Python `set_editor_property` 能覆盖大部分；写入 protected 字段（如 `UMaterialExpressionTextureSample::SamplerSource` 在某些版本）走 C++ helper |
| M2-8 | `add_comment(material_path, x, y, w, h, text, color)` / `add_reroute(material_path, x, y)` | 小 | `UMaterialExpressionComment` / `UMaterialExpressionReroute` |
| M2-9 | `auto_layout_material_graph(material_path, algorithm='topo'\|'pin_aligned')` — 按拓扑自动布局 | 中 | 从主输出反向 BFS 分层，每层按主轴居中；复用 BP auto-layout 思路 |
| M2-10 | `apply_material_graph_ops(material_path, ops_json)` — 批量原语单轮 round-trip，支持 `$N` 回引用，尾部统一编译 | 中 | 对齐 BP 的 `apply_graph_ops`；关键收益：模板生成器单次调用就能落盘 |
| M2-11 | `compile_material(path, save=True)` — 强制编译 + 可选保存，返回指令数 + 错误 | 小 | `MaterialEditingLibrary.recompile_material` + M1-3/M1-4 聚合 |
| M2-12 | `snapshot_material_graph_json(path)` / `diff_material_graph_snapshots(a, b)` | 中 | 对齐 BP 的 graph snapshot；让 agent "改完一版 → 对比前后 diff" 成为标准回合 |

**M2-4 expression 最低覆盖集（按 AAA 母材质常用度排序）**：

常量 / 参数：`Constant` / `Constant2Vector` / `Constant3Vector` / `Constant4Vector` / `ScalarParameter` / `VectorParameter` / `StaticBoolParameter` / `StaticSwitchParameter` / `StaticComponentMaskParameter`

纹理 / UV：`TextureSample` / `TextureSampleParameter2D` / `TextureSampleParameterCube` / `TextureObjectParameter` / `TextureCoordinate` / `Panner` / `Rotator` / `CustomRotator` / `RuntimeVirtualTextureSampleParameter`

数学：`Add` / `Subtract` / `Multiply` / `Divide` / `Lerp` / `Power` / `Clamp` / `Saturate` / `Abs` / `Frac` / `Floor` / `Ceil` / `OneMinus` / `Min` / `Max` / `If` / `Smoothstep` / `Sine` / `Cosine`

向量 / 通道：`Append` / `ComponentMask` / `BreakMaterialAttributes` / `MakeMaterialAttributes` / `BlendMaterialAttributes` / `Normalize` / `CrossProduct` / `DotProduct` / `Transform` / `TransformPosition`

屏幕 / 世界：`WorldPosition` / `ActorPositionWS` / `ObjectPositionWS` / `PixelDepth` / `SceneDepth` / `ScreenPosition` / `ViewSize` / `CameraVectorWS` / `CameraPositionWS`

顶点 / 几何：`VertexColor` / `VertexNormalWS` / `VertexTangentWS` / `PixelNormalWS` / `TwoSidedSign`

光照 / PBR 辅助：`Fresnel` / `ReflectionVectorWS` / `PrecomputedAOMask` / `DistanceFieldGradient`

开关 / feature 控制：`FeatureLevelSwitch` / `QualitySwitch` / `ShadingPathSwitch` / `ShaderStageSwitch` / `PreviousFrameSwitch`

函数 / 自定义：`MaterialFunctionCall` / `Custom`（HLSL 片段）/ `NamedRerouteDeclaration` / `NamedRerouteUsage`

层 / 顶点绘制（可选进 M3 阶段补）：`MaterialAttributeLayers` / `LandscapeLayerBlend` / `LandscapeLayerCoords`

---

### M2.5 — HLSL 混合编程基建

把 agent 的"写代码"能力接到 Material 层。UE 原生的 `UMaterialExpressionCustom` 节点允许在图里嵌 HLSL 片段；配合项目级 `.ush` 共享库，可以把高密度数学 / UE 图缺的原语 / 复杂条件调度 用代码而非连线实现。但 Custom 节点会让 UE 编译器失去节点级优化（常量折叠 / CSE / 死代码消除），所以**默认仍走图**，仅在收益明确时切 HLSL。

**决策边界**（agent 必须遵循，M5 lint 会执行）：

| 场景 | 走法 |
|---|---|
| 参数流（ScalarParameter/VectorParameter 需要被 MI 覆盖） | 节点 |
| StaticSwitchParameter permutation | 节点（HLSL `#if` 不接入 UE permutation 系统） |
| 主输出连线 / ShadingModel 分类 | 节点 |
| `TextureSample`（含 SceneTexture、VT） | 节点（走 Custom 会绕过依赖追踪 + sampler 共享 + 异步预取） |
| 高密度数学（噪声 / SDF / ACES / 卷积） | Custom，指令数常 1/2–1/3 |
| 需要 `ddx/ddy` / bit ops / `[unroll]` / `[branch]` 提示 | Custom（图没有等价节点） |
| 法线混合 / TBN 变换这类 8+ 节点可一行 HLSL 替换 | Custom |

**交付项**：

| # | 能力 | 工程量 | 备注 |
|---|---|---|---|
| M2.5-1 | `<Project>/Shaders/Private/BridgeSnippets.ush` 骨架 + 注册到 `FCoreDelegates::OnPostEngineInit` 的 `AddShaderSourceDirectoryMapping` | 小 | 共享 HLSL 片段集中存放；每个 snippet 有 header 注释（函数签名 / 输入输出 / 指令数估算 / 最低 feature level）。新增 snippet 时 agent 先查是否可复用 |
| M2.5-2 | `add_custom_expression(material_path, hlsl_body, inputs, output_type, include_paths, description)` — 专门的 Custom 节点原语，`include_paths` 指向 `.ush`，`hlsl_body` 可以只写 `return BridgeNormalBlend(DetailN, BaseN, Strength);` | 中 | 封装 `UMaterialExpressionCustom` 的 `Inputs`（名字 + 类型）+ `OutputType` + `Code` + `IncludeFilePaths`。输入名是接口契约，不可重排 |
| M2.5-3 | `register_shared_snippet(name, hlsl_source, signature, min_feature_level, instruction_estimate)` — 往 `BridgeSnippets.ush` 追加片段 + 索引更新 | 小 | 幂等；重跑同名覆盖。索引 JSON 存 `<Saved>/BridgeShaderSnippets.json`，用于 `list_shared_snippets` 查询 |
| M2.5-4 | `list_shared_snippets()` / `get_shared_snippet(name)` — 枚举已有片段 + 读取源码 | 小 | agent 在决定"新写还是复用"之前查一次 |
| M2.5-5 | `evaluate_custom_vs_graph(material_path, subgraph_expr_guids, hlsl_alternative_code, hlsl_inputs)` — 两条路径都在 transient material 上编译一次，返回指令数 / sampler 数 / 编译错误对比，给建议 | 中 | 保障"HLSL 不是瞎上"；如果图版更便宜就不建议切 |
| M2.5-6 | `inline_snippet_as_material_function(snippet_name, mf_path)` — 把一个 `.ush` 片段封装成 `UMaterialFunction`（输入 pin / 输出 pin / 内部一个 Custom 节点） | 中 | 让图里复用 snippet 和复用 MF 一样简单；agent 可以生成"MF_BridgeTriplanar" 这类函数，master material 里就是一个普通 MaterialFunctionCall |

**首发 snippet 建议**（M2.5-1 配套落地）：

- `BridgeBlendAngleCorrectedNormals(DetailN, BaseN, Strength)` — 细节法线混合，8 节点 → 1 行
- `BridgeTriplanarSample(Tex, SamplerState, WorldPos, WorldNormal, Scale, Sharpness)` — 三向投影贴图，~50 节点 → 20 条指令
- `BridgePerlin3D(Pos, Octaves, Persistence)` / `BridgeVoronoi(Pos, Cells)` — 常用程序化噪声
- `BridgeSobelEdge(SceneTex, UV, TexelSize, Threshold)` — 后处理 Sobel 边缘（素描 / 描边 PP 复用）
- `BridgeACESTonemap(Color)` — 电影级 tone curve
- `BridgeUnpackORM(Tex)` / `BridgePackORM(Occlusion, Roughness, Metallic)` — ORM 打包 / 解包标准化
- `BridgeDitherLODTransition(Opacity, PixelPos)` — LOD 交叉淡入，植被 / 角色换 LOD 常用
- `BridgeDepthFade(SceneDepth, PixelDepth, FadeDist)` — 粒子软化

**M3 模板按需引用**（具体哪些用 HLSL 下表）：

| 模板 | 图实现的部分 | HLSL snippet 的部分 |
|---|---|---|
| M3-2 角色铠甲 | 参数 / 输出路由 / 静态开关 | `BridgeBlendAngleCorrectedNormals`（DetailNormal）+ Anisotropy 切线旋转 |
| M3-3 环境道具 | 顶点色混合 / 参数 | `BridgeTriplanarSample`（可选，当启用 triplanar 开关） |
| M3-4 植被 | WPO / Subsurface / 参数 | `BridgeDitherLODTransition` |
| M3-5 Hero 武器 | 双 UV / Curve Atlas | `BridgeBlendAngleCorrectedNormals` + 可选 `BridgePOMRayMarch` |
| M3-9 VFX | Depth Fade / Particle 参数 | `BridgeDepthFade` |
| M4-2/3/4 后处理 | Blendable wiring / 参数 | `BridgeSobelEdge` / 半调 / 交叉线 HLSL 片段 |

验收：任一 M3 模板切换到"HLSL 增强版"后，指令数不升 + sampler 数不升 + 编译无错 + 预览图像素差 < ε。

---

### M3 — AAA 母材质模板库

每个模板 = 一个 Python 脚本，调 M2 原语生成完整 graph。模板参数化（`--use_layered=true` 等开关），生成时保留可配置 ScalarParameter / VectorParameter 给 MI 覆盖。

每个模板必须带：测试 MI、标准预览 capture、instruction count 目标、sampler count 目标。超出预算直接失败。

| # | 模板 | 用途 | 关键 feature |
|---|---|---|---|
| M3-1 | `M_Character_PBR` — 角色基础母材质 | 皮肤 / 布料 / 金属 通吃 | ORM 打包（Occlusion / Roughness / Metallic 一张图）+ BaseColor + Normal + 可选 Emissive。SubsurfaceProfile 可选。RoughnessMin/Max 重映射参数。 |
| M3-2 | `M_Character_Armor` — 铠甲 / 硬表面 | 用户点名的用例 | M3-1 + Anisotropy（头发 / 拉丝金属）+ DetailNormal（细节法线混合，基于 `BlendAngleCorrectedNormals`）+ Wear/Scratch 遮罩（静态开关）+ 冰/湿度 overlay（可选 static switch） |
| M3-3 | `M_Environment_Prop` — 场景道具 | 密集重复道具 | ORM + 顶点色遮罩的 2 层混合 + Wetness/Puddle（根据 VertexNormal.Z + 全局 wetness scalar） |
| M3-4 | `M_Foliage_Master` | 植被 | Subsurface 双面 + WindActor 驱动 WPO + SpeedTree 兼容可选 + Dither LOD transition |
| M3-5 | `M_Weapon_Hero` | Hero 武器 / 英雄资产 | M3-2 + 双 UV（第二 UV set 用于 Emissive mask）+ Curve Atlas 驱动的 Pulse 发光 + Parallax Occlusion（static switch，默认关） |
| M3-6 | `M_Glass_Translucent` | 玻璃 / 水晶 | ThinTranslucent shading model + Refraction + ReflectionVectorWS 采样 SkyLight |
| M3-7 | `M_Layered_Base` + `MF_Layer_Metal` / `MF_Layer_Fabric` / `MF_Layer_Dirt` | 材质层框架 | `UMaterialExpressionMaterialAttributeLayers`；3-4 层 blend；每层 MF 暴露参数子集 |
| M3-8 | `M_UI_Unlit` | UMG 控件 | Unlit + 额外的 PixelDepth mask（避免 overdraw 浪费）+ Slate fallback |
| M3-9 | `M_VFX_Unlit_Additive` / `M_VFX_Translucent_Soft` | Niagara 基础 | Unlit + Depth Fade + Particle Color 节点；AlphaClip vs Soft 两套 |

每个模板随附 `tests/test_<name>.py` 生成 → 编译 → 截图 → assert instruction_count < budget + sampler_count < 8 + 编译无错。

**默认性能预算**（基于 AAA 近年项目常见口径，可配置）：

| 模板 | 指令数上限 (PS, SM5, High) | sampler 上限 | texture lookup 上限 |
|---|---|---|---|
| 角色 / 武器 | 250 | 10 | 6 |
| 环境道具 | 200 | 8 | 4 |
| 植被 | 180 | 6 | 4 |
| UI | 60 | 2 | 2 |
| VFX | 100 | 4 | 3 |

---

### M4 — Post-Process 材质管线

用户点名的 "素描风后处理" 属于这类。做法不同于 Surface material，需要专门的路径。

| # | 能力 | 备注 |
|---|---|---|
| M4-1 | `create_post_process_material(path, blendable_location='AfterTonemapping', output_alpha=False)` | `MaterialDomain = PostProcess` + 相应的 input/output pin 对齐 |
| M4-2 | `PP_Sketch` 模板 — 铅笔 / 素描风 | Sobel 边缘（SceneTexture:Normal + SceneDepth 双通道）+ CrossHatch 纹理叠加 + 明暗分层 posterize |
| M4-3 | `PP_Outline` 模板 — 卡通 / 漫画描边 | 深度 + 法线 Sobel；描边宽度参数；外部 only vs 内部 crease 开关 |
| M4-4 | `PP_Halftone` 模板 — 半调网点 | UV 分块 + 阈值 + Luminance 取 pattern mask |
| M4-5 | `PP_ColorGradeLUT_Extended` | SceneColor → LUT + 分区曲线；内建的 ColorGrading 不够时用 |
| M4-6 | `PP_Film_Grain_AA` | 噪声 + dither，轻薄过滤 |
| M4-7 | `apply_post_process_material(volume_path, material_path, weight)` / `remove_post_process_material(volume_path, material_path)` | 找 `APostProcessVolume`（或创建 unbound 的），写 `WeightedBlendables` |
| M4-8 | `get_post_process_state()` — 枚举世界里的 PPV + 启用的 blendable 列表 | 让 agent 知道当前管线栈上有什么 |

验收：给 "做一个铅笔素描风的后处理" 这种任务，一个 `create_post_process_material` + 模板填充 + `apply_post_process_material` 三步出结果。

---

### M5 — 分析 / Lint / 自动优化

把"AAA + 性能最优"从口号变成可执行的质量闸。

| # | 能力 | 检查项 |
|---|---|---|
| M5-1 | `analyze_material(path) → AnalysisReport` 聚合 M1-3/M1-4 + 规则扫 | 全量跑以下 M5-2..M5-10 检查 |
| M5-2 | 指令数 / sampler 预算检查 | 对比传入 budget 或默认模板预算 |
| M5-3 | 未使用 expression 检测 | 从主输出反向可达性 BFS，未 reach 的节点报警 |
| M5-4 | 重复 texture lookup | 同 `UMaterialExpressionTextureSample`（同纹理、同 UV）多次引用 → 建议共享 reroute |
| M5-5 | SamplerSource 一致性 | Character 母材质里 BaseColor/Normal/ORM 应该 share wrap 采样器（`Shared_Wrap`）；不一致就白白占 sampler 槽位 |
| M5-6 | 静态开关误用 | 运行时只会走一支的分支应该是 `StaticSwitchParameter` 而非 `If` / `Lerp`；用 dynamic 节点但没有 ScalarParameter 驱动 = 可静态化 |
| M5-7 | Feature Level / Quality Switch 缺失 | 重运算节点（`Custom` / 多次 TextureSample）没包 QualitySwitch 就是给 Low 也跑 High 的配置 |
| M5-8 | ShadingModel 与输出连线一致性 | DefaultLit 没接 Metallic/Roughness？Subsurface 没接 SubsurfaceColor？Unlit 却接了 Normal？ |
| M5-9 | MI 覆盖深度 | MI 链超过 3 层 → 警告；MI 覆盖了 Master 的 StaticSwitch → 警告 shader permutation 爆炸风险 |
| M5-10 | Texture 设置体检 | BaseColor 纹理压缩是 `BC1/BC7`？Normal 是 `BC5/NormalMap`？ORM 是 `Masks`（非 sRGB）？单通道遮罩是 `Grayscale`？采样不匹配 → 报警 |
| M5-11 | Custom 节点误用（低收益） | Custom 里只做 ≤5 个节点等价运算 → 警告（失去常量折叠 / CSE / DCE，得不偿失） |
| M5-12 | Custom 节点误用（绕过分析） | Custom 里含 `Texture2DSample` / `SceneTextureLookup` → 警告（应改为图里 `TextureSample` 节点送进 Custom 做后续数学） |
| M5-13 | Custom 节点缺 feature level 门 | 使用 SM5-only intrinsic（`ddx_fine` / 部分 bit ops）但外围没 `FeatureLevelSwitch` 兜底 → 警告 |
| M5-14 | `auto_fix_material(path, fixes=['samplersource_share', 'drop_unused', 'static_switch_conversion', 'inline_trivial_custom'])` — 把 M5-3/M5-5/M5-6/M5-11 的部分能自动化的直接修 | 覆盖有把握的安全修复；风险大的只出建议 |

验收：对任意已有 Material，`analyze_material` 返回的报告 + auto_fix 后再跑，instruction count 不升高、质量分提升、编译无错。

---

### M6 — 参数迭代闭环

已有 `get_material_instance_parameters`；扩展到写 + 一步出截图，支持"改参 → 看效果 → 继续改"的调参循环。

| # | 能力 | 备注 |
|---|---|---|
| M6-1 | `set_mi_params(mi_path, params)` — 统一 scalar/vector/texture/static-switch/RVT 参数写 | Python `MaterialEditingLibrary.set_material_instance_scalar_parameter_value` 等能覆盖大部分；static switch 和 layered 参数走 C++ |
| M6-2 | `set_mi_and_preview(mi_path, params, mesh, lighting, resolution) → PNG` | M6-1 + M1-6 的原子化；一次调用出对比图 |
| M6-3 | `sweep_mi_params(mi_path, param, values, mesh, lighting) → [PNG]` | 单参数扫描（如 Roughness 从 0.1 到 0.9 扫 9 帧），输出网格拼图 |
| M6-4 | `set_material_parameter_collection(mpc_path, params)` | MPC 全局参数调节；影响所有引用该 MPC 的 material |
| M6-5 | `diff_mi_params(a_path, b_path)` — 两个 MI 参数差异对比 | 继承链不同也对齐到同一 Master 的参数名空间 |
| M6-6 | Golden-image 回归：`snapshot_material_preview(path, name)` + `compare_material_snapshot(path, name, tolerance)` | 与 A6-#23（计划项）对接；改母材质后能一键验证所有 MI 没回归 |

---

## 跨里程碑的非功能要求

1. **事务性 & undo**：所有写操作走 `FScopedTransaction`（与 Level write 一致），单次 ops 单次 undo。
2. **编译阻塞**：M2-11 默认阻塞到编译完（可 `async=True`）；agent 收到返回值时统计数可信。
3. **Saved/ 产物隔离**：所有 preview PNG 去 `<Saved>/MaterialPreviews/`；Lint 报告去 `<Saved>/MaterialReports/`；不污染 Content/ 目录。
4. **跨 session 可重跑**：模板脚本幂等（目标路径存在则覆盖同名 expression / 重连；不生成二次副本）。
5. **签名优先于实现**：所有函数先定签名 + USTRUCT，进 `bridge-material-api.md`，再写实现。M2-5 的 output/input **名字而非 index** 是硬约定。
6. **安全分级**：创建 / 改 MI 参数 = 轻量；修改已有 Master material / 删 expression = 中；跑 `auto_fix_material` 批量多个 material = 重，建议先 dry-run + 用户确认。

---

## 工程量与排期估算

| 里程碑 | 代码量 | 依赖 | 交付独立性 |
|---|---|---|---|
| M1 读 / 观察 | 800-1200 行 C++ + Python | 零 | 独立可交付，单独也有价值 |
| M2 表达式工厂 | 1500-2500 行 | M1-2（读 graph） | 独立可交付，但 M1 诊断能力强烈推荐同时上 |
| M2.5 HLSL 基建 | 400-700 行 C++ + Python + 首发 ~300 行 HLSL snippets | M2（需要 `add_material_expression` 原语）| 独立可交付；先落地就能立刻在 M3 模板里受益 |
| M3 模板库 | 1500-3000 行 Python（每个模板 200-400） | M2 + M1-6 + 可选 M2.5 | 按需上，一次上一个模板 |
| M4 后处理 | 400-800 行（M2 可复用；差异在 domain + blendable wiring + 4 个模板） | M2 + M2.5（Sobel / 半调依赖 HLSL snippet）| 独立可交付 |
| M5 分析 / Lint | 800-1500 行 | M1 + M2 + M2.5（M5-11/12/13 依赖 Custom 节点识别）| 独立可交付；auto_fix 部分可以先只出警告不动手 |
| M6 参数闭环 | 400-800 行 | M1-6 + 部分 M2 | 独立可交付 |

总量估计：**5500-10000 行**（C++ 约 3500-5700 + Python 模板与脚本 2000-4000 + HLSL snippets 300-500），对齐 `agent-capability-gaps.md` A2-#5 的原估 3-5k 行偏保守（未计模板库 + HLSL 基建）。

**建议交付顺序**：M1 → M2 → M2.5（HLSL 基建 + 首发 snippet）→ M6（先让读 + 基础写 + HLSL 能力 + 参数循环闭环）→ M3 挑一两个高频模板（Character 铠甲 / Environment）→ M5 lint → M4 后处理 → M3 剩余模板。M2.5 提前到 M3 之前交付是因为多个 M3 模板的"AAA 感"依赖 HLSL snippet（DetailNormal / Triplanar / DitherLOD），放后面会迫使模板先写一版图版再改 HLSL 版，浪费。

---

## Tier-B / 推迟项

- **Shader debugger 级别的逐像素着色**：RenderDoc 能做，bridge 范围外。
- **Material Graph 节点级拖拽录像**：没有意义，已有 snapshot/diff。
- **自动从参考图生成材质**（"给一张图让我还原成 PBR"）：需要外部视觉模型 + 贴图合成管线，属于 tier B，不进本路线图。
- **Substance / Designer 联动**：跨进程 DCC 集成，对齐 `agent-capability-gaps.md` Tier-B。
- **Runtime Virtual Texture / World Partition Landscape 写**：M2 的 expression 覆盖里只列 RVT **采样**；RVT volume 管理和 landscape material 编辑属于单独子系统，推迟。
- **Material Editor Slate UI 自动化**：不走 UI 自动化（脆），全部走 data-level API。

---

## 与其它路线图的关系

- **`agent-capability-gaps.md` A2-#5**（Material Graph 写）= 本路线图 M2
- **A2-#6**（Niagara 系统编辑）会复用 M2 的 expression 原语（Niagara module 底层也是 Material-like 图）— 做完 M2 再开 Niagara 路线图成本降一半
- **A6-#23**（Golden-image 回归）直接复用 M1-6 的预览 pipeline + M6-6 的 snapshot/compare
- **A1-#2**（GBuffer 通道截图）已交付，M1-7（shader complexity view）是同一套 `ASceneCapture2D` + ViewFamily 参数扩展
- **M2.5 HLSL 基建**可回流到 Niagara module / Control Rig 等其它走 `UMaterialExpression*` 或类似图的子系统 —— `BridgeSnippets.ush` 是跨子系统共享的代码库，不是 Material 专属

---

## 下次上手清单（handoff）

剩余工作按"每个 bullet = 一次 commit 大小的垂直切片"列出，从最容易上手的往后排，互不依赖。pick 一个开始即可，不必按顺序：

1. **M4-2 PP_Sketch（需先加 snippet）** — 在 `BridgeSnippets.ush` 加 `BridgeSobelEdge(d_l,d_r,d_u,d_d, threshold, gain)` HLSL 片段（~5 行）和 `BridgeCrossHatch(uv, lum, freq)`；再写 `material_templates/pp_sketch.py`（约 200 行 Python），复用 pp_outline 的 4-邻居采样结构 + 叠加 posterize + crosshatch 线条。验收：compile-clean + 0 lint finding。
2. **M3-9 VFX 基础模板** — 两个小模板：`vfx_unlit_additive.py`（Unlit + Additive blend + Depth Fade via `BridgeDepthFade` snippet）和 `vfx_translucent_soft.py`（Unlit + Translucent + 粒子 alpha + soft depth fade）。预算 100 指令 / 4 sampler。
3. **M5-10 texture compression 合规检查** — `analyze_material` 新增规则：BaseColorTex 压缩必须 BC1/BC7 + sRGB=true；Normal 必须 BC5 + NormalMap sampler；ORM 必须 Masks + sRGB=false；单通道遮罩 Grayscale。每条违反出一条 finding。C++ 大约 80 行。
4. **M5-9 MI chain depth** — 扩展 `analyze_material` 接受 MI 路径（目前只吃 UMaterial），沿 `Parent` 递归；超过 3 层出 warning，覆盖 StaticSwitch 出 permutation-爆炸 warning。约 60 行 C++。
5. **M5-6 静态开关误用** — 对 Lerp/If 节点，若所有 Alpha 输入都链到 ScalarParameter 常量 + 参数从未被 MI 重载，出 "可静态化" info。需要扫 MI 使用情况，中等难度（~120 行 C++）。
6. **M3-7 Layered 材质层框架** — `MaterialAttributeLayers` + 3-4 个 `MF_Layer_*` MaterialFunction (Metal / Fabric / Dirt)。体量大（300-500 行 Python），但覆盖 AAA 项目的"层级材质"workflow。
7. **M3-5 Weapon_Hero** — 建立在 M3-2 基础上 + Parallax Occlusion（POM）HLSL snippet + Curve Atlas 驱动的脉冲发光。需要先加 `BridgePOMRayMarch` snippet。
8. **M3-1 Character_PBR 精简版** — 纯 M3-2 的子集，把 Detail/Wear/Wetness/Anisotropy 那几个 StaticSwitch 砍掉。可选，优先级最低（代理可以直接用 M3-2 + 所有开关默认 false）。

### 本轮发现的坑 / 已在代码里注释

写新模板时照抄这些 pitfall 可避免同样的失误：

- **Fresnel 节点**：pin 叫 `ExponentIn`（连线用），fallback scalar **属性**叫 `Exponent`（set_prop 用）。混用会报 "could not set ExponentIn"。
- **If 节点**：branch 的 pin 名叫 `A > B` / `A == B` / `A < B`（空格+符号形式，不是 `AGreaterThanB`）——`GetShortenPinName` 在引擎侧对等转换。实际写 step 函数时改走 `saturate((a-b)*1000)` 更简单。
- **SceneTexture `Coordinates`**：写时必须叫 `UVs`（短名）。现在 bridge 双向兼容，但 get_material_graph 返回的也已经是 `UVs`。
- **SceneTexture 的 `Color` 输出是 float4**：接到 float3 的后续数学会报 "Arithmetic between float4 and float3 is undefined"。加一个 ComponentMask(R=G=B=true, A=false) 变 float3 再接。
- **ESceneTextureId**：ImportText 要 `PPI_` 前缀，如 `PPI_PostProcessInput0` / `PPI_SceneDepth`，不是 bare name。
- **VectorParameter 没有 `RGB` 输出**：outputs 是 `""` / `R` / `G` / `B` / `A`。写 Lerp 的 `B` 输入要用 `""`（float4）让下游隐式截断，或手动 ComponentMask。
- **UMaterial 的 OpacityMaskClipValue 是 material-level 属性**，不是图输入。暴露成 ScalarParameter 会被 M5-3 标成 unused。要改就对 UMaterial 自身调 set_material_expression_property（或直接在 Material Editor Details 改）。
- **apply_material_graph_ops(compile=True) 只重新编译 shader map，不保存 asset**。编辑器重启会丢模板；所有 template build 结尾都必须 `C.save_master(path)` 或 `unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)`。
- **TextureSampleParameter2D 的 default texture 不是真正的 "运行时纹理"**（MI 可覆盖）——M5-4 重复纹理查找规则故意只扫 plain `UMaterialExpressionTextureSample`，不扫 parameter 版本。
- **UE Python `str(unreal.Guid)` 返回 `<Struct 'Guid' (0x...) {}>`**，对 `FGuid::Parse` 无效。用 `.to_string()`（或我封装的 `_common.guid_to_str(g)`）拿 32-hex 形式。
- **UE Python 对 bool USTRUCT 字段去掉 `b` 前缀**：`bool bSuccess` → Python 里是 `.success`，不是 `.b_success`。
- **UE 5.7 `EBlendableLocation` 没有 `BL_SceneColorBeforeTonemapping`**（被删了）；要 pre-tonemap 用 `BL_SceneColorBeforeBloom`。bridge 的 `ParseBlendableLocation` 把字符串 `"BeforeTonemapping"` 作为别名映射到它。

### 回归测试的最小集合

下次上手后验证现有代码还能跑，最小命令组（假设编辑器已启动且加载项目）：

```bash
python .claude/skills/unreal-bridge/scripts/bridge.py ping
python .claude/skills/unreal-bridge/scripts/bridge.py exec "
import importlib, material_templates._common as c, material_templates.character_armor as ca, material_templates.environment_prop as ep, material_templates.foliage_master as fm, material_templates.glass_translucent as gl, material_templates.ui_unlit as ui, material_templates.pp_posterize as pp, material_templates.pp_halftone as ph, material_templates.pp_outline as po
for mod in (c, ca, ep, fm, gl, ui, pp, ph, po): importlib.reload(mod)
import unreal
L = unreal.UnrealBridgeMaterialLibrary
for b in (ca, ep, fm, gl, ui):
    r = b.build(rebuild=True); ar = L.analyze_material(r['master_path'], 0, 0)
    print(f\"{r['master_path']}: exprs={r['num_expressions']} findings={len(list(ar.findings))}\")
for b in (pp, ph, po):
    r = b.build(rebuild=True, apply_weight=0); ar = L.analyze_material(r['master_path'], 0, 0)
    print(f\"{r['master_path']}: ops={r['ops_applied']} findings={len(list(ar.findings))}\")
"
```

期望：全部 `findings=0`，无 exception。如果有 finding 冒出，大概率是引擎版本升级或某个默认 MI 参数漂移，优先查 `analyze_material` 的 detail 字段。

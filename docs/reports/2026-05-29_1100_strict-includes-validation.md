# UnrealBridge UE 5.7 StrictIncludes 验证记录

时间：2026-05-29 11:00:08 +08:00

## 变更范围

- 在 `.uplugin` 中显式声明 `StructUtils` 插件依赖。
- 为 public headers 补齐 `FThreadSafeBool` / `FTSTicker` 的显式 include。
- 为 StrictIncludes 暴露出的 `.cpp` 编译单元补齐直接使用类型所需的 include：
  - JSON serializer / writer / condensed print policy
  - `UDataAsset`
  - `ULocalPlayer`
  - `FTextureRenderTargetResource`
  - `UEdGraph`
  - `FStaticMeshRenderData`
  - `FAssetData`
  - `TObjectIterator`
  - `FConfigCacheIni`
  - `UMaterialInstance`

## 验证

普通 BuildPlugin：

```powershell
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeHygieneBuild57Loose_1048 -TargetPlatforms=Win64
```

结果：通过。

StrictIncludes：

```powershell
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeHygieneBuild57Strict_1058 -TargetPlatforms=Win64 -StrictIncludes
```

结果：通过。

Python smoke：

```powershell
python tools\smoke_mcp_stdio.py
python tools\smoke_output_spill.py
python tools\smoke_mcp_pagination.py
```

结果：均通过。

## 备注

UE 5.7 构建日志仍提示 `StructUtils` 插件在 UE 5.5 后 deprecated。当前改动只修复
“使用了 StructUtils 模块但 `.uplugin` 未声明插件依赖”的 packaging hygiene 问题；
未来替代 `StructUtils` 的迁移应放到独立兼容性工作中处理。

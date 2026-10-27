#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgePCGLibrary.generated.h"

USTRUCT(BlueprintType)
struct FBridgePCGComponentEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ActorLabel;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ComponentName;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString GraphPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bGenerated = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bGenerating = false;
};

USTRUCT(BlueprintType)
struct FBridgePCGComponentState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString GraphPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bGenerated = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bDirty = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bGenerating = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FBox GeneratedBounds = FBox(ForceInit);
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString LastGenerationIso;
};

USTRUCT(BlueprintType)
struct FBridgePCGOverrideEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString TypeStr;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ValueStr;
};

USTRUCT(BlueprintType)
struct FBridgePCGWaitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") float ElapsedMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString Note;
};

USTRUCT(BlueprintType)
struct FBridgePCGAuthoringResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString Error;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ObjectPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString SettingsPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ActorLabel;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ComponentName;
};

USTRUCT(BlueprintType)
struct FBridgePCGNodeInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString NodePath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString SettingsPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString SettingsClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") TArray<FString> InputPins;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") TArray<FString> OutputPins;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") int32 PositionX = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") int32 PositionY = 0;
};

USTRUCT(BlueprintType)
struct FBridgePCGMeshOutput
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString ComponentName;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString MeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") TArray<FString> MaterialPaths;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") int32 InstanceCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FString CollisionProfileName;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FVector BoundsMin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|PCG") FVector BoundsMax = FVector::ZeroVector;
};

/**
 * PCG inspection, bounded native graph authoring and editor generation.
 * 图的创建、节点及连线使用 PCG 原生接口；不直接重写节点或连线私有数组。
 * Graph creation, nodes and edges use native PCG APIs, never private topology arrays.
 *
 * Hard contract:
 *  - Whole library is gated to UE 5.7+. On 5.4-5.6 the UFUNCTIONs exist
 *    (UHT requires unconditional decls) but bodies live in the
 *    UnrealBridgePCGLibrary_Stubs.cpp and log a warning + return T{}.
 *  - Editor-world only — runtime/PIE PCG generation is PCG's own concern;
 *    bridge does not duplicate it.
 *  - WaitForPCGGenerate polls on the GameThread (FPlatformProcess::Sleep
 *    in 50ms steps) — caller must know this blocks. Same pattern as
 *    hot_reload.py.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgePCGLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 创建新的 PCG 图；AssetPath 为完整包路径，已有资产拒绝覆盖。bSave=false 保留可撤销的内存资产。
	 * Create a new PCG graph at a full package path; existing assets are rejected. False leaves it unsaved and undoable. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult CreatePCGGraph(const FString& AssetPath, bool bSave = false);

	/** 按原生 Settings 类增加节点；位置以图编辑器单位表示。返回节点与 Settings 路径供后续配置。
	 * Add a native settings-class node. X/Y use graph-editor units; returns node and settings paths for configuration. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult AddPCGGraphNode(const FString& GraphPath, const FString& SettingsClassPath, const FString& Title, int32 PositionX, int32 PositionY);

	/** 连接同一图的已有输出与输入引脚；引脚名称必须精确匹配。只增加连线，不清理或替换其他连线。
	 * Connect existing output/input pins in the same graph using exact pin labels; does not remove other connections. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult ConnectPCGGraphNodes(const FString& GraphPath, const FString& SourceNodePath, const FString& SourcePin, const FString& TargetNodePath, const FString& TargetPin);

	/** 读取节点、Settings 路径、引脚和布局，含图输入与输出节点。只读，不执行图。
	 * Read graph nodes, settings paths, pins and layout, including input/output nodes. Does not execute the graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static TArray<FBridgePCGNodeInfo> GetPCGGraphNodes(const FString& GraphPath);

	/** 配置静态网格生成节点。权重为空时各为 1，否则数量须与 MeshPaths 一致且为正整数；材质为空使用网格默认材质。
	 * Configure a mesh spawner. Empty weights mean 1 each; otherwise match mesh count with positive integer weights. Empty materials retain mesh defaults.
	 * CollisionProfileName 是现有碰撞配置名；bCastShadow 控制全部条目。全量验证成功后才替换该节点的条目。
	 * CollisionProfileName names an existing collision profile; bCastShadow applies to all entries. Validate all inputs before replacing entries. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult ConfigurePCGStaticMeshSpawner(const FString& GraphPath, const FString& NodePath, const TArray<FString>& MeshPaths, const TArray<int32>& Weights, const TArray<FString>& MaterialOverrides, const FString& CollisionProfileName, bool bCastShadow = true);

	/** 为已有编辑器 Actor 的 PCG 组件绑定图，并使用原生 SetGraph 刷新参数与回调；空组件名选择第一个。
	 * Bind a graph using native SetGraph, refreshing parameters/callbacks. Empty component name selects the first PCG component. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult SetPCGComponentGraph(const FString& ActorLabel, const FString& ComponentName, const FString& GraphPath);

	/** 新建独立样条与 PCG Actor。Points 为世界坐标 cm，2 至 10000 点；标签不可与已有 Actor 重复，种子 0 也是固定有效值。
	 * Create a standalone spline/PCG actor. Points are world coordinates in cm, 2..10000 finite points; label must be unique. Seed 0 is deterministic.
	 * 不自动生成，调用 TriggerPCGGenerate 后才执行。Does not generate until TriggerPCGGenerate is called. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult CreatePCGSplineActor(const FString& GraphPath, const FString& ActorLabel, const TArray<FVector>& Points, int32 Seed = 0);

	/** 修改已有样条并正确更新曲线缓存。Points 使用世界坐标 cm，2 至 10000 点；空组件名选择第一个样条。
	 * Replace spline points and refresh curve caches. World cm, 2..10000 finite points; empty component selects the first spline.
	 * bClosedLoop 决定是否闭合；只标记 PCG 输入失效，不自动清理或生成。Closed loop is explicit; dirties PCG input without cleanup/generation. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGAuthoringResult SetPCGSplinePoints(const FString& ActorLabel, const FString& ComponentName, const TArray<FVector>& Points, bool bClosedLoop = false);

	/** 读取指定 PCG 输入 Actor 自身的网格组件和实际实例数，不包含附属 Actor，也不把 Generated 标志当作有效输出。
	 * Read mesh components and actual instance counts owned by this input actor; excludes attached actors and does not infer output from Generated alone. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static TArray<FBridgePCGMeshOutput> GetPCGActorMeshOutputs(const FString& ActorLabel);

	/** L3-1 — list UPCGGraph assets in the project, optionally filtered by short-name substring. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static TArray<FString> ListPCGGraphAssets(const FString& Filter, int32 Max = 200);

	/** L3-2 — list UPCGComponent on actors in the editor world. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static TArray<FBridgePCGComponentEntry> ListPCGComponentsInLevel(const FString& LevelFilter, int32 Max = 200);

	/** L3-3 — full state for one component on a level actor. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGComponentState GetPCGComponentState(const FString& ActorLabel, const FString& ComponentName);

	/** L3-4 — list user-parameter values exposed by the component's graph instance. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static TArray<FBridgePCGOverrideEntry> GetPCGComponentOverrides(const FString& ActorLabel, const FString& ComponentName);

	/**
	 * L3-5 — set a single user-parameter override via Property->ImportText.
	 * `ExportedValue` must be in UE's standard exported form for the property's
	 * type (e.g. `42` for int, `(X=1, Y=2, Z=3)` for FVector, `"hello"` for FString).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static bool SetPCGComponentOverride(const FString& ActorLabel, const FString& ComponentName, const FString& Name, const FString& ExportedValue);

	/**
	 * L3-6 — request asynchronous generation. Returns immediately after the
	 * request is queued; poll GetPCGComponentState across separate bridge execs.
	 *
	 * @param bForce true → run even if not dirty.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static bool TriggerPCGGenerate(const FString& ActorLabel, const FString& ComponentName, bool bForce = false);

	/**
	 * L3-7 — block (with 50ms polling) until the component finishes generating
	 * or `TimeoutSec` elapses. Returns elapsed_ms + success flag.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static FBridgePCGWaitResult WaitForPCGGenerate(const FString& ActorLabel, const FString& ComponentName, float TimeoutSec = 60.0f);

	/** L3-8 — clean up generated content (purge actors / components tagged by this PCG generation). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PCG")
	static bool CleanupPCGComponent(const FString& ActorLabel, const FString& ComponentName, bool bRemoveComponents = false);
};

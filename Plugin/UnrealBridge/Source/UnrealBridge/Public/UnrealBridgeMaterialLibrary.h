#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeMaterialLibrary.generated.h"

/** A material parameter with its value. */
USTRUCT(BlueprintType)
struct FBridgeMaterialParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "Scalar", "Vector", "Texture", "DoubleVector", etc. */
	UPROPERTY(BlueprintReadOnly)
	FString ParamType;

	/** String representation of the value */
	UPROPERTY(BlueprintReadOnly)
	FString Value;
};

/** Overview of a Material Instance. */
USTRUCT(BlueprintType)
struct FBridgeMaterialInstanceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Parent material name */
	UPROPERTY(BlueprintReadOnly)
	FString ParentName;

	/** Parent material path */
	UPROPERTY(BlueprintReadOnly)
	FString ParentPath;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialParam> Parameters;
};

/** Default-value parameter entry (as declared on a Master Material). */
USTRUCT(BlueprintType)
struct FBridgeMaterialParamDefault
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "Scalar", "Vector", "Texture", "StaticSwitch" */
	UPROPERTY(BlueprintReadOnly)
	FString ParamType;

	/** String representation of the default value (scalar → float, vector → "(R,G,B,A)", texture → path, switch → "True"/"False") */
	UPROPERTY(BlueprintReadOnly)
	FString Value;

	UPROPERTY(BlueprintReadOnly)
	FGuid Guid;
};

/** Full metadata for a Material or Material Instance (M1-1). */
USTRUCT(BlueprintType)
struct FBridgeMaterialInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	/** True if the asset is a UMaterialInstance (vs a UMaterial master). */
	UPROPERTY(BlueprintReadOnly)
	bool bIsMaterialInstance = false;

	/** Immediate parent path (only set for MI). */
	UPROPERTY(BlueprintReadOnly)
	FString ParentPath;

	/** Ultimate base UMaterial path (same as Path for masters). */
	UPROPERTY(BlueprintReadOnly)
	FString BasePath;

	/** "Surface", "DeferredDecal", "LightFunction", "Volume", "PostProcess", "UI", "RuntimeVirtualTexture" */
	UPROPERTY(BlueprintReadOnly)
	FString MaterialDomain;

	/** "Opaque", "Masked", "Translucent", "Additive", "Modulate", "AlphaComposite", "AlphaHoldout" */
	UPROPERTY(BlueprintReadOnly)
	FString BlendMode;

	/** One entry per shading model present on this material (DefaultLit / Unlit / Subsurface / ...). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ShadingModels;

	UPROPERTY(BlueprintReadOnly)
	bool bTwoSided = false;

	UPROPERTY(BlueprintReadOnly)
	bool bUseMaterialAttributes = false;

	/** Human-readable flag names ("SkeletalMesh", "ParticleSprites", "Nanite", ...) that are enabled on the base material. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> UsageFlags;

	UPROPERTY(BlueprintReadOnly)
	FString SubsurfaceProfilePath;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialParamDefault> ScalarParameters;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialParamDefault> VectorParameters;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialParamDefault> TextureParameters;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialParamDefault> StaticSwitchParameters;

	/** Total number of UMaterialExpression nodes on the base material. */
	UPROPERTY(BlueprintReadOnly)
	int32 NumExpressions = 0;

	/** Number of UMaterialExpressionMaterialFunctionCall nodes. */
	UPROPERTY(BlueprintReadOnly)
	int32 NumFunctionCalls = 0;

	/** Whether the lookup succeeded. Callers should check this; empty struct otherwise. */
	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;
};

/** One input or output pin on a MaterialFunction. */
USTRUCT(BlueprintType)
struct FBridgeMaterialFunctionPort
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	/** "Scalar" / "Vector2" / "Vector3" / "Vector4" / "Texture2D" / "TextureCube" / "Texture2DArray" / "VolumeTexture" / "StaticBool" / "MaterialAttributes" / "TextureExternal" / "" (output — type determined by incoming connection) */
	UPROPERTY(BlueprintReadOnly)
	FString PortType;

	UPROPERTY(BlueprintReadOnly)
	int32 SortPriority = 0;

	/** For inputs only — stringified preview value when bUsePreviewValueAsDefault is true. */
	UPROPERTY(BlueprintReadOnly)
	FString DefaultValue;

	/** For inputs only — whether the preview value is used when an MF caller leaves the pin unconnected. */
	UPROPERTY(BlueprintReadOnly)
	bool bUsePreviewValueAsDefault = false;
};

/** Compact entry for list_material_functions. */
USTRUCT(BlueprintType)
struct FBridgeMaterialFunctionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadOnly)
	FString LibraryCategory;
};

/** Full metadata for a UMaterialFunction. */
USTRUCT(BlueprintType)
struct FBridgeMaterialFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadOnly)
	FString LibraryCategory;

	/** Sorted by SortPriority ascending. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialFunctionPort> Inputs;

	/** Sorted by SortPriority ascending. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialFunctionPort> Outputs;

	UPROPERTY(BlueprintReadOnly)
	int32 NumExpressions = 0;
};

/** One layer of an MI → parent → ... → UMaterial chain. */
USTRUCT(BlueprintType)
struct FBridgeMaterialInstanceLayer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	/** True for the final UMaterial base; false for all intermediate MIs. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsBaseMaterial = false;

	/** Parameters set *on this layer* (MI overrides). Empty for the base UMaterial — defaults live on the base. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialParam> OverrideParameters;
};

/** Result of walking an MI up to its UMaterial base. */
USTRUCT(BlueprintType)
struct FBridgeMaterialInstanceChain
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	/** Ordered leaf → base. Element 0 is the MI passed in; last element is the UMaterial. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialInstanceLayer> Layers;
};

/** Scalar entry on a UMaterialParameterCollection. */
USTRUCT(BlueprintType)
struct FBridgeMPCScalarParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	float DefaultValue = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FGuid Id;
};

/** Vector entry on a UMaterialParameterCollection. */
USTRUCT(BlueprintType)
struct FBridgeMPCVectorParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FLinearColor DefaultValue = FLinearColor::Black;

	UPROPERTY(BlueprintReadOnly)
	FGuid Id;
};

/** UMaterialParameterCollection metadata. */
USTRUCT(BlueprintType)
struct FBridgeMaterialParameterCollectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMPCScalarParam> ScalarParameters;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMPCVectorParam> VectorParameters;
};

/** One expression node in a material or material function graph. */
USTRUCT(BlueprintType)
struct FBridgeMaterialGraphNode
{
	GENERATED_BODY()

	/** MaterialExpressionGuid — stable across reloads, what connections reference. */
	UPROPERTY(BlueprintReadOnly)
	FGuid Guid;

	/** UMaterialExpression subclass name ("MaterialExpressionConstant3Vector"). */
	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(BlueprintReadOnly)
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Y = 0;

	/** First line of UMaterialExpression::GetCaption() — typically the displayed node title. */
	UPROPERTY(BlueprintReadOnly)
	FString Caption;

	/** UMaterialExpression::Desc — user comment on the node. */
	UPROPERTY(BlueprintReadOnly)
	FString Desc;

	/** Named output pins on this node (empty string if anonymous default output). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> OutputNames;

	/** Named input pins on this node. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> InputNames;

	/** Class-specific key properties as "k=v;k=v" — ParameterName, DefaultValue, Texture, SamplerType, FunctionPath, Code (truncated) for Custom, etc. */
	UPROPERTY(BlueprintReadOnly)
	FString KeyProperties;
};

/** One wire connecting two expressions (or an expression to a material property output). */
USTRUCT(BlueprintType)
struct FBridgeMaterialGraphConnection
{
	GENERATED_BODY()

	/** Source expression's MaterialExpressionGuid. */
	UPROPERTY(BlueprintReadOnly)
	FGuid SrcGuid;

	/** Name of the output pin on the source expression ("" = default / index 0 anonymous output). */
	UPROPERTY(BlueprintReadOnly)
	FString SrcOutputName;

	UPROPERTY(BlueprintReadOnly)
	int32 SrcOutputIndex = 0;

	/** Destination expression's guid. Invalid (all-zero) for material-property output connections (see DstPropertyName). */
	UPROPERTY(BlueprintReadOnly)
	FGuid DstGuid;

	UPROPERTY(BlueprintReadOnly)
	FString DstInputName;

	UPROPERTY(BlueprintReadOnly)
	int32 DstInputIndex = 0;

	/** For wires into main material outputs ("BaseColor" / "Metallic" / ...), the property name. Empty for expression→expression. */
	UPROPERTY(BlueprintReadOnly)
	FString DstPropertyName;
};

/** Result of get_material_graph. */
USTRUCT(BlueprintType)
struct FBridgeMaterialGraph
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	/** True if the asset is a UMaterialFunction (in which case OutputConnections will be empty). */
	UPROPERTY(BlueprintReadOnly)
	bool bIsMaterialFunction = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialGraphNode> Nodes;

	/** Expression → expression edges. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialGraphConnection> Connections;

	/** Expression → main material output (BaseColor / Metallic / Normal / ...) edges. Empty for material functions. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialGraphConnection> OutputConnections;
};

/** Instruction-count entry for one representative shader variant. */
USTRUCT(BlueprintType)
struct FBridgeMaterialShaderStat
{
	GENERATED_BODY()

	/** Human-readable shader variant name (e.g. "StationarySurface", "StaticMesh", "NaniteMesh"). */
	UPROPERTY(BlueprintReadOnly)
	FString ShaderType;

	/** Longer description of the variant (e.g. "Base pass shader with only stationary lighting"). */
	UPROPERTY(BlueprintReadOnly)
	FString ShaderDescription;

	UPROPERTY(BlueprintReadOnly)
	int32 InstructionCount = 0;

	/** Extra per-shader statistics as a free-form string (GPU-specific counters if available). */
	UPROPERTY(BlueprintReadOnly)
	FString ExtraStatistics;
};

/** Aggregate statistics + compile state for a Material at a given feature level / quality. */
USTRUCT(BlueprintType)
struct FBridgeMaterialStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	/** "SM5" / "SM6" / "ES3_1" etc. */
	UPROPERTY(BlueprintReadOnly)
	FString FeatureLevel;

	/** "Low" / "Medium" / "High" / "Epic" */
	UPROPERTY(BlueprintReadOnly)
	FString QualityLevel;

	/** One entry per representative shader variant. Empty if the material's shader map is still compiling. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialShaderStat> Shaders;

	/** UMaterialInterface::GetNumVirtualTextureStacks — count of VT sample stacks on this material. */
	UPROPERTY(BlueprintReadOnly)
	int32 VirtualTextureStackCount = 0;

	/** Compile errors reported by the shader map. Empty = clean compile. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> CompileErrors;

	/** False if the material resource is still compiling — call again once `is_compiling()` is false. */
	UPROPERTY(BlueprintReadOnly)
	bool bShaderMapReady = false;
};

/** One diagnostic emitted by analyze_material (M5). */
USTRUCT(BlueprintType)
struct FBridgeMaterialFinding
{
	GENERATED_BODY()

	/** Stable rule id ("M5-2", "M5-3", ...). Stable for programmatic filtering. */
	UPROPERTY(BlueprintReadOnly)
	FString RuleId;

	/** "info" / "warning" / "error". */
	UPROPERTY(BlueprintReadOnly)
	FString Severity;

	/** Short human-readable description. */
	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** Guid of the offending UMaterialExpression (invalid for material-level findings). */
	UPROPERTY(BlueprintReadOnly)
	FGuid ExpressionGuid;

	/** Class of the offending expression (empty for material-level findings). */
	UPROPERTY(BlueprintReadOnly)
	FString ExpressionClass;

	/** Free-form extra context (e.g. "Texture=.. UV=.. OutPin=.."). */
	UPROPERTY(BlueprintReadOnly)
	FString Detail;
};

/** Result of analyze_material (M5-1). */
USTRUCT(BlueprintType)
struct FBridgeMaterialAnalysis
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	/** Resolved material domain ("Surface" / "PostProcess" / ...). */
	UPROPERTY(BlueprintReadOnly)
	FString MaterialDomain;

	/** Shading models present on the material. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ShadingModels;

	/** Max instructions across all shader variants (0 if shader map not ready). */
	UPROPERTY(BlueprintReadOnly)
	int32 MaxInstructions = 0;

	/** Distinct texture-parameter count (upper bound on sampler slots). */
	UPROPERTY(BlueprintReadOnly)
	int32 SamplerCount = 0;

	/** Total expression count on the graph (excluding comments / reroutes). */
	UPROPERTY(BlueprintReadOnly)
	int32 ExpressionCount = 0;

	/** Budgets echoed back from the analyze call (0 = not enforced). */
	UPROPERTY(BlueprintReadOnly)
	int32 InstructionBudget = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SamplerBudget = 0;

	/** Compile errors from the shader map, if any. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> CompileErrors;

	/** Ordered error → warning → info, then by RuleId ascending. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMaterialFinding> Findings;

	/** False when shader stats couldn't be collected (material still compiling). */
	UPROPERTY(BlueprintReadOnly)
	bool bShaderStatsReady = false;
};

/** One weighted-blendable entry on a PostProcessVolume (M4-8). */
USTRUCT(BlueprintType)
struct FBridgePostProcessBlendable
{
	GENERATED_BODY()

	/** Material (or MI) asset path — empty when the blendable points at something non-UMaterialInterface. */
	UPROPERTY(BlueprintReadOnly)
	FString MaterialPath;

	UPROPERTY(BlueprintReadOnly)
	float Weight = 1.0f;
};

/** One PostProcessVolume's snapshot (M4-8). */
USTRUCT(BlueprintType)
struct FBridgePostProcessVolumeInfo
{
	GENERATED_BODY()

	/** Editor-visible label (the one you see in the Outliner). */
	UPROPERTY(BlueprintReadOnly)
	FString ActorLabel;

	/** Internal UObject name (stable across relabels). */
	UPROPERTY(BlueprintReadOnly)
	FString ActorName;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = false;

	/** True = global / ignores volume bounds. */
	UPROPERTY(BlueprintReadOnly)
	bool bUnbound = false;

	UPROPERTY(BlueprintReadOnly)
	float BlendWeight = 1.0f;

	UPROPERTY(BlueprintReadOnly)
	float Priority = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgePostProcessBlendable> Blendables;
};

/** Summary of an auto_fix_material call (M5-14). */
USTRUCT(BlueprintType)
struct FBridgeMaterialAutoFixResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	/** Count of findings resolved by the fix pass. */
	UPROPERTY(BlueprintReadOnly)
	int32 FindingsFixed = 0;

	/** Number of expressions deleted (drop_unused, static_switch_conversion, inline_trivial_custom). */
	UPROPERTY(BlueprintReadOnly)
	int32 NodesRemoved = 0;

	/** Number of expression properties changed (samplersource_share). */
	UPROPERTY(BlueprintReadOnly)
	int32 PropertiesChanged = 0;

	/** Number of expressions added by graph rewrites (static_switch_conversion, inline_trivial_custom). */
	UPROPERTY(BlueprintReadOnly)
	int32 NodesAdded = 0;

	/** Fix IDs that were requested but not recognised by the current implementation. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> SkippedFixes;

	/** Human-readable, one-action-per-line report. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Log;
};

/** Result of a material or MI asset creation (M2-1 / M2-2). */
USTRUCT(BlueprintType)
struct FBridgeCreateAssetResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	UPROPERTY(BlueprintReadOnly)
	FString Error;
};

/** One operation in a batched apply_material_graph_ops call. */
USTRUCT(BlueprintType)
struct FBridgeMaterialGraphOp
{
	GENERATED_BODY()

	/** "add" / "connect" / "connect_out" / "disconnect_in" / "disconnect_out" / "set_prop" / "comment" / "reroute" / "delete" */
	UPROPERTY(BlueprintReadWrite)
	FString Op;

	/** For "add": UE expression short name ("Constant3Vector") or full path. */
	UPROPERTY(BlueprintReadWrite)
	FString ClassName;

	UPROPERTY(BlueprintReadWrite)
	int32 X = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Y = 0;

	/**
	 * Source reference for "connect" / "connect_out".
	 * Either a concrete guid ("12345678-..."), or a back-ref "$N" to the Nth
	 * op in the same batch that produced a guid (add / comment / reroute).
	 */
	UPROPERTY(BlueprintReadWrite)
	FString SrcRef;

	UPROPERTY(BlueprintReadWrite)
	FString SrcOutput;

	/** Destination reference for "connect" / "disconnect_in" / "set_prop" / "delete". Same format as SrcRef. */
	UPROPERTY(BlueprintReadWrite)
	FString DstRef;

	UPROPERTY(BlueprintReadWrite)
	FString DstInput;

	/** Property name for "connect_out" / "disconnect_out" (BaseColor / Metallic / ...) or for "set_prop". */
	UPROPERTY(BlueprintReadWrite)
	FString Property;

	/** Value for "set_prop" (ImportText-format). */
	UPROPERTY(BlueprintReadWrite)
	FString Value;

	/** Comment-specific fields. */
	UPROPERTY(BlueprintReadWrite)
	int32 Width = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Height = 0;

	UPROPERTY(BlueprintReadWrite)
	FString Text;

	UPROPERTY(BlueprintReadWrite)
	FLinearColor Color = FLinearColor(0.2f, 0.7f, 1.0f, 0.4f);
};

/** Outcome of apply_material_graph_ops. */
USTRUCT(BlueprintType)
struct FBridgeMaterialGraphOpResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	int32 OpsApplied = 0;

	/**
	 * Guid produced by each op (if any). Same length as the input op list:
	 *   add / comment / reroute → the new node's guid
	 *   other ops → invalid guid
	 */
	UPROPERTY(BlueprintReadOnly)
	TArray<FGuid> Guids;

	/** First failure's message (empty on full success). */
	UPROPERTY(BlueprintReadOnly)
	FString Error;

	UPROPERTY(BlueprintReadOnly)
	int32 FailedAtIndex = -1;
};

/** One MI parameter override (for set_mi_params / MPC setter). */
USTRUCT(BlueprintType)
struct FBridgeMIParamSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString Name;

	/** "Scalar" / "Vector" / "Texture" / "StaticSwitch" / "Collection" (MPC scalar auto-detect). */
	UPROPERTY(BlueprintReadWrite)
	FString Type;

	/**
	 * ImportText-format value:
	 *   Scalar        "0.5"
	 *   Vector        "(R=1,G=0,B=0,A=1)"
	 *   Texture       "/Game/Textures/T_Base.T_Base"
	 *   StaticSwitch  "true" / "false"
	 */
	UPROPERTY(BlueprintReadWrite)
	FString Value;
};

/** Result of set_mi_params. */
USTRUCT(BlueprintType)
struct FBridgeMIParamResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	int32 Applied = 0;

	/** Per-failed-param diagnostic messages. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Skipped;
};

/** Metadata for an HLSL snippet shipped in BridgeSnippets.ush (M2.5). */
USTRUCT(BlueprintType)
struct FBridgeShaderSnippet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	/** Full HLSL function signature line. */
	UPROPERTY(BlueprintReadOnly)
	FString Signature;

	/** "SM5" / "SM6" / "ES3_1". */
	UPROPERTY(BlueprintReadOnly)
	FString MinFeatureLevel;

	/** Rough instruction-count estimate (as authored in the header comment). */
	UPROPERTY(BlueprintReadOnly)
	FString InstructionEstimate;

	/** Full function body — only populated by get_shared_snippet; list_shared_snippets leaves it empty. */
	UPROPERTY(BlueprintReadOnly)
	FString Source;
};

/** One property to set on an expression (for batched SetMaterialExpressionProperties). */
USTRUCT(BlueprintType)
struct FBridgeExpressionPropSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString Name;

	/** ImportText-format string (see SetMaterialExpressionProperty docs). */
	UPROPERTY(BlueprintReadWrite)
	FString Value;
};

/** Result of adding a single material expression (M2-4). */
USTRUCT(BlueprintType)
struct FBridgeAddExpressionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	/** MaterialExpressionGuid of the newly created node — pass this to connect / set-property ops. */
	UPROPERTY(BlueprintReadOnly)
	FGuid Guid;

	/** Resolved UE class name (e.g. "MaterialExpressionConstant3Vector"). */
	UPROPERTY(BlueprintReadOnly)
	FString ResolvedClass;

	UPROPERTY(BlueprintReadOnly)
	FString Error;
};

/**
 * Material introspection via UnrealBridge.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeMaterialLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Get all parameter overrides on a Material Instance.
	 * Returns scalar, vector, texture, and double vector parameters.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialInstanceInfo GetMaterialInstanceParameters(const FString& MaterialPath);

	/**
	 * M1-1: Full metadata for a Material or Material Instance — domain, blend mode, shading models,
	 * usage flags, parameter defaults, expression counts. Resolves MI to the underlying base material
	 * for master-only fields (domain / use-material-attributes / usage flags / expressions).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialInfo GetMaterialInfo(const FString& MaterialPath);

	/**
	 * M1-8: Enumerate UMaterialFunction assets via AssetRegistry.
	 * @param PathPrefix Package path prefix to scope the search (e.g. "/Game/Materials"). Empty = all.
	 * @param MaxResults Cap the returned count (<=0 means no cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static TArray<FBridgeMaterialFunctionSummary> ListMaterialFunctions(
		const FString& PathPrefix,
		int32 MaxResults);

	/**
	 * M1-8: Full metadata for a single UMaterialFunction — inputs, outputs, expression count.
	 * Inputs walk UMaterialExpressionFunctionInput nodes; outputs walk UMaterialExpressionFunctionOutput.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialFunctionInfo GetMaterialFunction(const FString& FunctionPath);

	/**
	 * M1-5: Walk MI → parent → ... → UMaterial. Each layer lists the parameter overrides it
	 * contributes (empty for the base UMaterial). The input path may be either a MI or a UMaterial —
	 * a UMaterial resolves to a single-element chain.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialInstanceChain ListMaterialInstanceChain(const FString& MaterialPath);

	/**
	 * M1-9: UMaterialParameterCollection scalar + vector parameters with their defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialParameterCollectionInfo GetMaterialParameterCollection(const FString& CollectionPath);

	/**
	 * M1-2: Full expression graph for a UMaterial or UMaterialFunction —
	 * nodes (class / position / caption / key properties / pin names) + connection edges +
	 * main-output wiring (UMaterial only). Connections identify source/destination by
	 * MaterialExpressionGuid and pin *name* (not index — names are stable across reorders).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialGraph GetMaterialGraph(const FString& MaterialPath);

	/**
	 * M1-3: Shader-level statistics for a compiled Material — per-variant instruction counts,
	 * VT stack count, feature level + quality — plus the current set of compile errors (M1-4
	 * overlap).
	 *
	 * @param FeatureLevel  "SM5" / "SM6" / "ES3_1" / "Default" (uses GMaxRHIFeatureLevel).
	 * @param Quality       "Low" / "Medium" / "High" / "Epic" / "Default" (current CVar).
	 *
	 * Returns bShaderMapReady=false if the material is still compiling — call again after
	 * IsMaterialShaderCompiling is false, or rely on get_material_compile_errors which doesn't
	 * block on shader-map retrieval.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialStats GetMaterialStats(
		const FString& MaterialPath,
		const FString& FeatureLevel,
		const FString& Quality);

	/**
	 * M1-4: The compile-errors list from a Material's most recent shader-map compile.
	 * Cheaper than GetMaterialStats when you only need to know whether a material is broken.
	 * Returns empty list on clean compile or when no shader map has been compiled yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static TArray<FString> GetMaterialCompileErrors(
		const FString& MaterialPath,
		const FString& FeatureLevel,
		const FString& Quality);

	/**
	 * M1-6: Render a preview of a Material on a standard mesh in an isolated FPreviewScene
	 * and save as PNG. The scene has its own lights (3-point or HDRI-style) — no dependency
	 * on the current level.
	 *
	 * @param MaterialPath  Material or MI to preview.
	 * @param Mesh          "sphere" / "plane" / "cube" / "cylinder" — engine basic shapes.
	 * @param Lighting      "studio" (3-point directional) / "hdri" (skylight + single dir).
	 * @param Resolution    Pixel square edge (e.g. 512). Clamped to [32, 4096].
	 * @param CameraYawDeg  Orbit azimuth around the mesh. 0 = front.
	 * @param CameraPitchDeg Orbit elevation. 0 = horizontal, +90 looks down.
	 * @param CameraDistance Distance from mesh center, cm. 0 = auto (2× mesh radius / tan(FOV/2)).
	 * @param OutPngPath    Where to write the PNG. Relative paths resolve to project root.
	 * @return True on success, false on any failure (logs to LogTemp).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool PreviewMaterial(
		const FString& MaterialPath,
		const FString& Mesh,
		const FString& Lighting,
		int32 Resolution,
		float CameraYawDeg,
		float CameraPitchDeg,
		float CameraDistance,
		const FString& OutPngPath);

	/**
	 * M1-7: Same as PreviewMaterial but with the ShaderComplexity view mode enabled.
	 * Output is the green→red heatmap UE's Shader Complexity viewmode produces.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool PreviewMaterialComplexity(
		const FString& MaterialPath,
		const FString& Mesh,
		const FString& Lighting,
		int32 Resolution,
		float CameraYawDeg,
		float CameraPitchDeg,
		float CameraDistance,
		const FString& OutPngPath);

	/**
	 * M2-1: Create a new UMaterial asset at Path.
	 * @param Path   Full content path (e.g. "/Game/Materials/M_MyMaster" — no extension).
	 * @param Domain        "Surface" / "DeferredDecal" / "LightFunction" / "Volume" / "PostProcess" / "UI" / "RuntimeVirtualTexture".
	 * @param ShadingModel  "DefaultLit" / "Unlit" / "Subsurface" / "PreintegratedSkin" / "ClearCoat" / "SubsurfaceProfile" / "TwoSidedFoliage" / "Hair" / "Cloth" / "Eye" / "SingleLayerWater" / "ThinTranslucent" / "FromMaterialExpression".
	 * @param BlendMode     "Opaque" / "Masked" / "Translucent" / "Additive" / "Modulate" / "AlphaComposite" / "AlphaHoldout".
	 * @param bTwoSided                 — usually true for foliage / skin.
	 * @param bUseMaterialAttributes    — true if you plan to drive the single MaterialAttributes pin (layered workflow).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeCreateAssetResult CreateMaterial(
		const FString& Path,
		const FString& Domain,
		const FString& ShadingModel,
		const FString& BlendMode,
		bool bTwoSided,
		bool bUseMaterialAttributes);

	/**
	 * M2-2: Create a new UMaterialInstanceConstant with the given parent.
	 * @param ParentPath    Full path to an existing UMaterial or UMaterialInstance.
	 * @param InstancePath  Full content path for the new MI.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeCreateAssetResult CreateMaterialInstance(
		const FString& ParentPath,
		const FString& InstancePath);

	/**
	 * M2-4: Create a single material expression node.
	 * @param ExpressionClass  Short name like "Constant3Vector", "ScalarParameter", "TextureSampleParameter2D",
	 *                         or a fully-qualified "/Script/Engine.MaterialExpressionConstant3Vector" path.
	 *                         "MaterialExpression"/"UMaterialExpression" prefix is auto-added for short names.
	 * @param X / Y            Editor-space position (MaterialExpressionEditorX / Y).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeAddExpressionResult AddMaterialExpression(
		const FString& MaterialPath,
		const FString& ExpressionClass,
		int32 X,
		int32 Y);

	/**
	 * M2-5: Connect one expression's output pin to another's input pin.
	 * Pin names are the names reported by get_material_graph (input_names / output_names) —
	 * "" or "None" resolves to the default pin.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool ConnectMaterialExpressions(
		const FString& MaterialPath,
		FGuid SrcGuid, const FString& SrcOutputName,
		FGuid DstGuid, const FString& DstInputName);

	/**
	 * M2-5: Clear one input pin (dst expression, named input). Returns true if a connection existed.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool DisconnectMaterialInput(
		const FString& MaterialPath,
		FGuid DstGuid, const FString& DstInputName);

	/**
	 * M2-6: Connect an expression to a main material property ("BaseColor", "Metallic", "Normal", ...).
	 * Property name matches get_material_graph's dst_property_name values (without "MP_" prefix).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool ConnectMaterialOutput(
		const FString& MaterialPath,
		FGuid SrcGuid, const FString& SrcOutputName,
		const FString& PropertyName);

	/**
	 * M2-6: Clear a main material property wire.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool DisconnectMaterialOutput(
		const FString& MaterialPath,
		const FString& PropertyName);

	/**
	 * M2-4 companion: Delete an expression by guid. Wires to/from it go away with it.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool DeleteMaterialExpression(
		const FString& MaterialPath,
		FGuid Guid);

	/**
	 * M2-11: Recompile the material and optionally save the asset.
	 * Blocks until the shader map is ready (synchronous path).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool CompileMaterial(
		const FString& MaterialPath,
		bool bSaveAfter);

	/**
	 * M2-7: Set a single UPROPERTY on an expression by name, using UE's ImportText.
	 *
	 * Value format follows ImportText conventions (the same format UE uses for .uasset
	 * text export):
	 *   Float / Int:    "1.5" / "42"
	 *   Bool:           "true" / "false"
	 *   String / Name:  "MyParam"               (no surrounding quotes)
	 *   FLinearColor /  "(R=1.0,G=0.5,B=0.2,A=1.0)"
	 *     Vectors:
	 *   Object ref:     "/Game/Textures/T_Base.T_Base"  (full path, no type prefix)
	 *   Enum:           "SAMPLERTYPE_Color" or just "Color"
	 *
	 * Common targets:
	 *   Constant                        R
	 *   Constant3Vector / Constant4     Constant
	 *   ScalarParameter / VectorParam   ParameterName / DefaultValue / Group / SortPriority
	 *   StaticSwitchParameter           ParameterName / DefaultValue
	 *   TextureSample / Parameter       Texture / SamplerType / SamplerSource / MipValueMode
	 *   MaterialFunctionCall            MaterialFunction
	 *   Comment                         Text / SizeX / SizeY / CommentColor
	 *   Custom                          Code / Description / OutputType
	 *   Any                             Desc / MaterialExpressionEditorX / Y
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool SetMaterialExpressionProperty(
		const FString& MaterialPath,
		FGuid Guid,
		const FString& PropertyName,
		const FString& Value);

	/**
	 * M2-7: Batch version of SetMaterialExpressionProperty — sets multiple properties
	 * on a single expression in one call with one PostEditChange broadcast.
	 * Returns the number of properties that applied successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static int32 SetMaterialExpressionProperties(
		const FString& MaterialPath,
		FGuid Guid,
		const TArray<FBridgeExpressionPropSet>& Properties);

	/**
	 * M2-8: Add a MaterialExpressionComment (group comment with resizable bounds).
	 * Returns the new comment's MaterialExpressionGuid, or invalid guid on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FGuid AddMaterialComment(
		const FString& MaterialPath,
		int32 X, int32 Y,
		int32 Width, int32 Height,
		const FString& Text,
		FLinearColor Color);

	/**
	 * M2-8: Add a MaterialExpressionReroute (transparent pass-through, single input/output).
	 * Useful for wire routing / organization.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FGuid AddMaterialReroute(
		const FString& MaterialPath,
		int32 X, int32 Y);

	/**
	 * M2-3: Create a new UMaterialFunction asset.
	 * Add UMaterialExpressionFunctionInput / Output nodes via add_material_expression on
	 * the returned path — there's no separate "add function input" op; the function pins
	 * are just special expressions inside the MF graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeCreateAssetResult CreateMaterialFunction(
		const FString& Path,
		const FString& Description,
		bool bExposeToLibrary,
		const FString& LibraryCategory);

	/**
	 * M2-10: Apply an ordered batch of graph ops in one round-trip.
	 * Back-references to newly-created nodes via "$N" syntax (index into the op list).
	 *
	 * Supported ops:
	 *   "add"            — class_name, x, y                       → guid
	 *   "comment"        — x, y, width, height, text, color       → guid
	 *   "reroute"        — x, y                                   → guid
	 *   "connect"        — src_ref, src_output, dst_ref, dst_input
	 *   "connect_out"    — src_ref, src_output, property
	 *   "disconnect_in"  — dst_ref, dst_input
	 *   "disconnect_out" — property
	 *   "set_prop"       — dst_ref, property, value               (ImportText-format)
	 *   "delete"         — dst_ref
	 *
	 * bCompile=true runs compile_material (synchronous) at the end if every op succeeded.
	 *
	 * Example (Python):
	 *   ops = [
	 *       make("add", class_name="Constant3Vector", x=-400, y=-100),        # -> $0
	 *       make("add", class_name="ScalarParameter", x=-400, y=60),          # -> $1
	 *       make("set_prop", dst_ref="$1", property="DefaultValue", value="0.35"),
	 *       make("connect_out", src_ref="$0", src_output="", property="BaseColor"),
	 *       make("connect_out", src_ref="$1", src_output="", property="Roughness"),
	 *   ]
	 *   apply_material_graph_ops(path, ops, compile=True)
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialGraphOpResult ApplyMaterialGraphOps(
		const FString& MaterialPath,
		const TArray<FBridgeMaterialGraphOp>& Ops,
		bool bCompile);

	/**
	 * M2-9: Topologically arrange all expressions based on distance from the main outputs.
	 * Columns go left (furthest from output) to right (at the output), with rows spread
	 * vertically per column. Overwrites existing MaterialExpressionEditorX / Y.
	 *
	 * @return Number of expressions repositioned.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static int32 AutoLayoutMaterialGraph(
		const FString& MaterialPath,
		int32 ColumnSpacing,
		int32 RowSpacing);

	/**
	 * M2-12: Produce a deterministic JSON snapshot of the graph (expressions + connections +
	 * main-output wiring) sorted and keyed for stable diffing.
	 * Pair with diff_material_graph_snapshots to detect what an edit changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FString SnapshotMaterialGraphJson(const FString& MaterialPath);

	/**
	 * M2-12: Diff two snapshot JSONs, returning a human-readable report of added /
	 * removed / moved nodes, added / removed wires, and changed key properties.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FString DiffMaterialGraphSnapshots(
		const FString& BeforeJson,
		const FString& AfterJson);

	/**
	 * M2.5-2: Add a UMaterialExpressionCustom node configured with named inputs, an HLSL
	 * body, output type, and include paths for BridgeSnippets.ush (or any other .ush).
	 *
	 * Typical agent flow:
	 *   1. Discover an available snippet: list_shared_snippets / get_shared_snippet
	 *   2. add_custom_expression(mat, x, y,
	 *          input_names=["BaseN", "DetailN", "Strength"],
	 *          output_type="Float3",
	 *          code='return BridgeBlendAngleCorrectedNormals(BaseN, DetailN, Strength);',
	 *          include_paths=["/Plugin/UnrealBridge/BridgeSnippets.ush"],
	 *          description="Detail normal blend (RNM)")
	 *   3. Connect ops wire each named input by its InputName via connect_material_expressions.
	 *
	 * OutputType values: "Float1" / "Float2" / "Float3" / "Float4" / "MaterialAttributes".
	 *
	 * Because the Custom node's pin names come from the InputNames array, the order in
	 * that array is also the input pin order, and agents should use the names — not
	 * indices — when wiring.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeAddExpressionResult AddCustomExpression(
		const FString& MaterialPath,
		int32 X, int32 Y,
		const TArray<FString>& InputNames,
		const FString& OutputType,
		const FString& Code,
		const TArray<FString>& IncludePaths,
		const FString& Description);

	/**
	 * M2.5-4: Enumerate the snippets declared in BridgeSnippets.ush. Fast — reads the file
	 * once, parses the //@snippet headers, returns metadata without function bodies.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static TArray<FBridgeShaderSnippet> ListSharedSnippets();

	/**
	 * M2.5-4: Fetch one snippet by name including its full function body.
	 * Returns a struct with .name empty if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeShaderSnippet GetSharedSnippet(const FString& Name);

	/**
	 * M6-1: Write a batch of parameter overrides onto a Material Instance (or MPC).
	 * Single dispatch covers scalar / vector / texture / static-switch by Type string.
	 * Each entry is independent — a failed param doesn't abort the rest; failures are
	 * reported in the Skipped array with reason text.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMIParamResult SetMIParams(
		const FString& MaterialInstancePath,
		const TArray<FBridgeMIParamSet>& Params);

	/**
	 * M6-2: Set MI parameters + render in one atomic call. The inverse of the usual
	 * "tweak, then render, then tweak again" round-trip. Saves the MI after applying
	 * so changes persist; use a disposable MI if you want non-destructive sweep.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool SetMIAndPreview(
		const FString& MaterialInstancePath,
		const TArray<FBridgeMIParamSet>& Params,
		const FString& Mesh,
		const FString& Lighting,
		int32 Resolution,
		float CameraYawDeg,
		float CameraPitchDeg,
		float CameraDistance,
		const FString& OutPngPath);

	/**
	 * M6-3: Sweep a single scalar / vector parameter over a list of values, rendering each
	 * into a grid-composited PNG. The MI is restored to its prior override state at the
	 * end so the sweep is non-destructive.
	 *
	 * @param ParamName  Parameter to vary (must be Scalar or Vector; others rejected).
	 * @param Values     ImportText-format values. Scalar: "0.1" "0.5" etc. Vector: "(R=1,G=0,B=0,A=1)".
	 * @param GridCols   0 = auto sqrt(N); otherwise forces a specific column count.
	 * @param OutGridPath   Where the combined image goes. Per-cell images land next to it with "_N" suffix.
	 *
	 * Returns path list: [grid_path, cell0, cell1, ...] on success, empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static TArray<FString> SweepMIParams(
		const FString& MaterialInstancePath,
		const FString& ParamName,
		const TArray<FString>& Values,
		const FString& Mesh,
		const FString& Lighting,
		int32 Resolution,
		float CameraYawDeg,
		float CameraPitchDeg,
		float CameraDistance,
		int32 GridCols,
		const FString& OutGridPath);

	/**
	 * M6-4: Write scalar / vector defaults onto a UMaterialParameterCollection.
	 * Uses the same FBridgeMIParamSet payload as set_mi_params. Type must be "Scalar"
	 * or "Vector" (the two MPC value kinds). MPC is saved after mutation.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMIParamResult SetMaterialParameterCollection(
		const FString& CollectionPath,
		const TArray<FBridgeMIParamSet>& Params);

	/**
	 * M6-5: Diff two MIs' override values. Text report formatted like:
	 *   + Scalar Roughness = 0.35     (set in B, absent in A)
	 *   - Vector Tint = (R=1,...)      (set in A, absent in B)
	 *   ~ Scalar Metallic: 0.5 -> 0.8  (different value)
	 * Parent chains are not walked — only each MI's own overrides are compared.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FString DiffMIParams(
		const FString& PathA,
		const FString& PathB);

	/**
	 * M4-1: Create a new UMaterial at Path with Domain=PostProcess, ready to
	 * be wired to SceneTexture / Sobel / tonemap chains.
	 *
	 * @param BlendableLocation  "AfterTonemapping" (default, LDR post-FX),
	 *                           "BeforeTonemapping" (HDR, pre-tonemap),
	 *                           "BeforeBloom", "SSRInput", "ReplacingTonemapper".
	 * @param bOutputAlpha        Set `BlendableOutputAlpha` — needed for masks
	 *                           that compose over the translucency pass.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeCreateAssetResult CreatePostProcessMaterial(
		const FString& Path,
		const FString& BlendableLocation,
		bool bOutputAlpha);

	/**
	 * M4-8: Enumerate every APostProcessVolume in the current editor world,
	 * returning its enable state, bounds mode, priority / blend weight, and
	 * the full list of WeightedBlendables (material paths + weights).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static TArray<FBridgePostProcessVolumeInfo> GetPostProcessState();

	/**
	 * M4-7: Append a PostProcess material (or MI) to a volume's
	 * WeightedBlendables. Updates the existing entry if the material is
	 * already present.
	 *
	 * @param VolumeActor    Actor label (as shown in the Outliner). Pass "" to
	 *                       target the first unbound PPV in the level — and
	 *                       spawn a new unbound PPV if none exists yet.
	 * @param MaterialPath   Material or MI path. Domain must be PostProcess.
	 * @param Weight         0..1 blend strength.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool ApplyPostProcessMaterial(
		const FString& VolumeActor,
		const FString& MaterialPath,
		float Weight);

	/**
	 * M4-7 companion: Remove a PostProcess material from a volume's
	 * WeightedBlendables. Returns false if the material wasn't present.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static bool RemovePostProcessMaterial(
		const FString& VolumeActor,
		const FString& MaterialPath);

	/**
	 * M5-1: Full lint-pass over a Material. Combines M1-3 shader stats + M1-4
	 * compile errors with a rules engine that flags common authoring / performance
	 * bugs. Fires the following rules (each emits 0+ findings):
	 *
	 *   M5-2  budget       — MaxInstructions > InstructionBudget, or SamplerCount > SamplerBudget
	 *                        (only when the respective budget is > 0; skip otherwise).
	 *   M5-3  unreachable  — nodes with no path to any main output via back-BFS from
	 *                        the MP_* inputs. Comments + reroutes are ignored.
	 *   M5-4  dup texture  — two or more TextureSample / TextureSampleParameter2D
	 *                        nodes reading the same texture with the same UV wire
	 *                        — suggest sharing a single sample.
	 *   M5-5  sampler src  — mixing SSM_FromTextureAsset and SSM_Wrap_WorldGroupSettings
	 *                        across sibling texture samples wastes sampler slots
	 *                        on dense PBR masters.
	 *   M5-8  shading/wire — DefaultLit missing BaseColor/Normal, Unlit wiring
	 *                        Metallic/Specular/Roughness (dead connections), etc.
	 *
	 * @param InstructionBudget  >0 to enforce; 0 to skip M5-2 instruction check.
	 * @param SamplerBudget      >0 to enforce; 0 to skip M5-2 sampler check.
	 *
	 * bShaderStatsReady=false means the shader map is still compiling — the
	 * graph-structure rules still ran, but MaxInstructions is unreliable. Call
	 * again after the compile settles.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialAnalysis AnalyzeMaterial(
		const FString& MaterialPath,
		int32 InstructionBudget,
		int32 SamplerBudget);

	/**
	 * M5-14: Apply the safe subset of lint auto-fixes to a Material.
	 *
	 * Supported fix IDs:
	 *   "drop_unused"          — delete every expression flagged by M5-3.
	 *                            Safe: unreachable nodes contribute zero to
	 *                            the compiled shader.
	 *   "samplersource_share"  — flip every TextureSample whose SamplerSource
	 *                            was flagged by M5-5 to SSM_Wrap_WorldGroupSettings
	 *                            so it feeds the global shared sampler.
	 *   "static_switch_conversion" — replace each M5-6 Pattern 2 Lerp whose
	 *                            Alpha is a boolean-intent ScalarParameter
	 *                            (Use* / Enable* / Is* / Has* / Should* / Show* /
	 *                            Bool* / Toggle*) with a StaticSwitchParameter
	 *                            that shares the same name. The dead side
	 *                            compiles out of the permutation. Deletes the
	 *                            ScalarParameter when no other node refers to it.
	 *   "inline_trivial_custom"  — replace each M5-11 flagged trivial Custom
	 *                            node whose body is a single-op expression
	 *                            (Add / Subtract / Multiply / Divide / Saturate
	 *                            / Abs / OneMinus / Frac / Floor / Ceil / Min
	 *                            / Max / Power / Lerp / Dot / Normalize) with
	 *                            the equivalent native graph node. Preserves
	 *                            the original input → new input wiring.
	 *
	 * Any unrecognised fix id lands in `SkippedFixes` on the result — the call
	 * itself still succeeds for the fixes it could apply. Material is
	 * PostEditChanged + saved when ``bSaveAfter`` is true.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialAutoFixResult AutoFixMaterial(
		const FString& MaterialPath,
		const TArray<FString>& Fixes,
		bool bSaveAfter);

	/**
	 * M3-7: Configure a MaterialAttributeLayers expression's DefaultLayers
	 * stack via the engine's authoritative FMaterialLayersFunctions APIs
	 * (AddDefaultBackgroundLayer + AppendBlendedLayer).
	 *
	 * Python-side direct set_editor_property("default_layers", ...) with a
	 * manually-constructed FMaterialLayersFunctions crashes UE 5.7 because
	 * it bypasses the LayerGuids + LayerLinkStates + background-GUID
	 * initialization that the struct's ctor/append methods do. This
	 * UFUNCTION uses the right authoring path + validates layer/blend/name
	 * counts, so Python only has to supply the MF pointers and display names.
	 *
	 * ``Layers`` must be ≥ 1 (first entry is the background).
	 * ``Blends.Num() == Layers.Num() - 1`` — Blends[i] is the blend MF that
	 *   composites Layers[i+1] onto the accumulated stack below it.
	 * ``LayerNames.Num() == Layers.Num()`` — display names in the editor's
	 *   Layers panel.
	 *
	 * Transactional (single FScopedTransaction) + calls PostEditChange so the
	 * material recompiles with the new stack.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Material")
	static FBridgeMaterialGraphOpResult SetMaterialAttributeLayers(
		const FString& MaterialPath,
		FGuid ExpressionGuid,
		const TArray<UMaterialFunctionInterface*>& Layers,
		const TArray<UMaterialFunctionInterface*>& Blends,
		const TArray<FString>& LayerNames);
};

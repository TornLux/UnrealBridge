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
};

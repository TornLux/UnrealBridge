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
};

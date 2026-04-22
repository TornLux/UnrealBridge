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
};

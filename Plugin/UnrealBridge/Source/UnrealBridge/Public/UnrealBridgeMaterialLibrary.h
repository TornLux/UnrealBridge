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
};

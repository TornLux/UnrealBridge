#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeGameplayAbilityLibrary.generated.h"

/** Overview of a UGameplayAbility Blueprint (read from CDO). */
USTRUCT(BlueprintType)
struct FBridgeGameplayAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Parent class name (e.g. "GameplayAbility" or a custom base). */
	UPROPERTY(BlueprintReadOnly)
	FString ParentClassName;

	/** "InstancedPerActor", "InstancedPerExecution", "NonInstanced", or empty on failure. */
	UPROPERTY(BlueprintReadOnly)
	FString InstancingPolicy;

	/** "LocalPredicted", "ServerInitiated", "ServerOnly", "LocalOnly", or empty on failure. */
	UPROPERTY(BlueprintReadOnly)
	FString NetExecutionPolicy;

	/** AbilityTags on the CDO. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> AbilityTags;

	/** Path to the cost GameplayEffect class, if any. */
	UPROPERTY(BlueprintReadOnly)
	FString CostGameplayEffectClass;

	/** Path to the cooldown GameplayEffect class, if any. */
	UPROPERTY(BlueprintReadOnly)
	FString CooldownGameplayEffectClass;
};

/**
 * GameplayAbilitySystem introspection via UnrealBridge.
 * Scaffold library — extended incrementally by the iteration loop.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeGameplayAbilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Read ability metadata from a UGameplayAbility Blueprint's CDO. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeGameplayAbilityInfo GetGameplayAbilityBlueprintInfo(const FString& AbilityBlueprintPath);
};

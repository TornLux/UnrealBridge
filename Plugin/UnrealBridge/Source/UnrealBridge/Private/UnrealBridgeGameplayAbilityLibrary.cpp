#include "UnrealBridgeGameplayAbilityLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/Blueprint.h"
#include "GameplayTagContainer.h"
#include "UObject/UObjectGlobals.h"

static UClass* LoadGeneratedClassFromBlueprint(const FString& BlueprintPath)
{
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP || !BP->GeneratedClass)
	{
		return nullptr;
	}
	return BP->GeneratedClass;
}

static void TagContainerToStrings(const FGameplayTagContainer& Tags, TArray<FString>& Out)
{
	for (const FGameplayTag& Tag : Tags)
	{
		Out.Add(Tag.ToString());
	}
}

FBridgeGameplayAbilityInfo UUnrealBridgeGameplayAbilityLibrary::GetGameplayAbilityBlueprintInfo(
	const FString& AbilityBlueprintPath)
{
	FBridgeGameplayAbilityInfo Result;

	UClass* AbilityClass = LoadGeneratedClassFromBlueprint(AbilityBlueprintPath);
	if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: '%s' is not a UGameplayAbility Blueprint"), *AbilityBlueprintPath);
		return Result;
	}

	UGameplayAbility* CDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
	if (!CDO)
	{
		return Result;
	}

	Result.Name = AbilityClass->GetName();

	if (UClass* Super = AbilityClass->GetSuperClass())
	{
		Result.ParentClassName = Super->GetName();
	}

	Result.InstancingPolicy = StaticEnum<EGameplayAbilityInstancingPolicy::Type>()
		->GetNameStringByValue(static_cast<int64>(CDO->GetInstancingPolicy()));
	Result.NetExecutionPolicy = StaticEnum<EGameplayAbilityNetExecutionPolicy::Type>()
		->GetNameStringByValue(static_cast<int64>(CDO->GetNetExecutionPolicy()));

	TagContainerToStrings(CDO->GetAssetTags(), Result.AbilityTags);

	if (CDO->GetCostGameplayEffect() && CDO->GetCostGameplayEffect()->GetClass())
	{
		Result.CostGameplayEffectClass = CDO->GetCostGameplayEffect()->GetClass()->GetPathName();
	}
	if (CDO->GetCooldownGameplayEffect() && CDO->GetCooldownGameplayEffect()->GetClass())
	{
		Result.CooldownGameplayEffectClass = CDO->GetCooldownGameplayEffect()->GetClass()->GetPathName();
	}

	return Result;
}

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

// ─── GameplayEffect structs ─────────────────────────────────

/** Single modifier entry on a GameplayEffect. */
USTRUCT(BlueprintType)
struct FBridgeGEModifierInfo
{
	GENERATED_BODY()

	/** Attribute being modified (e.g. "Health.Damage"). */
	UPROPERTY(BlueprintReadOnly)
	FString Attribute;

	/** "Additive", "Multiplicitive", "Division", "Override", or empty. */
	UPROPERTY(BlueprintReadOnly)
	FString ModOp;

	/** Best-effort constant magnitude (only populated for ScalableFloat with constant curve). */
	UPROPERTY(BlueprintReadOnly)
	float Magnitude = 0.f;

	/** "ScalableFloat", "AttributeBased", "CustomMagnitude", "SetByCaller", or empty. */
	UPROPERTY(BlueprintReadOnly)
	FString MagnitudeSource;
};

/** One GEComponent entry with any extracted tag containers. */
USTRUCT(BlueprintType)
struct FBridgeGEComponentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	/** Flat list of "PropertyName: Tag" entries for every FGameplayTagContainer / FInheritedTagContainer found. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Tags;
};

/** Overview of a UGameplayEffect Blueprint (read from CDO). */
USTRUCT(BlueprintType)
struct FBridgeGameplayEffectInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString ParentClassName;

	/** "Instant", "HasDuration", "Infinite", or empty. */
	UPROPERTY(BlueprintReadOnly)
	FString DurationPolicy;

	/** Best-effort constant duration in seconds (-1 when non-constant / infinite / instant). */
	UPROPERTY(BlueprintReadOnly)
	float DurationSeconds = -1.f;

	/** Best-effort constant period in seconds (0 = non-periodic). */
	UPROPERTY(BlueprintReadOnly)
	float PeriodSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeGEModifierInfo> Modifiers;

	/** "None", "AggregateBySource", "AggregateByTarget", or empty. */
	UPROPERTY(BlueprintReadOnly)
	FString StackingType;

	UPROPERTY(BlueprintReadOnly)
	int32 StackLimitCount = 0;

	/** GEComponents attached to this effect (tag requirements / asset tags / etc. live here in UE5.3+). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeGEComponentInfo> Components;
};

// ─── AttributeSet structs ───────────────────────────────────

/** A single attribute on a UAttributeSet class. */
USTRUCT(BlueprintType)
struct FBridgeAttributeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Underlying struct type, e.g. "GameplayAttributeData". */
	UPROPERTY(BlueprintReadOnly)
	FString Type;

	/** CDO base value. */
	UPROPERTY(BlueprintReadOnly)
	float BaseValue = 0.f;
};

/** Overview of a UAttributeSet class. */
USTRUCT(BlueprintType)
struct FBridgeAttributeSetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString ParentClassName;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAttributeInfo> Attributes;
};

// ─── Actor ASC structs ──────────────────────────────────────

/** One granted ability spec entry on an ASC. */
USTRUCT(BlueprintType)
struct FBridgeGrantedAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString AbilityClassName;

	UPROPERTY(BlueprintReadOnly)
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 InputID = -1;

	UPROPERTY(BlueprintReadOnly)
	bool bIsActive = false;
};

/** AbilitySystemComponent snapshot on an actor. */
USTRUCT(BlueprintType)
struct FBridgeActorAbilitySystemInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ActorName;

	/** True if an ASC was found on (or owned by) the actor. */
	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeGrantedAbilityInfo> GrantedAbilities;

	/** Currently owned gameplay tags (from effects + loose). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> OwnedTags;

	UPROPERTY(BlueprintReadOnly)
	int32 ActiveEffectCount = 0;

	/** AttributeSet class names spawned on this ASC. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> AttributeSetClasses;
};

/**
 * GameplayAbilitySystem introspection via UnrealBridge.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeGameplayAbilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Read ability metadata from a UGameplayAbility Blueprint's CDO. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeGameplayAbilityInfo GetGameplayAbilityBlueprintInfo(const FString& AbilityBlueprintPath);

	/**
	 * Read GameplayEffect metadata from a UGameplayEffect Blueprint's CDO.
	 * Duration/period extraction is best-effort: only constant ScalableFloats resolve; dynamic
	 * curves/SetByCaller magnitudes are reported in MagnitudeSource without a numeric value.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeGameplayEffectInfo GetGameplayEffectBlueprintInfo(const FString& EffectBlueprintPath);

	/**
	 * Read AttributeSet class metadata. Accepts:
	 *   - native class path ("/Script/MyModule.MyAttributeSet")
	 *   - BP asset path ("/Game/AS/BP_MyAttributeSet")
	 * Returns the FGameplayAttributeData fields on the class with CDO base values.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeAttributeSetInfo GetAttributeSetInfo(const FString& AttributeSetClassPath);

	/**
	 * Find all GameplayAbility Blueprints whose AssetTags contain a tag matching the query.
	 * @param TagQuery       Exact tag string (e.g. "Ability.Combat.Attack") or a parent tag — matches
	 *                       via FGameplayTag::MatchesTag (so parents match children).
	 * @param MaxResults     0 = unlimited (careful on large projects).
	 * @return Asset paths of matching ability Blueprints.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListAbilitiesByTag(const FString& TagQuery, int32 MaxResults);

	/**
	 * Find all GameplayEffect Blueprints whose AssetTags / GrantedTags contain a tag matching the query.
	 * Uses the same tag-match semantics as ListAbilitiesByTag.
	 * @param MaxResults     0 = unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListGameplayEffectsByTag(const FString& TagQuery, int32 MaxResults);

	/**
	 * Return the ASC snapshot for an actor in the editor world.
	 * Handles both actors implementing IAbilitySystemInterface and actors with a
	 * UAbilitySystemComponent subobject directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeActorAbilitySystemInfo GetActorAbilitySystemInfo(const FString& ActorName);

	/**
	 * Dump registered gameplay tags, optionally filtered by case-insensitive substring.
	 * @param Filter       Substring filter; pass "" to match all (requires MaxResults > 0).
	 * @param MaxResults   0 = unlimited (only when Filter is non-empty — empty filter + 0 is refused).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListGameplayTags(const FString& Filter, int32 MaxResults);
};

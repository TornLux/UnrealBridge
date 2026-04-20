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

/** Live attribute value read from an ASC. */
USTRUCT(BlueprintType)
struct FBridgeAttributeValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString AttributeName;

	/** True when the attribute was resolved on the actor's ASC. */
	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	/** Current (modified) value. */
	UPROPERTY(BlueprintReadOnly)
	float CurrentValue = 0.f;

	/** Base value (pre-modifier). */
	UPROPERTY(BlueprintReadOnly)
	float BaseValue = 0.f;
};

/** Cooldown metadata for a specific ability on an actor's ASC. */
USTRUCT(BlueprintType)
struct FBridgeAbilityCooldownInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString AbilityClassName;

	/** True when the ability spec was located on the ASC. */
	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	/** True when the ability is currently on cooldown. */
	UPROPERTY(BlueprintReadOnly)
	bool bOnCooldown = false;

	/** Seconds remaining on the active cooldown (0 when off cooldown). */
	UPROPERTY(BlueprintReadOnly)
	float TimeRemaining = 0.f;

	/** Total cooldown duration for the current application (0 when off cooldown). */
	UPROPERTY(BlueprintReadOnly)
	float CooldownDuration = 0.f;

	/** Tag strings that put this ability into cooldown (from the ability CDO). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> CooldownTags;
};

/** One active GameplayEffect entry on an ASC. */
USTRUCT(BlueprintType)
struct FBridgeActiveEffectInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString EffectClassName;

	/** Seconds remaining; -1 for infinite / non-duration effects. */
	UPROPERTY(BlueprintReadOnly)
	float TimeRemaining = 0.f;

	/** Total duration of this application. */
	UPROPERTY(BlueprintReadOnly)
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 StackCount = 1;

	/** Period length for periodic effects (0 = non-periodic). */
	UPROPERTY(BlueprintReadOnly)
	float PeriodSeconds = 0.f;

	/** Dynamic tags granted by the spec (design-time component tags live on the GE class). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> DynamicGrantedTags;
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

/** Design-time tag requirement sets on a UGameplayAbility CDO. */
USTRUCT(BlueprintType)
struct FBridgeAbilityTagRequirements
{
	GENERATED_BODY()

	/** Resolved ability class name (empty on failure). */
	UPROPERTY(BlueprintReadOnly)
	FString AbilityClassName;

	/** Abilities with any of these tags are cancelled when this ability activates. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> CancelAbilitiesWithTag;

	/** Abilities with any of these tags are blocked while this ability is active. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> BlockAbilitiesWithTag;

	/** Tags applied to the activating owner while this ability is active. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ActivationOwnedTags;

	/** Owner must have all of these tags to activate. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ActivationRequiredTags;

	/** Blocked if the owner has any of these tags. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ActivationBlockedTags;

	/** Source actor must have all of these tags. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> SourceRequiredTags;

	/** Blocked if the source actor has any of these tags. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> SourceBlockedTags;

	/** Target actor must have all of these tags. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> TargetRequiredTags;

	/** Blocked if the target actor has any of these tags. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> TargetBlockedTags;
};

/** One FAbilityTriggerData entry on a UGameplayAbility CDO. */
USTRUCT(BlueprintType)
struct FBridgeAbilityTriggerInfo
{
	GENERATED_BODY()

	/** Trigger tag string. */
	UPROPERTY(BlueprintReadOnly)
	FString TriggerTag;

	/** "GameplayEvent", "OwnedTagAdded", "OwnedTagPresent", or "Unknown". */
	UPROPERTY(BlueprintReadOnly)
	FString TriggerSource;
};

/** One currently-active ability on an ASC (ActiveCount > 0). */
USTRUCT(BlueprintType)
struct FBridgeActiveAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString AbilityClassName;

	UPROPERTY(BlueprintReadOnly)
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 InputID = -1;

	/** Number of currently running instances (always >= 1 when reported). */
	UPROPERTY(BlueprintReadOnly)
	int32 ActiveCount = 0;
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

	/**
	 * List loaded UAttributeSet subclasses (native + already-loaded BP).
	 * Note: BP attribute sets that have not been loaded into memory yet will not appear; open a
	 * relevant asset or run an ability that references them first.
	 * @param Filter       Case-insensitive substring on class name/path; "" matches all (requires MaxResults > 0).
	 * @param MaxResults   0 = unlimited (only when Filter is non-empty — empty filter + 0 is refused).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListAttributeSets(const FString& Filter, int32 MaxResults);

	/**
	 * Read a live attribute value off an actor's ASC.
	 * @param ActorName        Actor label or name in the editor world.
	 * @param AttributeName    Attribute field name; may be qualified ("MyAttributeSet.Health") or bare ("Health").
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeAttributeValue GetAttributeValue(const FString& ActorName, const FString& AttributeName);

	/**
	 * Enumerate active GameplayEffects on an actor's ASC with remaining-time/stack metadata.
	 * @param MaxResults   0 = unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FBridgeActiveEffectInfo> GetActorActiveEffects(const FString& ActorName, int32 MaxResults);

	/**
	 * Enumerate children of a gameplay tag in the tag hierarchy.
	 * @param bRecursive   When true, returns all descendants; when false, only immediate children.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> FindChildTags(const FString& ParentTag, bool bRecursive);

	/**
	 * Return the ancestor chain of a gameplay tag (excluding the tag itself, root first).
	 * Example: "A.B.C" -> ["A", "A.B"]. Invalid tag returns empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> GetTagParents(const FString& TagString);

	/**
	 * Query whether an actor's ASC currently owns a gameplay tag.
	 * @param ActorName      Actor label or FName in the editor world.
	 * @param TagString      Tag to test (must be registered).
	 * @param bExactMatch    When true, requires an exact tag match; when false, matches parents too (child tags satisfy parent queries).
	 * @param OutTagCount    Current stack count for the tag on the ASC (0 when not owned).
	 * @return True when the ASC owns the tag under the selected match mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool ActorHasGameplayTag(
		const FString& ActorName,
		const FString& TagString,
		bool bExactMatch,
		int32& OutTagCount);

	/**
	 * Query cooldown state for a specific ability on an actor's ASC.
	 * @param ActorName              Actor label or FName in the editor world.
	 * @param AbilityBlueprintPath   Path to a UGameplayAbility Blueprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeAbilityCooldownInfo GetAbilityCooldownInfo(
		const FString& ActorName,
		const FString& AbilityBlueprintPath);

	/**
	 * Filter active effects on an actor's ASC whose spec's asset tags or granted tags match TagQuery.
	 * Matches the design-time tags from the GE class (asset tags + component granted tags) and dynamic granted tags.
	 * @param MaxResults   0 = unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FBridgeActiveEffectInfo> FindActiveEffectsByTag(
		const FString& ActorName,
		const FString& TagQuery,
		int32 MaxResults);

	/**
	 * List all UGameplayAbility Blueprint asset paths in the AssetRegistry, with optional case-insensitive
	 * path substring filter. Empty filter + MaxResults=0 is refused to prevent accidental full dumps.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListAbilityBlueprints(const FString& Filter, int32 MaxResults);

	/**
	 * List all UGameplayEffect Blueprint asset paths in the AssetRegistry, with optional case-insensitive
	 * path substring filter. Empty filter + MaxResults=0 is refused. Complements ListGameplayEffectsByTag
	 * when you don't yet know which tags are registered in the project.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListGameplayEffectBlueprints(const FString& Filter, int32 MaxResults);

	/**
	 * List UAttributeSet Blueprint asset paths in the AssetRegistry. Unlike ListAttributeSets (which only
	 * reports loaded classes), this finds on-disk BP subclasses without loading them. Native AttributeSet
	 * classes are not included here — use ListAttributeSets for those.
	 * Empty filter + MaxResults=0 is refused.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListAttributeSetBlueprints(const FString& Filter, int32 MaxResults);

	/**
	 * Quick validity check — true when TagString is registered with UGameplayTagsManager.
	 * No side effects; safe to call in a hot loop to sanitise user-supplied tag input before
	 * passing to tag-query calls.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool IsValidGameplayTag(const FString& TagString);

	/**
	 * Test whether TagA matches TagB under gameplay-tag hierarchy rules.
	 *  - bExactMatch=true  → true only when A == B.
	 *  - bExactMatch=false → true when A equals B or any ancestor of B equals A (A matches B via MatchesTag on B, i.e. B is a descendant of A or equal).
	 * Returns false when either tag is unregistered.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool TagMatches(const FString& TagA, const FString& TagB, bool bExactMatch);

	/**
	 * Read every live attribute on every spawned AttributeSet of an actor's ASC in one call.
	 * Cheaper than looping GetAttributeValue per attribute — a single ASC walk. Empty list when
	 * the actor has no ASC or no spawned attribute sets.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FBridgeAttributeValue> GetActorAttributes(const FString& ActorName);

	/**
	 * Read the full tag-requirement surface of a UGameplayAbility Blueprint's CDO — every
	 * activation/source/target/cancel/block container in one call. Complements
	 * GetGameplayAbilityBlueprintInfo (which only reports AssetTags).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FBridgeAbilityTagRequirements GetAbilityTagRequirements(const FString& AbilityBlueprintPath);

	/**
	 * Enumerate FAbilityTriggerData entries on a UGameplayAbility Blueprint's CDO. Empty list
	 * when the ability isn't event/tag-triggered.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FBridgeAbilityTriggerInfo> GetAbilityTriggers(const FString& AbilityBlueprintPath);

	/**
	 * Return tags currently blocking ability activation on an actor's ASC
	 * (ASC->GetBlockedAbilityTags() — tags contributed by other active abilities' BlockAbilitiesWithTag).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> GetActorBlockedAbilityTags(const FString& ActorName);

	/**
	 * Check whether an ability would pass its Activation/Source/Target tag requirements against an
	 * actor's current ASC tag state. Runs UGameplayAbility::DoesAbilitySatisfyTagRequirements.
	 * Does not check cost, cooldown, or blocked tags — pair with GetAbilityCooldownInfo and
	 * GetActorBlockedAbilityTags for a fuller picture.
	 *
	 * @param OutBlockingTags   Populated with the tags that caused failure (empty on success).
	 * @return true when all tag gates pass.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool ActorAbilityMeetsTagRequirements(
		const FString& ActorName,
		const FString& AbilityBlueprintPath,
		TArray<FString>& OutBlockingTags);

	/**
	 * Enumerate abilities whose ActiveCount > 0 on an actor's ASC. This is a targeted subset of
	 * GetActorAbilitySystemInfo().granted_abilities — only the currently-running ones, with stack count.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FBridgeActiveAbilityInfo> GetActorActiveAbilities(const FString& ActorName);

	/**
	 * Send a GameplayEvent to an actor's ASC by name. Searches PIE worlds first
	 * (so this works during play), then the editor world. Mirrors
	 * UAbilitySystemBlueprintLibrary::SendGameplayEventToActor with a string
	 * actor handle so callers don't need a Python UObject reference.
	 *
	 * @param ActorName       Actor label or FName.
	 * @param EventTag        Registered gameplay tag to fire.
	 * @param EventMagnitude  Optional magnitude payload (FGameplayEventData::EventMagnitude).
	 * @return Number of triggered abilities (0 means no abilities responded — the event
	 *         still fired and any registered GenericGameplayEventCallbacks will run).
	 *         -1 indicates the actor or its ASC could not be resolved.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static int32 SendGameplayEventByName(
		const FString& ActorName,
		const FString& EventTag,
		float EventMagnitude);

	/**
	 * Ensure a UAbilitySystemComponent exists at the given Location, creating
	 * one if absent. Searches PIE worlds first, then editor world.
	 *
	 * @param Location  "Actor" (default) attaches ASC to the actor itself;
	 *                  "Controller" attaches to the actor's Controller (pawn only);
	 *                  "PlayerState" attaches to the actor's PlayerState (pawn only).
	 *                  Useful for exercising the ASC resolution walker with
	 *                  non-standard placements during tests.
	 *
	 * If an ASC already exists at the requested Location, this is a no-op
	 * and returns true.
	 *
	 * @return true on success, false when the actor/target can't be found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool EnsureAbilitySystemComponent(const FString& ActorName, const FString& Location = TEXT("Actor"));

	/**
	 * Test scaffolding: ensure the actor has a UBridgeTestAttributeSet (Health+Mana)
	 * registered on its ASC. Resolves the ASC via the standard walker
	 * (Actor / Pawn.PlayerState / Pawn.Controller / PC.PlayerState). Attribute
	 * set is initialized with Health=100, Mana=100. Safe to call multiple times
	 * — re-registration is avoided if already present.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool EnsureBridgeTestAttributeSet(const FString& ActorName);

	/**
	 * Set a numeric attribute on the actor's ASC via SetNumericAttributeBase.
	 * Fires the attribute-value-change delegate that FBridgeAttributeChangedAdapter
	 * listens to. AttributeName accepts "AttrSet.Field" or bare "Field".
	 *
	 * @return true when the attribute was found and written.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool SetActorAttributeValue(const FString& ActorName, const FString& AttributeName, float Value);

	// ─── GameplayAbility Blueprint CDO writes (M1) ─────────────

	/**
	 * Overwrite one of the ten FGameplayTagContainer UPROPERTYs on a
	 * UGameplayAbility Blueprint's CDO. The `ContainerName` is the C++ field
	 * name (case-sensitive). Any existing tags in that container are replaced
	 * — pass an empty `Tags` array to clear.
	 *
	 * Accepted container names:
	 *   AbilityTags           — aka "AssetTags" in the editor; grants these tags to
	 *                           activated instances and lets ListAbilitiesByTag find them.
	 *   CancelAbilitiesWithTag— abilities with any of these tags are cancelled when
	 *                           this one activates.
	 *   BlockAbilitiesWithTag — abilities with any of these tags are blocked while
	 *                           this one is active.
	 *   ActivationOwnedTags   — tags applied to the activating owner while active.
	 *   ActivationRequiredTags — owner must have all of these to activate.
	 *   ActivationBlockedTags — blocked if owner has any of these.
	 *   SourceRequiredTags    — source actor must have all of these.
	 *   SourceBlockedTags     — blocked if source actor has any.
	 *   TargetRequiredTags    — target actor must have all of these.
	 *   TargetBlockedTags     — blocked if target actor has any.
	 *
	 * Unregistered tags are silently skipped with a warning log; the final
	 * container contains only the tags that resolved. Returns the number of
	 * tags actually written, or -1 if the BP / container / policy is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static int32 SetAbilityTagContainer(
		const FString& AbilityBlueprintPath,
		const FString& ContainerName,
		const TArray<FString>& Tags);

	/**
	 * Set the ability's InstancingPolicy. Accepted values:
	 *   "InstancedPerActor"     (default / recommended)
	 *   "InstancedPerExecution"
	 *   "NonInstanced"          (deprecated since UE 5.5 — warns)
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool SetAbilityInstancingPolicy(const FString& AbilityBlueprintPath, const FString& Policy);

	/**
	 * Set the ability's NetExecutionPolicy. Accepted values:
	 *   "LocalPredicted" | "LocalOnly" | "ServerInitiated" | "ServerOnly"
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool SetAbilityNetExecutionPolicy(const FString& AbilityBlueprintPath, const FString& Policy);

	/**
	 * Set the cost GameplayEffect class on a UGameplayAbility CDO. Pass an
	 * empty string to clear (no cost).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool SetAbilityCost(
		const FString& AbilityBlueprintPath,
		const FString& CostGameplayEffectClassPath);

	/**
	 * Set the cooldown GameplayEffect class on a UGameplayAbility CDO. Pass
	 * an empty string to clear (no cooldown).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static bool SetAbilityCooldown(
		const FString& AbilityBlueprintPath,
		const FString& CooldownGameplayEffectClassPath);

	/**
	 * Append an FAbilityTriggerData entry to the ability's AbilityTriggers
	 * array (no dedup — a duplicate tag is allowed). Returns the new array
	 * length on success, -1 on invalid input.
	 *
	 * @param TriggerSource   "GameplayEvent" | "OwnedTagAdded" | "OwnedTagPresent".
	 *                        Short name or case-insensitive match is accepted.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static int32 AddAbilityTrigger(
		const FString& AbilityBlueprintPath,
		const FString& TriggerTag,
		const FString& TriggerSource);

	/**
	 * Remove every entry in the AbilityTriggers array whose `TriggerTag`
	 * matches `TriggerTag` exactly. Returns the number of entries removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static int32 RemoveAbilityTriggerByTag(
		const FString& AbilityBlueprintPath,
		const FString& TriggerTag);

	/** Clear every FAbilityTriggerData entry. Returns the number removed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static int32 ClearAbilityTriggers(const FString& AbilityBlueprintPath);

	// ─── GA graph node writes (M2) ─────────────────────────────

	/**
	 * Enumerate UAbilityTask subclasses discoverable via TObjectIterator<UClass>
	 * (native + already-loaded BP subclasses). Useful for agents to find e.g.
	 * `WaitGameplayEvent`, `PlayMontageAndWait`, `WaitTargetData` before
	 * spawning nodes.
	 * @param Filter       Case-insensitive substring on class name; "" matches all (requires MaxResults > 0).
	 * @param MaxResults   0 = unlimited (only when Filter is non-empty).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListAbilityTaskClasses(const FString& Filter, int32 MaxResults);

	/**
	 * List the static BlueprintCallable factory UFUNCTIONs on a UAbilityTask
	 * subclass — each one is a valid `factory_function_name` for
	 * `AddAbilityTaskNode`. A task may expose multiple factories (e.g.
	 * `UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy`
	 * vs `UAbilityTask_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent`).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static TArray<FString> ListAbilityTaskFactories(const FString& TaskClassPath);

	/**
	 * Spawn a `UK2Node_LatentAbilityCall` node wrapping a UAbilityTask
	 * factory function. Allocates default pins (factory input params on
	 * the left, output exec pins from multicast delegates on the right:
	 * OnSuccess / OnInterrupted / OnEnd / etc. depending on the task).
	 *
	 * The target graph must be a UGameplayAbility's ubergraph or macro
	 * (`UK2Node_LatentAbilityCall::IsCompatibleWithGraph` enforces this
	 * at compile-time via `ValidateNodeDuringCompilation`).
	 *
	 * Returns the new node's GUID (32 hex digits) or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FString AddAbilityTaskNode(
		const FString& AbilityBlueprintPath,
		const FString& GraphName,
		const FString& TaskClassPath,
		const FString& FactoryFunctionName,
		int32 NodePosX,
		int32 NodePosY);

	/**
	 * Spawn a `K2Node_CallFunction` targeting a BlueprintCallable function on
	 * `UGameplayAbility` itself — convenience for the common cases
	 * `CommitAbility`, `K2_EndAbility`, `K2_ApplyGameplayEffectToSelf`,
	 * `K2_ApplyGameplayEffectToTarget`, `BP_ApplyCooldown`,
	 * `MakeOutgoingGameplayEffectSpec`, etc. The function name is resolved
	 * against `UGameplayAbility::StaticClass()`.
	 *
	 * Returns the new node's GUID or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FString AddAbilityCallFunctionNode(
		const FString& AbilityBlueprintPath,
		const FString& GraphName,
		const FString& FunctionName,
		int32 NodePosX,
		int32 NodePosY);

	// ─── Create GA Blueprint (M3) ──────────────────────────────

	/**
	 * Create a new `UGameplayAbility` Blueprint asset and save it. This is
	 * the "scaffolding" entry point — pair with `set_ability_tag_container`,
	 * `set_ability_cost` / `set_ability_cooldown`, and the M2 graph-node
	 * UFUNCTIONs to build a complete ability from scratch in one bridge
	 * session.
	 *
	 * @param DestContentPath     Content-browser folder, e.g. "/Game/GA".
	 *                            Must start with "/Game" (or a mounted plugin path).
	 * @param AssetName           BP asset name without path or extension, e.g. "BP_Fireball".
	 *                            Must not already exist at DestContentPath.
	 * @param ParentClassPath     Native or BP path to the parent class. Empty =
	 *                            use `/Script/GameplayAbilities.GameplayAbility`.
	 *                            Must resolve to a UGameplayAbility-derived UClass.
	 *
	 * @return Full object path of the new BP ("/Game/GA/BP_Fireball.BP_Fireball"),
	 *         or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|GameplayAbility")
	static FString CreateGameplayAbilityBlueprint(
		const FString& DestContentPath,
		const FString& AssetName,
		const FString& ParentClassPath);
};

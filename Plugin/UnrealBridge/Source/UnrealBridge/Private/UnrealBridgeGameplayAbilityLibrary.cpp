#include "UnrealBridgeGameplayAbilityLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "UObject/UnrealType.h"

namespace BridgeGameplayAbilityImpl
{
	UClass* LoadGeneratedClassFromBlueprint(const FString& BlueprintPath)
	{
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		if (BP && BP->GeneratedClass)
		{
			return BP->GeneratedClass;
		}
		return nullptr;
	}

	/** Resolve a class path that may be a native class or a Blueprint asset path. */
	UClass* ResolveClassByPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}

		// Try native / loaded class first.
		if (UClass* C = FindObject<UClass>(nullptr, *Path))
		{
			return C;
		}

		// Try with _C suffix appended.
		if (!Path.EndsWith(TEXT("_C")))
		{
			const FString WithC = Path + TEXT("_C");
			if (UClass* C = FindObject<UClass>(nullptr, *WithC))
			{
				return C;
			}
		}

		// Try loading as Blueprint.
		if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path))
		{
			if (BP->GeneratedClass)
			{
				return BP->GeneratedClass;
			}
		}

		// Last resort: try as UClass via StaticLoadObject.
		if (UObject* Loaded = StaticLoadObject(UClass::StaticClass(), nullptr, *Path))
		{
			return Cast<UClass>(Loaded);
		}
		return nullptr;
	}

	void TagContainerToStrings(const FGameplayTagContainer& Tags, TArray<FString>& Out)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			Out.Add(Tag.ToString());
		}
	}

	/** Find editor-world actor by FName or label. */
	AActor* FindEditorActor(const FString& NameOrLabel)
	{
		if (!GEditor)
		{
			return nullptr;
		}
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A)
			{
				continue;
			}
			if (A->GetName() == NameOrLabel || A->GetActorNameOrLabel() == NameOrLabel)
			{
				return A;
			}
		}
		return nullptr;
	}

	/** Best-effort constant from an FScalableFloat (returns Value regardless of curve binding). */
	float GetScalableFloatConstant(const FScalableFloat& SF)
	{
		return SF.Value;
	}

	/**
	 * Extract magnitude from FGameplayEffectModifierMagnitude if it's a simple ScalableFloat.
	 * Returns true on success. Uses the engine's public GetStaticMagnitudeIfPossible helper.
	 */
	bool TryGetStaticMagnitude(const FGameplayEffectModifierMagnitude& M, float& OutValue)
	{
		return M.GetStaticMagnitudeIfPossible(1.f, OutValue);
	}

	/** Walk an object's properties and collect every gameplay tag referenced in struct fields. */
	void CollectTagsFromObject(UObject* Obj, TArray<FString>& Out)
	{
		if (!Obj)
		{
			return;
		}

		for (TFieldIterator<FStructProperty> It(Obj->GetClass()); It; ++It)
		{
			FStructProperty* SP = *It;
			if (!SP || !SP->Struct)
			{
				continue;
			}

			const FString PropertyName = SP->GetName();

			if (SP->Struct == TBaseStructure<FGameplayTagContainer>::Get())
			{
				const FGameplayTagContainer* TC =
					SP->ContainerPtrToValuePtr<FGameplayTagContainer>(Obj);
				if (TC)
				{
					for (const FGameplayTag& Tag : *TC)
					{
						Out.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *Tag.ToString()));
					}
				}
			}
			else if (SP->Struct == TBaseStructure<FGameplayTag>::Get())
			{
				const FGameplayTag* Tag = SP->ContainerPtrToValuePtr<FGameplayTag>(Obj);
				if (Tag && Tag->IsValid())
				{
					Out.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *Tag->ToString()));
				}
			}
			else if (SP->Struct && SP->Struct->GetFName() == FName(TEXT("InheritedTagContainer")))
			{
				// FInheritedTagContainer has a `CombinedTags` FGameplayTagContainer field — reflect into it.
				FStructProperty* CombinedProp =
					FindFProperty<FStructProperty>(SP->Struct, TEXT("CombinedTags"));
				if (CombinedProp && CombinedProp->Struct == TBaseStructure<FGameplayTagContainer>::Get())
				{
					const void* ITC = SP->ContainerPtrToValuePtr<void>(Obj);
					const FGameplayTagContainer* Combined =
						CombinedProp->ContainerPtrToValuePtr<FGameplayTagContainer>(ITC);
					if (Combined)
					{
						for (const FGameplayTag& Tag : *Combined)
						{
							Out.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *Tag.ToString()));
						}
					}
				}
			}
		}
	}

	/**
	 * Enumerate GEComponents on a UGameplayEffect via property reflection.
	 * Works regardless of the visibility of the GEComponents UPROPERTY across engine versions.
	 */
	TArray<UGameplayEffectComponent*> GetGameplayEffectComponents(UGameplayEffect* GE)
	{
		TArray<UGameplayEffectComponent*> Result;
		if (!GE)
		{
			return Result;
		}

		FArrayProperty* ArrayProp =
			FindFProperty<FArrayProperty>(UGameplayEffect::StaticClass(), TEXT("GEComponents"));
		if (!ArrayProp)
		{
			return Result;
		}

		FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner);
		if (!InnerObjProp)
		{
			return Result;
		}

		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(GE));
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			UObject* Obj = InnerObjProp->GetObjectPropertyValue(Helper.GetRawPtr(i));
			if (UGameplayEffectComponent* Comp = Cast<UGameplayEffectComponent>(Obj))
			{
				Result.Add(Comp);
			}
		}
		return Result;
	}

	/** Check whether any AssetTag / granted tag on a GE CDO matches the query tag (or its parents). */
	bool GameplayEffectHasTag(UGameplayEffect* GE, const FGameplayTag& QueryTag)
	{
		if (!GE || !QueryTag.IsValid())
		{
			return false;
		}

		TArray<FString> All;
		CollectTagsFromObject(GE, All);
		for (UGameplayEffectComponent* Comp : GetGameplayEffectComponents(GE))
		{
			CollectTagsFromObject(Comp, All);
		}

		const FString QueryStr = QueryTag.ToString();
		for (const FString& Entry : All)
		{
			int32 ColonIdx = INDEX_NONE;
			if (!Entry.FindChar(TEXT(':'), ColonIdx))
			{
				continue;
			}
			const FString TagStr = Entry.Mid(ColonIdx + 2).TrimStartAndEnd();
			FGameplayTag CandidateTag =
				FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			if (CandidateTag.IsValid() && CandidateTag.MatchesTag(QueryTag))
			{
				return true;
			}
		}
		return false;
	}

	/** Convert ModifierOp enum (int) to string — integer compare keeps this resilient across enum refactors. */
	FString ModOpToString(int32 OpValue)
	{
		switch (OpValue)
		{
		case 0: return TEXT("Additive");
		case 1: return TEXT("Multiplicitive");
		case 2: return TEXT("Division");
		case 3: return TEXT("Override");
		default: return TEXT("Unknown");
		}
	}

	FString DurationPolicyToString(EGameplayEffectDurationType Type)
	{
		switch (Type)
		{
		case EGameplayEffectDurationType::Instant:     return TEXT("Instant");
		case EGameplayEffectDurationType::HasDuration: return TEXT("HasDuration");
		case EGameplayEffectDurationType::Infinite:    return TEXT("Infinite");
		default:                                       return TEXT("");
		}
	}

	FString StackingTypeToString(EGameplayEffectStackingType Type)
	{
		switch (Type)
		{
		case EGameplayEffectStackingType::None:              return TEXT("None");
		case EGameplayEffectStackingType::AggregateBySource: return TEXT("AggregateBySource");
		case EGameplayEffectStackingType::AggregateByTarget: return TEXT("AggregateByTarget");
		default:                                             return TEXT("");
		}
	}
} // namespace BridgeGameplayAbilityImpl

// ─── GameplayAbility ─────────────────────────────────────────

FBridgeGameplayAbilityInfo UUnrealBridgeGameplayAbilityLibrary::GetGameplayAbilityBlueprintInfo(
	const FString& AbilityBlueprintPath)
{
	FBridgeGameplayAbilityInfo Result;

	UClass* AbilityClass = BridgeGameplayAbilityImpl::LoadGeneratedClassFromBlueprint(AbilityBlueprintPath);
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

	BridgeGameplayAbilityImpl::TagContainerToStrings(CDO->GetAssetTags(), Result.AbilityTags);

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

// ─── GameplayEffect ──────────────────────────────────────────

FBridgeGameplayEffectInfo UUnrealBridgeGameplayAbilityLibrary::GetGameplayEffectBlueprintInfo(
	const FString& EffectBlueprintPath)
{
	FBridgeGameplayEffectInfo Result;

	UClass* EffectClass = BridgeGameplayAbilityImpl::LoadGeneratedClassFromBlueprint(EffectBlueprintPath);
	if (!EffectClass || !EffectClass->IsChildOf(UGameplayEffect::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: '%s' is not a UGameplayEffect Blueprint"), *EffectBlueprintPath);
		return Result;
	}

	UGameplayEffect* CDO = EffectClass->GetDefaultObject<UGameplayEffect>();
	if (!CDO)
	{
		return Result;
	}

	Result.Name = EffectClass->GetName();
	if (UClass* Super = EffectClass->GetSuperClass())
	{
		Result.ParentClassName = Super->GetName();
	}

	Result.DurationPolicy = BridgeGameplayAbilityImpl::DurationPolicyToString(CDO->DurationPolicy);

	// Duration magnitude — best-effort constant.
	{
		float Mag = 0.f;
		if (BridgeGameplayAbilityImpl::TryGetStaticMagnitude(CDO->DurationMagnitude, Mag))
		{
			Result.DurationSeconds = Mag;
		}
	}

	// Period: ScalableFloat, always has Value field.
	Result.PeriodSeconds = BridgeGameplayAbilityImpl::GetScalableFloatConstant(CDO->Period);

	// Modifiers.
	for (const FGameplayModifierInfo& ModInfo : CDO->Modifiers)
	{
		FBridgeGEModifierInfo Out;
		Out.Attribute = ModInfo.Attribute.GetName();
		Out.ModOp = BridgeGameplayAbilityImpl::ModOpToString(static_cast<int32>(ModInfo.ModifierOp));

		float Mag = 0.f;
		if (BridgeGameplayAbilityImpl::TryGetStaticMagnitude(ModInfo.ModifierMagnitude, Mag))
		{
			Out.Magnitude = Mag;
			Out.MagnitudeSource = TEXT("ScalableFloat");
		}
		else
		{
			Out.MagnitudeSource = TEXT("Dynamic");
		}
		Result.Modifiers.Add(Out);
	}

	// Direct field access — GetStackingType() is declared but not DLL-exported in 5.7.
	// The engine warns the field will go private in a future release; re-evaluate on engine upgrade.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Result.StackingType = BridgeGameplayAbilityImpl::StackingTypeToString(CDO->StackingType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Result.StackLimitCount = CDO->StackLimitCount;

	// GEComponents in UE5.3+: tag requirements / granted tags live here.
	for (UGameplayEffectComponent* Comp :
		BridgeGameplayAbilityImpl::GetGameplayEffectComponents(CDO))
	{
		if (!Comp)
		{
			continue;
		}
		FBridgeGEComponentInfo CompInfo;
		CompInfo.ClassName = Comp->GetClass()->GetName();
		BridgeGameplayAbilityImpl::CollectTagsFromObject(Comp, CompInfo.Tags);
		Result.Components.Add(CompInfo);
	}

	return Result;
}

// ─── AttributeSet ────────────────────────────────────────────

FBridgeAttributeSetInfo UUnrealBridgeGameplayAbilityLibrary::GetAttributeSetInfo(
	const FString& AttributeSetClassPath)
{
	FBridgeAttributeSetInfo Result;

	UClass* AS = BridgeGameplayAbilityImpl::ResolveClassByPath(AttributeSetClassPath);
	if (!AS || !AS->IsChildOf(UAttributeSet::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: '%s' is not a UAttributeSet class"), *AttributeSetClassPath);
		return Result;
	}

	Result.Name = AS->GetName();
	if (UClass* Super = AS->GetSuperClass())
	{
		Result.ParentClassName = Super->GetName();
	}

	UAttributeSet* CDO = AS->GetDefaultObject<UAttributeSet>();
	if (!CDO)
	{
		return Result;
	}

	for (TFieldIterator<FStructProperty> It(AS); It; ++It)
	{
		FStructProperty* SP = *It;
		if (!SP || !SP->Struct)
		{
			continue;
		}
		if (SP->Struct != FGameplayAttributeData::StaticStruct())
		{
			continue;
		}

		FBridgeAttributeInfo Info;
		Info.Name = SP->GetName();
		Info.Type = TEXT("GameplayAttributeData");

		const FGameplayAttributeData* AD =
			SP->ContainerPtrToValuePtr<FGameplayAttributeData>(CDO);
		if (AD)
		{
			Info.BaseValue = AD->GetBaseValue();
		}
		Result.Attributes.Add(Info);
	}

	return Result;
}

// ─── List by tag (asset scans) ──────────────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListAbilitiesByTag(
	const FString& TagQuery, int32 MaxResults)
{
	TArray<FString> Results;

	const FGameplayTag Query =
		FGameplayTag::RequestGameplayTag(FName(*TagQuery), false);
	if (!Query.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: '%s' is not a registered gameplay tag"), *TagQuery);
		return Results;
	}

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AR.GetAssets(Filter, Assets);

	for (const FAssetData& AD : Assets)
	{
		UBlueprint* BP = Cast<UBlueprint>(AD.GetAsset());
		if (!BP || !BP->GeneratedClass)
		{
			continue;
		}
		if (!BP->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
		{
			continue;
		}
		UGameplayAbility* CDO = BP->GeneratedClass->GetDefaultObject<UGameplayAbility>();
		if (!CDO)
		{
			continue;
		}
		const FGameplayTagContainer AssetTags = CDO->GetAssetTags();
		bool bMatches = false;
		for (const FGameplayTag& T : AssetTags)
		{
			if (T.MatchesTag(Query))
			{
				bMatches = true;
				break;
			}
		}
		if (bMatches)
		{
			Results.Add(AD.GetSoftObjectPath().ToString());
			if (MaxResults > 0 && Results.Num() >= MaxResults)
			{
				break;
			}
		}
	}

	return Results;
}

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListGameplayEffectsByTag(
	const FString& TagQuery, int32 MaxResults)
{
	TArray<FString> Results;

	const FGameplayTag Query =
		FGameplayTag::RequestGameplayTag(FName(*TagQuery), false);
	if (!Query.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: '%s' is not a registered gameplay tag"), *TagQuery);
		return Results;
	}

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AR.GetAssets(Filter, Assets);

	for (const FAssetData& AD : Assets)
	{
		UBlueprint* BP = Cast<UBlueprint>(AD.GetAsset());
		if (!BP || !BP->GeneratedClass)
		{
			continue;
		}
		if (!BP->GeneratedClass->IsChildOf(UGameplayEffect::StaticClass()))
		{
			continue;
		}
		UGameplayEffect* CDO = BP->GeneratedClass->GetDefaultObject<UGameplayEffect>();
		if (!CDO)
		{
			continue;
		}
		if (BridgeGameplayAbilityImpl::GameplayEffectHasTag(CDO, Query))
		{
			Results.Add(AD.GetSoftObjectPath().ToString());
			if (MaxResults > 0 && Results.Num() >= MaxResults)
			{
				break;
			}
		}
	}
	return Results;
}

// ─── Actor ASC snapshot ─────────────────────────────────────

FBridgeActorAbilitySystemInfo UUnrealBridgeGameplayAbilityLibrary::GetActorAbilitySystemInfo(
	const FString& ActorName)
{
	FBridgeActorAbilitySystemInfo Result;
	Result.ActorName = ActorName;

	AActor* Actor = BridgeGameplayAbilityImpl::FindEditorActor(ActorName);
	if (!Actor)
	{
		return Result;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
	if (!ASC)
	{
		ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!ASC)
	{
		return Result;
	}

	Result.bFound = true;

	// Granted abilities.
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		FBridgeGrantedAbilityInfo Info;
		if (Spec.Ability)
		{
			Info.AbilityClassName = Spec.Ability->GetClass()->GetName();
		}
		Info.Level = Spec.Level;
		Info.InputID = Spec.InputID;
		Info.bIsActive = Spec.IsActive();
		Result.GrantedAbilities.Add(Info);
	}

	// Owned tags.
	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);
	BridgeGameplayAbilityImpl::TagContainerToStrings(OwnedTags, Result.OwnedTags);

	// Active effect count — empty query matches all active effects.
	{
		FGameplayEffectQuery EmptyQuery;
		Result.ActiveEffectCount = ASC->GetActiveEffects(EmptyQuery).Num();
	}

	// AttributeSet classes.
	for (const UAttributeSet* AS : ASC->GetSpawnedAttributes())
	{
		if (AS)
		{
			Result.AttributeSetClasses.Add(AS->GetClass()->GetName());
		}
	}

	return Result;
}

// ─── Tag dump ───────────────────────────────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListGameplayTags(
	const FString& Filter, int32 MaxResults)
{
	TArray<FString> Results;

	if (Filter.IsEmpty() && MaxResults <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListGameplayTags refuses empty filter with MaxResults=0 (would dump all tags)."));
		return Results;
	}

	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
	FGameplayTagContainer All;
	TagsManager.RequestAllGameplayTags(All, true);

	const bool bHasFilter = !Filter.IsEmpty();
	for (const FGameplayTag& Tag : All)
	{
		const FString S = Tag.ToString();
		if (bHasFilter && !S.Contains(Filter))
		{
			continue;
		}
		Results.Add(S);
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	return Results;
}

// ─── AttributeSet listing ───────────────────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListAttributeSets(
	const FString& Filter, int32 MaxResults)
{
	TArray<FString> Results;

	if (Filter.IsEmpty() && MaxResults <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListAttributeSets refuses empty filter with MaxResults=0."));
		return Results;
	}

	TArray<UClass*> Derived;
	GetDerivedClasses(UAttributeSet::StaticClass(), Derived, true);

	const bool bHasFilter = !Filter.IsEmpty();
	for (UClass* C : Derived)
	{
		if (!C || C->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		const FString Path = C->GetPathName();
		if (bHasFilter && !Path.Contains(Filter))
		{
			continue;
		}
		Results.Add(Path);
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	return Results;
}

// ─── Live attribute read ────────────────────────────────────

FBridgeAttributeValue UUnrealBridgeGameplayAbilityLibrary::GetAttributeValue(
	const FString& ActorName, const FString& AttributeName)
{
	FBridgeAttributeValue Result;
	Result.AttributeName = AttributeName;

	AActor* Actor = BridgeGameplayAbilityImpl::FindEditorActor(ActorName);
	if (!Actor)
	{
		return Result;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
	if (!ASC)
	{
		ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!ASC)
	{
		return Result;
	}

	// Accept "Set.Attribute" or bare "Attribute".
	FString BareName = AttributeName;
	int32 DotIdx;
	if (AttributeName.FindChar('.', DotIdx))
	{
		BareName = AttributeName.Mid(DotIdx + 1);
	}

	for (const UAttributeSet* AS : ASC->GetSpawnedAttributes())
	{
		if (!AS)
		{
			continue;
		}
		for (TFieldIterator<FStructProperty> It(AS->GetClass()); It; ++It)
		{
			FStructProperty* Prop = *It;
			if (!Prop || Prop->Struct != FGameplayAttributeData::StaticStruct())
			{
				continue;
			}
			if (Prop->GetName() != BareName)
			{
				continue;
			}
			FGameplayAttribute Attr(Prop);
			Result.bFound = true;
			Result.CurrentValue = ASC->GetNumericAttribute(Attr);
			Result.BaseValue = ASC->GetNumericAttributeBase(Attr);
			return Result;
		}
	}
	return Result;
}

// ─── Active effects ─────────────────────────────────────────

TArray<FBridgeActiveEffectInfo> UUnrealBridgeGameplayAbilityLibrary::GetActorActiveEffects(
	const FString& ActorName, int32 MaxResults)
{
	TArray<FBridgeActiveEffectInfo> Results;

	AActor* Actor = BridgeGameplayAbilityImpl::FindEditorActor(ActorName);
	if (!Actor)
	{
		return Results;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
	if (!ASC)
	{
		ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!ASC)
	{
		return Results;
	}

	const UWorld* World = Actor->GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	FGameplayEffectQuery EmptyQuery;
	TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(EmptyQuery);

	for (const FActiveGameplayEffectHandle& H : Handles)
	{
		const FActiveGameplayEffect* AE = ASC->GetActiveGameplayEffect(H);
		if (!AE)
		{
			continue;
		}

		FBridgeActiveEffectInfo Info;
		if (AE->Spec.Def)
		{
			Info.EffectClassName = AE->Spec.Def->GetClass()->GetName();
		}

		const float Duration = AE->GetDuration();
		Info.Duration = Duration;
		// UE uses 0 for infinite on FActiveGameplayEffect::GetDuration(); -1 is clearer for callers.
		Info.TimeRemaining = (Duration <= 0.f) ? -1.f : FMath::Max(0.f, AE->GetEndTime() - Now);
		Info.PeriodSeconds = AE->Spec.GetPeriod();
		Info.StackCount = AE->Spec.GetStackCount();

		for (const FGameplayTag& T : AE->Spec.DynamicGrantedTags)
		{
			Info.DynamicGrantedTags.Add(T.ToString());
		}

		Results.Add(Info);
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	return Results;
}

// ─── Tag parents ────────────────────────────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::GetTagParents(const FString& TagString)
{
	TArray<FString> Results;

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!Tag.IsValid())
	{
		return Results;
	}

	const FGameplayTagContainer Parents =
		UGameplayTagsManager::Get().RequestGameplayTagParents(Tag);

	// Parents container includes the tag itself — filter it, emit root-first.
	const FString Self = Tag.ToString();
	TArray<FString> Tmp;
	for (const FGameplayTag& P : Parents)
	{
		const FString S = P.ToString();
		if (S != Self)
		{
			Tmp.Add(S);
		}
	}
	Tmp.Sort([](const FString& A, const FString& B)
	{
		return A.Len() < B.Len();
	});
	Results = MoveTemp(Tmp);
	return Results;
}

// ─── Actor tag query ────────────────────────────────────────

bool UUnrealBridgeGameplayAbilityLibrary::ActorHasGameplayTag(
	const FString& ActorName,
	const FString& TagString,
	bool bExactMatch,
	int32& OutTagCount)
{
	OutTagCount = 0;

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!Tag.IsValid())
	{
		return false;
	}

	AActor* Actor = BridgeGameplayAbilityImpl::FindEditorActor(ActorName);
	if (!Actor)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
	if (!ASC)
	{
		ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!ASC)
	{
		return false;
	}

	OutTagCount = ASC->GetTagCount(Tag);
	if (bExactMatch)
	{
		return OutTagCount > 0;
	}
	return ASC->HasMatchingGameplayTag(Tag);
}

// ─── Ability cooldown query ─────────────────────────────────

FBridgeAbilityCooldownInfo UUnrealBridgeGameplayAbilityLibrary::GetAbilityCooldownInfo(
	const FString& ActorName,
	const FString& AbilityBlueprintPath)
{
	FBridgeAbilityCooldownInfo Result;

	UClass* AbilityClass =
		BridgeGameplayAbilityImpl::LoadGeneratedClassFromBlueprint(AbilityBlueprintPath);
	if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
	{
		return Result;
	}
	Result.AbilityClassName = AbilityClass->GetName();

	// Populate cooldown tags from the CDO regardless of ASC presence — useful metadata.
	if (UGameplayAbility* CDO = AbilityClass->GetDefaultObject<UGameplayAbility>())
	{
		if (const FGameplayTagContainer* CT = CDO->GetCooldownTags())
		{
			BridgeGameplayAbilityImpl::TagContainerToStrings(*CT, Result.CooldownTags);
		}
	}

	AActor* Actor = BridgeGameplayAbilityImpl::FindEditorActor(ActorName);
	if (!Actor)
	{
		return Result;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
	if (!ASC)
	{
		ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!ASC)
	{
		return Result;
	}

	FGameplayAbilitySpec* Spec =
		ASC->FindAbilitySpecFromClass(TSubclassOf<UGameplayAbility>(AbilityClass));
	if (!Spec || !Spec->Ability)
	{
		return Result;
	}
	Result.bFound = true;

	float TimeRemaining = 0.f;
	float CooldownDuration = 0.f;
	Spec->Ability->GetCooldownTimeRemainingAndDuration(
		Spec->Handle, ASC->AbilityActorInfo.Get(), TimeRemaining, CooldownDuration);

	Result.TimeRemaining = TimeRemaining;
	Result.CooldownDuration = CooldownDuration;
	Result.bOnCooldown = TimeRemaining > 0.f;
	return Result;
}

// ─── Filter active effects by tag ───────────────────────────

TArray<FBridgeActiveEffectInfo> UUnrealBridgeGameplayAbilityLibrary::FindActiveEffectsByTag(
	const FString& ActorName,
	const FString& TagQuery,
	int32 MaxResults)
{
	TArray<FBridgeActiveEffectInfo> Results;

	const FGameplayTag Query = FGameplayTag::RequestGameplayTag(FName(*TagQuery), false);
	if (!Query.IsValid())
	{
		return Results;
	}

	AActor* Actor = BridgeGameplayAbilityImpl::FindEditorActor(ActorName);
	if (!Actor)
	{
		return Results;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
	if (!ASC)
	{
		ASC = Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!ASC)
	{
		return Results;
	}

	const UWorld* World = Actor->GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	FGameplayEffectQuery EmptyQuery;
	TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(EmptyQuery);

	for (const FActiveGameplayEffectHandle& H : Handles)
	{
		const FActiveGameplayEffect* AE = ASC->GetActiveGameplayEffect(H);
		if (!AE)
		{
			continue;
		}

		FGameplayTagContainer AssetTags;
		AE->Spec.GetAllAssetTags(AssetTags);
		FGameplayTagContainer GrantedTags;
		AE->Spec.GetAllGrantedTags(GrantedTags);

		const bool bMatches =
			AssetTags.HasTag(Query) ||
			GrantedTags.HasTag(Query) ||
			AE->Spec.DynamicGrantedTags.HasTag(Query);
		if (!bMatches)
		{
			continue;
		}

		FBridgeActiveEffectInfo Info;
		if (AE->Spec.Def)
		{
			Info.EffectClassName = AE->Spec.Def->GetClass()->GetName();
		}
		const float Duration = AE->GetDuration();
		Info.Duration = Duration;
		Info.TimeRemaining = (Duration <= 0.f) ? -1.f : FMath::Max(0.f, AE->GetEndTime() - Now);
		Info.PeriodSeconds = AE->Spec.GetPeriod();
		Info.StackCount = AE->Spec.GetStackCount();
		for (const FGameplayTag& T : AE->Spec.DynamicGrantedTags)
		{
			Info.DynamicGrantedTags.Add(T.ToString());
		}
		Results.Add(Info);
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	return Results;
}

// ─── List ability Blueprints ────────────────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListAbilityBlueprints(
	const FString& Filter, int32 MaxResults)
{
	TArray<FString> Results;

	if (Filter.IsEmpty() && MaxResults <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListAbilityBlueprints refuses empty filter with MaxResults=0."));
		return Results;
	}

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();

	FARFilter ARFilter;
	ARFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	ARFilter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AR.GetAssets(ARFilter, Assets);

	const bool bHasFilter = !Filter.IsEmpty();
	for (const FAssetData& AD : Assets)
	{
		UBlueprint* BP = Cast<UBlueprint>(AD.GetAsset());
		if (!BP || !BP->GeneratedClass)
		{
			continue;
		}
		if (!BP->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
		{
			continue;
		}
		const FString Path = AD.GetSoftObjectPath().ToString();
		if (bHasFilter && !Path.Contains(Filter))
		{
			continue;
		}
		Results.Add(Path);
		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}
	return Results;
}

// ─── Tag hierarchy browse ───────────────────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::FindChildTags(
	const FString& ParentTag, bool bRecursive)
{
	TArray<FString> Results;

	const FGameplayTag Parent =
		FGameplayTag::RequestGameplayTag(FName(*ParentTag), false);
	if (!Parent.IsValid())
	{
		return Results;
	}

	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
	const FGameplayTagContainer Children = TagsManager.RequestGameplayTagChildren(Parent);

	const FString ParentStr = Parent.ToString();
	int32 ParentDots = 0;
	for (TCHAR C : ParentStr) { if (C == '.') ++ParentDots; }

	for (const FGameplayTag& Child : Children)
	{
		const FString S = Child.ToString();
		if (!bRecursive)
		{
			int32 ChildDots = 0;
			for (TCHAR C : S) { if (C == '.') ++ChildDots; }
			if (ChildDots != ParentDots + 1)
			{
				continue;
			}
		}
		Results.Add(S);
	}
	return Results;
}

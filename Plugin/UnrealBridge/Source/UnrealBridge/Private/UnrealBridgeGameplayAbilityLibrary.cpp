#include "UnrealBridgeGameplayAbilityLibrary.h"
#include "UnrealBridgeTestAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystemComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "HAL/PlatformTime.h"
#include "FileHelpers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_LatentAbilityCall.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
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
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
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

// ─── List GE / AttributeSet Blueprints ──────────────────────

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListGameplayEffectBlueprints(
	const FString& Filter, int32 MaxResults)
{
	TArray<FString> Results;

	if (Filter.IsEmpty() && MaxResults <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListGameplayEffectBlueprints refuses empty filter with MaxResults=0."));
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
		if (!BP->GeneratedClass->IsChildOf(UGameplayEffect::StaticClass()))
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

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListAttributeSetBlueprints(
	const FString& Filter, int32 MaxResults)
{
	TArray<FString> Results;

	if (Filter.IsEmpty() && MaxResults <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListAttributeSetBlueprints refuses empty filter with MaxResults=0."));
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
		if (!BP->GeneratedClass->IsChildOf(UAttributeSet::StaticClass()))
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

// ─── Tag validation / matching ──────────────────────────────

bool UUnrealBridgeGameplayAbilityLibrary::IsValidGameplayTag(const FString& TagString)
{
	if (TagString.IsEmpty())
	{
		return false;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	return Tag.IsValid();
}

bool UUnrealBridgeGameplayAbilityLibrary::TagMatches(
	const FString& TagA, const FString& TagB, bool bExactMatch)
{
	const FGameplayTag A = FGameplayTag::RequestGameplayTag(FName(*TagA), false);
	const FGameplayTag B = FGameplayTag::RequestGameplayTag(FName(*TagB), false);
	if (!A.IsValid() || !B.IsValid())
	{
		return false;
	}
	if (bExactMatch)
	{
		return A == B;
	}
	// "A matches B" meaning B is a descendant of A (or equal).
	return B.MatchesTag(A);
}

// ─── Batch live attribute read ──────────────────────────────

TArray<FBridgeAttributeValue> UUnrealBridgeGameplayAbilityLibrary::GetActorAttributes(
	const FString& ActorName)
{
	TArray<FBridgeAttributeValue> Results;

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

	for (const UAttributeSet* AS : ASC->GetSpawnedAttributes())
	{
		if (!AS)
		{
			continue;
		}
		const FString SetName = AS->GetClass()->GetName();
		for (TFieldIterator<FStructProperty> It(AS->GetClass()); It; ++It)
		{
			FStructProperty* Prop = *It;
			if (!Prop || Prop->Struct != FGameplayAttributeData::StaticStruct())
			{
				continue;
			}
			FGameplayAttribute Attr(Prop);
			FBridgeAttributeValue V;
			V.AttributeName = FString::Printf(TEXT("%s.%s"), *SetName, *Prop->GetName());
			V.bFound = true;
			V.CurrentValue = ASC->GetNumericAttribute(Attr);
			V.BaseValue = ASC->GetNumericAttributeBase(Attr);
			Results.Add(V);
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

// ─── Ability tag requirements / triggers ────────────────────

namespace BridgeGameplayAbilityImpl
{
	/** Read an FGameplayTagContainer UPROPERTY by name from an object (works for protected/private). */
	void ReadTagContainerProperty(UObject* Obj, const TCHAR* PropName, TArray<FString>& Out)
	{
		if (!Obj)
		{
			return;
		}
		FStructProperty* SP = FindFProperty<FStructProperty>(Obj->GetClass(), PropName);
		if (!SP || SP->Struct != TBaseStructure<FGameplayTagContainer>::Get())
		{
			return;
		}
		const FGameplayTagContainer* TC = SP->ContainerPtrToValuePtr<FGameplayTagContainer>(Obj);
		if (TC)
		{
			TagContainerToStrings(*TC, Out);
		}
	}

	FString TriggerSourceToString(int32 Value)
	{
		switch (Value)
		{
		case 0: return TEXT("GameplayEvent");
		case 1: return TEXT("OwnedTagAdded");
		case 2: return TEXT("OwnedTagPresent");
		default: return TEXT("Unknown");
		}
	}
} // namespace BridgeGameplayAbilityImpl

FBridgeAbilityTagRequirements UUnrealBridgeGameplayAbilityLibrary::GetAbilityTagRequirements(
	const FString& AbilityBlueprintPath)
{
	FBridgeAbilityTagRequirements Result;

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

	Result.AbilityClassName = AbilityClass->GetName();

	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("CancelAbilitiesWithTag"),  Result.CancelAbilitiesWithTag);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("BlockAbilitiesWithTag"),   Result.BlockAbilitiesWithTag);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("ActivationOwnedTags"),     Result.ActivationOwnedTags);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("ActivationRequiredTags"),  Result.ActivationRequiredTags);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("ActivationBlockedTags"),   Result.ActivationBlockedTags);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("SourceRequiredTags"),      Result.SourceRequiredTags);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("SourceBlockedTags"),       Result.SourceBlockedTags);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("TargetRequiredTags"),      Result.TargetRequiredTags);
	BridgeGameplayAbilityImpl::ReadTagContainerProperty(CDO, TEXT("TargetBlockedTags"),       Result.TargetBlockedTags);

	return Result;
}

TArray<FBridgeAbilityTriggerInfo> UUnrealBridgeGameplayAbilityLibrary::GetAbilityTriggers(
	const FString& AbilityBlueprintPath)
{
	TArray<FBridgeAbilityTriggerInfo> Results;

	UClass* AbilityClass = BridgeGameplayAbilityImpl::LoadGeneratedClassFromBlueprint(AbilityBlueprintPath);
	if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
	{
		return Results;
	}
	UGameplayAbility* CDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
	if (!CDO)
	{
		return Results;
	}

	FArrayProperty* ArrayProp =
		FindFProperty<FArrayProperty>(UGameplayAbility::StaticClass(), TEXT("AbilityTriggers"));
	if (!ArrayProp)
	{
		return Results;
	}
	FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
	if (!InnerStruct || !InnerStruct->Struct)
	{
		return Results;
	}

	FStructProperty* TagProp = FindFProperty<FStructProperty>(InnerStruct->Struct, TEXT("TriggerTag"));
	FProperty* SourceProp = InnerStruct->Struct->FindPropertyByName(TEXT("TriggerSource"));
	FByteProperty* SourceByteProp = CastField<FByteProperty>(SourceProp);
	FEnumProperty* SourceEnumProp = CastField<FEnumProperty>(SourceProp);

	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CDO));
	for (int32 i = 0; i < Helper.Num(); ++i)
	{
		const void* ElemPtr = Helper.GetRawPtr(i);

		FBridgeAbilityTriggerInfo Out;

		if (TagProp && TagProp->Struct == TBaseStructure<FGameplayTag>::Get())
		{
			const FGameplayTag* Tag = TagProp->ContainerPtrToValuePtr<FGameplayTag>(ElemPtr);
			if (Tag && Tag->IsValid())
			{
				Out.TriggerTag = Tag->ToString();
			}
		}

		int32 SourceVal = 0;
		if (SourceByteProp)
		{
			SourceVal = static_cast<int32>(SourceByteProp->GetPropertyValue_InContainer(ElemPtr));
		}
		else if (SourceEnumProp)
		{
			if (FNumericProperty* Underlying = SourceEnumProp->GetUnderlyingProperty())
			{
				SourceVal = static_cast<int32>(Underlying->GetSignedIntPropertyValue(
					SourceEnumProp->ContainerPtrToValuePtr<void>(ElemPtr)));
			}
		}
		Out.TriggerSource = BridgeGameplayAbilityImpl::TriggerSourceToString(SourceVal);

		Results.Add(Out);
	}
	return Results;
}

// ─── Actor ASC tag / activation queries ─────────────────────

namespace BridgeGameplayAbilityImpl
{
	UAbilitySystemComponent* ResolveActorASC(const FString& ActorName, AActor*& OutActor)
	{
		OutActor = FindEditorActor(ActorName);
		if (!OutActor)
		{
			return nullptr;
		}
		if (IAbilitySystemInterface* Iface = Cast<IAbilitySystemInterface>(OutActor))
		{
			if (UAbilitySystemComponent* ASC = Iface->GetAbilitySystemComponent())
			{
				return ASC;
			}
		}
		return OutActor->FindComponentByClass<UAbilitySystemComponent>();
	}
} // namespace BridgeGameplayAbilityImpl

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::GetActorBlockedAbilityTags(const FString& ActorName)
{
	TArray<FString> Results;
	AActor* Actor = nullptr;
	UAbilitySystemComponent* ASC = BridgeGameplayAbilityImpl::ResolveActorASC(ActorName, Actor);
	if (!ASC)
	{
		return Results;
	}
	FGameplayTagContainer Blocked;
	ASC->GetBlockedAbilityTags(Blocked);
	BridgeGameplayAbilityImpl::TagContainerToStrings(Blocked, Results);
	return Results;
}

bool UUnrealBridgeGameplayAbilityLibrary::ActorAbilityMeetsTagRequirements(
	const FString& ActorName,
	const FString& AbilityBlueprintPath,
	TArray<FString>& OutBlockingTags)
{
	OutBlockingTags.Reset();

	AActor* Actor = nullptr;
	UAbilitySystemComponent* ASC = BridgeGameplayAbilityImpl::ResolveActorASC(ActorName, Actor);
	if (!ASC)
	{
		return false;
	}

	UClass* AbilityClass = BridgeGameplayAbilityImpl::LoadGeneratedClassFromBlueprint(AbilityBlueprintPath);
	if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
	{
		return false;
	}
	UGameplayAbility* CDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
	if (!CDO)
	{
		return false;
	}

	FGameplayTagContainer Relevant;
	const bool bOk = CDO->DoesAbilitySatisfyTagRequirements(*ASC, nullptr, nullptr, &Relevant);
	BridgeGameplayAbilityImpl::TagContainerToStrings(Relevant, OutBlockingTags);
	return bOk;
}

TArray<FBridgeActiveAbilityInfo> UUnrealBridgeGameplayAbilityLibrary::GetActorActiveAbilities(
	const FString& ActorName)
{
	TArray<FBridgeActiveAbilityInfo> Results;
	AActor* Actor = nullptr;
	UAbilitySystemComponent* ASC = BridgeGameplayAbilityImpl::ResolveActorASC(ActorName, Actor);
	if (!ASC)
	{
		return Results;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		FBridgeActiveAbilityInfo Info;
		if (Spec.Ability)
		{
			Info.AbilityClassName = Spec.Ability->GetClass()->GetName();
		}
		Info.Level = Spec.Level;
		Info.InputID = Spec.InputID;
		Info.ActiveCount = Spec.ActiveCount;
		Results.Add(Info);
	}
	return Results;
}

// ─── Send GameplayEvent by actor name ────────────────────────

int32 UUnrealBridgeGameplayAbilityLibrary::SendGameplayEventByName(
	const FString& ActorName,
	const FString& EventTag,
	float EventMagnitude)
{
	if (!GEditor)
	{
		return -1;
	}

	auto SearchWorld = [&ActorName](UWorld* World) -> AActor*
	{
		if (!World) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A) continue;
			if (A->GetName() == ActorName || A->GetActorNameOrLabel() == ActorName)
			{
				return A;
			}
		}
		return nullptr;
	};

	AActor* Actor = nullptr;
	for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
	{
		if (Ctx.WorldType == EWorldType::PIE)
		{
			Actor = SearchWorld(Ctx.World());
			if (Actor) break;
		}
	}
	if (!Actor)
	{
		Actor = SearchWorld(GEditor->GetEditorWorldContext().World());
	}
	if (!Actor)
	{
		return -1;
	}

	// Walk standard GAS placement patterns: actor → pawn's PlayerState → pawn's
	// Controller (and its PlayerState). Mirrors UnrealBridgeReactiveLibrary's
	// ResolveActorASC; this project keeps the ASC on PlayerState.
	auto FromObject = [](UObject* Obj) -> UAbilitySystemComponent*
	{
		if (!Obj) return nullptr;
		if (IAbilitySystemInterface* I = Cast<IAbilitySystemInterface>(Obj))
		{
			if (UAbilitySystemComponent* ASC = I->GetAbilitySystemComponent()) return ASC;
		}
		if (AActor* A = Cast<AActor>(Obj))
		{
			return A->FindComponentByClass<UAbilitySystemComponent>();
		}
		return nullptr;
	};

	UAbilitySystemComponent* ASC = FromObject(Actor);
	if (!ASC)
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (APlayerState* PS = Pawn->GetPlayerState())
			{
				ASC = FromObject(PS);
			}
			if (!ASC)
			{
				if (AController* Ctrl = Pawn->GetController())
				{
					ASC = FromObject(Ctrl);
					if (!ASC)
					{
						if (APlayerController* PC = Cast<APlayerController>(Ctrl))
						{
							ASC = FromObject(PC->PlayerState);
						}
					}
				}
			}
		}
	}
	if (!ASC)
	{
		return -1;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*EventTag), false);
	if (!Tag.IsValid())
	{
		return -1;
	}

	FGameplayEventData EventData;
	EventData.EventTag = Tag;
	EventData.Instigator = Actor;
	EventData.Target = Actor;
	EventData.EventMagnitude = EventMagnitude;

	return ASC->HandleGameplayEvent(Tag, &EventData);
}

// ─── Ensure ASC on actor (test scaffolding) ─────────────────

namespace BridgeGameplayAbilityImpl
{
	/** Same PIE-first-then-editor lookup used by SendGameplayEventByName. */
	AActor* FindActorAnyWorld(const FString& ActorName)
	{
		if (!GEditor) return nullptr;
		auto SearchWorld = [&ActorName](UWorld* World) -> AActor*
		{
			if (!World) return nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* A = *It;
				if (!A) continue;
				if (A->GetName() == ActorName || A->GetActorNameOrLabel() == ActorName)
				{
					return A;
				}
			}
			return nullptr;
		};
		for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::PIE)
			{
				if (AActor* A = SearchWorld(Ctx.World())) return A;
			}
		}
		return SearchWorld(GEditor->GetEditorWorldContext().World());
	}

	/** Walk Pawn→PlayerState→Controller→PC.PlayerState looking for an ASC. Mirrors the reactive library walker. */
	UAbilitySystemComponent* WalkForASC(AActor* Actor)
	{
		if (!Actor) return nullptr;
		auto FromObject = [](UObject* Obj) -> UAbilitySystemComponent*
		{
			if (!Obj) return nullptr;
			if (IAbilitySystemInterface* I = Cast<IAbilitySystemInterface>(Obj))
			{
				if (UAbilitySystemComponent* ASC = I->GetAbilitySystemComponent()) return ASC;
			}
			if (AActor* A = Cast<AActor>(Obj))
			{
				return A->FindComponentByClass<UAbilitySystemComponent>();
			}
			return nullptr;
		};
		if (UAbilitySystemComponent* ASC = FromObject(Actor)) return ASC;
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (APlayerState* PS = Pawn->GetPlayerState())
			{
				if (UAbilitySystemComponent* ASC = FromObject(PS)) return ASC;
			}
			if (AController* Ctrl = Pawn->GetController())
			{
				if (UAbilitySystemComponent* ASC = FromObject(Ctrl)) return ASC;
				if (APlayerController* PC = Cast<APlayerController>(Ctrl))
				{
					if (UAbilitySystemComponent* ASC = FromObject(PC->PlayerState)) return ASC;
				}
			}
		}
		return nullptr;
	}
}

bool UUnrealBridgeGameplayAbilityLibrary::EnsureAbilitySystemComponent(const FString& ActorName, const FString& Location)
{
	AActor* Actor = BridgeGameplayAbilityImpl::FindActorAnyWorld(ActorName);
	if (!Actor) return false;

	// Resolve the UObject that should host the ASC.
	UObject* Host = nullptr;
	if (Location.Equals(TEXT("Controller"), ESearchCase::IgnoreCase))
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || !Pawn->GetController())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("EnsureAbilitySystemComponent: Location=Controller but actor '%s' has no Controller"),
				*ActorName);
			return false;
		}
		Host = Pawn->GetController();
	}
	else if (Location.Equals(TEXT("PlayerState"), ESearchCase::IgnoreCase))
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || !Pawn->GetPlayerState())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("EnsureAbilitySystemComponent: Location=PlayerState but actor '%s' has no PlayerState"),
				*ActorName);
			return false;
		}
		Host = Pawn->GetPlayerState();
	}
	else
	{
		Host = Actor;
	}

	AActor* HostActor = Cast<AActor>(Host);
	if (!HostActor) return false;

	// No-op if this host already has one.
	if (HostActor->FindComponentByClass<UAbilitySystemComponent>() != nullptr)
	{
		return true;
	}

	UAbilitySystemComponent* NewASC = NewObject<UAbilitySystemComponent>(HostActor, TEXT("BridgeTestASC"));
	NewASC->RegisterComponent();
	// For Controller/PlayerState placements the ASC conventionally takes the
	// owning actor = the host, avatar = the controlled pawn.
	AActor* Avatar = (HostActor == Actor) ? Actor : Actor;
	NewASC->InitAbilityActorInfo(HostActor, Avatar);
	return true;
}

bool UUnrealBridgeGameplayAbilityLibrary::EnsureBridgeTestAttributeSet(const FString& ActorName)
{
	AActor* Actor = BridgeGameplayAbilityImpl::FindActorAnyWorld(ActorName);
	if (!Actor) return false;

	UAbilitySystemComponent* ASC = BridgeGameplayAbilityImpl::WalkForASC(Actor);
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("EnsureBridgeTestAttributeSet: no ASC reachable on '%s' (call EnsureAbilitySystemComponent first)"),
			*ActorName);
		return false;
	}

	// Already spawned?
	for (const UAttributeSet* AS : ASC->GetSpawnedAttributes())
	{
		if (AS && AS->IsA(UBridgeTestAttributeSet::StaticClass()))
		{
			return true;
		}
	}

	UBridgeTestAttributeSet* NewAS = NewObject<UBridgeTestAttributeSet>(ASC->GetOwner());
	ASC->AddSpawnedAttribute(NewAS);
	// Seed base values so the first modification produces a clean delta.
	if (FStructProperty* HealthProp = FindFProperty<FStructProperty>(
		UBridgeTestAttributeSet::StaticClass(), TEXT("Health")))
	{
		ASC->SetNumericAttributeBase(FGameplayAttribute(HealthProp), 100.f);
	}
	if (FStructProperty* ManaProp = FindFProperty<FStructProperty>(
		UBridgeTestAttributeSet::StaticClass(), TEXT("Mana")))
	{
		ASC->SetNumericAttributeBase(FGameplayAttribute(ManaProp), 100.f);
	}
	return true;
}

bool UUnrealBridgeGameplayAbilityLibrary::SetActorAttributeValue(const FString& ActorName, const FString& AttributeName, float Value)
{
	AActor* Actor = BridgeGameplayAbilityImpl::FindActorAnyWorld(ActorName);
	if (!Actor) return false;

	UAbilitySystemComponent* ASC = BridgeGameplayAbilityImpl::WalkForASC(Actor);
	if (!ASC) return false;

	FString SetName, BareName;
	if (!AttributeName.Split(TEXT("."), &SetName, &BareName))
	{
		BareName = AttributeName;
	}

	for (const UAttributeSet* AS : ASC->GetSpawnedAttributes())
	{
		if (!AS) continue;
		if (!SetName.IsEmpty() && AS->GetClass()->GetName() != SetName) continue;
		for (TFieldIterator<FStructProperty> It(AS->GetClass()); It; ++It)
		{
			FStructProperty* P = *It;
			if (!P || P->Struct != FGameplayAttributeData::StaticStruct()) continue;
			if (P->GetName() == BareName)
			{
				ASC->SetNumericAttributeBase(FGameplayAttribute(P), Value);
				return true;
			}
		}
	}
	return false;
}

// ─── GA Blueprint CDO writes (M1) ───────────────────────────

namespace BridgeGAWriteImpl
{
	/** Valid FGameplayTagContainer UPROPERTY names on UGameplayAbility. */
	static const TSet<FString>& KnownTagContainers()
	{
		static const TSet<FString> S = {
			TEXT("AbilityTags"),
			TEXT("CancelAbilitiesWithTag"),
			TEXT("BlockAbilitiesWithTag"),
			TEXT("ActivationOwnedTags"),
			TEXT("ActivationRequiredTags"),
			TEXT("ActivationBlockedTags"),
			TEXT("SourceRequiredTags"),
			TEXT("SourceBlockedTags"),
			TEXT("TargetRequiredTags"),
			TEXT("TargetBlockedTags"),
		};
		return S;
	}

	/** Load the UBlueprint + verify it's a UGameplayAbility. */
	static UBlueprint* LoadAbilityBP(const FString& Path, UClass** OutGenClass = nullptr)
	{
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path);
		if (!BP || !BP->GeneratedClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: could not load ability BP '%s'"), *Path);
			return nullptr;
		}
		if (!BP->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: '%s' is not a UGameplayAbility subclass"), *Path);
			return nullptr;
		}
		if (OutGenClass) *OutGenClass = BP->GeneratedClass;
		return BP;
	}

	/** After mutating CDO: mark BP modified, compile to regenerate, mark dirty for save. */
	static void FinalizeBlueprintCDOChange(UBlueprint* BP)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		// Compile so the CDO change is baked into any subsequent package save
		// and so editors listening for BP-compiled events see the new state.
		FKismetEditorUtilities::CompileBlueprint(BP);
		BP->MarkPackageDirty();
	}

	/** Convert a tag-container field name to the FProperty on UGameplayAbility. */
	static FStructProperty* FindTagContainerProperty(UClass* GenClass, const FString& ContainerName)
	{
		FProperty* Prop = GenClass->FindPropertyByName(FName(*ContainerName));
		if (!Prop) return nullptr;
		FStructProperty* Struct = CastField<FStructProperty>(Prop);
		if (!Struct || Struct->Struct != TBaseStructure<FGameplayTagContainer>::Get())
		{
			return nullptr;
		}
		return Struct;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	static EGameplayAbilityInstancingPolicy::Type ParseInstancingPolicy(const FString& S, bool& bOk)
	{
		bOk = true;
		if (S.Equals(TEXT("InstancedPerActor"), ESearchCase::IgnoreCase))
			return EGameplayAbilityInstancingPolicy::InstancedPerActor;
		if (S.Equals(TEXT("InstancedPerExecution"), ESearchCase::IgnoreCase))
			return EGameplayAbilityInstancingPolicy::InstancedPerExecution;
		if (S.Equals(TEXT("NonInstanced"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: NonInstanced is deprecated since UE 5.5 — prefer InstancedPerActor."));
			// Deprecated in UE 5.5; kept for parity with the existing
			// GetGameplayAbilityBlueprintInfo read side.
			return EGameplayAbilityInstancingPolicy::NonInstanced;
		}
		bOk = false;
		return EGameplayAbilityInstancingPolicy::InstancedPerActor;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static EGameplayAbilityNetExecutionPolicy::Type ParseNetExecutionPolicy(const FString& S, bool& bOk)
	{
		bOk = true;
		if (S.Equals(TEXT("LocalPredicted"), ESearchCase::IgnoreCase))
			return EGameplayAbilityNetExecutionPolicy::LocalPredicted;
		if (S.Equals(TEXT("LocalOnly"), ESearchCase::IgnoreCase))
			return EGameplayAbilityNetExecutionPolicy::LocalOnly;
		if (S.Equals(TEXT("ServerInitiated"), ESearchCase::IgnoreCase))
			return EGameplayAbilityNetExecutionPolicy::ServerInitiated;
		if (S.Equals(TEXT("ServerOnly"), ESearchCase::IgnoreCase))
			return EGameplayAbilityNetExecutionPolicy::ServerOnly;
		bOk = false;
		return EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	}

	static EGameplayAbilityTriggerSource::Type ParseTriggerSource(const FString& S, bool& bOk)
	{
		bOk = true;
		if (S.Equals(TEXT("GameplayEvent"), ESearchCase::IgnoreCase))
			return EGameplayAbilityTriggerSource::GameplayEvent;
		if (S.Equals(TEXT("OwnedTagAdded"), ESearchCase::IgnoreCase))
			return EGameplayAbilityTriggerSource::OwnedTagAdded;
		if (S.Equals(TEXT("OwnedTagPresent"), ESearchCase::IgnoreCase))
			return EGameplayAbilityTriggerSource::OwnedTagPresent;
		bOk = false;
		return EGameplayAbilityTriggerSource::GameplayEvent;
	}
}

int32 UUnrealBridgeGameplayAbilityLibrary::SetAbilityTagContainer(
	const FString& AbilityBlueprintPath,
	const FString& ContainerName,
	const TArray<FString>& Tags)
{
	using namespace BridgeGAWriteImpl;

	if (!KnownTagContainers().Contains(ContainerName))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: SetAbilityTagContainer unknown container '%s'"), *ContainerName);
		return -1;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return -1;

	FStructProperty* Prop = FindTagContainerProperty(GenClass, ContainerName);
	if (!Prop)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: container '%s' not found on '%s'"), *ContainerName, *GenClass->GetName());
		return -1;
	}

	UObject* CDO = GenClass->GetDefaultObject();
	if (!CDO) return -1;

	BP->Modify();
	CDO->Modify();

	FGameplayTagContainer* Dest = Prop->ContainerPtrToValuePtr<FGameplayTagContainer>(CDO);
	Dest->Reset();

	int32 Applied = 0;
	for (const FString& TagStr : Tags)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
		if (!Tag.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: skipping unregistered tag '%s' for %s.%s"),
				*TagStr, *GenClass->GetName(), *ContainerName);
			continue;
		}
		Dest->AddTag(Tag);
		++Applied;
	}

	FinalizeBlueprintCDOChange(BP);
	return Applied;
}

bool UUnrealBridgeGameplayAbilityLibrary::SetAbilityInstancingPolicy(
	const FString& AbilityBlueprintPath,
	const FString& Policy)
{
	using namespace BridgeGAWriteImpl;

	bool bParseOk = false;
	const EGameplayAbilityInstancingPolicy::Type Val = ParseInstancingPolicy(Policy, bParseOk);
	if (!bParseOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown InstancingPolicy '%s'"), *Policy);
		return false;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return false;

	FProperty* Prop = GenClass->FindPropertyByName(TEXT("InstancingPolicy"));
	FByteProperty* ByteP = CastField<FByteProperty>(Prop);
	if (!ByteP)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: InstancingPolicy not a TEnumAsByte on '%s'"), *GenClass->GetName());
		return false;
	}

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();
	*ByteP->ContainerPtrToValuePtr<uint8>(CDO) = static_cast<uint8>(Val);

	FinalizeBlueprintCDOChange(BP);
	return true;
}

bool UUnrealBridgeGameplayAbilityLibrary::SetAbilityNetExecutionPolicy(
	const FString& AbilityBlueprintPath,
	const FString& Policy)
{
	using namespace BridgeGAWriteImpl;

	bool bParseOk = false;
	const EGameplayAbilityNetExecutionPolicy::Type Val = ParseNetExecutionPolicy(Policy, bParseOk);
	if (!bParseOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown NetExecutionPolicy '%s'"), *Policy);
		return false;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return false;

	FProperty* Prop = GenClass->FindPropertyByName(TEXT("NetExecutionPolicy"));
	FByteProperty* ByteP = CastField<FByteProperty>(Prop);
	if (!ByteP) return false;

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();
	*ByteP->ContainerPtrToValuePtr<uint8>(CDO) = static_cast<uint8>(Val);

	FinalizeBlueprintCDOChange(BP);
	return true;
}

namespace BridgeGAWriteImpl
{
	/** Shared helper for Cost / Cooldown which are both TSubclassOf<UGameplayEffect>. */
	static bool SetGEClassRef(
		const FString& AbilityBlueprintPath,
		const FString& FieldName,
		const FString& GameplayEffectClassPath)
	{
		UClass* GenClass = nullptr;
		UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
		if (!BP) return false;

		FProperty* Prop = GenClass->FindPropertyByName(FName(*FieldName));
		FClassProperty* ClassP = CastField<FClassProperty>(Prop);
		if (!ClassP) return false;

		UClass* GEClass = nullptr;
		if (!GameplayEffectClassPath.IsEmpty())
		{
			GEClass = BridgeGameplayAbilityImpl::ResolveClassByPath(GameplayEffectClassPath);
			if (!GEClass)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UnrealBridge: could not resolve GE class '%s'"), *GameplayEffectClassPath);
				return false;
			}
			if (!GEClass->IsChildOf(UGameplayEffect::StaticClass()))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UnrealBridge: '%s' is not a UGameplayEffect subclass"), *GameplayEffectClassPath);
				return false;
			}
		}

		UObject* CDO = GenClass->GetDefaultObject();
		BP->Modify();
		CDO->Modify();
		ClassP->SetObjectPropertyValue_InContainer(CDO, GEClass);

		FinalizeBlueprintCDOChange(BP);
		return true;
	}
}

bool UUnrealBridgeGameplayAbilityLibrary::SetAbilityCost(
	const FString& AbilityBlueprintPath,
	const FString& CostGameplayEffectClassPath)
{
	return BridgeGAWriteImpl::SetGEClassRef(
		AbilityBlueprintPath, TEXT("CostGameplayEffectClass"), CostGameplayEffectClassPath);
}

bool UUnrealBridgeGameplayAbilityLibrary::SetAbilityCooldown(
	const FString& AbilityBlueprintPath,
	const FString& CooldownGameplayEffectClassPath)
{
	return BridgeGAWriteImpl::SetGEClassRef(
		AbilityBlueprintPath, TEXT("CooldownGameplayEffectClass"), CooldownGameplayEffectClassPath);
}

int32 UUnrealBridgeGameplayAbilityLibrary::AddAbilityTrigger(
	const FString& AbilityBlueprintPath,
	const FString& TriggerTag,
	const FString& TriggerSource)
{
	using namespace BridgeGAWriteImpl;

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TriggerTag), false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityTrigger '%s' is not a registered tag"), *TriggerTag);
		return -1;
	}

	bool bParseOk = false;
	const EGameplayAbilityTriggerSource::Type Source = ParseTriggerSource(TriggerSource, bParseOk);
	if (!bParseOk)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityTrigger unknown source '%s' (expect GameplayEvent/OwnedTagAdded/OwnedTagPresent)"),
			*TriggerSource);
		return -1;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return -1;

	FProperty* Prop = GenClass->FindPropertyByName(TEXT("AbilityTriggers"));
	FArrayProperty* ArrayP = CastField<FArrayProperty>(Prop);
	if (!ArrayP) return -1;
	FStructProperty* ElemP = CastField<FStructProperty>(ArrayP->Inner);
	if (!ElemP || ElemP->Struct != FAbilityTriggerData::StaticStruct()) return -1;

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();

	FScriptArrayHelper ArrayHelper(ArrayP, ArrayP->ContainerPtrToValuePtr<void>(CDO));
	const int32 NewIdx = ArrayHelper.AddValue();
	FAbilityTriggerData* Elem = reinterpret_cast<FAbilityTriggerData*>(ArrayHelper.GetRawPtr(NewIdx));
	Elem->TriggerTag = Tag;
	Elem->TriggerSource = Source;

	const int32 NewNum = ArrayHelper.Num();
	FinalizeBlueprintCDOChange(BP);
	return NewNum;
}

int32 UUnrealBridgeGameplayAbilityLibrary::RemoveAbilityTriggerByTag(
	const FString& AbilityBlueprintPath,
	const FString& TriggerTag)
{
	using namespace BridgeGAWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return 0;

	FProperty* Prop = GenClass->FindPropertyByName(TEXT("AbilityTriggers"));
	FArrayProperty* ArrayP = CastField<FArrayProperty>(Prop);
	if (!ArrayP) return 0;

	UObject* CDO = GenClass->GetDefaultObject();
	FScriptArrayHelper ArrayHelper(ArrayP, ArrayP->ContainerPtrToValuePtr<void>(CDO));

	int32 Removed = 0;
	for (int32 i = ArrayHelper.Num() - 1; i >= 0; --i)
	{
		FAbilityTriggerData* Elem = reinterpret_cast<FAbilityTriggerData*>(ArrayHelper.GetRawPtr(i));
		if (Elem && Elem->TriggerTag.ToString() == TriggerTag)
		{
			if (Removed == 0)
			{
				BP->Modify();
				CDO->Modify();
			}
			ArrayHelper.RemoveValues(i, 1);
			++Removed;
		}
	}

	if (Removed > 0)
	{
		FinalizeBlueprintCDOChange(BP);
	}
	return Removed;
}

int32 UUnrealBridgeGameplayAbilityLibrary::ClearAbilityTriggers(const FString& AbilityBlueprintPath)
{
	using namespace BridgeGAWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return 0;

	FProperty* Prop = GenClass->FindPropertyByName(TEXT("AbilityTriggers"));
	FArrayProperty* ArrayP = CastField<FArrayProperty>(Prop);
	if (!ArrayP) return 0;

	UObject* CDO = GenClass->GetDefaultObject();
	FScriptArrayHelper ArrayHelper(ArrayP, ArrayP->ContainerPtrToValuePtr<void>(CDO));

	const int32 N = ArrayHelper.Num();
	if (N == 0) return 0;

	BP->Modify();
	CDO->Modify();
	ArrayHelper.EmptyValues();
	FinalizeBlueprintCDOChange(BP);
	return N;
}

// ─── GA graph node writes (M2) ──────────────────────────────

namespace BridgeGAGraphWriteImpl
{
	using namespace BridgeGameplayAbilityImpl;
	using namespace BridgeGAWriteImpl;

	/** Find a graph on a BP by case-insensitive short name. Matches
	 *  `UbergraphPages + FunctionGraphs + MacroGraphs`. */
	static UEdGraph* FindGraphByName(UBlueprint* BP, const FString& Name)
	{
		auto Match = [&Name](UEdGraph* G) { return G && G->GetName().Equals(Name, ESearchCase::IgnoreCase); };
		for (UEdGraph* G : BP->UbergraphPages) if (Match(G)) return G;
		for (UEdGraph* G : BP->FunctionGraphs) if (Match(G)) return G;
		for (UEdGraph* G : BP->MacroGraphs)    if (Match(G)) return G;
		return nullptr;
	}

	/** Set a protected FNameProperty on an already-allocated node via reflection. */
	static bool SetNameFieldByReflection(UObject* Node, const TCHAR* FieldName, FName Value)
	{
		FProperty* Prop = Node->GetClass()->FindPropertyByName(FName(FieldName));
		FNameProperty* NameP = CastField<FNameProperty>(Prop);
		if (!NameP) return false;
		*NameP->ContainerPtrToValuePtr<FName>(Node) = Value;
		return true;
	}

	/** Set a protected TObjectPtr<UClass> property on a node via reflection. */
	static bool SetClassFieldByReflection(UObject* Node, const TCHAR* FieldName, UClass* Value)
	{
		FProperty* Prop = Node->GetClass()->FindPropertyByName(FName(FieldName));
		FClassProperty* ClassP = CastField<FClassProperty>(Prop);
		if (!ClassP) return false;
		ClassP->SetObjectPropertyValue_InContainer(Node, Value);
		return true;
	}

	/** Return true iff Func looks like a UAbilityTask static factory:
	 *  static, BlueprintCallable, returns a pointer to a UAbilityTask subtype. */
	static bool IsAbilityTaskFactory(UFunction* Func)
	{
		if (!Func) return false;
		if (!Func->HasAnyFunctionFlags(FUNC_Static | FUNC_BlueprintCallable)) return false;
		FObjectProperty* RetProp = CastField<FObjectProperty>(Func->GetReturnProperty());
		if (!RetProp || !RetProp->PropertyClass) return false;
		return RetProp->PropertyClass->IsChildOf(UAbilityTask::StaticClass());
	}
}

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListAbilityTaskClasses(const FString& Filter, int32 MaxResults)
{
	TArray<FString> Result;
	if (Filter.IsEmpty() && MaxResults <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListAbilityTaskClasses refused empty filter + unlimited — pass a filter or MaxResults>0"));
		return Result;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Cls = *It;
		if (!Cls || !Cls->IsChildOf(UAbilityTask::StaticClass())) continue;
		if (Cls == UAbilityTask::StaticClass()) continue;
		if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		const FString Path = Cls->GetPathName();
		if (!Filter.IsEmpty() && !Path.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		Result.Add(Path);
		if (MaxResults > 0 && Result.Num() >= MaxResults) break;
	}
	Result.Sort();
	return Result;
}

TArray<FString> UUnrealBridgeGameplayAbilityLibrary::ListAbilityTaskFactories(const FString& TaskClassPath)
{
	using namespace BridgeGAGraphWriteImpl;

	TArray<FString> Result;
	UClass* TaskClass = BridgeGameplayAbilityImpl::ResolveClassByPath(TaskClassPath);
	if (!TaskClass || !TaskClass->IsChildOf(UAbilityTask::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: ListAbilityTaskFactories '%s' is not a UAbilityTask subclass"),
			*TaskClassPath);
		return Result;
	}

	for (TFieldIterator<UFunction> F(TaskClass); F; ++F)
	{
		UFunction* Fn = *F;
		if (!IsAbilityTaskFactory(Fn)) continue;
		// Skip inherited ones unless they live on this exact class.
		if (Fn->GetOuterUClass() != TaskClass) continue;
		Result.Add(Fn->GetName());
	}
	Result.Sort();
	return Result;
}

FString UUnrealBridgeGameplayAbilityLibrary::AddAbilityTaskNode(
	const FString& AbilityBlueprintPath,
	const FString& GraphName,
	const FString& TaskClassPath,
	const FString& FactoryFunctionName,
	int32 NodePosX,
	int32 NodePosY)
{
	using namespace BridgeGAGraphWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return FString();

	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityTaskNode graph '%s' not found on '%s'"),
			*GraphName, *AbilityBlueprintPath);
		return FString();
	}

	UClass* TaskClass = BridgeGameplayAbilityImpl::ResolveClassByPath(TaskClassPath);
	if (!TaskClass || !TaskClass->IsChildOf(UAbilityTask::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityTaskNode '%s' is not a UAbilityTask subclass"),
			*TaskClassPath);
		return FString();
	}

	UFunction* FactoryFn = TaskClass->FindFunctionByName(FName(*FactoryFunctionName));
	if (!FactoryFn || !IsAbilityTaskFactory(FactoryFn))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityTaskNode '%s' has no BlueprintCallable static factory '%s' returning a UAbilityTask*"),
			*TaskClassPath, *FactoryFunctionName);
		return FString();
	}

	FObjectProperty* ReturnProp = CastField<FObjectProperty>(FactoryFn->GetReturnProperty());
	if (!ReturnProp || !ReturnProp->PropertyClass) return FString();

	// The proxy factory usually lives on the task class; ProxyFactoryClass is
	// `Func->GetOuterUClass()`. ProxyClass is the return-type pointee.
	UClass* ProxyFactoryClass = FactoryFn->GetOuterUClass();
	UClass* ProxyClass = ReturnProp->PropertyClass;

	UK2Node_LatentAbilityCall* Node = NewObject<UK2Node_LatentAbilityCall>(Graph);
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);

	// ProxyFactory* fields are protected on UK2Node_BaseAsyncTask — write
	// via reflection so we don't need inheritance access.
	if (!SetNameFieldByReflection(Node, TEXT("ProxyFactoryFunctionName"), FactoryFn->GetFName()) ||
		!SetClassFieldByReflection(Node, TEXT("ProxyFactoryClass"), ProxyFactoryClass) ||
		!SetClassFieldByReflection(Node, TEXT("ProxyClass"), ProxyClass))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityTaskNode reflection set of ProxyFactory* failed on '%s'"),
			*Node->GetClass()->GetName());
		Graph->RemoveNode(Node);
		return FString();
	}

	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Node->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	BP->MarkPackageDirty();
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Create GA Blueprint (M3) ──────────────────────────────

FString UUnrealBridgeGameplayAbilityLibrary::CreateGameplayAbilityBlueprint(
	const FString& DestContentPath,
	const FString& AssetName,
	const FString& ParentClassPath)
{
	if (DestContentPath.IsEmpty() || AssetName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: CreateGameplayAbilityBlueprint requires both DestContentPath and AssetName"));
		return FString();
	}
	if (!DestContentPath.StartsWith(TEXT("/")))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: DestContentPath must start with '/' (e.g. '/Game/GA')"));
		return FString();
	}

	// Resolve parent class — default to UGameplayAbility.
	UClass* ParentClass = UGameplayAbility::StaticClass();
	if (!ParentClassPath.IsEmpty())
	{
		ParentClass = BridgeGameplayAbilityImpl::ResolveClassByPath(ParentClassPath);
		if (!ParentClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: CreateGameplayAbilityBlueprint could not resolve parent '%s'"),
				*ParentClassPath);
			return FString();
		}
		if (!ParentClass->IsChildOf(UGameplayAbility::StaticClass()))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: '%s' is not a UGameplayAbility subclass"),
				*ParentClassPath);
			return FString();
		}
	}

	const FString PackageName = DestContentPath + TEXT("/") + AssetName;
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;

	if (FPackageName::DoesPackageExist(PackageName))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: asset already exists at '%s'"), *PackageName);
		return FString();
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: CreatePackage failed for '%s'"), *PackageName);
		return FString();
	}

	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*AssetName),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		FName(TEXT("UnrealBridge")));

	if (!BP)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: FKismetEditorUtilities::CreateBlueprint returned null for '%s'"),
			*PackageName);
		return FString();
	}

	// Register with the asset registry so Content Browser picks it up; save
	// the package via UEditorLoadingAndSavingUtils (same API the File→Save
	// Dirty menu uses, the existing UnrealBridgeEditorLibrary save UFUNCTIONs
	// also funnel through this).
	FAssetRegistryModule::AssetCreated(BP);
	BP->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false);

	return ObjectPath;
}

FString UUnrealBridgeGameplayAbilityLibrary::AddAbilityCallFunctionNode(
	const FString& AbilityBlueprintPath,
	const FString& GraphName,
	const FString& FunctionName,
	int32 NodePosX,
	int32 NodePosY)
{
	using namespace BridgeGAGraphWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadAbilityBP(AbilityBlueprintPath, &GenClass);
	if (!BP) return FString();

	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	// Look up function on the GA generated class (covers both native
	// UGameplayAbility functions and any Blueprint-defined ones inherited).
	UFunction* Func = GenClass->FindFunctionByName(FName(*FunctionName));
	if (!Func)
	{
		// Fallback: match by DisplayName or ScriptName metadata — lets agents
		// pass "CommitAbility" and find `K2_CommitAbility`
		// (DisplayName="CommitAbility", ScriptName="CommitAbility").
		for (TFieldIterator<UFunction> It(GenClass); It; ++It)
		{
			UFunction* F = *It;
			if (!F) continue;
			if (F->GetMetaData(TEXT("DisplayName")) == FunctionName ||
				F->GetMetaData(TEXT("ScriptName")) == FunctionName)
			{
				Func = F;
				break;
			}
		}
	}
	if (!Func)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddAbilityCallFunctionNode '%s' not found on '%s' (tried internal name + DisplayName + ScriptName)"),
			*FunctionName, *GenClass->GetName());
		return FString();
	}

	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->CreateNewGuid();
	Node->FunctionReference.SetFromField<UFunction>(Func, /*bIsConsideredSelfContext*/ true);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Node->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	BP->MarkPackageDirty();
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── GameplayEffect CDO writes (targeted helpers) ───────────

namespace BridgeGEWriteImpl
{
	static UBlueprint* LoadEffectBP(const FString& Path, UClass** OutGenClass = nullptr)
	{
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path);
		if (!BP || !BP->GeneratedClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: could not load GE BP '%s'"), *Path);
			return nullptr;
		}
		if (!BP->GeneratedClass->IsChildOf(UGameplayEffect::StaticClass()))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: '%s' is not a UGameplayEffect subclass"), *Path);
			return nullptr;
		}
		if (OutGenClass) *OutGenClass = BP->GeneratedClass;
		return BP;
	}

	/** Reflection write: set the inner ScalableFloat.Value of a struct field
	 *  named `MagField` (must be FGameplayEffectModifierMagnitude). Bypasses
	 *  EditDefaultsOnly + protected. */
	static bool WriteScalableMagnitudeStruct(
		UStruct* OwnerStruct, void* OwnerPtr,
		const TCHAR* MagField, float Value)
	{
		FProperty* MagProp = OwnerStruct->FindPropertyByName(FName(MagField));
		FStructProperty* MagSP = CastField<FStructProperty>(MagProp);
		if (!MagSP) return false;
		void* MagPtr = MagSP->ContainerPtrToValuePtr<void>(OwnerPtr);

		FStructProperty* SfSP = CastField<FStructProperty>(
			MagSP->Struct->FindPropertyByName(TEXT("ScalableFloatMagnitude")));
		if (!SfSP) return false;
		void* SfPtr = SfSP->ContainerPtrToValuePtr<void>(MagPtr);

		FFloatProperty* ValFP = CastField<FFloatProperty>(
			SfSP->Struct->FindPropertyByName(TEXT("Value")));
		if (!ValFP) return false;

		// Reset MagnitudeCalculationType to ScalableFloat (enum 0) so any
		// previously-set Attribute-based magnitude doesn't override us.
		FProperty* TypeP = MagSP->Struct->FindPropertyByName(TEXT("MagnitudeCalculationType"));
		if (FEnumProperty* EnumP = CastField<FEnumProperty>(TypeP))
		{
			EnumP->GetUnderlyingProperty()->SetIntPropertyValue(
				EnumP->ContainerPtrToValuePtr<void>(MagPtr), (int64)0);
		}
		else if (FByteProperty* ByteP = CastField<FByteProperty>(TypeP))
		{
			*ByteP->ContainerPtrToValuePtr<uint8>(MagPtr) = 0;
		}

		*ValFP->ContainerPtrToValuePtr<float>(SfPtr) = Value;
		return true;
	}

	static FProperty* FindAttributeProperty(UClass* AttrSetClass, const FString& FieldName)
	{
		for (TFieldIterator<FProperty> It(AttrSetClass); It; ++It)
		{
			FProperty* P = *It;
			FStructProperty* SP = CastField<FStructProperty>(P);
			if (!SP) continue;
			if (SP->Struct != FGameplayAttributeData::StaticStruct()) continue;
			if (SP->GetName() == FieldName) return P;
		}
		return nullptr;
	}

	static bool ParseModOp(const FString& S, EGameplayModOp::Type& OutOp)
	{
		if (S.Equals(TEXT("Additive"), ESearchCase::IgnoreCase))       { OutOp = EGameplayModOp::Additive; return true; }
		if (S.Equals(TEXT("Multiplicitive"), ESearchCase::IgnoreCase)) { OutOp = EGameplayModOp::Multiplicitive; return true; }
		if (S.Equals(TEXT("Division"), ESearchCase::IgnoreCase))       { OutOp = EGameplayModOp::Division; return true; }
		if (S.Equals(TEXT("Override"), ESearchCase::IgnoreCase))       { OutOp = EGameplayModOp::Override; return true; }
		return false;
	}

	static void FinalizeGEBP(UBlueprint* BP)
	{
		// Mark structurally modified so the next compile picks the change up;
		// don't compile or save here — caller drives those via the
		// recompile_blueprint + save_asset two-step flow.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		BP->MarkPackageDirty();
	}
}

bool UUnrealBridgeGameplayAbilityLibrary::SetGEScalableFloatField(
	const FString& GameplayEffectBlueprintPath,
	const FString& FieldName,
	float FlatValue)
{
	using namespace BridgeGEWriteImpl;

	const FString RealField =
		(FieldName == TEXT("duration_magnitude") || FieldName == TEXT("DurationMagnitude"))     ? FString(TEXT("DurationMagnitude")) :
		(FieldName == TEXT("max_duration_magnitude") || FieldName == TEXT("MaxDurationMagnitude")) ? FString(TEXT("MaxDurationMagnitude")) :
		(FieldName == TEXT("period")              || FieldName == TEXT("Period"))                ? FString(TEXT("Period")) :
		FString();
	if (RealField.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: SetGEScalableFloatField unknown field '%s' (expect DurationMagnitude / MaxDurationMagnitude / Period)"),
			*FieldName);
		return false;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return false;

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();

	// Period is a raw FScalableFloat; the others wrap it in
	// FGameplayEffectModifierMagnitude. Branch by struct shape.
	FStructProperty* OuterSP = CastField<FStructProperty>(GenClass->FindPropertyByName(FName(*RealField)));
	if (!OuterSP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: '%s' missing on %s"), *RealField, *GenClass->GetName());
		return false;
	}

	bool bOk = false;
	if (OuterSP->Struct == TBaseStructure<FScalableFloat>::Get() ||
		OuterSP->Struct->GetFName() == TEXT("ScalableFloat"))
	{
		void* SfPtr = OuterSP->ContainerPtrToValuePtr<void>(CDO);
		FFloatProperty* ValFP = CastField<FFloatProperty>(OuterSP->Struct->FindPropertyByName(TEXT("Value")));
		if (ValFP)
		{
			*ValFP->ContainerPtrToValuePtr<float>(SfPtr) = FlatValue;
			bOk = true;
		}
	}
	else
	{
		bOk = WriteScalableMagnitudeStruct(GenClass, CDO, *RealField, FlatValue);
	}

	if (!bOk)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: SetGEScalableFloatField reflection walk failed on %s.%s"),
			*GenClass->GetName(), *RealField);
		return false;
	}

	FinalizeGEBP(BP);
	return true;
}

int32 UUnrealBridgeGameplayAbilityLibrary::AddGEModifierScalable(
	const FString& GameplayEffectBlueprintPath,
	const FString& AttributeSetClassPath,
	const FString& AttributeFieldName,
	const FString& ModOp,
	float FlatMagnitude)
{
	using namespace BridgeGEWriteImpl;

	UClass* AttrSetClass = BridgeGameplayAbilityImpl::ResolveClassByPath(AttributeSetClassPath);
	if (!AttrSetClass || !AttrSetClass->IsChildOf(UAttributeSet::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: AddGEModifierScalable '%s' is not a UAttributeSet subclass"),
			*AttributeSetClassPath);
		return -1;
	}

	FProperty* AttrProp = FindAttributeProperty(AttrSetClass, AttributeFieldName);
	if (!AttrProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: attribute '%s' not found on '%s'"),
			*AttributeFieldName, *AttrSetClass->GetName());
		return -1;
	}

	EGameplayModOp::Type Op;
	if (!ParseModOp(ModOp, Op))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: unknown mod op '%s' (Additive/Multiplicitive/Division/Override)"),
			*ModOp);
		return -1;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return -1;

	FArrayProperty* ArrP = CastField<FArrayProperty>(GenClass->FindPropertyByName(TEXT("Modifiers")));
	FStructProperty* ElemP = ArrP ? CastField<FStructProperty>(ArrP->Inner) : nullptr;
	if (!ArrP || !ElemP || ElemP->Struct != FGameplayModifierInfo::StaticStruct()) return -1;

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();

	FScriptArrayHelper AH(ArrP, ArrP->ContainerPtrToValuePtr<void>(CDO));
	const int32 NewIdx = AH.AddValue();
	FGameplayModifierInfo* Mod = reinterpret_cast<FGameplayModifierInfo*>(AH.GetRawPtr(NewIdx));
	Mod->Attribute = FGameplayAttribute(AttrProp);
	Mod->ModifierOp = Op;

	if (!WriteScalableMagnitudeStruct(ElemP->Struct, Mod, TEXT("ModifierMagnitude"), FlatMagnitude))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddGEModifierScalable failed to write magnitude"));
		AH.RemoveValues(NewIdx, 1);
		return -1;
	}

	const int32 NewLen = AH.Num();
	FinalizeGEBP(BP);
	return NewLen;
}

bool UUnrealBridgeGameplayAbilityLibrary::RemoveGEModifier(
	const FString& GameplayEffectBlueprintPath,
	int32 Index)
{
	using namespace BridgeGEWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return false;

	FArrayProperty* ArrP = CastField<FArrayProperty>(GenClass->FindPropertyByName(TEXT("Modifiers")));
	if (!ArrP) return false;

	UObject* CDO = GenClass->GetDefaultObject();
	FScriptArrayHelper AH(ArrP, ArrP->ContainerPtrToValuePtr<void>(CDO));
	const int32 Actual = (Index < 0) ? (AH.Num() - 1) : Index;
	if (Actual < 0 || Actual >= AH.Num()) return false;

	BP->Modify();
	CDO->Modify();
	AH.RemoveValues(Actual, 1);
	FinalizeGEBP(BP);
	return true;
}

int32 UUnrealBridgeGameplayAbilityLibrary::ClearGEModifiers(
	const FString& GameplayEffectBlueprintPath)
{
	using namespace BridgeGEWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return 0;

	FArrayProperty* ArrP = CastField<FArrayProperty>(GenClass->FindPropertyByName(TEXT("Modifiers")));
	if (!ArrP) return 0;

	UObject* CDO = GenClass->GetDefaultObject();
	FScriptArrayHelper AH(ArrP, ArrP->ContainerPtrToValuePtr<void>(CDO));
	const int32 N = AH.Num();
	if (N == 0) return 0;

	BP->Modify();
	CDO->Modify();
	AH.EmptyValues();
	FinalizeGEBP(BP);
	return N;
}

int32 UUnrealBridgeGameplayAbilityLibrary::AddGEComponent(
	const FString& GameplayEffectBlueprintPath,
	const FString& ComponentClassPath)
{
	using namespace BridgeGEWriteImpl;

	UClass* CompClass = BridgeGameplayAbilityImpl::ResolveClassByPath(ComponentClassPath);
	if (!CompClass || !CompClass->IsChildOf(UGameplayEffectComponent::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: '%s' is not a UGameplayEffectComponent subclass"),
			*ComponentClassPath);
		return -1;
	}
	if (CompClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: '%s' is abstract"), *ComponentClassPath);
		return -1;
	}

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return -1;

	FArrayProperty* ArrP = CastField<FArrayProperty>(GenClass->FindPropertyByName(TEXT("GEComponents")));
	if (!ArrP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: 'GEComponents' array not found on %s"), *GenClass->GetName());
		return -1;
	}
	FObjectProperty* ObjP = CastField<FObjectProperty>(ArrP->Inner);
	if (!ObjP) return -1;

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();

	UGameplayEffectComponent* Comp = NewObject<UGameplayEffectComponent>(
		CDO, CompClass, NAME_None, RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);

	FScriptArrayHelper AH(ArrP, ArrP->ContainerPtrToValuePtr<void>(CDO));
	const int32 NewIdx = AH.AddValue();
	ObjP->SetObjectPropertyValue(AH.GetRawPtr(NewIdx), Comp);

	const int32 NewLen = AH.Num();
	FinalizeGEBP(BP);
	return NewLen;
}

int32 UUnrealBridgeGameplayAbilityLibrary::RemoveGEComponentsByClass(
	const FString& GameplayEffectBlueprintPath,
	const FString& ComponentClassPath)
{
	using namespace BridgeGEWriteImpl;

	UClass* CompClass = BridgeGameplayAbilityImpl::ResolveClassByPath(ComponentClassPath);
	if (!CompClass) return 0;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return 0;

	FArrayProperty* ArrP = CastField<FArrayProperty>(GenClass->FindPropertyByName(TEXT("GEComponents")));
	FObjectProperty* ObjP = ArrP ? CastField<FObjectProperty>(ArrP->Inner) : nullptr;
	if (!ArrP || !ObjP) return 0;

	UObject* CDO = GenClass->GetDefaultObject();
	FScriptArrayHelper AH(ArrP, ArrP->ContainerPtrToValuePtr<void>(CDO));

	int32 Removed = 0;
	for (int32 i = AH.Num() - 1; i >= 0; --i)
	{
		UObject* Entry = ObjP->GetObjectPropertyValue(AH.GetRawPtr(i));
		if (Entry && Entry->GetClass() == CompClass)
		{
			if (Removed == 0)
			{
				BP->Modify();
				CDO->Modify();
			}
			AH.RemoveValues(i, 1);
			++Removed;
		}
	}
	if (Removed > 0)
	{
		FinalizeGEBP(BP);
	}
	return Removed;
}

int32 UUnrealBridgeGameplayAbilityLibrary::SetGEComponentInheritedTags(
	const FString& GameplayEffectBlueprintPath,
	int32 ComponentIndex,
	const FString& FieldName,
	const TArray<FString>& Tags)
{
	using namespace BridgeGEWriteImpl;

	UClass* GenClass = nullptr;
	UBlueprint* BP = LoadEffectBP(GameplayEffectBlueprintPath, &GenClass);
	if (!BP) return -1;

	UObject* CDO = GenClass->GetDefaultObject();
	FArrayProperty* ArrP = CastField<FArrayProperty>(GenClass->FindPropertyByName(TEXT("GEComponents")));
	FObjectProperty* ObjP = ArrP ? CastField<FObjectProperty>(ArrP->Inner) : nullptr;
	if (!ArrP || !ObjP) return -1;

	FScriptArrayHelper AH(ArrP, ArrP->ContainerPtrToValuePtr<void>(CDO));
	if (ComponentIndex < 0 || ComponentIndex >= AH.Num())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: SetGEComponentInheritedTags index %d out of range (count=%d)"),
			ComponentIndex, AH.Num());
		return -1;
	}

	UObject* Comp = ObjP->GetObjectPropertyValue(AH.GetRawPtr(ComponentIndex));
	if (!Comp)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GE component at index %d is null"), ComponentIndex);
		return -1;
	}

	FStructProperty* ITCProp = CastField<FStructProperty>(Comp->GetClass()->FindPropertyByName(FName(*FieldName)));
	if (!ITCProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: field '%s' not found on '%s'"),
			*FieldName, *Comp->GetClass()->GetName());
		return -1;
	}
	// Validate it's an FInheritedTagContainer.
	const bool bIsITC = (ITCProp->Struct && ITCProp->Struct->GetFName() == TEXT("InheritedTagContainer"));
	if (!bIsITC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: field '%s' on '%s' is not an FInheritedTagContainer (got %s)"),
			*FieldName, *Comp->GetClass()->GetName(),
			ITCProp->Struct ? *ITCProp->Struct->GetName() : TEXT("null"));
		return -1;
	}

	BP->Modify();
	CDO->Modify();
	Comp->Modify();

	void* ITCPtr = ITCProp->ContainerPtrToValuePtr<void>(Comp);

	// Set Added field
	FStructProperty* AddedSP = CastField<FStructProperty>(ITCProp->Struct->FindPropertyByName(TEXT("Added")));
	if (!AddedSP) return -1;
	FGameplayTagContainer* Added = AddedSP->ContainerPtrToValuePtr<FGameplayTagContainer>(ITCPtr);
	Added->Reset();

	int32 Applied = 0;
	for (const FString& TagStr : Tags)
	{
		const FGameplayTag T = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
		if (!T.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridge: skipping unregistered tag '%s' on %s.%s"),
				*TagStr, *Comp->GetClass()->GetName(), *FieldName);
			continue;
		}
		Added->AddTag(T);
		++Applied;
	}

	// Also refresh CombinedTags via the struct's helper. Without it, runtime
	// queries against the GE's tags read stale data. Use ApplyTo with an
	// empty parent container to populate CombinedTags from Added/Removed.
	FStructProperty* CombSP = CastField<FStructProperty>(ITCProp->Struct->FindPropertyByName(TEXT("CombinedTags")));
	if (CombSP)
	{
		FGameplayTagContainer* Comb = CombSP->ContainerPtrToValuePtr<FGameplayTagContainer>(ITCPtr);
		Comb->Reset();
		Comb->AppendTags(*Added);
	}

	FinalizeGEBP(BP);
	return Applied;
}

// ─── GameplayCue helper ───────────────────────────────────

bool UUnrealBridgeGameplayAbilityLibrary::SetGameplayCueTag(
	const FString& CueNotifyBlueprintPath,
	const FString& TagString)
{
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *CueNotifyBlueprintPath);
	if (!BP || !BP->GeneratedClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: SetGameplayCueTag could not load '%s'"), *CueNotifyBlueprintPath);
		return false;
	}
	UClass* GenClass = BP->GeneratedClass;
	if (!GenClass->IsChildOf(AGameplayCueNotify_Actor::StaticClass()) &&
		!GenClass->IsChildOf(UGameplayCueNotify_Static::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: '%s' is not a AGameplayCueNotify_Actor/_Static subclass"),
			*CueNotifyBlueprintPath);
		return false;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: SetGameplayCueTag '%s' is not a registered tag"), *TagString);
		return false;
	}

	FStructProperty* TagProp = CastField<FStructProperty>(
		GenClass->FindPropertyByName(TEXT("GameplayCueTag")));
	if (!TagProp || TagProp->Struct != TBaseStructure<FGameplayTag>::Get())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: GameplayCueTag FGameplayTag UPROPERTY not found on %s"),
			*GenClass->GetName());
		return false;
	}

	UObject* CDO = GenClass->GetDefaultObject();
	BP->Modify();
	CDO->Modify();
	*TagProp->ContainerPtrToValuePtr<FGameplayTag>(CDO) = Tag;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	BP->MarkPackageDirty();
	return true;
}

// ─── Cross-asset GameplayTag reference scanner ─────────────

namespace BridgeTagScanImpl
{
	/** Stop runaway recursion. CDO graphs can hit deeply-nested struct trees. */
	static constexpr int32 MaxRecursionDepth = 32;
	/** Hard ceiling — even with MaxResults=0 we never collect more than this. */
	static constexpr int32 AbsoluteCap = 5000;

	struct FScanState
	{
		FGameplayTag QueryTag;
		bool bMatchExact = true;
		int32 MaxResults = 5000;
		bool bTruncated = false;
		TArray<FBridgeTagReference> Refs;
		TSet<UObject*> VisitedObjects; // cycle protection per top-level asset
	};

	static bool TagMatches(const FGameplayTag& Found, const FGameplayTag& Query, bool bExact)
	{
		if (!Found.IsValid()) return false;
		if (bExact) return Found == Query;
		// Non-exact: Found is the same as Query OR a descendant of Query.
		return Found.MatchesTag(Query);
	}

	static void RecordRef(FScanState& S, const FString& AssetPath, const FString& AssetClass,
		const FString& Location, const FString& Context, const FGameplayTag& Tag)
	{
		if (S.Refs.Num() >= S.MaxResults || S.Refs.Num() >= AbsoluteCap)
		{
			S.bTruncated = true;
			return;
		}
		FBridgeTagReference R;
		R.AssetPath   = AssetPath;
		R.AssetClass  = AssetClass;
		R.Location    = Location;
		R.Context     = Context;
		R.MatchedTag  = Tag.ToString();
		S.Refs.Add(MoveTemp(R));
	}

	static void ScanContainerForTags(
		FScanState& S,
		UStruct* Struct, void* ContainerPtr,
		const FString& AssetPath, const FString& AssetClass,
		const FString& Context, const FString& PathPrefix,
		int32 Depth);

	static void ScanProperty(
		FScanState& S,
		FProperty* P, void* ValuePtr,
		const FString& AssetPath, const FString& AssetClass,
		const FString& Context, const FString& FieldPath,
		int32 Depth)
	{
		if (S.bTruncated || Depth > MaxRecursionDepth) return;

		if (FStructProperty* SP = CastField<FStructProperty>(P))
		{
			UScriptStruct* Type = SP->Struct;

			// Direct FGameplayTag
			if (Type == TBaseStructure<FGameplayTag>::Get())
			{
				const FGameplayTag* Tag = static_cast<const FGameplayTag*>(ValuePtr);
				if (TagMatches(*Tag, S.QueryTag, S.bMatchExact))
				{
					RecordRef(S, AssetPath, AssetClass, FieldPath, Context, *Tag);
				}
				return;
			}
			// FGameplayTagContainer
			if (Type == TBaseStructure<FGameplayTagContainer>::Get())
			{
				const FGameplayTagContainer* TC = static_cast<const FGameplayTagContainer*>(ValuePtr);
				for (const FGameplayTag& T : *TC)
				{
					if (TagMatches(T, S.QueryTag, S.bMatchExact))
					{
						RecordRef(S, AssetPath, AssetClass, FieldPath, Context, T);
						if (S.bTruncated) return;
					}
				}
				return;
			}
			// Nested struct — recurse.
			ScanContainerForTags(S, Type, ValuePtr, AssetPath, AssetClass, Context, FieldPath, Depth + 1);
			return;
		}

		if (FArrayProperty* AP = CastField<FArrayProperty>(P))
		{
			FScriptArrayHelper AH(AP, ValuePtr);
			for (int32 i = 0; i < AH.Num(); ++i)
			{
				if (S.bTruncated) return;
				const FString IdxPath = FString::Printf(TEXT("%s[%d]"), *FieldPath, i);
				ScanProperty(S, AP->Inner, AH.GetRawPtr(i), AssetPath, AssetClass, Context, IdxPath, Depth + 1);
			}
			return;
		}

		if (FMapProperty* MP = CastField<FMapProperty>(P))
		{
			FScriptMapHelper MH(MP, ValuePtr);
			for (FScriptMapHelper::FIterator It(MH); It; ++It)
			{
				if (S.bTruncated) return;
				const FString KeyPath = FieldPath + TEXT("[<key>]");
				const FString ValPath = FieldPath + TEXT("[<value>]");
				ScanProperty(S, MP->KeyProp,   MH.GetKeyPtr(It),   AssetPath, AssetClass, Context, KeyPath, Depth + 1);
				ScanProperty(S, MP->ValueProp, MH.GetValuePtr(It), AssetPath, AssetClass, Context, ValPath, Depth + 1);
			}
			return;
		}

		if (FSetProperty* SetP = CastField<FSetProperty>(P))
		{
			FScriptSetHelper SH(SetP, ValuePtr);
			for (FScriptSetHelper::FIterator It(SH); It; ++It)
			{
				if (S.bTruncated) return;
				const FString ElemPath = FieldPath + TEXT("[<elem>]");
				ScanProperty(S, SetP->ElementProp, SH.GetElementPtr(It), AssetPath, AssetClass, Context, ElemPath, Depth + 1);
			}
			return;
		}

		if (FObjectProperty* OP = CastField<FObjectProperty>(P))
		{
			UObject* Inner = OP->GetObjectPropertyValue(ValuePtr);
			if (!Inner) return;
			// Only recurse into instanced subobjects (component-style ownership)
			// to avoid following arbitrary references into the wider asset graph.
			// The outer chain test is the canonical UE pattern for "this object
			// is owned by the container".
			if (S.VisitedObjects.Contains(Inner)) return;

			// Heuristic: recurse only if the property is marked Instanced or
			// the inner object's outer is the container we came from. The
			// container itself might not be a UObject (it could be a struct
			// embedded in one), so rely on Instanced/PersistentInstance flags.
			const bool bIsInstanced = OP->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance);
			if (!bIsInstanced) return;

			S.VisitedObjects.Add(Inner);
			ScanContainerForTags(S, Inner->GetClass(), Inner, AssetPath, AssetClass, Context,
				FieldPath + TEXT("->"), Depth + 1);
			return;
		}

		// Scalar / enum / string / etc. — no tag content possible.
	}

	static void ScanContainerForTags(
		FScanState& S,
		UStruct* Struct, void* ContainerPtr,
		const FString& AssetPath, const FString& AssetClass,
		const FString& Context, const FString& PathPrefix,
		int32 Depth)
	{
		if (!Struct || !ContainerPtr || S.bTruncated || Depth > MaxRecursionDepth) return;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* P = *It;
			// Don't insert '.' when PathPrefix already ends with a separator
			// (the "->" object-recursion marker, or empty).
			FString FieldPath;
			if (PathPrefix.IsEmpty() || PathPrefix.EndsWith(TEXT("->")))
			{
				FieldPath = PathPrefix + P->GetName();
			}
			else
			{
				FieldPath = PathPrefix + TEXT(".") + P->GetName();
			}
			ScanProperty(S, P, P->ContainerPtrToValuePtr<void>(ContainerPtr),
				AssetPath, AssetClass, Context, FieldPath, Depth + 1);
			if (S.bTruncated) return;
		}
	}

	/** Pin default values for tag-typed pins serialize as `(TagName="Foo.Bar")`. */
	static bool ParseTagFromPinDefault(const FString& PinDefault, FGameplayTag& OutTag)
	{
		if (PinDefault.IsEmpty()) return false;
		const int32 Start = PinDefault.Find(TEXT("TagName=\""));
		if (Start == INDEX_NONE) return false;
		const int32 NameStart = Start + 9; // length of TagName="
		const int32 EndQuote = PinDefault.Find(TEXT("\""), ESearchCase::CaseSensitive,
			ESearchDir::FromStart, NameStart);
		if (EndQuote == INDEX_NONE) return false;
		const FString TagStr = PinDefault.Mid(NameStart, EndQuote - NameStart);
		if (TagStr.IsEmpty()) return false;
		OutTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
		return OutTag.IsValid();
	}

	static void ScanBlueprintGraphs(FScanState& S, UBlueprint* BP, const FString& AssetPath)
	{
		auto ScanGraph = [&](UEdGraph* Graph)
		{
			if (!Graph) return;
			const FString GraphName = Graph->GetName();
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (S.bTruncated) return;
				if (!Node) continue;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (S.bTruncated) return;
					if (!Pin) continue;
					// Only tag-typed pins
					UScriptStruct* SubObj = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
					if (!SubObj || SubObj != TBaseStructure<FGameplayTag>::Get()) continue;
					if (Pin->LinkedTo.Num() > 0) continue; // computed at runtime
					FGameplayTag Found;
					if (ParseTagFromPinDefault(Pin->DefaultValue, Found) &&
						TagMatches(Found, S.QueryTag, S.bMatchExact))
					{
						const FString Loc = FString::Printf(TEXT("Pin: %s"), *Pin->PinName.ToString());
						const FString Ctx = FString::Printf(TEXT("Graph: %s, Node: %s"),
							*GraphName, *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
						RecordRef(S, AssetPath, TEXT("Blueprint"), Loc, Ctx, Found);
					}
				}
			}
		};

		for (UEdGraph* G : BP->UbergraphPages)  ScanGraph(G);
		for (UEdGraph* G : BP->FunctionGraphs)  ScanGraph(G);
		for (UEdGraph* G : BP->MacroGraphs)     ScanGraph(G);
	}
}

FBridgeTagReferenceReport UUnrealBridgeGameplayAbilityLibrary::FindGameplayTagReferences(
	const FString& TagQuery,
	const FString& PackagePath,
	bool bMatchExact,
	int32 MaxResults)
{
	using namespace BridgeTagScanImpl;

	FBridgeTagReferenceReport Report;
	Report.TagQuery = TagQuery;
	Report.bMatchExact = bMatchExact;

	const double T0 = FPlatformTime::Seconds();

	const FGameplayTag QueryTag = FGameplayTag::RequestGameplayTag(FName(*TagQuery), false);
	if (!QueryTag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: FindGameplayTagReferences '%s' is not a registered tag"), *TagQuery);
		Report.ScanDurationMs = (FPlatformTime::Seconds() - T0) * 1000.0;
		return Report;
	}

	FScanState S;
	S.QueryTag = QueryTag;
	S.bMatchExact = bMatchExact;
	S.MaxResults = (MaxResults <= 0) ? AbsoluteCap : FMath::Min(MaxResults, AbsoluteCap);

	FString Root = PackagePath.IsEmpty() ? FString(TEXT("/Game")) : PackagePath;
	if (!Root.StartsWith(TEXT("/"))) Root = TEXT("/") + Root;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*Root));
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	int32 PrevRefCount = 0;
	for (const FAssetData& Data : Assets)
	{
		if (S.bTruncated) break;

		UObject* Asset = Data.GetAsset();
		++Report.AssetsScanned;
		if (!Asset) continue;

		S.VisitedObjects.Reset();
		const FString AssetPath = Data.GetSoftObjectPath().ToString();

		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			S.VisitedObjects.Add(BP);
			if (BP->GeneratedClass)
			{
				UObject* CDO = BP->GeneratedClass->GetDefaultObject();
				if (CDO)
				{
					S.VisitedObjects.Add(CDO);
					ScanContainerForTags(S, BP->GeneratedClass, CDO, AssetPath, TEXT("Blueprint"),
						TEXT("CDO"), TEXT(""), 0);
				}
			}
			ScanBlueprintGraphs(S, BP, AssetPath);
		}
		else if (UDataTable* DT = Cast<UDataTable>(Asset))
		{
			if (DT->RowStruct)
			{
				const TMap<FName, uint8*>& Rows = DT->GetRowMap();
				for (const TPair<FName, uint8*>& Pair : Rows)
				{
					if (S.bTruncated) break;
					const FString RowCtx = FString::Printf(TEXT("Row: %s"), *Pair.Key.ToString());
					ScanContainerForTags(S, DT->RowStruct, Pair.Value, AssetPath, TEXT("DataTable"),
						RowCtx, TEXT(""), 0);
				}
			}
		}
		else
		{
			// Generic UObject (DataAsset / UPrimaryDataAsset / etc.) — scan instance.
			S.VisitedObjects.Add(Asset);
			ScanContainerForTags(S, Asset->GetClass(), Asset, AssetPath, Asset->GetClass()->GetName(),
				TEXT("Asset"), TEXT(""), 0);
		}

		if (S.Refs.Num() > PrevRefCount)
		{
			++Report.AssetsMatched;
			PrevRefCount = S.Refs.Num();
		}
	}

	Report.bTruncated      = S.bTruncated;
	Report.ReferenceCount  = S.Refs.Num();
	Report.References      = MoveTemp(S.Refs);
	Report.ScanDurationMs  = static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
	return Report;
}


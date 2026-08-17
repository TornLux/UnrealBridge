#pragma once

#include "CoreMinimal.h"
#include "MaterialTypes.h"
#include "StaticParameterSet.h"

/**
 * 材质参数选择器的内部转换与精确匹配工具；完整身份始终由名称、关联和索引共同组成。
 * Internal conversion and exact-match helpers for material selectors; identity always includes name, association, and index.
 */
namespace BridgeMaterialParameterHelpers
{
	inline FString AssociationToString(EMaterialParameterAssociation Association)
	{
		switch (Association)
		{
			case EMaterialParameterAssociation::GlobalParameter: return TEXT("Global");
			case EMaterialParameterAssociation::LayerParameter: return TEXT("LayerParameter");
			case EMaterialParameterAssociation::BlendParameter: return TEXT("BlendParameter");
			default: return TEXT("Unknown");
		}
	}

	inline bool TryMakeParameterInfo(
		const FString& Name,
		const FString& Association,
		int32 Index,
		FMaterialParameterInfo& OutInfo,
		FString& OutError)
	{
		const FString Selector = Association.IsEmpty() ? TEXT("Global") : Association;
		EMaterialParameterAssociation ParsedAssociation;
		if (Selector.Equals(TEXT("Global"), ESearchCase::IgnoreCase))
		{
			ParsedAssociation = EMaterialParameterAssociation::GlobalParameter;
			if (Index != INDEX_NONE)
			{
				OutError = TEXT("Global association requires index -1");
				return false;
			}
		}
		else if (Selector.Equals(TEXT("LayerParameter"), ESearchCase::IgnoreCase))
		{
			ParsedAssociation = EMaterialParameterAssociation::LayerParameter;
			if (Index < 0)
			{
				OutError = TEXT("LayerParameter association requires a non-negative index");
				return false;
			}
		}
		else if (Selector.Equals(TEXT("BlendParameter"), ESearchCase::IgnoreCase))
		{
			ParsedAssociation = EMaterialParameterAssociation::BlendParameter;
			if (Index < 0)
			{
				OutError = TEXT("BlendParameter association requires a non-negative index");
				return false;
			}
		}
		else
		{
			OutError = FString::Printf(TEXT("unknown association '%s' — expected Global/LayerParameter/BlendParameter"), *Association);
			return false;
		}

		if (Name.IsEmpty())
		{
			OutError = TEXT("parameter name is empty");
			return false;
		}

		OutInfo = FMaterialParameterInfo(FName(*Name), ParsedAssociation, Index);
		return true;
	}

	inline bool ContainsExactInfo(
		const TArray<FMaterialParameterInfo>& Infos,
		const FMaterialParameterInfo& Wanted)
	{
		return Infos.ContainsByPredicate([&Wanted](const FMaterialParameterInfo& Candidate)
		{
			return Candidate == Wanted;
		});
	}

	inline FStaticSwitchParameter* FindExactStaticSwitch(
		FStaticParameterSet& Parameters,
		const FMaterialParameterInfo& Wanted)
	{
		return Parameters.StaticSwitchParameters.FindByPredicate([&Wanted](const FStaticSwitchParameter& Candidate)
		{
			return Candidate.ParameterInfo == Wanted;
		});
	}

	/**
	 * 组合操作只在原子参数批次成功后进入渲染；模板让测试以调用计数器验证该顺序，而无需真实渲染。
	 * The combined operation renders only after the atomic parameter batch succeeds; the template lets tests verify ordering with call counters and no real render.
	 */
	template <typename SetOperationType, typename RenderOperationType>
	bool SetMIAndPreviewAfterSuccessfulSet(
		SetOperationType&& SetOperation,
		RenderOperationType&& RenderOperation)
	{
		const auto ParamResult = Forward<SetOperationType>(SetOperation)();
		if (!ParamResult.bSuccess)
		{
			return false;
		}
		return Forward<RenderOperationType>(RenderOperation)();
	}
}

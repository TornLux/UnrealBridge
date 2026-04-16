#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveAttr, Log, All);

namespace BridgeReactiveAdapterImpl_Attr
{
	FString EscapeSingleQuoted(const FString& In)
	{
		FString Out; Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\'')) Out.AppendChar(TEXT('\\'));
			Out.AppendChar(C);
		}
		return Out;
	}

	/** Resolve FGameplayAttribute from "AttrSet.Field" or bare "Field" against an ASC's spawned sets. */
	FGameplayAttribute ResolveAttribute(UAbilitySystemComponent* ASC, const FString& Needle)
	{
		FString SetName, BareName;
		if (!Needle.Split(TEXT("."), &SetName, &BareName))
		{
			BareName = Needle;
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
					return FGameplayAttribute(P);
				}
			}
		}
		return FGameplayAttribute();
	}
}

/**
 * Binds to UAbilitySystemComponent::GetGameplayAttributeValueChangeDelegate(Attr).
 * Non-dynamic multicast delegate — AddLambda works directly, no listener UObject
 * required. One binding per (ASC, Attribute) shared across handlers.
 */
class FBridgeAttributeChangedAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::AttributeChanged; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC || Record.Selector.IsNone())
		{
			UE_LOG(LogUnrealBridgeReactiveAttr, Warning,
				TEXT("OnHandlerAdded %s: Subject must be an ASC and Selector (attribute name) required"),
				*Record.HandlerId);
			return;
		}

		const FString AttrString = Record.Selector.ToString();
		const FGameplayAttribute Attr = BridgeReactiveAdapterImpl_Attr::ResolveAttribute(ASC, AttrString);
		if (!Attr.IsValid())
		{
			UE_LOG(LogUnrealBridgeReactiveAttr, Warning,
				TEXT("OnHandlerAdded %s: attribute '%s' not found on ASC's spawned sets"),
				*Record.HandlerId, *AttrString);
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.ASC.Get() == ASC && B.AttrName == Record.Selector)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.ASC = ASC;
		NB.AttrName = Record.Selector;
		NB.Attribute = Attr;
		NB.HandlerCount = 1;

		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
		const FName AttrFName = Record.Selector;
		const FString AttrStrCapture = AttrString;

		NB.Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddLambda(
			[WeakASC, AttrFName, AttrStrCapture](const FOnAttributeChangeData& Data)
			{
				UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
				if (!Sub) return;
				TMap<FString, FString> Ctx;
				Ctx.Add(TEXT("trigger"),        TEXT("'attribute_changed'"));
				Ctx.Add(TEXT("attribute_name"), FString::Printf(TEXT("'%s'"),
					*BridgeReactiveAdapterImpl_Attr::EscapeSingleQuoted(AttrStrCapture)));
				Ctx.Add(TEXT("new_value"),      FString::Printf(TEXT("%f"), Data.NewValue));
				Ctx.Add(TEXT("old_value"),      FString::Printf(TEXT("%f"), Data.OldValue));
				Ctx.Add(TEXT("delta"),          FString::Printf(TEXT("%f"), Data.NewValue - Data.OldValue));
				Sub->Dispatch(
					EBridgeTrigger::AttributeChanged,
					TWeakObjectPtr<UObject>(WeakASC.Get()),
					AttrFName,
					Ctx);
			});

		Bindings.Add(NB);
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC) return;
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.ASC.Get() == ASC && B.AttrName == Record.Selector)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount <= 0)
				{
					if (B.ASC.IsValid())
					{
						B.ASC->GetGameplayAttributeValueChangeDelegate(B.Attribute).Remove(B.Handle);
					}
					Bindings.RemoveAtSwap(i);
				}
				return;
			}
		}
	}

	virtual void Shutdown() override
	{
		for (FBinding& B : Bindings)
		{
			if (B.ASC.IsValid())
			{
				B.ASC->GetGameplayAttributeValueChangeDelegate(B.Attribute).Remove(B.Handle);
			}
		}
		Bindings.Reset();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),        TEXT("str — always 'attribute_changed'"));
		D.Add(TEXT("attribute_name"), TEXT("str — 'AttrSet.Field' or the bare 'Field'"));
		D.Add(TEXT("new_value"),      TEXT("float — post-change numeric value"));
		D.Add(TEXT("old_value"),      TEXT("float — pre-change numeric value"));
		D.Add(TEXT("delta"),          TEXT("float — new_value - old_value"));
		return D;
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FName AttrName;
		FGameplayAttribute Attribute;
		FDelegateHandle Handle;
		int32 HandlerCount = 0;
	};
	TArray<FBinding> Bindings;
};

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeAttributeChangedAdapter()
	{
		return MakeUnique<FBridgeAttributeChangedAdapter>();
	}
}

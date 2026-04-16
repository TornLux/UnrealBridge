#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveGE, Log, All);

namespace BridgeReactiveAdapterImpl_GE
{
	/** Escape a string for embedding inside a single-quoted Python literal. */
	FString EscapeSingleQuoted(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\''))
			{
				Out.AppendChar(TEXT('\\'));
			}
			Out.AppendChar(C);
		}
		return Out;
	}

	/** Render a UObject* as a Python expression ("unreal.load_object(None, '/path')" or "None"). */
	FString RenderObjectLiteral(const UObject* Obj)
	{
		if (!Obj)
		{
			return TEXT("None");
		}
		const FString Path = Obj->GetPathName();
		return FString::Printf(TEXT("unreal.load_object(None, '%s')"), *EscapeSingleQuoted(Path));
	}

	FString RenderTagContainerLiteral(const FGameplayTagContainer& Tags)
	{
		if (Tags.Num() == 0)
		{
			return TEXT("[]");
		}
		FString Out = TEXT("[");
		bool bFirst = true;
		for (const FGameplayTag& Tag : Tags)
		{
			if (!bFirst) Out += TEXT(", ");
			bFirst = false;
			Out += FString::Printf(TEXT("'%s'"), *EscapeSingleQuoted(Tag.ToString()));
		}
		Out += TEXT("]");
		return Out;
	}
}

/**
 * Binds to UAbilitySystemComponent::GenericGameplayEventCallbacks[Tag]. The
 * adapter maintains a per-(ASC, Tag) binding record so multiple handlers on
 * the same key share one engine-level delegate registration.
 */
class FBridgeGameplayEventAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::GameplayEvent; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC)
		{
			UE_LOG(LogUnrealBridgeReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: Subject is not an ASC"), *Record.HandlerId);
			return;
		}
		if (Record.Selector.IsNone())
		{
			UE_LOG(LogUnrealBridgeReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: Selector (tag) required"), *Record.HandlerId);
			return;
		}

		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Record.Selector, /*bErrorIfNotFound=*/false);
		if (!Tag.IsValid())
		{
			UE_LOG(LogUnrealBridgeReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: tag '%s' is not registered"),
				*Record.HandlerId, *Record.Selector.ToString());
			return;
		}

		// Reuse binding if one already exists for (ASC, Tag).
		for (FBinding& B : Bindings)
		{
			if (B.ASC.Get() == ASC && B.Tag == Tag)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		// First handler for (ASC, Tag) — bind a lambda to the ASC delegate.
		FBinding NewBinding;
		NewBinding.ASC = ASC;
		NewBinding.Tag = Tag;
		NewBinding.HandlerCount = 1;

		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
		const FGameplayTag CaptureTag = Tag;

		NewBinding.Handle = ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddLambda(
			[WeakASC, CaptureTag](const FGameplayEventData* EventData)
			{
				UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
				if (!Sub)
				{
					return;
				}

				TMap<FString, FString> Ctx;
				Ctx.Add(TEXT("trigger"),   TEXT("'gameplay_event'"));
				Ctx.Add(TEXT("tag"),       FString::Printf(TEXT("'%s'"),
					*BridgeReactiveAdapterImpl_GE::EscapeSingleQuoted(CaptureTag.ToString())));

				if (EventData)
				{
					Ctx.Add(TEXT("event_instigator"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->Instigator.Get()));
					Ctx.Add(TEXT("event_target"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->Target.Get()));
					Ctx.Add(TEXT("event_optional_object"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->OptionalObject.Get()));
					Ctx.Add(TEXT("event_optional_object2"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->OptionalObject2.Get()));
					Ctx.Add(TEXT("event_magnitude"),
						FString::Printf(TEXT("%f"), EventData->EventMagnitude));
					Ctx.Add(TEXT("event_instigator_tags"),
						BridgeReactiveAdapterImpl_GE::RenderTagContainerLiteral(EventData->InstigatorTags));
					Ctx.Add(TEXT("event_target_tags"),
						BridgeReactiveAdapterImpl_GE::RenderTagContainerLiteral(EventData->TargetTags));
				}
				else
				{
					Ctx.Add(TEXT("event_instigator"),        TEXT("None"));
					Ctx.Add(TEXT("event_target"),            TEXT("None"));
					Ctx.Add(TEXT("event_optional_object"),   TEXT("None"));
					Ctx.Add(TEXT("event_optional_object2"),  TEXT("None"));
					Ctx.Add(TEXT("event_magnitude"),         TEXT("0.0"));
					Ctx.Add(TEXT("event_instigator_tags"),   TEXT("[]"));
					Ctx.Add(TEXT("event_target_tags"),       TEXT("[]"));
				}

				Sub->Dispatch(
					EBridgeTrigger::GameplayEvent,
					TWeakObjectPtr<UObject>(WeakASC.Get()),
					CaptureTag.GetTagName(),
					Ctx);
			});

		Bindings.Add(NewBinding);

		UE_LOG(LogUnrealBridgeReactiveGE, Verbose,
			TEXT("bound GameplayEvent lambda for ASC=%s Tag=%s"),
			*ASC->GetPathName(), *Tag.ToString());
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC)
		{
			return;
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Record.Selector, false);
		if (!Tag.IsValid())
		{
			return;
		}
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.ASC.Get() == ASC && B.Tag == Tag)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount <= 0)
				{
					if (B.ASC.IsValid())
					{
						B.ASC->GenericGameplayEventCallbacks.FindOrAdd(B.Tag).Remove(B.Handle);
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
				B.ASC->GenericGameplayEventCallbacks.FindOrAdd(B.Tag).Remove(B.Handle);
			}
		}
		Bindings.Reset();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),                 TEXT("str — always 'gameplay_event'"));
		D.Add(TEXT("tag"),                     TEXT("str — the GameplayTag that fired"));
		D.Add(TEXT("event_instigator"),        TEXT("unreal.Object | None — FGameplayEventData::Instigator"));
		D.Add(TEXT("event_target"),            TEXT("unreal.Object | None — FGameplayEventData::Target"));
		D.Add(TEXT("event_optional_object"),   TEXT("unreal.Object | None"));
		D.Add(TEXT("event_optional_object2"),  TEXT("unreal.Object | None"));
		D.Add(TEXT("event_magnitude"),         TEXT("float — FGameplayEventData::EventMagnitude"));
		D.Add(TEXT("event_instigator_tags"),   TEXT("list[str]"));
		D.Add(TEXT("event_target_tags"),       TEXT("list[str]"));
		return D;
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayTag Tag;
		FDelegateHandle Handle;
		int32 HandlerCount = 0;
	};
	TArray<FBinding> Bindings;
};

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeGameplayEventAdapter()
	{
		return MakeUnique<FBridgeGameplayEventAdapter>();
	}
}

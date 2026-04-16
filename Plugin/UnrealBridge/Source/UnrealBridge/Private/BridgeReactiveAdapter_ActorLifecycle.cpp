#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "UnrealBridgeReactiveListeners.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveLife, Log, All);

namespace BridgeReactiveAdapterImpl_Lifecycle
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

	FString EndPlayReasonName(EEndPlayReason::Type R)
	{
		switch (R)
		{
		case EEndPlayReason::Destroyed:           return TEXT("Destroyed");
		case EEndPlayReason::LevelTransition:     return TEXT("LevelTransition");
		case EEndPlayReason::EndPlayInEditor:     return TEXT("EndPlayInEditor");
		case EEndPlayReason::RemovedFromWorld:    return TEXT("RemovedFromWorld");
		case EEndPlayReason::Quit:                return TEXT("Quit");
		default:                                  return TEXT("Unknown");
		}
	}

	const FName EventDestroyed(TEXT("Destroyed"));
	const FName EventEndPlay(TEXT("EndPlay"));
}

/**
 * Binds AActor::OnDestroyed and AActor::OnEndPlay (both dynamic delegates).
 * One adapter handles both; selector encodes the event name. One listener
 * UObject per binding so unbinding is precise.
 */
class FBridgeActorLifecycleAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::ActorLifecycle; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		AActor* Actor = Cast<AActor>(Record.Subject.Get());
		if (!Actor)
		{
			UE_LOG(LogUnrealBridgeReactiveLife, Warning,
				TEXT("OnHandlerAdded %s: Subject is not an AActor"), *Record.HandlerId);
			return;
		}
		if (Record.Selector != BridgeReactiveAdapterImpl_Lifecycle::EventDestroyed &&
			Record.Selector != BridgeReactiveAdapterImpl_Lifecycle::EventEndPlay)
		{
			UE_LOG(LogUnrealBridgeReactiveLife, Warning,
				TEXT("OnHandlerAdded %s: Selector must be 'Destroyed' or 'EndPlay', got '%s'"),
				*Record.HandlerId, *Record.Selector.ToString());
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.Actor.Get() == Actor && B.Event == Record.Selector)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.Actor = Actor;
		NB.Event = Record.Selector;
		NB.HandlerCount = 1;
		NB.Listener.Reset(NewObject<UBridgeActorLifecycleListener>());
		NB.Listener->Adapter = this;
		NB.Listener->BoundActor = Actor;

		if (Record.Selector == BridgeReactiveAdapterImpl_Lifecycle::EventDestroyed)
		{
			Actor->OnDestroyed.AddDynamic(NB.Listener.Get(),
				&UBridgeActorLifecycleListener::OnActorDestroyed);
		}
		else
		{
			Actor->OnEndPlay.AddDynamic(NB.Listener.Get(),
				&UBridgeActorLifecycleListener::OnActorEndPlay);
		}

		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		AActor* Actor = Cast<AActor>(Record.Subject.Get());
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.Actor.Get() == Actor && B.Event == Record.Selector)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount <= 0)
				{
					UnbindOne(B);
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
			UnbindOne(B);
		}
		Bindings.Reset();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),         TEXT("str — always 'actor_lifecycle'"));
		D.Add(TEXT("event"),           TEXT("str — 'Destroyed' or 'EndPlay'"));
		D.Add(TEXT("actor"),           TEXT("unreal.Actor"));
		D.Add(TEXT("end_play_reason"), TEXT("str — EEndPlayReason name; '' for Destroyed"));
		return D;
	}

	void HandleDestroyed(AActor* Actor)
	{
		FireDispatch(Actor, BridgeReactiveAdapterImpl_Lifecycle::EventDestroyed, FString());
	}

	void HandleEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
	{
		FireDispatch(Actor, BridgeReactiveAdapterImpl_Lifecycle::EventEndPlay,
			BridgeReactiveAdapterImpl_Lifecycle::EndPlayReasonName(Reason));
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<AActor> Actor;
		FName Event;
		TStrongObjectPtr<UBridgeActorLifecycleListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.Actor.IsValid() && B.Listener.IsValid())
		{
			if (B.Event == BridgeReactiveAdapterImpl_Lifecycle::EventDestroyed)
			{
				B.Actor->OnDestroyed.RemoveDynamic(B.Listener.Get(),
					&UBridgeActorLifecycleListener::OnActorDestroyed);
			}
			else if (B.Event == BridgeReactiveAdapterImpl_Lifecycle::EventEndPlay)
			{
				B.Actor->OnEndPlay.RemoveDynamic(B.Listener.Get(),
					&UBridgeActorLifecycleListener::OnActorEndPlay);
			}
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	void FireDispatch(AActor* Actor, FName Event, const FString& ReasonName)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub || !Actor) return;
		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"), TEXT("'actor_lifecycle'"));
		Ctx.Add(TEXT("event"),   FString::Printf(TEXT("'%s'"), *Event.ToString()));
		Ctx.Add(TEXT("actor"),   FString::Printf(TEXT("unreal.load_object(None, '%s')"),
			*BridgeReactiveAdapterImpl_Lifecycle::EscapeSingleQuoted(Actor->GetPathName())));
		Ctx.Add(TEXT("end_play_reason"),
			FString::Printf(TEXT("'%s'"),
				*BridgeReactiveAdapterImpl_Lifecycle::EscapeSingleQuoted(ReasonName)));
		Sub->Dispatch(EBridgeTrigger::ActorLifecycle, TWeakObjectPtr<UObject>(Actor), Event, Ctx);
	}

	TArray<FBinding> Bindings;
};

void UBridgeActorLifecycleListener::OnActorDestroyed(AActor* Actor)
{
	if (Adapter) Adapter->HandleDestroyed(Actor);
}

void UBridgeActorLifecycleListener::OnActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (Adapter) Adapter->HandleEndPlay(Actor, Reason);
}

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeActorLifecycleAdapter()
	{
		return MakeUnique<FBridgeActorLifecycleAdapter>();
	}
}

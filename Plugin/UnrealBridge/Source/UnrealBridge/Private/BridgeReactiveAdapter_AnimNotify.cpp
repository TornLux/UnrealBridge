#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "UnrealBridgeReactiveListeners.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveAnim, Log, All);

namespace BridgeReactiveAdapterImpl_Anim
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

	FString RenderObject(const UObject* O)
	{
		if (!O) return TEXT("None");
		return FString::Printf(TEXT("unreal.load_object(None, '%s')"),
			*EscapeSingleQuoted(O->GetPathName()));
	}
}

/**
 * Binds UAnimInstance::OnPlayMontageNotifyBegin (dynamic). The dynamic delegate
 * lives on UAnimInstance, not on USkeletalMeshComponent — so Subject is the
 * AnimInstance. One listener per AnimInstance; per-notify filtering happens at
 * dispatch time via Selector.
 */
class FBridgeAnimNotifyAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::AnimNotify; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		UAnimInstance* AI = Cast<UAnimInstance>(Record.Subject.Get());
		if (!AI)
		{
			UE_LOG(LogUnrealBridgeReactiveAnim, Warning,
				TEXT("OnHandlerAdded %s: Subject is not a UAnimInstance"), *Record.HandlerId);
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.AnimInstance.Get() == AI)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.AnimInstance = AI;
		NB.HandlerCount = 1;
		NB.Listener.Reset(NewObject<UBridgeAnimNotifyListener>());
		NB.Listener->Adapter = this;
		// BoundMesh kept for diagnostics; not strictly required.
		NB.Listener->BoundMesh = AI->GetSkelMeshComponent();
		AI->OnPlayMontageNotifyBegin.AddDynamic(NB.Listener.Get(),
			&UBridgeAnimNotifyListener::OnNotifyBegin);
		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		UAnimInstance* AI = Cast<UAnimInstance>(Record.Subject.Get());
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.AnimInstance.Get() == AI)
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
		for (FBinding& B : Bindings) UnbindOne(B);
		Bindings.Reset();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),        TEXT("str — always 'anim_notify'"));
		D.Add(TEXT("notify_name"),    TEXT("str — the FName of the notify that fired"));
		D.Add(TEXT("anim_instance"),  TEXT("unreal.AnimInstance"));
		D.Add(TEXT("mesh_component"), TEXT("unreal.SkeletalMeshComponent | None"));
		D.Add(TEXT("owner_actor"),    TEXT("unreal.Actor | None — the SkelMesh's owner"));
		D.Add(TEXT("montage"),        TEXT("unreal.AnimMontage | None — non-null when notify fires from a montage"));
		D.Add(TEXT("source_asset"),   TEXT("unreal.AnimSequenceBase | None — raw SequenceAsset from the payload"));
		return D;
	}

	void HandleNotify(UAnimInstance* AI, FName NotifyName, UAnimSequenceBase* SourceAsset)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub || !AI) return;

		USkeletalMeshComponent* Mesh = AI->GetSkelMeshComponent();
		AActor* OwnerActor = Mesh ? Mesh->GetOwner() : nullptr;
		UAnimMontage* Montage = Cast<UAnimMontage>(SourceAsset);

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),        TEXT("'anim_notify'"));
		Ctx.Add(TEXT("notify_name"),    FString::Printf(TEXT("'%s'"),
			*BridgeReactiveAdapterImpl_Anim::EscapeSingleQuoted(NotifyName.ToString())));
		Ctx.Add(TEXT("anim_instance"),  BridgeReactiveAdapterImpl_Anim::RenderObject(AI));
		Ctx.Add(TEXT("mesh_component"), BridgeReactiveAdapterImpl_Anim::RenderObject(Mesh));
		Ctx.Add(TEXT("owner_actor"),    BridgeReactiveAdapterImpl_Anim::RenderObject(OwnerActor));
		Ctx.Add(TEXT("montage"),        BridgeReactiveAdapterImpl_Anim::RenderObject(Montage));
		Ctx.Add(TEXT("source_asset"),   BridgeReactiveAdapterImpl_Anim::RenderObject(SourceAsset));

		Sub->Dispatch(EBridgeTrigger::AnimNotify,
			TWeakObjectPtr<UObject>(AI), NotifyName, Ctx);
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAnimInstance> AnimInstance;
		TStrongObjectPtr<UBridgeAnimNotifyListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.AnimInstance.IsValid() && B.Listener.IsValid())
		{
			B.AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(B.Listener.Get(),
				&UBridgeAnimNotifyListener::OnNotifyBegin);
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	TArray<FBinding> Bindings;
};

void UBridgeAnimNotifyListener::OnNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	UAnimInstance* AI = nullptr;
	if (Payload.SkelMeshComponent)
	{
		AI = Payload.SkelMeshComponent->GetAnimInstance();
	}
	if (Adapter)
	{
		Adapter->HandleNotify(AI, NotifyName, Payload.SequenceAsset);
	}
}

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeAnimNotifyAdapter()
	{
		return MakeUnique<FBridgeAnimNotifyAdapter>();
	}
}

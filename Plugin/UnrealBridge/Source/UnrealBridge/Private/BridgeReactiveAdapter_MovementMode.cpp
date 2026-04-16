#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "UnrealBridgeReactiveListeners.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveMove, Log, All);

namespace BridgeReactiveAdapterImpl_Move
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

	FString MovementModeName(EMovementMode M)
	{
		switch (M)
		{
		case MOVE_None:       return TEXT("None");
		case MOVE_Walking:    return TEXT("Walking");
		case MOVE_NavWalking: return TEXT("NavWalking");
		case MOVE_Falling:    return TEXT("Falling");
		case MOVE_Swimming:   return TEXT("Swimming");
		case MOVE_Flying:     return TEXT("Flying");
		case MOVE_Custom:     return TEXT("Custom");
		default:              return TEXT("Unknown");
		}
	}
}

/**
 * Binds ACharacter::MovementModeChangedDelegate (dynamic). One listener per
 * character. Selector unused (handlers fire on any mode change; the script
 * filters via ctx if it cares about a specific transition).
 */
class FBridgeMovementModeAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::MovementModeChanged; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		ACharacter* Char = Cast<ACharacter>(Record.Subject.Get());
		if (!Char)
		{
			UE_LOG(LogUnrealBridgeReactiveMove, Warning,
				TEXT("OnHandlerAdded %s: Subject is not an ACharacter"), *Record.HandlerId);
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.Character.Get() == Char)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.Character = Char;
		NB.HandlerCount = 1;
		NB.Listener.Reset(NewObject<UBridgeMovementModeListener>());
		NB.Listener->Adapter = this;
		NB.Listener->BoundCharacter = Char;
		Char->MovementModeChangedDelegate.AddDynamic(NB.Listener.Get(),
			&UBridgeMovementModeListener::OnMovementModeChanged);
		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		ACharacter* Char = Cast<ACharacter>(Record.Subject.Get());
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.Character.Get() == Char)
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
		D.Add(TEXT("trigger"),             TEXT("str — always 'movement_mode_changed'"));
		D.Add(TEXT("character"),           TEXT("unreal.Character"));
		D.Add(TEXT("prev_mode"),           TEXT("int — EMovementMode value (1=Walking,3=Falling,...)"));
		D.Add(TEXT("prev_mode_name"),      TEXT("str — Walking|NavWalking|Falling|Swimming|Flying|Custom|None"));
		D.Add(TEXT("prev_custom_mode"),    TEXT("int — uint8 custom-mode subtype (0 if not Custom)"));
		D.Add(TEXT("current_mode"),        TEXT("int — current EMovementMode value"));
		D.Add(TEXT("current_mode_name"),   TEXT("str — current mode name"));
		D.Add(TEXT("current_custom_mode"), TEXT("int — current custom-mode subtype"));
		return D;
	}

	void HandleChange(ACharacter* Char, EMovementMode PrevMode, uint8 PrevCustom)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub || !Char) return;

		EMovementMode CurrMode = MOVE_None;
		uint8 CurrCustom = 0;
		if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
		{
			CurrMode = CMC->MovementMode;
			CurrCustom = CMC->CustomMovementMode;
		}

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),             TEXT("'movement_mode_changed'"));
		Ctx.Add(TEXT("character"),           FString::Printf(TEXT("unreal.load_object(None, '%s')"),
			*BridgeReactiveAdapterImpl_Move::EscapeSingleQuoted(Char->GetPathName())));
		Ctx.Add(TEXT("prev_mode"),           FString::Printf(TEXT("%d"), static_cast<int32>(PrevMode)));
		Ctx.Add(TEXT("prev_mode_name"),      FString::Printf(TEXT("'%s'"),
			*BridgeReactiveAdapterImpl_Move::MovementModeName(PrevMode)));
		Ctx.Add(TEXT("prev_custom_mode"),    FString::Printf(TEXT("%d"), PrevCustom));
		Ctx.Add(TEXT("current_mode"),        FString::Printf(TEXT("%d"), static_cast<int32>(CurrMode)));
		Ctx.Add(TEXT("current_mode_name"),   FString::Printf(TEXT("'%s'"),
			*BridgeReactiveAdapterImpl_Move::MovementModeName(CurrMode)));
		Ctx.Add(TEXT("current_custom_mode"), FString::Printf(TEXT("%d"), CurrCustom));

		Sub->Dispatch(EBridgeTrigger::MovementModeChanged,
			TWeakObjectPtr<UObject>(Char), NAME_None, Ctx);
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<ACharacter> Character;
		TStrongObjectPtr<UBridgeMovementModeListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.Character.IsValid() && B.Listener.IsValid())
		{
			B.Character->MovementModeChangedDelegate.RemoveDynamic(B.Listener.Get(),
				&UBridgeMovementModeListener::OnMovementModeChanged);
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	TArray<FBinding> Bindings;
};

void UBridgeMovementModeListener::OnMovementModeChanged(ACharacter* Char, EMovementMode PrevMode, uint8 PrevCustom)
{
	if (Adapter) Adapter->HandleChange(Char, PrevMode, PrevCustom);
}

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeMovementModeAdapter()
	{
		return MakeUnique<FBridgeMovementModeAdapter>();
	}
}

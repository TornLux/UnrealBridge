#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "UnrealBridgeReactiveListeners.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveInput, Log, All);

namespace BridgeReactiveAdapterImpl_Input
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

	FString TriggerEventName(ETriggerEvent E)
	{
		switch (E)
		{
		case ETriggerEvent::Triggered: return TEXT("Triggered");
		case ETriggerEvent::Started:   return TEXT("Started");
		case ETriggerEvent::Ongoing:   return TEXT("Ongoing");
		case ETriggerEvent::Canceled:  return TEXT("Canceled");
		case ETriggerEvent::Completed: return TEXT("Completed");
		case ETriggerEvent::None:      return TEXT("None");
		default:                       return TEXT("Unknown");
		}
	}
}

/**
 * Binds UEnhancedInputComponent::BindAction(Action, TriggerEvent, listener,
 * &OnActionFired). One listener per (InputComp, IA, TriggerEvent) so the
 * binding handle is unambiguous on unbind. Selector encodes
 * "<IAName>:<TriggerEvent>" so handlers are matched precisely.
 */
class FBridgeInputActionAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::InputAction; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		UEnhancedInputComponent* Comp = Cast<UEnhancedInputComponent>(Record.Subject.Get());
		if (!Comp)
		{
			UE_LOG(LogUnrealBridgeReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: Subject is not a UEnhancedInputComponent"), *Record.HandlerId);
			return;
		}
		if (Record.Selector.IsNone())
		{
			UE_LOG(LogUnrealBridgeReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: Selector required ('IAName:TriggerEvent')"), *Record.HandlerId);
			return;
		}

		// Selector format: "<IAName>:<TriggerEvent>". AdapterPayload carries the full IA asset path.
		FString IAName, EventName;
		if (!Record.Selector.ToString().Split(TEXT(":"), &IAName, &EventName))
		{
			UE_LOG(LogUnrealBridgeReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: bad Selector '%s'"), *Record.HandlerId, *Record.Selector.ToString());
			return;
		}
		UInputAction* IA = Cast<UInputAction>(StaticLoadObject(
			UInputAction::StaticClass(), nullptr, *Record.AdapterPayload));
		if (!IA)
		{
			UE_LOG(LogUnrealBridgeReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: UInputAction '%s' failed to load"),
				*Record.HandlerId, *Record.AdapterPayload);
			return;
		}

		ETriggerEvent Event = ETriggerEvent::Triggered;
		if      (EventName == TEXT("Triggered")) Event = ETriggerEvent::Triggered;
		else if (EventName == TEXT("Started"))   Event = ETriggerEvent::Started;
		else if (EventName == TEXT("Ongoing"))   Event = ETriggerEvent::Ongoing;
		else if (EventName == TEXT("Canceled"))  Event = ETriggerEvent::Canceled;
		else if (EventName == TEXT("Completed")) Event = ETriggerEvent::Completed;

		for (FBinding& B : Bindings)
		{
			if (B.Comp.Get() == Comp && B.Action.Get() == IA && B.Event == Event)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.Comp = Comp;
		NB.Action = IA;
		NB.Event = Event;
		NB.HandlerCount = 1;
		NB.Listener.Reset(NewObject<UBridgeInputActionListener>());
		NB.Listener->Adapter = this;
		NB.Listener->BoundComp = Comp;
		NB.Listener->BoundAction = IA;
		NB.Listener->TriggerEventValue = static_cast<uint8>(Event);

		FEnhancedInputActionEventBinding& EBind = Comp->BindAction(
			IA, Event, NB.Listener.Get(),
			GET_FUNCTION_NAME_CHECKED(UBridgeInputActionListener, OnActionFired));
		NB.BindingHandle = EBind.GetHandle();

		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		UEnhancedInputComponent* Comp = Cast<UEnhancedInputComponent>(Record.Subject.Get());
		FString IAName, EventName;
		Record.Selector.ToString().Split(TEXT(":"), &IAName, &EventName);
		ETriggerEvent Event = ETriggerEvent::Triggered;
		if      (EventName == TEXT("Triggered")) Event = ETriggerEvent::Triggered;
		else if (EventName == TEXT("Started"))   Event = ETriggerEvent::Started;
		else if (EventName == TEXT("Ongoing"))   Event = ETriggerEvent::Ongoing;
		else if (EventName == TEXT("Canceled"))  Event = ETriggerEvent::Canceled;
		else if (EventName == TEXT("Completed")) Event = ETriggerEvent::Completed;

		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.Comp.Get() == Comp && B.Event == Event &&
				B.Action.IsValid() && B.Action->GetName() == IAName)
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
		D.Add(TEXT("trigger"),         TEXT("str — always 'input_action'"));
		D.Add(TEXT("action_path"),     TEXT("str — the UInputAction's path name"));
		D.Add(TEXT("action_name"),     TEXT("str — short name (e.g. 'IA_Jump')"));
		D.Add(TEXT("trigger_event"),   TEXT("str — Triggered|Started|Ongoing|Canceled|Completed"));
		D.Add(TEXT("value_type"),      TEXT("str — Boolean|Axis1D|Axis2D|Axis3D"));
		D.Add(TEXT("value_bool"),      TEXT("bool — populated for Boolean actions"));
		D.Add(TEXT("value_axis1d"),    TEXT("float — populated for Axis1D"));
		D.Add(TEXT("value_axis2d_x"),  TEXT("float"));
		D.Add(TEXT("value_axis2d_y"),  TEXT("float"));
		D.Add(TEXT("value_axis3d_x"),  TEXT("float"));
		D.Add(TEXT("value_axis3d_y"),  TEXT("float"));
		D.Add(TEXT("value_axis3d_z"),  TEXT("float"));
		D.Add(TEXT("elapsed_seconds"), TEXT("float — Instance.GetElapsedTime()"));
		return D;
	}

	void HandleFire(UEnhancedInputComponent* Comp, const FInputActionInstance& Instance, const UInputAction* Action, ETriggerEvent Event)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub || !Action) return;

		const FInputActionValue Value = Instance.GetValue();
		const EInputActionValueType VT = Value.GetValueType();

		auto VTName = [VT]() -> FString
		{
			switch (VT)
			{
			case EInputActionValueType::Boolean: return TEXT("Boolean");
			case EInputActionValueType::Axis1D:  return TEXT("Axis1D");
			case EInputActionValueType::Axis2D:  return TEXT("Axis2D");
			case EInputActionValueType::Axis3D:  return TEXT("Axis3D");
			default:                             return TEXT("Unknown");
			}
		};

		const FString IAName = Action->GetName();
		const FString IAPath = Action->GetPathName();
		const FString EventName = BridgeReactiveAdapterImpl_Input::TriggerEventName(Event);
		const FName Selector(*FString::Printf(TEXT("%s:%s"), *IAName, *EventName));

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),       TEXT("'input_action'"));
		Ctx.Add(TEXT("action_path"),   FString::Printf(TEXT("'%s'"),
			*BridgeReactiveAdapterImpl_Input::EscapeSingleQuoted(IAPath)));
		Ctx.Add(TEXT("action_name"),   FString::Printf(TEXT("'%s'"),
			*BridgeReactiveAdapterImpl_Input::EscapeSingleQuoted(IAName)));
		Ctx.Add(TEXT("trigger_event"), FString::Printf(TEXT("'%s'"), *EventName));
		Ctx.Add(TEXT("value_type"),    FString::Printf(TEXT("'%s'"), *VTName()));
		Ctx.Add(TEXT("value_bool"),    Value.Get<bool>() ? TEXT("True") : TEXT("False"));
		const float V1 = (VT == EInputActionValueType::Axis1D) ? Value.Get<float>() : 0.f;
		Ctx.Add(TEXT("value_axis1d"),  FString::Printf(TEXT("%f"), V1));
		const FVector2D V2 = (VT == EInputActionValueType::Axis2D) ? Value.Get<FVector2D>() : FVector2D::ZeroVector;
		Ctx.Add(TEXT("value_axis2d_x"), FString::Printf(TEXT("%f"), V2.X));
		Ctx.Add(TEXT("value_axis2d_y"), FString::Printf(TEXT("%f"), V2.Y));
		const FVector V3 = (VT == EInputActionValueType::Axis3D) ? Value.Get<FVector>() : FVector::ZeroVector;
		Ctx.Add(TEXT("value_axis3d_x"), FString::Printf(TEXT("%f"), V3.X));
		Ctx.Add(TEXT("value_axis3d_y"), FString::Printf(TEXT("%f"), V3.Y));
		Ctx.Add(TEXT("value_axis3d_z"), FString::Printf(TEXT("%f"), V3.Z));
		Ctx.Add(TEXT("elapsed_seconds"), FString::Printf(TEXT("%f"), Instance.GetElapsedTime()));

		Sub->Dispatch(EBridgeTrigger::InputAction,
			TWeakObjectPtr<UObject>(Comp), Selector, Ctx);
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UEnhancedInputComponent> Comp;
		TWeakObjectPtr<UInputAction> Action;
		ETriggerEvent Event = ETriggerEvent::Triggered;
		uint32 BindingHandle = 0;
		TStrongObjectPtr<UBridgeInputActionListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.Comp.IsValid())
		{
			B.Comp->RemoveBindingByHandle(B.BindingHandle);
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	TArray<FBinding> Bindings;
};

void UBridgeInputActionListener::OnActionFired(const FInputActionInstance& Instance)
{
	if (!Adapter) return;
	UEnhancedInputComponent* Comp = BoundComp.Get();
	UInputAction* IA = BoundAction.Get();
	const ETriggerEvent E = static_cast<ETriggerEvent>(TriggerEventValue);
	Adapter->HandleFire(Comp, Instance, IA, E);
}

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeInputActionAdapter()
	{
		return MakeUnique<FBridgeInputActionAdapter>();
	}
}

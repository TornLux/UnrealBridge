#include "UnrealBridgeReactiveLibrary.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveLib, Log, All);

namespace BridgeReactiveLibImpl
{
	/**
	 * Resolve a UAbilitySystemComponent from an actor across the common GAS
	 * placement patterns: directly on the actor (single-player), on its
	 * PlayerState (multiplayer-friendly, this project's pattern), or on its
	 * Controller. Walks via IAbilitySystemInterface first, falls back to
	 * component lookup at each level.
	 */
	UAbilitySystemComponent* ResolveActorASC(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		auto FromObject = [](UObject* Obj) -> UAbilitySystemComponent*
		{
			if (!Obj) return nullptr;
			if (IAbilitySystemInterface* I = Cast<IAbilitySystemInterface>(Obj))
			{
				if (UAbilitySystemComponent* ASC = I->GetAbilitySystemComponent())
				{
					return ASC;
				}
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

	/** Find an actor by FName or label across all editor world contexts (PIE first). */
	AActor* FindActorByName(const FString& NameOrLabel)
	{
		if (!GEditor)
		{
			return nullptr;
		}

		auto SearchWorld = [&NameOrLabel](UWorld* World) -> AActor*
		{
			if (!World) return nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* A = *It;
				if (!A) continue;
				if (A->GetName() == NameOrLabel || A->GetActorNameOrLabel() == NameOrLabel)
				{
					return A;
				}
			}
			return nullptr;
		};

		// PIE worlds first — agent-owned actors with ASCs typically live there.
		for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::PIE)
			{
				if (AActor* A = SearchWorld(Ctx.World())) return A;
			}
		}
		// Fall back to the editor world.
		return SearchWorld(GEditor->GetEditorWorldContext().World());
	}

	/** "Permanent" | "Once" | "Count:N" | "WhilePIE" | "WhileSubjectAlive". */
	bool ParseLifetime(const FString& In, EBridgeHandlerLifetime& OutLifetime, int32& OutCount)
	{
		OutCount = -1;
		const FString S = In.IsEmpty() ? TEXT("Permanent") : In;
		if (S.Equals(TEXT("Permanent"), ESearchCase::IgnoreCase))
		{
			OutLifetime = EBridgeHandlerLifetime::Permanent; return true;
		}
		if (S.Equals(TEXT("Once"), ESearchCase::IgnoreCase))
		{
			OutLifetime = EBridgeHandlerLifetime::Once; return true;
		}
		if (S.Equals(TEXT("WhilePIE"), ESearchCase::IgnoreCase))
		{
			OutLifetime = EBridgeHandlerLifetime::WhilePIE; return true;
		}
		if (S.Equals(TEXT("WhileSubjectAlive"), ESearchCase::IgnoreCase))
		{
			OutLifetime = EBridgeHandlerLifetime::WhileSubjectAlive; return true;
		}
		if (S.StartsWith(TEXT("Count:"), ESearchCase::IgnoreCase))
		{
			const FString NStr = S.Mid(6);
			const int32 N = FCString::Atoi(*NStr);
			if (N <= 0)
			{
				return false;
			}
			OutLifetime = EBridgeHandlerLifetime::Count;
			OutCount = N;
			return true;
		}
		return false;
	}

	bool ParseErrorPolicy(const FString& In, EBridgeErrorPolicy& Out)
	{
		const FString S = In.IsEmpty() ? TEXT("LogContinue") : In;
		if (S.Equals(TEXT("LogContinue"), ESearchCase::IgnoreCase))   { Out = EBridgeErrorPolicy::LogContinue;   return true; }
		if (S.Equals(TEXT("LogUnregister"), ESearchCase::IgnoreCase)) { Out = EBridgeErrorPolicy::LogUnregister; return true; }
		if (S.Equals(TEXT("Throw"), ESearchCase::IgnoreCase))         { Out = EBridgeErrorPolicy::Throw;         return true; }
		return false;
	}
}

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeGameplayEvent(
	const FString& TaskName,
	const FString& Description,
	const FString& TargetActorName,
	const FString& EventTag,
	const FString& Script,
	const FString& ScriptPath,
	const TArray<FString>& Tags,
	const FString& Lifetime,
	const FString& ErrorPolicy,
	int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning, TEXT("RegisterRuntimeGameplayEvent: subsystem unavailable"));
		return FString();
	}
	if (TargetActorName.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning, TEXT("RegisterRuntimeGameplayEvent: TargetActorName is empty"));
		return FString();
	}
	AActor* TargetActor = BridgeReactiveLibImpl::FindActorByName(TargetActorName);
	if (!TargetActor)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeGameplayEvent: actor '%s' not found in PIE or editor world"),
			*TargetActorName);
		return FString();
	}
	UAbilitySystemComponent* ASC = BridgeReactiveLibImpl::ResolveActorASC(TargetActor);
	if (!ASC)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeGameplayEvent: no ASC on actor '%s'"), *TargetActor->GetPathName());
		return FString();
	}
	if (EventTag.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning, TEXT("RegisterRuntimeGameplayEvent: EventTag is empty"));
		return FString();
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*EventTag), /*bErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeGameplayEvent: tag '%s' is not registered"), *EventTag);
		return FString();
	}

	EBridgeHandlerLifetime ParsedLifetime = EBridgeHandlerLifetime::Permanent;
	int32 ParsedCount = -1;
	if (!BridgeReactiveLibImpl::ParseLifetime(Lifetime, ParsedLifetime, ParsedCount))
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeGameplayEvent: bad Lifetime '%s'"), *Lifetime);
		return FString();
	}

	EBridgeErrorPolicy ParsedPolicy = EBridgeErrorPolicy::LogContinue;
	if (!BridgeReactiveLibImpl::ParseErrorPolicy(ErrorPolicy, ParsedPolicy))
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeGameplayEvent: bad ErrorPolicy '%s'"), *ErrorPolicy);
		return FString();
	}

	FBridgeHandlerRecord Record;
	Record.Scope = TEXT("runtime");
	Record.TaskName = TaskName;
	Record.Description = Description;
	Record.Tags = Tags;
	Record.ScriptPath = ScriptPath;
	Record.Script = Script;
	Record.TriggerType = EBridgeTrigger::GameplayEvent;
	Record.Subject = TWeakObjectPtr<UObject>(ASC);
	Record.Selector = Tag.GetTagName();
	Record.Lifetime = ParsedLifetime;
	Record.RemainingCalls = ParsedCount;
	Record.ErrorPolicy = ParsedPolicy;
	Record.ThrottleMs = FMath::Max(0, ThrottleMs);

	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── Helpers shared across runtime register_* entry points ──────

namespace BridgeReactiveLibImpl
{
	/** Populate common fields on the record. Returns false if parse failed. */
	bool FillCommonRecordFields(
		FBridgeHandlerRecord& Record,
		const FString& TaskName,
		const FString& Description,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs,
		const TCHAR* CallerTag)
	{
		Record.Scope = TEXT("runtime");
		Record.TaskName = TaskName;
		Record.Description = Description;
		Record.Tags = Tags;
		Record.ScriptPath = ScriptPath;
		Record.Script = Script;
		Record.ThrottleMs = FMath::Max(0, ThrottleMs);

		int32 ParsedCount = -1;
		if (!ParseLifetime(Lifetime, Record.Lifetime, ParsedCount))
		{
			UE_LOG(LogUnrealBridgeReactiveLib, Warning,
				TEXT("%s: bad Lifetime '%s'"), CallerTag, *Lifetime);
			return false;
		}
		Record.RemainingCalls = ParsedCount;

		if (!ParseErrorPolicy(ErrorPolicy, Record.ErrorPolicy))
		{
			UE_LOG(LogUnrealBridgeReactiveLib, Warning,
				TEXT("%s: bad ErrorPolicy '%s'"), CallerTag, *ErrorPolicy);
			return false;
		}
		return true;
	}

	/** Find a UEnhancedInputComponent on the actor or its (player) controller. */
	UEnhancedInputComponent* ResolveInputComponent(AActor* Actor)
	{
		if (!Actor) return nullptr;
		if (UEnhancedInputComponent* C = Cast<UEnhancedInputComponent>(Actor->InputComponent))
		{
			return C;
		}
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (AController* Ctrl = Pawn->GetController())
			{
				if (UEnhancedInputComponent* C = Cast<UEnhancedInputComponent>(Ctrl->InputComponent))
				{
					return C;
				}
			}
		}
		return nullptr;
	}
}

// ─── AttributeChanged ───────────────────────────────────────────

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeAttributeChanged(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& AttributeName,
	const FString& Script, const FString& ScriptPath,
	const TArray<FString>& Tags, const FString& Lifetime,
	const FString& ErrorPolicy, int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = BridgeReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	UAbilitySystemComponent* ASC = BridgeReactiveLibImpl::ResolveActorASC(Actor);
	if (!ASC)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: no ASC on actor '%s'"), *Actor->GetPathName());
		return FString();
	}
	if (AttributeName.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: AttributeName is empty"));
		return FString();
	}

	FBridgeHandlerRecord Record;
	if (!BridgeReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeAttributeChanged")))
	{
		return FString();
	}
	Record.TriggerType = EBridgeTrigger::AttributeChanged;
	Record.Subject = TWeakObjectPtr<UObject>(ASC);
	Record.Selector = FName(*AttributeName);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── ActorLifecycle ─────────────────────────────────────────────

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeActorLifecycle(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& EventType,
	const FString& Script, const FString& ScriptPath,
	const TArray<FString>& Tags, const FString& Lifetime,
	const FString& ErrorPolicy, int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = BridgeReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeActorLifecycle: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	if (!EventType.Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase) &&
		!EventType.Equals(TEXT("EndPlay"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeActorLifecycle: EventType must be 'Destroyed' or 'EndPlay'"));
		return FString();
	}

	FBridgeHandlerRecord Record;
	if (!BridgeReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeActorLifecycle")))
	{
		return FString();
	}
	Record.TriggerType = EBridgeTrigger::ActorLifecycle;
	Record.Subject = TWeakObjectPtr<UObject>(Actor);
	Record.Selector = FName(EventType.Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase)
		? TEXT("Destroyed") : TEXT("EndPlay"));
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── MovementModeChanged ────────────────────────────────────────

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeMovementModeChanged(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = BridgeReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeMovementModeChanged: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	ACharacter* Char = Cast<ACharacter>(Actor);
	if (!Char)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeMovementModeChanged: '%s' is not an ACharacter"), *Actor->GetPathName());
		return FString();
	}

	FBridgeHandlerRecord Record;
	if (!BridgeReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeMovementModeChanged")))
	{
		return FString();
	}
	Record.TriggerType = EBridgeTrigger::MovementModeChanged;
	Record.Subject = TWeakObjectPtr<UObject>(Char);
	Record.Selector = NAME_None;
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── AnimNotify ─────────────────────────────────────────────────

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeAnimNotify(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& NotifyName,
	const FString& Script, const FString& ScriptPath,
	const TArray<FString>& Tags, const FString& Lifetime,
	const FString& ErrorPolicy, int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = BridgeReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: actor '%s' has no USkeletalMeshComponent"),
			*Actor->GetPathName());
		return FString();
	}
	UAnimInstance* AI = Mesh->GetAnimInstance();
	if (!AI)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: skeletal mesh on '%s' has no AnimInstance (AnimBP not set?)"),
			*Actor->GetPathName());
		return FString();
	}
	if (NotifyName.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: NotifyName is empty"));
		return FString();
	}

	FBridgeHandlerRecord Record;
	if (!BridgeReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeAnimNotify")))
	{
		return FString();
	}
	Record.TriggerType = EBridgeTrigger::AnimNotify;
	Record.Subject = TWeakObjectPtr<UObject>(AI);
	Record.Selector = FName(*NotifyName);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── InputAction ────────────────────────────────────────────────

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeInputAction(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& InputActionPath,
	const FString& TriggerEvent, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = BridgeReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	UEnhancedInputComponent* Comp = BridgeReactiveLibImpl::ResolveInputComponent(Actor);
	if (!Comp)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: no UEnhancedInputComponent on actor or its controller (not yet possessed?)"));
		return FString();
	}
	UInputAction* IA = Cast<UInputAction>(StaticLoadObject(
		UInputAction::StaticClass(), nullptr, *InputActionPath));
	if (!IA)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: UInputAction '%s' failed to load"), *InputActionPath);
		return FString();
	}

	// Validate TriggerEvent early.
	static const TCHAR* Known[] = { TEXT("Triggered"), TEXT("Started"), TEXT("Ongoing"),
		TEXT("Canceled"), TEXT("Completed") };
	bool bOk = false;
	for (const TCHAR* K : Known) { if (TriggerEvent.Equals(K, ESearchCase::IgnoreCase)) { bOk = true; break; } }
	if (!bOk)
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: bad TriggerEvent '%s'"), *TriggerEvent);
		return FString();
	}

	FBridgeHandlerRecord Record;
	if (!BridgeReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeInputAction")))
	{
		return FString();
	}
	Record.TriggerType = EBridgeTrigger::InputAction;
	Record.Subject = TWeakObjectPtr<UObject>(Comp);
	Record.Selector = FName(*FString::Printf(TEXT("%s:%s"), *IA->GetName(), *TriggerEvent));
	Record.AdapterPayload = InputActionPath;
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── Timer ──────────────────────────────────────────────────────

FString UUnrealBridgeReactiveLibrary::RegisterRuntimeTimer(
	const FString& TaskName, const FString& Description,
	float IntervalSeconds, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	if (!Sub) return FString();

	if (!(IntervalSeconds > 0.0f))
	{
		UE_LOG(LogUnrealBridgeReactiveLib, Warning,
			TEXT("RegisterRuntimeTimer: IntervalSeconds must be > 0 (got %f)"), IntervalSeconds);
		return FString();
	}

	FBridgeHandlerRecord Record;
	if (!BridgeReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeTimer")))
	{
		return FString();
	}
	Record.TriggerType = EBridgeTrigger::Timer;
	Record.Subject = TWeakObjectPtr<UObject>();   // global — no subject
	Record.Selector = NAME_None;
	Record.AdapterPayload = FString::SanitizeFloat(IntervalSeconds);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ────────────────────────────────────────────────────────────────

bool UUnrealBridgeReactiveLibrary::Unregister(const FString& HandlerId)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	return Sub ? Sub->UnregisterHandler(HandlerId) : false;
}

TArray<FBridgeHandlerSummary> UUnrealBridgeReactiveLibrary::ListAllHandlers(
	const FString& FilterScope,
	const FString& FilterTriggerType,
	const FString& FilterTag)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	return Sub ? Sub->ListAllHandlers(FilterScope, FilterTriggerType, FilterTag)
	           : TArray<FBridgeHandlerSummary>();
}

FBridgeHandlerDetail UUnrealBridgeReactiveLibrary::GetHandler(const FString& HandlerId)
{
	FBridgeHandlerDetail Detail;
	if (UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get())
	{
		Sub->GetHandler(HandlerId, Detail);
	}
	return Detail;
}

FBridgeHandlerStats UUnrealBridgeReactiveLibrary::GetHandlerStats(const FString& HandlerId)
{
	FBridgeHandlerStats Stats;
	if (UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get())
	{
		Sub->GetStats(HandlerId, Stats);
	}
	return Stats;
}

bool UUnrealBridgeReactiveLibrary::Pause(const FString& HandlerId)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	return Sub ? Sub->PauseHandler(HandlerId) : false;
}

bool UUnrealBridgeReactiveLibrary::Resume(const FString& HandlerId)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	return Sub ? Sub->ResumeHandler(HandlerId) : false;
}

int32 UUnrealBridgeReactiveLibrary::ClearAll(const FString& Scope)
{
	UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
	return Sub ? Sub->ClearHandlers(Scope) : 0;
}

void UUnrealBridgeReactiveLibrary::DeferToNextTick(const FString& Script)
{
	if (UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get())
	{
		Sub->DeferToNextTick(Script);
	}
}

TMap<FString, FString> UUnrealBridgeReactiveLibrary::DescribeTriggerContext(const FString& TriggerType)
{
	if (UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get())
	{
		return Sub->DescribeTriggerContext(TriggerType);
	}
	return TMap<FString, FString>();
}

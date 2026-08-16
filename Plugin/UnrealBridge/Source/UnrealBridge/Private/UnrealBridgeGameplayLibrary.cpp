#include "UnrealBridgeGameplayLibrary.h"

#include "UnrealBridgeWorldSelection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ScopedTransaction.h"
#include "InputModifiers.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "InputAction.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "FileHelpers.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "PlayerMappableKeySettings.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_GetInputActionValue.h"
#include "Containers/Ticker.h"
#include "UnrealBridgeReactiveSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeAgent, Log, All);

namespace BridgeAgentImpl
{
	// ─── Sticky EnhancedInput state ────────────────────────────────────
	// Persists across bridge exec calls. The GameThread ticker re-injects
	// every registered entry once per frame so continuous input axes
	// (IA_Move, IA_Look) behave as if the key were held, without Python
	// needing to match UE frame rate.
	struct FStickyEntry
	{
		TWeakObjectPtr<UInputAction> Action;
		FVector Value = FVector::ZeroVector;
		// 粘滞输入固定到创建它的本地玩家世界，禁止在多 PIE 启动或重连时跨世界切换时钟与注入目标。
		// Sticky input is pinned to its originating local-player world so PIE startup/reconnect cannot switch its clock or injection target.
		TWeakObjectPtr<UWorld> World;
		// Auto-clear deadline in world-time seconds. <= 0 means "no deadline,
		// caller will call ClearStickyInput". Set by TriggerInputAction() so
		// a timed hold releases on its own.
		double AutoClearWorldTime = 0.0;
	};

	static TMap<FString, FStickyEntry> GStickyInputs;
	static FTSTicker::FDelegateHandle GStickyTicker;

	static bool StickyTick(float /*Dt*/);

	static void EnsureStickyTickerRunning()
	{
		if (GStickyTicker.IsValid())
		{
			return;
		}
		// Delegate ticker ownership to the reactive subsystem: centralises
		// lifetime so editor shutdown cleans us up even if we forget.
		if (UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get())
		{
			GStickyTicker = Sub->RegisterPersistentTicker(
				[](float Dt) { return StickyTick(Dt); },
				TEXT("StickyInput"));
		}
		else
		{
			// Subsystem unavailable — fall back to direct ticker so sticky still
			// works during tests that run before subsystem init.
			GStickyTicker = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(&StickyTick), 0.0f);
		}
	}

	static void StopStickyTickerIfIdle()
	{
		if (GStickyInputs.Num() == 0 && GStickyTicker.IsValid())
		{
			if (UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get())
			{
				Sub->UnregisterPersistentTicker(GStickyTicker);
			}
			else
			{
				FTSTicker::GetCoreTicker().RemoveTicker(GStickyTicker);
				GStickyTicker.Reset();
			}
		}
	}

	/** 返回首个有效 PIE 世界，保留通用及权威操作原有的上下文顺序语义。 */
	UWorld* GetPIEWorld()
	{
		return GEditor ? SelectFirstValidPIEWorld(GEditor->GetWorldContexts()) : nullptr;
	}

	/** 返回首个已就绪的本地玩家 PIE 世界；客户端启动未就绪及纯服务端场景均显式返回空。 */
	UWorld* GetLocalPlayerPIEWorld()
	{
		return GEditor ? SelectFirstLocalPlayerPIEWorld(GEditor->GetWorldContexts()) : nullptr;
	}

	APawn* GetPlayerPawn(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			return PC->GetPawn();
		}
		return nullptr;
	}

	USkeletalMeshComponent* GetPlayerSkeletalMeshComponent(UWorld* World, const FString& ComponentName)
	{
		APawn* Pawn = GetPlayerPawn(World);
		if (!Pawn)
		{
			return nullptr;
		}

		if (ComponentName.IsEmpty())
		{
			if (const ACharacter* Character = Cast<ACharacter>(Pawn))
			{
				if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
				{
					return CharacterMesh;
				}
			}
			return Pawn->FindComponentByClass<USkeletalMeshComponent>();
		}

		TArray<USkeletalMeshComponent*> MeshComponents;
		Pawn->GetComponents<USkeletalMeshComponent>(MeshComponents);
		for (USkeletalMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent && MeshComponent->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				return MeshComponent;
			}
		}
		return nullptr;
	}
}

bool UUnrealBridgeGameplayLibrary::GetAgentObservation(
	FAgentObservation& OutObservation,
	float MaxActorDistance,
	bool bRequireLineOfSight,
	const FString& ClassFilter)
{
	OutObservation = FAgentObservation();

	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Verbose, TEXT("GetAgentObservation: no ready local-player PIE world"));
		return false;
	}
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn)
	{
		UE_LOG(LogUnrealBridgeAgent, Verbose, TEXT("GetAgentObservation: no player pawn"));
		return false;
	}

	// Pawn state ----------------------------------------------------------
	OutObservation.bValid = true;
	OutObservation.PawnLocation = Pawn->GetActorLocation();
	OutObservation.PawnRotation = Pawn->GetActorRotation();
	OutObservation.PawnVelocity = Pawn->GetVelocity();
	OutObservation.PawnClassName = Pawn->GetClass()->GetName();
	OutObservation.bInputBlocked = false; // PC-level check below
	OutObservation.LastControlInputVector = Pawn->GetLastMovementInputVector();
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (const UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			OutObservation.bOnGround = !Move->IsFalling();
			OutObservation.MovementComponentClassName = Move->GetClass()->GetName();
			OutObservation.MovementMode = (int32)Move->MovementMode;
			OutObservation.CurrentAcceleration = Move->GetCurrentAcceleration();
		}
	}
	else if (const UPawnMovementComponent* Move = Pawn->GetMovementComponent())
	{
		OutObservation.MovementComponentClassName = Move->GetClass()->GetName();
	}

	// Camera --------------------------------------------------------------
	FVector CamLoc = OutObservation.PawnLocation;
	FRotator CamRot = OutObservation.PawnRotation;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
	}
	OutObservation.CameraLocation = CamLoc;
	OutObservation.CameraForward = CamRot.Vector();

	// Visible actors ------------------------------------------------------
	const float MaxDistSq = MaxActorDistance * MaxActorDistance;
	const FString FilterLower = ClassFilter.ToLower();

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(BridgeAgentLOS), /*bTraceComplex=*/ false, Pawn);
	// Also ignore the pawn's current target (its root) from occlusion tests.

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == Pawn || Actor->IsPendingKillPending())
		{
			continue;
		}

		const FVector ActorLoc = Actor->GetActorLocation();
		const float DistSq = FVector::DistSquared(ActorLoc, OutObservation.PawnLocation);
		if (DistSq > MaxDistSq)
		{
			continue;
		}

		if (!FilterLower.IsEmpty())
		{
			const FString ActorClassLower = Actor->GetClass()->GetName().ToLower();
			if (!ActorClassLower.Contains(FilterLower))
			{
				continue;
			}
		}

		if (bRequireLineOfSight)
		{
			FCollisionQueryParams Params = TraceParams;
			Params.AddIgnoredActor(Actor);
			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(
				Hit, OutObservation.CameraLocation, ActorLoc, ECC_Visibility, Params);
			if (bBlocked)
			{
				continue;
			}
		}

		FAgentVisibleActor Entry;
		Entry.ActorName = Actor->GetFName();
		Entry.ClassName = Actor->GetClass()->GetName();
		Entry.Location = ActorLoc;
		Entry.Distance = FMath::Sqrt(DistSq);
		Entry.Tags = Actor->Tags;
		OutObservation.VisibleActors.Add(MoveTemp(Entry));
	}

	// Closest first — handy for agents that just want the nearest target.
	OutObservation.VisibleActors.Sort([](const FAgentVisibleActor& A, const FAgentVisibleActor& B)
	{
		return A.Distance < B.Distance;
	});
	return true;
}

bool UUnrealBridgeGameplayLibrary::FindNavPath(
	const FVector& StartLocation,
	const FVector& EndLocation,
	TArray<FVector>& OutWaypoints,
	float& OutPathLength)
{
	OutWaypoints.Reset();
	OutPathLength = 0.0f;

	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		// Fall back to the editor world so callers can plan paths without
		// being in PIE (useful for test rigs that pre-compute routes).
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext(false).World();
		}
	}
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("FindNavPath: no world available"));
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("FindNavPath: no navigation system in world"));
		return false;
	}

	UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
		World, StartLocation, EndLocation);
	if (!Path || !Path->IsValid() || Path->IsPartial() && Path->PathPoints.Num() == 0)
	{
		UE_LOG(LogUnrealBridgeAgent, Log,
			TEXT("FindNavPath: no path from %s to %s (partial=%s)"),
			*StartLocation.ToString(), *EndLocation.ToString(),
			Path && Path->IsPartial() ? TEXT("true") : TEXT("false"));
		return false;
	}

	OutWaypoints = Path->PathPoints;
	OutPathLength = Path->GetPathLength();
	return true;
}

// ─── Actuators ─────────────────────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::ApplyMovementInput(const FVector& WorldDirection, float ScaleValue, bool bForce)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn)
	{
		return false;
	}
	Pawn->AddMovementInput(WorldDirection, ScaleValue, bForce);
	return true;
}

bool UUnrealBridgeGameplayLibrary::ApplyLookInput(float YawDelta, float PitchDelta)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	if (!FMath::IsNearlyZero(YawDelta))
	{
		PC->AddYawInput(YawDelta);
	}
	if (!FMath::IsNearlyZero(PitchDelta))
	{
		PC->AddPitchInput(PitchDelta);
	}
	return true;
}

bool UUnrealBridgeGameplayLibrary::SetControlRotation(const FRotator& NewRotation)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	PC->SetControlRotation(NewRotation);
	return true;
}

bool UUnrealBridgeGameplayLibrary::Jump()
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	ACharacter* Char = Cast<ACharacter>(Pawn);
	if (!Char)
	{
		return false;
	}
	Char->Jump();
	return true;
}

bool UUnrealBridgeGameplayLibrary::StopJumping()
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	ACharacter* Char = Cast<ACharacter>(Pawn);
	if (!Char)
	{
		return false;
	}
	Char->StopJumping();
	return true;
}

bool UUnrealBridgeGameplayLibrary::InjectEnhancedInputAxis(const FString& InputActionPath, const FVector& AxisValue)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("InjectEnhancedInputAxis: no ready local-player PIE world"));
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->GetLocalPlayer())
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("InjectEnhancedInputAxis: no local player"));
		return false;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("InjectEnhancedInputAxis: project has no EnhancedInputLocalPlayerSubsystem"));
		return false;
	}

	UInputAction* Action = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!Action)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("InjectEnhancedInputAxis: failed to load IA '%s'"), *InputActionPath);
		return false;
	}

	// FInputActionValue rejects type-mismatched raw values silently — e.g.
	// passing Axis3D to a Bool/Axis2D IA drops the inject. Construct the
	// value with the IA's declared type so inputs always land.
	const EInputActionValueType IaType = Action->ValueType;
	FInputActionValue Value(IaType, AxisValue);
	Subsystem->InjectInputForAction(Action, Value, {}, {});
	UE_LOG(LogUnrealBridgeAgent, VeryVerbose,
		TEXT("InjectEnhancedInputAxis: %s type=%d raw=(%.2f,%.2f,%.2f)"),
		*Action->GetName(), (int32)IaType, AxisValue.X, AxisValue.Y, AxisValue.Z);
	return true;
}

// ─── Sticky inject tick ────────────────────────────────────────────────

namespace BridgeAgentImpl
{
	bool StickyTick(float /*Dt*/)
	{
		if (GStickyInputs.Num() == 0)
		{
			return true; // nothing to do — but keep ticker alive, caller may add more
		}

		for (TMap<FString, FStickyEntry>::TIterator It(GStickyInputs); It; ++It)
		{
			FStickyEntry& E = It->Value;
			UWorld* World = E.World.Get();
			UInputAction* Action = E.Action.Get();
			if (!World || !World->HasBegunPlay() || !Action)
			{
				It.RemoveCurrent();
				continue;
			}

			APlayerController* PlayerController = World->GetFirstPlayerController();
			if (!PlayerController || !PlayerController->GetLocalPlayer())
			{
				// 原本地玩家失效后丢弃条目；不得把旧输入迟到注入新客户端或服务端世界。
				// Drop entries whose original local player disappeared; never inject stale input into a new client or server world.
				It.RemoveCurrent();
				continue;
			}

			UEnhancedInputLocalPlayerSubsystem* Subsystem =
				PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
			if (!Subsystem)
			{
				continue;
			}

			const double WorldNow = World->GetTimeSeconds();
			if (E.AutoClearWorldTime > 0.0 && WorldNow >= E.AutoClearWorldTime)
			{
				UE_LOG(LogUnrealBridgeAgent, Verbose,
					TEXT("StickyTick: auto-clear '%s' at world-time %.3f"),
					*It->Key, WorldNow);
				It.RemoveCurrent();
				continue;
			}
			const EInputActionValueType IaType = Action->ValueType;
			FInputActionValue Value(IaType, E.Value);
			Subsystem->InjectInputForAction(Action, Value, {}, {});
		}
		StopStickyTickerIfIdle();
		return true;
	}
}

bool UUnrealBridgeGameplayLibrary::SetStickyInput(const FString& InputActionPath, const FVector& AxisValue)
{
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!Action)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("SetStickyInput: failed to load IA '%s'"), *InputActionPath);
		return false;
	}
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("SetStickyInput: no ready local-player PIE world for '%s'"), *InputActionPath);
		return false;
	}
	BridgeAgentImpl::FStickyEntry Entry;
	Entry.Action = Action;
	Entry.Value = AxisValue;
	Entry.World = World;
	BridgeAgentImpl::GStickyInputs.Add(InputActionPath, Entry);
	BridgeAgentImpl::EnsureStickyTickerRunning();
	UE_LOG(LogUnrealBridgeAgent, Log, TEXT("SetStickyInput: %s = (%.2f, %.2f, %.2f)"),
		*InputActionPath, AxisValue.X, AxisValue.Y, AxisValue.Z);
	return true;
}

bool UUnrealBridgeGameplayLibrary::ClearStickyInput(const FString& InputActionPath)
{
	if (InputActionPath.IsEmpty())
	{
		const int32 N = BridgeAgentImpl::GStickyInputs.Num();
		BridgeAgentImpl::GStickyInputs.Reset();
		UE_LOG(LogUnrealBridgeAgent, Log, TEXT("ClearStickyInput: removed all %d entries"), N);
		BridgeAgentImpl::StopStickyTickerIfIdle();
		return N > 0;
	}
	const int32 Removed = BridgeAgentImpl::GStickyInputs.Remove(InputActionPath);
	BridgeAgentImpl::StopStickyTickerIfIdle();
	return Removed > 0;
}

// ─── Adaptive trigger ──────────────────────────────────────────────────

namespace BridgeAgentImpl
{
	/** Short class name for an InputTrigger subclass, stripped of the
	 *  "InputTrigger" prefix so callers see "Hold" instead of "InputTriggerHold". */
	static FString TriggerShortName(const UInputTrigger* Trigger)
	{
		if (!Trigger) return TEXT("");
		FString Name = Trigger->GetClass()->GetName();
		Name.RemoveFromStart(TEXT("InputTrigger"));
		return Name;
	}

	/** Time threshold (seconds) baked into the trigger, or 0 if it has none. */
	static float TriggerThresholdSeconds(const UInputTrigger* Trigger)
	{
		if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger))
		{
			return Hold->HoldTimeThreshold;
		}
		if (const UInputTriggerTap* Tap = Cast<UInputTriggerTap>(Trigger))
		{
			return Tap->TapReleaseTimeThreshold;
		}
		if (const UInputTriggerPulse* Pulse = Cast<UInputTriggerPulse>(Trigger))
		{
			return Pulse->Interval;
		}
		return 0.0f;
	}
}

bool UUnrealBridgeGameplayLibrary::GetInputActionTriggers(
	const FString& InputActionPath,
	TArray<FString>& OutTriggerNames,
	TArray<float>& OutThresholdSeconds)
{
	OutTriggerNames.Reset();
	OutThresholdSeconds.Reset();
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!Action)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("GetInputActionTriggers: failed to load IA '%s'"), *InputActionPath);
		return false;
	}
	OutTriggerNames.Reserve(Action->Triggers.Num());
	OutThresholdSeconds.Reserve(Action->Triggers.Num());
	for (const UInputTrigger* Trigger : Action->Triggers)
	{
		OutTriggerNames.Add(BridgeAgentImpl::TriggerShortName(Trigger));
		OutThresholdSeconds.Add(BridgeAgentImpl::TriggerThresholdSeconds(Trigger));
	}
	return true;
}

bool UUnrealBridgeGameplayLibrary::TriggerInputAction(const FString& InputActionPath, float HoldSeconds)
{
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!Action)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("TriggerInputAction: failed to load IA '%s'"), *InputActionPath);
		return false;
	}
	if (Action->ValueType != EInputActionValueType::Boolean)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("TriggerInputAction: IA '%s' is not Bool (type=%d) — use SetStickyInput for axes."),
			*InputActionPath, (int32)Action->ValueType);
		return false;
	}

	// Resolve hold duration.
	float EffectiveHold = HoldSeconds;
	if (EffectiveHold < 0.0f)
	{
		// Auto: inspect triggers.
		EffectiveHold = 0.0f; // default to single pulse
		for (const UInputTrigger* Trigger : Action->Triggers)
		{
			if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger))
			{
				EffectiveHold = FMath::Max(EffectiveHold, Hold->HoldTimeThreshold + 0.05f);
			}
			else if (const UInputTriggerTap* Tap = Cast<UInputTriggerTap>(Trigger))
			{
				// Release before the tap threshold so the Tap trigger fires.
				const float TapHold = FMath::Max(0.01f, Tap->TapReleaseTimeThreshold * 0.5f);
				EffectiveHold = FMath::Max(EffectiveHold, TapHold);
			}
			else if (Cast<UInputTriggerReleased>(Trigger) || Cast<UInputTriggerPulse>(Trigger)
				|| Cast<UInputTriggerDown>(Trigger))
			{
				// Released needs a key-up edge; Pulse needs at least one interval;
				// Down fires continuously while held and many gameplay handlers
				// (Character::Jump via InputComponent, sprint, etc.) need
				// several ticks of Triggered state before they latch — a
				// single pulse isn't enough. 150 ms matches the press duration
				// the earlier manual sticky→clear demo used to trigger jump
				// reliably on 60 Hz.
				EffectiveHold = FMath::Max(EffectiveHold, 0.15f);
			}
		}
	}

	if (EffectiveHold <= 0.0f)
	{
		// Single-tick pulse path — same as InjectEnhancedInputAxis.
		return InjectEnhancedInputAxis(InputActionPath, FVector(1.0, 0.0, 0.0));
	}

	// Sticky-hold path: need a PIE world to anchor the world-time deadline.
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("TriggerInputAction: no ready local-player PIE world; cannot schedule auto-clear for '%s'"),
			*InputActionPath);
		return false;
	}

	BridgeAgentImpl::FStickyEntry Entry;
	Entry.Action = Action;
	Entry.Value = FVector(1.0, 0.0, 0.0);
	Entry.World = World;
	Entry.AutoClearWorldTime = World->GetTimeSeconds() + EffectiveHold;
	BridgeAgentImpl::GStickyInputs.Add(InputActionPath, Entry);
	BridgeAgentImpl::EnsureStickyTickerRunning();
	UE_LOG(LogUnrealBridgeAgent, Log,
		TEXT("TriggerInputAction: '%s' held for %.3fs (auto-clear)"),
		*InputActionPath, EffectiveHold);
	return true;
}

// ─── State inspection + reset ──────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::GetControlRotation(FRotator& OutRotation)
{
	OutRotation = FRotator::ZeroRotator;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	OutRotation = PC->GetControlRotation();
	return true;
}

bool UUnrealBridgeGameplayLibrary::TeleportPawn(
	const FVector& NewLocation,
	const FRotator& NewRotation,
	bool bSnapController,
	bool bStopVelocity)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn)
	{
		return false;
	}

	// Stop before teleport — moving a character while velocity is live can
	// cause it to slide past the target on the next movement tick.
	if (bStopVelocity)
	{
		if (UPawnMovementComponent* Move = Pawn->GetMovementComponent())
		{
			Move->StopMovementImmediately();
		}
	}

	Pawn->SetActorLocationAndRotation(NewLocation, NewRotation, /*bSweep=*/ false,
		/*OutSweepHitResult=*/ nullptr, ETeleportType::TeleportPhysics);

	if (bSnapController)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetControlRotation(NewRotation);
		}
	}
	return true;
}

int32 UUnrealBridgeGameplayLibrary::GetStickyInputs(TArray<FString>& OutPaths, TArray<FVector>& OutValues)
{
	OutPaths.Reset();
	OutValues.Reset();
	OutPaths.Reserve(BridgeAgentImpl::GStickyInputs.Num());
	OutValues.Reserve(BridgeAgentImpl::GStickyInputs.Num());
	for (const TPair<FString, BridgeAgentImpl::FStickyEntry>& Pair : BridgeAgentImpl::GStickyInputs)
	{
		OutPaths.Add(Pair.Key);
		OutValues.Add(Pair.Value.Value);
	}
	return OutPaths.Num();
}

bool UUnrealBridgeGameplayLibrary::IsInPIE()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	return World != nullptr && BridgeAgentImpl::GetPlayerPawn(World) != nullptr;
}

// ─── Camera aim + perception helpers ───────────────────────────────────

namespace BridgeAgentImpl
{
	/** Find an actor by FName or visible label in the PIE world. */
	static AActor* FindPIEActor(UWorld* World, const FString& NameOrLabel)
	{
		if (!World || NameOrLabel.IsEmpty())
		{
			return nullptr;
		}
		const FName AsFName(*NameOrLabel);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A) continue;
			if (A->GetFName() == AsFName)
			{
				return A;
			}
		}
		// Fall back to label match.
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (A && A->GetActorLabel() == NameOrLabel)
			{
				return A;
			}
		}
		return nullptr;
	}

	/** Camera viewpoint of the first PIE player controller. */
	static bool GetCameraView(UWorld* World, FVector& OutLoc, FRotator& OutRot)
	{
		if (!World) return false;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) return false;
		PC->GetPlayerViewPoint(OutLoc, OutRot);
		return true;
	}
}

FString UUnrealBridgeGameplayLibrary::GetCameraHitActor(float MaxDistance)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	FVector CamLoc; FRotator CamRot;
	if (!BridgeAgentImpl::GetCameraView(World, CamLoc, CamRot))
	{
		return FString();
	}
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	const FVector End = CamLoc + CamRot.Vector() * FMath::Max(MaxDistance, 0.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeCameraAim), /*bTraceComplex=*/ false);
	if (Pawn) Params.AddIgnoredActor(Pawn);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params);
	if (!bHit || !Hit.GetActor())
	{
		return FString();
	}
	return Hit.GetActor()->GetFName().ToString();
}

bool UUnrealBridgeGameplayLibrary::GetCameraHitLocation(float MaxDistance, FVector& OutHitLocation)
{
	OutHitLocation = FVector::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	FVector CamLoc; FRotator CamRot;
	if (!BridgeAgentImpl::GetCameraView(World, CamLoc, CamRot))
	{
		return false;
	}
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	const FVector End = CamLoc + CamRot.Vector() * FMath::Max(MaxDistance, 0.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeCameraAimLoc), /*bTraceComplex=*/ false);
	if (Pawn) Params.AddIgnoredActor(Pawn);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params))
	{
		return false;
	}
	OutHitLocation = Hit.ImpactPoint;
	return true;
}

bool UUnrealBridgeGameplayLibrary::IsActorVisibleFromCamera(const FString& ActorName, float MaxDistance)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	FVector CamLoc; FRotator CamRot;
	if (!BridgeAgentImpl::GetCameraView(World, CamLoc, CamRot))
	{
		return false;
	}
	AActor* Target = BridgeAgentImpl::FindPIEActor(World, ActorName);
	if (!Target)
	{
		return false;
	}

	// Test against 9 sample points: bounds center + 8 corners. An actor
	// counts as visible if ANY of these can be reached without being
	// blocked by a different actor first. This handles cases where the
	// actor's pivot is inside its own mesh / beneath terrain.
	FVector Origin, Extent;
	Target->GetActorBounds(/*bOnlyCollidingComponents=*/ false, Origin, Extent, /*bIncludeFromChildActors=*/ true);

	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	const float MaxDistSafe = FMath::Max(MaxDistance, 0.0f);

	TArray<FVector, TInlineAllocator<9>> SamplePoints;
	SamplePoints.Add(Origin);
	for (int32 i = 0; i < 8; ++i)
	{
		const FVector C(
			(i & 1) ? Extent.X : -Extent.X,
			(i & 2) ? Extent.Y : -Extent.Y,
			(i & 4) ? Extent.Z : -Extent.Z);
		SamplePoints.Add(Origin + C);
	}

	for (const FVector& P : SamplePoints)
	{
		if (FVector::Dist(CamLoc, P) > MaxDistSafe)
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeCameraVis), /*bTraceComplex=*/ false);
		if (Pawn) Params.AddIgnoredActor(Pawn);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, CamLoc, P, ECC_Visibility, Params);
		if (!bHit || Hit.GetActor() == Target)
		{
			return true;
		}
	}
	return false;
}

float UUnrealBridgeGameplayLibrary::GetPawnGroundHeight(float MaxDistance)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn)
	{
		return -1.0f;
	}
	const FVector Start = Pawn->GetActorLocation();
	const FVector End = Start - FVector(0, 0, FMath::Max(MaxDistance, 0.0f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgePawnGround), /*bTraceComplex=*/ false, Pawn);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return -1.0f;
	}
	return (Start - Hit.ImpactPoint).Size();
}

// ─── Navmesh utilities ─────────────────────────────────────────────────

namespace BridgeAgentImpl
{
	/** PIE world first, editor world second (matches FindNavPath behaviour). */
	static UWorld* GetNavWorld()
	{
		if (UWorld* W = GetPIEWorld())
		{
			return W;
		}
		return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
	}
}

bool UUnrealBridgeGameplayLibrary::ProjectPointToNavmesh(const FVector& Point, const FVector& SearchExtent, FVector& OutProjected)
{
	OutProjected = FVector::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetNavWorld();
	if (!World)
	{
		return false;
	}
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}
	const FVector SafeExtent(
		FMath::Max(SearchExtent.X, 1.0f),
		FMath::Max(SearchExtent.Y, 1.0f),
		FMath::Max(SearchExtent.Z, 1.0f));
	FNavLocation Projected;
	if (!NavSys->ProjectPointToNavigation(Point, Projected, SafeExtent))
	{
		return false;
	}
	OutProjected = Projected.Location;
	return true;
}

bool UUnrealBridgeGameplayLibrary::IsPointOnNavmesh(const FVector& Point, float Tolerance)
{
	FVector Dummy;
	const float T = FMath::Max(Tolerance, 1.0f);
	return ProjectPointToNavmesh(Point, FVector(T, T, T), Dummy);
}

bool UUnrealBridgeGameplayLibrary::GetNavMeshBounds(FVector& OutMin, FVector& OutMax)
{
	OutMin = FVector::ZeroVector;
	OutMax = FVector::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetNavWorld();
	if (!World)
	{
		return false;
	}
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}
	const FBox Bounds = NavSys->GetNavigableWorldBounds();
	if (!Bounds.IsValid)
	{
		return false;
	}
	OutMin = Bounds.Min;
	OutMax = Bounds.Max;
	return true;
}

bool UUnrealBridgeGameplayLibrary::GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint)
{
	OutPoint = FVector::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetNavWorld();
	if (!World)
	{
		return false;
	}
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}
	FNavLocation Result;
	if (!NavSys->GetRandomReachablePointInRadius(Origin, FMath::Max(Radius, 0.0f), Result))
	{
		return false;
	}
	OutPoint = Result.Location;
	return true;
}

// ─── Screen-space perception ───────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::GetPIEViewportSize(FVector2D& OutSize)
{
	OutSize = FVector2D::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);
	if (W <= 0 || H <= 0)
	{
		return false;
	}
	OutSize.X = static_cast<double>(W);
	OutSize.Y = static_cast<double>(H);
	return true;
}

bool UUnrealBridgeGameplayLibrary::DeprojectScreenToWorld(float NormalizedX, float NormalizedY, FVector& OutOrigin, FVector& OutDirection)
{
	OutOrigin = FVector::ZeroVector;
	OutDirection = FVector::ForwardVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);
	if (W <= 0 || H <= 0)
	{
		return false;
	}
	const float PixelX = FMath::Clamp(NormalizedX, 0.0f, 1.0f) * W;
	const float PixelY = FMath::Clamp(NormalizedY, 0.0f, 1.0f) * H;
	return PC->DeprojectScreenPositionToWorld(PixelX, PixelY, OutOrigin, OutDirection);
}

bool UUnrealBridgeGameplayLibrary::ProjectWorldToScreen(const FVector& WorldLocation, FVector2D& OutNormalized)
{
	OutNormalized = FVector2D::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);
	if (W <= 0 || H <= 0)
	{
		return false;
	}
	FVector2D Screen;
	if (!PC->ProjectWorldLocationToScreen(WorldLocation, Screen, /*bPlayerViewportRelative=*/ false))
	{
		return false;
	}
	const float NX = Screen.X / W;
	const float NY = Screen.Y / H;
	OutNormalized.X = NX;
	OutNormalized.Y = NY;
	// Off-screen check — reject if outside [0,1] (already tells caller the
	// point isn't currently rendered).
	return NX >= 0.0f && NX <= 1.0f && NY >= 0.0f && NY <= 1.0f;
}

// ─── Camera control + fast view query ─────────────────────────────────

namespace BridgeAgentImpl
{
	static APlayerCameraManager* GetPIECameraManager()
	{
		UWorld* World = GetLocalPlayerPIEWorld();
		if (!World) return nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		return PC ? PC->PlayerCameraManager : nullptr;
	}
}

float UUnrealBridgeGameplayLibrary::GetCameraFOV()
{
	APlayerCameraManager* Cam = BridgeAgentImpl::GetPIECameraManager();
	return Cam ? Cam->GetFOVAngle() : -1.0f;
}

bool UUnrealBridgeGameplayLibrary::SetCameraFOV(float FOV)
{
	if (FOV < 1.0f || FOV > 170.0f)
	{
		return false;
	}
	APlayerCameraManager* Cam = BridgeAgentImpl::GetPIECameraManager();
	if (!Cam)
	{
		return false;
	}
	Cam->SetFOV(FOV);
	return true;
}

bool UUnrealBridgeGameplayLibrary::UnlockCameraFOV()
{
	APlayerCameraManager* Cam = BridgeAgentImpl::GetPIECameraManager();
	if (!Cam)
	{
		return false;
	}
	Cam->UnlockFOV();
	return true;
}

// ─── Character movement tuning ─────────────────────────────────────────

namespace BridgeAgentImpl
{
	/**
	 * 从显式选择的世界读取玩家角色移动组件，不在辅助函数内隐藏 world policy。
	 * Reads character movement from an explicitly selected world so the helper cannot hide world policy.
	 */
	static UCharacterMovementComponent* GetCharacterMovement(UWorld* World)
	{
		APawn* Pawn = GetPlayerPawn(World);
		if (ACharacter* Char = Cast<ACharacter>(Pawn))
		{
			return Char->GetCharacterMovement();
		}
		return nullptr;
	}
}

float UUnrealBridgeGameplayLibrary::GetPawnMaxWalkSpeed()
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	UCharacterMovementComponent* Move = BridgeAgentImpl::GetCharacterMovement(World);
	return Move ? Move->MaxWalkSpeed : -1.0f;
}

bool UUnrealBridgeGameplayLibrary::SetPawnMaxWalkSpeed(float Speed)
{
	if (Speed < 0.0f)
	{
		return false;
	}
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	UCharacterMovementComponent* Move = BridgeAgentImpl::GetCharacterMovement(World);
	if (!Move)
	{
		return false;
	}
	Move->MaxWalkSpeed = Speed;
	return true;
}

bool UUnrealBridgeGameplayLibrary::SetPawnGravityScale(float Scale)
{
	if (Scale < 0.0f)
	{
		return false;
	}
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	UCharacterMovementComponent* Move = BridgeAgentImpl::GetCharacterMovement(World);
	if (!Move)
	{
		return false;
	}
	Move->GravityScale = Scale;
	return true;
}

float UUnrealBridgeGameplayLibrary::GetPawnSpeed()
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn)
	{
		return -1.0f;
	}
	return Pawn->GetVelocity().Size();
}

bool UUnrealBridgeGameplayLibrary::GetPawnCapabilities(
	float& JumpZVelocity, float& MaxWalkSpeed,
	float& MaxStepHeight, float& WalkableFloorAngleDeg,
	float& CapsuleRadius, float& CapsuleHalfHeight, float& CrouchedHalfHeight,
	bool& bCanCrouch, bool& bCanJump)
{
	JumpZVelocity = 0.0f;
	MaxWalkSpeed = 0.0f;
	MaxStepHeight = 0.0f;
	WalkableFloorAngleDeg = 0.0f;
	CapsuleRadius = 0.0f;
	CapsuleHalfHeight = 0.0f;
	CrouchedHalfHeight = 0.0f;
	bCanCrouch = false;
	bCanJump = false;

	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	ACharacter* Char = Cast<ACharacter>(BridgeAgentImpl::GetPlayerPawn(World));
	if (!Char)
	{
		return false;
	}
	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		JumpZVelocity = Move->JumpZVelocity;
		MaxWalkSpeed = Move->MaxWalkSpeed;
		MaxStepHeight = Move->MaxStepHeight;
		WalkableFloorAngleDeg = Move->GetWalkableFloorAngle();
		CrouchedHalfHeight = Move->GetCrouchedHalfHeight();
		bCanCrouch = Move->GetNavAgentPropertiesRef().bCanCrouch;
		bCanJump = Move->GetNavAgentPropertiesRef().bCanJump;
	}
	if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
	return true;
}

bool UUnrealBridgeGameplayLibrary::SimulateJumpArc(
	const FVector& StartLocation, const FVector& InitialVelocity,
	float MaxTime, float StepDt, float MaxPathLength,
	FVector& OutLandLocation, FString& OutLandActorLabel)
{
	OutLandLocation = StartLocation;
	OutLandActorLabel.Reset();

	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return false;
	}
	const float Dt = FMath::Max(StepDt, 0.01f);
	const float TimeBudget = FMath::Max(MaxTime, Dt);
	const float PathBudget = FMath::Max(MaxPathLength, 0.0f);
	const float Gravity = World->GetGravityZ();      // typically -980
	FVector Pos = StartLocation;
	FVector Vel = InitialVelocity;
	float PathTravelled = 0.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeJumpArc), /*bTraceComplex*/ true);
	// Exclude the pawn itself so the trace doesn't immediately hit our capsule.
	if (APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World))
	{
		Params.AddIgnoredActor(Pawn);
	}
	for (float T = 0.0f; T < TimeBudget; T += Dt)
	{
		const FVector NextPos = Pos + Vel * Dt + FVector(0, 0, 0.5f * Gravity * Dt * Dt);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Pos, NextPos, ECC_Visibility, Params))
		{
			OutLandLocation = Hit.ImpactPoint;
			if (AActor* A = Hit.GetActor())
			{
				OutLandActorLabel = A->GetActorLabel();
			}
			return true;
		}
		Vel.Z += Gravity * Dt;
		PathTravelled += (NextPos - Pos).Size();
		Pos = NextPos;
		if (PathBudget > 0.0f && PathTravelled >= PathBudget)
		{
			break;
		}
	}
	OutLandLocation = Pos;
	return false;
}

// ─── PIE runtime spawn/destroy + query ─────────────────────────────────

namespace BridgeAgentImpl
{
	/** Resolve a class path, trying `_C` suffix for bare Blueprint paths. */
	static UClass* LoadActorClass(const FString& ClassPath)
	{
		if (ClassPath.IsEmpty()) return nullptr;
		if (UClass* Cls = LoadObject<UClass>(nullptr, *ClassPath))
		{
			return Cls;
		}
		// Blueprint classes are typically stored as `Pkg.BP_Foo_C` — if the
		// caller passed the bare path, append `_C`.
		if (!ClassPath.EndsWith(TEXT("_C")))
		{
			const FString WithC = ClassPath + TEXT("_C");
			if (UClass* Cls = LoadObject<UClass>(nullptr, *WithC))
			{
				return Cls;
			}
		}
		return nullptr;
	}
}

FString UUnrealBridgeGameplayLibrary::SpawnActorInPIE(const FString& ClassPath, const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return FString();
	}
	UClass* Cls = BridgeAgentImpl::LoadActorClass(ClassPath);
	if (!Cls || !Cls->IsChildOf(AActor::StaticClass()))
	{
		return FString();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* NewActor = World->SpawnActor<AActor>(Cls, Location, Rotation, Params);
	return NewActor ? NewActor->GetFName().ToString() : FString();
}

bool UUnrealBridgeGameplayLibrary::DestroyActorInPIE(const FString& ActorName)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	AActor* Target = BridgeAgentImpl::FindPIEActor(World, ActorName);
	if (!Target)
	{
		return false;
	}
	return World->DestroyActor(Target);
}

bool UUnrealBridgeGameplayLibrary::GetPIEActorLocation(const FString& ActorName, FVector& OutLocation)
{
	OutLocation = FVector::ZeroVector;
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	AActor* Target = BridgeAgentImpl::FindPIEActor(World, ActorName);
	if (!Target)
	{
		return false;
	}
	OutLocation = Target->GetActorLocation();
	return true;
}

// ─── Time dilation control ─────────────────────────────────────────────

float UUnrealBridgeGameplayLibrary::GetGlobalTimeDilation()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return 1.0f;
	}
	AWorldSettings* WS = World->GetWorldSettings();
	return WS ? WS->TimeDilation : 1.0f;
}

bool UUnrealBridgeGameplayLibrary::SetGlobalTimeDilation(float Scale)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return false;
	}
	const float Clamped = FMath::Clamp(Scale, 0.0001f, 20.0f);
	if (Scale <= 0.0f)
	{
		return false;
	}
	UGameplayStatics::SetGlobalTimeDilation(World, Clamped);
	return true;
}

float UUnrealBridgeGameplayLibrary::GetActorTimeDilation(const FString& ActorName)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	AActor* Target = BridgeAgentImpl::FindPIEActor(World, ActorName);
	if (!Target)
	{
		return -1.0f;
	}
	return Target->CustomTimeDilation;
}

bool UUnrealBridgeGameplayLibrary::SetActorTimeDilation(const FString& ActorName, float Scale)
{
	if (Scale <= 0.0f)
	{
		return false;
	}
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	AActor* Target = BridgeAgentImpl::FindPIEActor(World, ActorName);
	if (!Target)
	{
		return false;
	}
	Target->CustomTimeDilation = FMath::Clamp(Scale, 0.0001f, 20.0f);
	return true;
}

// ─── Audio + damage proxies ────────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::PlaySound2D(const FString& SoundAssetPath, float VolumeMultiplier, float PitchMultiplier)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return false;
	}
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundAssetPath);
	if (!Sound)
	{
		return false;
	}
	UGameplayStatics::PlaySound2D(World, Sound, VolumeMultiplier, PitchMultiplier);
	return true;
}

bool UUnrealBridgeGameplayLibrary::PlaySoundAtLocation(const FString& SoundAssetPath, const FVector& Location, float VolumeMultiplier, float PitchMultiplier)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return false;
	}
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundAssetPath);
	if (!Sound)
	{
		return false;
	}
	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, VolumeMultiplier, PitchMultiplier);
	return true;
}

float UUnrealBridgeGameplayLibrary::ApplyDamageToActor(const FString& TargetActorName, float DamageAmount)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	AActor* Target = BridgeAgentImpl::FindPIEActor(World, TargetActorName);
	if (!Target)
	{
		return -1.0f;
	}
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	AController* Instigator = (Pawn ? Pawn->GetController() : nullptr);
	return UGameplayStatics::ApplyDamage(
		Target, DamageAmount, Instigator, Pawn, UDamageType::StaticClass());
}

// ─── Multi-pawn + controller queries ───────────────────────────────────

TArray<FString> UUnrealBridgeGameplayLibrary::GetAllPawns()
{
	TArray<FString> Out;
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return Out;
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (APawn* P = *It)
		{
			Out.Add(P->GetFName().ToString());
		}
	}
	return Out;
}

TArray<FString> UUnrealBridgeGameplayLibrary::GetAIPawns()
{
	TArray<FString> Out;
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return Out;
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* P = *It;
		if (!P) continue;
		AController* C = P->GetController();
		if (!C) continue;
		// Identify AI controllers by class-name prefix rather than a hard
		// AAIController include — avoids pulling the AIModule into the
		// plugin's public build deps.
		if (C->GetClass()->GetName().Contains(TEXT("AIController")))
		{
			Out.Add(P->GetFName().ToString());
		}
	}
	return Out;
}

FString UUnrealBridgeGameplayLibrary::GetActorController(const FString& ActorName)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	AActor* A = BridgeAgentImpl::FindPIEActor(World, ActorName);
	APawn* P = Cast<APawn>(A);
	if (!P || !P->GetController())
	{
		return FString();
	}
	return P->GetController()->GetClass()->GetName();
}

bool UUnrealBridgeGameplayLibrary::IsActorAIControlled(const FString& ActorName)
{
	const FString ClassName = GetActorController(ActorName);
	return !ClassName.IsEmpty() && ClassName.Contains(TEXT("AIController"));
}

// ─── On-screen debug + debug drawing ───────────────────────────────────

bool UUnrealBridgeGameplayLibrary::AddOnScreenDebugMessage(const FString& Message, float DurationSeconds, float R, float G, float B)
{
	if (!GEngine || Message.IsEmpty())
	{
		return false;
	}
	const float Dur = FMath::Clamp(DurationSeconds, 0.1f, 60.0f);
	const FColor Color = FLinearColor(
		FMath::Clamp(R, 0.0f, 1.0f),
		FMath::Clamp(G, 0.0f, 1.0f),
		FMath::Clamp(B, 0.0f, 1.0f)).ToFColor(/*sRGB=*/ true);
	GEngine->AddOnScreenDebugMessage(/*Key=*/ INDEX_NONE, Dur, Color, Message);
	return true;
}

bool UUnrealBridgeGameplayLibrary::ClearOnScreenDebugMessages()
{
	if (!GEngine)
	{
		return false;
	}
	GEngine->ClearOnScreenDebugMessages();
	return true;
}

bool UUnrealBridgeGameplayLibrary::DrawDebugLine(const FVector& Start, const FVector& End, float Thickness, float DurationSeconds)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return false;
	}
	// UE convention: bPersistent=true stays until FlushPersistentDebugLines
	// (LifeTime ignored); bPersistent=false + LifeTime>0 decays after N sec.
	// Treat negative DurationSeconds as the explicit "persistent" opt-in.
	::DrawDebugLine(World, Start, End, FColor::Yellow,
		/*bPersistent=*/ DurationSeconds < 0.0f, DurationSeconds,
		/*DepthPriority=*/ 0, FMath::Max(Thickness, 0.0f));
	return true;
}

bool UUnrealBridgeGameplayLibrary::DrawDebugSphereAt(const FVector& Center, float Radius, float Thickness, float DurationSeconds)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return false;
	}
	::DrawDebugSphere(World, Center, FMath::Max(Radius, 0.0f), /*Segments=*/ 16, FColor::Yellow,
		/*bPersistent=*/ DurationSeconds < 0.0f, DurationSeconds,
		/*DepthPriority=*/ 0, FMath::Max(Thickness, 0.0f));
	return true;
}

// ─── Enhanced Input mapping contexts ───────────────────────────────────

namespace BridgeAgentImpl
{
	static UEnhancedInputLocalPlayerSubsystem* GetEILocalPlayerSub()
	{
		UWorld* World = GetLocalPlayerPIEWorld();
		if (!World) return nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->GetLocalPlayer()) return nullptr;
		return PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
}

bool UUnrealBridgeGameplayLibrary::AddMappingContext(const FString& MappingContextPath, int32 Priority)
{
	UEnhancedInputLocalPlayerSubsystem* Sub = BridgeAgentImpl::GetEILocalPlayerSub();
	if (!Sub)
	{
		return false;
	}
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	if (!IMC)
	{
		return false;
	}
	Sub->AddMappingContext(IMC, Priority);
	return true;
}

bool UUnrealBridgeGameplayLibrary::RemoveMappingContext(const FString& MappingContextPath)
{
	UEnhancedInputLocalPlayerSubsystem* Sub = BridgeAgentImpl::GetEILocalPlayerSub();
	if (!Sub)
	{
		return false;
	}
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	if (!IMC)
	{
		return false;
	}
	Sub->RemoveMappingContext(IMC);
	return true;
}

bool UUnrealBridgeGameplayLibrary::IsMappingContextActive(const FString& MappingContextPath)
{
	UEnhancedInputLocalPlayerSubsystem* Sub = BridgeAgentImpl::GetEILocalPlayerSub();
	if (!Sub)
	{
		return false;
	}
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	return IMC && Sub->HasMappingContext(IMC);
}

FString UUnrealBridgeGameplayLibrary::GetInputActionValueType(const FString& InputActionPath)
{
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA)
	{
		return FString();
	}
	switch (IA->ValueType)
	{
	case EInputActionValueType::Boolean: return TEXT("Bool");
	case EInputActionValueType::Axis1D:  return TEXT("Axis1D");
	case EInputActionValueType::Axis2D:  return TEXT("Axis2D");
	case EInputActionValueType::Axis3D:  return TEXT("Axis3D");
	default:                             return TEXT("Unknown");
	}
}

// ─── Extended debug drawing ────────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::DrawDebugBoxAt(const FVector& Center, const FVector& Extent, float Thickness, float DurationSeconds)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return false;
	::DrawDebugBox(World, Center, Extent, FColor::Yellow,
		/*bPersistent=*/ DurationSeconds < 0.0f, DurationSeconds,
		/*DepthPriority=*/ 0, FMath::Max(Thickness, 0.0f));
	return true;
}

bool UUnrealBridgeGameplayLibrary::DrawDebugArrow(const FVector& Start, const FVector& End, float ArrowSize, float DurationSeconds)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return false;
	::DrawDebugDirectionalArrow(World, Start, End, FMath::Max(ArrowSize, 1.0f),
		FColor::Yellow, /*bPersistent=*/ DurationSeconds < 0.0f, DurationSeconds);
	return true;
}

bool UUnrealBridgeGameplayLibrary::DrawDebugString(const FString& Text, const FVector& Location, float DurationSeconds)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World || Text.IsEmpty()) return false;
	::DrawDebugString(World, Location, Text, /*TestBaseActor=*/ nullptr,
		FColor::Yellow, DurationSeconds, /*bDrawShadow=*/ true);
	return true;
}

bool UUnrealBridgeGameplayLibrary::FlushPersistentDebugDraws()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return false;
	::FlushPersistentDebugLines(World);
	return true;
}

// ─── Pawn basis vectors + distance ─────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::GetPawnForwardVector(FVector& OutForward)
{
	OutForward = FVector::ForwardVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn) return false;
	OutForward = Pawn->GetActorForwardVector();
	return true;
}

bool UUnrealBridgeGameplayLibrary::GetPawnRightVector(FVector& OutRight)
{
	OutRight = FVector::RightVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn) return false;
	OutRight = Pawn->GetActorRightVector();
	return true;
}

bool UUnrealBridgeGameplayLibrary::GetPawnUpVector(FVector& OutUp)
{
	OutUp = FVector::UpVector;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn) return false;
	OutUp = Pawn->GetActorUpVector();
	return true;
}

float UUnrealBridgeGameplayLibrary::GetDistanceToPawn(const FVector& Location)
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn) return -1.0f;
	return FVector::Dist(Pawn->GetActorLocation(), Location);
}

// ─── PIE clock / counter queries ───────────────────────────────────────

int64 UUnrealBridgeGameplayLibrary::GetPIEFrameNumber()
{
	return static_cast<int64>(GFrameCounter);
}

float UUnrealBridgeGameplayLibrary::GetPIEDeltaSeconds()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	return World ? World->GetDeltaSeconds() : 0.0f;
}

int32 UUnrealBridgeGameplayLibrary::GetPIENumPlayers()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return 0;
	return UGameplayStatics::GetNumPlayerControllers(World);
}

int32 UUnrealBridgeGameplayLibrary::GetPIENumAIControllers()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return 0;
	int32 Count = 0;
	for (TActorIterator<AController> It(World); It; ++It)
	{
		AController* C = *It;
		if (C && C->GetClass()->GetName().Contains(TEXT("AIController")))
		{
			++Count;
		}
	}
	return Count;
}

// ─── Physics impulse / force ───────────────────────────────────────────

namespace BridgeAgentImpl
{
	static UPrimitiveComponent* GetPIEPrimitive(const FString& ActorName)
	{
		UWorld* World = GetPIEWorld();
		AActor* A = FindPIEActor(World, ActorName);
		if (!A) return nullptr;
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(A->GetRootComponent()))
		{
			return Root;
		}
		return A->FindComponentByClass<UPrimitiveComponent>();
	}
}

bool UUnrealBridgeGameplayLibrary::AddImpulseToPIEActor(const FString& ActorName, const FVector& Impulse, bool bVelocityChange)
{
	UPrimitiveComponent* Prim = BridgeAgentImpl::GetPIEPrimitive(ActorName);
	if (!Prim) return false;
	Prim->AddImpulse(Impulse, NAME_None, bVelocityChange);
	return true;
}

bool UUnrealBridgeGameplayLibrary::AddForceToPIEActor(const FString& ActorName, const FVector& Force)
{
	UPrimitiveComponent* Prim = BridgeAgentImpl::GetPIEPrimitive(ActorName);
	if (!Prim) return false;
	Prim->AddForce(Force);
	return true;
}

bool UUnrealBridgeGameplayLibrary::WakePIEActorPhysics(const FString& ActorName)
{
	UPrimitiveComponent* Prim = BridgeAgentImpl::GetPIEPrimitive(ActorName);
	if (!Prim) return false;
	Prim->WakeAllRigidBodies();
	return true;
}

bool UUnrealBridgeGameplayLibrary::GetPIEActorLinearVelocity(const FString& ActorName, FVector& OutVelocity)
{
	OutVelocity = FVector::ZeroVector;
	UPrimitiveComponent* Prim = BridgeAgentImpl::GetPIEPrimitive(ActorName);
	if (!Prim) return false;
	OutVelocity = Prim->GetPhysicsLinearVelocity();
	return true;
}

// ─── Camera shake control ──────────────────────────────────────────────

namespace BridgeAgentImpl
{
	/** Resolve a shake class path, accepting Blueprint or native. */
	static TSubclassOf<UCameraShakeBase> LoadShakeClass(const FString& ClassPath)
	{
		if (ClassPath.IsEmpty()) return nullptr;
		if (UClass* Cls = LoadObject<UClass>(nullptr, *ClassPath))
		{
			if (Cls->IsChildOf(UCameraShakeBase::StaticClass()))
			{
				return Cls;
			}
		}
		// Try `_C` suffix for bare BP paths.
		if (!ClassPath.EndsWith(TEXT("_C")))
		{
			const FString WithC = ClassPath + TEXT("_C");
			if (UClass* Cls = LoadObject<UClass>(nullptr, *WithC))
			{
				if (Cls->IsChildOf(UCameraShakeBase::StaticClass()))
				{
					return Cls;
				}
			}
		}
		return nullptr;
	}
}

bool UUnrealBridgeGameplayLibrary::StartCameraShake(const FString& ShakeClassPath, float Scale)
{
	APlayerCameraManager* Cam = BridgeAgentImpl::GetPIECameraManager();
	if (!Cam) return false;
	TSubclassOf<UCameraShakeBase> Cls = BridgeAgentImpl::LoadShakeClass(ShakeClassPath);
	if (!Cls) return false;
	APlayerController* PC = Cam->GetOwningPlayerController();
	if (!PC) return false;
	PC->ClientStartCameraShake(Cls, FMath::Max(Scale, 0.0f));
	return true;
}

bool UUnrealBridgeGameplayLibrary::StopCameraShakeByClass(const FString& ShakeClassPath, bool bImmediately)
{
	APlayerCameraManager* Cam = BridgeAgentImpl::GetPIECameraManager();
	if (!Cam) return false;
	TSubclassOf<UCameraShakeBase> Cls = BridgeAgentImpl::LoadShakeClass(ShakeClassPath);
	if (!Cls) return false;
	APlayerController* PC = Cam->GetOwningPlayerController();
	if (!PC) return false;
	PC->ClientStopCameraShake(Cls, bImmediately);
	return true;
}

bool UUnrealBridgeGameplayLibrary::StopAllCameraShakes(bool bImmediately)
{
	APlayerCameraManager* Cam = BridgeAgentImpl::GetPIECameraManager();
	if (!Cam) return false;
	Cam->StopAllCameraShakes(bImmediately);
	return true;
}

bool UUnrealBridgeGameplayLibrary::PlayWorldCameraShake(const FString& ShakeClassPath, const FVector& Epicenter, float InnerRadius, float OuterRadius, float ScaleMultiplier)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return false;
	TSubclassOf<UCameraShakeBase> Cls = BridgeAgentImpl::LoadShakeClass(ShakeClassPath);
	if (!Cls) return false;
	UGameplayStatics::PlayWorldCameraShake(
		World, Cls, Epicenter,
		FMath::Max(InnerRadius, 0.0f), FMath::Max(OuterRadius, 0.0f),
		/*Falloff=*/ 1.0f, /*bOrientShakeTowardsEpicenter=*/ false);
	return true;
}

// ─── Player pawn / start / respawn ─────────────────────────────────────

namespace BridgeAgentImpl
{
	/** Find first APlayerStart in PIE world, falling back to editor world. */
	static AActor* FindPlayerStart()
	{
		UWorld* World = GetPIEWorld();
		if (!World)
		{
			World = GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
		}
		if (!World) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (A && A->GetClass()->GetName().Contains(TEXT("PlayerStart")))
			{
				return A;
			}
		}
		return nullptr;
	}
}

FString UUnrealBridgeGameplayLibrary::GetPlayerPawnActorName()
{
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	return Pawn ? Pawn->GetFName().ToString() : FString();
}

USkeletalMeshComponent* UUnrealBridgeGameplayLibrary::GetPlayerSkeletalMeshComponent(const FString& ComponentName)
{
	return BridgeAgentImpl::GetPlayerSkeletalMeshComponent(BridgeAgentImpl::GetLocalPlayerPIEWorld(), ComponentName);
}

UAnimInstance* UUnrealBridgeGameplayLibrary::GetPlayerAnimInstance(const FString& ComponentName)
{
	if (USkeletalMeshComponent* MeshComponent = GetPlayerSkeletalMeshComponent(ComponentName))
	{
		return MeshComponent->GetAnimInstance();
	}
	return nullptr;
}

FString UUnrealBridgeGameplayLibrary::GetPlayerStartActorName()
{
	AActor* Start = BridgeAgentImpl::FindPlayerStart();
	return Start ? Start->GetFName().ToString() : FString();
}

bool UUnrealBridgeGameplayLibrary::GetPlayerStartTransform(FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	AActor* Start = BridgeAgentImpl::FindPlayerStart();
	if (!Start) return false;
	OutLocation = Start->GetActorLocation();
	OutRotation = Start->GetActorRotation();
	return true;
}

bool UUnrealBridgeGameplayLibrary::RespawnPlayerPawn()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn) return false;
	AActor* Start = BridgeAgentImpl::FindPlayerStart();
	if (!Start) return false;

	// Reuse the public TeleportPawn path so physics / controller snap are consistent.
	return TeleportPawn(Start->GetActorLocation(), Start->GetActorRotation(),
		/*bSnapController=*/ true, /*bStopVelocity=*/ true);
}

// ─── Runtime pause + game class queries ────────────────────────────────

bool UUnrealBridgeGameplayLibrary::PauseGame(bool bPaused)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return false;
	return UGameplayStatics::SetGamePaused(World, bPaused);
}

bool UUnrealBridgeGameplayLibrary::IsGamePaused()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return false;
	return UGameplayStatics::IsGamePaused(World);
}

FString UUnrealBridgeGameplayLibrary::GetGameModeClassName()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return FString();
	AGameModeBase* GM = UGameplayStatics::GetGameMode(World);
	return GM ? GM->GetClass()->GetName() : FString();
}

FString UUnrealBridgeGameplayLibrary::GetGameStateClassName()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World) return FString();
	AGameStateBase* GS = UGameplayStatics::GetGameState(World);
	return GS ? GS->GetClass()->GetName() : FString();
}

int32 UUnrealBridgeGameplayLibrary::ApplyRadialDamage(const FVector& Origin, float DamageAmount, float InnerRadius, float OuterRadius)
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return 0;
	}
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	AController* Instigator = (Pawn ? Pawn->GetController() : nullptr);

	// Collect actors within OuterRadius first so we can report the count.
	TArray<AActor*> Affected;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == Pawn) continue;
		if (FVector::Dist(A->GetActorLocation(), Origin) <= FMath::Max(OuterRadius, 0.0f))
		{
			Affected.Add(A);
		}
	}
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		World, DamageAmount, /*MinDamage=*/ 0.0f, Origin,
		FMath::Max(InnerRadius, 0.0f), FMath::Max(OuterRadius, 0.0f),
		/*DamageFalloff=*/ 1.0f, UDamageType::StaticClass(),
		/*IgnoreActors=*/ {}, Pawn, Instigator);
	return Affected.Num();
}

TArray<FString> UUnrealBridgeGameplayLibrary::FindPIEActorsByClass(const FString& ClassPath)
{
	TArray<FString> Out;
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		return Out;
	}
	UClass* Cls = BridgeAgentImpl::LoadActorClass(ClassPath);
	if (!Cls || !Cls->IsChildOf(AActor::StaticClass()))
	{
		return Out;
	}
	for (TActorIterator<AActor> It(World, Cls); It; ++It)
	{
		if (AActor* A = *It)
		{
			Out.Add(A->GetFName().ToString());
		}
	}
	return Out;
}

bool UUnrealBridgeGameplayLibrary::GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	if (!World)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}
	PC->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

FString UUnrealBridgeGameplayLibrary::GetActorAtScreenPosition(float NormalizedX, float NormalizedY, float MaxDistance)
{
	FVector Origin, Direction;
	if (!DeprojectScreenToWorld(NormalizedX, NormalizedY, Origin, Direction))
	{
		return FString();
	}
	UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	const FVector End = Origin + Direction * FMath::Max(MaxDistance, 0.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BridgeScreenHit), /*bTraceComplex=*/ false);
	if (Pawn) Params.AddIgnoredActor(Pawn);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
	{
		return FString();
	}
	AActor* A = Hit.GetActor();
	return A ? A->GetFName().ToString() : FString();
}

bool UUnrealBridgeGameplayLibrary::PressKey(const FString& KeyName, bool bPressed)
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("PressKey: invalid key name '%s'"), *KeyName);
		return false;
	}

	FSlateApplication& Slate = FSlateApplication::Get();

	// Build a synthetic key event targeting the currently focused widget.
	const uint32 CharCode = 0;
	const uint32 KeyCode  = 0;
	const FModifierKeysState ModKeys;

	if (bPressed)
	{
		FKeyEvent KeyEvent(Key, ModKeys, Slate.GetUserIndexForKeyboard(), /*bIsRepeat=*/ false, CharCode, KeyCode);
		Slate.ProcessKeyDownEvent(KeyEvent);
	}
	else
	{
		FKeyEvent KeyEvent(Key, ModKeys, Slate.GetUserIndexForKeyboard(), /*bIsRepeat=*/ false, CharCode, KeyCode);
		Slate.ProcessKeyUpEvent(KeyEvent);
	}

	return true;
}

// ─── Enhanced Input authoring (IA/IMC enumeration + IMC mappings) ───

namespace BridgeInputAuthoringImpl
{
	/** Run an AssetRegistry query for a given UClass, optionally filtered by
	 *  package-path prefix and capped at MaxResults. Returns asset paths. */
	TArray<FString> ListAssetsOfClass(UClass* Class, const FString& PathFilter, int32 MaxResults)
	{
		TArray<FString> Result;
		if (!Class) return Result;

		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.bIncludeOnlyOnDiskAssets = false;

		TArray<FAssetData> Assets;
		AR.GetAssets(Filter, Assets);

		for (const FAssetData& A : Assets)
		{
			const FString PackagePath = A.PackageName.ToString();
			if (!PathFilter.IsEmpty() && !PackagePath.StartsWith(PathFilter)) continue;
			Result.Add(A.GetSoftObjectPath().ToString());
			if (MaxResults > 0 && Result.Num() >= MaxResults) break;
		}
		Result.Sort();
		return Result;
	}
}

TArray<FString> UUnrealBridgeGameplayLibrary::ListInputActions(
	const FString& ContentPathFilter, int32 MaxResults)
{
	return BridgeInputAuthoringImpl::ListAssetsOfClass(
		UInputAction::StaticClass(), ContentPathFilter, MaxResults);
}

TArray<FString> UUnrealBridgeGameplayLibrary::ListInputMappingContexts(
	const FString& ContentPathFilter, int32 MaxResults)
{
	return BridgeInputAuthoringImpl::ListAssetsOfClass(
		UInputMappingContext::StaticClass(), ContentPathFilter, MaxResults);
}

TArray<FBridgeIMCMapping> UUnrealBridgeGameplayLibrary::GetInputMappingContextMappings(
	const FString& MappingContextPath)
{
	TArray<FBridgeIMCMapping> Result;

	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	if (!IMC)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("GetInputMappingContextMappings: failed to load IMC '%s'"), *MappingContextPath);
		return Result;
	}

	for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
	{
		FBridgeIMCMapping Out;
		if (const UInputAction* IA = M.Action)
		{
			Out.ActionPath = IA->GetPathName();
			Out.ActionName = IA->GetName();
		}
		Out.KeyName = M.Key.ToString();

		for (const TObjectPtr<UInputTrigger>& T : M.Triggers)
		{
			if (T) Out.TriggerClasses.Add(T->GetClass()->GetName());
		}
		for (const TObjectPtr<UInputModifier>& Mod : M.Modifiers)
		{
			if (Mod) Out.ModifierClasses.Add(Mod->GetClass()->GetName());
		}
		Result.Add(MoveTemp(Out));
	}
	return Result;
}

bool UUnrealBridgeGameplayLibrary::AddIAMappingToIMC(
	const FString& MappingContextPath,
	const FString& InputActionPath,
	const FString& KeyName)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	if (!IMC)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddIAMappingToIMC: failed to load IMC '%s'"), *MappingContextPath);
		return false;
	}
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddIAMappingToIMC: failed to load IA '%s'"), *InputActionPath);
		return false;
	}
	if (KeyName.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("AddIAMappingToIMC: KeyName is empty"));
		return false;
	}

	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddIAMappingToIMC: '%s' is not a recognised FKey"), *KeyName);
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("Add IMC Mapping")));
	IMC->Modify();
	IMC->MapKey(IA, Key);
	IMC->MarkPackageDirty();
	return true;
}

bool UUnrealBridgeGameplayLibrary::RemoveIAMappingFromIMC(
	const FString& MappingContextPath,
	const FString& InputActionPath,
	const FString& KeyName)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	if (!IMC)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("RemoveIAMappingFromIMC: failed to load IMC '%s'"), *MappingContextPath);
		return false;
	}
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("RemoveIAMappingFromIMC: failed to load IA '%s'"), *InputActionPath);
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("Remove IMC Mapping")));
	IMC->Modify();
	if (KeyName.IsEmpty())
	{
		IMC->UnmapAllKeysFromAction(IA);
	}
	else
	{
		const FKey Key(*KeyName);
		if (!Key.IsValid())
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("RemoveIAMappingFromIMC: '%s' is not a recognised FKey"), *KeyName);
			return false;
		}
		IMC->UnmapKey(IA, Key);
	}
	IMC->MarkPackageDirty();
	return true;
}

// ─── Asset creation: IA / IMC (A1, A2) ──────────────────────────────

namespace BridgeInputAuthoringImpl
{
	/** Map "Boolean"/"Bool" → Boolean; "Axis1D" → Axis1D; case-insensitive. */
	static EInputActionValueType ParseValueType(const FString& Raw)
	{
		const FString N = Raw.TrimStartAndEnd().ToLower();
		if (N == TEXT("axis1d") || N == TEXT("float") || N == TEXT("1d")) return EInputActionValueType::Axis1D;
		if (N == TEXT("axis2d") || N == TEXT("vector2d") || N == TEXT("2d")) return EInputActionValueType::Axis2D;
		if (N == TEXT("axis3d") || N == TEXT("vector") || N == TEXT("3d")) return EInputActionValueType::Axis3D;
		// "boolean" / "bool" / anything unrecognised
		return EInputActionValueType::Boolean;
	}

	/**
	 * Split "/Game/Foo/MyAsset" into ("/Game/Foo/MyAsset", "MyAsset"). Validates
	 * that the path starts with "/" (a mount-rooted package path). Returns false
	 * if the path is malformed (no leading slash, trailing slash, or empty name).
	 */
	static bool SplitPackagePath(const FString& InPath, FString& OutPackageName, FString& OutAssetName)
	{
		if (InPath.IsEmpty() || !InPath.StartsWith(TEXT("/")) || InPath.EndsWith(TEXT("/")))
		{
			return false;
		}
		// Strip any ".AssetName" suffix the caller may have included.
		FString Stripped = InPath;
		int32 DotIdx;
		if (Stripped.FindChar(TCHAR('.'), DotIdx))
		{
			Stripped.LeftInline(DotIdx);
		}
		int32 SlashIdx;
		if (!Stripped.FindLastChar(TCHAR('/'), SlashIdx))
		{
			return false;
		}
		OutAssetName = Stripped.Mid(SlashIdx + 1);
		if (OutAssetName.IsEmpty()) return false;
		OutPackageName = Stripped;
		return true;
	}

	/** Create a new asset of UClass at PackagePath. Returns nullptr on failure. */
	template <typename TAsset>
	static TAsset* CreateInputAsset(const FString& PackagePath, FString& OutObjectPath)
	{
		FString PackageName, AssetName;
		if (!SplitPackagePath(PackagePath, PackageName, AssetName))
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("CreateInputAsset: malformed package path '%s'"), *PackagePath);
			return nullptr;
		}

		if (FPackageName::DoesPackageExist(PackageName))
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("CreateInputAsset: package already exists at '%s'"), *PackageName);
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("CreateInputAsset: CreatePackage failed for '%s'"), *PackageName);
			return nullptr;
		}

		TAsset* Asset = NewObject<TAsset>(
			Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
		if (!Asset) return nullptr;

		FAssetRegistryModule::AssetCreated(Asset);
		Asset->MarkPackageDirty();

		OutObjectPath = PackageName + TEXT(".") + AssetName;
		return Asset;
	}
}

FString UUnrealBridgeGameplayLibrary::CreateInputAction(
	const FString& PackagePath, const FString& ValueType,
	const FString& Description, bool bSave)
{
	FString ObjectPath;
	UInputAction* IA = BridgeInputAuthoringImpl::CreateInputAsset<UInputAction>(PackagePath, ObjectPath);
	if (!IA) return FString();

	IA->ValueType = BridgeInputAuthoringImpl::ParseValueType(ValueType);
	if (!Description.IsEmpty())
	{
		IA->ActionDescription = FText::FromString(Description);
	}

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ IA->GetOutermost() }, /*bOnlyDirty*/ false);
	}
	return ObjectPath;
}

FString UUnrealBridgeGameplayLibrary::CreateInputMappingContext(
	const FString& PackagePath, const FString& Description, bool bSave)
{
	FString ObjectPath;
	UInputMappingContext* IMC = BridgeInputAuthoringImpl::CreateInputAsset<UInputMappingContext>(PackagePath, ObjectPath);
	if (!IMC) return FString();

	if (!Description.IsEmpty())
	{
		IMC->ContextDescription = FText::FromString(Description);
	}

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ IMC->GetOutermost() }, /*bOnlyDirty*/ false);
	}
	return ObjectPath;
}

// ─── Trigger / Modifier instance editing on IA (A4, A6) ─────────────

namespace BridgeInputAuthoringImpl
{
	/**
	 * Resolve a user-supplied class string to a UClass derived from BaseClass.
	 *
	 *   "Hold"                  → BaseClass-prefixed name (e.g. UInputTriggerHold)
	 *   "InputTriggerHold"      → resolved by short name
	 *   "/Script/EnhancedInput.InputTriggerHold"  → full path
	 */
	static UClass* ResolveSubclass(UClass* BaseClass, const FString& Raw)
	{
		const FString Trim = Raw.TrimStartAndEnd();
		if (Trim.IsEmpty() || !BaseClass) return nullptr;

		// Path form
		if (Trim.StartsWith(TEXT("/")))
		{
			return LoadObject<UClass>(nullptr, *Trim);
		}

		// Try short name as-is first
		if (UClass* Direct = FindFirstObject<UClass>(*Trim, EFindFirstObjectOptions::None))
		{
			if (Direct->IsChildOf(BaseClass)) return Direct;
		}

		// Try with the base class's prefix-stripped name e.g. "UInputTrigger" → "Hold" → "UInputTriggerHold"
		const FString BaseShort = BaseClass->GetName(); // e.g. "InputTrigger" / "InputModifier"
		const FString Prefixed = BaseShort + Trim;
		if (UClass* WithPrefix = FindFirstObject<UClass>(*Prefixed, EFindFirstObjectOptions::None))
		{
			if (WithPrefix->IsChildOf(BaseClass)) return WithPrefix;
		}

		return nullptr;
	}

	/** Parse JSON object → assign each key as a UPROPERTY on Target. Returns true on success. */
	static bool ApplyJsonParamsToObject(UObject* Target, const FString& Json)
	{
		if (!Target || Json.IsEmpty() || Json == TEXT("{}")) return true;

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("ApplyJsonParamsToObject: malformed JSON: %s"), *Json);
			return false;
		}

		// Use FJsonObjectConverter to map JSON keys onto properties of this UObject.
		// Mirrors how UE imports default property values. CheckFlags=0 means no
		// property-flag filter; the relevant subset are EditAnywhere / Config /
		// public on the trigger/modifier classes.
		const bool bOk = FJsonObjectConverter::JsonObjectToUStruct(
			Root.ToSharedRef(), Target->GetClass(), Target,
			/*CheckFlags*/ 0, /*SkipFlags*/ 0);
		if (!bOk)
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("ApplyJsonParamsToObject: JsonObjectToUStruct returned false on %s"),
				*Target->GetClass()->GetName());
		}
		return bOk;
	}

	/** Resolve a possibly-negative index against array length. -1 = last. */
	static int32 NormalizeIndex(int32 Idx, int32 Len)
	{
		if (Idx < 0) Idx += Len;
		return (Idx >= 0 && Idx < Len) ? Idx : INDEX_NONE;
	}
}

int32 UUnrealBridgeGameplayLibrary::AddTriggerToIA(
	const FString& InputActionPath, const FString& TriggerClass,
	const FString& ParamsJson, bool bSave)
{
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddTriggerToIA: failed to load IA '%s'"), *InputActionPath);
		return -1;
	}

	UClass* Cls = BridgeInputAuthoringImpl::ResolveSubclass(UInputTrigger::StaticClass(), TriggerClass);
	if (!Cls || Cls->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddTriggerToIA: '%s' is not a concrete UInputTrigger subclass"), *TriggerClass);
		return -1;
	}

	UInputTrigger* Instance = NewObject<UInputTrigger>(IA, Cls, NAME_None, RF_Transactional);
	if (!Instance) return -1;

	if (!BridgeInputAuthoringImpl::ApplyJsonParamsToObject(Instance, ParamsJson))
	{
		// JSON failure is non-fatal — keep the instance with whatever properties
		// did stick. Caller already got a warning in the log.
	}

	IA->Modify();
	const int32 NewIdx = IA->Triggers.Add(Instance);

	FPropertyChangedEvent Evt(
		FindFProperty<FProperty>(IA->GetClass(), GET_MEMBER_NAME_CHECKED(UInputAction, Triggers)),
		EPropertyChangeType::ArrayAdd);
	IA->PostEditChangeProperty(Evt);
	IA->MarkPackageDirty();

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ IA->GetOutermost() }, /*bOnlyDirty*/ false);
	}
	return NewIdx;
}

int32 UUnrealBridgeGameplayLibrary::AddModifierToIA(
	const FString& InputActionPath, const FString& ModifierClass,
	const FString& ParamsJson, bool bSave)
{
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddModifierToIA: failed to load IA '%s'"), *InputActionPath);
		return -1;
	}

	UClass* Cls = BridgeInputAuthoringImpl::ResolveSubclass(UInputModifier::StaticClass(), ModifierClass);
	if (!Cls || Cls->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogUnrealBridgeAgent, Warning,
			TEXT("AddModifierToIA: '%s' is not a concrete UInputModifier subclass"), *ModifierClass);
		return -1;
	}

	UInputModifier* Instance = NewObject<UInputModifier>(IA, Cls, NAME_None, RF_Transactional);
	if (!Instance) return -1;

	BridgeInputAuthoringImpl::ApplyJsonParamsToObject(Instance, ParamsJson);

	IA->Modify();
	const int32 NewIdx = IA->Modifiers.Add(Instance);

	FPropertyChangedEvent Evt(
		FindFProperty<FProperty>(IA->GetClass(), GET_MEMBER_NAME_CHECKED(UInputAction, Modifiers)),
		EPropertyChangeType::ArrayAdd);
	IA->PostEditChangeProperty(Evt);
	IA->MarkPackageDirty();

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ IA->GetOutermost() }, /*bOnlyDirty*/ false);
	}
	return NewIdx;
}

bool UUnrealBridgeGameplayLibrary::RemoveTriggerFromIA(
	const FString& InputActionPath, int32 Index, bool bSave)
{
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return false;
	const int32 Norm = BridgeInputAuthoringImpl::NormalizeIndex(Index, IA->Triggers.Num());
	if (Norm == INDEX_NONE) return false;

	IA->Modify();
	IA->Triggers.RemoveAt(Norm);
	FPropertyChangedEvent Evt(
		FindFProperty<FProperty>(IA->GetClass(), GET_MEMBER_NAME_CHECKED(UInputAction, Triggers)),
		EPropertyChangeType::ArrayRemove);
	IA->PostEditChangeProperty(Evt);
	IA->MarkPackageDirty();

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ IA->GetOutermost() }, /*bOnlyDirty*/ false);
	}
	return true;
}

bool UUnrealBridgeGameplayLibrary::RemoveModifierFromIA(
	const FString& InputActionPath, int32 Index, bool bSave)
{
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return false;
	const int32 Norm = BridgeInputAuthoringImpl::NormalizeIndex(Index, IA->Modifiers.Num());
	if (Norm == INDEX_NONE) return false;

	IA->Modify();
	IA->Modifiers.RemoveAt(Norm);
	FPropertyChangedEvent Evt(
		FindFProperty<FProperty>(IA->GetClass(), GET_MEMBER_NAME_CHECKED(UInputAction, Modifiers)),
		EPropertyChangeType::ArrayRemove);
	IA->PostEditChangeProperty(Evt);
	IA->MarkPackageDirty();

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ IA->GetOutermost() }, /*bOnlyDirty*/ false);
	}
	return true;
}

// ─── Read APIs (full trigger/modifier dump + value-type filter) ──────

namespace BridgeInputAuthoringImpl
{
	static FString SerializeUObjectToJson(UObject* Obj)
	{
		if (!Obj) return FString();
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		FJsonObjectConverter::UStructToJsonObject(Obj->GetClass(), Obj, Out, /*CheckFlags*/ 0, /*SkipFlags*/ 0);
		FString S;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&S);
		FJsonSerializer::Serialize(Out, W);
		return S;
	}

	static FString ValueTypeToString(EInputActionValueType T)
	{
		switch (T)
		{
		case EInputActionValueType::Boolean: return TEXT("Boolean");
		case EInputActionValueType::Axis1D:  return TEXT("Axis1D");
		case EInputActionValueType::Axis2D:  return TEXT("Axis2D");
		case EInputActionValueType::Axis3D:  return TEXT("Axis3D");
		}
		return TEXT("Unknown");
	}

	static int32 FindMappingIndex(UInputMappingContext* IMC, const UInputAction* IA, FKey Key)
	{
		const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();
		for (int32 i = 0; i < Mappings.Num(); ++i)
		{
			if (Mappings[i].Action == IA && Mappings[i].Key == Key) return i;
		}
		return INDEX_NONE;
	}

	static FEnhancedActionKeyMapping* GetMutableMapping(UInputMappingContext* IMC, int32 Idx)
	{
		return const_cast<FEnhancedActionKeyMapping*>(&IMC->GetMappings()[Idx]);
	}
}

TArray<FBridgeInputComponentInstance> UUnrealBridgeGameplayLibrary::GetInputActionTriggersFull(
	const FString& InputActionPath)
{
	TArray<FBridgeInputComponentInstance> Out;
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return Out;
	for (UInputTrigger* T : IA->Triggers)
	{
		if (!T) continue;
		FBridgeInputComponentInstance E;
		E.ClassName = T->GetClass()->GetName();
		E.ClassPath = T->GetClass()->GetPathName();
		E.ParamsJson = BridgeInputAuthoringImpl::SerializeUObjectToJson(T);
		Out.Add(MoveTemp(E));
	}
	return Out;
}

TArray<FBridgeInputComponentInstance> UUnrealBridgeGameplayLibrary::GetInputActionModifiersFull(
	const FString& InputActionPath)
{
	TArray<FBridgeInputComponentInstance> Out;
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return Out;
	for (UInputModifier* M : IA->Modifiers)
	{
		if (!M) continue;
		FBridgeInputComponentInstance E;
		E.ClassName = M->GetClass()->GetName();
		E.ClassPath = M->GetClass()->GetPathName();
		E.ParamsJson = BridgeInputAuthoringImpl::SerializeUObjectToJson(M);
		Out.Add(MoveTemp(E));
	}
	return Out;
}

TArray<FString> UUnrealBridgeGameplayLibrary::ListInputActionsByValueType(
	const FString& ContentPathFilter, const FString& ValueTypeFilter, int32 MaxResults)
{
	TArray<FString> All = BridgeInputAuthoringImpl::ListAssetsOfClass(
		UInputAction::StaticClass(), ContentPathFilter, /*MaxResults*/ 0);
	if (ValueTypeFilter.IsEmpty())
	{
		if (MaxResults > 0 && All.Num() > MaxResults) All.SetNum(MaxResults);
		return All;
	}
	const EInputActionValueType Want = BridgeInputAuthoringImpl::ParseValueType(ValueTypeFilter);
	TArray<FString> Out;
	for (const FString& Path : All)
	{
		if (UInputAction* IA = LoadObject<UInputAction>(nullptr, *Path))
		{
			if (IA->ValueType == Want) Out.Add(Path);
			if (MaxResults > 0 && Out.Num() >= MaxResults) break;
		}
	}
	return Out;
}

// ─── A3 / A9 / A10 — IA/IMC asset metadata + duplicate ───────────────

bool UUnrealBridgeGameplayLibrary::SetInputActionProperty(
	const FString& InputActionPath, const FString& PropertyName, const FString& JsonValue, bool bSave)
{
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return false;

	if (PropertyName == TEXT("ValueType"))
	{
		FString Trim = JsonValue.TrimStartAndEnd().TrimQuotes();
		IA->ValueType = BridgeInputAuthoringImpl::ParseValueType(Trim);
	}
	else
	{
		FProperty* Prop = FindFProperty<FProperty>(IA->GetClass(), FName(*PropertyName));
		if (!Prop) return false;
		void* Ptr = Prop->ContainerPtrToValuePtr<void>(IA);
		const TCHAR* Buf = *JsonValue;
		if (!Prop->ImportText_Direct(Buf, Ptr, IA, PPF_None))
		{
			UE_LOG(LogUnrealBridgeAgent, Warning,
				TEXT("SetInputActionProperty: failed to import '%s' into '%s'"), *JsonValue, *PropertyName);
			return false;
		}
	}

	IA->Modify();
	IA->PostEditChange();
	IA->MarkPackageDirty();
	if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ IA->GetOutermost() }, false);
	return true;
}

bool UUnrealBridgeGameplayLibrary::SetIMCMappingPlayerMappableKeySettings(
	const FString& MappingContextPath, const FString& InputActionPath, const FString& KeyName,
	const FString& PlayerMappableKeySettingsPath, bool bSave)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IMC || !IA) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;

	const int32 Idx = BridgeInputAuthoringImpl::FindMappingIndex(IMC, IA, Key);
	if (Idx == INDEX_NONE) return false;

	UPlayerMappableKeySettings* PMKS = nullptr;
	if (!PlayerMappableKeySettingsPath.IsEmpty())
	{
		PMKS = LoadObject<UPlayerMappableKeySettings>(nullptr, *PlayerMappableKeySettingsPath);
		if (!PMKS) return false;
	}

	IMC->Modify();
	FEnhancedActionKeyMapping* M = BridgeInputAuthoringImpl::GetMutableMapping(IMC, Idx);
	if (!M) return false;

	// FEnhancedActionKeyMapping::PlayerMappableKeySettings is a protected
	// UPROPERTY — no public setter. Set it via reflection on the struct
	// to avoid friend-class hacks. (UE 5.7: field name verified.)
	if (UScriptStruct* SS = FEnhancedActionKeyMapping::StaticStruct())
	{
		if (FObjectProperty* OP = FindFProperty<FObjectProperty>(SS, TEXT("PlayerMappableKeySettings")))
		{
			OP->SetObjectPropertyValue(OP->ContainerPtrToValuePtr<void>(M), PMKS);
		}
	}

	IMC->PostEditChange();
	IMC->MarkPackageDirty();
	if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ IMC->GetOutermost() }, false);
	return true;
}

namespace BridgeInputAuthoringImpl
{
	template<typename TAsset>
	static FString DuplicateAsset(const FString& SourcePath, const FString& DestPath, bool bSave)
	{
		TAsset* Src = LoadObject<TAsset>(nullptr, *SourcePath);
		if (!Src) return FString();
		FString PackageName, AssetName;
		if (!SplitPackagePath(DestPath, PackageName, AssetName)) return FString();
		if (FPackageName::DoesPackageExist(PackageName)) return FString();

		UPackage* DestPkg = CreatePackage(*PackageName);
		if (!DestPkg) return FString();
		UObject* Dup = StaticDuplicateObject(Src, DestPkg, FName(*AssetName), RF_AllFlags);
		if (!Dup) return FString();
		Dup->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Dup);
		Dup->MarkPackageDirty();
		if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ DestPkg }, false);
		return PackageName + TEXT(".") + AssetName;
	}
}

FString UUnrealBridgeGameplayLibrary::DuplicateInputAction(const FString& SourcePath, const FString& DestPath, bool bSave)
{
	return BridgeInputAuthoringImpl::DuplicateAsset<UInputAction>(SourcePath, DestPath, bSave);
}

FString UUnrealBridgeGameplayLibrary::DuplicateInputMappingContext(const FString& SourcePath, const FString& DestPath, bool bSave)
{
	return BridgeInputAuthoringImpl::DuplicateAsset<UInputMappingContext>(SourcePath, DestPath, bSave);
}

// ─── A7 / A8 — IMC per-mapping triggers + modifiers ─────────────────

namespace BridgeInputAuthoringImpl
{
	static FEnhancedActionKeyMapping* ResolveMapping(UInputMappingContext* IMC,
		const UInputAction* IA, FKey Key, int32& OutIdx)
	{
		OutIdx = FindMappingIndex(IMC, IA, Key);
		if (OutIdx == INDEX_NONE) return nullptr;
		return GetMutableMapping(IMC, OutIdx);
	}
}

int32 UUnrealBridgeGameplayLibrary::AddTriggerToIMCMapping(
	const FString& MappingContextPath, const FString& InputActionPath, const FString& KeyName,
	const FString& TriggerClass, const FString& ParamsJson, bool bSave)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IMC || !IA) return -1;
	FKey Key(*KeyName); if (!Key.IsValid()) return -1;

	int32 MappingIdx;
	FEnhancedActionKeyMapping* M = BridgeInputAuthoringImpl::ResolveMapping(IMC, IA, Key, MappingIdx);
	if (!M) return -1;

	UClass* Cls = BridgeInputAuthoringImpl::ResolveSubclass(UInputTrigger::StaticClass(), TriggerClass);
	if (!Cls || Cls->HasAnyClassFlags(CLASS_Abstract)) return -1;

	UInputTrigger* Inst = NewObject<UInputTrigger>(IMC, Cls, NAME_None, RF_Transactional);
	if (!Inst) return -1;
	BridgeInputAuthoringImpl::ApplyJsonParamsToObject(Inst, ParamsJson);

	IMC->Modify();
	const int32 NewIdx = M->Triggers.Add(Inst);
	IMC->PostEditChange();
	IMC->MarkPackageDirty();
	if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ IMC->GetOutermost() }, false);
	return NewIdx;
}

int32 UUnrealBridgeGameplayLibrary::AddModifierToIMCMapping(
	const FString& MappingContextPath, const FString& InputActionPath, const FString& KeyName,
	const FString& ModifierClass, const FString& ParamsJson, bool bSave)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IMC || !IA) return -1;
	FKey Key(*KeyName); if (!Key.IsValid()) return -1;

	int32 MappingIdx;
	FEnhancedActionKeyMapping* M = BridgeInputAuthoringImpl::ResolveMapping(IMC, IA, Key, MappingIdx);
	if (!M) return -1;

	UClass* Cls = BridgeInputAuthoringImpl::ResolveSubclass(UInputModifier::StaticClass(), ModifierClass);
	if (!Cls || Cls->HasAnyClassFlags(CLASS_Abstract)) return -1;

	UInputModifier* Inst = NewObject<UInputModifier>(IMC, Cls, NAME_None, RF_Transactional);
	if (!Inst) return -1;
	BridgeInputAuthoringImpl::ApplyJsonParamsToObject(Inst, ParamsJson);

	IMC->Modify();
	const int32 NewIdx = M->Modifiers.Add(Inst);
	IMC->PostEditChange();
	IMC->MarkPackageDirty();
	if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ IMC->GetOutermost() }, false);
	return NewIdx;
}

bool UUnrealBridgeGameplayLibrary::RemoveTriggerFromIMCMapping(
	const FString& MappingContextPath, const FString& InputActionPath, const FString& KeyName,
	int32 Index, bool bSave)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IMC || !IA) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;

	int32 MappingIdx;
	FEnhancedActionKeyMapping* M = BridgeInputAuthoringImpl::ResolveMapping(IMC, IA, Key, MappingIdx);
	if (!M) return false;

	const int32 Norm = BridgeInputAuthoringImpl::NormalizeIndex(Index, M->Triggers.Num());
	if (Norm == INDEX_NONE) return false;

	IMC->Modify();
	M->Triggers.RemoveAt(Norm);
	IMC->PostEditChange();
	IMC->MarkPackageDirty();
	if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ IMC->GetOutermost() }, false);
	return true;
}

bool UUnrealBridgeGameplayLibrary::RemoveModifierFromIMCMapping(
	const FString& MappingContextPath, const FString& InputActionPath, const FString& KeyName,
	int32 Index, bool bSave)
{
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IMC || !IA) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;

	int32 MappingIdx;
	FEnhancedActionKeyMapping* M = BridgeInputAuthoringImpl::ResolveMapping(IMC, IA, Key, MappingIdx);
	if (!M) return false;

	const int32 Norm = BridgeInputAuthoringImpl::NormalizeIndex(Index, M->Modifiers.Num());
	if (Norm == INDEX_NONE) return false;

	IMC->Modify();
	M->Modifiers.RemoveAt(Norm);
	IMC->PostEditChange();
	IMC->MarkPackageDirty();
	if (bSave) UEditorLoadingAndSavingUtils::SavePackages({ IMC->GetOutermost() }, false);
	return true;
}

// ─── D1 / D2 — find references ──────────────────────────────────────

namespace BridgeInputSearchImpl
{
	static void EnumerateBlueprints(const FString& PackagePathFilter, TArray<UBlueprint*>& Out)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FString Root = PackagePathFilter.IsEmpty() ? FString(TEXT("/Game")) : PackagePathFilter;
		if (!Root.StartsWith(TEXT("/"))) Root = TEXT("/") + Root;
		FARFilter F; F.bRecursivePaths = true;
		F.PackagePaths.Add(FName(*Root));
		F.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		F.bRecursiveClasses = true;

		TArray<FAssetData> Data;
		AR.GetAssets(F, Data);
		for (const FAssetData& A : Data)
		{
			if (UBlueprint* BP = Cast<UBlueprint>(A.GetAsset())) Out.Add(BP);
		}
	}

	static void ForEachGraph(UBlueprint* BP, TFunctionRef<void(UEdGraph*)> Fn)
	{
		for (UEdGraph* G : BP->UbergraphPages)   Fn(G);
		for (UEdGraph* G : BP->FunctionGraphs)   Fn(G);
		for (UEdGraph* G : BP->MacroGraphs)      Fn(G);
	}
}

TArray<FBridgeInputReference> UUnrealBridgeGameplayLibrary::FindInputActionReferences(
	const FString& InputActionPath, const FString& BlueprintPackagePathFilter)
{
	TArray<FBridgeInputReference> Out;
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return Out;

	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter F; F.bRecursivePaths = true;
		F.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
		TArray<FAssetData> Data; AR.GetAssets(F, Data);
		for (const FAssetData& A : Data)
		{
			UInputMappingContext* IMC = Cast<UInputMappingContext>(A.GetAsset());
			if (!IMC) continue;
			for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
			{
				if (M.Action == IA)
				{
					FBridgeInputReference R;
					R.Kind = TEXT("imc_mapping");
					R.AssetPath = IMC->GetPathName();
					R.Detail = FString::Printf(TEXT("Key=%s"), *M.Key.ToString());
					Out.Add(MoveTemp(R));
				}
			}
		}
	}

	TArray<UBlueprint*> BPs;
	BridgeInputSearchImpl::EnumerateBlueprints(BlueprintPackagePathFilter, BPs);
	for (UBlueprint* BP : BPs)
	{
		BridgeInputSearchImpl::ForEachGraph(BP, [&](UEdGraph* G)
		{
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_EnhancedInputAction* EvNode = Cast<UK2Node_EnhancedInputAction>(N))
				{
					if (EvNode->InputAction == IA)
					{
						FBridgeInputReference R;
						R.Kind = TEXT("event_node");
						R.AssetPath = BP->GetPathName();
						R.GraphName = G->GetName();
						R.NodeGuid = EvNode->NodeGuid.ToString(EGuidFormats::Digits);
						R.Detail = TEXT("K2Node_EnhancedInputAction");
						Out.Add(MoveTemp(R));
					}
				}
				else if (UK2Node_GetInputActionValue* GetNode = Cast<UK2Node_GetInputActionValue>(N))
				{
					if (GetNode->InputAction == IA)
					{
						FBridgeInputReference R;
						R.Kind = TEXT("event_node");
						R.AssetPath = BP->GetPathName();
						R.GraphName = G->GetName();
						R.NodeGuid = GetNode->NodeGuid.ToString(EGuidFormats::Digits);
						R.Detail = TEXT("K2Node_GetInputActionValue");
						Out.Add(MoveTemp(R));
					}
				}
				else if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(N))
				{
					for (UEdGraphPin* Pin : Call->Pins)
					{
						if (Pin->Direction != EGPD_Input) continue;
						if (Pin->DefaultObject == IA)
						{
							const FName FnName = Call->FunctionReference.GetMemberName();
							const bool bIsBindAction = FnName.ToString().Contains(TEXT("BindAction"));
							FBridgeInputReference R;
							R.Kind = bIsBindAction ? TEXT("bind_action_call") : TEXT("function_param");
							R.AssetPath = BP->GetPathName();
							R.GraphName = G->GetName();
							R.NodeGuid = Call->NodeGuid.ToString(EGuidFormats::Digits);
							R.Detail = FString::Printf(TEXT("%s pin=%s"),
								*FnName.ToString(),
								*Pin->PinName.ToString());
							Out.Add(MoveTemp(R));
						}
					}
				}
			}
		});
	}
	return Out;
}

TArray<FBridgeInputReference> UUnrealBridgeGameplayLibrary::FindInputMappingContextReferences(
	const FString& MappingContextPath, const FString& BlueprintPackagePathFilter)
{
	TArray<FBridgeInputReference> Out;
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *MappingContextPath);
	if (!IMC) return Out;

	TArray<UBlueprint*> BPs;
	BridgeInputSearchImpl::EnumerateBlueprints(BlueprintPackagePathFilter, BPs);
	for (UBlueprint* BP : BPs)
	{
		if (UClass* Gen = BP->GeneratedClass)
		{
			UObject* CDO = Gen->GetDefaultObject();
			for (TFieldIterator<FObjectProperty> It(Gen); It; ++It)
			{
				FObjectProperty* OP = *It;
				if (!OP->PropertyClass || !OP->PropertyClass->IsChildOf(UInputMappingContext::StaticClass())) continue;
				UObject* Val = OP->GetObjectPropertyValue_InContainer(CDO);
				if (Val == IMC)
				{
					FBridgeInputReference R;
					R.Kind = TEXT("default_pawn_imc_field");
					R.AssetPath = BP->GetPathName();
					R.Detail = FString::Printf(TEXT("Field=%s"), *OP->GetName());
					Out.Add(MoveTemp(R));
				}
			}
		}

		BridgeInputSearchImpl::ForEachGraph(BP, [&](UEdGraph* G)
		{
			for (UEdGraphNode* N : G->Nodes)
			{
				UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(N);
				if (!Call) continue;
				const FName FnName = Call->FunctionReference.GetMemberName();
				if (!FnName.ToString().Contains(TEXT("MappingContext"))) continue;
				for (UEdGraphPin* Pin : Call->Pins)
				{
					if (Pin->Direction == EGPD_Input && Pin->DefaultObject == IMC)
					{
						FBridgeInputReference R;
						R.Kind = TEXT("subsystem_call_arg");
						R.AssetPath = BP->GetPathName();
						R.GraphName = G->GetName();
						R.NodeGuid = Call->NodeGuid.ToString(EGuidFormats::Digits);
						R.Detail = FString::Printf(TEXT("%s pin=%s"), *FnName.ToString(), *Pin->PinName.ToString());
						Out.Add(MoveTemp(R));
					}
				}
			}
		});
	}
	return Out;
}

// ─── E1 / E2 / E4 — PIE runtime state ───────────────────────────────

namespace BridgeInputRuntimeImpl
{
	static UEnhancedInputLocalPlayerSubsystem* GetActiveSubsystem()
	{
		UWorld* World = BridgeAgentImpl::GetLocalPlayerPIEWorld();
		if (!World) return nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) return nullptr;
		ULocalPlayer* LP = PC->GetLocalPlayer();
		return LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}
}

TArray<FBridgeMappingContextEntry> UUnrealBridgeGameplayLibrary::GetActiveMappingContextStack()
{
	TArray<FBridgeMappingContextEntry> Out;
	UEnhancedInputLocalPlayerSubsystem* Sub = BridgeInputRuntimeImpl::GetActiveSubsystem();
	if (!Sub) return Out;
	// UEnhancedPlayerInput::GetAppliedInputContextData is protected in 5.7;
	// IEnhancedInputSubsystemInterface::HasMappingContext is public. Iterate
	// every loaded IMC, ask the subsystem if it's currently applied + at what
	// priority. Cost is O(loaded IMCs) which is small — typically <20.
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter F; F.bRecursivePaths = true;
	F.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
	TArray<FAssetData> Data; AR.GetAssets(F, Data);
	for (const FAssetData& A : Data)
	{
		UInputMappingContext* IMC = Cast<UInputMappingContext>(A.GetAsset());
		if (!IMC) continue;
		int32 Priority = 0;
		if (Sub->HasMappingContext(IMC, Priority))
		{
			FBridgeMappingContextEntry E;
			E.MappingContextPath = IMC->GetPathName();
			E.Priority = Priority;
			Out.Add(MoveTemp(E));
		}
	}
	Out.Sort([](const FBridgeMappingContextEntry& A, const FBridgeMappingContextEntry& B) { return A.Priority > B.Priority; });
	return Out;
}

FBridgeInputActionState UUnrealBridgeGameplayLibrary::GetCurrentInputActionState(const FString& InputActionPath)
{
	FBridgeInputActionState Out;
	Out.TriggerEvent = TEXT("None");
	UEnhancedInputLocalPlayerSubsystem* Sub = BridgeInputRuntimeImpl::GetActiveSubsystem();
	if (!Sub) return Out;
	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return Out;
	const FInputActionInstance* Inst = Sub->GetPlayerInput()
		? Sub->GetPlayerInput()->FindActionInstanceData(IA)
		: nullptr;
	if (!Inst) return Out;

	const ETriggerEvent Evt = Inst->GetTriggerEvent();
	if (UEnum* E = StaticEnum<ETriggerEvent>())
	{
		Out.TriggerEvent = E->GetNameStringByValue(static_cast<int64>(Evt));
	}
	const FInputActionValue V = Inst->GetValue();
	const EInputActionValueType VT = V.GetValueType();
	switch (VT)
	{
	case EInputActionValueType::Boolean: Out.Value = FVector(V.Get<bool>() ? 1 : 0, 0, 0); break;
	case EInputActionValueType::Axis1D:  Out.Value = FVector(V.Get<float>(), 0, 0); break;
	case EInputActionValueType::Axis2D:  { const FVector2D V2 = V.Get<FVector2D>(); Out.Value = FVector(V2.X, V2.Y, 0); } break;
	case EInputActionValueType::Axis3D:  Out.Value = V.Get<FVector>(); break;
	}
	Out.ElapsedTriggeredTime = Inst->GetTriggeredTime();
	Out.ElapsedProcessedTime = Inst->GetElapsedTime();
	Out.bHasState = true;
	return Out;
}

int32 UUnrealBridgeGameplayLibrary::DumpInjectedInputQueue(
	TArray<FString>& OutPaths, TArray<FVector>& OutValues, TArray<float>& OutHoldRemainingSeconds)
{
	OutPaths.Reset(); OutValues.Reset(); OutHoldRemainingSeconds.Reset();
	for (const TPair<FString, BridgeAgentImpl::FStickyEntry>& Pair : BridgeAgentImpl::GStickyInputs)
	{
		OutPaths.Add(Pair.Key);
		OutValues.Add(Pair.Value.Value);
		OutHoldRemainingSeconds.Add(
			Pair.Value.AutoClearWorldTime > 0.0
				? static_cast<float>(Pair.Value.AutoClearWorldTime - FApp::GetCurrentTime())
				: -1.f);
	}
	return OutPaths.Num();
}

// ─── E5 — raw key event ─────────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::SimulateKeyEvent(const FString& KeyName, bool bPressed, int32 UserIndex)
{
	FKey Key(*KeyName);
	if (!Key.IsValid()) return false;
	if (!FSlateApplication::IsInitialized()) return false;

	FKeyEvent Evt(Key, FModifierKeysState(), UserIndex, /*bIsRepeat*/ false, /*CharCode*/ 0, /*KeyCode*/ 0);
	if (bPressed) FSlateApplication::Get().ProcessKeyDownEvent(Evt);
	else          FSlateApplication::Get().ProcessKeyUpEvent(Evt);
	return true;
}

// ─── F1 / F2 / F3 — legacy InputAxis/Action mappings (.ini) ─────────

TArray<FBridgeLegacyAxisMapping> UUnrealBridgeGameplayLibrary::ListLegacyAxisMappings()
{
	TArray<FBridgeLegacyAxisMapping> Out;
	const UInputSettings* S = UInputSettings::GetInputSettings();
	if (!S) return Out;
	for (const FInputAxisKeyMapping& M : S->GetAxisMappings())
	{
		FBridgeLegacyAxisMapping E;
		E.MappingName = M.AxisName.ToString();
		E.KeyName = M.Key.ToString();
		E.Scale = M.Scale;
		Out.Add(MoveTemp(E));
	}
	return Out;
}

TArray<FBridgeLegacyActionMapping> UUnrealBridgeGameplayLibrary::ListLegacyActionMappings()
{
	TArray<FBridgeLegacyActionMapping> Out;
	const UInputSettings* S = UInputSettings::GetInputSettings();
	if (!S) return Out;
	for (const FInputActionKeyMapping& M : S->GetActionMappings())
	{
		FBridgeLegacyActionMapping E;
		E.MappingName = M.ActionName.ToString();
		E.KeyName = M.Key.ToString();
		E.bShift = M.bShift; E.bCtrl = M.bCtrl; E.bAlt = M.bAlt; E.bCmd = M.bCmd;
		Out.Add(MoveTemp(E));
	}
	return Out;
}

bool UUnrealBridgeGameplayLibrary::AddLegacyAxisMapping(const FString& MappingName, const FString& KeyName, float Scale)
{
	UInputSettings* S = UInputSettings::GetInputSettings();
	if (!S) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;
	FInputAxisKeyMapping M; M.AxisName = FName(*MappingName); M.Key = Key; M.Scale = Scale;
	S->AddAxisMapping(M);
	S->SaveKeyMappings();
	S->ForceRebuildKeymaps();
	return true;
}

bool UUnrealBridgeGameplayLibrary::AddLegacyActionMapping(
	const FString& MappingName, const FString& KeyName,
	bool bShift, bool bCtrl, bool bAlt, bool bCmd)
{
	UInputSettings* S = UInputSettings::GetInputSettings();
	if (!S) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;
	FInputActionKeyMapping M;
	M.ActionName = FName(*MappingName); M.Key = Key;
	M.bShift = bShift; M.bCtrl = bCtrl; M.bAlt = bAlt; M.bCmd = bCmd;
	S->AddActionMapping(M);
	S->SaveKeyMappings();
	S->ForceRebuildKeymaps();
	return true;
}

bool UUnrealBridgeGameplayLibrary::RemoveLegacyAxisMapping(const FString& MappingName, const FString& KeyName)
{
	UInputSettings* S = UInputSettings::GetInputSettings();
	if (!S) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;
	FInputAxisKeyMapping M; M.AxisName = FName(*MappingName); M.Key = Key;
	S->RemoveAxisMapping(M);
	S->SaveKeyMappings();
	S->ForceRebuildKeymaps();
	return true;
}

bool UUnrealBridgeGameplayLibrary::RemoveLegacyActionMapping(const FString& MappingName, const FString& KeyName)
{
	UInputSettings* S = UInputSettings::GetInputSettings();
	if (!S) return false;
	FKey Key(*KeyName); if (!Key.IsValid()) return false;
	FInputActionKeyMapping M; M.ActionName = FName(*MappingName); M.Key = Key;
	S->RemoveActionMapping(M);
	S->SaveKeyMappings();
	S->ForceRebuildKeymaps();
	return true;
}

// ─── D3 / D4 — validation / conflict detection ──────────────────────

TArray<FBridgeInputBindingIssue> UUnrealBridgeGameplayLibrary::ValidateInputBindings(
	const FString& BlueprintPackagePathFilter)
{
	TArray<FBridgeInputBindingIssue> Out;

	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter F; F.bRecursivePaths = true;
		F.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
		TArray<FAssetData> Data; AR.GetAssets(F, Data);
		for (const FAssetData& A : Data)
		{
			UInputMappingContext* IMC = Cast<UInputMappingContext>(A.GetAsset());
			if (!IMC) continue;
			const TArray<FEnhancedActionKeyMapping>& Maps = IMC->GetMappings();
			for (int32 i = 0; i < Maps.Num(); ++i)
			{
				if (Maps[i].Action == nullptr)
				{
					FBridgeInputBindingIssue I;
					I.Kind = TEXT("imc_null_action");
					I.AssetPath = IMC->GetPathName();
					I.Detail = FString::Printf(TEXT("idx=%d Key=%s"), i, *Maps[i].Key.ToString());
					Out.Add(MoveTemp(I));
				}
			}
		}
	}

	TArray<UBlueprint*> BPs;
	BridgeInputSearchImpl::EnumerateBlueprints(BlueprintPackagePathFilter, BPs);
	for (UBlueprint* BP : BPs)
	{
		BridgeInputSearchImpl::ForEachGraph(BP, [&](UEdGraph* G)
		{
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_EnhancedInputAction* Ev = Cast<UK2Node_EnhancedInputAction>(N))
				{
					if (Ev->InputAction == nullptr)
					{
						FBridgeInputBindingIssue I;
						I.Kind = TEXT("event_node_null_ia");
						I.AssetPath = BP->GetPathName();
						I.Detail = FString::Printf(TEXT("graph=%s node=%s"),
							*G->GetName(), *Ev->NodeGuid.ToString(EGuidFormats::Digits));
						Out.Add(MoveTemp(I));
					}
				}
				else if (UK2Node_GetInputActionValue* Get = Cast<UK2Node_GetInputActionValue>(N))
				{
					if (Get->InputAction == nullptr)
					{
						FBridgeInputBindingIssue I;
						I.Kind = TEXT("event_node_null_ia");
						I.AssetPath = BP->GetPathName();
						I.Detail = FString::Printf(TEXT("graph=%s node=%s class=GetInputActionValue"),
							*G->GetName(), *Get->NodeGuid.ToString(EGuidFormats::Digits));
						Out.Add(MoveTemp(I));
					}
				}
			}
		});
	}
	return Out;
}

TArray<FBridgeKeyConflict> UUnrealBridgeGameplayLibrary::DetectKeyConflicts(const TArray<FString>& MappingContextPaths)
{
	TArray<FBridgeKeyConflict> Out;
	for (const FString& IMCPath : MappingContextPaths)
	{
		UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *IMCPath);
		if (!IMC) continue;
		TMap<FString, TArray<FString>> KeyToActions;
		for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
		{
			if (!M.Action) continue;
			KeyToActions.FindOrAdd(M.Key.ToString()).AddUnique(M.Action->GetPathName());
		}
		for (const TPair<FString, TArray<FString>>& Pair : KeyToActions)
		{
			if (Pair.Value.Num() < 2) continue;
			FBridgeKeyConflict C;
			C.MappingContextPath = IMCPath;
			C.KeyName = Pair.Key;
			C.ActionPaths = Pair.Value;
			Out.Add(MoveTemp(C));
		}
	}
	return Out;
}

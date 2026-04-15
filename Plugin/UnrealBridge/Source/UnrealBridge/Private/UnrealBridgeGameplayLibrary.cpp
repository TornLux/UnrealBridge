#include "UnrealBridgeGameplayLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Containers/Ticker.h"

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
		GStickyTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&StickyTick), 0.0f);
	}

	static void StopStickyTickerIfIdle()
	{
		if (GStickyInputs.Num() == 0 && GStickyTicker.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GStickyTicker);
			GStickyTicker.Reset();
		}
	}

	/** Return the active PIE world, or nullptr if not playing. */
	UWorld* GetPIEWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::PIE && Ctx.World() && Ctx.World()->HasBegunPlay())
			{
				return Ctx.World();
			}
		}
		return nullptr;
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
}

bool UUnrealBridgeGameplayLibrary::GetAgentObservation(
	FAgentObservation& OutObservation,
	float MaxActorDistance,
	bool bRequireLineOfSight,
	const FString& ClassFilter)
{
	OutObservation = FAgentObservation();

	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Verbose, TEXT("GetAgentObservation: PIE not running"));
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	if (!World)
	{
		UE_LOG(LogUnrealBridgeAgent, Warning, TEXT("InjectEnhancedInputAxis: no PIE world"));
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

		UWorld* World = GetPIEWorld();
		if (!World)
		{
			// PIE not running: drop all sticky entries, leaving a fresh
			// slate for the next PIE session.
			GStickyInputs.Reset();
			return true;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->GetLocalPlayer())
		{
			return true;
		}
		UEnhancedInputLocalPlayerSubsystem* Subsystem =
			PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (!Subsystem)
		{
			return true;
		}

		for (TMap<FString, FStickyEntry>::TIterator It(GStickyInputs); It; ++It)
		{
			UInputAction* Action = It->Value.Action.Get();
			if (!Action)
			{
				It.RemoveCurrent();
				continue;
			}
			const EInputActionValueType IaType = Action->ValueType;
			FInputActionValue Value(IaType, It->Value.Value);
			Subsystem->InjectInputForAction(Action, Value, {}, {});
		}
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
	BridgeAgentImpl::FStickyEntry Entry;
	Entry.Action = Action;
	Entry.Value = AxisValue;
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

// ─── State inspection + reset ──────────────────────────────────────────

bool UUnrealBridgeGameplayLibrary::GetControlRotation(FRotator& OutRotation)
{
	OutRotation = FRotator::ZeroRotator;
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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

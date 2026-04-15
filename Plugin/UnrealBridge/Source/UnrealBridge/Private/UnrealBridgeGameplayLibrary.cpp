#include "UnrealBridgeGameplayLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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
		UWorld* World = GetPIEWorld();
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
	static UCharacterMovementComponent* GetPIECharMove()
	{
		UWorld* World = GetPIEWorld();
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
	UCharacterMovementComponent* Move = BridgeAgentImpl::GetPIECharMove();
	return Move ? Move->MaxWalkSpeed : -1.0f;
}

bool UUnrealBridgeGameplayLibrary::SetPawnMaxWalkSpeed(float Speed)
{
	if (Speed < 0.0f)
	{
		return false;
	}
	UCharacterMovementComponent* Move = BridgeAgentImpl::GetPIECharMove();
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
	UCharacterMovementComponent* Move = BridgeAgentImpl::GetPIECharMove();
	if (!Move)
	{
		return false;
	}
	Move->GravityScale = Scale;
	return true;
}

float UUnrealBridgeGameplayLibrary::GetPawnSpeed()
{
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
	APawn* Pawn = BridgeAgentImpl::GetPlayerPawn(World);
	if (!Pawn)
	{
		return -1.0f;
	}
	return Pawn->GetVelocity().Size();
}

bool UUnrealBridgeGameplayLibrary::GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = FVector::ZeroVector;
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
	UWorld* World = BridgeAgentImpl::GetPIEWorld();
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

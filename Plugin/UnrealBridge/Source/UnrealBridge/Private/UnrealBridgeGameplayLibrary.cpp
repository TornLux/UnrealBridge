#include "UnrealBridgeGameplayLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeAgent, Log, All);

namespace BridgeAgentImpl
{
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
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (const UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			OutObservation.bOnGround = !Move->IsFalling();
		}
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

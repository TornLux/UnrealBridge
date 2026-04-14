#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeGameplayLibrary.generated.h"

/** One entry in FAgentObservation.VisibleActors. Intentionally flat so the
 *  Python binding returns a plain struct-of-primitives per actor without
 *  any UObject indirection. */
USTRUCT(BlueprintType)
struct FAgentVisibleActor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FName ActorName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString ClassName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	float Distance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	TArray<FName> Tags;
};

/** Packed agent-side observation of the PIE world. One UFUNCTION call
 *  assembles this so a Python agent loop doesn't need multiple bridge
 *  round-trips per tick. */
USTRUCT(BlueprintType)
struct FAgentObservation
{
	GENERATED_BODY()

	/** False when PIE is not running or there is no player pawn yet. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector PawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FRotator PawnRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector PawnVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bOnGround = true;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector CameraLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector CameraForward = FVector::ForwardVector;

	/** Other pawns/actors the agent can perceive. Filtering rules depend
	 *  on the flags passed to GetAgentObservation. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	TArray<FAgentVisibleActor> VisibleActors;
};

/**
 * Agent sensors + navigation for PIE automation.
 *
 * MVP scope:
 *   - GetAgentObservation: pawn state + nearby-actor list in one call.
 *   - FindNavPath: wrap UNavigationSystemV1::FindPathToLocationSynchronously.
 *
 * Actuators (apply_movement_input, press_action, etc.) are a separate PR.
 */
UCLASS()
class UUnrealBridgeGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fill an FAgentObservation for the active PIE player pawn.
	 *
	 * @param OutObservation     Populated struct. Check bValid before reading.
	 * @param MaxActorDistance   Filter radius (cm). Actors farther than this
	 *                           are dropped from VisibleActors.
	 * @param bRequireLineOfSight  If true, do a camera-to-actor line trace;
	 *                           drop actors that are occluded.
	 * @param ClassFilter        Optional substring match on class name
	 *                           (case-insensitive). Empty = all pawns+actors.
	 * @return true when a PIE world + player pawn exist, false otherwise
	 *         (OutObservation.bValid mirrors the return).
	 *
	 * Cost: O(N) scan of pawns in the PIE world + one line trace per candidate
	 * if bRequireLineOfSight. Single GameThread call. Typical <1ms for small
	 * scenes; up to a few ms for levels with hundreds of actors.
	 *
	 * Output footprint: small — one FAgentObservation with an inline list of
	 * visible actors. Size scales linearly with MaxActorDistance.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetAgentObservation(
		FAgentObservation& OutObservation,
		float MaxActorDistance = 3000.0f,
		bool bRequireLineOfSight = true,
		const FString& ClassFilter = TEXT(""));

	/**
	 * Synchronously compute a navmesh path between two world-space points
	 * in the PIE world. Wraps UNavigationSystemV1::FindPathToLocationSynchronously.
	 *
	 * @param StartLocation   World-space source.
	 * @param EndLocation     World-space target.
	 * @param OutWaypoints    Receives the path's FNavPathPoint::Location list,
	 *                        including Start as waypoints[0] and End-nearest
	 *                        as the last element. Empty on failure.
	 * @param OutPathLength   Receives sum of segment lengths (cm).
	 * @return true if a path was found on the editor/PIE world's navmesh.
	 *
	 * Cost: one synchronous navmesh query on the GameThread. Fast for local
	 * pathfinding (<5ms typical) but scales with distance + corridor width.
	 *
	 * Output footprint: small — one FVector array, usually 2-20 waypoints.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool FindNavPath(
		const FVector& StartLocation,
		const FVector& EndLocation,
		TArray<FVector>& OutWaypoints,
		float& OutPathLength);
};

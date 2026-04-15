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

	/** Short class name of the player pawn (e.g. "BP_UnitPlayerCharacter_C"). Empty if no pawn. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString PawnClassName;

	/** Short class name of the pawn's movement component, if any (e.g.
	 *  "CharacterMovementComponent"). Empty for raw APawn without a movement comp. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString MovementComponentClassName;

	/** UCharacterMovementComponent::MovementMode as integer (see EMovementMode). 0 for non-Character pawns. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	int32 MovementMode = 0;

	/** True if APawn::IsInputBlocked() or the controller is suppressing input. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bInputBlocked = false;

	/** APawn::GetLastMovementInputVector() — the last per-tick input
	 *  accumulator that was consumed. Lets callers verify their input
	 *  actually reached the pawn (zero here means the request never
	 *  arrived or was cleared). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector LastControlInputVector = FVector::ZeroVector;

	/** UCharacterMovementComponent::GetCurrentAcceleration() — the
	 *  acceleration the movement comp computed from input last tick.
	 *  Non-zero means input got through and movement is reacting; zero
	 *  here despite a non-zero LastControlInputVector points at a
	 *  root-motion / custom-locomotion override. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector CurrentAcceleration = FVector::ZeroVector;

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

	// ─── Actuators ────────────────────────────────────────────────────
	//
	// All actuators target the PIE world's first player pawn/controller.
	// They return false when PIE is not running or no pawn exists.
	//
	// Input is per-frame accumulative (UE's standard APawn pattern):
	// AddMovementInput is consumed by the movement component once per
	// tick and then cleared. A Python agent running at e.g. 30 Hz should
	// call ApplyMovementInput once per tick for continuous motion — if
	// the agent pauses, the pawn stops naturally on the next frame.

	/**
	 * Feed one frame's worth of movement input to the pawn.
	 * Wraps APawn::AddMovementInput.
	 *
	 * @param WorldDirection  World-space direction; magnitude is ignored,
	 *                        the vector is normalised internally.
	 * @param ScaleValue      [-1, 1]; sign flips the direction, magnitude
	 *                        scales the resulting input axis.
	 * @param bForce          Applied even if the pawn is currently ignoring
	 *                        input (debug / cutscene override).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool ApplyMovementInput(const FVector& WorldDirection, float ScaleValue = 1.0f, bool bForce = false);

	/**
	 * Feed one frame's worth of look input to the controller.
	 * Wraps APlayerController::AddYawInput / AddPitchInput. Values are in
	 * "input units" — the same units the input mapping context would
	 * deliver from mouse delta. Typical gameplay mapping is ~1 unit per
	 * degree but project-dependent.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool ApplyLookInput(float YawDelta, float PitchDelta);

	/**
	 * Instantly set the player controller's rotation, bypassing input
	 * smoothing. Useful for test teleports / facing a specific actor in
	 * one call. Does not touch the pawn — the pawn's visual rotation
	 * follows the controller on the next tick via bUseControllerRotation*.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetControlRotation(const FRotator& NewRotation);

	/** Start a jump if the pawn is a Character. Mirror of ACharacter::Jump. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool Jump();

	/** Release the jump latch; mirror of ACharacter::StopJumping. Call this
	 *  to end a variable-height jump, or after a single tick for a tap jump. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool StopJumping();

	/**
	 * Inject a single-tick value into an EnhancedInput InputAction for
	 * the PIE player. Good for discrete "press" actions (IA_Jump,
	 * IA_Interact) — one call fires the "Pressed"/"Triggered" phase
	 * for that frame.
	 *
	 * For continuously-held axes (IA_Move, IA_Look), use
	 * SetStickyInput / ClearStickyInput instead — a single injection
	 * lasts one tick, and the Python bridge can't reliably call every
	 * UE frame. Sticky inject repeats in-process on the GameThread.
	 *
	 * Value type is auto-coerced to the IA's declared EInputActionValueType
	 * so Axis3D callers work with Bool/Axis1D/Axis2D IAs.
	 *
	 * @param InputActionPath  Asset path, e.g. "/LocomotionDriver/Input/IA_Move".
	 * @param AxisValue        FVector; components beyond the IA's type are ignored.
	 * @return true if the IA asset loaded and the input was queued.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool InjectEnhancedInputAxis(const FString& InputActionPath, const FVector& AxisValue);

	/**
	 * Set a "sticky" EnhancedInput value: re-injected every GameThread
	 * tick until cleared or overwritten. The caller sets it once and
	 * movement/look input persists at UE frame rate without Python
	 * needing to keep up. Multiple IAs can be sticky at the same time
	 * (e.g. hold forward + look yaw continuously).
	 *
	 * Calling SetStickyInput with the same InputActionPath overwrites
	 * the previous value (not additive). Passing a zero vector does
	 * NOT clear it — call ClearStickyInput for that so the caller can
	 * explicitly pause input without losing the binding.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetStickyInput(const FString& InputActionPath, const FVector& AxisValue);

	/** Remove a single sticky entry. Pass empty string to clear all. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool ClearStickyInput(const FString& InputActionPath = TEXT(""));

	// ─── State inspection + reset ─────────────────────────────────────

	/**
	 * Read the current APlayerController control rotation. Pairs with
	 * SetControlRotation — useful for "where is the camera facing right
	 * now" queries without assembling a full FAgentObservation.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetControlRotation(FRotator& OutRotation);

	/**
	 * Hard-reset the pawn's pose. Combines SetActorLocationAndRotation
	 * with optional controller re-alignment and velocity clearing so a
	 * Python agent can drop the pawn at a known state for scenario setup.
	 *
	 * @param NewLocation       Target world-space location.
	 * @param NewRotation       Target pawn rotation. Also applied to the
	 *                          controller when bSnapController=true.
	 * @param bSnapController   If true, also set PlayerController->SetControlRotation.
	 * @param bStopVelocity     If true, zero linear/angular velocity via
	 *                          StopMovementImmediately on the movement comp.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool TeleportPawn(
		const FVector& NewLocation,
		const FRotator& NewRotation,
		bool bSnapController = true,
		bool bStopVelocity = true);

	/**
	 * Enumerate active sticky EnhancedInput entries. Paths and values are
	 * returned in parallel arrays; index i in OutPaths corresponds to
	 * index i in OutValues.
	 *
	 * @return Number of active sticky entries.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 GetStickyInputs(TArray<FString>& OutPaths, TArray<FVector>& OutValues);

	/** True when a PIE world is currently playing with a player pawn spawned. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool IsInPIE();

	// ─── Camera aim + perception helpers ──────────────────────────────

	/**
	 * Line-trace from the player's camera along its forward vector up to
	 * `MaxDistance` cm. Returns the hit actor's FName (not label — consistent
	 * with FAgentVisibleActor.ActorName) or empty string if nothing hit.
	 * Pawn is ignored from the trace so the ray doesn't self-hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetCameraHitActor(float MaxDistance = 10000.0f);

	/**
	 * Same ray as GetCameraHitActor, but returns the world-space hit point.
	 * @return true on hit; OutHitLocation is zeroed on miss / no PIE.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetCameraHitLocation(float MaxDistance, FVector& OutHitLocation);

	/**
	 * Line-trace from camera to the named actor's centroid. Returns true if
	 * the ray is clear (or only obstructed by the actor itself). Actor is
	 * matched by FName first, then label. Returns false if no PIE, no
	 * actor, or line-of-sight blocked.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool IsActorVisibleFromCamera(const FString& ActorName, float MaxDistance = 10000.0f);

	/**
	 * Downward line-trace from the pawn's pivot for `MaxDistance` cm.
	 * Returns the distance (cm) to the first hit surface; -1.0 if the
	 * trace misses everything or no PIE. Handy for stairs/drop detection
	 * in agent policies without resolving the full CharacterMovement state.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetPawnGroundHeight(float MaxDistance = 5000.0f);

	// ─── Navmesh utilities ────────────────────────────────────────────
	//
	// Companion queries to FindNavPath. All operate on the PIE world's
	// primary RecastNavMesh when PIE is running, otherwise fall back to
	// the editor world's navmesh (same behaviour as FindNavPath).

	/**
	 * Project `Point` onto the nearest navmesh surface within a half-extent
	 * search box. Used to "clamp" arbitrary world points (e.g. a camera
	 * hit position) onto walkable navmesh before planning a path.
	 *
	 * @param Point          World-space source.
	 * @param SearchExtent   Half-extent of the search box (cm). Default
	 *                       200 cm covers most small drops/ramps.
	 * @param OutProjected   Projected point on navmesh; zeroed on failure.
	 * @return true if a navmesh point was found within the box.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool ProjectPointToNavmesh(const FVector& Point, const FVector& SearchExtent, FVector& OutProjected);

	/**
	 * Quick yes/no variant of ProjectPointToNavmesh — true if *any* navmesh
	 * surface is reachable within `Tolerance` cm of the query point.
	 * Tolerance is expanded to an axis-aligned half-extent of (T, T, T).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool IsPointOnNavmesh(const FVector& Point, float Tolerance = 50.0f);

	/**
	 * World-space AABB of the primary RecastNavMesh data. Both corners are
	 * zeroed when no navmesh exists. Useful for sizing random-sample
	 * queries or validating that a plan origin lies within the navigable
	 * region at all.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetNavMeshBounds(FVector& OutMin, FVector& OutMax);

	/**
	 * Pick a random reachable navmesh point within `Radius` cm of `Origin`.
	 * Wraps UNavigationSystemV1::GetRandomReachablePointInRadius — respects
	 * corridor connectivity, so the returned point is guaranteed path-
	 * reachable from Origin (not just "on the navmesh nearby").
	 *
	 * @param OutPoint   Random reachable point; zeroed on failure.
	 * @return true if a point was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint);

	// ─── Screen-space perception ──────────────────────────────────────

	/**
	 * PIE viewport pixel size (X = width, Y = height). Returns false when
	 * PIE isn't running or no viewport exists. Use this to denormalize the
	 * [0,1] coordinates that the project/deproject helpers accept.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPIEViewportSize(FVector2D& OutSize);

	/**
	 * Convert a normalized viewport position (`NormalizedX`, `NormalizedY`
	 * both in [0,1]; origin top-left) to a world-space ray. Mirrors
	 * `UGameplayStatics::DeprojectScreenToWorld` but accepts normalized
	 * input so callers don't need the pixel size first.
	 *
	 * @param OutOrigin      Ray start in world-space (near plane).
	 * @param OutDirection   Unit direction into the scene.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DeprojectScreenToWorld(float NormalizedX, float NormalizedY, FVector& OutOrigin, FVector& OutDirection);

	/**
	 * Project a world-space point to the PIE viewport. Returns the
	 * normalized [0,1] viewport coordinates in `OutNormalized` (origin
	 * top-left). Return value is false when the point is behind the
	 * camera or off-screen, or when PIE isn't running.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool ProjectWorldToScreen(const FVector& WorldLocation, FVector2D& OutNormalized);

	/**
	 * Convenience: deproject a normalized viewport position into a ray and
	 * line-trace it up to `MaxDistance` cm (visibility channel, pawn
	 * ignored). Returns the hit actor's FName as string, or empty string
	 * on miss. Equivalent to `DeprojectScreenToWorld` + manual trace but
	 * saves a round-trip when the caller just wants the actor name.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetActorAtScreenPosition(float NormalizedX, float NormalizedY, float MaxDistance = 10000.0f);
};

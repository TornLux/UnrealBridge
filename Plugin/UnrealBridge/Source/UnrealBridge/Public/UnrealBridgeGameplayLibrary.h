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
};

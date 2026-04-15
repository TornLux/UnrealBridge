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

	// ─── Camera control + fast view query ─────────────────────────────

	/**
	 * Current camera vertical field of view in degrees. Returns -1.0 when
	 * PIE is not running or no PlayerCameraManager is available.
	 * Reads from `APlayerCameraManager::GetFOVAngle()`.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetCameraFOV();

	/**
	 * Override the camera FOV using `APlayerCameraManager::SetFOV`.
	 * The override persists until `UnlockCameraFOV` is called. Typical
	 * range is 60–120 degrees; values below 1 or above 170 are rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetCameraFOV(float FOV);

	/**
	 * Clear the FOV override set by `SetCameraFOV` so the camera returns
	 * to the default FOV sourced from the active view target.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool UnlockCameraFOV();

	/**
	 * Current camera world location and rotation. Lightweight alternative
	 * to assembling an `FAgentObservation` when the caller only needs the
	 * view transform (e.g. stable cinematography logic).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation);

	// ─── Character movement tuning ────────────────────────────────────
	//
	// All four target the PIE player pawn's UCharacterMovementComponent.
	// They return a sentinel / false for non-Character pawns (raw APawn
	// subclasses without CharMove won't respond).

	/**
	 * `UCharacterMovementComponent::MaxWalkSpeed` in cm/s.
	 * Returns -1.0 when PIE isn't running or the pawn lacks a
	 * character-movement component.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetPawnMaxWalkSpeed();

	/**
	 * Override `MaxWalkSpeed`. Does not touch `MaxAcceleration` — the
	 * pawn ramps up to the new speed over a few frames via existing
	 * acceleration rules.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetPawnMaxWalkSpeed(float Speed);

	/**
	 * Override `UCharacterMovementComponent::GravityScale`. 1.0 = default,
	 * 0.0 = zero-g, 2.0 = double gravity. Negative values are rejected.
	 * Useful for test scenarios where you want the pawn to hang in air
	 * or fall faster without changing world gravity.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetPawnGravityScale(float Scale);

	/**
	 * Current speed of the pawn — magnitude of `APawn::GetVelocity()` in
	 * cm/s. Returns -1.0 when PIE isn't running / no pawn. Works for
	 * non-Character pawns too (falls back to `AActor::GetVelocity`).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetPawnSpeed();

	// ─── PIE runtime spawn/destroy + query ────────────────────────────
	//
	// These operate on the active PIE world — distinct from the Level
	// library's spawn/destroy which target the editor world. Use these
	// when an agent script needs to drop or remove test targets mid-PIE.

	/**
	 * Spawn an actor in the PIE world at the given transform.
	 *
	 * @param ClassPath   Full class path, e.g. `/Script/Engine.StaticMeshActor`
	 *                    or a Blueprint class (`_C` suffix optional).
	 * @param Location    World-space location (cm).
	 * @param Rotation    World-space rotation.
	 * @return            FName of the new actor, or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString SpawnActorInPIE(const FString& ClassPath, const FVector& Location, const FRotator& Rotation);

	/**
	 * Destroy a PIE actor by FName or display label. Returns false when
	 * the actor is missing or PIE isn't running.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DestroyActorInPIE(const FString& ActorName);

	/**
	 * Current world location of a PIE actor. UE Python convention: returns
	 * the `FVector` on success, `None` when PIE isn't running or the actor
	 * is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPIEActorLocation(const FString& ActorName, FVector& OutLocation);

	/**
	 * Names (FName as string) of all PIE actors whose class derives from
	 * `ClassPath`. Pass a broad class (e.g. `/Script/Engine.Actor`) to list
	 * everything, or a specific class / BP for a targeted query.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FString> FindPIEActorsByClass(const FString& ClassPath);

	// ─── Time dilation control ───────────────────────────────────────
	//
	// Differs from Editor::PausePIE (which hard-pauses PIE via the
	// editor's play-session facility). Time dilation scales delta-time
	// delivered to actor ticks — 0.25 = quarter-speed, 4.0 = 4x, 0.0 is
	// rejected (use PausePIE for a true pause).

	/** Current world-level time dilation. Returns 1.0 outside PIE. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetGlobalTimeDilation();

	/**
	 * Set world-level time dilation. Clamped to `[0.0001, 20.0]`;
	 * zero is rejected. Persists until overwritten — NOT reset when PIE
	 * stops, so a test script should restore to 1.0 in its teardown.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetGlobalTimeDilation(float Scale);

	/**
	 * Per-actor time dilation (multiplied with the global value). Returns
	 * -1.0 on missing actor / no PIE.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetActorTimeDilation(const FString& ActorName);

	/**
	 * Set a per-actor time dilation multiplier. Clamped to `[0.0001, 20.0]`.
	 * Combined with `GetGlobalTimeDilation`, the effective tick rate for
	 * this actor is `global * actor_scale`.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetActorTimeDilation(const FString& ActorName, float Scale);

	// ─── Audio + damage proxies ──────────────────────────────────────
	//
	// Thin wrappers around UGameplayStatics for common gameplay triggers.
	// All target the PIE world and no-op outside PIE.

	/**
	 * Play a 2D (non-spatialized) sound. Returns false if PIE isn't
	 * running or the asset fails to load.
	 *
	 * @param SoundAssetPath     Full path to a USoundBase asset.
	 * @param VolumeMultiplier   Typical range 0..2.
	 * @param PitchMultiplier    Typical range 0.5..2.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool PlaySound2D(const FString& SoundAssetPath, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/**
	 * Play a spatialized sound at a world location. Uses
	 * `UGameplayStatics::PlaySoundAtLocation` — attenuation follows the
	 * sound asset's default attenuation settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool PlaySoundAtLocation(const FString& SoundAssetPath, const FVector& Location, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/**
	 * Apply point damage to a PIE actor. Returns the damage value the
	 * engine reports as actually applied (post-modifiers). Returns -1.0
	 * for missing target / no PIE.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float ApplyDamageToActor(const FString& TargetActorName, float DamageAmount);

	/**
	 * Apply radial damage at a world point. Returns the count of actors
	 * reached by the damage sphere (regardless of how much damage each
	 * actually took — zero-damage hits still count).
	 *
	 * @param InnerRadius  Full-damage radius (cm).
	 * @param OuterRadius  Zero-damage radius (cm); falloff is linear.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 ApplyRadialDamage(const FVector& Origin, float DamageAmount, float InnerRadius, float OuterRadius);

	// ─── Multi-pawn + controller queries ─────────────────────────────
	//
	// Read-only queries for scenarios with multiple pawns / AI agents.
	// The single-player helpers above (GetAgentObservation etc.) target
	// the first player pawn only — these give a broader view.

	/** FNames of every APawn in the PIE world. Empty outside PIE. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FString> GetAllPawns();

	/**
	 * FNames of pawns whose controller class name contains "AI" (i.e.
	 * `AAIController` subclasses). String match is case-sensitive.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FString> GetAIPawns();

	/**
	 * Short class name of the controller currently possessing a PIE
	 * actor (pawn or non-pawn). Empty string when the actor has no
	 * controller or isn't a pawn. Examples: "PlayerController",
	 * "AIController", "BP_EnemyAIController_C".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetActorController(const FString& ActorName);

	/**
	 * Quick "is this pawn AI-driven" test — true when the actor is a
	 * pawn and its controller's class is `AAIController` or a subclass.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool IsActorAIControlled(const FString& ActorName);

	// ─── On-screen debug + debug drawing ─────────────────────────────
	//
	// Visualise agent state in the PIE viewport. All four target the PIE
	// world and no-op outside PIE.

	/**
	 * Print a message on the viewport via `GEngine->AddOnScreenDebugMessage`.
	 *
	 * @param Message          Text to display.
	 * @param DurationSeconds  How long the message stays visible. Clamped to [0.1, 60].
	 * @param R, G, B          Color channels in [0, 1].
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddOnScreenDebugMessage(const FString& Message, float DurationSeconds = 4.0f, float R = 1.0f, float G = 1.0f, float B = 1.0f);

	/** Remove all on-screen debug messages currently visible. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool ClearOnScreenDebugMessages();

	/**
	 * Draw a debug line segment in the PIE viewport for `DurationSeconds`.
	 * Pass 0 for a single-frame draw (next tick erases it).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DrawDebugLine(const FVector& Start, const FVector& End, float Thickness = 1.0f, float DurationSeconds = 5.0f);

	/** Draw a debug wireframe sphere in the PIE viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DrawDebugSphereAt(const FVector& Center, float Radius = 50.0f, float Thickness = 1.0f, float DurationSeconds = 5.0f);

	// ─── Enhanced Input mapping contexts ─────────────────────────────
	//
	// Thin wrappers around UEnhancedInputLocalPlayerSubsystem for
	// runtime IMC management. All require PIE + a local-player
	// EnhancedInput subsystem.

	/**
	 * Add a Mapping Context asset to the local player's Enhanced Input
	 * subsystem. Higher `Priority` wins when multiple contexts bind the
	 * same key. Returns false if the asset fails to load or PIE isn't
	 * ready.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddMappingContext(const FString& MappingContextPath, int32 Priority = 0);

	/** Remove a previously-added Mapping Context. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveMappingContext(const FString& MappingContextPath);

	/** True if the Mapping Context is currently applied on the local player. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool IsMappingContextActive(const FString& MappingContextPath);

	/**
	 * Declared value type of an InputAction asset, as a string:
	 * "Bool" | "Axis1D" | "Axis2D" | "Axis3D". Empty when the asset
	 * fails to load. Useful to sanity-check SetStickyInput values.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetInputActionValueType(const FString& InputActionPath);

	// ─── Extended debug drawing ──────────────────────────────────────
	//
	// Companions to DrawDebugLine / DrawDebugSphereAt. All require PIE.
	// `DurationSeconds = 0` keeps the shape alive only for one frame.

	/** Draw an axis-aligned wireframe box in the PIE viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DrawDebugBoxAt(const FVector& Center, const FVector& Extent, float Thickness = 1.0f, float DurationSeconds = 5.0f);

	/**
	 * Draw a debug arrow from Start to End. ArrowSize controls the head
	 * length (cm); total segment length is `End - Start` regardless.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DrawDebugArrow(const FVector& Start, const FVector& End, float ArrowSize = 20.0f, float DurationSeconds = 5.0f);

	/**
	 * Draw a world-anchored text label at `Location`. Renders via the
	 * engine's debug text system — visible only inside the PIE viewport
	 * (not captured by high-res screenshots).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DrawDebugString(const FString& Text, const FVector& Location, float DurationSeconds = 5.0f);

	/**
	 * Immediately clear every persistent debug-draw primitive
	 * (lines / spheres / boxes / arrows / strings) in the PIE world.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool FlushPersistentDebugDraws();

	// ─── Pawn basis vectors + distance ───────────────────────────────
	//
	// Convenience reads around the player pawn's orientation, for agents
	// that reason in local space. All return false / -1.0 outside PIE.

	/** World-space forward unit vector of the PIE player pawn. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPawnForwardVector(FVector& OutForward);

	/** World-space right unit vector. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPawnRightVector(FVector& OutRight);

	/** World-space up unit vector. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPawnUpVector(FVector& OutUp);

	/**
	 * Distance (cm) from the PIE pawn's origin to `Location`. Returns
	 * -1.0 outside PIE.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetDistanceToPawn(const FVector& Location);

	// ─── PIE clock / counter queries ─────────────────────────────────

	/** Engine global frame counter (`GFrameCounter`). Wraps around at uint64 max. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int64 GetPIEFrameNumber();

	/**
	 * Last delta-seconds the PIE world ticked with. Returns 0 outside
	 * PIE. Dominant uses: agent-loop cadence tuning, velocity integration
	 * checks.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static float GetPIEDeltaSeconds();

	/** Count of player controllers in the PIE world (split-screen / multi-player). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 GetPIENumPlayers();

	/**
	 * Count of controllers whose class name contains "AIController".
	 * Mirrors `GetAIPawns`'s string-match detection — robust to custom
	 * AI controller subclasses but insensitive to projects that rename
	 * the "AIController" token.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 GetPIENumAIControllers();

	// ─── Physics impulse / force ─────────────────────────────────────
	//
	// Target the PIE actor's primary `UPrimitiveComponent` (root if it's
	// a primitive, else the first primitive found). The component must
	// have physics simulation enabled — see `bridge-level-api.set_actor_
	// simulate_physics`. Non-PIE calls return false.

	/**
	 * Apply an instantaneous impulse (kg·cm/s) to the actor.
	 *
	 * @param bVelocityChange  If true, treats `Impulse` as a delta-V
	 *                         (ignores mass). False = classical impulse.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddImpulseToPIEActor(const FString& ActorName, const FVector& Impulse, bool bVelocityChange = false);

	/**
	 * Apply a continuous force this frame (kg·cm/s²). Caller must re-apply
	 * each tick for sustained acceleration — otherwise behaves like a
	 * one-shot kick.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddForceToPIEActor(const FString& ActorName, const FVector& Force);

	/** Wake a sleeping physics body so it responds to forces this tick. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool WakePIEActorPhysics(const FString& ActorName);

	/**
	 * Linear velocity (cm/s) of the actor's primary physics body.
	 * UE Python convention: returns `Vector` on success, `None` when
	 * the actor is missing or has no physics body.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPIEActorLinearVelocity(const FString& ActorName, FVector& OutVelocity);

	// ─── Camera shake control ────────────────────────────────────────
	//
	// Target the first PIE player controller / camera manager. Shake
	// class paths must resolve to a `UCameraShakeBase` Blueprint (e.g.
	// `/Game/Camera/BP_HitShake_C`) or native subclass. PIE-only.

	/**
	 * Start a camera shake on the local player. `Scale` >= 0; the shake
	 * plays for its default duration and then auto-stops.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool StartCameraShake(const FString& ShakeClassPath, float Scale = 1.0f);

	/**
	 * Stop a shake of the given class on the local player.
	 * `bImmediately=true` cuts it instantly; false lets the blend-out
	 * phase play.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool StopCameraShakeByClass(const FString& ShakeClassPath, bool bImmediately = true);

	/** Stop every camera shake currently playing on the local player. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool StopAllCameraShakes(bool bImmediately = true);

	/**
	 * Radial-falloff shake. All players within `OuterRadius` of `Epicenter`
	 * feel a shake scaled by distance — full strength inside `InnerRadius`,
	 * fading to zero at `OuterRadius`. Wraps
	 * `UGameplayStatics::PlayWorldCameraShake`.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool PlayWorldCameraShake(const FString& ShakeClassPath, const FVector& Epicenter, float InnerRadius, float OuterRadius, float ScaleMultiplier = 1.0f);

	// ─── Player pawn / start / respawn ───────────────────────────────

	/**
	 * FName of the PIE player pawn. Use this when a Level or Gameplay
	 * API wants an actor name for the possessed pawn. Empty outside PIE
	 * / no pawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetPlayerPawnActorName();

	/**
	 * FName of the first `APlayerStart` actor in the PIE world (or the
	 * editor world if no PIE). Empty when no PlayerStart exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetPlayerStartActorName();

	/** World transform of the first PlayerStart; None when missing. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPlayerStartTransform(FVector& OutLocation, FRotator& OutRotation);

	/**
	 * Teleport the player pawn to the first PlayerStart, clearing
	 * velocity and snapping the controller rotation to match.
	 * Convenience over TeleportPawn + GetPlayerStartTransform.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RespawnPlayerPawn();

	// ─── Runtime pause + game class queries ──────────────────────────
	//
	// `SetGamePaused` / `IsGamePaused` via UGameplayStatics — distinct
	// from Editor.pause_pie (which targets the editor play-session
	// facility). Both are safe to use; this one reads/writes the
	// game-side pause flag that affects actor ticks.

	/** Set runtime game pause. Returns false outside PIE. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool PauseGame(bool bPaused);

	/** Read runtime game pause state. False outside PIE. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool IsGamePaused();

	/** Short class name of the active `AGameModeBase` (e.g. "GameMode"). Empty outside PIE. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetGameModeClassName();

	/** Short class name of the active `AGameStateBase`. Empty outside PIE. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString GetGameStateClassName();
};

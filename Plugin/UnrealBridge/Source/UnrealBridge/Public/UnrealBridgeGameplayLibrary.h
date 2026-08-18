#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeGameplayLibrary.generated.h"

class UAnimInstance;
class USkeletalMeshComponent;

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

/**
 * 打包已就绪本地玩家 PIE 世界的 Agent 观测；一次 UFUNCTION 调用完成采集，减少 Python 循环往返。
 * Packed observation of the ready local-player PIE world, assembled in one UFUNCTION to reduce Python round trips.
 */
USTRUCT(BlueprintType)
struct FAgentObservation
{
	GENERATED_BODY()

	/** 没有已就绪的本地玩家 PIE 世界或尚未生成其玩家 Pawn 时为 false。 */
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

/** One row from a UInputMappingContext: an InputAction bound to a key,
 *  with the per-mapping Trigger / Modifier classes the IMC stamped on it. */
USTRUCT(BlueprintType)
struct FBridgeIMCMapping
{
	GENERATED_BODY()

	/** Asset path of the bound InputAction (e.g. "/Game/Input/IA_Move"). Empty if the action ptr was null. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString ActionPath;

	/** Short asset name of the IA, for display (e.g. "IA_Move"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString ActionName;

	/** FKey::ToString() of the bound key (e.g. "SpaceBar", "Gamepad_FaceButton_Bottom"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString KeyName;

	/** Class names of UInputTrigger subclasses configured on this mapping
	 *  (e.g. "InputTriggerHold", "InputTriggerPressed"). Per-mapping triggers
	 *  ride on top of any IA-level triggers. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	TArray<FString> TriggerClasses;

	/** Class names of UInputModifier subclasses configured on this mapping
	 *  (e.g. "InputModifierDeadZone", "InputModifierNegate", "InputModifierSwizzleAxis"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	TArray<FString> ModifierClasses;
};

/** A single Trigger/Modifier instance, fully serialized to JSON for round-trip with A4/A6/A7/A8 writers. */
USTRUCT(BlueprintType)
struct FBridgeInputComponentInstance
{
	GENERATED_BODY()

	/** Short class name (e.g. "InputTriggerHold", "InputModifierScalar"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString ClassName;

	/** Full class path (e.g. "/Script/EnhancedInput.InputTriggerHold"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString ClassPath;

	/** All UPROPERTY values on the instance, JSON-serialized via FJsonObjectConverter. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString ParamsJson;
};

/** One reference to an IA (or IMC) found by D1 / D2. */
USTRUCT(BlueprintType)
struct FBridgeInputReference
{
	GENERATED_BODY()

	/** "imc_mapping" / "event_node" / "bind_action_call" / "function_param" / "subsystem_call_arg" / "default_pawn_imc_field" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString Kind;

	/** Path of the asset that holds the reference (BP, IMC, or a config-bearing class). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString AssetPath;

	/** Graph name within a BP (empty for non-BP references). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString GraphName;

	/** Node GUID within a BP graph (empty for non-graph references). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString NodeGuid;

	/** Free-form detail (e.g. "Triggered → HandleMove", "Key=SpaceBar", "ParamName=Action"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString Detail;
};

/** One entry on the LocalPlayer Subsystem's IMC stack (E1). */
USTRUCT(BlueprintType)
struct FBridgeMappingContextEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString MappingContextPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	int32 Priority = 0;
};

/** Live runtime state of one IA in PIE (E2). */
USTRUCT(BlueprintType)
struct FBridgeInputActionState
{
	GENERATED_BODY()

	/** ETriggerEvent name: "None"/"Triggered"/"Started"/"Ongoing"/"Canceled"/"Completed". */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString TriggerEvent;

	/** Current FInputActionValue, projected to FVector (Y/Z = 0 for lower-rank types). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FVector Value = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	float ElapsedTriggeredTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	float ElapsedProcessedTime = 0.f;

	/** 没有已就绪的本地玩家 PIE 世界、没有活动 PlayerInput 或找不到该 IA 记录时为 false。 */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bHasState = false;
};

/** A row from DefaultInput.ini's AxisMappings (F1). */
USTRUCT(BlueprintType)
struct FBridgeLegacyAxisMapping
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString MappingName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString KeyName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	float Scale = 1.f;
};

/** A row from DefaultInput.ini's ActionMappings (F1). */
USTRUCT(BlueprintType)
struct FBridgeLegacyActionMapping
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString MappingName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString KeyName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bShift = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bCtrl = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bAlt = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	bool bCmd = false;
};

/** A single conflict found by D4 detect_key_conflicts. */
USTRUCT(BlueprintType)
struct FBridgeKeyConflict
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString MappingContextPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString KeyName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	TArray<FString> ActionPaths;
};

/** A single issue found by D3 validate_input_bindings. */
USTRUCT(BlueprintType)
struct FBridgeInputBindingIssue
{
	GENERATED_BODY()

	/** "imc_null_action" / "imc_dangling_action" / "event_node_null_ia" / "bind_action_null_ia". */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString Kind;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Agent")
	FString Detail;
};

/**
 * 为 PIE 自动化提供 Agent 传感、输入、相机及通用 Gameplay 操作。
 *
 * World policy 分为两个显式边界：
 * - 本地输入、相机/视口、玩家传感及活动输入状态只使用“首个已 BeginPlay 且 FirstPlayerController 拥有 LocalPlayer”的世界；
 *   客户端启动尚未就绪或纯服务端 PIE 时返回 false、空结果或约定哨兵，不回退到服务端世界。
 * - Spawn/Destroy、Damage、Physics、Pause/TimeDilation、GameMode、Teleport/Respawn 及权威属性写入继续使用
 *   Editor 上下文顺序中的首个有效 BeginPlay PIE 世界，以保留通用/权威语义。
 * - Sticky input 固定到创建时的本地玩家世界、FirstPlayerController、LocalPlayer 及 World Time；任一身份变化时删除，不迁移到替代对象。
 *
 * Provides agent sensing, input, camera, and general gameplay operations for PIE automation.
 * Local-player operations fail closed until a ready LocalPlayer world exists, while general/authority operations preserve first-valid context order.
 * Sticky input remains pinned to its originating world, first controller, local player, and world clock.
 */
UCLASS()
class UUnrealBridgeGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 采集已就绪本地玩家 PIE 世界中玩家 Pawn 的 FAgentObservation；客户端启动未就绪及纯服务端 PIE 显式失败。
	 *
	 * @param OutObservation 输出观测；读取其他字段前检查 bValid。
	 * @param MaxActorDistance 过滤半径，单位 cm；更远 Actor 不进入 VisibleActors。
	 * @param bRequireLineOfSight 为 true 时执行相机到 Actor 的射线检测并排除遮挡目标。
	 * @param ClassFilter 可选的不区分大小写类名子串；空字符串表示不过滤。
	 * @return 仅在已就绪本地玩家世界及其玩家 Pawn 均存在时返回 true；bValid 与返回值一致。
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
	// 下列输入执行器只作用于已就绪本地玩家世界的 FirstPlayerController/Pawn；客户端启动未就绪或纯服务端时返回 false。
	// The input actuators below target only the ready local-player world's first controller/pawn and fail closed during startup or server-only PIE.
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
	 * 向已就绪本地玩家的 EnhancedInput InputAction 注入单 tick 值；没有该本地玩家世界时返回 false，不回退服务端。
	 * Inject a single-tick value into an EnhancedInput InputAction for
	 * the ready local PIE player. Good for discrete "press" actions (IA_Jump,
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
	 * 设置粘滞 EnhancedInput 值并固定到当前已就绪本地玩家世界；每 tick 使用同一世界及 World Time 重注入。
	 * 原世界或本地玩家失效时条目被删除，绝不迁移到其他客户端或服务端；没有已就绪本地玩家时返回 false。
	 *
	 * Set a "sticky" EnhancedInput value pinned to the current ready local-player world: re-injected every GameThread
	 * tick until explicitly cleared, overwritten, or automatically removed when the captured action/world/player identity becomes invalid.
	 * The caller sets it once and movement/look input persists at UE frame rate while that identity remains valid, without Python needing to keep up. Multiple IAs can be sticky at the same time
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
	 * 在通用/权威 world policy 的首个有效 PIE 世界中硬重置 Pawn，不使用本地玩家 fail-closed 选择。
	 * Hard-reset the pawn's pose in the first-valid general/authority PIE world. Combines SetActorLocationAndRotation
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
	// 读取 API 使用已就绪本地玩家 Pawn；MaxWalkSpeed/GravityScale 写入使用首个有效通用/权威 PIE 世界。
	// Read APIs use the ready local-player pawn; MaxWalkSpeed/GravityScale mutations use the first-valid general/authority PIE world.
	// 非 Character Pawn 返回哨兵或 false；raw APawn 没有 CharacterMovementComponent。

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

	/**
	 * Read the pawn's traversal capabilities out of
	 * CharacterMovementComponent + CapsuleComponent — the numbers an agent
	 * needs to reason about "can I fit / jump / step up there". All values
	 * returned via out-params:
	 *
	 *   JumpZVelocity        cm/s — vertical launch speed from Jump().
	 *   MaxWalkSpeed         cm/s — current walking cap (post any runtime edit).
	 *   MaxStepHeight        cm   — height the movement comp will auto-step.
	 *   WalkableFloorAngleDeg deg — slopes shallower than this count as floor.
	 *   CapsuleRadius        cm   — half-width of the collision capsule.
	 *   CapsuleHalfHeight    cm   — half-height standing up.
	 *   CrouchedHalfHeight   cm   — half-height while crouched.
	 *   bCanCrouch           bool — whether MovementMode allows crouch.
	 *   bCanJump             bool — NavAgentProps.bCanJump.
	 *
	 * Returns false with zero-initialised out-params when there's no PIE
	 * pawn or the pawn isn't a Character.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetPawnCapabilities(
		float& JumpZVelocity, float& MaxWalkSpeed,
		float& MaxStepHeight, float& WalkableFloorAngleDeg,
		float& CapsuleRadius, float& CapsuleHalfHeight, float& CrouchedHalfHeight,
		bool& bCanCrouch, bool& bCanJump);

	/**
	 * Simulate a projectile arc from `StartLocation` with `InitialVelocity`,
	 * stepping ballistics at `StepDt` until we hit something, run out of
	 * `MaxTime`, or pass `MaxPathLength` in travelled distance.
	 *
	 * Uses the PIE world's gravity (pawn's GravityScale is NOT applied —
	 * pass a scaled velocity if you need to mimic the pawn's jump from
	 * inside a gravity-scaled area). Each step line-traces the segment so
	 * we stop the moment the trajectory would collide.
	 *
	 * Useful for "if I jump with this velocity, where do I land?" queries
	 * without actually commanding the pawn to jump.
	 *
	 * @return true when the arc hit something (floor or wall). OutLandLocation
	 *         is the impact; OutLandActorLabel names the hit actor.
	 *         Returns false when the arc timed out in midair — OutLandLocation
	 *         is the final simulated position.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SimulateJumpArc(
		const FVector& StartLocation, const FVector& InitialVelocity,
		float MaxTime, float StepDt, float MaxPathLength,
		FVector& OutLandLocation, FString& OutLandActorLabel);

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
	 * Draw a debug line segment in the PIE viewport.
	 *
	 * Duration semantics:
	 *   > 0 : line decays after that many seconds (most common case)
	 *   = 0 : single-frame draw (next tick erases it)
	 *   < 0 : persistent — stays until FlushPersistentDebugDraws() is called
	 *
	 * (This API used to invert the persistent flag so any positive duration
	 * left the line on forever; fixed so the duration does what it says.)
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool DrawDebugLine(const FVector& Start, const FVector& End, float Thickness = 1.0f, float DurationSeconds = 5.0f);

	/**
	 * Draw a debug wireframe sphere in the PIE viewport.
	 * Same Duration semantics as DrawDebugLine: > 0 decays, 0 one frame,
	 * < 0 persistent until flush.
	 */
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

	/**
	 * Inspect the Triggers array configured on an InputAction asset.
	 * Returns parallel arrays: trigger class short-name (e.g. "Pressed",
	 * "Released", "Hold", "Tap", "Pulse", "ChordAction", "Down") and a
	 * threshold (seconds) parsed from HoldTimeThreshold / TapReleaseTimeThreshold
	 * / Interval — 0.0f when the trigger has no timing parameter.
	 *
	 * Note: this only inspects triggers declared on the IA itself. Per-mapping
	 * triggers added inside a UInputMappingContext are NOT reflected here.
	 * Returns false if the IA fails to load.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool GetInputActionTriggers(
		const FString& InputActionPath,
		TArray<FString>& OutTriggerNames,
		TArray<float>& OutThresholdSeconds);

	/**
	 * 自适应触发 Bool IA；仅使用已就绪本地玩家世界。Sticky hold 的截止时间与重注入固定到创建时的同一世界。
	 * 客户端启动未就绪或纯服务端 PIE 时返回 false，且不会稍后迁移/补发到其他世界。
	 *
	 * Adaptive "press a Bool IA" entry point. Inspects the IA's Triggers
	 * and picks between single-tick inject vs. sticky-then-auto-release so
	 * callers don't have to know which pattern fits a given action.
	 *
	 * HoldSeconds semantics:
	 *   < 0 (default): auto-pick from Triggers. If any Hold/Tap/Released/Pulse
	 *       trigger is present, use a sticky hold sized to satisfy the
	 *       timing threshold (Hold duration + 50 ms, or Tap threshold * 0.5).
	 *       Otherwise fire a single-tick pulse.
	 *   == 0: force single-tick pulse (same as InjectEnhancedInputAxis).
	 *   > 0: force sticky hold for exactly this many seconds, then auto-clear.
	 *
	 * The auto-clear runs on the GameThread ticker using World time, so it
	 * tracks time-dilation correctly — a Hold(1.0) trigger will still fire
	 * when the game is slowed down 0.5×, because both our deadline and UE's
	 * hold timer use the same clock.
	 *
	 * For Axis1D / Axis2D / Axis3D IAs (continuous inputs like IA_Move,
	 * IA_Look), this function refuses and returns false — use SetStickyInput
	 * with an explicit release point instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool TriggerInputAction(const FString& InputActionPath, float HoldSeconds = -1.0f);

	// ─── Enhanced Input authoring (IA / IMC enumeration + binding) ───
	//
	// The runtime-injection surface above lets an agent press buttons; this
	// block lets it discover what's there, inspect the binding configuration
	// of an IMC, and add/remove key bindings. Pairs with
	// `add_node_by_class_name` (in BlueprintLibrary) for wiring IA→callbacks
	// on a Pawn BP.

	/**
	 * Find every UInputAction asset matching an optional content-path prefix.
	 * @param ContentPathFilter  E.g. "/Game/Input/" — empty matches all.
	 * @param MaxResults         0 = unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FString> ListInputActions(const FString& ContentPathFilter, int32 MaxResults = 0);

	/** Same as ListInputActions, for UInputMappingContext assets. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FString> ListInputMappingContexts(const FString& ContentPathFilter, int32 MaxResults = 0);

	/**
	 * Create a new UInputAction asset at PackagePath. Returns the full asset
	 * path on success, empty string on failure (path already exists, parent
	 * folder invalid, save failed, ...).
	 *
	 * @param PackagePath  Content path WITHOUT extension, e.g. "/Game/Input/IA_Sprint".
	 * @param ValueType    "Boolean" / "Axis1D" / "Axis2D" / "Axis3D" — case-insensitive.
	 *                     Defaults to "Boolean" if unrecognized.
	 * @param Description  Optional. Goes into the IA's `ActionDescription` field.
	 * @param bSave        Save the asset to disk after creation. Default true.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString CreateInputAction(const FString& PackagePath, const FString& ValueType,
		const FString& Description = TEXT(""), bool bSave = true);

	/**
	 * Create a new UInputMappingContext asset at PackagePath. Returns the full
	 * asset path on success, empty string on failure.
	 *
	 * @param PackagePath  Content path WITHOUT extension, e.g. "/Game/Input/IMC_Player".
	 * @param Description  Optional. Goes into the IMC's `ContextDescription` field
	 *                     (5.4+; ignored on older versions where the field is absent).
	 * @param bSave        Save the asset to disk after creation. Default true.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString CreateInputMappingContext(const FString& PackagePath,
		const FString& Description = TEXT(""), bool bSave = true);

	/**
	 * Append a UInputTrigger instance to IA->Triggers. The new instance has
	 * its UPROPERTY values populated from `ParamsJson` (a JSON object whose
	 * keys are property names; e.g. {"HoldTimeThreshold": 0.5} for Hold).
	 *
	 * @param InputActionPath  e.g. "/Game/Input/IA_Sprint.IA_Sprint".
	 * @param TriggerClass     Short name ("Hold", "Pressed", "Tap", "Pulse",
	 *                         "Released", "HoldAndRelease", "Down", "Combo",
	 *                         "ChordAction", "ChordBlocker", "RepeatedTap"),
	 *                         long name ("InputTriggerHold"), or full class
	 *                         path ("/Script/EnhancedInput.InputTriggerHold").
	 * @param ParamsJson       JSON object of {property_name: value}. Empty
	 *                         "{}" means use defaults.
	 * @param bSave            Save the IA package after editing. Default true.
	 * @return Index of the new entry in Triggers[], or -1 on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 AddTriggerToIA(const FString& InputActionPath,
		const FString& TriggerClass, const FString& ParamsJson = TEXT("{}"),
		bool bSave = true);

	/**
	 * Append a UInputModifier instance to IA->Modifiers, with UPROPERTY values
	 * populated from `ParamsJson`.
	 *
	 * @param ModifierClass  Short name ("Negate", "Scalar", "DeadZone",
	 *                       "Smooth", "SwizzleAxis", "ToWorldSpace",
	 *                       "ScaleByDeltaTime", "FOVScaling",
	 *                       "ResponseCurveExponential", "ResponseCurveUser",
	 *                       "SmoothDelta"), long name ("InputModifierNegate"),
	 *                       or full class path.
	 * @return Index of the new entry in Modifiers[], or -1 on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 AddModifierToIA(const FString& InputActionPath,
		const FString& ModifierClass, const FString& ParamsJson = TEXT("{}"),
		bool bSave = true);

	/**
	 * Remove the entry at `Index` from IA->Triggers. Use a negative index to
	 * mean "from end" (Python-style: -1 = last). Returns true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveTriggerFromIA(const FString& InputActionPath, int32 Index, bool bSave = true);

	/** Symmetric remover for IA->Modifiers. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveModifierFromIA(const FString& InputActionPath, int32 Index, bool bSave = true);

	// ─── A3 / A9 / A10 — IA/IMC asset metadata + duplicates ─────────

	/**
	 * Set a UPROPERTY on a UInputAction asset by name. Common targets:
	 * "ValueType" ("Boolean"/"Axis1D"/"Axis2D"/"Axis3D" — string mapped to enum;
	 * other props use raw JSON value). Examples:
	 *   set_input_action_property(ia, "bConsumeInput", "true")
	 *   set_input_action_property(ia, "bTriggerWhenPaused", "false")
	 *   set_input_action_property(ia, "ActionDescription", "\"My description\"")
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetInputActionProperty(const FString& InputActionPath,
		const FString& PropertyName, const FString& JsonValue, bool bSave = true);

	/**
	 * Set the PlayerMappableKeySettings asset reference on an IMC mapping
	 * (per-mapping data; UE 5.4+ uses a UPlayerMappableKeySettings asset
	 * pointer; on 5.3 the path is `FPlayerMappableKeyOptions` struct fields).
	 * Empty PMKS path = clear the reference.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SetIMCMappingPlayerMappableKeySettings(const FString& MappingContextPath,
		const FString& InputActionPath, const FString& KeyName,
		const FString& PlayerMappableKeySettingsPath, bool bSave = true);

	/** Duplicate an existing IA to a new content path. Returns new asset path or empty on failure. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString DuplicateInputAction(const FString& SourcePath, const FString& DestPath, bool bSave = true);

	/** Duplicate an existing IMC. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FString DuplicateInputMappingContext(const FString& SourcePath, const FString& DestPath, bool bSave = true);

	// ─── A7 / A8 — IMC per-mapping triggers + modifiers ─────────────

	/** Append a UInputTrigger instance to a specific IA→Key mapping in an IMC. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 AddTriggerToIMCMapping(const FString& MappingContextPath,
		const FString& InputActionPath, const FString& KeyName,
		const FString& TriggerClass, const FString& ParamsJson = TEXT("{}"),
		bool bSave = true);

	/** Append a UInputModifier instance to a specific IA→Key mapping in an IMC. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 AddModifierToIMCMapping(const FString& MappingContextPath,
		const FString& InputActionPath, const FString& KeyName,
		const FString& ModifierClass, const FString& ParamsJson = TEXT("{}"),
		bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveTriggerFromIMCMapping(const FString& MappingContextPath,
		const FString& InputActionPath, const FString& KeyName, int32 Index, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveModifierFromIMCMapping(const FString& MappingContextPath,
		const FString& InputActionPath, const FString& KeyName, int32 Index, bool bSave = true);

	// ─── Read API — full trigger/modifier dump (companion to A4/A6) ──

	/** Like get_input_action_triggers but returns each trigger's full UPROPERTY
	 *  set as JSON (round-trip-able with AddTriggerToIA / Import G1). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeInputComponentInstance> GetInputActionTriggersFull(const FString& InputActionPath);

	/** Same for IA->Modifiers. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeInputComponentInstance> GetInputActionModifiersFull(const FString& InputActionPath);

	/** Filtered list_input_actions: empty ValueTypeFilter = all; otherwise
	 *  "Boolean"/"Axis1D"/"Axis2D"/"Axis3D" (case-insensitive). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FString> ListInputActionsByValueType(const FString& ContentPathFilter,
		const FString& ValueTypeFilter, int32 MaxResults = 0);

	// ─── D1 / D2 — find references ──────────────────────────────────

	/** Find every reference to an IA across the project (IMC mappings + BP nodes). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeInputReference> FindInputActionReferences(const FString& InputActionPath,
		const FString& BlueprintPackagePathFilter = TEXT(""));

	/** Find every reference to an IMC (BP CallFunction args + Pawn DefaultPawnInputMappingContext field). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeInputReference> FindInputMappingContextReferences(const FString& MappingContextPath,
		const FString& BlueprintPackagePathFilter = TEXT(""));

	// ─── E1 / E2 / E4 — PIE runtime state ───────────────────────────

	/** 读取已就绪本地玩家 Subsystem 的活动 IMC 栈；客户端未就绪或纯服务端 PIE 返回空数组。 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeMappingContextEntry> GetActiveMappingContextStack();

	/** 读取已就绪本地玩家的 IA 运行时状态；客户端未就绪或纯服务端 PIE 返回 bHasState=false。 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static FBridgeInputActionState GetCurrentInputActionState(const FString& InputActionPath);

	/** Dump the bridge's currently pending one-shot inject queue (E4 part 2 — sticky already exposed via GetStickyInputs). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static int32 DumpInjectedInputQueue(TArray<FString>& OutPaths, TArray<FVector>& OutValues, TArray<float>& OutHoldRemainingSeconds);

	// ─── E5 — raw key event simulation ──────────────────────────────

	/** Inject a Slate key event (covers menus / Slate / legacy InputComponent paths). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool SimulateKeyEvent(const FString& KeyName, bool bPressed, int32 UserIndex = 0);

	// ─── F1 / F2 / F3 — legacy InputAxis/Action mappings (.ini) ─────

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeLegacyAxisMapping> ListLegacyAxisMappings();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeLegacyActionMapping> ListLegacyActionMappings();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddLegacyAxisMapping(const FString& MappingName, const FString& KeyName, float Scale = 1.f);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddLegacyActionMapping(const FString& MappingName, const FString& KeyName,
		bool bShift = false, bool bCtrl = false, bool bAlt = false, bool bCmd = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveLegacyAxisMapping(const FString& MappingName, const FString& KeyName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveLegacyActionMapping(const FString& MappingName, const FString& KeyName);

	// ─── D3 / D4 — validation ───────────────────────────────────────

	/** Run lint over the project's input bindings. Empty `BlueprintPackagePathFilter` scans all `/Game/...` BPs. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeInputBindingIssue> ValidateInputBindings(const FString& BlueprintPackagePathFilter = TEXT(""));

	/** Find keys bound to multiple IAs within a single IMC, or across the supplied set of IMCs. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeKeyConflict> DetectKeyConflicts(const TArray<FString>& MappingContextPaths);

	/**
	 * Enumerate the IA→Key mappings (with per-mapping Trigger / Modifier
	 * classes) on a UInputMappingContext asset. Reads via the public
	 * GetMappings() accessor, which returns DefaultKeyMappings.Mappings on
	 * 5.7+ and the deprecated `Mappings` field on 5.3-5.6 — same surface,
	 * version-portable.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static TArray<FBridgeIMCMapping> GetInputMappingContextMappings(const FString& MappingContextPath);

	/**
	 * Add an InputAction → Key binding to an IMC asset.
	 * Calls UInputMappingContext::MapKey under FScopedTransaction so the edit
	 * is undoable. Triggers / Modifiers are not configured here — UE creates
	 * the mapping with empty arrays; the IA's own triggers still apply.
	 *
	 * @param KeyName  FKey identifier, e.g. "SpaceBar", "LeftMouseButton",
	 *                 "Gamepad_FaceButton_Bottom". Parsed via FKey::FromString.
	 * @return true on success; false if asset load fails or KeyName is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool AddIAMappingToIMC(
		const FString& MappingContextPath,
		const FString& InputActionPath,
		const FString& KeyName);

	/**
	 * Remove a mapping from an IMC. If KeyName is empty, removes EVERY
	 * binding of InputActionPath in the IMC (UnmapAllKeysFromAction);
	 * otherwise unmaps just the specified IA→Key pair.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool RemoveIAMappingFromIMC(
		const FString& MappingContextPath,
		const FString& InputActionPath,
		const FString& KeyName);

	// ─── Extended debug drawing ──────────────────────────────────────
	//
	// Companions to DrawDebugLine / DrawDebugSphereAt. All require PIE.
	// Duration semantics (same across the whole family):
	//   > 0 : decays after that many seconds
	//   = 0 : single-frame draw
	//   < 0 : persistent until FlushPersistentDebugDraws() is called

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
	 * Resolve a skeletal mesh component on the PIE player pawn. Empty
	 * ComponentName prefers ACharacter::GetMesh(), then the first skeletal
	 * mesh component. A non-empty name must match the component object name.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static USkeletalMeshComponent* GetPlayerSkeletalMeshComponent(const FString& ComponentName = TEXT(""));

	/** AnimInstance currently owned by GetPlayerSkeletalMeshComponent(). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static UAnimInstance* GetPlayerAnimInstance(const FString& ComponentName = TEXT(""));

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

	/**
	 * Inject a key press/release event through FSlateApplication.
	 * Bypasses Enhanced Input entirely — the event is processed by
	 * whatever Slate widget currently has focus (UMG menus, CommonUI, etc.).
	 *
	 * @param KeyName   FKey name string, e.g. "Gamepad_FaceButton_Bottom",
	 *                  "Gamepad_DPad_Down", "SpaceBar", "Enter", "BackSpace".
	 * @param bPressed  true = key down, false = key up.
	 * @return true if the event was dispatched.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Agent")
	static bool PressKey(const FString& KeyName, bool bPressed = true);
};

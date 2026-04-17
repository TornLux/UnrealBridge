#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeReactiveTypes.h"
#include "UnrealBridgeReactiveLibrary.generated.h"


/**
 * Python-facing entry points for the reactive-handler framework. Agents
 * register a Python script + metadata against a UE event source; when the
 * event fires, the C++ side runs the script on the GameThread with the
 * event payload injected as globals (see bridge-reactive.md for the
 * context-key contract).
 *
 * Runtime vs editor entry points are split ("register_runtime_*" /
 * "register_editor_*") so the agent's intent is explicit. All handlers
 * share one registry internally — they survive PIE transitions, are
 * cleaned up on world cleanup (for subjects tied to the old world), and
 * are listed together via list_all_handlers.
 *
 * HandlerIds are system-issued ("rt_<seq>_<rand4>" / "ed_<seq>_<rand4>")
 * and returned on register. Agents must store the id to later unregister
 * or pause. Registering again with the same id is not supported — call
 * unregister(id) first.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeReactiveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ─── Runtime-domain registration ────────────────────────────

	/**
	 * Register a Python script to run when TargetActor's ASC receives a
	 * GameplayEvent matching EventTag. Returns the new HandlerId on success,
	 * empty string on failure (bad actor, no ASC on the actor, tag not
	 * registered, missing required metadata).
	 *
	 * @param TaskName     Short human-readable label (required). Shown by list_all_handlers.
	 * @param Description  Longer text explaining what this handler does (required).
	 * @param TargetActorName  Actor label or FName. Resolved against the PIE world first,
	 *                     then the editor world. Actor must have a UAbilitySystemComponent
	 *                     (via IAbilitySystemInterface or as a direct component).
	 * @param EventTag     GameplayTag string, e.g. "Event.Combat.Hit". Must be registered.
	 * @param Script       Python source. See bridge-reactive.md for available globals (ctx,
	 *                     event_instigator, event_target, event_magnitude, bridge_state, state,
	 *                     log, defer_to_next_tick, …).
	 * @param ScriptPath   Optional: path to the source file on disk, recorded for
	 *                     introspection (shown by list_all_handlers).
	 * @param Tags         Optional: free-form labels for filtering via list_all_handlers.
	 * @param Lifetime     "Permanent" (default), "Once", "Count:N" (e.g. "Count:5"),
	 *                     "WhilePIE", "WhileSubjectAlive".
	 * @param ErrorPolicy  "LogContinue" (default), "LogUnregister", "Throw".
	 * @param ThrottleMs   Minimum milliseconds between invocations (0 = unlimited).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeGameplayEvent(
		const FString& TaskName,
		const FString& Description,
		const FString& TargetActorName,
		const FString& EventTag,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire when TargetActor's ASC attribute changes value. AttributeName accepts
	 * "AttrSet.Field" (e.g. "BridgeTestAttrSet.Health") or bare "Field".
	 * Context keys: attribute_name, new_value, old_value, delta.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeAttributeChanged(
		const FString& TaskName,
		const FString& Description,
		const FString& TargetActorName,
		const FString& AttributeName,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire when TargetActor's OnDestroyed or OnEndPlay dynamic delegate fires.
	 * @param EventType  "Destroyed" or "EndPlay".
	 * Context keys: event, actor, end_play_reason.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeActorLifecycle(
		const FString& TaskName,
		const FString& Description,
		const FString& TargetActorName,
		const FString& EventType,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire when TargetActor (must be an ACharacter) changes MovementMode.
	 * Handler script can filter by prev_mode/current_mode via ctx.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeMovementModeChanged(
		const FString& TaskName,
		const FString& Description,
		const FString& TargetActorName,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire when an AnimNotify named NotifyName plays on TargetActor's skeletal
	 * mesh's AnimInstance. Adapter binds UAnimInstance::OnPlayMontageNotifyBegin.
	 * Context keys: notify_name, anim_instance, mesh_component, owner_actor, montage.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeAnimNotify(
		const FString& TaskName,
		const FString& Description,
		const FString& TargetActorName,
		const FString& NotifyName,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire on EnhancedInput action trigger. Binds via UEnhancedInputComponent::
	 * BindAction against TargetActor's (or its Controller's) input component.
	 * @param InputActionPath  e.g. "/Game/Input/IA_Jump".
	 * @param TriggerEvent     "Triggered"|"Started"|"Ongoing"|"Canceled"|"Completed".
	 * Context keys: action_path, action_name, trigger_event, value_type,
	 *   value_bool, value_axis1d, value_axis2d_x/y, value_axis3d_x/y/z, elapsed_seconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeInputAction(
		const FString& TaskName,
		const FString& Description,
		const FString& TargetActorName,
		const FString& InputActionPath,
		const FString& TriggerEvent,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Register a Python script that runs periodically at an interval. Driven
	 * by a single multiplexed FTSTicker inside the Timer adapter — one ticker
	 * services all registered timers; each fires independently when its own
	 * interval has elapsed.
	 *
	 * @param IntervalSeconds  Seconds between fires. Must be > 0. Use small
	 *                         values (e.g. 0.033) for ~per-frame, but bear in
	 *                         mind each fire runs a Python exec on the
	 *                         GameThread — don't go under ~0.03 lightly.
	 *
	 * Context keys: trigger ('timer'), interval_seconds, elapsed_since_last,
	 *   fire_count, world_time, editor_time.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterRuntimeTimer(
		const FString& TaskName,
		const FString& Description,
		float IntervalSeconds,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	// ─── Editor-domain registration (P5) ────────────────────────

	/**
	 * Fire when the AssetRegistry reports an asset-lifecycle event.
	 *
	 * @param EventFilter  "" matches any of the four events; otherwise must be
	 *                     one of "Added", "Removed", "Renamed", "Updated".
	 *                     A bad filter string is refused (returns "").
	 *
	 * Subject: always null (global). Script can filter further via
	 * ctx['asset_class'] / ctx['package_name'] — e.g. only react to Material
	 * renames, or ignore everything outside a folder.
	 *
	 * Context keys: trigger ('asset_event'), event ('Added'|'Removed'|'Renamed'
	 *   |'Updated'), asset_path, asset_class, package_name, old_path (empty
	 *   unless event=='Renamed').
	 *
	 * Startup scan: during the initial discovery scan at editor startup the
	 * AssetRegistry fires OnAssetAdded for every asset in the project. Agents
	 * that register this handler DURING startup will see that flood — v1
	 * doesn't gate. Register after IsLoadingAssets() returns false if that's
	 * a problem (it usually isn't, because agent sessions start long after
	 * the scan completes).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterEditorAssetEvent(
		const FString& TaskName,
		const FString& Description,
		const FString& EventFilter,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire on Play-in-Editor phase transitions.
	 *
	 * @param PhaseFilter  "" matches any phase; otherwise one of
	 *                     "PreBeginPIE", "BeginPIE", "PostPIEStarted",
	 *                     "PrePIEEnded", "EndPIE", "PausePIE", "ResumePIE",
	 *                     "SingleStepPIE". Bad filter is refused.
	 *
	 * Subject: always null. Context keys: trigger ('pie_event'), phase,
	 *   is_simulating (True for Simulate-in-Editor, False for Play).
	 *
	 * ⚠️ Ordering gotcha: the subsystem core EndPIE hook (which purges
	 * WhilePIE handlers) runs BEFORE this adapter's EndPIE dispatch. So
	 * WhilePIE-lifetime handlers listening for "EndPIE" get removed before
	 * their own event fires. To observe EndPIE, use Permanent lifetime — or
	 * use "PrePIEEnded" which fires before the purge.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterEditorPieEvent(
		const FString& TaskName,
		const FString& Description,
		const FString& PhaseFilter,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	/**
	 * Fire when a Blueprint is compiled.
	 *
	 * @param BlueprintPathFilter  "" → global (hooks GEditor->OnBlueprintCompiled,
	 *                             but no BP payload — ctx['blueprint_path'] is
	 *                             empty). Otherwise an asset path like
	 *                             "/Game/.../BP_MyActor" resolves a specific
	 *                             UBlueprint and binds its OnCompiled() event;
	 *                             ctx exposes blueprint_path + parent_class.
	 *
	 * Selector: always NAME_None (no sub-types). Agents scope per-BP by
	 * providing a path, or listen globally with "".
	 *
	 * Context keys: trigger ('bp_compiled'), blueprint_path, parent_class.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FString RegisterEditorBpCompiled(
		const FString& TaskName,
		const FString& Description,
		const FString& BlueprintPathFilter,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs);

	// ─── Common management ──────────────────────────────────────

	/** Remove a handler by id. Returns true when the id was known. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static bool Unregister(const FString& HandlerId);

	/**
	 * List all registered handlers, optionally filtered.
	 * @param FilterScope       "" matches all, "runtime", or "editor".
	 * @param FilterTriggerType "" matches all, otherwise an EBridgeTrigger name like "GameplayEvent".
	 * @param FilterTag         "" matches all, otherwise a tag string that must be in a handler's Tags.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static TArray<FBridgeHandlerSummary> ListAllHandlers(
		const FString& FilterScope,
		const FString& FilterTriggerType,
		const FString& FilterTag);

	/** Full record for one handler, including the script source. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FBridgeHandlerDetail GetHandler(const FString& HandlerId);

	/** Current stats (calls, avg µs, max µs, last error). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static FBridgeHandlerStats GetHandlerStats(const FString& HandlerId);

	/** Pause a handler without removing it — skipped by Dispatch until Resume(). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static bool Pause(const FString& HandlerId);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static bool Resume(const FString& HandlerId);

	/** Clear all handlers in a scope. Scope: "runtime", "editor", or "all". */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static int32 ClearAll(const FString& Scope);

	/**
	 * Queue a Python snippet to run on the next GameThread tick. Useful from
	 * inside a handler that wants to kick off heavy work without hitching the
	 * event frame. Also exposed to handler scripts as a bare
	 * `defer_to_next_tick(src)` function in the preamble.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static void DeferToNextTick(const FString& Script);

	/**
	 * Docs helper: return the ctx-key spec injected by the adapter for a given
	 * trigger type. Keys are the Python variable names the handler script can
	 * reference; values describe their Python type / meaning. Useful for
	 * agents to discover the context-key contract without leaving Python.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Reactive")
	static TMap<FString, FString> DescribeTriggerContext(const FString& TriggerType);
};

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "UnrealBridgeReactiveTypes.h"
#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.generated.h"

/**
 * Full handler record stored in the registry. Not a USTRUCT — exposed via
 * FBridgeHandlerSummary / FBridgeHandlerDetail to Blueprints/Python.
 */
struct FBridgeHandlerRecord
{
	FString HandlerId;
	FString Scope;             // "runtime" or "editor"
	FString TaskName;
	FString Description;
	TArray<FString> Tags;
	FString ScriptPath;
	FString Script;

	EBridgeTrigger TriggerType = EBridgeTrigger::None;
	TWeakObjectPtr<UObject> Subject;
	FName Selector;

	/** Adapter-specific free-form payload set by the library entry point.
	 *  InputAction stores the UInputAction path here; other adapters ignore it. */
	FString AdapterPayload;

	/**
	 * User-provided registration params captured by the library entry point
	 * (target_actor_name, event_tag, interval_seconds, attribute_name, …).
	 * Used by the persistence layer to re-resolve Subject / Selector /
	 * AdapterPayload across editor restarts, since the raw Subject pointer
	 * is a PIE-tied weak ref that won't survive a session boundary.
	 * Consequently this map is the load-bearing identity of the handler —
	 * not Subject/Selector on their own.
	 */
	TMap<FString, FString> RegistrationContext;

	EBridgeHandlerLifetime Lifetime = EBridgeHandlerLifetime::Permanent;
	int32 RemainingCalls = -1;
	EBridgeErrorPolicy ErrorPolicy = EBridgeErrorPolicy::LogContinue;
	int32 ThrottleMs = 0;
	bool bPaused = false;

	FDateTime CreatedAt;
	FBridgeHandlerStats Stats;

	/** For throttle enforcement (platform time, seconds). */
	double LastFirePlatformSeconds = 0.0;
};

/**
 * Registry + dispatcher for Python event handlers. Single UEditorSubsystem
 * holds both runtime-domain (GAS, Anim, Movement, Input, …) and
 * editor-domain (AssetRegistry, PIE, BP compile) handlers — the scope is
 * recorded on each record but storage is unified so handlers survive PIE
 * transitions.
 *
 * Threading: all mutation + dispatch happens on the GameThread. The bridge
 * TCP server already serializes Python exec calls through a GameThread
 * ticker, so registration paths are GameThread too. Adapters call
 * Dispatch(...) from UE delegate callbacks, which are GameThread as well.
 */
UCLASS()
class UNREALBRIDGE_API UBridgeReactiveSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// UEditorSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Convenience accessor. Returns nullptr outside the editor. */
	static UBridgeReactiveSubsystem* Get();

	// ─── Registration (called by UnrealBridgeReactiveLibrary entry points) ──

	/**
	 * Add a new handler. Returns the generated HandlerId on success, empty
	 * string on failure (bad input, TaskName empty, Subject required by
	 * trigger but null, etc.). `Record.HandlerId` is ignored on input.
	 */
	FString RegisterHandler(FBridgeHandlerRecord&& Record);

	/** Remove by id. Returns true if removed. */
	bool UnregisterHandler(const FString& HandlerId);

	bool PauseHandler(const FString& HandlerId);
	bool ResumeHandler(const FString& HandlerId);

	/** Scope: "runtime", "editor", or "all". Returns count removed. */
	int32 ClearHandlers(const FString& Scope);

	// ─── Introspection ──────────────────────────────────────────

	TArray<FBridgeHandlerSummary> ListAllHandlers(
		const FString& FilterScope,
		const FString& FilterTriggerTypeName,
		const FString& FilterTag) const;

	bool GetHandler(const FString& HandlerId, FBridgeHandlerDetail& OutDetail) const;
	bool GetStats(const FString& HandlerId, FBridgeHandlerStats& OutStats) const;

	// ─── Dispatch (called by adapters when their UE delegate fires) ─────

	/**
	 * Fire all handlers matching (TriggerType, Subject, Selector).
	 * `ContextLiterals` maps Python variable names → Python source-literal
	 * expressions (e.g. "'Event.Combat.Hit'", "12.5",
	 * "unreal.load_object(None, '/Game/...')"). The subsystem assembles
	 * these into the preamble and hands the combined script to
	 * IPythonScriptPlugin::ExecPythonCommandEx.
	 */
	void Dispatch(
		EBridgeTrigger TriggerType,
		TWeakObjectPtr<UObject> Subject,
		FName Selector,
		const TMap<FString, FString>& ContextLiterals);

	/**
	 * Fire a specific handler by id, bypassing TriggerType/Subject/Selector
	 * filtering. Used by adapters whose "fire" source is internally generated
	 * (e.g. Timer) and that want to target exactly one registered handler.
	 * Still honours pause, throttle, lifetime, error policy, stats, and the
	 * depth guard.
	 */
	void DispatchOne(
		const FString& HandlerId,
		const TMap<FString, FString>& ContextLiterals);

	// ─── Persistent ticker registry (for C++ internal callbacks) ────

	/**
	 * Register a C++ callback to run every editor tick until explicitly
	 * removed or the subsystem deinitializes. Not exposed to Python; used
	 * by internal bridge systems that need self-driving periodic work
	 * (e.g. sticky-input injection). Deinitialize unregisters all.
	 *
	 * @param Callback  Returns true to keep ticking, false to auto-remove.
	 * @param DebugName Appears in logs on cleanup.
	 * @return Opaque handle for UnregisterPersistentTicker; invalid on failure.
	 */
	FTSTicker::FDelegateHandle RegisterPersistentTicker(
		TFunction<bool(float)> Callback,
		const FString& DebugName);

	void UnregisterPersistentTicker(FTSTicker::FDelegateHandle& Handle);

	// ─── Adapter access ─────────────────────────────────────────

	IBridgeReactiveAdapter* FindAdapter(EBridgeTrigger TriggerType) const;

	/** Documentation helper — returns the context-key spec of a trigger. */
	TMap<FString, FString> DescribeTriggerContext(const FString& TriggerTypeName) const;

	// ─── Cross-handler deferred execution ───────────────────────

	/** Queue a Python snippet to run next GameThread tick (not in the event frame). */
	void DeferToNextTick(const FString& Script);

	// ─── Persistence (P6.B3) ────────────────────────────────────
	//
	// All non-WhilePIE handlers auto-persist to a JSON file in the
	// project's Saved/UnrealBridge/ folder. Save is debounced 100 ms after
	// any registry mutation (Register / Unregister / Clear). Load runs
	// once at subsystem Initialize. PIE-tied subjects that can't resolve
	// at editor startup get parked in DeferredHandlers and retried on
	// FEditorDelegates::PostPIEStarted.

	/** Write the current registry to disk immediately (bypassing the debounce). */
	bool SaveAllHandlers();

	/** Replace the in-memory registry with the on-disk file contents.
	 *  Returns the number of handlers restored (excludes deferred). */
	int32 LoadAllHandlers();

	/** Absolute path used for save/load. Default: <ProjectSaved>/UnrealBridge/reactive-handlers.json */
	static FString GetPersistencePath();

	/** Number of handlers waiting for their PIE-tied subject to resolve.
	 *  Retries on PostPIEStarted; surfaces via list_all_handlers for
	 *  agent introspection (with subject_path = "<unresolved>"). */
	int32 GetDeferredHandlerCount() const;

	/** Internal: restore a single record without reissuing its HandlerId.
	 *  Used by LoadAllHandlers + deferred-retry paths. Caller must have
	 *  populated Subject / Selector / AdapterPayload already. */
	FString RestoreSingleRecord(FBridgeHandlerRecord&& Record);

	/** Flag the registry as modified so the debounce ticker saves in ~100ms.
	 *  Called internally from Register / Unregister / Clear. Public so
	 *  external mutators (e.g. BpCompiled adapter's per-subject binding
	 *  state) can re-persist if they ever mutate Record fields. */
	void MarkDirty();

private:
	void RegisterAdapter(TUniquePtr<IBridgeReactiveAdapter> Adapter);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePieEnded(bool bIsSimulating);

	/** Build the per-fire Python preamble and concatenate the user script. */
	static FString BuildWrappedScript(
		const FBridgeHandlerRecord& Record,
		const TMap<FString, FString>& ContextLiterals);

	/** Execute a fully-assembled script via IPythonScriptPlugin. */
	static bool ExecutePythonScript(const FString& FullScript, FString& OutError);

	/** System-issued id: "<scope>_<seq>_<rand4>". */
	FString IssueHandlerId(const FString& Scope);

	/** Core dispatch, called on GameThread. */
	void DispatchLocked(
		EBridgeTrigger TriggerType,
		const TWeakObjectPtr<UObject>& Subject,
		const FName& Selector,
		const TMap<FString, FString>& ContextLiterals);

	/** Execute a single handler's script once, handling pause / throttle /
	 *  lifetime / error policy / stats. Shared between Dispatch (filtered
	 *  fan-out) and DispatchOne (targeted fire by id). Appends the handler
	 *  id to OutToRemove when it should be unregistered after this call. */
	void ExecuteHandlerOnce(
		FBridgeHandlerRecord& Record,
		const TMap<FString, FString>& ContextLiterals,
		TArray<FString>& OutToRemove);

	void RemoveByIdInternal(const FString& HandlerId);

	TMap<FString, TSharedRef<FBridgeHandlerRecord>> Handlers;
	TArray<TUniquePtr<IBridgeReactiveAdapter>> Adapters;

	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle PieEndedHandle;
	FTSTicker::FDelegateHandle DeferTickerHandle;

	/** Handles registered via RegisterPersistentTicker, cleaned up on Deinitialize. */
	TArray<FTSTicker::FDelegateHandle> PersistentTickers;

	TArray<FString> DeferredScripts;

	int32 RuntimeSeq = 0;
	int32 EditorSeq = 0;
	int32 DispatchDepth = 0;

	// ─── Persistence state ──────────────────────────────────────
	bool bPersistenceDirty = false;
	FTSTicker::FDelegateHandle PersistenceDebounceHandle;
	FDelegateHandle PostPIEStartedHandle;
	/** Handlers whose Subject didn't resolve at load time; retried on PostPIEStarted. */
	TArray<FBridgeHandlerRecord> DeferredHandlers;

	void HandlePostPIEStarted(bool bIsSimulating);
	/** Try to re-resolve every DeferredHandler via the library's resolver;
	 *  successful ones are inserted into Handlers and removed from the queue. */
	void RetryDeferredHandlers();

	/** Move a live handler into DeferredHandlers without marking the
	 *  persistence layer dirty. Used by world-cleanup: the record is still
	 *  persisted (save walks both Handlers + DeferredHandlers), but no
	 *  longer bound to any UE delegate. PostPIEStarted retries restore. */
	void MoveHandlerToDeferred(const FString& HandlerId);

	/** Serialize the current registry to a JSON string. */
	FString BuildPersistenceJson() const;
	/** Parse JSON + populate fresh registry. Handlers whose subject can't
	 *  be resolved yet get pushed to DeferredHandlers. */
	int32 RestoreFromJson(const FString& JsonText);
};

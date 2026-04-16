#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "UnrealBridgeReactiveTypes.generated.h"

/** Which UE event source a handler binds to. */
UENUM(BlueprintType)
enum class EBridgeTrigger : uint8
{
	None,
	GameplayEvent,        // UAbilitySystemComponent::GenericGameplayEventCallbacks (P2)
	AnimNotify,           // P3
	AttributeChanged,     // P3
	MovementModeChanged,  // P3
	InputAction,          // P3 (reactive, distinct from sticky-inject)
	ActorLifecycle,       // P3 (BeginPlay/EndPlay/Destroyed)
	Timer,                // P4
	AssetEvent,           // P5
	PieEvent,             // P5
	BpCompiled,           // P5
};

/** When the handler should auto-deregister. */
UENUM(BlueprintType)
enum class EBridgeHandlerLifetime : uint8
{
	/** Never auto-deregister. */
	Permanent,
	/** Fire once, then auto-deregister. */
	Once,
	/** Fire N times, then auto-deregister (RemainingCalls counts down). */
	Count,
	/** Auto-deregister on PIE end. */
	WhilePIE,
	/** Auto-deregister when Subject becomes invalid. */
	WhileSubjectAlive,
};

/** What to do when the handler script raises an exception. */
UENUM(BlueprintType)
enum class EBridgeErrorPolicy : uint8
{
	/** Log error, keep handler active. (default) */
	LogContinue,
	/** Log error, auto-deregister the handler. */
	LogUnregister,
	/** Log at Error severity (still keeps handler active). */
	Throw,
};

/** Per-handler runtime statistics, surfaced via GetHandlerStats. */
USTRUCT(BlueprintType)
struct FBridgeHandlerStats
{
	GENERATED_BODY()

	/** Total times this handler was invoked. */
	UPROPERTY(BlueprintReadOnly)
	int64 Calls = 0;

	/** Cumulative wall-time spent inside ExecPythonCommandEx, microseconds. */
	UPROPERTY(BlueprintReadOnly)
	int64 TotalMicroseconds = 0;

	/** Worst-case single invocation, microseconds. */
	UPROPERTY(BlueprintReadOnly)
	int64 MaxMicroseconds = 0;

	/** Number of invocations that returned bSuccess=false. */
	UPROPERTY(BlueprintReadOnly)
	int64 ErrorCount = 0;

	/** Most recent error string (empty if none). */
	UPROPERTY(BlueprintReadOnly)
	FString LastError;

	/** UE world time (Editor world) of most recent fire. */
	UPROPERTY(BlueprintReadOnly)
	double LastFireTimeSeconds = 0.0;
};

/** Compact summary for ListAllHandlers — does not include the script source. */
USTRUCT(BlueprintType)
struct FBridgeHandlerSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString HandlerId;
	UPROPERTY(BlueprintReadOnly) FString Scope;            // "runtime" or "editor"
	UPROPERTY(BlueprintReadOnly) FString TaskName;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) FString TriggerSummary;   // e.g. "GameplayEvent:Event.Combat.Hit"
	UPROPERTY(BlueprintReadOnly) FString SubjectPath;      // resolved path or "" for global / dead
	UPROPERTY(BlueprintReadOnly) FString ScriptPath;       // optional, agent-provided
	UPROPERTY(BlueprintReadOnly) TArray<FString> Tags;
	UPROPERTY(BlueprintReadOnly) FString Lifetime;         // EBridgeHandlerLifetime as string
	UPROPERTY(BlueprintReadOnly) bool bPaused = false;
	UPROPERTY(BlueprintReadOnly) FBridgeHandlerStats Stats;
};

/** Full record returned by GetHandler — includes script source. */
USTRUCT(BlueprintType)
struct FBridgeHandlerDetail
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FBridgeHandlerSummary Summary;

	/** Inline Python source the handler will execute. */
	UPROPERTY(BlueprintReadOnly) FString Script;

	/** ErrorPolicy as string. */
	UPROPERTY(BlueprintReadOnly) FString ErrorPolicy;

	/** Throttle interval in milliseconds (0 = unlimited). */
	UPROPERTY(BlueprintReadOnly) int32 ThrottleMs = 0;

	/** For Lifetime=Count: invocations remaining (-1 if not Count). */
	UPROPERTY(BlueprintReadOnly) int32 RemainingCalls = -1;

	/** ISO-8601 timestamp string of registration. */
	UPROPERTY(BlueprintReadOnly) FString CreatedAt;
};

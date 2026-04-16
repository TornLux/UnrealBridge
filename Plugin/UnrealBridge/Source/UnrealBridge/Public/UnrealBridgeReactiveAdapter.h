#pragma once

#include "CoreMinimal.h"
#include "UnrealBridgeReactiveTypes.h"

class UBridgeReactiveSubsystem;
struct FBridgeHandlerRecord;

/**
 * Per-event-source adapter. Concrete adapters (GameplayEvent, AnimNotify, …)
 * implement this to bind/unbind UE delegates and translate event payloads
 * into Python-source-literal context maps.
 *
 * All methods run on the GameThread. Adapters own their FDelegateHandles
 * and are expected to be idempotent: OnHandlerAdded for the second handler
 * on the same (Subject, Selector) should reuse the already-bound delegate.
 */
class IBridgeReactiveAdapter
{
public:
	virtual ~IBridgeReactiveAdapter() = default;

	/** Which trigger type this adapter services. */
	virtual EBridgeTrigger GetTriggerType() const = 0;

	/**
	 * Called after a handler is added to the registry. Adapter binds the
	 * underlying UE delegate if this is the first handler on (Subject,
	 * Selector); otherwise just records that another handler is interested.
	 */
	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) = 0;

	/**
	 * Called before a handler is removed. Adapter unbinds the underlying
	 * UE delegate when the last handler on (Subject, Selector) goes away.
	 */
	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) = 0;

	/** Called from subsystem Deinitialize. Must unbind every FDelegateHandle. */
	virtual void Shutdown() = 0;

	/**
	 * Documentation-only: describe the keys this adapter injects into the
	 * `ctx` dict, so agents can call describe_trigger_context() to learn
	 * what variables their script can reference.
	 */
	virtual TMap<FString, FString> DescribeContext() const = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

/** Single captured bridge TCP request (internal — not a USTRUCT). */
struct FBridgeCallRecord
{
	FString RequestId;
	FString Command;        // "exec" / "ping" / "debug_resume" / "gamethread_ping" / ""
	FString ScriptPreview;  // for exec: first ~80 chars of the Python script
	double UnixSeconds = 0; // fractional seconds since the Unix epoch (UTC)
	double TotalDurationMs = 0;
	double ExecDurationMs = 0;
	bool bSuccess = false;
	int32 OutputBytes = 0;
	int32 ErrorBytes = 0;
	FString Endpoint;
	FString ErrorPreview;   // first ~200 chars of Result.Error when failure
};

/**
 * Thread-safe ring buffer for bridge TCP call records. Populated from
 * HandleClient (worker threads) and read from UFUNCTION callers
 * (GameThread). Capacity is small (default 500) — O(N) shift on eviction
 * is trivial in practice.
 */
class FBridgeCallLog
{
public:
	static FBridgeCallLog& Get();

	void Append(FBridgeCallRecord&& Record);

	/** Returns the count of dropped entries. */
	int32 Clear();

	int32 GetCapacity() const;

	/**
	 * Resize the ring. Clamped to [10, 5000] by the caller (SetBridgeCallLogCapacity).
	 * Returns the new capacity.
	 */
	int32 SetCapacity(int32 NewCap);

	/** Lifetime count of entries evicted because the ring was full. */
	int32 GetTotalDropped() const;

	/**
	 * Copy up to `MaxEntries` newest records (newest last). Pass 0 for
	 * everything currently buffered.
	 */
	TArray<FBridgeCallRecord> Snapshot(int32 MaxEntries) const;

private:
	FBridgeCallLog() = default;

	mutable FCriticalSection Lock;
	TArray<FBridgeCallRecord> Entries;
	int32 Capacity = 500;
	int32 TotalDropped = 0;
};

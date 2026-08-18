#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Templates/Function.h"

/**
 * 模态注意摘要只保存可跨线程复制的值；Slate 控件永远不离开其所属线程。
 * Copy-only modal attention data; Slate widgets never leave their owning thread.
 */
struct FUnrealBridgeModalAttention
{
	bool bPresent = false;
	bool bDebugging = false;
	uint64 WindowGeneration = 0;
	FString SnapshotId;
	FString Title;
	int32 ButtonCount = 0;
	int32 InputCount = 0;
	int32 CheckBoxCount = 0;
};

/**
 * 健康状态读取结果在锁内一次性生成，保证各字段来自同一缓存版本。
 * A health readout is assembled under one lock so every field comes from one cache version.
 */
struct FUnrealBridgeEditorHealthReadout
{
	bool bReady = false;
	bool bStale = true;
	bool bEngineTickObserved = false;
	bool bEngineStale = true;
	uint64 EngineTickSequence = 0;
	FString LastEngineTickUtc;
	double EngineTickAgeMs = 0.0;
	bool bSlateTickObserved = false;
	bool bSlateStale = true;
	uint64 SlateTickSequence = 0;
	FString LastSlateTickUtc;
	double SlateTickAgeMs = 0.0;
	FString UiState = TEXT("unavailable");
	uint64 AttentionId = 0;
	FString ActiveModalFirstSeenUtc;
	FUnrealBridgeModalAttention Modal;
};

/**
 * 编辑器健康缓存由 Engine/Slate 所属线程写入，并向 TCP 工作线程提供只读快照。
 * The editor health cache is written by the owning Engine/Slate threads and exposes read-only snapshots to TCP workers.
 */
class FUnrealBridgeEditorHealthCache
{
public:
	using FClock = TFunction<double()>;
	using FUtcClock = TFunction<FString()>;

	/** 使用平台单调时钟和 UTC 时钟。 / Use the platform monotonic and UTC clocks. */
	FUnrealBridgeEditorHealthCache();

	/** 注入单调时钟仅用于确定性年龄测试。 / Inject a monotonic clock for deterministic age tests. */
	explicit FUnrealBridgeEditorHealthCache(FClock InClock);

	/** 注入全部时钟以确定性测试 wire 时间字段。 / Inject both clocks to test wire timestamps deterministically. */
	FUnrealBridgeEditorHealthCache(FClock InClock, FUtcClock InUtcClock);

	/** 清除所有观测并恢复未就绪状态。 / Clear all observations and return to not-ready state. */
	void Reset();

	/** 记录主窗口就绪门状态。 / Record the main-frame readiness gate. */
	void SetReady(bool bInReady);

	/** 记录一次 Engine tick。 / Record one Engine tick. */
	void RecordEngineTick();

	/** 记录一次 Slate tick 及其当时的模态摘要。 / Record one Slate tick and its modal summary. */
	void RecordSlateTick(const FUnrealBridgeModalAttention& InModal);

	/** 按指定阈值读取年龄和陈旧性。 / Read ages and staleness using the supplied threshold. */
	FUnrealBridgeEditorHealthReadout Read(double StaleAfterSeconds);

private:
	/** 年龄对回拨时钟按零钳制。 / Clamp age to zero if an injected clock moves backwards. */
	static double AgeMilliseconds(double NowSeconds, double ObservedSeconds);

	FClock Clock;
	FUtcClock UtcClock;
	FCriticalSection Lock;
	bool bReady = false;
	bool bEngineTickObserved = false;
	uint64 EngineTickSequence = 0;
	double LastEngineTickSeconds = 0.0;
	FString LastEngineTickUtc;
	bool bSlateTickObserved = false;
	uint64 SlateTickSequence = 0;
	double LastSlateTickSeconds = 0.0;
	FString LastSlateTickUtc;
	uint64 AttentionId = 0;
	FString ActiveModalFirstSeenUtc;
	FUnrealBridgeModalAttention Modal;
};

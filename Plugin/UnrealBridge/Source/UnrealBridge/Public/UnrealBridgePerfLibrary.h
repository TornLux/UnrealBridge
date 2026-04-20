#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgePerfLibrary.generated.h"

/**
 * Frame timing snapshot in milliseconds. Values come from the raw
 * GGameThreadTime / GRenderThreadTime / RHIGetGPUFrameCycles globals (updated
 * every frame by FViewport::Draw). When `stat unit` is enabled on the active
 * level viewport, the smoothed FStatUnitData values override the raw ones.
 * Fps is GAverageFPS.
 */
USTRUCT(BlueprintType)
struct FBridgeFrameTiming
{
	GENERATED_BODY()

	/** Engine's running-average FPS (GAverageFPS). 0 before the first full frame. */
	UPROPERTY(BlueprintReadOnly)
	float Fps = 0.f;

	/** Frame time in ms. FStatUnitData::FrameTime when smoothed; else 1000/Fps. */
	UPROPERTY(BlueprintReadOnly)
	float FrameMs = 0.f;

	/** Game-thread time, ms. Raw GGameThreadTime, or smoothed FStatUnitData value. */
	UPROPERTY(BlueprintReadOnly)
	float GameThreadMs = 0.f;

	/** Render-thread time, ms. Raw GRenderThreadTime, or smoothed FStatUnitData value. */
	UPROPERTY(BlueprintReadOnly)
	float RenderThreadMs = 0.f;

	/** GPU frame time, ms. Summed across MAX_NUM_GPUS via RHIGetGPUFrameCycles. */
	UPROPERTY(BlueprintReadOnly)
	float GpuMs = 0.f;

	/** RHI translation time, ms. 0 on the raw path — only set when bSmoothed=true. */
	UPROPERTY(BlueprintReadOnly)
	float RhiMs = 0.f;

	/** Delta seconds reported by FApp for the most recent frame. */
	UPROPERTY(BlueprintReadOnly)
	float DeltaSeconds = 0.f;

	/** GFrameCounter value at capture time. */
	UPROPERTY(BlueprintReadOnly)
	int64 FrameNumber = 0;

	/** True when `stat unit` is active and values came from FStatUnitData (smoothed). */
	/** False means raw per-frame globals (no running average). */
	UPROPERTY(BlueprintReadOnly)
	bool bSmoothed = false;
};

/**
 * Per-frame draw call and primitive counters sampled from the RHI globals.
 * These values are the counts for the most recently submitted frame — pulling
 * them once per second is fine; pulling several times per frame returns
 * near-identical numbers.
 */
USTRUCT(BlueprintType)
struct FBridgeRenderCounters
{
	GENERATED_BODY()

	/** GNumDrawCallsRHI summed across MAX_NUM_GPUS. */
	UPROPERTY(BlueprintReadOnly)
	int32 DrawCalls = 0;

	/** GNumPrimitivesDrawnRHI summed across MAX_NUM_GPUS. */
	UPROPERTY(BlueprintReadOnly)
	int32 PrimitivesDrawn = 0;

	/** GNumExplicitGPUsForRendering — usually 1 on desktop builds. */
	UPROPERTY(BlueprintReadOnly)
	int32 NumGpus = 1;
};

/** Process + platform memory snapshot, in megabytes (MiB). */
USTRUCT(BlueprintType)
struct FBridgeMemoryStats
{
	GENERATED_BODY()

	/** Process working set (FPlatformMemoryStats::UsedPhysical), MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 UsedPhysicalMb = 0;

	/** Process virtual commit (UsedVirtual), MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 UsedVirtualMb = 0;

	/** Peak working set observed this session, MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 PeakUsedPhysicalMb = 0;

	/** Peak virtual commit observed this session, MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 PeakUsedVirtualMb = 0;

	/** System-wide available physical memory at capture time, MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 AvailablePhysicalMb = 0;

	/** System-wide available virtual memory at capture time, MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 AvailableVirtualMb = 0;

	/** Total physical RAM on the machine, MiB. */
	UPROPERTY(BlueprintReadOnly)
	int64 TotalPhysicalMb = 0;
};

/** Histogram entry: "this class has N live UObjects". */
USTRUCT(BlueprintType)
struct FBridgeUObjectClassCount
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(BlueprintReadOnly)
	int32 Count = 0;
};

/**
 * UObject population snapshot. Iterates TObjectIterator<UObject> and aggregates
 * by UClass, returning the top-N classes by count. O(N) in the number of live
 * UObjects (typically 100k-2M in an editor session) — don't call on a hot path.
 */
USTRUCT(BlueprintType)
struct FBridgeUObjectStats
{
	GENERATED_BODY()

	/** Total number of live UObjects walked. */
	UPROPERTY(BlueprintReadOnly)
	int32 TotalObjects = 0;

	/** Number of distinct UClass types seen. */
	UPROPERTY(BlueprintReadOnly)
	int32 UniqueClasses = 0;

	/** Top classes by live-object count, descending. Capped at the caller's TopN. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeUObjectClassCount> TopClasses;
};

/** Bundled perf snapshot returned by GetPerfSnapshot. */
USTRUCT(BlueprintType)
struct FBridgePerfSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FBridgeFrameTiming Timing;

	UPROPERTY(BlueprintReadOnly)
	FBridgeRenderCounters Render;

	UPROPERTY(BlueprintReadOnly)
	FBridgeMemoryStats Memory;

	/** Populated only when bIncludeUObjectStats was true. */
	UPROPERTY(BlueprintReadOnly)
	FBridgeUObjectStats UObjects;

	/** ISO-8601 UTC timestamp for delta-comparison across snapshots. */
	UPROPERTY(BlueprintReadOnly)
	FString CaptureTimeUtc;

	UPROPERTY(BlueprintReadOnly)
	FString EngineVersion;

	/** True when PIE was active at capture time. */
	UPROPERTY(BlueprintReadOnly)
	bool bWasInPie = false;
};

/**
 * Structured performance snapshots for UnrealBridge. Replaces parsing
 * `stat unit` text output. All values are read from engine globals + platform
 * APIs on the GameThread.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgePerfLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Return smoothed frame timing (FPS, game/render/GPU ms). Prefers
	 * FStatUnitData from the active editor viewport (smoothed running
	 * average). Falls back to raw RenderCore / RHI globals when no viewport
	 * client is reachable (e.g. headless commandlet). Timings are stale when
	 * the editor hasn't rendered recently — check `frame_number` to detect
	 * this.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Perf")
	static FBridgeFrameTiming GetFrameTiming();

	/**
	 * Return draw call and primitive counts for the most recent rendered
	 * frame, summed across MAX_NUM_GPUS. 0 on headless builds or before the
	 * first frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Perf")
	static FBridgeRenderCounters GetRenderCounters();

	/** Return process + system memory stats in MiB. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Perf")
	static FBridgeMemoryStats GetMemoryStats();

	/**
	 * Return the top-N UClass types by live UObject count. Iterates every
	 * live UObject (~100k-2M typical) — expect 10-200 ms. TopN clamped to
	 * [1, 200].
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Perf")
	static FBridgeUObjectStats GetUObjectStats(int32 TopN = 20);

	/**
	 * Aggregate snapshot. bIncludeUObjectStats defaults off because the
	 * UObject iteration is the slow part; pass true when you want the
	 * histogram, false for a cheap per-second sampler.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Perf")
	static FBridgePerfSnapshot GetPerfSnapshot(bool bIncludeUObjectStats = false, int32 UObjectTopN = 20);
};

#include "UnrealBridgePerfLibrary.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorViewport.h"
#include "UnrealClient.h"
#include "RenderTimer.h"
#include "MultiGPU.h"
#include "RHIStats.h"
#include "RHIGlobals.h"
#include "DynamicRHI.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "UObject/UObjectIterator.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/DateTime.h"

// GAverageFPS / GAverageMS are defined in UnrealEngine.cpp and have no
// canonical public header declaration — consumers (UnrealEdMisc, etc.) declare
// them inline. Follow that convention.
extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;

namespace BridgePerfImpl
{
	static constexpr int64 BytesPerMb = 1024LL * 1024LL;

	static int64 BytesToMb(uint64 Bytes)
	{
		return static_cast<int64>(Bytes / BytesPerMb);
	}

	/** Pull the smoothed FStatUnitData from the first active level viewport, if any. */
	static FStatUnitData* GetActiveViewportStatUnit()
	{
		if (!FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			return nullptr;
		}
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<SLevelViewport> Viewport = LE.GetFirstActiveLevelViewport();
		if (!Viewport.IsValid())
		{
			return nullptr;
		}
		FLevelEditorViewportClient& Client = Viewport->GetLevelViewportClient();
		return Client.GetStatUnitData();
	}

	/** Sum GPU frame time across MAX_NUM_GPUS (FStatUnitData stores per-GPU). */
	static float SumGpuMs(const float (&PerGpuMs)[MAX_NUM_GPUS])
	{
		float Total = 0.f;
		for (int32 i = 0; i < MAX_NUM_GPUS; ++i)
		{
			Total += PerGpuMs[i];
		}
		return Total;
	}
}

// ─── Frame timing ───────────────────────────────────────────

FBridgeFrameTiming UUnrealBridgePerfLibrary::GetFrameTiming()
{
	FBridgeFrameTiming Out;

	Out.Fps = GAverageFPS;
	Out.FrameMs = (GAverageFPS > 0.f) ? (1000.f / GAverageFPS) : GAverageMS;
	Out.DeltaSeconds = static_cast<float>(FApp::GetDeltaTime());
	Out.FrameNumber = static_cast<int64>(GFrameCounter);

	// Raw per-frame cycle counters — always updated by FViewport::Draw and the
	// renderer, independent of whether `stat unit` is displayed.
	Out.GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	Out.RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	{
		// RHIGetGPUFrameCycles is the UE 5.6+ replacement for the deprecated
		// GGPUFrameTime global. Sum per-GPU for MGPU builds.
		uint32 GpuCycles = 0;
		for (uint32 i = 0; i < GNumExplicitGPUsForRendering; ++i)
		{
			GpuCycles += RHIGetGPUFrameCycles(i);
		}
		Out.GpuMs = FPlatformTime::ToMilliseconds(GpuCycles);
	}
	Out.RhiMs = 0.f;
	Out.bSmoothed = false;

	// FStatUnitData is only populated when `stat unit` is actively being drawn
	// on a viewport (FStatUnitData::DrawStat is the sole writer). When the
	// struct has a non-zero FrameTime, the user has stat unit enabled and the
	// smoothed running averages are more stable than our raw snapshot — use
	// them. Otherwise stick with the raw values above.
	if (FStatUnitData* StatUnit = BridgePerfImpl::GetActiveViewportStatUnit())
	{
		if (StatUnit->FrameTime > 0.f)
		{
			Out.bSmoothed = true;
			Out.GameThreadMs = StatUnit->GameThreadTime;
			Out.RenderThreadMs = StatUnit->RenderThreadTime;
			Out.GpuMs = BridgePerfImpl::SumGpuMs(StatUnit->GPUFrameTime);
			Out.RhiMs = StatUnit->RHITTime;
			Out.FrameMs = StatUnit->FrameTime;
		}
	}

	return Out;
}

// ─── Render counters ────────────────────────────────────────

FBridgeRenderCounters UUnrealBridgePerfLibrary::GetRenderCounters()
{
	FBridgeRenderCounters Out;

	int64 TotalDraws = 0;
	int64 TotalPrims = 0;
	for (int32 i = 0; i < MAX_NUM_GPUS; ++i)
	{
		TotalDraws += GNumDrawCallsRHI[i];
		TotalPrims += GNumPrimitivesDrawnRHI[i];
	}

	Out.DrawCalls = static_cast<int32>(FMath::Min<int64>(TotalDraws, MAX_int32));
	Out.PrimitivesDrawn = static_cast<int32>(FMath::Min<int64>(TotalPrims, MAX_int32));
	Out.NumGpus = static_cast<int32>(GNumExplicitGPUsForRendering);
	return Out;
}

// ─── Memory ─────────────────────────────────────────────────

FBridgeMemoryStats UUnrealBridgePerfLibrary::GetMemoryStats()
{
	FBridgeMemoryStats Out;

	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	const FPlatformMemoryConstants& Constants = FPlatformMemory::GetConstants();

	Out.UsedPhysicalMb = BridgePerfImpl::BytesToMb(Stats.UsedPhysical);
	Out.UsedVirtualMb = BridgePerfImpl::BytesToMb(Stats.UsedVirtual);
	Out.PeakUsedPhysicalMb = BridgePerfImpl::BytesToMb(Stats.PeakUsedPhysical);
	Out.PeakUsedVirtualMb = BridgePerfImpl::BytesToMb(Stats.PeakUsedVirtual);
	Out.AvailablePhysicalMb = BridgePerfImpl::BytesToMb(Stats.AvailablePhysical);
	Out.AvailableVirtualMb = BridgePerfImpl::BytesToMb(Stats.AvailableVirtual);
	Out.TotalPhysicalMb = BridgePerfImpl::BytesToMb(Constants.TotalPhysical);

	return Out;
}

// ─── UObject histogram ──────────────────────────────────────

FBridgeUObjectStats UUnrealBridgePerfLibrary::GetUObjectStats(int32 TopN)
{
	FBridgeUObjectStats Out;

	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 200);

	TMap<FName, int32> Counts;
	Counts.Reserve(4096);

	int32 TotalObjects = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (!Obj)
		{
			continue;
		}
		UClass* Cls = Obj->GetClass();
		if (!Cls)
		{
			continue;
		}
		++Counts.FindOrAdd(Cls->GetFName());
		++TotalObjects;
	}

	Out.TotalObjects = TotalObjects;
	Out.UniqueClasses = Counts.Num();

	TArray<TPair<FName, int32>> Sorted;
	Sorted.Reserve(Counts.Num());
	for (const TPair<FName, int32>& Entry : Counts)
	{
		Sorted.Emplace(Entry);
	}
	Sorted.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B)
	{
		return A.Value > B.Value;
	});

	const int32 Take = FMath::Min(ClampedTopN, Sorted.Num());
	Out.TopClasses.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		FBridgeUObjectClassCount Row;
		Row.ClassName = Sorted[i].Key.ToString();
		Row.Count = Sorted[i].Value;
		Out.TopClasses.Add(MoveTemp(Row));
	}

	return Out;
}

// ─── Aggregate snapshot ─────────────────────────────────────

FBridgePerfSnapshot UUnrealBridgePerfLibrary::GetPerfSnapshot(bool bIncludeUObjectStats, int32 UObjectTopN)
{
	FBridgePerfSnapshot Out;

	Out.Timing = GetFrameTiming();
	Out.Render = GetRenderCounters();
	Out.Memory = GetMemoryStats();
	if (bIncludeUObjectStats)
	{
		Out.UObjects = GetUObjectStats(UObjectTopN);
	}
	Out.CaptureTimeUtc = FDateTime::UtcNow().ToIso8601();
	Out.EngineVersion = FEngineVersion::Current().ToString();
	Out.bWasInPie = (GEditor && GEditor->PlayWorld != nullptr);

	return Out;
}

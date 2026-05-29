#include "UnrealBridgePerfLibrary.h"
#include "UnrealBridgeCompat.h"

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
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Trace/Trace.h"
#endif
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "WorldPartition/WorldPartition.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundWave.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "HAL/IConsoleManager.h"

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

	/** Pick the editor world for breakdown queries — these are introspection
	 *  ops on the level the user has open, never the live PIE world. */
	static UWorld* GetEditorWorldForPerf()
	{
		if (GEditor)
		{
			return GEditor->GetEditorWorldContext().World();
		}
		return nullptr;
	}

	/** Short level name (package short name) for breakdown row keys. */
	static FString GetLevelShortName(const ULevel* Level)
	{
		if (!Level)
		{
			return FString();
		}
		const UPackage* Pkg = Level->GetOutermost();
		if (!Pkg)
		{
			return FString();
		}
		return FPackageName::GetShortName(Pkg->GetName());
	}

	/** Sort breakdown rows by (Count desc, Key asc) and clamp to MaxGroups. */
	static void FinalizeBreakdownRows(
		TArray<FBridgePerfBreakdownRow>& Rows,
		int32 MaxGroups)
	{
		Rows.Sort([](const FBridgePerfBreakdownRow& A, const FBridgePerfBreakdownRow& B)
		{
			if (A.Count != B.Count)
			{
				return A.Count > B.Count;
			}
			if (A.TotalBytes != B.TotalBytes)
			{
				return A.TotalBytes > B.TotalBytes;
			}
			return A.Key < B.Key;
		});
		const int32 Clamp = FMath::Clamp(MaxGroups, 1, 100000);
		if (Rows.Num() > Clamp)
		{
			Rows.SetNum(Clamp);
		}
	}

	/** Sort breakdown rows by (TotalBytes desc, Count desc, Key asc). Used for
	 *  asset memory breakdowns where bytes are the primary perf signal. */
	static void FinalizeBreakdownRowsByBytes(
		TArray<FBridgePerfBreakdownRow>& Rows,
		int32 MaxGroups)
	{
		Rows.Sort([](const FBridgePerfBreakdownRow& A, const FBridgePerfBreakdownRow& B)
		{
			if (A.TotalBytes != B.TotalBytes)
			{
				return A.TotalBytes > B.TotalBytes;
			}
			if (A.Count != B.Count)
			{
				return A.Count > B.Count;
			}
			return A.Key < B.Key;
		});
		const int32 Clamp = FMath::Clamp(MaxGroups, 1, 100000);
		if (Rows.Num() > Clamp)
		{
			Rows.SetNum(Clamp);
		}
	}

	/** On-disk size for a package, in bytes. Works on every UE version because
	 *  IAssetRegistry::GetAssetSizeOnDisk doesn't exist on stock 5.3-5.7 —
	 *  we always go through DoesPackageExist + FileSize. Returns 0 for
	 *  never-saved or missing packages. */
	static int64 GetPackageDiskSize(FName PackageName)
	{
		if (PackageName.IsNone())
		{
			return 0;
		}
		const FString PackageStr = PackageName.ToString();
		FString Filename;
		if (!FPackageName::DoesPackageExist(PackageStr, &Filename))
		{
			return 0;
		}
		const int64 Size = IFileManager::Get().FileSize(*Filename);
		return Size > 0 ? Size : 0;
	}

	/** Extract the leading content folder from a package path, e.g.
	 *  "/Game/Characters/Hero/T_Skin" → "/Game/Characters". One level deep —
	 *  callers wanting deeper bucketing can post-process. */
	static FString GetTopLevelFolder(const FString& PackagePath)
	{
		// PackagePath is the directory portion (e.g. "/Game/Characters/Hero").
		// Strip a leading slash, take up to the first two components, re-prefix.
		FString Trimmed = PackagePath;
		if (Trimmed.StartsWith(TEXT("/")))
		{
			Trimmed.RemoveAt(0);
		}
		int32 SecondSlash = INDEX_NONE;
		int32 FirstSlash = INDEX_NONE;
		if (Trimmed.FindChar(TEXT('/'), FirstSlash))
		{
			Trimmed.FindChar(TEXT('/'), SecondSlash);
			if (FirstSlash != INDEX_NONE)
			{
				int32 Pos = FirstSlash + 1;
				int32 NextSlash = INDEX_NONE;
				if (Trimmed.RightChop(Pos).FindChar(TEXT('/'), NextSlash))
				{
					return TEXT("/") + Trimmed.Left(Pos + NextSlash);
				}
			}
		}
		// No second slash — the whole thing is the top folder.
		return TEXT("/") + Trimmed;
	}

	/** Add (or update) one breakdown bucket. Maintains Count, TotalBytes, and
	 *  up to 3 sample paths in insertion order. */
	static void AccumulateBucket(
		TMap<FString, FBridgePerfBreakdownRow>& Buckets,
		const FString& Key,
		int64 BytesToAdd,
		const FString& SamplePath)
	{
		FBridgePerfBreakdownRow& Bucket = Buckets.FindOrAdd(Key);
		if (Bucket.Key.IsEmpty())
		{
			Bucket.Key = Key;
		}
		Bucket.Count += 1;
		Bucket.TotalBytes += BytesToAdd;
		if (Bucket.SamplePaths.Num() < 3)
		{
			Bucket.SamplePaths.Add(SamplePath);
		}
	}

	/** Map a TextureCompressionSettings enum value (TC_*) to a coarse sampler
	 *  type bucket name, mirroring UE's GetSamplerTypeForCompressionSettings
	 *  conventions but without needing the texture loaded. Used by group_by
	 *  ="sampler_type" in disk mode. */
	static FString CompressionSettingsToSamplerBucket(const FString& TcEnumString)
	{
		// Strings come back as the enum identifier (e.g. "TC_Normalmap").
		if (TcEnumString.Equals(TEXT("TC_Normalmap"), ESearchCase::IgnoreCase))
		{
			return TEXT("Normal");
		}
		if (TcEnumString.Equals(TEXT("TC_Masks"), ESearchCase::IgnoreCase) ||
			TcEnumString.Equals(TEXT("TC_Alpha"), ESearchCase::IgnoreCase) ||
			TcEnumString.Equals(TEXT("TC_Grayscale"), ESearchCase::IgnoreCase))
		{
			return TEXT("Masks");
		}
		if (TcEnumString.Contains(TEXT("HDR")) ||
			TcEnumString.Contains(TEXT("HighDynamicRange")) ||
			TcEnumString.Equals(TEXT("TC_HalfFloat"), ESearchCase::IgnoreCase) ||
			TcEnumString.Equals(TEXT("TC_SingleFloat"), ESearchCase::IgnoreCase))
		{
			return TEXT("LinearColor");
		}
		if (TcEnumString.Equals(TEXT("TC_Displacementmap"), ESearchCase::IgnoreCase) ||
			TcEnumString.Equals(TEXT("TC_VectorDisplacementmap"), ESearchCase::IgnoreCase))
		{
			return TEXT("Displacement");
		}
		if (TcEnumString.Equals(TEXT("TC_DistanceFieldFont"), ESearchCase::IgnoreCase))
		{
			return TEXT("DistanceFieldFont");
		}
		// TC_Default, TC_BC7, TC_LQ, etc. — color sampler.
		return TEXT("Color");
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

// ─── World actor breakdown (M1-3) ───────────────────────────

namespace BridgePerfImpl
{
	/** Per-key accumulator: count + first 3 sample paths. */
	struct FActorBreakdownAcc
	{
		int32 Count = 0;
		TArray<FString> Samples;
	};

	/** Build a composite map key as TPair<LevelName, ClassName>. TPair has
	 *  built-in GetTypeHash via TPair's ADL hook, so no custom hash needed.
	 *  Either component is NAME_None when the corresponding group_by mode
	 *  doesn't include it. */
	using FActorBreakdownKey = TPair<FName, FName>;
}

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetWorldActorBreakdown(
	const FString& LevelFilter,
	const FString& GroupBy,
	int32 MaxGroups)
{
	TArray<FBridgePerfBreakdownRow> Out;

	UWorld* World = BridgePerfImpl::GetEditorWorldForPerf();
	if (!World)
	{
		return Out;
	}

	// Validate group_by; fall back to "class" silently to avoid empty results.
	const bool bGroupByLevel = GroupBy.Equals(TEXT("level"), ESearchCase::IgnoreCase);
	const bool bGroupByLevelClass = GroupBy.Equals(TEXT("level_class"), ESearchCase::IgnoreCase);
	const bool bGroupByClass = !bGroupByLevel && !bGroupByLevelClass;

	// World Partition projects: GetLevels() only returns currently-loaded
	// actors. Listing unloaded actor descs requires the WP ActorDescContainer
	// API which has churned across 5.3-5.7 (ActorDescContainer →
	// ActorDescContainerInstance → ActorDescContainerCollection). For now
	// return whatever GetLevels() gives and warn that WP has been detected;
	// a separate UFUNCTION can list unloaded descs in a follow-up milestone.
	// TODO(M1+): partition-aware enumeration via UWorldPartition::ForEachActorDescInstance
	const bool bIsWorldPartition = (World->GetWorldPartition() != nullptr);
	if (bIsWorldPartition)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UnrealBridgePerf: GetWorldActorBreakdown — World Partition detected; ")
			TEXT("only loaded actors counted (unloaded descs not yet supported)"));
	}

	// Walk levels, build (level, class) → count + samples map.
	TMap<BridgePerfImpl::FActorBreakdownKey, BridgePerfImpl::FActorBreakdownAcc> Acc;
	Acc.Reserve(256);

	const FString TrimmedFilter = LevelFilter.TrimStartAndEnd();

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level)
		{
			continue;
		}
		const FString LevelShort = BridgePerfImpl::GetLevelShortName(Level);
		if (!TrimmedFilter.IsEmpty() && !LevelShort.Contains(TrimmedFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FName LevelFName(*LevelShort);

		for (AActor* Actor : Level->Actors)
		{
			if (!Actor || !IsValid(Actor))
			{
				continue;
			}
			UClass* Cls = Actor->GetClass();
			if (!Cls)
			{
				continue;
			}

			// TPair<LevelName, ClassName>. NAME_None for the dimension we
			// aren't grouping on.
			BridgePerfImpl::FActorBreakdownKey MapKey;
			if (bGroupByLevel)
			{
				MapKey = BridgePerfImpl::FActorBreakdownKey(LevelFName, NAME_None);
			}
			else if (bGroupByLevelClass)
			{
				MapKey = BridgePerfImpl::FActorBreakdownKey(LevelFName, Cls->GetFName());
			}
			else // bGroupByClass
			{
				MapKey = BridgePerfImpl::FActorBreakdownKey(NAME_None, Cls->GetFName());
			}

			BridgePerfImpl::FActorBreakdownAcc& Bucket = Acc.FindOrAdd(MapKey);
			++Bucket.Count;
			if (Bucket.Samples.Num() < 3)
			{
				Bucket.Samples.Add(Actor->GetPathName());
			}
		}
	}

	// Materialise into output rows. TPair: .Key=LevelName, .Value=ClassName.
	Out.Reserve(Acc.Num());
	for (const TPair<BridgePerfImpl::FActorBreakdownKey, BridgePerfImpl::FActorBreakdownAcc>& Entry : Acc)
	{
		const FName LevelKeyComp = Entry.Key.Key;
		const FName ClassKeyComp = Entry.Key.Value;

		FBridgePerfBreakdownRow Row;
		if (bGroupByLevel)
		{
			Row.Key = LevelKeyComp.ToString();
		}
		else if (bGroupByLevelClass)
		{
			Row.Key = ClassKeyComp.ToString();
			Row.LevelName = LevelKeyComp.ToString();
		}
		else // bGroupByClass
		{
			Row.Key = ClassKeyComp.ToString();
		}
		Row.Count = Entry.Value.Count;
		Row.TotalBytes = 0; // actors have no on-disk size in this view
		Row.SamplePaths = Entry.Value.Samples;
		Out.Add(MoveTemp(Row));
	}

	BridgePerfImpl::FinalizeBreakdownRows(Out, MaxGroups);
	return Out;
}

// ─── Texture memory breakdown (M1-1) ────────────────────────

namespace BridgePerfImpl
{
	/** Texture group_by mode. Translated once up front to avoid per-asset string compares. */
	enum class ETextureGroupBy : uint8
	{
		Folder,
		LodGroup,
		CompressionFormat,
		SamplerType,
	};

	static bool ParseTextureGroupBy(const FString& In, ETextureGroupBy& Out)
	{
		if (In.Equals(TEXT("folder"), ESearchCase::IgnoreCase)) { Out = ETextureGroupBy::Folder; return true; }
		if (In.Equals(TEXT("lod_group"), ESearchCase::IgnoreCase)) { Out = ETextureGroupBy::LodGroup; return true; }
		if (In.Equals(TEXT("compression_format"), ESearchCase::IgnoreCase)) { Out = ETextureGroupBy::CompressionFormat; return true; }
		if (In.Equals(TEXT("sampler_type"), ESearchCase::IgnoreCase)) { Out = ETextureGroupBy::SamplerType; return true; }
		return false;
	}

	static FString GetTextureGroupKeyFromAssetData(
		const FAssetData& Data,
		ETextureGroupBy Mode)
	{
		switch (Mode)
		{
		case ETextureGroupBy::Folder:
		{
			return GetTopLevelFolder(Data.PackagePath.ToString());
		}
		case ETextureGroupBy::LodGroup:
		{
			FString Tag;
			if (Data.GetTagValue(TEXT("LODGroup"), Tag) && !Tag.IsEmpty())
			{
				return Tag;
			}
			return TEXT("(unspecified)");
		}
		case ETextureGroupBy::CompressionFormat:
		{
			FString Tag;
			if (Data.GetTagValue(TEXT("CompressionSettings"), Tag) && !Tag.IsEmpty())
			{
				return Tag;
			}
			return TEXT("(unspecified)");
		}
		case ETextureGroupBy::SamplerType:
		{
			FString Tag;
			if (Data.GetTagValue(TEXT("CompressionSettings"), Tag) && !Tag.IsEmpty())
			{
				return CompressionSettingsToSamplerBucket(Tag);
			}
			return TEXT("Color"); // TC_Default fallback
		}
		}
		return TEXT("(unknown)");
	}

	static FString GetTextureGroupKeyFromObject(
		const UTexture* Tex,
		ETextureGroupBy Mode)
	{
		if (!Tex)
		{
			return TEXT("(null)");
		}
		switch (Mode)
		{
		case ETextureGroupBy::Folder:
		{
			const UPackage* Pkg = Tex->GetOutermost();
			if (!Pkg) return TEXT("(transient)");
			// Outer package name is /Game/Foo/Bar/PackageName — strip filename.
			const FString PackageName = Pkg->GetName();
			int32 LastSlash = INDEX_NONE;
			if (PackageName.FindLastChar(TEXT('/'), LastSlash))
			{
				return GetTopLevelFolder(PackageName.Left(LastSlash));
			}
			return PackageName;
		}
		case ETextureGroupBy::LodGroup:
		{
			const UEnum* Enum = StaticEnum<TextureGroup>();
			if (Enum)
			{
				return Enum->GetNameStringByValue(static_cast<int64>(Tex->LODGroup));
			}
			return FString::FromInt(static_cast<int32>(Tex->LODGroup));
		}
		case ETextureGroupBy::CompressionFormat:
		{
			const UEnum* Enum = StaticEnum<TextureCompressionSettings>();
			if (Enum)
			{
				return Enum->GetNameStringByValue(static_cast<int64>(Tex->CompressionSettings));
			}
			return FString::FromInt(static_cast<int32>(Tex->CompressionSettings));
		}
		case ETextureGroupBy::SamplerType:
		{
			const UEnum* Enum = StaticEnum<TextureCompressionSettings>();
			if (Enum)
			{
				const FString TcStr = Enum->GetNameStringByValue(static_cast<int64>(Tex->CompressionSettings));
				return CompressionSettingsToSamplerBucket(TcStr);
			}
			return TEXT("Color");
		}
		}
		return TEXT("(unknown)");
	}
}

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetTextureMemoryBreakdown(
	const FString& GroupBy,
	const FString& Mode,
	int32 MaxGroups)
{
	TArray<FBridgePerfBreakdownRow> Out;

	BridgePerfImpl::ETextureGroupBy ModeEnum = BridgePerfImpl::ETextureGroupBy::Folder;
	if (!BridgePerfImpl::ParseTextureGroupBy(GroupBy, ModeEnum))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetTextureMemoryBreakdown bad group_by '%s' — expected ")
			TEXT("folder | lod_group | compression_format | sampler_type"),
			*GroupBy);
		return Out;
	}

	const bool bRuntimeMode = Mode.Equals(TEXT("runtime"), ESearchCase::IgnoreCase);
	const bool bDiskMode = Mode.IsEmpty() || Mode.Equals(TEXT("disk"), ESearchCase::IgnoreCase);
	if (!bRuntimeMode && !bDiskMode)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetTextureMemoryBreakdown bad mode '%s' — expected disk | runtime"),
			*Mode);
		return Out;
	}

	TMap<FString, FBridgePerfBreakdownRow> Buckets;
	Buckets.Reserve(64);

	if (bRuntimeMode)
	{
		// Iterate every loaded UTexture (covers Texture2D/Cube/Volume/etc.).
		// Only objects already in memory are counted — this is the contract.
		for (TObjectIterator<UTexture> It; It; ++It)
		{
			UTexture* Tex = *It;
			if (!Tex || !IsValid(Tex))
			{
				continue;
			}
			const int64 Bytes = static_cast<int64>(
				Tex->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
			const FString Key = BridgePerfImpl::GetTextureGroupKeyFromObject(Tex, ModeEnum);
			BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, Tex->GetPathName());
		}
	}
	else
	{
		// Disk mode: walk AssetRegistry, no LoadObject. Skip never-saved
		// assets (TagsAndValues empty + no on-disk file → disk size 0 — they
		// legitimately don't contribute to project disk footprint).
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByClass(UTexture::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

		for (const FAssetData& Data : Assets)
		{
			if (!Data.IsValid())
			{
				continue;
			}
			const int64 Bytes = BridgePerfImpl::GetPackageDiskSize(Data.PackageName);
			if (Bytes <= 0)
			{
				// Never-saved or missing on disk. Skip (matches doc contract).
				continue;
			}
			const FString Key = BridgePerfImpl::GetTextureGroupKeyFromAssetData(Data, ModeEnum);
			BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, Data.GetSoftObjectPath().ToString());
		}
	}

	Out.Reserve(Buckets.Num());
	for (const TPair<FString, FBridgePerfBreakdownRow>& Pair : Buckets)
	{
		Out.Add(Pair.Value);
	}
	BridgePerfImpl::FinalizeBreakdownRowsByBytes(Out, MaxGroups);
	return Out;
}

// ─── Mesh memory breakdown (M1-2) ───────────────────────────

namespace BridgePerfImpl
{
	enum class EMeshGroupBy : uint8
	{
		Folder,
		LodCount,
		VertexCountBucket,
	};

	static bool ParseMeshGroupBy(const FString& In, EMeshGroupBy& Out)
	{
		if (In.Equals(TEXT("folder"), ESearchCase::IgnoreCase)) { Out = EMeshGroupBy::Folder; return true; }
		if (In.Equals(TEXT("lod_count"), ESearchCase::IgnoreCase)) { Out = EMeshGroupBy::LodCount; return true; }
		if (In.Equals(TEXT("vertex_count_bucket"), ESearchCase::IgnoreCase)) { Out = EMeshGroupBy::VertexCountBucket; return true; }
		return false;
	}

	/** Map a vertex count to a coarse log-scale bucket. */
	static FString VertexCountToBucket(int64 N)
	{
		if (N < 0) N = 0;
		if (N < 1000)        return TEXT("<1k");
		if (N < 10000)       return TEXT("1k-10k");
		if (N < 100000)      return TEXT("10k-100k");
		if (N < 1000000)     return TEXT("100k-1M");
		return TEXT(">=1M");
	}

	/** Read a numeric tag from FAssetData; returns Default when missing. */
	static int64 GetTagInt(const FAssetData& Data, FName Tag, int64 Default)
	{
		FString Value;
		if (Data.GetTagValue(Tag, Value) && !Value.IsEmpty())
		{
			return FCString::Atoi64(*Value);
		}
		return Default;
	}

	static FString GetMeshGroupKeyFromAssetData(const FAssetData& Data, EMeshGroupBy Mode)
	{
		switch (Mode)
		{
		case EMeshGroupBy::Folder:
			return GetTopLevelFolder(Data.PackagePath.ToString());
		case EMeshGroupBy::LodCount:
		{
			const int64 N = GetTagInt(Data, TEXT("LODs"), -1);
			if (N < 0) return TEXT("(unknown)");
			return FString::Printf(TEXT("%d LOD%s"), static_cast<int32>(N), (N == 1 ? TEXT("") : TEXT("s")));
		}
		case EMeshGroupBy::VertexCountBucket:
		{
			const int64 N = GetTagInt(Data, TEXT("Vertices"), -1);
			if (N < 0) return TEXT("(unknown)");
			return VertexCountToBucket(N);
		}
		}
		return TEXT("(unknown)");
	}
}

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetMeshMemoryBreakdown(
	const FString& GroupBy,
	const FString& MeshType,
	const FString& Mode,
	int32 MaxGroups)
{
	TArray<FBridgePerfBreakdownRow> Out;

	BridgePerfImpl::EMeshGroupBy ModeEnum = BridgePerfImpl::EMeshGroupBy::Folder;
	if (!BridgePerfImpl::ParseMeshGroupBy(GroupBy, ModeEnum))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetMeshMemoryBreakdown bad group_by '%s' — expected ")
			TEXT("folder | lod_count | vertex_count_bucket"),
			*GroupBy);
		return Out;
	}

	const bool bWantStatic = MeshType.IsEmpty()
		|| MeshType.Equals(TEXT("all"), ESearchCase::IgnoreCase)
		|| MeshType.Equals(TEXT("static"), ESearchCase::IgnoreCase);
	const bool bWantSkeletal = MeshType.IsEmpty()
		|| MeshType.Equals(TEXT("all"), ESearchCase::IgnoreCase)
		|| MeshType.Equals(TEXT("skeletal"), ESearchCase::IgnoreCase);
	if (!bWantStatic && !bWantSkeletal)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetMeshMemoryBreakdown bad mesh_type '%s' — expected static | skeletal | all"),
			*MeshType);
		return Out;
	}

	const bool bRuntimeMode = Mode.Equals(TEXT("runtime"), ESearchCase::IgnoreCase);
	const bool bDiskMode = Mode.IsEmpty() || Mode.Equals(TEXT("disk"), ESearchCase::IgnoreCase);
	if (!bRuntimeMode && !bDiskMode)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetMeshMemoryBreakdown bad mode '%s' — expected disk | runtime"),
			*Mode);
		return Out;
	}

	TMap<FString, FBridgePerfBreakdownRow> Buckets;
	Buckets.Reserve(64);

	if (bRuntimeMode)
	{
		if (bWantStatic)
		{
			for (TObjectIterator<UStaticMesh> It; It; ++It)
			{
				UStaticMesh* SM = *It;
				if (!SM || !IsValid(SM)) continue;
				const int64 Bytes = static_cast<int64>(SM->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
				FString Key;
				switch (ModeEnum)
				{
				case BridgePerfImpl::EMeshGroupBy::Folder:
				{
					if (UPackage* Pkg = SM->GetOutermost())
					{
						const FString PackageName = Pkg->GetName();
						int32 LastSlash = INDEX_NONE;
						if (PackageName.FindLastChar(TEXT('/'), LastSlash))
						{
							Key = BridgePerfImpl::GetTopLevelFolder(PackageName.Left(LastSlash));
						}
						else
						{
							Key = PackageName;
						}
					}
					else
					{
						Key = TEXT("(transient)");
					}
					break;
				}
				case BridgePerfImpl::EMeshGroupBy::LodCount:
				{
					const int32 NumLODs = SM->GetNumLODs();
					Key = FString::Printf(TEXT("%d LOD%s"), NumLODs, (NumLODs == 1 ? TEXT("") : TEXT("s")));
					break;
				}
				case BridgePerfImpl::EMeshGroupBy::VertexCountBucket:
				{
					int64 TotalVerts = 0;
					if (FStaticMeshRenderData* RD = SM->GetRenderData())
					{
						for (const FStaticMeshLODResources& LOD : RD->LODResources)
						{
							TotalVerts += LOD.GetNumVertices();
						}
					}
					Key = BridgePerfImpl::VertexCountToBucket(TotalVerts);
					break;
				}
				}
				BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, SM->GetPathName());
			}
		}
		if (bWantSkeletal)
		{
			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				USkeletalMesh* SK = *It;
				if (!SK || !IsValid(SK)) continue;
				const int64 Bytes = static_cast<int64>(SK->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
				FString Key;
				switch (ModeEnum)
				{
				case BridgePerfImpl::EMeshGroupBy::Folder:
				{
					if (UPackage* Pkg = SK->GetOutermost())
					{
						const FString PackageName = Pkg->GetName();
						int32 LastSlash = INDEX_NONE;
						if (PackageName.FindLastChar(TEXT('/'), LastSlash))
						{
							Key = BridgePerfImpl::GetTopLevelFolder(PackageName.Left(LastSlash));
						}
						else
						{
							Key = PackageName;
						}
					}
					else
					{
						Key = TEXT("(transient)");
					}
					break;
				}
				case BridgePerfImpl::EMeshGroupBy::LodCount:
				{
					int32 NumLODs = 0;
					if (FSkeletalMeshRenderData* RD = SK->GetResourceForRendering())
					{
						NumLODs = RD->LODRenderData.Num();
					}
					Key = FString::Printf(TEXT("%d LOD%s"), NumLODs, (NumLODs == 1 ? TEXT("") : TEXT("s")));
					break;
				}
				case BridgePerfImpl::EMeshGroupBy::VertexCountBucket:
				{
					int64 TotalVerts = 0;
					if (FSkeletalMeshRenderData* RD = SK->GetResourceForRendering())
					{
						for (const FSkeletalMeshLODRenderData& LOD : RD->LODRenderData)
						{
							TotalVerts += LOD.GetNumVertices();
						}
					}
					Key = BridgePerfImpl::VertexCountToBucket(TotalVerts);
					break;
				}
				}
				BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, SK->GetPathName());
			}
		}
	}
	else
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		auto WalkAssets = [&Buckets, ModeEnum, &AR](UClass* Cls)
		{
			TArray<FAssetData> Assets;
			AR.GetAssetsByClass(Cls->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
			for (const FAssetData& Data : Assets)
			{
				if (!Data.IsValid()) continue;
				const int64 Bytes = BridgePerfImpl::GetPackageDiskSize(Data.PackageName);
				if (Bytes <= 0) continue;
				const FString Key = BridgePerfImpl::GetMeshGroupKeyFromAssetData(Data, ModeEnum);
				BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, Data.GetSoftObjectPath().ToString());
			}
		};
		if (bWantStatic) WalkAssets(UStaticMesh::StaticClass());
		if (bWantSkeletal) WalkAssets(USkeletalMesh::StaticClass());
	}

	Out.Reserve(Buckets.Num());
	for (const TPair<FString, FBridgePerfBreakdownRow>& Pair : Buckets)
	{
		Out.Add(Pair.Value);
	}
	BridgePerfImpl::FinalizeBreakdownRowsByBytes(Out, MaxGroups);
	return Out;
}

// ─── UObject memory breakdown (M1-4) ────────────────────────

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetUObjectMemoryBreakdown(int32 TopN)
{
	TArray<FBridgePerfBreakdownRow> Out;
	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 200);

	// Walk every live UObject once: bump class counter + accumulate exclusive
	// bytes + remember up to 3 sample paths per class.
	TMap<FName, FBridgePerfBreakdownRow> ByClass;
	ByClass.Reserve(4096);

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (!Obj) continue;
		UClass* Cls = Obj->GetClass();
		if (!Cls) continue;

		FBridgePerfBreakdownRow& Bucket = ByClass.FindOrAdd(Cls->GetFName());
		if (Bucket.Key.IsEmpty())
		{
			Bucket.Key = Cls->GetFName().ToString();
		}
		Bucket.Count += 1;
		// EResourceSizeMode::Exclusive: only objects "owned" by this UObject;
		// avoids triple-counting referenced material/texture/mesh bytes that
		// would inflate aggregate by orders of magnitude.
		Bucket.TotalBytes += static_cast<int64>(Obj->GetResourceSizeBytes(EResourceSizeMode::Exclusive));
		if (Bucket.SamplePaths.Num() < 3)
		{
			Bucket.SamplePaths.Add(Obj->GetPathName());
		}
	}

	Out.Reserve(ByClass.Num());
	for (const TPair<FName, FBridgePerfBreakdownRow>& Pair : ByClass)
	{
		Out.Add(Pair.Value);
	}
	BridgePerfImpl::FinalizeBreakdownRowsByBytes(Out, ClampedTopN);
	return Out;
}

// ─── Audio memory breakdown (M1-5) ──────────────────────────

namespace BridgePerfImpl
{
	enum class EAudioGroupBy : uint8
	{
		CompressionFormat,
		Folder,
		SampleRateBucket,
		ChannelCount,
	};

	static bool ParseAudioGroupBy(const FString& In, EAudioGroupBy& Out)
	{
		if (In.IsEmpty() || In.Equals(TEXT("compression_format"), ESearchCase::IgnoreCase))
		{
			Out = EAudioGroupBy::CompressionFormat;
			return true;
		}
		if (In.Equals(TEXT("folder"), ESearchCase::IgnoreCase)) { Out = EAudioGroupBy::Folder; return true; }
		if (In.Equals(TEXT("sample_rate_bucket"), ESearchCase::IgnoreCase)) { Out = EAudioGroupBy::SampleRateBucket; return true; }
		if (In.Equals(TEXT("channel_count"), ESearchCase::IgnoreCase)) { Out = EAudioGroupBy::ChannelCount; return true; }
		return false;
	}

	static FString SampleRateToBucket(int64 Hz)
	{
		if (Hz <= 0)        return TEXT("(unknown)");
		if (Hz < 8000)      return TEXT("<8k");
		if (Hz < 16000)     return TEXT("8k-16k");
		if (Hz < 22050)     return TEXT("16k-22k");
		if (Hz < 44100)     return TEXT("22k-44k");
		if (Hz < 48000)     return TEXT("44k-48k");
		if (Hz < 96000)     return TEXT("48k-96k");
		return TEXT(">=96k");
	}

	static FString ChannelCountToBucket(int64 N)
	{
		if (N <= 0) return TEXT("(unknown)");
		if (N == 1) return TEXT("Mono");
		if (N == 2) return TEXT("Stereo");
		if (N == 6) return TEXT("5.1");
		if (N == 8) return TEXT("7.1");
		return TEXT("Other");
	}

	static FString GetAudioGroupKeyFromAssetData(const FAssetData& Data, EAudioGroupBy Mode)
	{
		switch (Mode)
		{
		case EAudioGroupBy::Folder:
			return GetTopLevelFolder(Data.PackagePath.ToString());
		case EAudioGroupBy::CompressionFormat:
		{
			FString Tag;
			// SoundAssetCompressionType is the modern tag (5.0+); covers PCM /
			// ADPCM / BinkAudio / Opus / etc.
			if (Data.GetTagValue(TEXT("SoundAssetCompressionType"), Tag) && !Tag.IsEmpty())
			{
				return Tag;
			}
			return TEXT("(unspecified)");
		}
		case EAudioGroupBy::SampleRateBucket:
		{
			return SampleRateToBucket(GetTagInt(Data, TEXT("SampleRate"), -1));
		}
		case EAudioGroupBy::ChannelCount:
		{
			return ChannelCountToBucket(GetTagInt(Data, TEXT("NumChannels"), -1));
		}
		}
		return TEXT("(unknown)");
	}

	static FString GetAudioGroupKeyFromObject(const USoundWave* SW, EAudioGroupBy Mode)
	{
		if (!SW) return TEXT("(null)");
		switch (Mode)
		{
		case EAudioGroupBy::Folder:
		{
			if (UPackage* Pkg = SW->GetOutermost())
			{
				const FString PackageName = Pkg->GetName();
				int32 LastSlash = INDEX_NONE;
				if (PackageName.FindLastChar(TEXT('/'), LastSlash))
				{
					return GetTopLevelFolder(PackageName.Left(LastSlash));
				}
				return PackageName;
			}
			return TEXT("(transient)");
		}
		case EAudioGroupBy::CompressionFormat:
		{
			const ESoundAssetCompressionType Type = SW->GetSoundAssetCompressionType();
			const UEnum* Enum = StaticEnum<ESoundAssetCompressionType>();
			if (Enum)
			{
				return Enum->GetNameStringByValue(static_cast<int64>(Type));
			}
			return FString::FromInt(static_cast<int32>(Type));
		}
		case EAudioGroupBy::SampleRateBucket:
			return SampleRateToBucket(static_cast<int64>(SW->GetSampleRateForCurrentPlatform()));
		case EAudioGroupBy::ChannelCount:
			// USoundWave::NumChannels is a public int32 UPROPERTY across 5.3-5.7.
			// (5.3 doesn't have a public USoundWave::GetNumChannels() — that
			// method lives on FSoundWaveData/FSoundWaveProxy proxy types only.)
			return ChannelCountToBucket(static_cast<int64>(SW->NumChannels));
		}
		return TEXT("(unknown)");
	}
}

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetAudioMemoryBreakdown(
	const FString& GroupBy,
	const FString& Mode,
	int32 MaxGroups)
{
	TArray<FBridgePerfBreakdownRow> Out;

	BridgePerfImpl::EAudioGroupBy ModeEnum = BridgePerfImpl::EAudioGroupBy::CompressionFormat;
	if (!BridgePerfImpl::ParseAudioGroupBy(GroupBy, ModeEnum))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetAudioMemoryBreakdown bad group_by '%s' — expected ")
			TEXT("compression_format | folder | sample_rate_bucket | channel_count"),
			*GroupBy);
		return Out;
	}

	const bool bRuntimeMode = Mode.Equals(TEXT("runtime"), ESearchCase::IgnoreCase);
	const bool bDiskMode = Mode.IsEmpty() || Mode.Equals(TEXT("disk"), ESearchCase::IgnoreCase);
	if (!bRuntimeMode && !bDiskMode)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetAudioMemoryBreakdown bad mode '%s' — expected disk | runtime"),
			*Mode);
		return Out;
	}

	TMap<FString, FBridgePerfBreakdownRow> Buckets;
	Buckets.Reserve(32);

	if (bRuntimeMode)
	{
		for (TObjectIterator<USoundWave> It; It; ++It)
		{
			USoundWave* SW = *It;
			if (!SW || !IsValid(SW)) continue;
			const int64 Bytes = static_cast<int64>(SW->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
			const FString Key = BridgePerfImpl::GetAudioGroupKeyFromObject(SW, ModeEnum);
			BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, SW->GetPathName());
		}
	}
	else
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByClass(USoundWave::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
		for (const FAssetData& Data : Assets)
		{
			if (!Data.IsValid()) continue;
			const int64 Bytes = BridgePerfImpl::GetPackageDiskSize(Data.PackageName);
			if (Bytes <= 0) continue;
			const FString Key = BridgePerfImpl::GetAudioGroupKeyFromAssetData(Data, ModeEnum);
			BridgePerfImpl::AccumulateBucket(Buckets, Key, Bytes, Data.GetSoftObjectPath().ToString());
		}
	}

	Out.Reserve(Buckets.Num());
	for (const TPair<FString, FBridgePerfBreakdownRow>& Pair : Buckets)
	{
		Out.Add(Pair.Value);
	}
	BridgePerfImpl::FinalizeBreakdownRowsByBytes(Out, MaxGroups);
	return Out;
}

// ─── Asset size top-N (M1-6) ────────────────────────────────

namespace BridgePerfImpl
{
	/** Resolve a class filter string to a UClass + the AR query class path.
	 *  Returns true when the filter is empty (no filter — caller should walk
	 *  every asset) or when a class was resolved. Sets OutClassPath to the
	 *  matching class's TopLevelAssetPath; OutClass may be null if we got a
	 *  short-name string that we couldn't resolve to a UClass at runtime. */
	static bool ResolveClassFilter(
		const FString& Filter,
		FTopLevelAssetPath& OutClassPath,
		UClass*& OutClass)
	{
		OutClass = nullptr;
		OutClassPath = FTopLevelAssetPath();

		const FString Trimmed = Filter.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return true; // no filter
		}

		// Full path "/Script/Engine.Texture2D" or "/Game/.../BP_Foo.BP_Foo_C".
		if (Trimmed.StartsWith(TEXT("/")))
		{
			const FTopLevelAssetPath Parsed(Trimmed);
			if (Parsed.IsValid())
			{
				OutClassPath = Parsed;
				OutClass = FindObject<UClass>(nullptr, *Trimmed);
				return true;
			}
			return false;
		}

		// Short name — try resolving as a UClass via FindFirstObject.
		// (UE 5.1+ deprecated FindObject<UClass>(ANY_PACKAGE, ...); the modern
		// API is FindFirstObject which is available 5.3+.)
		if (UClass* Cls = FindFirstObject<UClass>(*Trimmed, EFindFirstObjectOptions::EnsureIfAmbiguous))
		{
			OutClass = Cls;
			OutClassPath = Cls->GetClassPathName();
			return true;
		}

		// Couldn't resolve — agent passed a typo. Refuse to walk the entire
		// AssetRegistry pretending the filter applied.
		return false;
	}
}

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetAssetSizeTopN(
	const FString& ClassFilter,
	int32 TopN)
{
	TArray<FBridgePerfBreakdownRow> Out;
	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetData> Assets;
	const FString TrimmedFilter = ClassFilter.TrimStartAndEnd();
	if (TrimmedFilter.IsEmpty())
	{
		// All assets. Use a wide-open ARFilter rather than walking every
		// known class — the AR has a single internal call for this.
		FARFilter Filter;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(FName(TEXT("/")));
		Filter.bRecursiveClasses = true;
		AR.GetAssets(Filter, Assets);
	}
	else
	{
		FTopLevelAssetPath ClassPath;
		UClass* Cls = nullptr;
		if (!BridgePerfImpl::ResolveClassFilter(TrimmedFilter, ClassPath, Cls))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UnrealBridgePerf: GetAssetSizeTopN can't resolve class_filter '%s' — ")
				TEXT("expected empty, a /Script/Module.Class path, or a known short class name"),
				*ClassFilter);
			return Out;
		}
		// Subclasses included by default — UTexture sweeps Texture2D / TextureCube / etc.
		AR.GetAssetsByClass(ClassPath, Assets, /*bSearchSubClasses=*/true);
	}

	// Build per-asset rows, then top-N by TotalBytes.
	Out.Reserve(FMath::Min(Assets.Num(), 4096));
	for (const FAssetData& Data : Assets)
	{
		if (!Data.IsValid()) continue;
		const int64 Bytes = BridgePerfImpl::GetPackageDiskSize(Data.PackageName);
		if (Bytes <= 0) continue;

		FBridgePerfBreakdownRow Row;
		Row.Key = Data.GetSoftObjectPath().ToString();
		Row.Count = 1;
		Row.TotalBytes = Bytes;
		Row.LevelName = Data.AssetClassPath.ToString(); // class path (overload)
		Out.Add(MoveTemp(Row));
	}
	BridgePerfImpl::FinalizeBreakdownRowsByBytes(Out, ClampedTopN);
	return Out;
}

// ─── M2-4..6: always-on OnEndFrame hook (histogram + hitch log) ─

namespace BridgePerfFrameHook
{
	/**
	 * Storage strategy: every frame we increment one fine-grained 0.5 ms bucket
	 * (FineBucketCount = 401, covering 0..200 ms + overflow). GetFrameTimeHistogram
	 * re-aggregates these into the caller's coarser BucketMs buckets on demand.
	 *
	 * This decouples the OnEndFrame fast path (single index + atomic increment)
	 * from the caller's view parameters. Storage is fixed at ~1.6 KB regardless
	 * of how many different bucket sizes the agent asks for.
	 */
	static constexpr float FineBucketWidthMs = 0.5f;
	static constexpr int32 FineBucketCount = 401; // 400 buckets * 0.5ms = 200ms + 1 overflow
	static constexpr float MaxFineMs = static_cast<float>(FineBucketCount - 1) * FineBucketWidthMs; // 200ms

	/** Below this threshold we don't bother logging the frame as a hitch.
	 *  Anything >= 33 ms is already below 30 fps. Caller's GetHitchLog
	 *  threshold filters further. */
	static constexpr float HitchMinMs = 33.f;

	/** Hard cap on the hitch ring buffer size. ~64 bytes/entry * 200 = 13 KB. */
	static constexpr int32 MaxHitchEntries = 200;

	/** Lock guards both buckets + hitches against simultaneous read from a UFUNCTION
	 *  call and write from the OnEndFrame hook. Both run on GT in practice but the
	 *  scoped lock is cheap and defensive. */
	static FCriticalSection State_Lock;

	static int32 FineBuckets[FineBucketCount] = {};
	static int64 TotalFramesObserved = 0;

	static TArray<FBridgeHitchEntry> Hitches;

	static FDelegateHandle EndFrameHandle;
	static double LastFrameStartSeconds = 0.0;
	static bool bHasPriorFrame = false;

	// ── M8-1 auto-hitch capture state ──
	static bool bAutoHitchActive = false;
	static float AutoHitchThresholdMs = 50.f;
	static int32 AutoHitchMaxEntries = 100;
	static FString AutoHitchStartedAtUtc;
	static TArray<FBridgeAutoHitchEntry> AutoHitchBuffer;

	static void OnEndFrame()
	{
		// Compute frame duration from the FApp wall clock. FApp::GetCurrentTime()
		// is set once per main loop iteration; the delta from the previous
		// OnEndFrame fire is the wall-clock total frame time including idle.
		// On the first call there's no prior sample so just record the time.
		const double Now = FApp::GetCurrentTime();
		float FrameMs = 0.f;
		if (bHasPriorFrame)
		{
			FrameMs = static_cast<float>((Now - LastFrameStartSeconds) * 1000.0);
		}
		LastFrameStartSeconds = Now;
		bHasPriorFrame = true;

		// Defensive: if FApp clock hasn't moved (single-frame editor pause) skip.
		if (FrameMs <= 0.f)
		{
			return;
		}

		// Drop the bucket index. Any frame above MaxFineMs lands in the overflow
		// bucket (last index). Negative shouldn't happen but clamp anyway.
		int32 Idx = FMath::FloorToInt(FrameMs / FineBucketWidthMs);
		if (Idx < 0) Idx = 0;
		if (Idx >= FineBucketCount) Idx = FineBucketCount - 1;

		FScopeLock L(&State_Lock);
		FineBuckets[Idx] += 1;
		TotalFramesObserved += 1;

		if (FrameMs >= HitchMinMs)
		{
			// Source per-thread breakdown from the same globals GetFrameTiming uses.
			FBridgeHitchEntry Entry;
			Entry.FrameNumber = static_cast<int64>(GFrameCounter);
			Entry.TimestampSeconds = Now;
			Entry.GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
			Entry.RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
			{
				uint32 GpuCycles = 0;
				for (uint32 i = 0; i < GNumExplicitGPUsForRendering; ++i)
				{
					GpuCycles += RHIGetGPUFrameCycles(i);
				}
				Entry.GpuMs = FPlatformTime::ToMilliseconds(GpuCycles);
			}
			Entry.TotalMs = FrameMs;

			Hitches.Add(MoveTemp(Entry));
			while (Hitches.Num() > MaxHitchEntries)
			{
				Hitches.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
			}
		}

		// M8-1 auto-hitch capture: independent threshold + richer state.
		if (bAutoHitchActive && FrameMs >= AutoHitchThresholdMs)
		{
			FBridgeAutoHitchEntry AH;
			AH.FrameNumber       = static_cast<int64>(GFrameCounter);
			AH.TimestampSeconds  = Now;
			AH.TimestampUtc      = FDateTime::UtcNow().ToIso8601();
			AH.TotalMs           = FrameMs;
			AH.GameThreadMs      = FPlatformTime::ToMilliseconds(GGameThreadTime);
			AH.RenderThreadMs    = FPlatformTime::ToMilliseconds(GRenderThreadTime);
			{
				uint32 GpuCycles = 0;
				for (uint32 i = 0; i < GNumExplicitGPUsForRendering; ++i)
				{
					GpuCycles += RHIGetGPUFrameCycles(i);
				}
				AH.GpuMs = FPlatformTime::ToMilliseconds(GpuCycles);
			}
			{
				const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
				AH.UsedPhysicalMb     = static_cast<int64>(Stats.UsedPhysical / (1024ull * 1024ull));
				AH.PeakUsedPhysicalMb = static_cast<int64>(Stats.PeakUsedPhysical / (1024ull * 1024ull));
			}
			{
				int32 DrawCalls = 0;
				int32 Prims     = 0;
				for (uint32 i = 0; i < GNumExplicitGPUsForRendering; ++i)
				{
					DrawCalls += GNumDrawCallsRHI[i];
					Prims     += GNumPrimitivesDrawnRHI[i];
				}
				AH.DrawCalls       = DrawCalls;
				AH.PrimitivesDrawn = Prims;
			}

			AutoHitchBuffer.Add(MoveTemp(AH));
			while (AutoHitchBuffer.Num() > AutoHitchMaxEntries)
			{
				AutoHitchBuffer.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
			}
		}
	}

	void Register()
	{
		if (EndFrameHandle.IsValid()) return;
		EndFrameHandle = FCoreDelegates::OnEndFrame.AddStatic(&OnEndFrame);
	}

	void Unregister()
	{
		if (!EndFrameHandle.IsValid()) return;
		FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
		EndFrameHandle.Reset();

		FScopeLock L(&State_Lock);
		FMemory::Memzero(FineBuckets, sizeof(FineBuckets));
		TotalFramesObserved = 0;
		Hitches.Empty();
		bHasPriorFrame = false;
	}

	/** Snapshot the fine buckets + total frame count under a single lock. */
	static void SnapshotFineBuckets(int32 (&OutBuckets)[FineBucketCount], int64& OutTotal)
	{
		FScopeLock L(&State_Lock);
		FMemory::Memcpy(OutBuckets, FineBuckets, sizeof(FineBuckets));
		OutTotal = TotalFramesObserved;
	}

	static void ResetHistogram()
	{
		FScopeLock L(&State_Lock);
		FMemory::Memzero(FineBuckets, sizeof(FineBuckets));
		TotalFramesObserved = 0;
	}

	static void ClearHitches()
	{
		FScopeLock L(&State_Lock);
		Hitches.Empty();
	}

	static TArray<FBridgeHitchEntry> SnapshotHitches()
	{
		FScopeLock L(&State_Lock);
		return Hitches;
	}
}

TArray<FBridgeHistogramBucket> UUnrealBridgePerfLibrary::GetFrameTimeHistogram(
	float BucketMs,
	float MaxBucketMs)
{
	TArray<FBridgeHistogramBucket> Out;

	// Snap caller's bucket width up to the internal resolution.
	float Width = FMath::Clamp(BucketMs, BridgePerfFrameHook::FineBucketWidthMs, 50.f);
	// Round Width to a multiple of FineBucketWidthMs so the re-aggregation is
	// exact (no fractional fine-buckets in any output bucket).
	const float Snapped = FMath::Max(BridgePerfFrameHook::FineBucketWidthMs,
		FMath::RoundToFloat(Width / BridgePerfFrameHook::FineBucketWidthMs)
		* BridgePerfFrameHook::FineBucketWidthMs);
	Width = Snapped;

	const float Max = FMath::Clamp(MaxBucketMs, Width, BridgePerfFrameHook::MaxFineMs);

	int32 SnapshotBuckets[BridgePerfFrameHook::FineBucketCount];
	int64 Total = 0;
	BridgePerfFrameHook::SnapshotFineBuckets(SnapshotBuckets, Total);

	const int32 OutBucketCount = FMath::CeilToInt(Max / Width);
	Out.Reserve(OutBucketCount + 1);
	for (int32 i = 0; i < OutBucketCount; ++i)
	{
		FBridgeHistogramBucket Row;
		Row.LowerMs = static_cast<float>(i) * Width;
		Row.UpperMs = Row.LowerMs + Width;
		Out.Add(Row);
	}
	// Overflow bucket: anything >= Max goes here. UpperMs = FLT_MAX as documented.
	{
		FBridgeHistogramBucket Overflow;
		Overflow.LowerMs = Max;
		Overflow.UpperMs = FLT_MAX;
		Out.Add(Overflow);
	}

	// Distribute fine-bucket counts into the output buckets. Bucket i covers
	// fine indices [floor(LowerMs / FineWidth), floor(UpperMs / FineWidth)).
	// Map each fine bucket's lower edge to the output bucket via integer math.
	const float FineWidth = BridgePerfFrameHook::FineBucketWidthMs;
	for (int32 FineIdx = 0; FineIdx < BridgePerfFrameHook::FineBucketCount; ++FineIdx)
	{
		const int32 Count = SnapshotBuckets[FineIdx];
		if (Count == 0) continue;

		const float FineLowerMs = static_cast<float>(FineIdx) * FineWidth;
		// The last fine bucket holds the storage-level overflow (>= MaxFineMs).
		// If caller's Max < MaxFineMs, this still goes into our visible overflow.
		int32 OutIdx;
		if (FineLowerMs >= Max || FineIdx == BridgePerfFrameHook::FineBucketCount - 1)
		{
			OutIdx = OutBucketCount; // overflow row
		}
		else
		{
			OutIdx = FMath::FloorToInt(FineLowerMs / Width);
			if (OutIdx >= OutBucketCount) OutIdx = OutBucketCount;
			if (OutIdx < 0) OutIdx = 0;
		}
		Out[OutIdx].Count += Count;
	}

	if (Total > 0)
	{
		const float TotalF = static_cast<float>(Total);
		for (FBridgeHistogramBucket& Row : Out)
		{
			Row.Percent = static_cast<float>(Row.Count) / TotalF;
		}
	}

	return Out;
}

TArray<FBridgeHitchEntry> UUnrealBridgePerfLibrary::GetHitchLog(
	float ThresholdMs,
	int32 MaxEntries)
{
	const int32 ClampedMax = FMath::Clamp(MaxEntries, 1, BridgePerfFrameHook::MaxHitchEntries);
	const float Threshold = FMath::Max(ThresholdMs, 0.f);

	TArray<FBridgeHitchEntry> All = BridgePerfFrameHook::SnapshotHitches();

	// Filter by threshold (the captured set already has TotalMs >= HitchMinMs;
	// caller's threshold tightens that further).
	TArray<FBridgeHitchEntry> Out;
	Out.Reserve(FMath::Min(All.Num(), ClampedMax));
	for (const FBridgeHitchEntry& E : All)
	{
		if (E.TotalMs >= Threshold)
		{
			Out.Add(E);
		}
	}

	if (Out.Num() > ClampedMax)
	{
		// Keep most recent (the snapshot is in chronological order).
		const int32 Drop = Out.Num() - ClampedMax;
		Out.RemoveAt(0, Drop, /*EAllowShrinking*/ EAllowShrinking::No);
	}
	return Out;
}

void UUnrealBridgePerfLibrary::ResetFrameTimeHistogram()
{
	BridgePerfFrameHook::ResetHistogram();
}

void UUnrealBridgePerfLibrary::ClearHitchLog()
{
	BridgePerfFrameHook::ClearHitches();
}

// ─── M8-1: Auto-hitch capture ───────────────────────────────

bool UUnrealBridgePerfLibrary::BeginAutoHitchCapture(float ThresholdMs, int32 MaxEntries)
{
	const float ClampedThreshold = FMath::Clamp(ThresholdMs, 10.f, 5000.f);
	const int32 ClampedMax       = FMath::Clamp(MaxEntries, 1, 1000);

	FScopeLock L(&BridgePerfFrameHook::State_Lock);
	BridgePerfFrameHook::bAutoHitchActive    = true;
	BridgePerfFrameHook::AutoHitchThresholdMs = ClampedThreshold;
	BridgePerfFrameHook::AutoHitchMaxEntries = ClampedMax;
	BridgePerfFrameHook::AutoHitchStartedAtUtc = FDateTime::UtcNow().ToIso8601();
	BridgePerfFrameHook::AutoHitchBuffer.Reset();
	return true;
}

TArray<FBridgeAutoHitchEntry> UUnrealBridgePerfLibrary::EndAutoHitchCapture()
{
	FScopeLock L(&BridgePerfFrameHook::State_Lock);
	BridgePerfFrameHook::bAutoHitchActive = false;
	TArray<FBridgeAutoHitchEntry> Out = BridgePerfFrameHook::AutoHitchBuffer;
	BridgePerfFrameHook::AutoHitchBuffer.Reset();
	return Out;
}

FBridgeAutoHitchState UUnrealBridgePerfLibrary::GetAutoHitchState()
{
	FBridgeAutoHitchState Out;
	FScopeLock L(&BridgePerfFrameHook::State_Lock);
	Out.bActive          = BridgePerfFrameHook::bAutoHitchActive;
	Out.ThresholdMs      = BridgePerfFrameHook::AutoHitchThresholdMs;
	Out.EntriesBuffered  = BridgePerfFrameHook::AutoHitchBuffer.Num();
	Out.MaxEntries       = BridgePerfFrameHook::AutoHitchMaxEntries;
	Out.StartedAtUtc     = BridgePerfFrameHook::AutoHitchStartedAtUtc;
	return Out;
}

// ─── M5-4: GetFrameTimePercentiles ──────────────────────────

TArray<float> UUnrealBridgePerfLibrary::GetFrameTimePercentiles(const TArray<float>& Percentiles)
{
	TArray<float> Out;
	Out.Reserve(Percentiles.Num());

	int32 SnapshotBuckets[BridgePerfFrameHook::FineBucketCount];
	int64 Total = 0;
	BridgePerfFrameHook::SnapshotFineBuckets(SnapshotBuckets, Total);

	if (Total <= 0)
	{
		// No frames observed yet — return zero per requested percentile.
		Out.AddZeroed(Percentiles.Num());
		return Out;
	}

	const float FineWidth = BridgePerfFrameHook::FineBucketWidthMs;
	const int32 LastIdx   = BridgePerfFrameHook::FineBucketCount - 1;

	for (float Raw : Percentiles)
	{
		const double P = static_cast<double>(FMath::Clamp(Raw, 0.f, 100.f));

		// Target rank = ceil(P/100 * Total). p=100 → Total; p=0 → 0 (handled
		// specially: report first non-empty bucket lower edge).
		int64 Target = static_cast<int64>(FMath::CeilToDouble((P / 100.0) * static_cast<double>(Total)));
		if (P <= 0.0)
		{
			Target = 1; // any non-empty bucket — yields min observed time.
		}
		Target = FMath::Clamp<int64>(Target, 1, Total);

		// Walk fine buckets cumulating until we hit Target.
		int64 Cum = 0;
		int32 HitBucket = LastIdx;
		for (int32 i = 0; i < BridgePerfFrameHook::FineBucketCount; ++i)
		{
			const int32 BucketCount = SnapshotBuckets[i];
			if (BucketCount <= 0) continue;
			const int64 Next = Cum + BucketCount;
			if (Next >= Target)
			{
				HitBucket = i;
				// Linear interpolation: position within bucket = (Target - Cum) / BucketCount.
				const double Pos = (BucketCount > 0)
					? static_cast<double>(Target - Cum) / static_cast<double>(BucketCount)
					: 0.0;
				if (HitBucket == LastIdx)
				{
					// Overflow bucket: real ms unknown; report lower edge.
					Out.Add(static_cast<float>(HitBucket) * FineWidth);
				}
				else
				{
					const double LowerMs = static_cast<double>(HitBucket) * FineWidth;
					Out.Add(static_cast<float>(LowerMs + Pos * FineWidth));
				}
				goto NextPercentile;
			}
			Cum = Next;
		}
		// Defensive fallback — shouldn't reach here because Target ≤ Total
		// and the cumulative walk is exhaustive.
		Out.Add(static_cast<float>(LastIdx) * FineWidth);
		NextPercentile:;
	}

	return Out;
}

// ─── M2-1..3: opt-in periodic perf sampling ─────────────────

namespace BridgePerfSampler
{
	/**
	 * Module-lifetime singleton holding the FTSTicker handle, configured params,
	 * and the captured FBridgePerfSnapshot ring buffer. Access is funneled
	 * through `Sampler_Lock` because the ticker fires on GT but UFUNCTION
	 * callers can in theory touch state on different threads in tests; both
	 * sides are cheap so a scoped lock is the simplest correct answer.
	 */
	static FCriticalSection Sampler_Lock;

	static FTSTicker::FDelegateHandle TickerHandle;
	static bool bActive = false;
	static FString StartedAtUtc;
	static int32 PeriodMsConfigured = 0;
	static int32 MaxSamplesConfigured = 0;
	static bool bIncludeUObjectStatsConfigured = false;

	static TArray<FBridgePerfSnapshot> Buffer;

	/** Append one snapshot, evicting the oldest when the ring fills. */
	static void AppendSnapshot(FBridgePerfSnapshot&& Snap)
	{
		FScopeLock L(&Sampler_Lock);
		Buffer.Add(MoveTemp(Snap));
		while (Buffer.Num() > MaxSamplesConfigured && MaxSamplesConfigured > 0)
		{
			Buffer.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
		}
	}

	/** Ticker fn — must return true to keep ticking. */
	static bool Tick(float /*DeltaTime*/)
	{
		// GetPerfSnapshot is cheap when bIncludeUObjectStats=false; with
		// uobject stats enabled the call costs 50-300 ms, which we accept
		// because the caller deliberately turned it on with a long period.
		const bool bWantUObjects = bIncludeUObjectStatsConfigured;
		FBridgePerfSnapshot Snap = UUnrealBridgePerfLibrary::GetPerfSnapshot(bWantUObjects, /*UObjectTopN*/ 20);
		AppendSnapshot(MoveTemp(Snap));
		return true; // keep ticking
	}

	/** Tear down the active ticker registration if any. Returns whatever was
	 *  buffered; the buffer is cleared as part of the swap. */
	static TArray<FBridgePerfSnapshot> StopAndDrain()
	{
		TArray<FBridgePerfSnapshot> Out;
		{
			FScopeLock L(&Sampler_Lock);
			if (TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
				TickerHandle.Reset();
			}
			bActive = false;
			Out = MoveTemp(Buffer);
			Buffer.Reset();
		}
		return Out;
	}

	/** Module shutdown hook — release without returning data. */
	void Shutdown()
	{
		FScopeLock L(&Sampler_Lock);
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
		bActive = false;
		Buffer.Reset();
		StartedAtUtc.Reset();
		PeriodMsConfigured = 0;
		MaxSamplesConfigured = 0;
		bIncludeUObjectStatsConfigured = false;
	}

	bool Start(int32 PeriodMs, int32 MaxSamples, bool bIncludeUObjectStats)
	{
		FScopeLock L(&Sampler_Lock);

		// Idempotent restart: cancel any prior run first.
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
		Buffer.Reset();

		PeriodMsConfigured = PeriodMs;
		MaxSamplesConfigured = MaxSamples;
		bIncludeUObjectStatsConfigured = bIncludeUObjectStats;
		StartedAtUtc = FDateTime::UtcNow().ToIso8601();

		const float Period = static_cast<float>(PeriodMs) / 1000.f;
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&Tick), Period);

		bActive = TickerHandle.IsValid();
		return bActive;
	}

	FBridgePerfSamplingState GetState()
	{
		FBridgePerfSamplingState Out;
		FScopeLock L(&Sampler_Lock);
		Out.bActive = bActive;
		Out.StartedAtUtc = StartedAtUtc;
		Out.SamplesCollected = Buffer.Num();
		Out.PeriodMs = PeriodMsConfigured;
		Out.MaxSamples = MaxSamplesConfigured;
		Out.bIncludeUObjectStats = bIncludeUObjectStatsConfigured;
		return Out;
	}

	/** Snapshot the buffer without clearing it (used by CSV export). */
	TArray<FBridgePerfSnapshot> SnapshotBuffer()
	{
		FScopeLock L(&Sampler_Lock);
		return Buffer;
	}
}

bool UUnrealBridgePerfLibrary::StartPerfSampling(
	int32 PeriodMs,
	int32 MaxSamples,
	bool bIncludeUObjectStats)
{
	const int32 ClampedPeriod = FMath::Clamp(PeriodMs, 10, 60000);
	const int32 ClampedMax = FMath::Clamp(MaxSamples, 1, 10000);
	if (bIncludeUObjectStats && ClampedPeriod < 5000)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: StartPerfSampling include_uobject_stats=true with period_ms=%d ")
			TEXT("(<5000) will cause every tick to spend 50-300 ms in TObjectIterator — proceeding ")
			TEXT("but this is expensive."), ClampedPeriod);
	}
	return BridgePerfSampler::Start(ClampedPeriod, ClampedMax, bIncludeUObjectStats);
}

TArray<FBridgePerfSnapshot> UUnrealBridgePerfLibrary::StopPerfSampling()
{
	return BridgePerfSampler::StopAndDrain();
}

FBridgePerfSamplingState UUnrealBridgePerfLibrary::GetPerfSamplingState()
{
	return BridgePerfSampler::GetState();
}

// ─── M2-7: CSV export ───────────────────────────────────────

namespace BridgePerfImpl
{
	/** Escape one field for CSV (RFC 4180): wrap in quotes when it contains
	 *  comma, quote, CR, or LF; double up internal quotes. */
	static FString CsvEscape(const FString& In)
	{
		const bool bNeedsQuote =
			In.Contains(TEXT(",")) ||
			In.Contains(TEXT("\"")) ||
			In.Contains(TEXT("\n")) ||
			In.Contains(TEXT("\r"));
		if (!bNeedsQuote)
		{
			return In;
		}
		FString Escaped = In;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return TEXT("\"") + Escaped + TEXT("\"");
	}

	/** Resolve the caller-provided OutputPath to a concrete file path:
	 *   - empty → <Project>/Saved/UnrealBridge/perf_samples_<unix>.csv
	 *   - directory → that dir + auto-named file
	 *   - anything else → use as-is.
	 *  Also ensures the parent directory exists (creates if missing). */
	static FString ResolveCsvOutputPath(const FString& OutputPath)
	{
		FString Resolved = OutputPath.TrimStartAndEnd();
		const int64 UnixNow = FDateTime::UtcNow().ToUnixTimestamp();

		if (Resolved.IsEmpty())
		{
			Resolved = FPaths::Combine(FPaths::ProjectSavedDir(),
				TEXT("UnrealBridge"),
				FString::Printf(TEXT("perf_samples_%lld.csv"), static_cast<long long>(UnixNow)));
		}
		else
		{
			// Treat path as a directory if either the trailing char is a slash
			// or the path actually exists as a directory on disk.
			const bool bEndsWithSlash =
				Resolved.EndsWith(TEXT("/")) || Resolved.EndsWith(TEXT("\\"));
			const bool bIsDir = bEndsWithSlash || IFileManager::Get().DirectoryExists(*Resolved);
			if (bIsDir)
			{
				Resolved = FPaths::Combine(Resolved,
					FString::Printf(TEXT("perf_samples_%lld.csv"), static_cast<long long>(UnixNow)));
			}
		}

		// Ensure parent dir exists.
		const FString ParentDir = FPaths::GetPath(Resolved);
		if (!ParentDir.IsEmpty() && !IFileManager::Get().DirectoryExists(*ParentDir))
		{
			IFileManager::Get().MakeDirectory(*ParentDir, /*Tree*/ true);
		}
		return Resolved;
	}
}

bool UUnrealBridgePerfLibrary::ExportPerfSamplesToCsv(const FString& OutputPath)
{
	TArray<FBridgePerfSnapshot> Samples = BridgePerfSampler::SnapshotBuffer();
	if (Samples.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: ExportPerfSamplesToCsv — sample buffer empty, nothing to write."));
		return false;
	}

	const FString ResolvedPath = BridgePerfImpl::ResolveCsvOutputPath(OutputPath);

	// Build the CSV string: a single FString::Reserve + Append loop. Each row
	// is ~200-250 chars; reserve generously to avoid mid-build reallocs.
	FString Csv;
	Csv.Reserve(256 + Samples.Num() * 256);

	Csv += TEXT("timestamp_utc,frame_number,fps,frame_ms,gt_ms,rt_ms,gpu_ms,rhi_ms,delta_seconds,")
		TEXT("used_physical_mb,used_virtual_mb,available_physical_mb,draw_calls,primitives_drawn,")
		TEXT("was_in_pie,engine_version\r\n");

	for (const FBridgePerfSnapshot& S : Samples)
	{
		Csv += BridgePerfImpl::CsvEscape(S.CaptureTimeUtc);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%lld"), static_cast<long long>(S.Timing.FrameNumber));
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.3f"), S.Timing.Fps);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.3f"), S.Timing.FrameMs);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.3f"), S.Timing.GameThreadMs);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.3f"), S.Timing.RenderThreadMs);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.3f"), S.Timing.GpuMs);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.3f"), S.Timing.RhiMs);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%.6f"), S.Timing.DeltaSeconds);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%lld"), static_cast<long long>(S.Memory.UsedPhysicalMb));
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%lld"), static_cast<long long>(S.Memory.UsedVirtualMb));
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%lld"), static_cast<long long>(S.Memory.AvailablePhysicalMb));
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%d"), S.Render.DrawCalls);
		Csv += TEXT(",");
		Csv += FString::Printf(TEXT("%d"), S.Render.PrimitivesDrawn);
		Csv += TEXT(",");
		Csv += S.bWasInPie ? TEXT("true") : TEXT("false");
		Csv += TEXT(",");
		Csv += BridgePerfImpl::CsvEscape(S.EngineVersion);
		Csv += TEXT("\r\n");
	}

	const bool bOk = FFileHelper::SaveStringToFile(Csv, *ResolvedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: ExportPerfSamplesToCsv — failed to write '%s' (%d samples)"),
			*ResolvedPath, Samples.Num());
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("UnrealBridgePerf: wrote %d perf samples to '%s'"),
		Samples.Num(), *ResolvedPath);
	return true;
}

// ─── M3: render breakdown helpers ───────────────────────────

namespace BridgePerfRender
{
	/** LOD0 triangle count for a static mesh asset (0 when unavailable). */
	static int64 GetStaticMeshLod0Triangles(const UStaticMesh* Mesh)
	{
		if (!Mesh)
		{
			return 0;
		}
		const FStaticMeshRenderData* RD = Mesh->GetRenderData();
		if (!RD || RD->LODResources.Num() == 0)
		{
			return 0;
		}
		const FStaticMeshLODResources& LOD0 = RD->LODResources[0];
		return static_cast<int64>(LOD0.GetNumTriangles());
	}

	/** LOD0 triangle count for a skeletal mesh asset (0 when unavailable). */
	static int64 GetSkeletalMeshLod0Triangles(const USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return 0;
		}
		const FSkeletalMeshRenderData* RD = Mesh->GetResourceForRendering();
		if (!RD || RD->LODRenderData.Num() == 0)
		{
			return 0;
		}
		return static_cast<int64>(RD->LODRenderData[0].GetTotalFaces());
	}

	/** Best-effort "current LOD index" for a primitive component:
	 *   - StaticMeshComponent: ForcedLodModel - 1 when forced (>0); else 0.
	 *   - SkeletalMesh / Skinned: GetPredictedLODLevel() if non-negative; else 0.
	 *  Returns -1 when the component type isn't recognized as mesh-like. */
	static int32 ResolveComponentLodIndex(const UPrimitiveComponent* Comp)
	{
		if (!Comp)
		{
			return -1;
		}
		if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
		{
			// Direct field access — works on every UE version (5.3 has no
			// GetForcedLodModel inline accessor; the field is public).
			const int32 Forced = SMC->ForcedLodModel;
			return Forced > 0 ? (Forced - 1) : 0;
		}
		if (const USkinnedMeshComponent* Sk = Cast<USkinnedMeshComponent>(Comp))
		{
			const int32 Predicted = Sk->GetPredictedLODLevel();
			return Predicted >= 0 ? Predicted : 0;
		}
		return -1;
	}

	/** Pull the mesh asset path that drives this component's LOD bucket. */
	static FString GetMeshAssetPathForLod(const UPrimitiveComponent* Comp)
	{
		if (!Comp) return FString();
		if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
		{
			if (UStaticMesh* SM = SMC->GetStaticMesh())
			{
				return SM->GetPathName();
			}
		}
		if (const USkeletalMeshComponent* SkC = Cast<USkeletalMeshComponent>(Comp))
		{
			if (USkeletalMesh* Sk = SkC->GetSkeletalMeshAsset())
			{
				return Sk->GetPathName();
			}
		}
		return FString();
	}

	/** Aggregate per-component triangle estimate for an actor.
	 *  Static mesh components → LOD0 tri count from RenderData.
	 *  Skeletal mesh components → LOD0 tri count from RenderData.
	 *  Other primitive types → 0. */
	static int64 EstimateComponentTriangles(const UPrimitiveComponent* Comp)
	{
		if (!Comp) return 0;
		if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
		{
			return GetStaticMeshLod0Triangles(SMC->GetStaticMesh());
		}
		if (const USkeletalMeshComponent* SkC = Cast<USkeletalMeshComponent>(Comp))
		{
			return GetSkeletalMeshLod0Triangles(SkC->GetSkeletalMeshAsset());
		}
		return 0;
	}

	/** Build per-actor render cost row from a UPrimitiveComponent set.
	 *  Used both by GetActorRenderCost and GetShadowCasterBreakdown. */
	static FBridgeActorRenderCost BuildActorCostFromComponents(
		const AActor* Actor,
		const TArray<UPrimitiveComponent*>& Comps)
	{
		FBridgeActorRenderCost Out;
		if (!Actor || Comps.Num() == 0)
		{
			return Out;
		}
		Out.ActorPath = Actor->GetPathName();
		Out.PrimitiveComponentCount = Comps.Num();

		TSet<FString> SeenMaterials;
		SeenMaterials.Reserve(Comps.Num() * 2);

		for (UPrimitiveComponent* Comp : Comps)
		{
			if (!Comp) continue;
			Out.MaterialSlotCount += Comp->GetNumMaterials();
			Out.EstimatedTriangleCount += EstimateComponentTriangles(Comp);
			if (Comp->bCastDynamicShadow)
			{
				Out.bCastsDynamicShadow = true;
			}
			TArray<UMaterialInterface*> MatList;
			Comp->GetUsedMaterials(MatList);
			for (UMaterialInterface* Mat : MatList)
			{
				if (!Mat) continue;
				const FString Path = Mat->GetPathName();
				if (!SeenMaterials.Contains(Path))
				{
					SeenMaterials.Add(Path);
					Out.Materials.Add(Path);
				}
			}
		}
		return Out;
	}
}

// ─── M3-1: get_visible_primitives_by_material ─────────────────

TArray<FBridgeMaterialRenderRow> UUnrealBridgePerfLibrary::GetVisiblePrimitivesByMaterial(
	int32 ViewportIndex,
	int32 TopN)
{
	(void)ViewportIndex; // reserved for future RT-side enrichment

	TArray<FBridgeMaterialRenderRow> Out;
	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);

	UWorld* World = BridgePerfImpl::GetEditorWorldForPerf();
	if (!World)
	{
		return Out;
	}

	// Per-material accumulator: (primitive_count, sample_actor_paths,
	// triangle_total). Triangle total per material is the sum of LOD0
	// triangles from each contributing primitive — overcounts when a single
	// primitive references the same material on multiple slots, which is the
	// common pattern but is what we want here ("how much geometry is this
	// material driving").
	struct FMatAcc
	{
		int32 PrimitiveCount = 0;
		int64 TotalTriangles = 0;
		TArray<FString> SampleActorPaths;
	};
	TMap<FString, FMatAcc> Acc;
	Acc.Reserve(512);

	// Walk every actor in every level; per-actor pull primitive components
	// and accumulate into the material map. We use TActorIterator to skip
	// the (rare) per-level Actors[] entries that are pending kill / unset.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !IsValid(Actor)) continue;

		TArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents<UPrimitiveComponent>(Prims);
		if (Prims.Num() == 0) continue;

		const FString ActorPath = Actor->GetPathName();
		// Track which materials this actor already contributed to so we don't
		// double-add the actor's path to the same material's sample list.
		TSet<FString> SeenForActor;
		SeenForActor.Reserve(8);

		for (UPrimitiveComponent* Comp : Prims)
		{
			if (!Comp) continue;

			// Triangles attributed to each material on this component:
			// total component triangles split across distinct materials.
			// This is approximate — different sections drive different tri
			// budgets — but gives a useful proxy without hitting per-section
			// data which lives in the render proxy.
			TArray<UMaterialInterface*> CompMaterials;
			Comp->GetUsedMaterials(CompMaterials);
			if (CompMaterials.Num() == 0) continue;

			const int64 CompTriangles =
				BridgePerfRender::EstimateComponentTriangles(Comp);
			TSet<UMaterialInterface*> DistinctMaterials;
			DistinctMaterials.Reserve(CompMaterials.Num());
			for (UMaterialInterface* M : CompMaterials)
			{
				if (M)
				{
					DistinctMaterials.Add(M);
				}
			}
			const int32 DistinctCount = DistinctMaterials.Num();
			if (DistinctCount == 0) continue;

			const int64 PerMaterialTris =
				DistinctCount > 0 ? (CompTriangles / DistinctCount) : 0;

			for (UMaterialInterface* Mat : DistinctMaterials)
			{
				if (!Mat) continue;
				const FString MatPath = Mat->GetPathName();
				FMatAcc& Bucket = Acc.FindOrAdd(MatPath);
				Bucket.PrimitiveCount += 1;
				Bucket.TotalTriangles += PerMaterialTris;
				if (Bucket.SampleActorPaths.Num() < 3 && !SeenForActor.Contains(MatPath))
				{
					Bucket.SampleActorPaths.Add(ActorPath);
					SeenForActor.Add(MatPath);
				}
			}
		}
	}

	// Materialize into output rows.
	Out.Reserve(Acc.Num());
	for (const TPair<FString, FMatAcc>& Entry : Acc)
	{
		FBridgeMaterialRenderRow Row;
		Row.MaterialPath = Entry.Key;
		Row.PrimitiveCount = Entry.Value.PrimitiveCount;
		Row.TotalTriangles = Entry.Value.TotalTriangles;
		Row.SampleActorPaths = Entry.Value.SampleActorPaths;
		Out.Add(MoveTemp(Row));
	}

	// Sort by (PrimitiveCount desc, TotalTriangles desc, MaterialPath asc),
	// then truncate to TopN.
	Out.Sort([](const FBridgeMaterialRenderRow& A, const FBridgeMaterialRenderRow& B)
	{
		if (A.PrimitiveCount != B.PrimitiveCount)
		{
			return A.PrimitiveCount > B.PrimitiveCount;
		}
		if (A.TotalTriangles != B.TotalTriangles)
		{
			return A.TotalTriangles > B.TotalTriangles;
		}
		return A.MaterialPath < B.MaterialPath;
	});
	if (Out.Num() > ClampedTopN)
	{
		Out.SetNum(ClampedTopN);
	}

	return Out;
}

// ─── M3-2: get_actor_render_cost ────────────────────────────

FBridgeActorRenderCost UUnrealBridgePerfLibrary::GetActorRenderCost(const FString& ActorPath)
{
	FBridgeActorRenderCost Out;

	UWorld* World = BridgePerfImpl::GetEditorWorldForPerf();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridgePerf: GetActorRenderCost — no editor world available"));
		return Out;
	}
	if (ActorPath.IsEmpty())
	{
		return Out;
	}

	// Resolve the actor by path. FindObject<AActor> on the qualified path
	// works for "/Game/Map.Map:PersistentLevel.<ActorName>" and equivalent.
	AActor* Actor = FindObject<AActor>(nullptr, *ActorPath);
	if (!Actor)
	{
		// Fallback: walk World->GetLevels() and match by GetPathName().
		// Slower but covers cases where FindObject couldn't resolve the
		// outer chain (level not loaded into the search path, etc.).
		for (ULevel* Level : World->GetLevels())
		{
			if (!Level) continue;
			for (AActor* Cand : Level->Actors)
			{
				if (Cand && Cand->GetPathName() == ActorPath)
				{
					Actor = Cand;
					break;
				}
			}
			if (Actor) break;
		}
	}
	if (!Actor)
	{
		return Out;
	}

	TArray<UPrimitiveComponent*> Prims;
	Actor->GetComponents<UPrimitiveComponent>(Prims);
	return BridgePerfRender::BuildActorCostFromComponents(Actor, Prims);
}

// ─── M3-3: get_lod_distribution ─────────────────────────────

TArray<FBridgePerfBreakdownRow> UUnrealBridgePerfLibrary::GetLodDistribution(
	const FString& ClassFilter,
	const FString& ActorFilter)
{
	TArray<FBridgePerfBreakdownRow> Out;

	UWorld* World = BridgePerfImpl::GetEditorWorldForPerf();
	if (!World)
	{
		return Out;
	}

	const FString TrimmedClassFilter = ClassFilter.TrimStartAndEnd();
	const FString TrimmedActorFilter = ActorFilter.TrimStartAndEnd();

	// Map (mesh_path, lod_index) → accumulator.
	struct FLodAcc
	{
		int32 Count = 0;
		TArray<FString> Samples;
	};
	using FKey = TPair<FString, int32>;
	TMap<FKey, FLodAcc> Acc;
	Acc.Reserve(256);

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level) continue;
		for (AActor* Actor : Level->Actors)
		{
			if (!Actor || !IsValid(Actor)) continue;

			if (!TrimmedActorFilter.IsEmpty())
			{
				if (!Actor->GetFName().ToString().Contains(TrimmedActorFilter, ESearchCase::IgnoreCase))
				{
					continue;
				}
			}

			TArray<UPrimitiveComponent*> Prims;
			Actor->GetComponents<UPrimitiveComponent>(Prims);
			for (UPrimitiveComponent* Comp : Prims)
			{
				if (!Comp) continue;

				if (!TrimmedClassFilter.IsEmpty())
				{
					const FString CompClass = Comp->GetClass()->GetFName().ToString();
					if (!CompClass.Contains(TrimmedClassFilter, ESearchCase::IgnoreCase))
					{
						continue;
					}
				}

				const FString MeshPath = BridgePerfRender::GetMeshAssetPathForLod(Comp);
				if (MeshPath.IsEmpty())
				{
					continue;
				}
				const int32 LodIdx = BridgePerfRender::ResolveComponentLodIndex(Comp);
				if (LodIdx < 0)
				{
					continue;
				}

				FKey K(MeshPath, LodIdx);
				FLodAcc& Bucket = Acc.FindOrAdd(K);
				++Bucket.Count;
				if (Bucket.Samples.Num() < 3)
				{
					Bucket.Samples.Add(Actor->GetPathName());
				}
			}
		}
	}

	Out.Reserve(Acc.Num());
	for (const TPair<FKey, FLodAcc>& Entry : Acc)
	{
		FBridgePerfBreakdownRow Row;
		Row.Key = FString::Printf(TEXT("%s:LOD%d"), *Entry.Key.Key, Entry.Key.Value);
		Row.Count = Entry.Value.Count;
		Row.TotalBytes = 0;
		Row.SamplePaths = Entry.Value.Samples;
		Out.Add(MoveTemp(Row));
	}

	BridgePerfImpl::FinalizeBreakdownRows(Out, /*MaxGroups*/ 1000);
	return Out;
}

// ─── M3-4 / M3-5: Lumen + Nanite diagnostics (5.7+ only) ────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "LumenVisualizationData.h"
#include "Rendering/NaniteStreamingManager.h"

namespace BridgePerfRender
{
	/** Look up an int32 CVar by name; return its current value, or `Default`
	 *  when the CVar isn't registered. */
	static int32 GetCVarInt(const TCHAR* Name, int32 Default)
	{
		IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name);
		return CV ? CV->GetInt() : Default;
	}

	/** Look up an FString CVar by name (TCHAR view); return value or empty. */
	static FString GetCVarString(const TCHAR* Name)
	{
		IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name);
		return CV ? CV->GetString() : FString();
	}
}

FBridgeLumenDiagnostics UUnrealBridgePerfLibrary::GetLumenDiagnostics()
{
	FBridgeLumenDiagnostics Out;
	Out.EngineVersion = FEngineVersion::Current().ToString();

	// FLumenVisualizationData is the only Lumen state with a public API
	// surface. ENGINE_API getter; safe to call from GT. The data is lazily
	// initialized by the renderer when the visualization viewmode console
	// commands are first registered.
	const FLumenVisualizationData& Vis = GetLumenVisualizationData();
	Out.bAvailable = Vis.IsInitialized();
	if (Out.bAvailable)
	{
		const FLumenVisualizationData::TModeMap& Modes = Vis.GetModeMap();
		Out.VisualizationModes.Reserve(Modes.Num());
		// TMultiMap doesn't dedupe by FName; use a TSet to keep the public
		// list distinct and stable.
		TSet<FString> Seen;
		Seen.Reserve(Modes.Num());
		for (const auto& Pair : Modes)
		{
			const FString Name = Pair.Key.ToString();
			if (!Seen.Contains(Name))
			{
				Seen.Add(Name);
				Out.VisualizationModes.Add(Name);
			}
		}
		Out.VisualizationModes.Sort();
	}

	// Active mode = current value of r.Lumen.Visualize.ViewMode (FString).
	Out.ActiveVisualizationMode = BridgePerfRender::GetCVarString(
		FLumenVisualizationData::GetVisualizeConsoleCommandName());

	// Lumen GI / reflections enable state — read the canonical CVars. These
	// are int (0=off, non-zero=method id). The Lumen method values are
	// stable enough across 5.6 / 5.7 to use a non-zero check; a UE major
	// version that breaks this would also be 6.0+.
	const int32 GiMethod = BridgePerfRender::GetCVarInt(
		TEXT("r.DynamicGlobalIlluminationMethod"), 0);
	const int32 ReflMethod = BridgePerfRender::GetCVarInt(
		TEXT("r.ReflectionMethod"), 0);
	// Method ID 1 = Lumen for both CVars (per EDynamicGlobalIlluminationMethod
	// / EReflectionMethod). Anything else is None / Screen Space / Path Tracing.
	Out.bLumenGiEnabled = (GiMethod == 1);
	Out.bLumenReflectionsEnabled = (ReflMethod == 1);

	return Out;
}

FBridgeNaniteStats UUnrealBridgePerfLibrary::GetNaniteStats()
{
	FBridgeNaniteStats Out;
	Out.EngineVersion = FEngineVersion::Current().ToString();

	// Nanite::GStreamingManager is an ENGINE_API global; the public getters
	// are GT-callable. The HasResourceEntries / IsSafeForRendering reads
	// query members maintained by the streaming manager; the per-frame
	// counts (StatNumRootPages, StatVisibleSetSize, ...) are private.
	Out.bAvailable = Nanite::GStreamingManager.HasResourceEntries();
	Out.MaxStreamingPages = static_cast<int32>(Nanite::GStreamingManager.GetMaxStreamingPages());
	Out.MaxHierarchyLevels = static_cast<int32>(Nanite::GStreamingManager.GetMaxHierarchyLevels());
	Out.bIsSafeForRendering = Nanite::GStreamingManager.IsSafeForRendering();

	// GT-side scan for components whose StaticMesh actually has Nanite data.
	// Independent of the streaming manager's view; useful for "this scene has
	// N Nanite mesh components" without needing a render frame to be in flight.
	int32 NaniteCount = 0;
	if (UWorld* World = BridgePerfImpl::GetEditorWorldForPerf())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !IsValid(Actor)) continue;
			TArray<UStaticMeshComponent*> SMCs;
			Actor->GetComponents<UStaticMeshComponent>(SMCs);
			for (UStaticMeshComponent* SMC : SMCs)
			{
				if (!SMC) continue;
				UStaticMesh* SM = SMC->GetStaticMesh();
				if (SM && SM->IsNaniteEnabled() && SM->HasValidNaniteData())
				{
					++NaniteCount;
				}
			}
		}
	}
	Out.NaniteStaticMeshComponents = NaniteCount;

	return Out;
}

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

// ─── M3-6: get_shadow_caster_breakdown ──────────────────────

TArray<FBridgeActorRenderCost> UUnrealBridgePerfLibrary::GetShadowCasterBreakdown(int32 TopN)
{
	TArray<FBridgeActorRenderCost> Out;

	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);

	UWorld* World = BridgePerfImpl::GetEditorWorldForPerf();
	if (!World)
	{
		return Out;
	}

	// Walk editor-world actors, keep only those with at least one
	// shadow-casting primitive. We accumulate the full render cost row
	// (same as GetActorRenderCost) but with the EstimatedTriangleCount
	// computed from ONLY the shadow-casting subset of primitives — that's
	// the actually-relevant cost number, since non-casting components
	// don't contribute to shadow render time.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !IsValid(Actor)) continue;

		TArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents<UPrimitiveComponent>(Prims);
		if (Prims.Num() == 0) continue;

		// Filter to shadow-casting primitives only. If none, skip.
		TArray<UPrimitiveComponent*> ShadowCasters;
		ShadowCasters.Reserve(Prims.Num());
		for (UPrimitiveComponent* Comp : Prims)
		{
			if (Comp && Comp->bCastDynamicShadow)
			{
				ShadowCasters.Add(Comp);
			}
		}
		if (ShadowCasters.Num() == 0) continue;

		// Build the cost row from the shadow-caster subset. The row's
		// PrimitiveComponentCount + EstimatedTriangleCount thus reflect
		// shadow-caster counts only, not the full actor.
		FBridgeActorRenderCost Row =
			BridgePerfRender::BuildActorCostFromComponents(Actor, ShadowCasters);
		// Forced true (every contributing primitive cast a shadow).
		Row.bCastsDynamicShadow = true;
		if (Row.EstimatedTriangleCount > 0 || Row.PrimitiveComponentCount > 0)
		{
			Out.Add(MoveTemp(Row));
		}
	}

	// Sort by (EstimatedTriangleCount desc, PrimitiveComponentCount desc,
	// ActorPath asc) and clamp to TopN.
	Out.Sort([](const FBridgeActorRenderCost& A, const FBridgeActorRenderCost& B)
	{
		if (A.EstimatedTriangleCount != B.EstimatedTriangleCount)
		{
			return A.EstimatedTriangleCount > B.EstimatedTriangleCount;
		}
		if (A.PrimitiveComponentCount != B.PrimitiveComponentCount)
		{
			return A.PrimitiveComponentCount > B.PrimitiveComponentCount;
		}
		return A.ActorPath < B.ActorPath;
	});
	if (Out.Num() > ClampedTopN)
	{
		Out.SetNum(ClampedTopN);
	}

	return Out;
}

// ─── M4: UE Trace integration ──────────────────────────────

namespace BridgePerfTrace
{
	/** Single global cs guarding the cached capture state below. The
	 *  underlying FTraceAuxiliary calls are themselves thread-safe; this
	 *  lock only protects our shadow copy of (path, channels, started_at,
	 *  start_seconds) which we read back at stop time. */
	static FCriticalSection State_Lock;

	/** True between StartTraceCapture success and StopTraceCapture (or until
	 *  GetTraceState reconciles with FTraceAuxiliary::IsConnected() == false). */
	static bool bActive = false;

	/** Absolute path the active or last capture wrote to. Survives a stop so
	 *  GetTraceState can keep reporting "last run path". */
	static FString ActivePath;

	/** Channel set passed to Start (split list, sorted). */
	static TArray<FString> ActiveChannels;

	/** Configured cap (advisory only). Recorded for parity with the spec. */
	static int32 ActiveMaxSizeMb = 0;

	/** ISO-8601 UTC timestamp of the matching Start. */
	static FString ActiveStartedAtUtc;

	/** Wall clock seconds at start (FApp::GetCurrentTime). Used for Stop's
	 *  duration_seconds. */
	static double ActiveStartSeconds = 0.0;

	/** Default channel set when caller passed an empty list. The 5.4+ trace
	 *  API also accepts the literal "default" preset for this. */
	static FString DefaultChannelString()
	{
		return TEXT("default");
	}

	/** Join channel names with commas, trimming each entry and dropping empties. */
	static FString JoinChannels(const TArray<FString>& Channels)
	{
		TArray<FString> Cleaned;
		Cleaned.Reserve(Channels.Num());
		for (const FString& C : Channels)
		{
			const FString Trimmed = C.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Cleaned.Add(Trimmed);
			}
		}
		return FString::Join(Cleaned, TEXT(","));
	}

	/** Resolve OutputDir to a final absolute file path. Handles three shapes:
	 *  empty → default Saved/UnrealBridge/Traces/<unix>.utrace; existing or
	 *  obvious-directory string → Dir + auto-named file; anything else → use
	 *  as-is. Caller's responsibility to ensure parent dir is creatable. */
	static FString ResolveTraceFilePath(const FString& OutputDir)
	{
		const FString DefaultDir = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("UnrealBridge"),
			TEXT("Traces"));
		const int64 Unix = FDateTime::UtcNow().ToUnixTimestamp();
		const FString AutoName = FString::Printf(TEXT("%lld.utrace"), Unix);

		if (OutputDir.IsEmpty())
		{
			return FPaths::Combine(DefaultDir, AutoName);
		}

		// "Obvious directory": existing dir on disk OR a path that ends in a
		// path separator OR has no extension. The third heuristic catches new
		// directories the caller wants us to create. We never silently
		// overwrite a caller-supplied filename — preserve as-is.
		const FString Trimmed = OutputDir.TrimStartAndEnd();
		const bool bExistsAsDir = IFileManager::Get().DirectoryExists(*Trimmed);
		const bool bEndsInSep = Trimmed.EndsWith(TEXT("/")) || Trimmed.EndsWith(TEXT("\\"));
		const bool bNoExt = FPaths::GetExtension(Trimmed).IsEmpty();
		if (bExistsAsDir || bEndsInSep || bNoExt)
		{
			return FPaths::Combine(Trimmed, AutoName);
		}
		return Trimmed;
	}

	/** Ensure the parent directory of `FilePath` exists (creating it if not). */
	static bool EnsureParentDir(const FString& FilePath)
	{
		const FString ParentDir = FPaths::GetPath(FilePath);
		if (ParentDir.IsEmpty())
		{
			return true;
		}
		IFileManager& FM = IFileManager::Get();
		if (FM.DirectoryExists(*ParentDir))
		{
			return true;
		}
		return FM.MakeDirectory(*ParentDir, /*Tree=*/true);
	}

	/** Stat the trace file size; 0 if absent (just-flushed or never created). */
	static int64 QueryFileSize(const FString& FilePath)
	{
		if (FilePath.IsEmpty())
		{
			return 0;
		}
		const int64 Size = IFileManager::Get().FileSize(*FilePath);
		return Size > 0 ? Size : 0;
	}

	/** Reset the cached state to "never started". */
	static void ResetState_NoLock()
	{
		bActive = false;
		ActivePath.Reset();
		ActiveChannels.Reset();
		ActiveMaxSizeMb = 0;
		ActiveStartedAtUtc.Reset();
		ActiveStartSeconds = 0.0;
	}
}

FBridgeTraceStartResult UUnrealBridgePerfLibrary::StartTraceCapture(
	const TArray<FString>& Channels,
	const FString& OutputDir,
	int32 MaxSizeMb)
{
	FBridgeTraceStartResult Out;
	const int32 ClampedMaxMb = FMath::Clamp(MaxSizeMb, 0, 65536);

	{
		FScopeLock L(&BridgePerfTrace::State_Lock);
		if (BridgePerfTrace::bActive)
		{
			Out.bSuccess = false;
			Out.Error = TEXT("already_active");
			Out.Path = BridgePerfTrace::ActivePath;
			Out.Channels = BridgePerfTrace::ActiveChannels;
			return Out;
		}
	}

	const FString FilePath = BridgePerfTrace::ResolveTraceFilePath(OutputDir);
	if (!BridgePerfTrace::EnsureParentDir(FilePath))
	{
		Out.bSuccess = false;
		Out.Error = FString::Printf(TEXT("could_not_create_parent_dir: %s"),
			*FPaths::GetPath(FilePath));
		return Out;
	}

	// Build the channel list: empty caller list ⇒ engine "default" preset.
	// Otherwise comma-join trimmed entries. Engine accepts either a CSV list
	// of channel names or a registered preset name in the same string.
	FString ChannelStr = BridgePerfTrace::JoinChannels(Channels);
	if (ChannelStr.IsEmpty())
	{
		ChannelStr = BridgePerfTrace::DefaultChannelString();
	}

	bool bStarted = false;
	FString ErrorMsg;

#if !UE_VERSION_OLDER_THAN(5, 4, 0)
	// 5.4+ public API. FTraceAuxiliary::Start() returns false if a connection
	// is already active, the target couldn't be opened, or trace was disabled
	// at compile time. We don't expose Options yet — defaults match the
	// engine's own "Trace.Start" console command behavior.
	bStarted = FTraceAuxiliary::Start(
		FTraceAuxiliary::EConnectionType::File,
		*FilePath,
		*ChannelStr,
		/*Options=*/nullptr);
	if (!bStarted)
	{
		ErrorMsg = TEXT("FTraceAuxiliary::Start returned false (already connected, file open failed, or trace disabled at compile time)");
	}
#else
	// 5.3 fallback: drive the trace system through the console. The Trace
	// system parses File="..." and Channels=... from the cmd line. We pass
	// World=nullptr because trace is editor-process global, not world-scoped.
	if (GEngine)
	{
		const FString Cmd = FString::Printf(
			TEXT("Trace.Start File=\"%s\" Channels=%s"),
			*FilePath,
			*ChannelStr);
		bStarted = GEngine->Exec(/*World=*/nullptr, *Cmd);
		if (!bStarted)
		{
			ErrorMsg = TEXT("GEngine->Exec(Trace.Start) returned false");
		}
	}
	else
	{
		ErrorMsg = TEXT("GEngine null; cannot run Trace.Start");
	}
#endif

	if (!bStarted)
	{
		Out.bSuccess = false;
		Out.Error = ErrorMsg.IsEmpty() ? TEXT("trace_start_failed") : ErrorMsg;
		return Out;
	}

	// Persist shadow state so Stop / GetState can read it back.
	{
		FScopeLock L(&BridgePerfTrace::State_Lock);
		BridgePerfTrace::bActive = true;
		BridgePerfTrace::ActivePath = FilePath;
		BridgePerfTrace::ActiveMaxSizeMb = ClampedMaxMb;
		BridgePerfTrace::ActiveStartedAtUtc = FDateTime::UtcNow().ToIso8601();
		BridgePerfTrace::ActiveStartSeconds = FApp::GetCurrentTime();

		// Split the joined string back into entries for the result struct.
		// Use the original list when present, otherwise echo "default".
		BridgePerfTrace::ActiveChannels.Reset();
		if (Channels.Num() > 0)
		{
			for (const FString& C : Channels)
			{
				const FString T = C.TrimStartAndEnd();
				if (!T.IsEmpty())
				{
					BridgePerfTrace::ActiveChannels.Add(T);
				}
			}
		}
		if (BridgePerfTrace::ActiveChannels.Num() == 0)
		{
			BridgePerfTrace::ActiveChannels.Add(BridgePerfTrace::DefaultChannelString());
		}

		Out.Path = BridgePerfTrace::ActivePath;
		Out.Channels = BridgePerfTrace::ActiveChannels;
	}
	Out.bSuccess = true;
	Out.Error.Reset();
	return Out;
}

FBridgeTraceStopResult UUnrealBridgePerfLibrary::StopTraceCapture()
{
	FBridgeTraceStopResult Out;

	FString PathSnapshot;
	double StartSecondsSnapshot = 0.0;
	bool bWasActive = false;
	{
		FScopeLock L(&BridgePerfTrace::State_Lock);
		bWasActive = BridgePerfTrace::bActive;
		PathSnapshot = BridgePerfTrace::ActivePath;
		StartSecondsSnapshot = BridgePerfTrace::ActiveStartSeconds;
	}

	bool bStopped = false;
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
	bStopped = FTraceAuxiliary::Stop();
#else
	if (GEngine)
	{
		bStopped = GEngine->Exec(/*World=*/nullptr, TEXT("Trace.Stop"));
	}
#endif

	// Capture final size before resetting state — small race window between
	// the API's stop and the OS flush, but good enough for diagnostics.
	const int64 SizeBytes = BridgePerfTrace::QueryFileSize(PathSnapshot);
	const double NowSeconds = FApp::GetCurrentTime();
	const double Duration = (StartSecondsSnapshot > 0.0)
		? FMath::Max(0.0, NowSeconds - StartSecondsSnapshot)
		: 0.0;

	{
		FScopeLock L(&BridgePerfTrace::State_Lock);
		// Only flip bActive off if we were the ones who started it; leave the
		// path / channels populated so GetTraceState can still report the
		// last run.
		BridgePerfTrace::bActive = false;
		BridgePerfTrace::ActiveStartSeconds = 0.0;
	}

	Out.bSuccess = bStopped && bWasActive;
	Out.Path = PathSnapshot;
	Out.SizeBytes = SizeBytes;
	Out.DurationSeconds = static_cast<float>(Duration);
	return Out;
}

FBridgeTraceState UUnrealBridgePerfLibrary::GetTraceState()
{
	FBridgeTraceState Out;
	{
		FScopeLock L(&BridgePerfTrace::State_Lock);
		Out.bActive = BridgePerfTrace::bActive;
		Out.Path = BridgePerfTrace::ActivePath;
		Out.Channels = BridgePerfTrace::ActiveChannels;
		Out.StartedAtUtc = BridgePerfTrace::ActiveStartedAtUtc;
	}

#if !UE_VERSION_OLDER_THAN(5, 4, 0)
	// Reconcile with the engine's own view: if our shadow says active but
	// the engine reports disconnected (e.g. user ran Trace.Stop in the
	// console), flip our flag without losing the path. If the engine reports
	// connected and our shadow is empty, we don't try to fabricate a path —
	// leave bActive=false so callers know we didn't start this one.
	const bool bEngineConnected = FTraceAuxiliary::IsConnected();
	if (Out.bActive && !bEngineConnected)
	{
		Out.bActive = false;
		FScopeLock L(&BridgePerfTrace::State_Lock);
		BridgePerfTrace::bActive = false;
	}
	else if (!Out.bActive && bEngineConnected)
	{
		// External capture: reflect the destination but keep our channels
		// empty (we don't know what they passed).
		const FString Dest = FTraceAuxiliary::GetTraceDestinationString();
		if (!Dest.IsEmpty())
		{
			Out.bActive = true;
			Out.Path = Dest;
		}
	}
#endif

	Out.CurrentSizeBytes = BridgePerfTrace::QueryFileSize(Out.Path);
	return Out;
}

namespace BridgePerfTrace
{
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
	/** EnumerateChannels callback — uses the simpler ChannelIterFunc shape
	 *  that's been stable since the API was introduced. We strip the
	 *  trailing "Channel" suffix the trace system internally appends to
	 *  every channel C++ identifier (matches what other engine code does
	 *  for user-facing display). */
	static void EnumerateChannelCallback(const ANSICHAR* Name, bool bEnabled, void* User)
	{
		auto* Sink = static_cast<TArray<FBridgeTraceChannelInfo>*>(User);
		if (!Sink || !Name)
		{
			return;
		}
		FBridgeTraceChannelInfo Row;
		FString DisplayName(ANSI_TO_TCHAR(Name));
		// "FooChannel" → "Foo". Engine convention; matches FTraceAuxiliaryImpl.
		const FString Suffix = TEXT("Channel");
		if (DisplayName.EndsWith(Suffix))
		{
			DisplayName.LeftChopInline(Suffix.Len());
		}
		// Lowercase for canonical form: trace channels are documented in
		// lowercase ("cpu", "frame", "rdg") even though the C++ symbols
		// have varied case ("CpuChannel" / "FrameChannel" / etc.).
		Row.Name = DisplayName.ToLower();
		Row.bEnabled = bEnabled;
		Row.Description.Reset(); // Simpler API doesn't expose description.
		Sink->Add(MoveTemp(Row));
	}
#endif
}

TArray<FBridgeTraceChannelInfo> UUnrealBridgePerfLibrary::ListTraceChannels()
{
	TArray<FBridgeTraceChannelInfo> Out;

#if !UE_VERSION_OLDER_THAN(5, 4, 0)
	UE::Trace::EnumerateChannels(&BridgePerfTrace::EnumerateChannelCallback, &Out);

	// Stable alpha-sorted order. Two channels with the same display name
	// (rare but possible if engine modules register duplicates) collapse
	// into the order EnumerateChannels surfaced them — we don't dedupe.
	Out.Sort([](const FBridgeTraceChannelInfo& A, const FBridgeTraceChannelInfo& B)
	{
		return A.Name < B.Name;
	});
#else
	// 5.3 has neither UE::Trace::EnumerateChannels nor a documented public
	// equivalent. Returning an empty array is honest; callers can still use
	// StartTraceCapture with their own channel name list.
#endif

	return Out;
}

// ─── M4-5 ParseTraceToSummary ───────────────────────────────────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
#include "Modules/ModuleManager.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Diagnostics.h"
#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "TraceServices/Model/Counters.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/NetProfiler.h"
#include "TraceServices/Model/CookProfilerProvider.h"
#include "TraceServices/Containers/Tables.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/StreamableRenderAsset.h"
#include "ContentStreaming.h"
#include "TextureResource.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditingLibrary.h"
#include "ProfilingDebugging/MiscTrace.h"  // ETraceFrameType

namespace BridgePerfTraceImpl
{
	/** Per-timer aggregate: total inclusive time + call count. */
	struct FAgg
	{
		double TotalMs = 0.0;
		int64  Count   = 0;
	};

	/**
	 * Resolve `Event.TimerIndex` to the canonical TimerId.
	 *
	 * `Event.TimerIndex` carries one of two encodings:
	 *  - regular id in [0..TimerCount-1] — used for plain CPU/GPU scopes
	 *  - bit-inverted metadata id (i.e. very large `uint32`, top bit set) —
	 *    used for events that carry per-instance metadata (`Foo Class=Bar`).
	 *    These need `GetOriginalTimerIdFromMetadata` to recover the real id.
	 *
	 * Without this mapping, GPU + metadata-tagged events in the aggregation
	 * fragment per-instance and resolve to `<TimerIndex N>` in the output.
	 */
	static uint32 ResolveTimerId(
		const TraceServices::ITimingProfilerProvider* TimingProv,
		uint32 RawIndex,
		uint32 TimerCount)
	{
		if (RawIndex < TimerCount)
		{
			return RawIndex;
		}
		return TimingProv->GetOriginalTimerIdFromMetadata(RawIndex);
	}

	/**
	 * Walk a single timeline and accumulate per-timer inclusive-time aggregates
	 * into `Aggregates`. Reused by the global CPU pass, the per-thread pass and
	 * the GPU pass — same EnumerateEvents shape works for all of them.
	 *
	 * `TimingProv` and `TimerCount` are needed so `ResolveTimerId` can collapse
	 * metadata-tagged events back to their canonical timer.
	 */
	static void AccumulateTimeline(
		const TraceServices::ITimingProfilerProvider* TimingProv,
		uint32 TimerCount,
		const TraceServices::ITimingProfilerProvider::Timeline& Timeline,
		TMap<uint32, FAgg>& Aggregates)
	{
		const double Start = Timeline.GetStartTime();
		const double End   = Timeline.GetEndTime();
		if (Start >= End) return;

		Timeline.EnumerateEvents(Start, End,
			[&Aggregates, TimingProv, TimerCount](double StartTime, double EndTime, uint32 /*Depth*/,
				const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
			{
				const double DurMs = (EndTime - StartTime) * 1000.0;
				if (!FMath::IsFinite(DurMs) || DurMs < 0.0)
				{
					return TraceServices::EEventEnumerate::Continue;
				}
				const uint32 ResolvedId = ResolveTimerId(TimingProv, Event.TimerIndex, TimerCount);
				FAgg& A = Aggregates.FindOrAdd(ResolvedId);
				A.TotalMs += DurMs;
				A.Count++;
				return TraceServices::EEventEnumerate::Continue;
			});
	}

	/**
	 * Convert a TimerId → FAgg map into an FBridgePerfHotScope array, ranked
	 * by TotalMs descending and capped at TopN. `TimerNames` resolves the
	 * display name; missing entries render as `<TimerIndex N>`.
	 */
	static TArray<FBridgePerfHotScope> RankAndConvert(
		const TMap<uint32, FAgg>& Aggregates,
		const TMap<uint32, FString>& TimerNames,
		int32 TopN)
	{
		TArray<TPair<uint32, FAgg>> Ranked;
		Ranked.Reserve(Aggregates.Num());
		for (const TPair<uint32, FAgg>& Pair : Aggregates)
		{
			Ranked.Emplace(Pair.Key, Pair.Value);
		}
		Ranked.Sort(
			[](const TPair<uint32, FAgg>& A, const TPair<uint32, FAgg>& B)
			{
				return A.Value.TotalMs > B.Value.TotalMs;
			});

		const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);
		const int32 Take = FMath::Min(Ranked.Num(), ClampedTopN);
		TArray<FBridgePerfHotScope> Result;
		Result.Reserve(Take);
		for (int32 i = 0; i < Take; ++i)
		{
			FBridgePerfHotScope Row;
			const FString* NamePtr = TimerNames.Find(Ranked[i].Key);
			Row.Name      = NamePtr ? *NamePtr : FString::Printf(TEXT("<TimerIndex %u>"), Ranked[i].Key);
			Row.TotalMs   = Ranked[i].Value.TotalMs;
			Row.CallCount = static_cast<int32>(Ranked[i].Value.Count);
			Result.Add(MoveTemp(Row));
		}
		return Result;
	}
}

FBridgePerfTraceSummary UUnrealBridgePerfLibrary::ParseTraceToSummary(
	const FString& UtracePath,
	int32 TopN,
	int32 TopNPerThread,
	int32 TopNCounters)
{
	FBridgePerfTraceSummary Out;
	Out.TracePath = UtracePath;
	IFileManager& FileMgr = IFileManager::Get();
	if (!FileMgr.FileExists(*UtracePath))
	{
		Out.Error = FString::Printf(TEXT("trace file not found: %s"), *UtracePath);
		return Out;
	}
	Out.FileSizeBytes = FileMgr.FileSize(*UtracePath);
	if (Out.FileSizeBytes <= 0)
	{
		Out.Error = FString::Printf(TEXT("trace file empty or unreadable: %s"), *UtracePath);
		return Out;
	}

	ITraceServicesModule* TSModule = FModuleManager::LoadModulePtr<ITraceServicesModule>("TraceServices");
	if (!TSModule)
	{
		Out.Error = TEXT("TraceServices module load failed");
		return Out;
	}
	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TSModule->GetAnalysisService();
	if (!AnalysisService)
	{
		Out.Error = TEXT("TraceServices::IAnalysisService unavailable");
		return Out;
	}

	// Synchronous Analyze — blocks until the trace stream is fully consumed.
	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*UtracePath);
	if (!Session.IsValid())
	{
		Out.Error = TEXT("Analyze() returned null session — trace probably malformed");
		return Out;
	}

	// All provider reads must happen inside a read scope. RAII helper.
	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);

	// Diagnostics provider — engine version + project metadata.
	if (const TraceServices::IDiagnosticsProvider* Diag = TraceServices::ReadDiagnosticsProvider(*Session))
	{
		if (Diag->IsSessionInfoAvailable())
		{
			const TraceServices::FSessionInfo& Info = Diag->GetSessionInfo();
			Out.Platform     = Info.Platform;
			Out.AppName      = Info.AppName;
			Out.ProjectName  = Info.ProjectName;
			Out.BuildVersion = Info.BuildVersion;
			Out.Changelist   = static_cast<int32>(Info.Changelist);
		}
	}

	// Frames provider — Game frames only. Returns a reference (never null).
	{
		const TraceServices::IFrameProvider& FrameProv = TraceServices::ReadFrameProvider(*Session);
		const uint64 GameFrameCount = FrameProv.GetFrameCount(TraceFrameType_Game);
		Out.GameFrameCount = static_cast<int32>(GameFrameCount);

		double TotalMs = 0.0;
		double MinMs   = TNumericLimits<double>::Max();
		double MaxMs   = 0.0;
		int32 ValidFrames = 0;
		FrameProv.EnumerateFrames(TraceFrameType_Game, 0, GameFrameCount,
			[&](const TraceServices::FFrame& Frame)
			{
				const double DurMs = (Frame.EndTime - Frame.StartTime) * 1000.0;
				// Skip frames whose End is infinite (open at trace stop) or
				// negative (clock drift) — they pollute avg/min/max.
				if (!FMath::IsFinite(DurMs) || DurMs < 0.0)
				{
					return;
				}
				TotalMs += DurMs;
				++ValidFrames;
				if (DurMs < MinMs) MinMs = DurMs;
				if (DurMs > MaxMs) MaxMs = DurMs;
			});
		Out.GameFrameCount = ValidFrames;
		Out.TotalDurationSeconds = TotalMs / 1000.0;
		if (ValidFrames > 0)
		{
			Out.FrameAvgMs = static_cast<float>(TotalMs / static_cast<double>(ValidFrames));
			Out.FrameMinMs = static_cast<float>(MinMs);
			Out.FrameMaxMs = static_cast<float>(MaxMs);
		}
	}

	// TimingProfiler — global CPU + GPU + per-thread hot scopes.
	if (const TraceServices::ITimingProfilerProvider* TimingProv = TraceServices::ReadTimingProfilerProvider(*Session))
	{
		using namespace BridgePerfTraceImpl;

		// Snapshot TimerId → display name. Reader's pointers are only valid
		// inside this callback, so we copy into FStrings. We capture
		// `TimerCount` so the resolver outside this scope can detect
		// bit-inverted metadata ids (TimerIndex >= TimerCount).
		TMap<uint32, FString> TimerNames;
		uint32 TimerCount = 0;
		TimingProv->ReadTimers(
			[&TimerNames, &TimerCount](const TraceServices::ITimingProfilerTimerReader& Reader)
			{
				TimerCount = Reader.GetTimerCount();
				TimerNames.Reserve(TimerCount);
				for (uint32 i = 0; i < TimerCount; ++i)
				{
					if (const TraceServices::FTimingProfilerTimer* Timer = Reader.GetTimer(i))
					{
						const TCHAR* Name = Timer->Name ? Timer->Name : TEXT("<unnamed>");
						TimerNames.Add(Timer->Id, FString(Name));
					}
				}
			});

		// ── Global CPU aggregate (every CPU timeline merged) ──
		TMap<uint32, FAgg> CpuAggregates;
		CpuAggregates.Reserve(TimerNames.Num());
		TimingProv->EnumerateTimelines(
			[&CpuAggregates, TimingProv, TimerCount](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				AccumulateTimeline(TimingProv, TimerCount, Timeline, CpuAggregates);
			});
		Out.HotScopes = RankAndConvert(CpuAggregates, TimerNames, TopN);

		// ── M5-2: per-thread aggregate ──
		// Skipped when caller passes TopNPerThread=0 — saves time + memory on
		// huge traces where only the global picture matters.
		if (TopNPerThread > 0)
		{
			const int32 ClampedTopNPerThread = FMath::Clamp(TopNPerThread, 1, 200);
			const TraceServices::IThreadProvider& ThreadProv = TraceServices::ReadThreadProvider(*Session);

			ThreadProv.EnumerateThreads(
				[&Out, &TimingProv, &TimerNames, TimerCount, ClampedTopNPerThread](const TraceServices::FThreadInfo& Thread)
				{
					uint32 TimelineIdx = ~0u;
					if (!TimingProv->GetCpuThreadTimelineIndex(Thread.Id, TimelineIdx))
					{
						return; // Thread emitted no CPU timing events.
					}

					TMap<uint32, FAgg> ThreadAgg;
					TimingProv->ReadTimeline(TimelineIdx,
						[&ThreadAgg, TimingProv, TimerCount](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
						{
							AccumulateTimeline(TimingProv, TimerCount, Timeline, ThreadAgg);
						});

					if (ThreadAgg.Num() == 0)
					{
						return;
					}

					FBridgePerThreadHotScopes Row;
					Row.ThreadName = Thread.Name ? FString(Thread.Name) : FString();
					Row.GroupName  = Thread.GroupName ? FString(Thread.GroupName) : FString();
					Row.ThreadId   = static_cast<int64>(Thread.Id);
					double Sum = 0.0;
					for (const TPair<uint32, FAgg>& P : ThreadAgg)
					{
						Sum += P.Value.TotalMs;
					}
					Row.TotalCpuMs = Sum;
					Row.TopScopes  = RankAndConvert(ThreadAgg, TimerNames, ClampedTopNPerThread);
					Out.PerThreadHotScopes.Add(MoveTemp(Row));
				});

			Out.PerThreadHotScopes.Sort(
				[](const FBridgePerThreadHotScopes& A, const FBridgePerThreadHotScopes& B)
				{
					return A.TotalCpuMs > B.TotalCpuMs;
				});
		}

		// ── M5-1: GPU hot scopes (every GPU queue merged into one ranking) ──
		if (TimingProv->HasGpuTiming())
		{
			TMap<uint32, FAgg> GpuAggregates;
			TArray<uint32> GpuTimelineIndices;
			TimingProv->EnumerateGpuQueues(
				[&GpuTimelineIndices](const TraceServices::FGpuQueueInfo& Queue)
				{
					if (Queue.TimelineIndex != ~0u)
					{
						GpuTimelineIndices.Add(Queue.TimelineIndex);
					}
					if (Queue.WorkTimelineIndex != ~0u)
					{
						GpuTimelineIndices.Add(Queue.WorkTimelineIndex);
					}
				});

			// Old-style GPU timelines (back-compat path — populated when the
			// trace came from an older runtime that didn't emit GPU queue
			// metadata yet). EnumerateGpuQueues returns empty in that case.
			uint32 LegacyGpu1 = ~0u;
			uint32 LegacyGpu2 = ~0u;
			if (TimingProv->GetGpuTimelineIndex(LegacyGpu1))
			{
				GpuTimelineIndices.AddUnique(LegacyGpu1);
			}
			if (TimingProv->GetGpu2TimelineIndex(LegacyGpu2))
			{
				GpuTimelineIndices.AddUnique(LegacyGpu2);
			}

			for (uint32 Idx : GpuTimelineIndices)
			{
				TimingProv->ReadTimeline(Idx,
					[&GpuAggregates, TimingProv, TimerCount](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						AccumulateTimeline(TimingProv, TimerCount, Timeline, GpuAggregates);
					});
			}

			Out.GpuHotScopes = RankAndConvert(GpuAggregates, TimerNames, TopN);
		}
	}

	// ── M5-3: counter aggregates ──
	// Walks every counter the trace registered, computing min/max/avg/last/sum
	// over the full session interval. Counter timelines can be huge (10k counters
	// × 100Hz × N seconds), so we never materialise the full timeline — the
	// EnumerateValues callback aggregates as it streams. Counters with 0 samples
	// are dropped. Output capped at TopNCounters by SampleCount desc.
	if (TopNCounters > 0)
	{
		const int32 ClampedTopNCounters = FMath::Clamp(TopNCounters, 1, 2000);
		const TraceServices::ICounterProvider& CounterProv = TraceServices::ReadCounterProvider(*Session);

		TArray<FBridgePerfCounter> AllCounters;
		AllCounters.Reserve(static_cast<int32>(CounterProv.GetCounterCount()));

		CounterProv.EnumerateCounters(
			[&AllCounters](uint32 CounterId, const TraceServices::ICounter& Counter)
			{
				FBridgePerfCounter Row;
				Row.Name              = Counter.GetName()        ? FString(Counter.GetName())        : FString();
				Row.Group             = Counter.GetGroup()       ? FString(Counter.GetGroup())       : FString();
				Row.Description       = Counter.GetDescription() ? FString(Counter.GetDescription()) : FString();
				Row.bFloatingPoint    = Counter.IsFloatingPoint();
				Row.bResetEveryFrame  = Counter.IsResetEveryFrame();

				int64 Count = 0;
				double Min   = TNumericLimits<double>::Max();
				double Max   = -TNumericLimits<double>::Max();
				double Sum   = 0.0;
				double Last  = 0.0;

				if (Counter.IsFloatingPoint())
				{
					Counter.EnumerateFloatValues(-DBL_MAX, DBL_MAX, /*bIncludeExternalBounds*/ false,
						[&Count, &Min, &Max, &Sum, &Last](double /*Time*/, double Value)
						{
							++Count;
							if (Value < Min) Min = Value;
							if (Value > Max) Max = Value;
							Sum  += Value;
							Last  = Value;
						});
				}
				else
				{
					Counter.EnumerateValues(-DBL_MAX, DBL_MAX, /*bIncludeExternalBounds*/ false,
						[&Count, &Min, &Max, &Sum, &Last](double /*Time*/, int64 Value)
						{
							++Count;
							const double DV = static_cast<double>(Value);
							if (DV < Min) Min = DV;
							if (DV > Max) Max = DV;
							Sum  += DV;
							Last  = DV;
						});
				}

				if (Count == 0)
				{
					return; // no samples — drop it.
				}

				Row.SampleCount  = static_cast<int32>(Count);
				Row.MinValue     = Min;
				Row.MaxValue     = Max;
				Row.SumValue     = Sum;
				Row.AverageValue = Sum / static_cast<double>(Count);
				Row.LastValue    = Last;
				AllCounters.Add(MoveTemp(Row));
			});

		AllCounters.Sort(
			[](const FBridgePerfCounter& A, const FBridgePerfCounter& B)
			{
				return A.SampleCount > B.SampleCount;
			});

		const int32 Take = FMath::Min(AllCounters.Num(), ClampedTopNCounters);
		Out.Counters.Reserve(Take);
		for (int32 i = 0; i < Take; ++i)
		{
			Out.Counters.Add(MoveTemp(AllCounters[i]));
		}
	}

	// ── M5-5: load-time breakdown by package ──
	// Empty when the trace lacks the `loadtime` channel — the provider exists
	// but its tables are empty.
	if (const TraceServices::ILoadTimeProfilerProvider* LoadProv = TraceServices::ReadLoadTimeProfilerProvider(*Session))
	{
		// Use the full-session interval. CreatePackageDetailsTable allocates
		// a heap-owned ITable that we delete after walking.
		TraceServices::ITable<TraceServices::FPackagesTableRow>* PkgTable =
			LoadProv->CreatePackageDetailsTable(-DBL_MAX, DBL_MAX);
		if (PkgTable)
		{
			TraceServices::ITableReader<TraceServices::FPackagesTableRow>* Reader = PkgTable->CreateReader();
			if (Reader)
			{
				TArray<FBridgePerfLoadTimeRow> AllRows;
				AllRows.Reserve(static_cast<int32>(PkgTable->GetRowCount()));
				while (Reader->IsValid())
				{
					if (const TraceServices::FPackagesTableRow* Row = Reader->GetCurrentRow())
					{
						FBridgePerfLoadTimeRow Out2;
						const TCHAR* Name = (Row->PackageInfo && Row->PackageInfo->Name) ? Row->PackageInfo->Name : TEXT("<unknown>");
						Out2.PackageName            = FString(Name);
						Out2.MainThreadMs           = Row->MainThreadTime * 1000.0;
						Out2.AsyncLoadingThreadMs   = Row->AsyncLoadingThreadTime * 1000.0;
						Out2.TotalLoadingMs         = Out2.MainThreadMs + Out2.AsyncLoadingThreadMs;
						Out2.SerializedSizeBytes    = static_cast<int64>(Row->TotalSerializedSize);
						Out2.HeaderSizeBytes        = static_cast<int64>(Row->SerializedHeaderSize);
						Out2.ExportsSizeBytes       = static_cast<int64>(Row->SerializedExportsSize);
						Out2.ExportCount            = static_cast<int32>(Row->SerializedExportsCount);
						AllRows.Add(MoveTemp(Out2));
					}
					Reader->NextRow();
				}
				delete Reader;

				AllRows.Sort(
					[](const FBridgePerfLoadTimeRow& A, const FBridgePerfLoadTimeRow& B)
					{
						return A.TotalLoadingMs > B.TotalLoadingMs;
					});

				const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);
				const int32 Take = FMath::Min(AllRows.Num(), ClampedTopN);
				Out.LoadTimeBreakdown.Reserve(Take);
				for (int32 i = 0; i < Take; ++i)
				{
					Out.LoadTimeBreakdown.Add(MoveTemp(AllRows[i]));
				}
			}
			delete PkgTable;
		}
	}

	Out.bSuccess = true;
	return Out;
}

// ─── M6-1 ParseAllocTraceToSummary ───────────────────────────────

FBridgePerfAllocSummary UUnrealBridgePerfLibrary::ParseAllocTraceToSummary(const FString& UtracePath)
{
	FBridgePerfAllocSummary Out;
	Out.TracePath = UtracePath;

	IFileManager& FileMgr = IFileManager::Get();
	if (!FileMgr.FileExists(*UtracePath))
	{
		Out.Error = FString::Printf(TEXT("trace file not found: %s"), *UtracePath);
		return Out;
	}
	Out.FileSizeBytes = FileMgr.FileSize(*UtracePath);
	if (Out.FileSizeBytes <= 0)
	{
		Out.Error = FString::Printf(TEXT("trace file empty or unreadable: %s"), *UtracePath);
		return Out;
	}

	ITraceServicesModule* TSModule = FModuleManager::LoadModulePtr<ITraceServicesModule>("TraceServices");
	if (!TSModule)
	{
		Out.Error = TEXT("TraceServices module load failed");
		return Out;
	}
	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TSModule->GetAnalysisService();
	if (!AnalysisService)
	{
		Out.Error = TEXT("TraceServices::IAnalysisService unavailable");
		return Out;
	}

	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*UtracePath);
	if (!Session.IsValid())
	{
		Out.Error = TEXT("Analyze() returned null session — trace probably malformed");
		return Out;
	}

	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);

	const TraceServices::IAllocationsProvider* AllocProv = TraceServices::ReadAllocationsProvider(*Session);
	if (!AllocProv)
	{
		Out.Error = TEXT("AllocationsProvider unavailable — trace lacks memalloc channel");
		Out.bSuccess = true; // call succeeded, just no data.
		return Out;
	}

	// AllocationsProvider has its own RAII read lock that runs in addition to
	// the session-level read scope; both are required.
	AllocProv->BeginRead();
	ON_SCOPE_EXIT { AllocProv->EndRead(); };

	if (!AllocProv->IsInitialized())
	{
		Out.Error = TEXT("AllocationsProvider not initialized — trace likely empty");
		Out.bSuccess = true;
		return Out;
	}

	Out.bHasEvents = AllocProv->HasAllocationEvents();
	if (!Out.bHasEvents)
	{
		Out.bSuccess = true;
		return Out;
	}

	const int32 PointCount = AllocProv->GetTimelineNumPoints();
	if (PointCount > 0)
	{
		// Walk the full timeline once to find peak total + peak live count.
		uint64 PeakBytes = 0;
		AllocProv->EnumerateTimeline(TraceServices::IAllocationsProvider::ETimelineU64::MaxTotalAllocatedMemory,
			0, PointCount - 1,
			[&PeakBytes](double /*Time*/, double /*Duration*/, uint64 Value)
			{
				if (Value > PeakBytes) PeakBytes = Value;
			});
		Out.PeakTotalAllocatedBytes = static_cast<int64>(PeakBytes);

		uint32 PeakLive = 0;
		AllocProv->EnumerateTimeline(TraceServices::IAllocationsProvider::ETimelineU32::MaxLiveAllocations,
			0, PointCount - 1,
			[&PeakLive](double /*Time*/, double /*Duration*/, uint32 Value)
			{
				if (Value > PeakLive) PeakLive = Value;
			});
		Out.PeakLiveAllocations = static_cast<int64>(PeakLive);

		// Sum the per-bin alloc / free event counts to get totals.
		uint64 SumAllocs = 0;
		AllocProv->EnumerateTimeline(TraceServices::IAllocationsProvider::ETimelineU32::AllocEvents,
			0, PointCount - 1,
			[&SumAllocs](double /*Time*/, double /*Duration*/, uint32 Value)
			{
				SumAllocs += Value;
			});
		Out.TotalAllocEvents = static_cast<int64>(SumAllocs);

		uint64 SumFrees = 0;
		AllocProv->EnumerateTimeline(TraceServices::IAllocationsProvider::ETimelineU32::FreeEvents,
			0, PointCount - 1,
			[&SumFrees](double /*Time*/, double /*Duration*/, uint32 Value)
			{
				SumFrees += Value;
			});
		Out.TotalFreeEvents = static_cast<int64>(SumFrees);

		Out.AllocFreeDelta = Out.TotalAllocEvents - Out.TotalFreeEvents;
	}

	// Tag inventory.
	AllocProv->EnumerateTags(
		[&Out](const TCHAR* Name, const TCHAR* /*FullPath_unused*/, TraceServices::TagIdType Id, TraceServices::TagIdType ParentId)
		{
			FBridgePerfAllocTag Row;
			Row.Id        = static_cast<int32>(Id);
			Row.ParentId  = ParentId == ~uint32(0) ? -1 : static_cast<int32>(ParentId);
			Row.Name      = Name ? FString(Name) : FString();
			Out.Tags.Add(MoveTemp(Row));
		});
	// Backfill FullPath via GetTagFullPath after the enumerate (tag list is now stable).
	for (FBridgePerfAllocTag& T : Out.Tags)
	{
		if (const TCHAR* Full = AllocProv->GetTagFullPath(static_cast<TraceServices::TagIdType>(T.Id)))
		{
			T.FullPath = FString(Full);
		}
	}

	Out.bSuccess = true;
	return Out;
}

// ─── M6-2 ParseNetTraceToSummary ─────────────────────────────────

FBridgePerfNetSummary UUnrealBridgePerfLibrary::ParseNetTraceToSummary(const FString& UtracePath)
{
	FBridgePerfNetSummary Out;
	Out.TracePath = UtracePath;

	IFileManager& FileMgr = IFileManager::Get();
	if (!FileMgr.FileExists(*UtracePath))
	{
		Out.Error = FString::Printf(TEXT("trace file not found: %s"), *UtracePath);
		return Out;
	}
	Out.FileSizeBytes = FileMgr.FileSize(*UtracePath);
	if (Out.FileSizeBytes <= 0)
	{
		Out.Error = FString::Printf(TEXT("trace file empty or unreadable: %s"), *UtracePath);
		return Out;
	}

	ITraceServicesModule* TSModule = FModuleManager::LoadModulePtr<ITraceServicesModule>("TraceServices");
	if (!TSModule)
	{
		Out.Error = TEXT("TraceServices module load failed");
		return Out;
	}
	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TSModule->GetAnalysisService();
	if (!AnalysisService)
	{
		Out.Error = TEXT("TraceServices::IAnalysisService unavailable");
		return Out;
	}
	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*UtracePath);
	if (!Session.IsValid())
	{
		Out.Error = TEXT("Analyze() returned null session — trace probably malformed");
		return Out;
	}
	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);

	const TraceServices::INetProfilerProvider* NetProv = TraceServices::ReadNetProfilerProvider(*Session);
	if (!NetProv)
	{
		Out.Error = TEXT("NetProfilerProvider unavailable — engine module not loaded");
		Out.bSuccess = true;
		return Out;
	}

	Out.NetTraceVersion = static_cast<int32>(NetProv->GetNetTraceVersion());
	if (Out.NetTraceVersion == 0)
	{
		// Trace lacks the `net` channel — provider is loaded but empty.
		Out.bSuccess = true;
		return Out;
	}

	const uint32 InstCount = NetProv->GetGameInstanceCount();
	if (InstCount == 0)
	{
		Out.bSuccess = true;
		return Out;
	}

	Out.bHasEvents = true;
	Out.GameInstances.Reserve(InstCount);

	NetProv->ReadGameInstances(
		[NetProv, &Out](const TraceServices::FNetProfilerGameInstance& Instance)
		{
			FBridgePerfNetGameInstance Row;
			Row.InstanceName             = Instance.InstanceName ? FString(Instance.InstanceName) : FString();
			Row.bIsServer                = Instance.bIsServer;
			Row.bIsUsingIrisReplication  = Instance.bIsUsingIrisReplication;
			Row.ObjectCount              = static_cast<int32>(NetProv->GetObjectCount(Instance.GameInstanceIndex));

			const uint32 ConnCount = NetProv->GetConnectionCount(Instance.GameInstanceIndex);
			Row.Connections.Reserve(ConnCount);

			NetProv->ReadConnections(Instance.GameInstanceIndex,
				[NetProv, &Row](const TraceServices::FNetProfilerConnection& Conn)
				{
					FBridgePerfNetConnection ConnRow;
					ConnRow.Name           = Conn.Name          ? FString(Conn.Name)          : FString();
					ConnRow.AddressString  = Conn.AddressString ? FString(Conn.AddressString) : FString();
					ConnRow.ConnectionId   = static_cast<int32>(Conn.ConnectionId);

					// Outgoing direction
					{
						const uint32 PktCount = NetProv->GetPacketCount(Conn.ConnectionIndex,
							TraceServices::ENetProfilerConnectionMode::Outgoing);
						ConnRow.OutgoingPacketCount = PktCount;
						uint64 ByteSum = 0;
						if (PktCount > 0)
						{
							NetProv->EnumeratePackets(Conn.ConnectionIndex,
								TraceServices::ENetProfilerConnectionMode::Outgoing,
								0, PktCount - 1,
								[&ByteSum](const TraceServices::FNetProfilerPacket& P)
								{
									ByteSum += P.TotalPacketSizeInBytes;
								});
						}
						ConnRow.OutgoingBytes = static_cast<int64>(ByteSum);
					}

					// Incoming direction
					{
						const uint32 PktCount = NetProv->GetPacketCount(Conn.ConnectionIndex,
							TraceServices::ENetProfilerConnectionMode::Incoming);
						ConnRow.IncomingPacketCount = PktCount;
						uint64 ByteSum = 0;
						if (PktCount > 0)
						{
							NetProv->EnumeratePackets(Conn.ConnectionIndex,
								TraceServices::ENetProfilerConnectionMode::Incoming,
								0, PktCount - 1,
								[&ByteSum](const TraceServices::FNetProfilerPacket& P)
								{
									ByteSum += P.TotalPacketSizeInBytes;
								});
						}
						ConnRow.IncomingBytes = static_cast<int64>(ByteSum);
					}

					Row.Connections.Add(MoveTemp(ConnRow));
				});

			Out.GameInstances.Add(MoveTemp(Row));
		});

	Out.bSuccess = true;
	return Out;
}

// ─── M7-1 GetTextureStreamingResidency ───────────────────────────

FBridgeTextureStreamingState UUnrealBridgePerfLibrary::GetTextureStreamingResidency(int32 TopN)
{
	FBridgeTextureStreamingState Out;
	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);

	// IStreamingManager::Get() returns the FStreamingManagerCollection singleton
	// which is the concrete owner of texture-streaming accessors; the base
	// IStreamingManager interface doesn't expose them.
	FStreamingManagerCollection& Mgr = IStreamingManager::Get();
	Out.bEnabled = Mgr.IsTextureStreamingEnabled();

	IRenderAssetStreamingManager& RAS = Mgr.GetRenderAssetStreamingManager();
	Out.PoolSizeBytes          = RAS.GetPoolSize();
	Out.RequiredPoolBytes      = RAS.GetRequiredPoolSize();
	Out.MemoryOverBudgetBytes  = RAS.GetMemoryOverBudget();
	Out.MaxEverRequiredBytes   = RAS.GetMaxEverRequired();

	const double NowSeconds = FApp::GetCurrentTime();

	TArray<FBridgeTextureStreamingRow> AllRows;
	AllRows.Reserve(2048);

	for (TObjectIterator<UTexture2D> It; It; ++It)
	{
		UTexture2D* Tex = *It;
		if (!Tex || !Tex->IsStreamable())
		{
			continue;
		}
		++Out.NumStreamingTextures;

		const FStreamableRenderResourceState& S = Tex->GetStreamableResourceState();
		if (S.MaxNumLODs == 0)
		{
			continue;
		}

		FBridgeTextureStreamingRow Row;
		Row.TexturePath       = Tex->GetPathName();
		Row.ResidentMipCount  = S.NumResidentLODs;
		Row.WantedMipCount    = S.NumRequestedLODs;
		Row.MaxMipCount       = S.MaxNumLODs;
		// `UTexture2D::CalcTextureMemorySize` is unexported in UE 5.7 — calling
		// CalcCumulativeLODSize from an external module fails to link. Walk the
		// PlatformData mip array directly: cumulative mip bytes from the
		// smallest-up to NumResidentLODs / NumRequestedLODs.
		int64 ResidentBytes = 0;
		int64 WantedBytes   = 0;
		if (const FTexturePlatformData* PD = Tex->GetPlatformData())
		{
			const int32 NumMips = PD->Mips.Num();
			// Resident mips are indices [NumMips - NumResidentLODs, NumMips - 1].
			const int32 ResidentFirst = FMath::Max(0, NumMips - S.NumResidentLODs);
			const int32 WantedFirst   = FMath::Max(0, NumMips - S.NumRequestedLODs);
			for (int32 i = ResidentFirst; i < NumMips; ++i)
			{
				ResidentBytes += PD->Mips[i].BulkData.GetBulkDataSize();
			}
			for (int32 i = WantedFirst; i < NumMips; ++i)
			{
				WantedBytes += PD->Mips[i].BulkData.GetBulkDataSize();
			}
		}
		// Fallback: if PlatformData mip walk produced 0 (cooked/streaming texture
		// whose serialized size landed elsewhere), use the UObject resource size.
		if (ResidentBytes == 0)
		{
			ResidentBytes = static_cast<int64>(Tex->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
			WantedBytes   = ResidentBytes;
		}
		Row.ResidentBytes     = ResidentBytes;
		Row.WantedBytes       = WantedBytes;
		const float LastRender = Tex->GetLastRenderTimeForStreaming();
		Row.LastVisibleSeconds = (LastRender == FLT_MAX)
			? FLT_MAX
			: static_cast<float>(NowSeconds - static_cast<double>(LastRender));
		Row.bForceResident    = Tex->ShouldMipLevelsBeForcedResident();
		AllRows.Add(MoveTemp(Row));
	}

	AllRows.Sort(
		[](const FBridgeTextureStreamingRow& A, const FBridgeTextureStreamingRow& B)
		{
			return A.ResidentBytes > B.ResidentBytes;
		});

	const int32 Take = FMath::Min(AllRows.Num(), ClampedTopN);
	Out.Rows.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		Out.Rows.Add(MoveTemp(AllRows[i]));
	}

	return Out;
}

// ─── M7-2 GetRenderTargetMemory ──────────────────────────────────

FBridgeRenderTargetMemory UUnrealBridgePerfLibrary::GetRenderTargetMemory(int32 TopN)
{
	FBridgeRenderTargetMemory Out;
	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);

	TArray<FBridgeRenderTargetEntry> AllEntries;
	AllEntries.Reserve(256);

	// TObjectIterator<UTextureRenderTarget> returns nothing when the base is
	// abstract on UE 5.7; walk UTexture instead and filter via IsA. UTexture
	// is the closest concrete-enough ancestor that's still narrow.
	for (TObjectIterator<UTexture> It; It; ++It)
	{
		UTextureRenderTarget* RT = Cast<UTextureRenderTarget>(*It);
		if (!RT) continue;

		FBridgeRenderTargetEntry E;
		E.Path     = RT->GetPathName();
		E.TypeName = RT->GetClass()->GetName();
		E.Bytes    = static_cast<int64>(RT->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));

		if (UTextureRenderTarget2D* RT2D = Cast<UTextureRenderTarget2D>(RT))
		{
			E.Width  = RT2D->SizeX;
			E.Height = RT2D->SizeY;
			E.Depth  = 1;
			Out.RenderTarget2DBytes += E.Bytes;
		}
		else if (UTextureRenderTargetCube* RTCube = Cast<UTextureRenderTargetCube>(RT))
		{
			E.Width  = RTCube->SizeX;
			E.Height = RTCube->SizeX;
			E.Depth  = 6;
			Out.RenderTargetCubeBytes += E.Bytes;
		}
		else if (UTextureRenderTarget2DArray* RTArr = Cast<UTextureRenderTarget2DArray>(RT))
		{
			E.Width  = RTArr->SizeX;
			E.Height = RTArr->SizeY;
			E.Depth  = RTArr->Slices;
			Out.RenderTarget2DArrayBytes += E.Bytes;
		}
		else if (UTextureRenderTargetVolume* RTVol = Cast<UTextureRenderTargetVolume>(RT))
		{
			E.Width  = RTVol->SizeX;
			E.Height = RTVol->SizeY;
			E.Depth  = RTVol->SizeZ;
			Out.RenderTargetVolumeBytes += E.Bytes;
		}
		else
		{
			// Unrecognized RT subclass — still count its bytes in TotalBytes
			// but leave dimensions as 0.
		}

		Out.TotalBytes += E.Bytes;
		++Out.RenderTargetCount;
		AllEntries.Add(MoveTemp(E));
	}

	AllEntries.Sort(
		[](const FBridgeRenderTargetEntry& A, const FBridgeRenderTargetEntry& B)
		{
			return A.Bytes > B.Bytes;
		});

	const int32 Take = FMath::Min(AllEntries.Num(), ClampedTopN);
	Out.Entries.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		Out.Entries.Add(MoveTemp(AllEntries[i]));
	}

	return Out;
}

// ─── M7-3 GetPerPassGpuTimings ───────────────────────────────────

FBridgeGpuPassTimings UUnrealBridgePerfLibrary::GetPerPassGpuTimings()
{
	FBridgeGpuPassTimings Out;

#if HAS_GPU_STATS && (RHI_NEW_GPU_PROFILER == 0) && GPUPROFILERTRACE_ENABLED
	if (!AreGPUStatsEnabled())
	{
		Out.Diagnostic = TEXT("GPU stats are disabled — set r.GPUStatsEnabled=1");
		return Out;
	}

	FRealtimeGPUProfiler* Profiler = FRealtimeGPUProfiler::Get();
	if (!Profiler)
	{
		Out.Diagnostic = TEXT("FRealtimeGPUProfiler::Get() returned null");
		return Out;
	}

	TArray<FRealtimeGPUProfilerDescriptionResult> Results;
	Profiler->FetchPerfByDescription(Results);

	if (Results.Num() == 0)
	{
		// History buffer empty (profiler hasn't ticked or no stats registered yet).
		Out.Diagnostic = TEXT("FRealtimeGPUProfiler returned empty history — render at least a few frames first");
		Out.bAvailable = true;
		return Out;
	}

	Out.bAvailable = true;
	Out.PassCount  = Results.Num();
	Out.Passes.Reserve(Results.Num());
	double Sum = 0.0;

	for (const FRealtimeGPUProfilerDescriptionResult& R : Results)
	{
		FBridgeGpuPassTiming Row;
		Row.PassName  = R.Description;
		Row.GpuIndex  = static_cast<int32>(R.GPUMask.GetFirstIndex());
		// FRealtimeGPUProfilerDescriptionResult times are in microseconds.
		Row.AverageMs = static_cast<double>(R.AverageTime) / 1000.0;
		Row.MinMs     = static_cast<double>(R.MinTime)     / 1000.0;
		Row.MaxMs     = static_cast<double>(R.MaxTime)     / 1000.0;
		Sum          += Row.AverageMs;
		Out.Passes.Add(MoveTemp(Row));
	}

	Out.SumAverageMs = Sum;
	Out.Passes.Sort(
		[](const FBridgeGpuPassTiming& A, const FBridgeGpuPassTiming& B)
		{
			return A.AverageMs > B.AverageMs;
		});
#else
	Out.bAvailable = false;
	Out.Diagnostic = TEXT("RHI_NEW_GPU_PROFILER active or HAS_GPU_STATS off — use Insights with gpu+rdg channels");
#endif

	return Out;
}

// ─── M8-3 BeginInsightsForTrace ──────────────────────────────────

FBridgeInsightsLaunchResult UUnrealBridgePerfLibrary::BeginInsightsForTrace(const FString& UtracePath)
{
	FBridgeInsightsLaunchResult Out;
	Out.TracePath = UtracePath;

	IFileManager& FileMgr = IFileManager::Get();
	if (!FileMgr.FileExists(*UtracePath))
	{
		Out.Error = FString::Printf(TEXT("trace file not found: %s"), *UtracePath);
		return Out;
	}

	// Resolve UnrealInsights.exe under <EngineRoot>/Engine/Binaries/Win64/.
	const FString InsightsExe = FPaths::Combine(
		FPaths::EngineDir(), TEXT("Binaries"), TEXT("Win64"), TEXT("UnrealInsights.exe"));
	if (!FileMgr.FileExists(*InsightsExe))
	{
		Out.Error = FString::Printf(TEXT("UnrealInsights.exe not found at %s"), *InsightsExe);
		return Out;
	}
	Out.InsightsExePath = InsightsExe;

	// Args: -OpenTraceFile="<path>". Quote the path (may contain spaces).
	const FString Args = FString::Printf(TEXT("-OpenTraceFile=\"%s\""), *UtracePath);

	// Launch detached; we don't want to wait or capture stdout. bLaunchHidden=false
	// so the GUI is visible. CreateProc returns a handle even on failure — check
	// IsValid before pulling the PID.
	uint32 ProcessId = 0;
	FProcHandle Handle = FPlatformProcess::CreateProc(
		*InsightsExe,
		*Args,
		/*bLaunchDetached=*/ true,
		/*bLaunchHidden=*/ false,
		/*bLaunchReallyHidden=*/ false,
		&ProcessId,
		/*PriorityModifier=*/ 0,
		/*OptionalWorkingDirectory=*/ nullptr,
		/*PipeWriteChild=*/ nullptr);

	if (!Handle.IsValid())
	{
		Out.Error = TEXT("CreateProc returned invalid handle — check Windows event log for launch failure");
		return Out;
	}

	// Detached process: we don't wait. Close the handle so it doesn't leak;
	// the actual process keeps running.
	FPlatformProcess::CloseProc(Handle);

	Out.bSuccess = true;
	Out.ProcessId = static_cast<int64>(ProcessId);
	return Out;
}

// ─── M8-2 ComparePerfSnapshots ───────────────────────────────────

FBridgePerfSnapshotDelta UUnrealBridgePerfLibrary::ComparePerfSnapshots(
	const FBridgePerfSnapshot& Before,
	const FBridgePerfSnapshot& After,
	float RegressionThreshold)
{
	FBridgePerfSnapshotDelta Out;
	Out.BeforeTimeUtc = Before.CaptureTimeUtc;
	Out.AfterTimeUtc  = After.CaptureTimeUtc;

	// Numeric deltas (After - Before).
	Out.DeltaFps              = After.Timing.Fps              - Before.Timing.Fps;
	Out.DeltaFrameMs          = After.Timing.FrameMs          - Before.Timing.FrameMs;
	Out.DeltaGameThreadMs     = After.Timing.GameThreadMs     - Before.Timing.GameThreadMs;
	Out.DeltaRenderThreadMs   = After.Timing.RenderThreadMs   - Before.Timing.RenderThreadMs;
	Out.DeltaGpuMs            = After.Timing.GpuMs            - Before.Timing.GpuMs;
	Out.DeltaDrawCalls        = After.Render.DrawCalls        - Before.Render.DrawCalls;
	Out.DeltaPrimitivesDrawn  = After.Render.PrimitivesDrawn  - Before.Render.PrimitivesDrawn;
	Out.DeltaUsedPhysicalMb   = After.Memory.UsedPhysicalMb   - Before.Memory.UsedPhysicalMb;
	Out.DeltaUsedVirtualMb    = After.Memory.UsedVirtualMb    - Before.Memory.UsedVirtualMb;
	Out.DeltaPeakUsedPhysicalMb = After.Memory.PeakUsedPhysicalMb - Before.Memory.PeakUsedPhysicalMb;
	Out.DeltaAvailablePhysicalMb = After.Memory.AvailablePhysicalMb - Before.Memory.AvailablePhysicalMb;
	Out.DeltaTotalObjects     = After.UObjects.TotalObjects   - Before.UObjects.TotalObjects;
	Out.DeltaUniqueClasses    = After.UObjects.UniqueClasses  - Before.UObjects.UniqueClasses;

	// Lambda for percent-regression detection on a "higher is worse" metric:
	// e.g. frame_ms got worse if it grew by ≥ threshold * before.
	auto FlagWorse = [&Out, RegressionThreshold](const TCHAR* MetricName, double BeforeVal, double AfterVal, const TCHAR* Unit, bool bHigherIsWorse)
	{
		if (BeforeVal <= 0.0)
		{
			return; // can't compute % from 0 baseline; skip.
		}
		const double Pct = (AfterVal - BeforeVal) / BeforeVal;
		const double SignedPctForWorse = bHigherIsWorse ? Pct : -Pct;
		if (SignedPctForWorse >= static_cast<double>(RegressionThreshold))
		{
			Out.bSignificantRegression = true;
			Out.Regressions.Add(FString::Printf(TEXT("%s %+.1f%% (%.2f → %.2f %s)"),
				MetricName,
				Pct * 100.0,
				BeforeVal,
				AfterVal,
				Unit));
		}
	};

	FlagWorse(TEXT("frame_ms"),         Before.Timing.FrameMs,        After.Timing.FrameMs,        TEXT("ms"), /*higherIsWorse=*/true);
	FlagWorse(TEXT("game_thread_ms"),   Before.Timing.GameThreadMs,   After.Timing.GameThreadMs,   TEXT("ms"), true);
	FlagWorse(TEXT("render_thread_ms"), Before.Timing.RenderThreadMs, After.Timing.RenderThreadMs, TEXT("ms"), true);
	FlagWorse(TEXT("gpu_ms"),           Before.Timing.GpuMs,          After.Timing.GpuMs,          TEXT("ms"), true);
	FlagWorse(TEXT("fps"),              Before.Timing.Fps,            After.Timing.Fps,            TEXT(""),   /*higherIsWorse=*/false);
	FlagWorse(TEXT("draw_calls"),       Before.Render.DrawCalls,      After.Render.DrawCalls,      TEXT(""),   true);
	FlagWorse(TEXT("primitives"),       Before.Render.PrimitivesDrawn,After.Render.PrimitivesDrawn,TEXT(""),   true);
	FlagWorse(TEXT("used_physical_mb"), Before.Memory.UsedPhysicalMb, After.Memory.UsedPhysicalMb, TEXT("MiB"),true);
	FlagWorse(TEXT("used_virtual_mb"),  Before.Memory.UsedVirtualMb,  After.Memory.UsedVirtualMb,  TEXT("MiB"),true);
	FlagWorse(TEXT("uobject_count"),    Before.UObjects.TotalObjects, After.UObjects.TotalObjects, TEXT(""),   true);

	return Out;
}

// ─── M6-3 ParseCookTraceToSummary ────────────────────────────────

FBridgePerfCookSummary UUnrealBridgePerfLibrary::ParseCookTraceToSummary(const FString& UtracePath, int32 TopN)
{
	FBridgePerfCookSummary Out;
	Out.TracePath = UtracePath;

	IFileManager& FileMgr = IFileManager::Get();
	if (!FileMgr.FileExists(*UtracePath))
	{
		Out.Error = FString::Printf(TEXT("trace file not found: %s"), *UtracePath);
		return Out;
	}
	Out.FileSizeBytes = FileMgr.FileSize(*UtracePath);
	if (Out.FileSizeBytes <= 0)
	{
		Out.Error = FString::Printf(TEXT("trace file empty or unreadable: %s"), *UtracePath);
		return Out;
	}

	ITraceServicesModule* TSModule = FModuleManager::LoadModulePtr<ITraceServicesModule>("TraceServices");
	if (!TSModule)
	{
		Out.Error = TEXT("TraceServices module load failed");
		return Out;
	}
	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TSModule->GetAnalysisService();
	if (!AnalysisService)
	{
		Out.Error = TEXT("TraceServices::IAnalysisService unavailable");
		return Out;
	}
	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*UtracePath);
	if (!Session.IsValid())
	{
		Out.Error = TEXT("Analyze() returned null session — trace probably malformed");
		return Out;
	}
	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);

	const TraceServices::ICookProfilerProvider* CookProv = TraceServices::ReadCookProfilerProvider(*Session);
	if (!CookProv)
	{
		Out.Error = TEXT("CookProfilerProvider unavailable — trace lacks cook channel");
		Out.bSuccess = true;
		return Out;
	}

	CookProv->BeginRead();
	ON_SCOPE_EXIT { CookProv->EndRead(); };

	Out.PackageCount = static_cast<int32>(CookProv->GetNumPackages());
	if (Out.PackageCount == 0)
	{
		Out.bSuccess = true;
		return Out;
	}

	Out.bHasEvents = true;

	TArray64<TraceServices::FPackageData> Aggregation;
	CookProv->CreateAggregation(Aggregation);

	TArray<FBridgePerfCookRow> Rows;
	Rows.Reserve(static_cast<int32>(Aggregation.Num()));
	double GrandTotalMs = 0.0;
	for (const TraceServices::FPackageData& P : Aggregation)
	{
		FBridgePerfCookRow R;
		R.PackageName = P.Name        ? FString(P.Name)        : FString();
		R.AssetClass  = P.AssetClass  ? FString(P.AssetClass)  : FString();
		R.LoadTimeMs                              = P.LoadTimeIncl                              * 1000.0;
		R.SaveTimeMs                              = P.SaveTimeIncl                              * 1000.0;
		R.BeginCacheCookedPlatformDataMs          = P.BeginCacheForCookedPlatformDataIncl       * 1000.0;
		R.IsCachedCookedPlatformDataLoadedMs      = P.IsCachedCookedPlatformDataLoadedIncl      * 1000.0;
		R.TotalCookTimeMs = R.LoadTimeMs + R.SaveTimeMs + R.BeginCacheCookedPlatformDataMs + R.IsCachedCookedPlatformDataLoadedMs;
		GrandTotalMs += R.TotalCookTimeMs;
		Rows.Add(MoveTemp(R));
	}

	Out.TotalCookTimeSeconds = GrandTotalMs / 1000.0;

	Rows.Sort(
		[](const FBridgePerfCookRow& A, const FBridgePerfCookRow& B)
		{
			return A.TotalCookTimeMs > B.TotalCookTimeMs;
		});

	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);
	const int32 Take = FMath::Min(Rows.Num(), ClampedTopN);
	Out.Packages.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		Out.Packages.Add(MoveTemp(Rows[i]));
	}

	Out.bSuccess = true;
	return Out;
}

// ─── M7-4 AnalyzeAllMaterials ─────────────────────────────────────

FBridgeAllMaterialsAnalysis UUnrealBridgePerfLibrary::AnalyzeAllMaterials(int32 TopN)
{
	FBridgeAllMaterialsAnalysis Out;
	const int32 ClampedTopN = FMath::Clamp(TopN, 1, 1000);

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetData> MaterialAssets;
	AR.GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), MaterialAssets, /*bSearchSubClasses*/ true);

	TArray<FAssetData> MaterialInstanceAssets;
	AR.GetAssetsByClass(UMaterialInstance::StaticClass()->GetClassPathName(), MaterialInstanceAssets, /*bSearchSubClasses*/ true);
	Out.TotalMaterialInstances = MaterialInstanceAssets.Num();

	TArray<FBridgeMaterialPerfRow> AllRows;
	AllRows.Reserve(MaterialAssets.Num());

	for (const FAssetData& AD : MaterialAssets)
	{
		// Skip subclasses with their own type (we want UMaterial only — material
		// instances are already excluded by the StaticClass GetClassPathName check
		// since UMaterialInstance doesn't inherit from UMaterial; but other
		// engine-derived UMaterial subclasses may exist).
		UMaterial* Mat = Cast<UMaterial>(AD.GetAsset());
		if (!Mat) continue;
		++Out.TotalMaterials;

		FBridgeMaterialPerfRow Row;
		Row.MaterialPath          = Mat->GetPathName();
		Row.bTwoSided             = Mat->TwoSided != 0;
		Row.bUsedWithSkeletalMesh = Mat->bUsedWithSkeletalMesh != 0;
		Row.bUsedWithStaticLighting = Mat->bUsedWithStaticLighting != 0;

		if (const UEnum* BlendEnum = StaticEnum<EBlendMode>())
		{
			Row.BlendMode = BlendEnum->GetNameStringByValue(static_cast<int64>(Mat->BlendMode));
		}
		if (const UEnum* DomainEnum = StaticEnum<EMaterialDomain>())
		{
			Row.MaterialDomain = DomainEnum->GetNameStringByValue(static_cast<int64>(Mat->MaterialDomain));
		}
		// ShadingModels is a bitfield in 5.7; we surface the first set bit name.
		if (const UEnum* SMEnum = StaticEnum<EMaterialShadingModel>())
		{
			const FMaterialShadingModelField Models = Mat->GetShadingModels();
			for (int32 i = 0; i < MSM_NUM; ++i)
			{
				if (Models.HasShadingModel(static_cast<EMaterialShadingModel>(i)))
				{
					Row.ShadingModel = SMEnum->GetNameStringByValue(static_cast<int64>(i));
					break;
				}
			}
		}

		// Walk every expression — use class-name string match to avoid extra
		// header dependencies (TextureSample / Custom / StaticSwitch each
		// lives in its own header). Counts include parameter variants
		// (e.g. TextureSampleParameter2D matches "TextureSample").
		for (TObjectPtr<UMaterialExpression> ExprPtr : Mat->GetExpressions())
		{
			UMaterialExpression* Expr = ExprPtr.Get();
			if (!Expr) continue;
			++Row.ExpressionCount;

			const FString CN = Expr->GetClass()->GetName();
			if (CN.Contains(TEXT("TextureSample")))
			{
				++Row.TextureSampleCount;
				if (CN.Contains(TEXT("Parameter")))
				{
					++Row.TextureParameterCount;
				}
			}
			else if (CN.Contains(TEXT("Custom")) && !CN.Contains(TEXT("CustomOutput")))
			{
				++Row.CustomExpressionCount;
			}
			else if (CN.Contains(TEXT("StaticSwitch")))
			{
				++Row.StaticSwitchCount;
			}
			else if (CN == TEXT("MaterialExpressionScalarParameter"))
			{
				++Row.ScalarParameterCount;
			}
			else if (CN == TEXT("MaterialExpressionVectorParameter"))
			{
				++Row.VectorParameterCount;
			}
		}

		Row.ComplexityScore = Row.ExpressionCount
			+ 4 * Row.TextureSampleCount
			+ 8 * Row.CustomExpressionCount;

		AllRows.Add(MoveTemp(Row));
	}

	AllRows.Sort(
		[](const FBridgeMaterialPerfRow& A, const FBridgeMaterialPerfRow& B)
		{
			return A.ComplexityScore > B.ComplexityScore;
		});

	const int32 Take = FMath::Min(AllRows.Num(), ClampedTopN);
	Out.Rows.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		Out.Rows.Add(MoveTemp(AllRows[i]));
	}

	return Out;
}
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0) — pre-5.7 path lives in UnrealBridgePerfLibrary_Stubs.cpp

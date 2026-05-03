#include "UnrealBridgePoseSearchLibrary.h"

#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchNormalizationSet.h"
#include "PoseSearch/PoseSearchIndex.h"

#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#endif

#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Animation/MirrorDataTable.h"
#include "Engine/SkeletalMesh.h"
#include "BoneContainer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UnrealBridgePoseSearch"

namespace BridgePoseSearchImpl
{
	UPoseSearchSchema* LoadSchema(const FString& Path)
	{
		UPoseSearchSchema* PSS = LoadObject<UPoseSearchSchema>(nullptr, *Path);
		if (!PSS)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load PoseSearchSchema '%s'"), *Path);
		return PSS;
	}

	UPoseSearchDatabase* LoadDatabase(const FString& Path)
	{
		UPoseSearchDatabase* PSD = LoadObject<UPoseSearchDatabase>(nullptr, *Path);
		if (!PSD)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load PoseSearchDatabase '%s'"), *Path);
		return PSD;
	}

	UObject* LoadAnyAnim(const FString& Path)
	{
		UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
		if (!Asset)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load anim asset '%s'"), *Path);
		return Asset;
	}

	bool ParseMirrorOption(const FString& In, EPoseSearchMirrorOption& OutOption)
	{
		const FString S = In.ToLower();
		if (S == TEXT("unmirroredonly") || S == TEXT("original") || S.IsEmpty())
		{
			OutOption = EPoseSearchMirrorOption::UnmirroredOnly; return true;
		}
		if (S == TEXT("mirroredonly") || S == TEXT("mirror"))
		{
			OutOption = EPoseSearchMirrorOption::MirroredOnly; return true;
		}
		if (S == TEXT("unmirroredandmirrored") || S == TEXT("both"))
		{
			OutOption = EPoseSearchMirrorOption::UnmirroredAndMirrored; return true;
		}
		return false;
	}

	FString MirrorOptionToString(EPoseSearchMirrorOption Opt)
	{
		switch (Opt)
		{
		case EPoseSearchMirrorOption::UnmirroredOnly:        return TEXT("UnmirroredOnly");
		case EPoseSearchMirrorOption::MirroredOnly:          return TEXT("MirroredOnly");
		case EPoseSearchMirrorOption::UnmirroredAndMirrored: return TEXT("UnmirroredAndMirrored");
		}
		return TEXT("UnmirroredOnly");
	}

	FString PoseSearchModeToString(EPoseSearchMode Mode)
	{
		switch (Mode)
		{
		case EPoseSearchMode::BruteForce: return TEXT("BruteForce");
		case EPoseSearchMode::PCAKDTree:  return TEXT("PCAKDTree");
		case EPoseSearchMode::VPTree:     return TEXT("VPTree");
		case EPoseSearchMode::EventOnly:  return TEXT("EventOnly");
		}
		return TEXT("Unknown");
	}

	FString PreprocessorToString(EPoseSearchDataPreprocessor P)
	{
		switch (P)
		{
		case EPoseSearchDataPreprocessor::None:                      return TEXT("None");
		case EPoseSearchDataPreprocessor::Normalize:                 return TEXT("Normalize");
		case EPoseSearchDataPreprocessor::NormalizeOnlyByDeviation:  return TEXT("NormalizeOnlyByDeviation");
		case EPoseSearchDataPreprocessor::NormalizeWithCommonSchema: return TEXT("NormalizeWithCommonSchema");
		}
		return TEXT("Unknown");
	}

	FString GetAssetPathSafe(const UObject* Obj)
	{
		return Obj ? Obj->GetPathName() : FString();
	}

	// Read a float UPROPERTY by name on an arbitrary UObject; returns Default if missing.
	float ReadFloatProperty(const UObject* Obj, FName PropName, float Default = 0.f)
	{
		if (!Obj) return Default;
		const FFloatProperty* Prop = CastField<FFloatProperty>(Obj->GetClass()->FindPropertyByName(PropName));
		if (!Prop) return Default;
		return Prop->GetPropertyValue_InContainer(Obj);
	}

	FString ReadBoneNameProperty(const UObject* Obj, FName PropName)
	{
		if (!Obj) return FString();
		const FStructProperty* Prop = CastField<FStructProperty>(Obj->GetClass()->FindPropertyByName(PropName));
		if (!Prop || Prop->Struct != TBaseStructure<FBoneReference>::Get())
		{
			// Some bone-reference UPROPERTYs use a custom struct; try by name match.
			Prop = CastField<FStructProperty>(Obj->GetClass()->FindPropertyByName(PropName));
			if (!Prop) return FString();
		}
		const FBoneReference* Bone = Prop->ContainerPtrToValuePtr<FBoneReference>(Obj);
		return Bone ? Bone->BoneName.ToString() : FString();
	}

#if WITH_EDITOR
	UE::PoseSearch::EAsyncBuildIndexResult ProbeIndexState(const UPoseSearchDatabase* PSD)
	{
		using namespace UE::PoseSearch;
		// ContinueRequest = "make sure there's data; don't kick a fresh rebuild"
		return FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::ContinueRequest);
	}

	FString IndexStatusToString(UE::PoseSearch::EAsyncBuildIndexResult R)
	{
		using namespace UE::PoseSearch;
		switch (R)
		{
		case EAsyncBuildIndexResult::InProgress: return TEXT("Indexing");
		case EAsyncBuildIndexResult::Success:    return TEXT("Indexed");
		case EAsyncBuildIndexResult::Failed:     return TEXT("Failed");
		}
		return TEXT("NotIndexed");
	}
#endif // WITH_EDITOR
}

// ─── PSS info ──────────────────────────────────────────────

FBridgePSSInfo UUnrealBridgePoseSearchLibrary::GetSchemaInfo(const FString& SchemaPath)
{
	using namespace BridgePoseSearchImpl;
	FBridgePSSInfo Out;

	UPoseSearchSchema* PSS = LoadSchema(SchemaPath);
	if (!PSS) return Out;

	Out.SampleRate = PSS->SampleRate;
	Out.SchemaCardinality = PSS->SchemaCardinality;
#if WITH_EDITORONLY_DATA
	Out.NumberOfPermutations = PSS->NumberOfPermutations;
	Out.PermutationsSampleRate = PSS->PermutationsSampleRate;
	Out.PermutationsTimeOffset = PSS->PermutationsTimeOffset;
	Out.DataPreprocessor = PreprocessorToString(PSS->DataPreprocessor);
#endif
	Out.bAddDataPadding = PSS->bAddDataPadding;
	Out.bInjectAdditionalDebugChannels = PSS->bInjectAdditionalDebugChannels;
	Out.FinalizedChannelCount = PSS->GetChannels().Num();

	// Top-level (authored) channels live behind a private array — use reflection.
	if (FArrayProperty* ChannelsProp =
		CastField<FArrayProperty>(UPoseSearchSchema::StaticClass()->FindPropertyByName(TEXT("Channels"))))
	{
		FScriptArrayHelper Helper(ChannelsProp, ChannelsProp->ContainerPtrToValuePtr<void>(PSS));
		Out.ChannelCount = Helper.Num();
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			UPoseSearchFeatureChannel* Ch =
				*reinterpret_cast<UPoseSearchFeatureChannel**>(Helper.GetRawPtr(i));
			if (Ch)
			{
				Out.TotalChannelWeight += ReadFloatProperty(Ch, TEXT("Weight"), 1.f);
			}
		}
	}

	for (const FPoseSearchRoledSkeleton& RS : PSS->GetRoledSkeletons())
	{
		FBridgePSSSkeleton S;
		S.SkeletonPath = GetAssetPathSafe(RS.Skeleton);
		S.MirrorDataTablePath = GetAssetPathSafe(RS.MirrorDataTable);
		S.Role = RS.Role.ToString();
		Out.Skeletons.Add(S);
	}

	return Out;
}

TArray<FBridgePSSChannel> UUnrealBridgePoseSearchLibrary::ListSchemaChannels(const FString& SchemaPath)
{
	using namespace BridgePoseSearchImpl;
	TArray<FBridgePSSChannel> Out;

	UPoseSearchSchema* PSS = LoadSchema(SchemaPath);
	if (!PSS) return Out;

	if (FArrayProperty* ChannelsProp =
		CastField<FArrayProperty>(UPoseSearchSchema::StaticClass()->FindPropertyByName(TEXT("Channels"))))
	{
		FScriptArrayHelper Helper(ChannelsProp, ChannelsProp->ContainerPtrToValuePtr<void>(PSS));
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			UPoseSearchFeatureChannel* Ch =
				*reinterpret_cast<UPoseSearchFeatureChannel**>(Helper.GetRawPtr(i));
			FBridgePSSChannel Row;
			Row.Kind = Ch ? Ch->GetClass()->GetName() : TEXT("(null)");
			if (Ch)
			{
				Row.Weight = ReadFloatProperty(Ch, TEXT("Weight"), 1.f);
				Row.SampleTimeOffset = ReadFloatProperty(Ch, TEXT("SampleTimeOffset"), 0.f);
				Row.BoneName = ReadBoneNameProperty(Ch, TEXT("Bone"));
				Row.SubChannelCount = Ch->GetSubChannels().Num();
			}
			Out.Add(Row);
		}
	}
	return Out;
}

// ─── PSD info ──────────────────────────────────────────────

FBridgePSDInfo UUnrealBridgePoseSearchLibrary::GetDatabaseInfo(const FString& DatabasePath)
{
	using namespace BridgePoseSearchImpl;
	FBridgePSDInfo Out;

	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return Out;

	Out.SchemaPath = GetAssetPathSafe(PSD->Schema);
#if WITH_EDITORONLY_DATA
	Out.NormalizationSetPath = GetAssetPathSafe(PSD->NormalizationSet);
#endif
	Out.EntryCount = PSD->GetNumAnimationAssets();
	Out.PoseSearchMode = PoseSearchModeToString(PSD->PoseSearchMode);
#if WITH_EDITORONLY_DATA
	Out.NumberOfPrincipalComponents = PSD->NumberOfPrincipalComponents;
#endif
	Out.KDTreeQueryNumNeighbors = PSD->KDTreeQueryNumNeighbors;
	Out.ContinuingPoseCostBias = PSD->ContinuingPoseCostBias;
	Out.BaseCostBias = PSD->BaseCostBias;
	Out.LoopingCostBias = PSD->LoopingCostBias;
	Out.Tags = PSD->Tags;

#if WITH_EDITOR
	Out.IndexStatus = IndexStatusToString(ProbeIndexState(PSD));
	const UE::PoseSearch::FSearchIndex& Idx = PSD->GetSearchIndex();
	Out.IndexedPoseCount = Idx.GetNumPoses();
	// Approximate memory: feature-vector data is the dominant cost. PoseMetadata is small per-pose.
	Out.IndexedMemoryBytes = static_cast<int64>(Idx.Values.GetAllocatedSize() + Idx.PoseMetadata.GetAllocatedSize());
#endif

	return Out;
}

TArray<FBridgePSDAnimEntry> UUnrealBridgePoseSearchLibrary::ListDatabaseAnimations(const FString& DatabasePath)
{
	using namespace BridgePoseSearchImpl;
	TArray<FBridgePSDAnimEntry> Out;

	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return Out;

	const int32 N = PSD->GetNumAnimationAssets();
	Out.Reserve(N);
	for (int32 i = 0; i < N; ++i)
	{
		FBridgePSDAnimEntry Row;
		const FPoseSearchDatabaseAnimationAsset* Entry = PSD->GetDatabaseAnimationAsset(i);
		if (!Entry)
		{
			Out.Add(Row);
			continue;
		}

		UObject* Asset = Entry->AnimAsset;
		Row.AssetPath = GetAssetPathSafe(Asset);
		Row.Kind = Asset ? Asset->GetClass()->GetName() : FString();
#if WITH_EDITORONLY_DATA
		Row.bEnabled = Entry->bEnabled;
		Row.bDisableReselection = Entry->bDisableReselection;
		Row.MirrorOption = MirrorOptionToString(Entry->MirrorOption);

		if (Cast<UBlendSpace>(Asset))
		{
			Row.BlendSpaceHorizontalSamples = Entry->NumberOfHorizontalSamples;
			Row.BlendSpaceVerticalSamples = Entry->NumberOfVerticalSamples;
			Row.bBlendSpaceUseSingleSample = Entry->bUseSingleSample;
			Row.bBlendSpaceUseGridForSampling = Entry->bUseGridForSampling;
			Row.BlendSpaceParamX = Entry->BlendParamX;
			Row.BlendSpaceParamY = Entry->BlendParamY;
		}

		const FFloatInterval Range = Entry->GetSamplingRange();
		Row.SamplingRangeMin = Range.Min;
		Row.SamplingRangeMax = Range.Max;
#endif
		Out.Add(Row);
	}
	return Out;
}

TArray<FString> UUnrealBridgePoseSearchLibrary::FindDatabasesUsingAnimation(const FString& AnimationAssetPath)
{
	using namespace BridgePoseSearchImpl;
	TArray<FString> Out;

	UObject* Anim = LoadAnyAnim(AnimationAssetPath);
	if (!Anim) return Out;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> PSDAssets;
	AR.GetAssetsByClass(UPoseSearchDatabase::StaticClass()->GetClassPathName(), PSDAssets);
	for (const FAssetData& AD : PSDAssets)
	{
		UPoseSearchDatabase* PSD = Cast<UPoseSearchDatabase>(AD.GetAsset());
		if (PSD && PSD->Contains(Anim))
		{
			Out.Add(PSD->GetPathName());
		}
	}
	return Out;
}

// ─── PSD writes ────────────────────────────────────────────

namespace BridgePoseSearchImpl
{
	int32 AddEntryCommon(UPoseSearchDatabase* PSD, UObject* Asset,
		float Min, float Max, EPoseSearchMirrorOption Mirror, bool bEnabled,
		const TFunction<void(FPoseSearchDatabaseAnimationAsset&)>& BlendSpaceTuner)
	{
#if WITH_EDITORONLY_DATA
		if (!PSD || !Asset) return INDEX_NONE;

		FPoseSearchDatabaseAnimationAsset Entry;
		Entry.AnimAsset = Asset;
		Entry.bEnabled = bEnabled;
		Entry.MirrorOption = Mirror;
		Entry.SamplingRange = FFloatInterval(Min, Max);
		if (BlendSpaceTuner)
		{
			BlendSpaceTuner(Entry);
		}

		const FScopedTransaction Tx(LOCTEXT("AddPSDEntry", "Add PoseSearch Database Entry"));
		PSD->Modify();
		PSD->AddAnimationAsset(Entry);
		PSD->MarkPackageDirty();
		// Schedule re-index. NewRequest invalidates and starts a fresh build.
#if WITH_EDITOR
		using namespace UE::PoseSearch;
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
		return PSD->GetNumAnimationAssets() - 1;
#else
		return INDEX_NONE;
#endif
	}

	FBridgePSDAddResult MakeAddError(const FString& Msg)
	{
		FBridgePSDAddResult R;
		R.Index = -1;
		R.Error = Msg;
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: %s"), *Msg);
		return R;
	}
}

FBridgePSDAddResult UUnrealBridgePoseSearchLibrary::AddAnimationToDatabase(const FString& DatabasePath, const FString& AnimationAssetPath,
	float SamplingRangeMin, float SamplingRangeMax, const FString& MirrorOption, bool bEnabled)
{
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD)
	{
		return MakeAddError(FString::Printf(TEXT("AddAnimationToDatabase: cannot load PoseSearchDatabase '%s'"), *DatabasePath));
	}

	UObject* Asset = LoadAnyAnim(AnimationAssetPath);
	if (!Asset)
	{
		return MakeAddError(FString::Printf(TEXT("AddAnimationToDatabase: cannot load animation asset '%s' (typo? wrong package mount?)"), *AnimationAssetPath));
	}
	if (!Cast<UAnimSequence>(Asset) && !Cast<UBlendSpace>(Asset) && !Cast<UAnimComposite>(Asset) && !Cast<UAnimMontage>(Asset))
	{
		return MakeAddError(FString::Printf(TEXT("AddAnimationToDatabase: '%s' is a %s, not AnimSequence/BlendSpace/AnimComposite/AnimMontage"),
			*AnimationAssetPath, *Asset->GetClass()->GetName()));
	}

	EPoseSearchMirrorOption Mirror = EPoseSearchMirrorOption::UnmirroredOnly;
	if (!ParseMirrorOption(MirrorOption, Mirror))
	{
		return MakeAddError(FString::Printf(TEXT("AddAnimationToDatabase: invalid MirrorOption '%s' — expected one of UnmirroredOnly / MirroredOnly / UnmirroredAndMirrored (case-insensitive)"), *MirrorOption));
	}

	FBridgePSDAddResult R;
	R.Index = AddEntryCommon(PSD, Asset, SamplingRangeMin, SamplingRangeMax, Mirror, bEnabled, nullptr);
	if (R.Index < 0)
	{
		R.Error = TEXT("AddAnimationToDatabase: AddEntryCommon failed (likely WITH_EDITORONLY_DATA off)");
	}
	return R;
}

FBridgePSDAddResult UUnrealBridgePoseSearchLibrary::AddBlendSpaceToDatabase(const FString& DatabasePath, const FString& BlendSpacePath,
	int32 HSamples, int32 VSamples,
	bool bUseGridForSampling, bool bUseSingleSample,
	float BlendParamX, float BlendParamY,
	float SamplingRangeMin, float SamplingRangeMax,
	const FString& MirrorOption, bool bEnabled)
{
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD)
	{
		return MakeAddError(FString::Printf(TEXT("AddBlendSpaceToDatabase: cannot load PoseSearchDatabase '%s'"), *DatabasePath));
	}

	UObject* Loaded = LoadAnyAnim(BlendSpacePath);
	if (!Loaded)
	{
		return MakeAddError(FString::Printf(TEXT("AddBlendSpaceToDatabase: cannot load asset '%s'"), *BlendSpacePath));
	}
	UBlendSpace* BS = Cast<UBlendSpace>(Loaded);
	if (!BS)
	{
		return MakeAddError(FString::Printf(TEXT("AddBlendSpaceToDatabase: '%s' is a %s, not a BlendSpace"),
			*BlendSpacePath, *Loaded->GetClass()->GetName()));
	}

	EPoseSearchMirrorOption Mirror = EPoseSearchMirrorOption::UnmirroredOnly;
	if (!ParseMirrorOption(MirrorOption, Mirror))
	{
		return MakeAddError(FString::Printf(TEXT("AddBlendSpaceToDatabase: invalid MirrorOption '%s'"), *MirrorOption));
	}

	auto Tuner = [&](FPoseSearchDatabaseAnimationAsset& E)
	{
#if WITH_EDITORONLY_DATA
		E.NumberOfHorizontalSamples = FMath::Max(1, HSamples);
		E.NumberOfVerticalSamples   = FMath::Max(1, VSamples);
		E.bUseGridForSampling       = bUseGridForSampling;
		E.bUseSingleSample          = bUseSingleSample;
		E.BlendParamX               = BlendParamX;
		E.BlendParamY               = BlendParamY;
#endif
	};

	FBridgePSDAddResult R;
	R.Index = AddEntryCommon(PSD, BS, SamplingRangeMin, SamplingRangeMax, Mirror, bEnabled, Tuner);
	if (R.Index < 0)
	{
		R.Error = TEXT("AddBlendSpaceToDatabase: AddEntryCommon failed (likely WITH_EDITORONLY_DATA off)");
	}
	return R;
}

bool UUnrealBridgePoseSearchLibrary::RemoveDatabaseAnimationAt(const FString& DatabasePath, int32 Index)
{
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	if (Index < 0 || Index >= PSD->GetNumAnimationAssets())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: RemoveDatabaseAnimationAt index %d out of range [0, %d)"),
			Index, PSD->GetNumAnimationAssets());
		return false;
	}

	const FScopedTransaction Tx(LOCTEXT("RemovePSDEntry", "Remove PoseSearch Database Entry"));
	PSD->Modify();
	PSD->RemoveAnimationAssetAt(Index);
	PSD->MarkPackageDirty();
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
	return true;
}

int32 UUnrealBridgePoseSearchLibrary::RemoveDatabaseAnimationByAsset(const FString& DatabasePath, const FString& AnimationAssetPath)
{
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return INDEX_NONE;

	UObject* Anim = LoadAnyAnim(AnimationAssetPath);
	if (!Anim) return INDEX_NONE;

	for (int32 i = 0; i < PSD->GetNumAnimationAssets(); ++i)
	{
		const FPoseSearchDatabaseAnimationAsset* E = PSD->GetDatabaseAnimationAsset(i);
		if (E && E->AnimAsset == Anim)
		{
			RemoveDatabaseAnimationAt(DatabasePath, i);
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UUnrealBridgePoseSearchLibrary::ClearDatabaseAnimations(const FString& DatabasePath)
{
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return 0;

	const int32 Before = PSD->GetNumAnimationAssets();
	if (Before == 0) return 0;

	const FScopedTransaction Tx(LOCTEXT("ClearPSDEntries", "Clear PoseSearch Database Entries"));
	PSD->Modify();
	for (int32 i = Before - 1; i >= 0; --i)
	{
		PSD->RemoveAnimationAssetAt(i);
	}
	PSD->MarkPackageDirty();
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
	return Before;
}

bool UUnrealBridgePoseSearchLibrary::SetDatabaseAnimationEnabled(const FString& DatabasePath, int32 Index, bool bEnabled)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	FPoseSearchDatabaseAnimationAsset* E = PSD->GetMutableDatabaseAnimationAsset(Index);
	if (!E)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetDatabaseAnimationEnabled index %d invalid"), Index);
		return false;
	}
	const FScopedTransaction Tx(LOCTEXT("PSDEntryEnabled", "Set PoseSearch Database Entry Enabled"));
	PSD->Modify();
	E->bEnabled = bEnabled;
	PSD->MarkPackageDirty();
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
	return true;
#else
	return false;
#endif
}

bool UUnrealBridgePoseSearchLibrary::SetDatabaseAnimationSamplingRange(const FString& DatabasePath, int32 Index,
	float SamplingRangeMin, float SamplingRangeMax)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	FPoseSearchDatabaseAnimationAsset* E = PSD->GetMutableDatabaseAnimationAsset(Index);
	if (!E) return false;

	const FScopedTransaction Tx(LOCTEXT("PSDEntryRange", "Set PoseSearch Database Entry Sampling Range"));
	PSD->Modify();
	E->SamplingRange = FFloatInterval(SamplingRangeMin, SamplingRangeMax);
	PSD->MarkPackageDirty();
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
	return true;
#else
	return false;
#endif
}

bool UUnrealBridgePoseSearchLibrary::SetDatabaseAnimationMirrorOption(const FString& DatabasePath, int32 Index, const FString& MirrorOption)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	FPoseSearchDatabaseAnimationAsset* E = PSD->GetMutableDatabaseAnimationAsset(Index);
	if (!E) return false;

	EPoseSearchMirrorOption Opt = EPoseSearchMirrorOption::UnmirroredOnly;
	if (!ParseMirrorOption(MirrorOption, Opt)) return false;

	const FScopedTransaction Tx(LOCTEXT("PSDEntryMirror", "Set PoseSearch Database Entry Mirror Option"));
	PSD->Modify();
	E->MirrorOption = Opt;
	PSD->MarkPackageDirty();
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
	return true;
#else
	return false;
#endif
}

bool UUnrealBridgePoseSearchLibrary::SetDatabaseBlendSpaceSampling(const FString& DatabasePath, int32 Index,
	int32 HSamples, int32 VSamples,
	bool bUseGridForSampling, bool bUseSingleSample,
	float BlendParamX, float BlendParamY)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	FPoseSearchDatabaseAnimationAsset* E = PSD->GetMutableDatabaseAnimationAsset(Index);
	if (!E) return false;
	if (!Cast<UBlendSpace>(E->AnimAsset))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetDatabaseBlendSpaceSampling entry %d is not a BlendSpace"), Index);
		return false;
	}

	const FScopedTransaction Tx(LOCTEXT("PSDEntryBSSampling", "Set PoseSearch Database BlendSpace Sampling"));
	PSD->Modify();
	E->NumberOfHorizontalSamples = FMath::Max(1, HSamples);
	E->NumberOfVerticalSamples   = FMath::Max(1, VSamples);
	E->bUseGridForSampling       = bUseGridForSampling;
	E->bUseSingleSample          = bUseSingleSample;
	E->BlendParamX               = BlendParamX;
	E->BlendParamY               = BlendParamY;
	PSD->MarkPackageDirty();
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
#endif
	return true;
#else
	return false;
#endif
}

// ─── Index lifecycle ───────────────────────────────────────

FString UUnrealBridgePoseSearchLibrary::RequestAsyncBuildIndex(const FString& DatabasePath)
{
#if WITH_EDITOR
	using namespace BridgePoseSearchImpl;
	using namespace UE::PoseSearch;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return TEXT("Error");
	const EAsyncBuildIndexResult R = FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
	return IndexStatusToString(R);
#else
	return TEXT("Error");
#endif
}

bool UUnrealBridgePoseSearchLibrary::IsIndexReady(const FString& DatabasePath)
{
#if WITH_EDITOR
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	return ProbeIndexState(PSD) == UE::PoseSearch::EAsyncBuildIndexResult::Success;
#else
	return false;
#endif
}

FString UUnrealBridgePoseSearchLibrary::GetIndexStatus(const FString& DatabasePath)
{
#if WITH_EDITOR
	using namespace BridgePoseSearchImpl;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return TEXT("Error");
	return IndexStatusToString(ProbeIndexState(PSD));
#else
	return TEXT("Error");
#endif
}

bool UUnrealBridgePoseSearchLibrary::InvalidateIndex(const FString& DatabasePath)
{
#if WITH_EDITOR
	using namespace BridgePoseSearchImpl;
	using namespace UE::PoseSearch;
	UPoseSearchDatabase* PSD = LoadDatabase(DatabasePath);
	if (!PSD) return false;
	// NewRequest cancels any pending request and forces a fresh build cycle.
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PSD, ERequestAsyncBuildFlag::NewRequest);
	return true;
#else
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

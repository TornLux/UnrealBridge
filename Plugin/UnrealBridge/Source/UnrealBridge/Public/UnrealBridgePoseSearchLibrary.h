#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgePoseSearchLibrary.generated.h"

// ─── Schema (PSS) info ─────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgePSSChannel
{
	GENERATED_BODY()

	/** Channel class short name, e.g. "PoseSearchFeatureChannel_Position". */
	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	/** Channel weight. */
	UPROPERTY(BlueprintReadOnly)
	float Weight = 1.f;

	/** For bone-based channels (Position/Velocity/Heading): the bone name. "" otherwise. */
	UPROPERTY(BlueprintReadOnly)
	FString BoneName;

	/** Sample-time offset in seconds (0 = present). */
	UPROPERTY(BlueprintReadOnly)
	float SampleTimeOffset = 0.f;

	/** Number of immediate child channels (Group/Trajectory have these). */
	UPROPERTY(BlueprintReadOnly)
	int32 SubChannelCount = 0;
};

USTRUCT(BlueprintType)
struct FBridgePSSSkeleton
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SkeletonPath;

	UPROPERTY(BlueprintReadOnly)
	FString MirrorDataTablePath;

	UPROPERTY(BlueprintReadOnly)
	FString Role;
};

USTRUCT(BlueprintType)
struct FBridgePSSInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SampleRate = 30;

	/** Final feature-vector dimension after Finalize() — drives KDTree/PCA performance. */
	UPROPERTY(BlueprintReadOnly)
	int32 SchemaCardinality = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NumberOfPermutations = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 PermutationsSampleRate = 30;

	UPROPERTY(BlueprintReadOnly)
	float PermutationsTimeOffset = 0.f;

	/** EPoseSearchDataPreprocessor as string. */
	UPROPERTY(BlueprintReadOnly)
	FString DataPreprocessor;

	UPROPERTY(BlueprintReadOnly)
	bool bAddDataPadding = false;

	UPROPERTY(BlueprintReadOnly)
	bool bInjectAdditionalDebugChannels = false;

	UPROPERTY(BlueprintReadOnly)
	int32 ChannelCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 FinalizedChannelCount = 0;

	/** Sum of every top-level channel weight (sanity check for normalization). */
	UPROPERTY(BlueprintReadOnly)
	float TotalChannelWeight = 0.f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgePSSSkeleton> Skeletons;
};

// ─── Database (PSD) info ───────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgePSDAnimEntry
{
	GENERATED_BODY()

	/** Underlying asset class short name: "AnimSequence", "BlendSpace", "AnimComposite", "AnimMontage", "" if missing. */
	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	UPROPERTY(BlueprintReadOnly)
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly)
	bool bDisableReselection = false;

	/** Sampling range min (seconds). 0 with max==0 means "use entire range". */
	UPROPERTY(BlueprintReadOnly)
	float SamplingRangeMin = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float SamplingRangeMax = 0.f;

	/** EPoseSearchMirrorOption as string: "UnmirroredOnly" / "MirroredOnly" / "UnmirroredAndMirrored". */
	UPROPERTY(BlueprintReadOnly)
	FString MirrorOption;

	/** BlendSpace-only: number of horizontal samples. -1 if N/A. */
	UPROPERTY(BlueprintReadOnly)
	int32 BlendSpaceHorizontalSamples = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 BlendSpaceVerticalSamples = -1;

	UPROPERTY(BlueprintReadOnly)
	bool bBlendSpaceUseSingleSample = false;

	UPROPERTY(BlueprintReadOnly)
	bool bBlendSpaceUseGridForSampling = false;

	UPROPERTY(BlueprintReadOnly)
	float BlendSpaceParamX = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float BlendSpaceParamY = 0.f;
};

USTRUCT(BlueprintType)
struct FBridgePSDInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SchemaPath;

	UPROPERTY(BlueprintReadOnly)
	FString NormalizationSetPath;

	UPROPERTY(BlueprintReadOnly)
	int32 EntryCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 IndexedPoseCount = 0;

	/** "NotIndexed" / "Indexing" / "Indexed" / "Failed". */
	UPROPERTY(BlueprintReadOnly)
	FString IndexStatus;

	UPROPERTY(BlueprintReadOnly)
	int64 IndexedMemoryBytes = 0;

	/** "BruteForce" / "PCAKDTree" / "VPTree" / "EventOnly". */
	UPROPERTY(BlueprintReadOnly)
	FString PoseSearchMode;

	UPROPERTY(BlueprintReadOnly)
	int32 NumberOfPrincipalComponents = 4;

	UPROPERTY(BlueprintReadOnly)
	int32 KDTreeQueryNumNeighbors = 200;

	UPROPERTY(BlueprintReadOnly)
	float ContinuingPoseCostBias = -0.01f;

	UPROPERTY(BlueprintReadOnly)
	float BaseCostBias = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float LoopingCostBias = -0.005f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FName> Tags;
};

// ─── Library class ─────────────────────────────────────────

UCLASS()
class UNREALBRIDGE_API UUnrealBridgePoseSearchLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── Schema (PSS) ──

	/** Get high-level metadata about a PoseSearchSchema asset. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static FBridgePSSInfo GetSchemaInfo(const FString& SchemaPath);

	/** List the top-level channels on a PoseSearchSchema (sub-channels not flattened — see SubChannelCount). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static TArray<FBridgePSSChannel> ListSchemaChannels(const FString& SchemaPath);

	// ── Database (PSD) read ──

	/** Get high-level metadata about a PoseSearchDatabase asset including index status. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static FBridgePSDInfo GetDatabaseInfo(const FString& DatabasePath);

	/** List every animation entry in the database (Sequence/BlendSpace/Composite/Montage). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static TArray<FBridgePSDAnimEntry> ListDatabaseAnimations(const FString& DatabasePath);

	/** Find every PoseSearchDatabase that contains the given animation asset. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static TArray<FString> FindDatabasesUsingAnimation(const FString& AnimationAssetPath);

	// ── Database (PSD) write ──

	/**
	 * Add an animation asset (AnimSequence / BlendSpace / AnimComposite / AnimMontage) to a PSD.
	 *
	 * @param SamplingRangeMin/Max  pass 0/0 to use the entire animation range.
	 * @param MirrorOption          "UnmirroredOnly" / "MirroredOnly" / "UnmirroredAndMirrored" (case-insensitive).
	 * @return new entry index, or -1 on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static int32 AddAnimationToDatabase(const FString& DatabasePath, const FString& AnimationAssetPath,
		float SamplingRangeMin, float SamplingRangeMax, const FString& MirrorOption, bool bEnabled);

	/**
	 * Add a BlendSpace with explicit sampling counts. Wraps AddAnimationToDatabase + sets blend-space-specific fields.
	 *
	 * @param HSamples / VSamples   number of horizontal/vertical samples (>= 1). Ignored if bUseGridForSampling/bUseSingleSample.
	 * @param bUseGridForSampling   override H/V samples with the BlendSpace's authored grid.
	 * @param bUseSingleSample      treat the BlendSpace as a single segment at (BlendParamX, BlendParamY).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static int32 AddBlendSpaceToDatabase(const FString& DatabasePath, const FString& BlendSpacePath,
		int32 HSamples, int32 VSamples,
		bool bUseGridForSampling, bool bUseSingleSample,
		float BlendParamX, float BlendParamY,
		float SamplingRangeMin, float SamplingRangeMax,
		const FString& MirrorOption, bool bEnabled);

	/** Remove the entry at `Index` from a PSD. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool RemoveDatabaseAnimationAt(const FString& DatabasePath, int32 Index);

	/** Remove the first entry referencing AnimationAssetPath. Returns the removed index, or -1. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static int32 RemoveDatabaseAnimationByAsset(const FString& DatabasePath, const FString& AnimationAssetPath);

	/** Clear every animation entry. Returns count removed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static int32 ClearDatabaseAnimations(const FString& DatabasePath);

	/** Toggle a single entry's `bEnabled` flag. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool SetDatabaseAnimationEnabled(const FString& DatabasePath, int32 Index, bool bEnabled);

	/** Update an entry's sampling range. Pass [0, 0] to mean "entire range". */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool SetDatabaseAnimationSamplingRange(const FString& DatabasePath, int32 Index,
		float SamplingRangeMin, float SamplingRangeMax);

	/** Update an entry's mirror option ("UnmirroredOnly" / "MirroredOnly" / "UnmirroredAndMirrored"). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool SetDatabaseAnimationMirrorOption(const FString& DatabasePath, int32 Index, const FString& MirrorOption);

	/** Update a BlendSpace entry's sampling layout. Returns false when the entry is not a BlendSpace. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool SetDatabaseBlendSpaceSampling(const FString& DatabasePath, int32 Index,
		int32 HSamples, int32 VSamples,
		bool bUseGridForSampling, bool bUseSingleSample,
		float BlendParamX, float BlendParamY);

	// ── Index lifecycle ──

	/** Kick off async index build for a PSD. Returns "InProgress" / "Success" / "Failed" / "Error". */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static FString RequestAsyncBuildIndex(const FString& DatabasePath);

	/** Returns true when the database has a current, valid index (no rebuild pending). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool IsIndexReady(const FString& DatabasePath);

	/** "NotIndexed" / "Indexing" / "Indexed" / "Failed". Convenience wrapper around the same probe IsIndexReady uses. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static FString GetIndexStatus(const FString& DatabasePath);

	/** Force the index to rebuild on next request (sets DDC dirty by issuing a NewRequest). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|PoseSearch")
	static bool InvalidateIndex(const FString& DatabasePath);
};

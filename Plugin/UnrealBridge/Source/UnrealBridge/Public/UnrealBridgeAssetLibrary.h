#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeAssetLibrary.generated.h"

/** Search scope for keyword-based asset search. */
UENUM(BlueprintType)
enum class EBridgeAssetSearchScope : uint8
{
	AllAssets            UMETA(DisplayName = "All Assets"),
	Project              UMETA(DisplayName = "Project (/Game)"),
	CustomPackagePath    UMETA(DisplayName = "Custom Package Root"),
};

/** A single AssetRegistry tag key/value pair. */
USTRUCT(BlueprintType)
struct FBridgeAssetTag
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	FString Key;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	FString Value;
};

/** Registry-backed asset metadata (no asset loading). */
USTRUCT(BlueprintType)
struct FBridgeAssetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	FString PackageName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	FString AssetName;

	/** TopLevelAssetPath of the asset's class (e.g. "/Script/Engine.Blueprint"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	FString ClassPath;

	/** True if this asset is an ObjectRedirector. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	bool bIsRedirector = false;

	/** Whether the registry actually found this asset. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	bool bFound = false;

	/** Size in bytes of the .uasset/.umap on disk; -1 if not resolvable. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	int64 DiskSize = -1;

	/** All AssetRegistry tag key/value pairs (may be empty). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset")
	TArray<FBridgeAssetTag> Tags;
};

/** Per-LOD stats for a static / skeletal mesh. */
USTRUCT(BlueprintType)
struct FBridgeMeshLODStats
{
	GENERATED_BODY()

	/** LOD index (0 = highest detail). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 LODIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 VertexCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 TriangleCount = 0;

	/** Material slot indices referenced by this LOD's sections. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<int32> MaterialIndices;
};

/** Summary of a UStaticMesh asset. */
USTRUCT(BlueprintType)
struct FBridgeStaticMeshInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString AssetPath;

	/** World-space bounds of the full mesh (LOD 0). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FVector BoundsOrigin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FVector BoundsExtent = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") float BoundsSphereRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumLODs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FBridgeMeshLODStats> LODStats;

	/** Material slot names in declaration order. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FString> MaterialSlotNames;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FString> MaterialAssetPaths;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumSockets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FString> SocketNames;

	/** true = has a simple collision mesh body; false = render-only. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bHasCollision = false;

	/** Number of UV channels on LOD 0. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumUVChannels = 0;

	/** Nanite-enabled flag (UE 5.x). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bHasNaniteData = false;
};

/** Summary of a USkeletalMesh asset. */
USTRUCT(BlueprintType)
struct FBridgeSkeletalMeshInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FVector BoundsOrigin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FVector BoundsExtent = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") float BoundsSphereRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumLODs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FBridgeMeshLODStats> LODStats;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FString> MaterialSlotNames;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FString> MaterialAssetPaths;

	/** Path to the bound USkeleton asset. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString SkeletonPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumBones = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumSockets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") TArray<FString> SocketNames;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumMorphTargets = 0;

	/** Path to the bound UPhysicsAsset if any. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString PhysicsAssetPath;
};

/** Summary of a UTexture2D asset. */
USTRUCT(BlueprintType)
struct FBridgeTextureInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 Width = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 Height = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumMips = 0;

	/** EPixelFormat name (e.g. "PF_DXT5", "PF_BC7", "PF_FloatRGBA"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString PixelFormat;

	/** TextureCompressionSettings enum name ("TC_Default", "TC_NormalMap", ...). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString CompressionSettings;

	/** TextureGroup enum name ("TEXTUREGROUP_World", "TEXTUREGROUP_UI", ...). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString LODGroup;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bSRGB = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bNeverStream = false;

	/** Bytes actually resident in memory (GetResourceSize). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int64 ResourceSizeBytes = 0;
};

/** Summary of a USoundWave / USoundCue asset. */
USTRUCT(BlueprintType)
struct FBridgeSoundInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString AssetPath;

	/** "SoundWave" | "SoundCue" | "MetaSound" | other. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") FString SoundKind;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") float DurationSeconds = 0.0f;

	/** Sample rate in Hz. 0 if not applicable (e.g. SoundCue). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 SampleRate = 0;

	/** 1 = mono, 2 = stereo, etc. 0 if not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int32 NumChannels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") bool bLooping = false;

	/** Approx compressed data bytes (SoundWave); 0 otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Asset") int64 CompressedDataBytes = 0;
};

/**
 * Asset query utilities exposed to Python/Blueprint via UnrealBridge.
 * Ported from UnrealClientProtocol (MIT License - Italink).
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── Asset Search ──────────────────────────────────────────

	/**
	 * SearchEverywhere-style keyword search with full options.
	 * Supports include/exclude tokens ("hero !enemy"), type filter ("&Type=Blueprint").
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void SearchAssets(
		const FString& Query,
		EBridgeAssetSearchScope Scope,
		const FString& ClassFilter,
		bool bCaseSensitive,
		bool bWholeWord,
		int32 MaxResults,
		int32 MinCharacters,
		const FString& CustomPackagePath,
		TArray<FSoftObjectPath>& OutSoftPaths,
		TArray<FString>& OutIncludeTokensForHighlight);

	/** Simplified: search all content roots, case-insensitive, no whole-word, MinCharacters=1. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void SearchAssetsInAllContent(
		const FString& Query,
		int32 MaxResults,
		TArray<FSoftObjectPath>& OutSoftPaths,
		TArray<FString>& OutIncludeTokensForHighlight);

	/** Simplified: search under a specific content folder path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void SearchAssetsUnderPath(
		const FString& ContentFolderPath,
		const FString& Query,
		int32 MaxResults,
		TArray<FSoftObjectPath>& OutSoftPaths,
		TArray<FString>& OutIncludeTokensForHighlight);

	// ── Derived Classes ───────────────────────────────────────

	/**
	 * Get all classes derived from the given base classes (excluding SKEL_, REINST_, hidden, deprecated).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetDerivedClasses(
		const TArray<UClass*>& BaseClasses,
		const TSet<UClass*>& ExcludedClasses,
		TSet<UClass*>& OutDerivedClasses);

	/**
	 * Get all classes derived from a Blueprint (by asset path).
	 * Accepts content path, object path, or export-text path with quotes.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetDerivedClassesByBlueprintPath(
		const FString& BlueprintClassPath,
		TArray<UClass*>& OutDerivedClasses);

	// helper used by GetDerivedClasses (with custom skip filter)
	static void GetDerivedClassesWithFilter(
		const TArray<UClass*>& BaseClasses,
		const TSet<UClass*>& ExcludedClasses,
		TSet<UClass*>& OutDerivedClasses,
		TFunction<bool(const UClass*)> ShouldSkipClassFilter);

	// ── Asset References ──────────────────────────────────────

	/**
	 * Get all dependencies and referencers of an asset in one call.
	 * Accepts object path or export-text path with quotes.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetReferences(
		const FString& AssetPath,
		TArray<FSoftObjectPath>& OutDependencies,
		TArray<FSoftObjectPath>& OutReferencers);

	// ── DataAsset Queries ─────────────────────────────────────

	/** Get all DataAssets of a given base class (recursive). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetDataAssetsByBaseClass(
		TSubclassOf<UDataAsset> BaseDataAssetClass,
		TArray<FAssetData>& OutAssetDatas);

	/** Get all DataAssets by loading the base class from an asset path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetDataAssetsByAssetPath(
		const FString& DataAssetPath,
		TArray<FAssetData>& OutAssetDatas);

	/** Same as GetDataAssetsByBaseClass but returns soft paths. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetDataAssetSoftPathsByBaseClass(
		TSubclassOf<UDataAsset> BaseDataAssetClass,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/** Same as GetDataAssetsByAssetPath but returns soft paths. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetDataAssetSoftPathsByAssetPath(
		const FString& DataAssetPath,
		TArray<FSoftObjectPath>& OutSoftPaths);

	// ── Folder / Path Queries ─────────────────────────────────

	/** List all asset soft paths under a content folder path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void ListAssetsUnderPath(
		const FString& FolderPath,
		bool bIncludeSubfolders,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/** Simplified: always recursive. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void ListAssetsUnderPathSimple(
		const FString& ContentFolderPath,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/** Get immediate sub-folder paths under a content path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetSubFolderPaths(
		const FString& FolderPath,
		TArray<FString>& OutSubFolderPaths);

	/** Get immediate sub-folder names under a content path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetSubFolderNames(
		const FName& FolderPath,
		TArray<FName>& OutSubFolderNames);

	// ── Registry Metadata (no load) ───────────────────────────

	/** Does the AssetRegistry know about this asset path (no load). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static bool DoesAssetExist(const FString& AssetPath);

	/** Registry-backed asset metadata: class path, redirector flag, disk size, all tags. No load. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FBridgeAssetInfo GetAssetInfo(const FString& AssetPath);

	/**
	 * All assets of a given class. ClassPath is a TopLevelAssetPath string, e.g.
	 * "/Script/Engine.Texture2D" or "/Game/BP/BP_MyActor.BP_MyActor_C".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetsByClass(
		const FString& ClassPath,
		bool bSearchSubClasses,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/**
	 * All assets whose AssetRegistry tag matches (TagName, TagValue). Optional class filter
	 * narrows the sweep ("" = all classes, accepts TopLevelAssetPath string).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetsByTagValue(
		const FString& TagName,
		const FString& TagValue,
		const FString& OptionalClassPath,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/**
	 * If AssetPath resolves to a UObjectRedirector, follow one hop and return the target
	 * object path as a string. Returns empty string when not a redirector or on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FString ResolveRedirector(const FString& AssetPath);

	// ── Cheap scalar / batch registry queries ──────────────────

	/**
	 * Just the class path of an asset (no tags, no disk size). Cheap lookup when you
	 * only need to know "what kind of asset is this?". Returns "" if the asset is unknown.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FString GetAssetClassPath(const FString& AssetPath);

	/**
	 * Read a single AssetRegistry tag value for an asset. No load. Returns "" when the
	 * asset is unknown or the tag does not exist on it.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FString GetAssetTagValue(const FString& AssetPath, const FString& TagName);

	/**
	 * Batch list every asset under any of the given content folder paths in a single
	 * registry sweep. Optional ClassFilter is a TopLevelAssetPath ("" = all classes).
	 * `bRecursive` controls subfolder descent for the path filter, and also enables
	 * recursive class matching when ClassFilter is non-empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetsByPackagePaths(
		const TArray<FString>& FolderPaths,
		const FString& ClassFilter,
		bool bRecursive,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/**
	 * One registry pass returning every asset whose class matches any entry in
	 * `ClassPaths` (TopLevelAssetPath strings). Use instead of N separate
	 * `GetAssetsByClass` calls when you need multiple types at once.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetsOfClasses(
		const TArray<FString>& ClassPaths,
		bool bSearchSubClasses,
		TArray<FSoftObjectPath>& OutSoftPaths);

	/**
	 * Find every UObjectRedirector under a content folder. Pair with
	 * `unreal.UnrealBridgeEditorLibrary.fixup_redirectors` for batch cleanup.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void FindRedirectorsUnderPath(
		const FString& FolderPath,
		bool bRecursive,
		TArray<FSoftObjectPath>& OutSoftPaths);

	// ── Cheap counts & batched per-asset queries ────────────────

	/**
	 * Count assets under a content folder in a single registry sweep. No soft paths
	 * are materialized — cheap way to decide whether a folder is small enough to list.
	 * Optional ClassFilter is a TopLevelAssetPath ("" = any class). Returns 0 on
	 * unknown folder.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static int32 GetAssetCountUnderPath(
		const FString& FolderPath,
		const FString& ClassFilter,
		bool bRecursive);

	/**
	 * Package-level dependencies. PackageName is a content path like "/Game/BP/BP_MyActor"
	 * (no class-name suffix). When bHardOnly is true, only Hard+Game dependencies are
	 * returned (cooker-relevant); otherwise all categories. Returns package name strings.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetPackageDependencies(
		const FString& PackageName,
		bool bHardOnly,
		TArray<FString>& OutDependencyPackageNames);

	/**
	 * Package-level referencers. Mirror of GetPackageDependencies. Useful before
	 * rename/delete to check "who would break if this package goes away".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetPackageReferencers(
		const FString& PackageName,
		bool bHardOnly,
		TArray<FString>& OutReferencerPackageNames);

	/**
	 * Batch read of a single AssetRegistry tag value for many asset paths, one
	 * registry pass. OutValues is aligned 1:1 with AssetPaths — missing assets or
	 * missing tags produce an empty string at that index.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetTagValuesBatch(
		const TArray<FString>& AssetPaths,
		const FString& TagName,
		TArray<FString>& OutValues);

	/**
	 * Batch disk size (bytes) for many asset paths. OutSizes is aligned 1:1 with
	 * AssetPaths; unresolved entries are -1. Sidesteps N GetAssetInfo round-trips
	 * when the caller only wants sizes.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetDiskSizesBatch(
		const TArray<FString>& AssetPaths,
		TArray<int64>& OutSizes);

	// ── Structural / aggregate helpers ─────────────────────────

	/**
	 * List all mounted content roots visible to the AssetRegistry (e.g. "/Game/",
	 * "/Engine/", plugin roots). Trailing slashes match UE's own convention — strip
	 * before passing to registry APIs that expect no trailing slash.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetContentRoots(TArray<FString>& OutRoots);

	/**
	 * Cheap existence check for a content folder path. True when the AssetRegistry
	 * has the path indexed (it holds at least one asset directly or recursively).
	 * Accepts "/Game/Foo" with or without trailing slash.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static bool DoesFolderExist(const FString& FolderPath);

	/**
	 * Aggregate .uasset/.umap disk bytes under a content folder in a single registry
	 * sweep. Optional ClassFilter narrows by TopLevelAssetPath ("" = any class).
	 * Returns total size; OutAssetCount is filled with the number of assets summed.
	 * Unresolvable files are skipped silently.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static int64 GetTotalDiskSizeUnderPath(
		const FString& FolderPath,
		const FString& ClassFilter,
		bool bRecursive,
		int32& OutAssetCount);

	/**
	 * Batch class-path lookup. OutClassPaths is aligned 1:1 with AssetPaths —
	 * unknown assets yield "" at that index. One registry pass, no load. Use instead
	 * of N GetAssetClassPath round-trips.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetAssetClassPathsBatch(
		const TArray<FString>& AssetPaths,
		TArray<FString>& OutClassPaths);

	/**
	 * Transitive package dependency closure, bounded by MaxDepth. The returned set
	 * is the union of all packages reachable from PackageName via dependency edges,
	 * excluding PackageName itself. When bHardOnly=true, only Hard+Game edges are
	 * traversed (cooker-relevant closure). MaxDepth <= 0 is treated as unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static void GetPackageDependenciesRecursive(
		const FString& PackageName,
		bool bHardOnly,
		int32 MaxDepth,
		TArray<FString>& OutDependencyPackageNames);

	// ── Asset introspection ──────────────────────────────────

	/**
	 * Per-LOD vertex / triangle counts, mesh bounds, material slots, sockets,
	 * UV channel count, Nanite flag for a UStaticMesh asset.
	 * Returns `bFound = false` if the asset path can't be resolved.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FBridgeStaticMeshInfo GetStaticMeshInfo(const FString& AssetPath);

	/**
	 * Per-LOD vertex / triangle counts, mesh bounds, material slots, bone /
	 * socket / morph target counts, bound Skeleton + PhysicsAsset paths for
	 * a USkeletalMesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FBridgeSkeletalMeshInfo GetSkeletalMeshInfo(const FString& AssetPath);

	/**
	 * Dimensions, pixel format, compression settings, LOD group, sRGB flag,
	 * resident memory bytes for a UTexture2D.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FBridgeTextureInfo GetTextureInfo(const FString& AssetPath);

	/**
	 * Duration, sample rate, channel count, loop flag for a USoundWave or
	 * USoundCue. SoundCue duration is the computed total cue length; sample
	 * rate + channel count are only populated for SoundWave.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Asset")
	static FBridgeSoundInfo GetSoundInfo(const FString& AssetPath);
};

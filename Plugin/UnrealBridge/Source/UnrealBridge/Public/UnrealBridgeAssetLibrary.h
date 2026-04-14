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

	UPROPERTY(BlueprintReadOnly)
	FString Key;

	UPROPERTY(BlueprintReadOnly)
	FString Value;
};

/** Registry-backed asset metadata (no asset loading). */
USTRUCT(BlueprintType)
struct FBridgeAssetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString PackageName;

	UPROPERTY(BlueprintReadOnly)
	FString AssetName;

	/** TopLevelAssetPath of the asset's class (e.g. "/Script/Engine.Blueprint"). */
	UPROPERTY(BlueprintReadOnly)
	FString ClassPath;

	/** True if this asset is an ObjectRedirector. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsRedirector = false;

	/** Whether the registry actually found this asset. */
	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	/** Size in bytes of the .uasset/.umap on disk; -1 if not resolvable. */
	UPROPERTY(BlueprintReadOnly)
	int64 DiskSize = -1;

	/** All AssetRegistry tag key/value pairs (may be empty). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAssetTag> Tags;
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
};

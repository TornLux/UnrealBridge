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
};

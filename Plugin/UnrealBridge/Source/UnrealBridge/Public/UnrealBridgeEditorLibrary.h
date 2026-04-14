#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeEditorLibrary.generated.h"

// ─── Editor state ───────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeEditorState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString EngineVersion;

	UPROPERTY(BlueprintReadOnly)
	FString ProjectName;

	UPROPERTY(BlueprintReadOnly)
	bool bIsPIE = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsPaused = false;

	UPROPERTY(BlueprintReadOnly)
	FString CurrentLevelPath;

	UPROPERTY(BlueprintReadOnly)
	int32 NumOpenedAssets = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NumSelectedActors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NumContentBrowserSelection = 0;
};

// ─── Opened asset ───────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeOpenedAsset
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(BlueprintReadOnly)
	bool bIsDirty = false;
};

// ─── Compile result ─────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Path;

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	FString ErrorMessage;
};

// ─── Viewport camera ────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeViewportCamera
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly)
	float FOV = 90.f;
};

/**
 * Editor session / asset control via UnrealBridge.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ─── State ────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FBridgeEditorState GetEditorState();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FBridgeOpenedAsset> GetOpenedAssets();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FString> GetContentBrowserSelection();

	/** Currently-focused Content Browser path (e.g. "/Game/MyFolder"). Empty if unavailable. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetContentBrowserPath();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FBridgeViewportCamera GetEditorViewportCamera();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetEngineVersion();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsInPIE();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsPlayInEditorPaused();

	// ─── Asset control ────────────────────────────────────

	/** Open the appropriate asset editor. Accepts object path or package path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool OpenAsset(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool CloseAllAssetEditors();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SaveAsset(const FString& AssetPath);

	/** Save all dirty packages. Returns true if the save attempt finished without user cancel. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SaveAllDirtyAssets(bool bIncludeMaps);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SaveCurrentLevel();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool ReloadAsset(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetContentBrowserSelection(const TArray<FString>& AssetPaths);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SyncContentBrowserToAsset(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool FocusViewportOnSelection();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetEditorViewportCamera(FVector Location, FRotator Rotation, float FOV);

	// ─── PIE ──────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool StartPIE();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool StopPIE();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool PausePIE(bool bPaused);

	// ─── Undo ─────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool Undo();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool Redo();

	// ─── Console / CVar ───────────────────────────────────

	/** Run a console command. Returns captured GLog output (best-effort — some commands print to viewport only). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString ExecuteConsoleCommand(const FString& Command);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetCVar(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetCVar(const FString& Name, const FString& Value);

	/** Search CVars by substring. Returns "Name = Value" entries. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FString> ListCVars(const FString& Keyword);

	// ─── Utility ──────────────────────────────────────────

	/**
	 * Fix up object redirectors under the given content paths (e.g. "/Game/Foo").
	 * Re-saves referencers to point at the destination and deletes the redirector.
	 * Returns the number of redirectors processed.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 FixupRedirectors(const TArray<FString>& Paths);

	/** Compile the listed Blueprints. Returns per-BP success + error summary. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FBridgeCompileResult> CompileBlueprints(const TArray<FString>& BlueprintPaths);

	// ─── Dirty-state tracking ────────────────────────────

	/** Package names of all currently-dirty `/Game/...` packages. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FString> GetDirtyPackageNames();

	/** True if the asset's package has unsaved modifications. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsAssetDirty(const FString& AssetPath);

	/** Mark the asset's package dirty (useful after external mutations). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool MarkAssetDirty(const FString& AssetPath);

	/** True if an asset editor tab is currently open for this asset. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsAssetEditorOpen(const FString& AssetPath);

	/**
	 * Save the listed assets (silent — no save dialog). Returns the number
	 * of packages successfully written.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 SaveAssets(const TArray<FString>& AssetPaths);

	// ─── Level / map control ─────────────────────────────

	/**
	 * Load a map into the editor. Accepts a package path like `/Game/Maps/MyLevel`.
	 * If there are unsaved changes the user is prompted unless `bPromptSaveChanges`
	 * is false (then unsaved changes are discarded).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool LoadLevel(const FString& LevelPath, bool bPromptSaveChanges);

	/** Create a new empty level and make it the current editor world. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool CreateNewLevel(bool bSaveExisting);

	// ─── Asset editor / browser extras ───────────────────

	/** Close the editor tab for a single asset. Returns false if no editor was open. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool CloseAssetEditor(const FString& AssetPath);

	/** True if the asset's package is already loaded in memory (does NOT force-load). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsAssetLoaded(const FString& AssetPath);

	/** Navigate the Content Browser to a folder (e.g. `/Game/Maps`). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetContentBrowserPath(const FString& FolderPath);

	// ─── Viewport capture ────────────────────────────────

	/**
	 * Request a high-res screenshot of the active level viewport.
	 * Writes to `Saved/Screenshots/...` by default — the engine decides the
	 * filename; `ResolutionMultiplier` scales the viewport size (1.0 = native).
	 * Returns true if the request was queued.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool TakeHighResScreenshot(float ResolutionMultiplier);
};

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

	/**
	 * Start "Simulate in Editor" — spins up the play world without spawning a
	 * player controller / possessing a pawn. Useful for observing AI, physics,
	 * or Sequencer playback without taking input focus.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool StartSimulate();

	/** True when a Simulate-in-Editor session is currently running. False for PIE proper or idle editor. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsSimulating();

	/**
	 * Network mode of the active PIE/Simulate world, as a string:
	 * "Standalone" | "DedicatedServer" | "ListenServer" | "Client".
	 * Empty string when no PIE/Simulate world is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetPIENetMode();

	/**
	 * Play world time in seconds since `BeginPlay` on the PIE/Simulate world.
	 * Returns -1.0 when no PIE/Simulate world is running. Honors PIE pause
	 * (frozen time advances to whatever UWorld::GetTimeSeconds reports, which
	 * stops while paused).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetPIEWorldTime();

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

	// ─── Viewport render / display ───────────────────────

	/** Toggle realtime rendering for the active level viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetViewportRealtime(bool bRealtime);

	/** Query realtime state of the active level viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsViewportRealtime();

	/** Pixel size of the active level viewport (X = width, Y = height). Zero if no viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FVector2D GetViewportSize();

	/**
	 * Set the active viewport's view mode by name.
	 * Accepted: "Lit", "Unlit", "Wireframe", "BrushWireframe", "DetailLighting",
	 * "LightingOnly", "LightComplexity", "ShaderComplexity", "LightmapDensity",
	 * "ReflectionOverride", "CollisionPawn", "CollisionVisibility", "LODColoration".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetViewportViewMode(const FString& Mode);

	/** Current view mode name of the active viewport (e.g. "Lit"). Empty if no viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetViewportViewMode();

	/**
	 * Toggle a named engine show flag on the active viewport (e.g. "Grid",
	 * "Bounds", "Collision", "Navigation", "Landscape"). Name matches
	 * `FEngineShowFlags::FindIndexByName`.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetViewportShowFlag(const FString& FlagName, bool bEnabled);

	/** Read a named engine show flag on the active viewport. False if flag name unknown. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool GetViewportShowFlag(const FString& FlagName);

	/**
	 * Set the active viewport's projection type.
	 * Accepted: "Perspective", "Top" / "OrthoXY", "Front" / "OrthoXZ",
	 * "Side" / "OrthoYZ", "OrthoFreelook".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetViewportType(const FString& ViewportType);

	/** Current viewport projection type name ("Perspective", "Top", "Front", "Side", "OrthoFreelook"). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetViewportType();

	// ─── Editor UX + plugin introspection ────────────────────

	/**
	 * Show a Slate toast notification in the editor's lower-right corner.
	 * Useful for long-running automation scripts to report progress.
	 *
	 * @param Message           Body text.
	 * @param DurationSeconds   Fade-out delay; clamped to [1, 60]. Default 4s.
	 * @param bSuccess          Style hint — green checkmark (true) or
	 *                          red X (false). Neutral toasts pass true.
	 * @return true if the notification was queued.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool ShowEditorNotification(const FString& Message, float DurationSeconds = 4.0f, bool bSuccess = true);

	/** Names of all currently-enabled plugins (sorted alphabetically). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FString> GetEnabledPlugins();

	/** True if a plugin is known and currently enabled. Case-insensitive match on the plugin name. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsPluginEnabled(const FString& PluginName);

	/**
	 * Compiled build configuration of the running editor, one of:
	 * "Debug", "DebugGame", "Development", "Shipping", "Test", or "Unknown".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetEditorBuildConfig();

	// ─── Log + paths + window focus ──────────────────────────

	/**
	 * Emit a message to GLog under a dedicated "UnrealBridgePy" category.
	 * Lets Python scripts route into the standard UE log stream so their
	 * output is captured by UE's log file and the Output Log window.
	 *
	 * @param Severity  "Verbose" | "Log" | "Warning" | "Error"
	 *                  (case-insensitive). Unknown values default to Log.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool WriteLogMessage(const FString& Message, const FString& Severity = TEXT("Log"));

	/** Absolute path to the current editor log file (`.../Saved/Logs/<Project>.log`). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetLogFilePath();

	/** Absolute path to the project's screenshot directory (`.../Saved/Screenshots`). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetScreenshotDirectory();

	/**
	 * Raise and activate the editor's main window. Useful after long-running
	 * background automation when the user switched to another app.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool BringEditorToFront();

	// ─── Engine stats + GC ───────────────────────────────────

	/**
	 * Instantaneous FPS from the last delta-time sample.
	 * Clamped to 0 at extremely small deltas. Expect per-frame jitter —
	 * average client-side if you need a smoother signal.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetFrameRate();

	/** Physical memory used by the editor process, in MB. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetMemoryUsageMB();

	/**
	 * Seconds since the engine finished initializing. Monotonic —
	 * same clock `unreal.SystemLibrary.get_game_time_in_seconds` uses
	 * for the editor world.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetEngineUptime();

	/**
	 * Force a garbage-collection pass. Useful after destroying a large
	 * batch of actors / unloading many assets when the caller wants
	 * memory reclaimed now rather than at the next engine tick.
	 *
	 * @param bFullPurge  true = full purge (slow, compacts the GC pool);
	 *                    false = incremental collection (default).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool TriggerGarbageCollection(bool bFullPurge = false);

	// ─── Project metadata + paths ────────────────────────────

	/**
	 * `ProjectVersion` string from `[/Script/EngineSettings.GeneralProjectSettings]`
	 * in DefaultGame.ini. Empty when the project has none set.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetProjectVersion();

	/** `CompanyName` from the project's general settings. Empty if unset. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetProjectCompanyName();

	/**
	 * Unique project identifier (`ProjectID` GUID from the .uproject).
	 * Useful as a stable key for per-project caches / telemetry.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetProjectID();

	/** Absolute path to the project's autosave directory (`.../Saved/Autosaves`). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetAutoSaveDirectory();

	// ─── Editor tab management + window title ────────────────

	/**
	 * Invoke a docked-tab spawner by tab id. Opens the tab if registered
	 * but not open; refocuses it if already open. Common ids:
	 *   "OutputLog", "ContentBrowserTab1", "StatsViewer",
	 *   "MessageLog", "LevelEditorToolBox".
	 * Returns false if the id isn't a registered spawner.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool OpenEditorTab(const FString& TabName);

	/**
	 * Close a live docked tab by id. Returns false if no matching tab is
	 * currently open. No effect on tabs that aren't registered.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool CloseEditorTab(const FString& TabName);

	/** True if a tab with the given id is currently live (docked or floating). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsEditorTabOpen(const FString& TabName);

	/**
	 * Title text of the editor's main frame window (e.g.
	 * "GameplayLocomotion - Unreal Editor"). Empty string when the
	 * main frame module isn't yet available.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetMainWindowTitle();

	// ─── Host system info ────────────────────────────────────

	/** Platform OS version string (e.g. "Windows 10 (Build 19045)"). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetOSVersion();

	/** CPU brand string (e.g. "13th Gen Intel(R) Core(TM) i7-13700K"). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetCPUBrand();

	/** Number of logical CPU cores available to the editor process. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetCPUCoreCount();

	/** Total physical RAM installed on the host machine, in MB. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetTotalPhysicalMemoryMB();

	// ─── Shader + asset compile state ────────────────────────

	/**
	 * Pending shader-compile job count across all shader compilers. 0 when
	 * the editor is idle. Spikes after opening a new map / editing a
	 * material / reimporting a texture.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetShaderCompileJobCount();

	/** Pending async asset-compile count (materials, textures, meshes). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetAssetCompileJobCount();

	/** True while any shader or asset is still compiling in the background. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsCompiling();

	/**
	 * Block the game thread until all pending shader + asset compilation
	 * jobs finish. Can take tens of seconds after a big import. Returns
	 * after the queue drains.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool FlushCompilation();

	// ─── Output log tail ─────────────────────────────────────
	//
	// Lazily installs a custom FOutputDevice on first query, keeping the
	// most recent log lines in a thread-safe ring buffer. Ideal for
	// automation that wants to surface editor warnings/errors after a
	// long operation without scraping the full .log file.

	/**
	 * Return the most recent captured log lines, newest last. Each entry
	 * is preformatted as `"[Category][Severity] Message"`.
	 *
	 * @param NumLines       Max lines to return; clamped to ring capacity.
	 *                       Pass 0 to return everything currently buffered.
	 * @param MinSeverity    Filter: "Verbose" | "Log" | "Display" |
	 *                       "Warning" | "Error" | "Fatal" (case-insensitive).
	 *                       Only entries at or above this severity are
	 *                       returned. Pass empty string for no filter.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FString> GetRecentLogLines(int32 NumLines = 50, const FString& MinSeverity = TEXT(""));

	/** Number of lines currently buffered. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetLogBufferSize();

	/** Ring buffer capacity (max lines retained). Default 500. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetLogBufferCapacity();

	/** Clear the log ring buffer. Returns the number of lines dropped. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 ClearLogBuffer();

	// ─── Engine module introspection ─────────────────────────

	/**
	 * True if a module of the given name is currently loaded in the editor
	 * process. Complements `is_plugin_enabled` — plugins own modules but
	 * a module can also exist without a plugin wrapper (engine built-ins).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsModuleLoaded(const FString& ModuleName);

	/**
	 * Names of every module currently registered with the module manager
	 * (loaded or unloaded). Sorted alphabetically. Typical editor count:
	 * 500-1000.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static TArray<FString> GetRegisteredModuleNames();

	/**
	 * Force-load a module if it isn't already loaded. Returns true if the
	 * module is loaded after the call, false if the name doesn't match a
	 * registered module. WARNING: loading game modules mid-session may
	 * introduce UObjects that affect GC. Editor-only tooling is safer.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool LoadModule(const FString& ModuleName);

	/**
	 * Filesystem path of a loaded module's compiled binary (e.g. the DLL
	 * on Windows). Empty string if the module isn't loaded or has no
	 * backing file (script-only modules).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetModuleBinaryPath(const FString& ModuleName);

	// ─── Viewport gizmo mode + coord system ──────────────────

	/**
	 * Active transform-gizmo mode in the level viewport:
	 * "Translate" | "Rotate" | "Scale" | "TranslateRotateZ" | "2D" | "None".
	 * Empty string when editor mode tools aren't available.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetWidgetMode();

	/**
	 * Switch gizmo mode. Accepts the strings returned by GetWidgetMode
	 * (case-insensitive). Returns false on unknown mode name.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetWidgetMode(const FString& Mode);

	/** Current coordinate system: "World" or "Local". */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetCoordSystem();

	/**
	 * Switch coord system. Accepts "World" / "Local" (case-insensitive).
	 * Mirrors the ~ key toggle in the viewport toolbar.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetCoordSystem(const FString& System);

	// ─── Viewport grid / snap ────────────────────────────────

	/**
	 * Current location-grid snap size in cm (entry in `PosGridSizes`
	 * at the active `CurrentPosGridSize` index). Returns 0 when the
	 * settings aren't available.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetLocationGridSize();

	/**
	 * Current rotation-grid snap size in degrees (pitch axis of the
	 * active `RotGridSizes` entry). Returns 0 when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static float GetRotationGridSize();

	/** True if viewport grid-snap is currently enabled. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsGridSnapEnabled();

	/** Toggle grid-snap on/off. Persists to editor config. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetGridSnapEnabled(bool bEnabled);

	// ─── Autosave settings ───────────────────────────────────

	/** True if the editor autosave timer is currently enabled. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsAutoSaveEnabled();

	/**
	 * Toggle editor autosave. Persists to editor config.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetAutoSaveEnabled(bool bEnabled);

	/**
	 * Autosave interval in minutes (mirrors the value in Editor
	 * Preferences → Loading & Saving → Auto Save). Returns -1 when the
	 * settings object is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetAutoSaveIntervalMinutes();

	/**
	 * Set the autosave interval. Accepts 1..120 minutes. Values outside
	 * that range are rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool SetAutoSaveIntervalMinutes(int32 Minutes);

	// ─── Asset-on-disk metadata ──────────────────────────────

	/**
	 * True when the package resolves to an existing `.uasset` / `.umap`
	 * on disk. Does not load or touch the asset registry — pure filesystem
	 * check. Accepts package paths (`/Game/Foo/Bar`).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool DoesAssetExistOnDisk(const FString& AssetPath);

	/**
	 * Absolute filesystem path of the package's `.uasset` / `.umap`.
	 * Empty string when the path can't be resolved.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetAssetDiskPath(const FString& AssetPath);

	/**
	 * Size on disk in bytes for the package's file. Returns -1 when
	 * the file is missing. Uses `IFileManager::FileSize` — no package
	 * load, no registry read.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int64 GetAssetFileSize(const FString& AssetPath);

	/**
	 * ISO-8601 UTC timestamp of the asset file's last-modified time,
	 * e.g. `"2026-04-15T09:12:33Z"`. Empty string when the file is
	 * missing or the timestamp can't be read.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetAssetLastModifiedTime(const FString& AssetPath);

	// ─── Session identity + timestamp ────────────────────────

	/** OS user name running the editor (FPlatformProcess::UserName). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetOSUserName();

	/** Host machine name (FPlatformProcess::ComputerName). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetMachineName();

	/** ISO-8601 UTC timestamp for "now", e.g. `"2026-04-15T09:12:33Z"`. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetNowUTC();

	/** Process ID of the running editor. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static int32 GetEditorProcessID();

	// ─── Source control basics ───────────────────────────────

	/** True if a source-control provider is both registered and available. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool IsSourceControlEnabled();

	/**
	 * Name of the active source-control provider, e.g. "Perforce",
	 * "Git", "Plastic", "None". Empty string if the module isn't loaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetSourceControlProviderName();

	/**
	 * Synchronous source-control state lookup for an asset. Returns one
	 * of: "CheckedOut" | "NotCheckedOut" | "CheckedOutOther" | "Added" |
	 * "Deleted" | "Ignored" | "NotControlled" | "Unknown". Empty if the
	 * asset can't be resolved or SCC is disabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static FString GetAssetSourceControlState(const FString& AssetPath);

	/**
	 * Try to check out the asset from source control (synchronous).
	 * Returns true on success; false if SCC is disabled, asset can't be
	 * resolved, or the provider rejects the operation.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Editor")
	static bool CheckOutAsset(const FString& AssetPath);
};

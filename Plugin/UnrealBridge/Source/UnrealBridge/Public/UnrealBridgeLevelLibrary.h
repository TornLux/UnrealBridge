#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeLevelLibrary.generated.h"

// ─── Transform ──────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeTransform
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly)
	FVector Scale = FVector::OneVector;
};

// ─── Actor brief (lightweight) ──────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeActorBrief
{
	GENERATED_BODY()

	/** Internal FName */
	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** User-visible label */
	UPROPERTY(BlueprintReadOnly)
	FString Label;

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadOnly)
	bool bHidden = false;
};

// ─── Component info ─────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeLevelComponentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	/** Name of parent component (empty if root / non-scene) */
	UPROPERTY(BlueprintReadOnly)
	FString AttachParent;

	UPROPERTY(BlueprintReadOnly)
	FBridgeTransform RelativeTransform;
};

// ─── Actor info (detailed) ──────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeActorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Label;

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	/** Full path of the actor's class */
	UPROPERTY(BlueprintReadOnly)
	FString ClassPath;

	UPROPERTY(BlueprintReadOnly)
	FBridgeTransform Transform;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Tags;

	/** Parent actor name (empty if not attached) */
	UPROPERTY(BlueprintReadOnly)
	FString AttachedTo;

	/** Attached child actor names */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Children;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeLevelComponentInfo> Components;

	UPROPERTY(BlueprintReadOnly)
	bool bHidden = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHiddenInGame = false;
};

// ─── Level summary ──────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeLevelSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString LevelName;

	UPROPERTY(BlueprintReadOnly)
	FString LevelPath;

	UPROPERTY(BlueprintReadOnly)
	int32 NumActors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NumStreamingLevels = 0;

	/** "Editor", "PIE", "Game" */
	UPROPERTY(BlueprintReadOnly)
	FString WorldType;

	UPROPERTY(BlueprintReadOnly)
	bool bWorldPartition = false;
};

// ─── Streaming level ────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeStreamingLevel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString PackageName;

	UPROPERTY(BlueprintReadOnly)
	bool bLoaded = false;

	UPROPERTY(BlueprintReadOnly)
	bool bVisible = false;
};

// ─── Actor bounds ───────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeActorBounds
{
	GENERATED_BODY()

	/** World-space center of the bounds box. */
	UPROPERTY(BlueprintReadOnly)
	FVector Origin = FVector::ZeroVector;

	/** Half-extents on each axis. */
	UPROPERTY(BlueprintReadOnly)
	FVector BoxExtent = FVector::ZeroVector;

	/** Radius of the bounding sphere. */
	UPROPERTY(BlueprintReadOnly)
	float SphereRadius = 0.f;
};

// ─── Radius hit ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FBridgeActorRadiusHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.f;
};

/**
 * Level / actor introspection via UnrealBridge. Operates on the editor world.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeLevelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ─── Read ─────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeLevelSummary GetLevelSummary();

	/** Count actors passing optional class filter (short name or full path). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetActorCount(const FString& ClassFilter);

	/**
	 * Return actor labels (user-visible names). All filters are optional.
	 * @param ClassFilter  Class short name or full path. Matches the actor's class or any parent.
	 * @param TagFilter    Match actors having this tag.
	 * @param NameFilter   Case-insensitive substring match on the label.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorNames(const FString& ClassFilter, const FString& TagFilter, const FString& NameFilter);

	/**
	 * Detailed list of actors in the level.
	 * ⚠️ Can be large on populated levels. Prefer GetActorNames or restrict with filters.
	 * @param MaxResults  0 = unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FBridgeActorBrief> ListActors(
		const FString& ClassFilter,
		const FString& TagFilter,
		const FString& NameFilter,
		bool bSelectedOnly,
		int32 MaxResults);

	/** Look up actor by FName or label. If duplicate labels exist, returns the first match. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeActorInfo GetActorInfo(const FString& ActorName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeTransform GetActorTransform(const FString& ActorName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FBridgeLevelComponentInfo> GetActorComponents(const FString& ActorName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> FindActorsByClass(const FString& ClassPath, int32 MaxResults);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> FindActorsByTag(const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetSelectedActors();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FBridgeStreamingLevel> GetStreamingLevels();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetCurrentLevelPath();

	// ─── Write ────────────────────────────────────────────

	/**
	 * Spawn an actor from a class path.
	 * @param ClassPath  Full class path ("/Script/Engine.StaticMeshActor") or Blueprint path
	 *                   ("/Game/.../BP_Foo" — `_C` suffix optional).
	 * @return The spawned actor's label, or empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString SpawnActor(const FString& ClassPath, FVector Location, FRotator Rotation);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool DestroyActor(const FString& ActorName);

	/** Destroy many actors in a single undo transaction. Returns count actually destroyed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 DestroyActors(const TArray<FString>& ActorNames);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorTransform(const FString& ActorName, FVector Location, FRotator Rotation, FVector Scale);

	/** Apply delta to current actor transform. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool MoveActor(const FString& ActorName, FVector DeltaLocation, FRotator DeltaRotation);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool AttachActor(const FString& ChildName, const FString& ParentName, const FString& SocketName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool DetachActor(const FString& ActorName);

	/** Select actors in the editor viewport. If bAddToSelection is false, clears selection first. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SelectActors(const TArray<FString>& ActorNames, bool bAddToSelection);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool DeselectAllActors();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorLabel(const FString& ActorName, const FString& NewLabel);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorHiddenInGame(const FString& ActorName, bool bHidden);

	/** Set actor hidden in the editor viewport (bHiddenEd). Distinct from SetActorHiddenInGame. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorHiddenInEditor(const FString& ActorName, bool bHidden);

	/** Add a tag to the actor if not already present. Returns true if added, false if duplicate/missing actor. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool AddActorTag(const FString& ActorName, const FName Tag);

	/** Remove a tag from the actor. Returns true if a tag was removed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool RemoveActorTag(const FString& ActorName, const FName Tag);

	/**
	 * Histogram of actor class short names → count, sorted descending by count.
	 * Lines are formatted "Count\tClassName". Small output — one line per distinct class.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorClassHistogram();

	/**
	 * Return asset paths of materials used by the actor's mesh components (static + skeletal).
	 * Deduplicated. Empty list if the actor has no mesh components or is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorMaterials(const FString& ActorName);

	// ─── Deep queries ─────────────────────────────────────

	/**
	 * Return the exported-text value of a property.
	 * PropertyPath supports dotted nesting into structs and subobjects, e.g.
	 *   "RootComponent.RelativeLocation", "StaticMeshComponent.Mobility".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorProperty(const FString& ActorName, const FString& PropertyPath);

	/**
	 * Set a property from an exported-text value. Dotted path supported.
	 * ⚠️ For transient struct fields (e.g. component RelativeLocation) prefer the
	 * dedicated transform setters — direct struct writes won't trigger component updates.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorProperty(const FString& ActorName, const FString& PropertyPath, const FString& ExportedValue);

	/** Recursive attachment hierarchy, one indented line per descendant. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetAttachmentTree(const FString& ActorName);

	/** Actors within Radius (cm) of Location, distance-sorted. Optional class filter. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FBridgeActorRadiusHit> FindActorsInRadius(FVector Location, float Radius, const FString& ClassFilter);

	/** Duplicate actors; returns labels of new copies. Single undo transaction. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> DuplicateActors(const TArray<FString>& ActorNames);

	// ─── Spatial queries ──────────────────────────────────

	/**
	 * World-space bounds of an actor (all colliding + non-colliding primitives).
	 * Returns zero-bounds if the actor has no renderable/collision geometry.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeActorBounds GetActorBounds(const FString& ActorName);

	/** Labels of actors whose location falls inside the axis-aligned box [Min, Max]. Optional class filter. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorsInBox(FVector Min, FVector Max, const FString& ClassFilter);

	/** Label of the actor nearest to Location, or empty string if none match. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString FindNearestActor(FVector Location, const FString& ClassFilter);

	/** Distance between two actors' world locations (cm). Returns -1 if either is missing. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float GetActorDistance(const FString& ActorA, const FString& ActorB);

	/** True if the given actor is currently selected in the editor viewport. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool IsActorSelected(const FString& ActorName);

	// ─── Folder organization ─────────────────────────────

	/** Return the actor's World Outliner folder path ("" if at root or missing). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorFolder(const FString& ActorName);

	/**
	 * Set the actor's World Outliner folder path. Empty string moves it to the root.
	 * Wrapped in a single undo transaction.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorFolder(const FString& ActorName, const FString& FolderPath);

	/** Return the sorted set of distinct folder paths used by actors in the current level. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorFolders();

	/**
	 * Labels of actors whose folder path matches `FolderPath`.
	 * If `bRecursive` is true, actors in sub-folders ("Foo/Bar" when querying "Foo") are included.
	 * Pass "" to list actors at the outliner root.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorsInFolder(const FString& FolderPath, bool bRecursive);

	// ─── Spatial — trace ─────────────────────────────────
	//
	// All trace / overlap queries below run against the **runtime world** —
	// the live PIE world while Play-in-Editor is active, otherwise the
	// editor world as a fallback. Agents navigating inside PIE therefore
	// see dynamic objects (spawned actors, moving platforms, destructible
	// walls) that only exist in the PIE copy.

	/**
	 * Line-trace against the runtime world (complex collision, visibility channel).
	 * Returns the label of the first actor hit, or empty string if nothing was hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString LineTraceFirstActor(FVector Start, FVector End);

	/**
	 * Line-trace that returns full hit detail: actor label, hit distance in
	 * cm along the ray, and world-space impact location. `bHit` is false
	 * when the ray reaches End without obstruction (in that case Distance
	 * equals the ray length and ImpactLocation equals End).
	 *
	 * Far cheaper than probing at multiple distances to find "reach" — one
	 * call tells you exactly how far the clear corridor extends in that
	 * direction, which is the common agent-survey pattern.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool LineTraceHitInfo(
		FVector Start, FVector End,
		FString& OutActorLabel, float& OutDistance, FVector& OutImpactLocation);

	/**
	 * Multi-hit line trace against the runtime world (visibility channel, complex collision).
	 * Returns deduplicated actor labels along the ray, ordered from nearest to farthest.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> MultiLineTraceActors(FVector Start, FVector End);

	/**
	 * Sphere sweep (fat ray) against the runtime world's visibility channel.
	 * Catches actors a line trace would miss — useful for cover/interest
	 * detection where partial overlap with the ray tube counts as a hit.
	 * Returns the first hit actor's label, or empty string.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString SphereTraceFirstActor(FVector Start, FVector End, float Radius);

	/**
	 * Multi-hit sphere sweep. Returns deduplicated actor labels along the
	 * swept volume, ordered nearest to farthest.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> MultiSphereTraceActors(FVector Start, FVector End, float Radius);

	/**
	 * Axis-aligned box sweep against the runtime world. `BoxHalfExtent` is
	 * the half-size on each axis. Returns the first hit actor's label, or
	 * empty string. For oriented box sweeps call the UE API directly —
	 * this wrapper keeps the Python surface simple.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString BoxTraceFirstActor(FVector Start, FVector End, FVector BoxHalfExtent);

	// ─── 3D geometry sensing ─────────────────────────────────
	//
	// Vertical probes and height sampling — what the horizontal trace family
	// cannot answer: "how tall is the wall in front of me", "is the floor
	// ahead a cliff", "what's the ceiling clearance".

	/**
	 * Downward trace at arbitrary XY. Casts from (X, Y, ZStart) down to
	 * (X, Y, ZEnd). Returns true on hit with `OutGroundZ` set to the Z of
	 * the impact point and `OutActorLabel` set to whatever was hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool GetHeightAt(float X, float Y, float ZStart, float ZEnd,
		FString& OutActorLabel, float& OutGroundZ);

	/**
	 * Sample ground height along a straight XY segment. Returns a parallel
	 * array of ground Z values (one per sample, evenly spaced) and the
	 * actor labels hit at each sample. Missed samples get Z = ZEnd and an
	 * empty label. Classic "walkable?" check: feed the pawn's current path
	 * and look at the delta between consecutive heights — big jumps mean
	 * cliffs, steep rises mean unwalkable slopes.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetHeightProfileAlong(const FVector& StartXY, const FVector& EndXY,
		int32 SampleCount, float ZStart, float ZEnd,
		TArray<float>& OutHeights, TArray<FString>& OutActorLabels);

	/**
	 * Upward trace from `Origin`. Returns the distance (cm) to whatever is
	 * directly above, clamped to `MaxUp`. Returns MaxUp when nothing was
	 * hit — i.e. open sky above. Use to check "can I stand up from crouch
	 * here" (compare to CrouchedHalfHeight vs full CapsuleHalfHeight) or
	 * "can I jump this high".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float MeasureCeilingHeight(const FVector& Origin, float MaxUp);

	/**
	 * Fan out N rays in the XY plane from a single origin. Replaces the
	 * "call line_trace_hit_info N times from Python" pattern — one bridge
	 * round-trip instead of N.
	 *
	 * @param StartAngleDeg   First ray angle (0 = +X, 90 = +Y).
	 * @param SpanDeg         Total arc swept; 360 = full circle.
	 * @param NumRays         Ray count. Evenly distributed across SpanDeg.
	 * @param MaxDistance     Ray length.
	 * @param OutDistances    Hit distance per ray (or MaxDistance if clear).
	 * @param OutActorLabels  First-hit actor label (or "" if clear).
	 * @return number of rays cast.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 ProbeFanXY(const FVector& Origin,
		int32 NumRays, float MaxDistance,
		float StartAngleDeg, float SpanDeg,
		TArray<float>& OutDistances, TArray<FString>& OutActorLabels);

	// ─── NavGraph: persistent exploration topology ───────────────
	//
	// An agent-maintained graph of visited locations and the corridors
	// between them. Lives in a process-global singleton (so it survives
	// across bridge exec calls) and can be serialised to JSON for reuse
	// across sessions.
	//
	// Node names are caller-chosen strings (e.g. "wp_12" or "ledge_north").
	// Edges are directed — add both directions if traversal is symmetric.
	// Missing from/to names are created automatically when AddEdge references
	// them and NodeLocation is provided.

	/** Remove all nodes and edges from the in-memory graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static void NavGraphClear();

	/** Add or update a node. Returns true if this was a new node. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static bool NavGraphAddNode(const FString& Name, const FVector& Location);

	/** Add or update a directed edge `From -> To` with cost (usually
	 *  distance in cm). Returns false if either node is unknown. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static bool NavGraphAddEdge(const FString& From, const FString& To, float Cost);

	/** List all node names currently in the graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static TArray<FString> NavGraphListNodes();

	/** Look up a node's location. Returns zero vector + false if unknown. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static bool NavGraphGetNodeLocation(const FString& Name, FVector& OutLocation);

	/**
	 * Dijkstra shortest-path from `From` to `To` over the current edge set.
	 * Returns the ordered list of node names (inclusive of both endpoints)
	 * and total cost via out-param. Returns empty array when unreachable.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static TArray<FString> NavGraphShortestPath(const FString& From, const FString& To, float& OutTotalCost);

	/** Serialise the graph to a JSON file at the given absolute path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static bool NavGraphSaveJson(const FString& FilePath);

	/** Load a graph from a JSON file, replacing any in-memory graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|NavGraph")
	static bool NavGraphLoadJson(const FString& FilePath);

	// ─── Vision: render-to-file capture ─────────────────────────
	//
	// Synchronously capture a frame from the runtime world to a PNG file.
	// Backed by a transient ASceneCapture2D + UTextureRenderTarget2D, so
	// the capture does not depend on the editor viewport having focus and
	// can be issued from any pose.

	/**
	 * Render a top-down orthographic view centred on `Center`, covering
	 * `WorldSize` cm horizontally, to a PNG file at `FilePath`.
	 *
	 * The camera is placed `CameraHeight` cm above Center looking straight
	 * down. Use this for whole-level / maze overviews the agent can reason
	 * about visually in one shot — the single-best upgrade for navmesh-
	 * free navigation.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|Vision")
	static bool CaptureOrthoTopDown(const FVector& Center, float WorldSize,
		int32 Width, int32 Height, const FString& FilePath,
		float CameraHeight = 5000.0f);

	/**
	 * Render a perspective view from the given pose to a PNG file.
	 * `FOVDeg` = 90 gives a wide FPS-style frame; lower = more zoomed in.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|Vision")
	static bool CaptureFromPose(const FVector& CameraLocation, const FRotator& CameraRotation,
		float FOVDeg, int32 Width, int32 Height, const FString& FilePath);

	/**
	 * Render N views of a skeletal mesh posed at a specific time of an anim
	 * sequence / montage, composited into one PNG grid. The capture runs in a
	 * FPreviewScene — isolated UWorld with default neutral skylight +
	 * directional light — so the project's level lighting, sky, fog, and
	 * stray actors do NOT appear in the frame.
	 *
	 * @param AnimPath          Soft path to UAnimSequenceBase (sequence or montage).
	 * @param Time              Seconds into the anim. Clamped to [0, length].
	 * @param SkeletalMeshPath  Mesh to pose. Empty → use the skeleton's preview
	 *                          mesh (USkeleton::GetPreviewMesh()); fails if absent.
	 * @param Views             Subset of: "Front", "Back", "Side" (right), "SideLeft",
	 *                          "ThreeQuarter" (front-right-elevated), "Top", "Bottom".
	 *                          Unknown names are skipped.
	 * @param bBoneOverlay      Overlay key bone chains (spine / arms / legs)
	 *                          with coloured parent→child line segments and
	 *                          joint dots, drawn into each cell before compositing.
	 *                          Missing bones degrade gracefully (skipped).
	 * @param bPerViewFraming   Each view computes its own target + distance from
	 *                          the posed bone bbox in camera-space; a consensus
	 *                          (max) distance is then applied across views so
	 *                          character scale stays equal. Eliminates dead
	 *                          space on asymmetric poses. Default false → legacy
	 *                          pelvis-anchored framing.
	 * @param bGroundGrid       Project a Z=GroundZ world-XY grid (50 cm spacing)
	 *                          onto each cell so the agent can tell airborne vs
	 *                          grounded at a glance. Centre auto-picked from
	 *                          pelvis / trajectory extent.
	 * @param bRootTrajectory   Densely sample the pelvis bone across the whole
	 *                          anim, project the XY path to Z=GroundZ, draw as
	 *                          a green polyline with tick markers. Tick spacing
	 *                          reads as velocity profile (even=constant,
	 *                          bunched=slow, spread=fast).
	 * @param GroundZ           World-space Z of the ground plane. Default 0.0 —
	 *                          matches the UE convention where the skeleton's
	 *                          reference pose has feet at Z=0.
	 * @param GridCols          Columns in the composite; rows = ceil(N/cols).
	 * @param CellWidth         Pixel width of each view cell (e.g. 512).
	 * @param CellHeight        Pixel height of each view cell (e.g. 512).
	 * @param FilePath          Output PNG path (will create parent dirs).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|Vision")
	static bool CaptureAnimPoseGrid(
		const FString& AnimPath,
		float Time,
		const FString& SkeletalMeshPath,
		const TArray<FString>& Views,
		bool bBoneOverlay,
		bool bPerViewFraming,
		bool bGroundGrid,
		bool bRootTrajectory,
		float GroundZ,
		int32 GridCols,
		int32 CellWidth,
		int32 CellHeight,
		const FString& FilePath);

	/**
	 * Render an anim's timeline as a grid: NumTimeSamples rows × len(Views)
	 * columns, composited into one PNG. Time samples are evenly spaced
	 * across [0, play_length]; views repeat per row with FIXED camera poses
	 * so motion reads left-to-right within a row and top-to-bottom across
	 * rows.
	 *
	 * Camera framing is motion-aware: the function evaluates the pose at
	 * every sample time first to build a union bone AABB covering the
	 * whole timeline, then positions cameras to fit that. Character scale
	 * stays consistent across rows even when the anim translates the pelvis.
	 *
	 * @param AnimPath          Soft path to UAnimSequenceBase.
	 * @param SkeletalMeshPath  Empty → skeleton's preview mesh.
	 * @param NumTimeSamples    Rows; times = 0, L/(N-1), 2L/(N-1), ..., L.
	 *                          N=1 → single row at t=0.
	 * @param Views             Column views; same names as CaptureAnimPoseGrid.
	 * @param bBoneOverlay      Overlay key bone chains (spine / arms / legs)
	 *                          into each cell before compositing.
	 * @param bPerViewFraming   Each view reframes on the union of bone
	 *                          positions across the whole timeline; a consensus
	 *                          distance is applied so scale stays equal across
	 *                          both rows and columns.
	 * @param bGroundGrid       Same as CaptureAnimPoseGrid — ground plane grid
	 *                          at Z=GroundZ.
	 * @param bRootTrajectory   Same as CaptureAnimPoseGrid — pelvis XY path,
	 *                          per-row tick at the sample time, current-row
	 *                          marker highlighted. Tick spacing reads as
	 *                          velocity profile for the whole motion.
	 * @param GroundZ           World-space Z of the ground plane. Default 0.0.
	 * @param CellWidth,CellHeight  Pixel size per cell (e.g. 384).
	 * @param FilePath          Output PNG path (parent dirs auto-created).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level|Vision")
	static bool CaptureAnimMontageTimeline(
		const FString& AnimPath,
		const FString& SkeletalMeshPath,
		int32 NumTimeSamples,
		const TArray<FString>& Views,
		bool bBoneOverlay,
		bool bPerViewFraming,
		bool bGroundGrid,
		bool bRootTrajectory,
		float GroundZ,
		int32 CellWidth,
		int32 CellHeight,
		const FString& FilePath);

	/**
	 * Physics-overlap query: actors whose collision primitives intersect
	 * a sphere at `Center` with `Radius` cm. Distinct from
	 * `FindActorsInRadius`, which tests actor centroids — overlap catches
	 * large actors straddling the sphere even if their pivot is outside.
	 * Results are deduplicated; order matches the query's internal order
	 * (not distance-sorted).
	 *
	 * @param ClassFilter  Optional class short name / full path.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> OverlapSphereActors(FVector Center, float Radius, const FString& ClassFilter);

	// ─── Components / sockets ────────────────────────────

	/**
	 * Return socket names on the actor's scene components, formatted "ComponentName:SocketName".
	 * Useful for discovering valid SocketName arguments for AttachActor or GetSocketWorldTransform.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorSockets(const FString& ActorName);

	/** World transform of a socket on a named scene component. Returns identity if not found. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeTransform GetSocketWorldTransform(const FString& ActorName, const FString& ComponentName, const FName SocketName);

	/** World transform of a scene component by name. Returns identity if not found. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeTransform GetComponentWorldTransform(const FString& ActorName, const FString& ComponentName);

	/**
	 * Toggle visibility of a scene component on the actor.
	 * @param bPropagateToChildren  Cascade to attached child components.
	 * Returns false if the component is missing or not a USceneComponent.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetComponentVisibility(const FString& ActorName, const FString& ComponentName, bool bVisible, bool bPropagateToChildren);

	/**
	 * Set a scene component's mobility.
	 * @param Mobility  "Static", "Stationary", or "Movable" (case-insensitive).
	 * Returns false if the component is missing, not scene, or mobility string is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetComponentMobility(const FString& ActorName, const FString& ComponentName, const FString& Mobility);

	// ─── Bulk transform + level-wide spatial ─────────────────

	/**
	 * Line-trace downward from the actor's current location up to `MaxDistance`
	 * cm and set the actor's Z to the hit surface. The actor is ignored from
	 * the trace so it doesn't self-hit. Leaves X/Y/rotation/scale untouched.
	 * Wrapped in an undo transaction. Returns false on miss, missing actor,
	 * or no editor world.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SnapActorToFloor(const FString& ActorName, float MaxDistance = 10000.0f);

	/**
	 * Quantise each actor's world location to a grid of `GridSize` cm. Rotation
	 * and scale are untouched. Whole batch is a single undo transaction.
	 * @return Number of actors actually moved (existed in level and were non-null).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 SnapActorsToGrid(const TArray<FString>& ActorNames, float GridSize);

	/**
	 * Apply `DeltaLocation` cm to every named actor's world location. Useful
	 * for sliding a selection as a group. Single undo transaction.
	 * @return Number of actors actually offset.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 OffsetActors(const TArray<FString>& ActorNames, FVector DeltaLocation);

	/**
	 * Union of bounds across every actor in the current level (all primitives,
	 * colliding or not). Returns zero-bounds when the level is empty or has
	 * no renderable actors. Use this to size an overview camera or sanity-check
	 * world extent before running a spatial query.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeActorBounds GetLevelBounds();

	// ─── Editor visibility grouping ──────────────────────────
	//
	// All helpers here manipulate the editor-only "bHiddenEd" flag via
	// SetActorHiddenInEditor — they do NOT touch bHiddenInGame and have
	// no runtime effect. Mirrors the "H / Alt+H" hotkey behaviour in the
	// viewport. Every write is wrapped in a single undo transaction.

	/**
	 * Hide every actor in the current level that is NOT in `KeepVisible`.
	 * Matches the viewport "Isolate Selection" gesture. Actors that were
	 * already hidden stay hidden. Returns the count of actors newly hidden.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 IsolateActors(const TArray<FString>& KeepVisible);

	/**
	 * Un-hide every currently-hidden actor in the editor. Returns the count
	 * of actors made visible. Pairs with IsolateActors to restore the scene.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 ShowAllActors();

	/** Labels of actors whose `bHiddenEd` flag is currently set. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetHiddenActorNames();

	/**
	 * Flip each named actor's editor hidden state. Returns the count of
	 * actors successfully toggled (missing names skipped).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 ToggleActorsHidden(const TArray<FString>& ActorNames);

	// ─── Static mesh + material setters ──────────────────────

	/**
	 * Asset path of the UStaticMesh on the actor's first UStaticMeshComponent.
	 * Empty string if the actor has no SMC or the mesh slot is empty.
	 * Pair with `set_actor_mesh` to swap the mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorMesh(const FString& ActorName);

	/**
	 * Swap the mesh on the actor's first UStaticMeshComponent.
	 * @param MeshAssetPath  Full asset path to a UStaticMesh (e.g. "/Game/Meshes/SM_Cube").
	 * Wrapped in a single undo transaction.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorMesh(const FString& ActorName, const FString& MeshAssetPath);

	/**
	 * Override a material slot on the actor's first UMeshComponent
	 * (Static or Skeletal). Passing an empty `MaterialAssetPath` resets
	 * that slot to the mesh's default material.
	 *
	 * @param MaterialIndex    Slot index (0-based).
	 * @param MaterialAssetPath Path to a UMaterialInterface, or "" to reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorMaterial(const FString& ActorName, int32 MaterialIndex, const FString& MaterialAssetPath);

	/**
	 * Restore every overridden material slot on the actor's mesh
	 * components back to the mesh's default materials (i.e. clear all
	 * Material Overrides set via SetActorMaterial or the details panel).
	 * Returns the number of overrides cleared.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 ResetActorMaterials(const FString& ActorName);

	// ─── Collision + physics control ────────────────────────
	//
	// All four helpers operate on the actor's first UPrimitiveComponent
	// (preferring the root when it's a primitive). Every write is a
	// single undo transaction.

	/**
	 * Current collision profile name on the actor's primary primitive
	 * component (e.g. "BlockAll", "NoCollision", "Pawn"). Empty string
	 * when the actor has no primitive component or is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorCollisionProfile(const FString& ActorName);

	/**
	 * Set the collision profile on the actor's primary primitive.
	 * Common presets: "NoCollision", "BlockAll", "BlockAllDynamic",
	 * "OverlapAll", "Pawn", "PhysicsActor", "Vehicle". Profile names
	 * outside the project's `DefaultEngine.ini` list are still accepted
	 * by UE but behave as "NoCollision".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorCollisionProfile(const FString& ActorName, const FString& ProfileName);

	/**
	 * Toggle physics simulation on the actor's primary primitive.
	 * `bSimulate=true` requires the component's Mobility to be Movable
	 * — this helper auto-promotes it if needed (reversed on undo).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorSimulatePhysics(const FString& ActorName, bool bSimulate);

	/**
	 * Actor-level collision enable (cascades to all components via
	 * `AActor::SetActorEnableCollision`). Useful for quickly making an
	 * actor ignore traces without destroying it.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetActorEnableCollision(const FString& ActorName, bool bEnabled);

	// ─── Component add/remove + root query ───────────────────

	/**
	 * Name of the actor's root scene component ("RootComponent" alias
	 * is NOT resolved — returns the component's actual FName). Empty
	 * string if the actor has no root or is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorRootComponentName(const FString& ActorName);

	/**
	 * Instantiate a new component on an editor actor.
	 *
	 * @param ComponentClassPath  Full class path, e.g. `/Script/Engine.PointLightComponent`
	 *                            or a Blueprint component class.
	 * @return  FName of the new component, or empty string on failure.
	 *          The component is registered and tracked as an editor-instance
	 *          component (survives save/reload). Scene components are attached
	 *          to the actor's root.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString AddComponentOfClass(const FString& ActorName, const FString& ComponentClassPath);

	/**
	 * Remove a component from an editor actor by its FName. Only instance
	 * components (not archetype/CDO components) can be removed — trying to
	 * remove a native-CDO component returns false.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool RemoveComponent(const FString& ActorName, const FString& ComponentName);

	/**
	 * Set the relative transform on a named scene component. Leaves
	 * other component fields untouched. Single undo transaction.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetComponentRelativeTransform(
		const FString& ActorName,
		const FString& ComponentName,
		FVector Location,
		FRotator Rotation,
		FVector Scale);

	// ─── Level streaming runtime control ─────────────────────
	//
	// Writer-side companions to GetStreamingLevels. All target the
	// editor world. Toggling load/visibility queues a state change;
	// call FlushLevelStreaming to apply synchronously.

	/**
	 * Request that a streaming sublevel be loaded or unloaded.
	 * Matched by exact `WorldAssetPackageName` (e.g. "/Game/Maps/Sub_Foo").
	 * Returns false if no matching streaming level exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetStreamingLevelLoaded(const FString& PackageName, bool bLoaded);

	/**
	 * Request that a streaming sublevel be visible (only meaningful when
	 * it is also loaded).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetStreamingLevelVisible(const FString& PackageName, bool bVisible);

	/** True if a streaming sublevel is currently loaded in memory. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool IsStreamingLevelLoaded(const FString& PackageName);

	/**
	 * Block the game thread until pending streaming-level load/visibility
	 * state changes apply. Use this after a batch of SetStreamingLevel*
	 * calls if the caller needs the new state reflected immediately
	 * (e.g. before running a spatial query).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool FlushLevelStreaming();

	// ─── World settings: gravity + kill Z ────────────────────
	//
	// All four target the current editor world's AWorldSettings. Writes
	// use FScopedTransaction for undo.

	/**
	 * Effective vertical gravity in cm/s². Reads `AWorldSettings::GetGravityZ()`,
	 * which returns the per-world override if set, otherwise the engine
	 * default (`-980.0` on most projects).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float GetWorldGravity();

	/**
	 * Set the per-world gravity override (WorldGravityZ + bWorldGravitySet).
	 * Passing `bOverride=false` reverts to the engine default — the
	 * `Gravity` value is still stored but not applied until overridden.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetWorldGravity(float Gravity, bool bOverride = true);

	/**
	 * World "kill Z" — any actor whose Z drops below this value is
	 * destroyed (UE's built-in pit-detection). Typical default: -1e6.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float GetKillZ();

	/** Set the world's kill Z. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool SetKillZ(float NewKillZ);

	// ─── Ground / downward trace helpers ─────────────────────
	//
	// Shoots a line trace straight down from a very high Z to find the
	// first visibility-channel hit below an XY point. Useful for
	// placement heuristics and landscape sampling without pulling the
	// Landscape module into the plugin's public API.

	/**
	 * First hit Z below (X, Y) along the downward ray. Returns the hit
	 * point's Z (cm), or -1e6 on miss / no editor world.
	 *
	 * @param StartHeight  Ray origin Z in cm. Default 1e5 (1 km) is
	 *                     enough for most worlds; raise for skyboxes.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float GetGroundHeightAt(float X, float Y, float StartHeight = 100000.0f);

	/**
	 * Surface normal at the first hit below (X, Y). Returns false on miss.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool GetGroundNormalAt(float X, float Y, FVector& OutNormal, float StartHeight = 100000.0f);

	/** Label of the actor hit by the downward trace at (X, Y). Empty on miss. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetGroundHitActor(float X, float Y, float StartHeight = 100000.0f);

	/**
	 * Distance (cm) from the actor's pivot to the first surface below it.
	 * The actor itself is ignored. Returns -1.0 on miss / missing actor.
	 * Editor-world variant of `bridge-gameplay-api.get_pawn_ground_height`.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float GetActorGroundClearance(const FString& ActorName, float MaxDistance = 10000.0f);

	// ─── Actor hierarchy traversal ───────────────────────────

	/**
	 * Labels of all actors attached to this actor at any depth.
	 * Complements `GetAttachmentTree` (formatted text) with a flat list
	 * suitable for iteration. Pre-order (parent's children listed before
	 * grandchildren).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetAllDescendants(const FString& ActorName);

	/**
	 * Labels of actors attached to the same parent as this actor
	 * (excluding this actor itself). Empty if the actor has no parent
	 * or is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorSiblings(const FString& ActorName);

	/**
	 * Label of the topmost ancestor in this actor's attachment chain.
	 * Returns the actor's own label when it has no parent. Empty on
	 * missing actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetRootAttachParent(const FString& ActorName);

	/**
	 * Depth in the attachment tree. 0 = no parent (root), 1 = child of
	 * root, etc. Returns -1 on missing actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetAttachmentDepth(const FString& ActorName);

	// ─── Static-mesh budget stats ────────────────────────────
	//
	// All four sum over every UStaticMeshComponent on the actor (static
	// meshes only — skeletal and Niagara components are skipped). LOD 0
	// is sampled; use the UE Python API for per-LOD detail. Returns -1
	// for missing actor / 0 for actors with no static meshes.

	/** LOD 0 vertex count summed over all StaticMeshComponents. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetActorVertexCount(const FString& ActorName);

	/** LOD 0 triangle count summed over all StaticMeshComponents. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetActorTriangleCount(const FString& ActorName);

	/**
	 * Distinct material-slot count across every mesh component (static +
	 * skeletal). Counts slots — not unique materials — so a component
	 * with a 4-slot mesh contributes 4 even if all slots reference the
	 * same material.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetActorMaterialSlotCount(const FString& ActorName);

	/**
	 * Max LOD count across all StaticMeshComponents on the actor (i.e.
	 * the "best-authored" mesh's LOD depth). Returns 0 for non-SMC actors.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetActorLODCount(const FString& ActorName);

	// ─── Tag bulk ops ────────────────────────────────────────

	/** Sorted unique set of every Tag FName used by any actor in the level. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetAllActorTagsInLevel();

	/** Count of actors carrying the given Tag. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 CountActorsByTag(const FName Tag);

	/**
	 * Select every actor carrying the given Tag in the editor viewport.
	 * @param bAddToSelection  false clears the existing selection first.
	 * @return Count of actors added to selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 SelectActorsByTag(const FName Tag, bool bAddToSelection = false);

	/**
	 * Remove a Tag from every actor in the level that carries it.
	 * Wrapped in a single undo transaction.
	 * @return Count of actors modified.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 RemoveTagFromAllActors(const FName Tag);

	// ─── Actor class introspection ───────────────────────────

	/**
	 * True if the actor is an instance of `ClassPath` or any subclass.
	 * Matches `AActor::IsA` via `ClassPath` (full class path or short
	 * name — uses the same resolver as MatchesClassFilter).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool IsActorOfClass(const FString& ActorName, const FString& ClassPath);

	/**
	 * Immediate superclass of the actor's class (e.g. `"StaticMeshActor"`'s
	 * parent is `"Actor"`). Empty string on missing actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorParentClass(const FString& ActorName);

	/**
	 * Full class-ancestor chain from the actor's class up to `UObject`,
	 * e.g. `["BP_Foo_C", "Foo", "Actor", "Object"]`. Empty on missing actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorClassHierarchy(const FString& ActorName);

	/**
	 * Labels of actors whose class matches `ClassFilter` AND whose Tags
	 * contain `Tag`. Combines the existing class / tag filters with an
	 * AND relation, avoiding a double round-trip.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> FindActorsByClassAndTag(const FString& ClassFilter, const FName Tag);

	// ─── Bulk rotate / scale / mirror ────────────────────────
	//
	// Companions to OffsetActors. All three writers wrap in a single
	// undo transaction and return the count of actors touched.

	/**
	 * Add `DeltaRotation` to each named actor's world rotation.
	 * Combined via `FRotator + FRotator` (additive on each axis; not
	 * quaternion-composed).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 RotateActors(const TArray<FString>& ActorNames, FRotator DeltaRotation);

	/**
	 * Multiply each named actor's world scale by `ScaleMultiplier`
	 * component-wise (e.g. `(2, 2, 2)` doubles uniform scale).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 ScaleActors(const TArray<FString>& ActorNames, FVector ScaleMultiplier);

	/**
	 * Set each named actor's world scale to a uniform value
	 * (same value on all three axes).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 SetActorsUniformScale(const TArray<FString>& ActorNames, float UniformScale);

	/**
	 * Mirror each named actor's location across a world-space plane
	 * through the origin along the named `Axis` ("X"|"Y"|"Z",
	 * case-insensitive). The sign of the actor's scale on that axis is
	 * also flipped so rendered geometry mirrors correctly. Returns count
	 * mirrored; 0 on unknown axis.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 MirrorActors(const TArray<FString>& ActorNames, const FString& Axis);

	// ─── Folder bulk management ──────────────────────────────
	//
	// Companions to the per-actor SetActorFolder / GetActorFolders.
	// All writes wrap in a single undo transaction.

	/** Assign a batch of actors to the same World Outliner folder. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 MoveActorsToFolder(const TArray<FString>& ActorNames, const FString& FolderPath);

	/**
	 * Rename a folder: move every actor whose folder path matches
	 * `OldFolder` (exact match, not recursive) to `NewFolder`. Returns
	 * count moved.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 RenameFolder(const FString& OldFolder, const FString& NewFolder);

	/**
	 * Dissolve a folder: move every actor currently assigned to
	 * `FolderPath` (exact match, not recursive) to the outliner root.
	 * Returns count moved.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 DissolveFolder(const FString& FolderPath);

	/** True if no actor is currently assigned to `FolderPath` (exact match). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool IsFolderEmpty(const FString& FolderPath);

	// ─── Sublevel-scoped actor queries ───────────────────────

	/**
	 * Labels of actors living in a specific sublevel by package name
	 * (e.g. `"/Game/Maps/Sub_Foo"` or the persistent level's package).
	 * Empty when the sublevel isn't loaded or no match is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetActorsInSublevel(const FString& PackageName);

	/** Count of actors in the given sublevel. Cheaper than listing + len(). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 CountActorsInSublevel(const FString& PackageName);

	/**
	 * Package name of the level an actor lives in
	 * (e.g. `"/Game/Maps/MyLevel"`). Empty on missing actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString GetActorLevelPackageName(const FString& ActorName);

	/** Number of actors in the persistent level only (excludes sublevels). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetPersistentLevelActorCount();

	// ─── Bulk selection helpers ──────────────────────────────

	/**
	 * Flip the selection state of every actor in the level. Actors
	 * currently selected become deselected and vice versa. Returns the
	 * count now selected after the flip.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 InvertSelection();

	/** Select every actor in the level. Returns total selected. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 SelectAllActors();

	/**
	 * Select actors whose pivot location falls inside the axis-aligned
	 * box [Min, Max]. `bAddToSelection=false` clears the current selection
	 * first.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 SelectActorsInBox(FVector Min, FVector Max, bool bAddToSelection = false);

	/**
	 * Select actors whose pivot location lies within `Radius` of `Center`.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 SelectActorsInSphere(FVector Center, float Radius, bool bAddToSelection = false);

	// ─── Selection aggregate queries ─────────────────────────

	/**
	 * Count of actors currently selected in the editor viewport.
	 * Faster than `len(GetSelectedActors())` — skips label allocation.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static int32 GetSelectionCount();

	/**
	 * Union of bounds across every selected actor. Zero-bounds when the
	 * selection is empty or no selected actor has renderable geometry.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FBridgeActorBounds GetSelectionBounds();

	/**
	 * Arithmetic mean of selected actors' pivot locations. Zero vector
	 * when the selection is empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FVector GetSelectionCentroid();

	/**
	 * Sorted unique set of class short-names present in the selection
	 * (e.g. `["StaticMeshActor", "PointLight"]`).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> GetSelectionClassSet();

	// ─── Cone / segment geometry helpers ─────────────────────

	/**
	 * Labels of actors whose pivot falls inside a wedge/cone rooted at
	 * `Origin`, facing `Direction`, with half-angle `HalfAngleDeg` and
	 * length `MaxDistance` cm. Matches `FindActorsInRadius` on distance
	 * but adds an angular filter. Not distance-sorted.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> FindActorsInCone(FVector Origin, FVector Direction, float HalfAngleDeg, float MaxDistance, const FString& ClassFilter);

	/**
	 * True when the named actor's pivot is inside the cone at
	 * (`Origin`, `Direction`, `HalfAngleDeg`) within `MaxDistance` cm.
	 * Cheaper than `FindActorsInCone` when caller already has the name.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static bool IsActorInCone(const FString& ActorName, FVector Origin, FVector Direction, float HalfAngleDeg, float MaxDistance);

	/**
	 * Nearest point on the finite line segment (A,B) to `Point`. Useful
	 * for snapping test actors to a ray or measuring to a corridor.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FVector ClosestPointOnSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd);

	/** Perpendicular distance (cm) from `Point` to the finite segment (A,B). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static float DistanceFromPointToSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd);
};

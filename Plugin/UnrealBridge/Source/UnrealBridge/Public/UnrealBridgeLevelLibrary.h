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

	/**
	 * Line-trace against the editor world (complex collision, visibility channel).
	 * Returns the label of the first actor hit, or empty string if nothing was hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString LineTraceFirstActor(FVector Start, FVector End);

	/**
	 * Multi-hit line trace against the editor world (visibility channel, complex collision).
	 * Returns deduplicated actor labels along the ray, ordered from nearest to farthest.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static TArray<FString> MultiLineTraceActors(FVector Start, FVector End);

	/**
	 * Sphere sweep (fat ray) against the editor world's visibility channel.
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
	 * Axis-aligned box sweep against the editor world. `BoxHalfExtent` is
	 * the half-size on each axis. Returns the first hit actor's label, or
	 * empty string. For oriented box sweeps call the UE API directly —
	 * this wrapper keeps the Python surface simple.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Level")
	static FString BoxTraceFirstActor(FVector Start, FVector End, FVector BoxHalfExtent);

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
};

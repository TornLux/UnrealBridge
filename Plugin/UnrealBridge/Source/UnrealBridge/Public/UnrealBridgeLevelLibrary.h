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
};

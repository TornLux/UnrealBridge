#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeAnimLibrary.generated.h"

// ─── State Machine structs ──────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	bool bIsConduit = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	bool bIsDefault = false;
};

USTRUCT(BlueprintType)
struct FBridgeAnimTransition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString FromState;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString ToState;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	bool bBidirectional = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float CrossfadeDuration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 PriorityOrder = 0;
};

USTRUCT(BlueprintType)
struct FBridgeStateMachineInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeAnimState> States;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeAnimTransition> Transitions;
};

// ─── AnimGraph Node structs ─────────────────────────────────

/** Connection between anim graph nodes (pose link). */
USTRUCT(BlueprintType)
struct FBridgeAnimNodeConnection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SourcePin;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 TargetNodeIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString TargetPin;
};

/** A node in the AnimGraph. */
USTRUCT(BlueprintType)
struct FBridgeAnimGraphNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NodeIndex = 0;

	/** Stable address: stringified NodeGuid (Digits form). Use with `get_anim_node_details_by_guid` and the rest of the GUID-based write API — index drifts on graph mutation, GUID does not. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NodeGuid;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NodeTitle;

	/** e.g. "AnimGraphNode_SequencePlayer", "AnimGraphNode_Slot" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NodeType;

	/** Extra detail: referenced anim asset, slot name, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Detail;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeAnimNodeConnection> Connections;
};

// ─── Linked Layer structs ───────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimLayerInfo
{
	GENERATED_BODY()

	/** Interface class name, e.g. "ALI_ItemAnimLayers" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString InterfaceName;

	/** Layer function name, e.g. "Hands", "Arms" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString LayerName;

	/** The ABP class implementing this layer (empty if unset) */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString ImplementingClass;
};

// ─── Slot struct ────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SlotName;

	/** Which graph contains this slot node */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString GraphName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NodeTitle;
};

// ─── Anim Sequence structs ──────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimNotifyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NotifyName;

	/** "Notify" or "NotifyState" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NotifyType;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float TriggerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float Duration = 0.f;
};

USTRUCT(BlueprintType)
struct FBridgeAnimCurveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString CurveName;

	/** Number of keys in the curve */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumKeys = 0;
};

USTRUCT(BlueprintType)
struct FBridgeAnimSequenceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SkeletonName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float PlayLength = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumFrames = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float FrameRate = 30.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float RateScale = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	bool bHasRootMotion = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeAnimNotifyInfo> Notifies;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeAnimCurveInfo> Curves;
};

// ─── Montage structs ────────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeMontageSectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SectionName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float StartTime = 0.f;

	/** Next section name for auto-linking (empty = none) */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString NextSectionName;
};

USTRUCT(BlueprintType)
struct FBridgeMontageInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SkeletonName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float PlayLength = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float BlendInTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float BlendOutTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	bool bEnableAutoBlendOut = true;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeMontageSectionInfo> Sections;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeAnimNotifyInfo> Notifies;
};

// ─── BlendSpace structs ─────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeBlendSpaceAxis
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float Min = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float Max = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 GridDivisions = 4;
};

USTRUCT(BlueprintType)
struct FBridgeBlendSpaceSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString AnimationName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FVector SampleValue = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float RateScale = 1.f;
};

USTRUCT(BlueprintType)
struct FBridgeBlendSpaceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SkeletonName;

	/** 1 for BlendSpace1D, 2 for standard BlendSpace */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumAxes = 2;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeBlendSpaceAxis> Axes;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FBridgeBlendSpaceSample> Samples;
};

// ─── Skeleton structs ───────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeBoneInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString BoneName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 BoneIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 ParentIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString ParentName;
};

// ─── Virtual Bone struct ────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeVirtualBoneInfo
{
	GENERATED_BODY()

	/** Virtual bone name (already includes the "VB " prefix as stored on the skeleton). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString VirtualBoneName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SourceBoneName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString TargetBoneName;
};

// ─── Montage Slot Segment struct ────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeMontageSlotSegment
{
	GENERATED_BODY()

	/** Slot name that owns this segment. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SlotName;

	/** Referenced anim asset path (empty if the segment has no anim). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString AnimReferencePath;

	/** Montage-local start position in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float StartPos = 0.f;

	/** Sub-clip start time inside the referenced anim. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float AnimStartTime = 0.f;

	/** Sub-clip end time inside the referenced anim. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float AnimEndTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float AnimPlayRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 LoopingCount = 1;
};

// ─── Socket struct ──────────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeSocketInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString SocketName;

	/** Bone this socket is attached to */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString ParentBoneName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FVector RelativeScale = FVector::OneVector;
};

// ─── Sync Marker struct ─────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimSyncMarker
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString MarkerName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float Time = 0.f;

	/** Editor-only notify track index (-1 when no editor data). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 TrackIndex = -1;
};

// ─── AnimBlueprint metadata struct ──────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimBlueprintInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	/** Parent class full path (e.g. "/Script/Engine.AnimInstance"). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString ParentClass;

	/** Target skeleton soft object path. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString TargetSkeleton;

	/** Editor preview skeletal mesh soft object path (empty if unset). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString PreviewSkeletalMesh;

	/** True for a Template (skeleton-agnostic) AnimBlueprint. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	bool bIsTemplate = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumStateMachines = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumLinkedLayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumSlots = 0;

	/** Implemented AnimLayer interfaces (class names). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	TArray<FString> ImplementedInterfaces;
};

// ─── AnimGraph layout / write-op structs ────────────────────

/** Summary returned by list_anim_graphs — one row per UEdGraph in the ABP
 *  (top-level AnimGraph + each state-machine graph + each state's BoundGraph
 *  + each transition's rule graph). */
USTRUCT(BlueprintType)
struct FBridgeAnimGraphSummary
{
	GENERATED_BODY()

	/** Graph name as stored on the UEdGraph (use this as GraphName in write ops). */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	/** One of: "AnimGraph", "StateMachine", "State", "Conduit", "TransitionRule", "Function", "Other". */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Kind;

	/** Parent container graph name — "" for top-level graphs. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString ParentGraphName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumNodes = 0;
};

/** Result of the AnimGraph auto-layout. */
USTRUCT(BlueprintType)
struct FBridgeAnimLayoutResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation") bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation") int32 NodesPositioned = 0;

	/** Layer / column count of the final layout. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation") int32 LayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation") int32 BoundsWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation") int32 BoundsHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation") TArray<FString> Warnings;
};

// ─── Blend Profile structs ──────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeBlendProfileInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Name;

	/** "TimeFactor", "WeightFactor", or "BlendMask". */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString Mode;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	int32 NumEntries = 0;
};

USTRUCT(BlueprintType)
struct FBridgeBlendProfileEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	FString BoneName;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Animation")
	float BlendScale = 0.f;
};

// ─── Library class ──────────────────────────────────────────

UCLASS()
class UNREALBRIDGE_API UUnrealBridgeAnimLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Get all state machines in an Animation Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeStateMachineInfo> GetAnimGraphInfo(const FString& AnimBlueprintPath);

	/** Get all nodes in the AnimGraph with connections. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeAnimGraphNodeInfo> GetAnimGraphNodes(const FString& AnimBlueprintPath);

	/** Get linked anim layer bindings. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeAnimLayerInfo> GetAnimLinkedLayers(const FString& AnimBlueprintPath);

	/** Get all Slot nodes with their slot names. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeAnimSlotInfo> GetAnimSlots(const FString& AnimBlueprintPath);

	/** Get non-default properties of a specific anim graph node by its NodeIndex (as returned by GetAnimGraphNodes). LIMITED to the top-level AnimGraph; for state interiors / transition rules / nested function graphs use GetAnimNodeDetailsByGuid. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FString> GetAnimNodeDetails(const FString& AnimBlueprintPath, int32 NodeIndex);

	/**
	 * Get non-default properties of an anim graph node by its NodeGuid + GraphName.
	 * Stable across graph mutations (unlike NodeIndex), and works on ANY graph in the
	 * ABP — top-level AnimGraph, state-machine interiors, state BoundGraphs,
	 * transition-rule graphs, nested function graphs.
	 *
	 * @param GraphName  Graph name as listed by ListAnimGraphs (use "AnimGraph" for the top-level).
	 * @param NodeGuid   Stringified guid (Digits form) — surfaced in FBridgeAnimGraphNodeInfo.NodeGuid
	 *                   and as the return of FindAnimGraphNodeByClass.
	 * @return list of "PropName (CPPType) = T3DValue" lines for non-default properties.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FString> GetAnimNodeDetailsByGuid(const FString& AnimBlueprintPath, const FString& GraphName, const FString& NodeGuid);

	/** Get curves referenced in the AnimGraph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FString> GetAnimCurves(const FString& AnimBlueprintPath);

	/** Get animation sequence info: length, notifies, curves. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FBridgeAnimSequenceInfo GetAnimSequenceInfo(const FString& SequencePath);

	/** Get montage info: sections, slot, blend settings, notifies. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FBridgeMontageInfo GetMontageInfo(const FString& MontagePath);

	/** Get blend space info: axes, samples. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FBridgeBlendSpaceInfo GetBlendSpaceInfo(const FString& BlendSpacePath);

	/** Get skeleton bone hierarchy. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeBoneInfo> GetSkeletonBoneTree(const FString& SkeletonPath);

	/** Get all sockets defined on a skeleton (attach points for weapons, FX, etc.). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeSocketInfo> GetSkeletonSockets(const FString& SkeletonPath);

	// ─── Write ops ───────────────────────────────────────────

	/**
	 * Add a name-only anim notify to an AnimSequence at TriggerTime.
	 * Duration > 0 creates a state notify (without a class); 0 creates an instant notify.
	 * Returns true on success; marks the package dirty on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool AddAnimNotify(const FString& SequencePath, const FString& NotifyName, float TriggerTime, float Duration);

	/** Remove all notifies whose NotifyName matches (case-insensitive). Returns removed count. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static int32 RemoveAnimNotifiesByName(const FString& SequencePath, const FString& NotifyName);

	/** Set RateScale on an AnimSequence. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimSequenceRateScale(const FString& SequencePath, float RateScale);

	/** Add a composite section to a montage. Returns false when name already exists or StartTime invalid. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool AddMontageSection(const FString& MontagePath, const FString& SectionName, float StartTime);

	/** Wire SectionName -> NextSectionName on a montage; pass empty NextSectionName to clear the link. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetMontageSectionNext(const FString& MontagePath, const FString& SectionName, const FString& NextSectionName);

	/** Get the anim segments laid out on each montage slot track (order = layout in the slot). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeMontageSlotSegment> GetMontageSlotSegments(const FString& MontagePath);

	/** Remove a composite section from a montage by name. Returns true when removed; also clears any `NextSectionName` references to it. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RemoveMontageSection(const FString& MontagePath, const FString& SectionName);

	/**
	 * Set montage blend settings.
	 * Negative values leave the corresponding field unchanged (sentinel).
	 * @param BlendInTime       >= 0 to set; < 0 to skip.
	 * @param BlendOutTime      >= 0 to set; < 0 to skip.
	 * @param BlendOutTriggerTime  pass `-1` for "blend out at end"; any other value explicitly overrides the trigger time.
	 * @param bEnableAutoBlendOut  always written.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetMontageBlendTimes(const FString& MontagePath, float BlendInTime, float BlendOutTime, float BlendOutTriggerTime, bool bEnableAutoBlendOut);

	/** Get all virtual bones defined on a skeleton (synthetic bones mapping source->target for animation constraints). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeVirtualBoneInfo> GetSkeletonVirtualBones(const FString& SkeletonPath);

	/**
	 * Add a new socket to a skeleton parented to ParentBoneName.
	 * Fails if the bone does not exist or a socket with the same name is already present.
	 * @param RelativeLocation  offset from parent bone in parent-bone space.
	 * @param RelativeRotation  rotation in parent-bone space.
	 * @param RelativeScale     scale relative to parent (use 1,1,1 for identity).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool AddSkeletonSocket(const FString& SkeletonPath, const FString& SocketName, const FString& ParentBoneName,
		FVector RelativeLocation, FRotator RelativeRotation, FVector RelativeScale);

	/**
	 * List anim assets (AnimSequence / AnimMontage / BlendSpace) bound to a given skeleton via the AssetRegistry.
	 * @param AssetType  "Sequence", "Montage", "BlendSpace", or "" for all three.
	 * @param MaxResults 0 = unlimited.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FString> ListAssetsForSkeleton(const FString& SkeletonPath, const FString& AssetType, int32 MaxResults);

	/** Remove a named socket from a skeleton. Returns true when a socket was removed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RemoveSkeletonSocket(const FString& SkeletonPath, const FString& SocketName);

	/** Get authored sync markers on an AnimSequence (sorted by time). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeAnimSyncMarker> GetAnimSyncMarkers(const FString& SequencePath);

	/** Get all blend profiles defined on a skeleton (name, mode, entry count). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeBlendProfileInfo> GetSkeletonBlendProfiles(const FString& SkeletonPath);

	/** Get the per-bone entries of a specific blend profile on a skeleton. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeBlendProfileEntry> GetBlendProfileEntries(const FString& SkeletonPath, const FString& ProfileName);

	/** Move an existing montage composite section to a new start time. Sections are re-sorted on success. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetMontageSectionStartTime(const FString& MontagePath, const FString& SectionName, float StartTime);

	/**
	 * Append a sync marker to an AnimSequence's AuthoredSyncMarkers.
	 * Rejects empty name and out-of-range time; duplicates (same name+time) are allowed (matches editor behaviour).
	 * Marks the package dirty on success; markers are re-sorted by time.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool AddAnimSyncMarker(const FString& SequencePath, const FString& MarkerName, float Time);

	/** Remove every authored sync marker whose name matches (exact FName compare). Returns removed count. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static int32 RemoveAnimSyncMarkersByName(const FString& SequencePath, const FString& MarkerName);

	/**
	 * Set the transform of an existing socket on a skeleton. Returns false when the socket is missing.
	 * Marks the skeleton package dirty on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetSkeletonSocketTransform(const FString& SkeletonPath, const FString& SocketName,
		FVector RelativeLocation, FRotator RelativeRotation, FVector RelativeScale);

	/**
	 * Rename an existing socket on a skeleton. Fails when the old socket is missing or the new name is empty / already used.
	 * Marks the skeleton package dirty on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RenameSkeletonSocket(const FString& SkeletonPath, const FString& OldName, const FString& NewName);

	/** Get high-level AnimBlueprint metadata: parent class, target skeleton, preview mesh, graph counts, interfaces. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FBridgeAnimBlueprintInfo GetAnimBlueprintInfo(const FString& AnimBlueprintPath);

	// ─── AnimGraph / state machine WRITE ops ─────────────────

	/**
	 * Enumerate every UEdGraph inside the AnimBlueprint and classify it:
	 * top-level "AnimGraph", a state-machine graph, a state's BoundGraph,
	 * a conduit, a transition rule graph, a regular K2 function, etc.
	 *
	 * Use the `Name` field of the returned entries as the `GraphName` parameter
	 * to every other write op. ParentGraphName locates where each sub-graph
	 * lives (e.g. a state's BoundGraph lists its owning state-machine graph
	 * as parent, so you can reconstruct the hierarchy client-side).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FBridgeAnimGraphSummary> ListAnimGraphs(const FString& AnimBlueprintPath);

	/**
	 * List (node_guid, node_class_short_name, node_title) for every node in
	 * the named graph. Unlike UnrealBridgeBlueprintLibrary.get_function_nodes
	 * (which only walks top-level FunctionGraphs), this uses the same
	 * recursive graph resolver as every other anim write op — so it works on
	 * state machine interiors, state BoundGraphs, and transition rule graphs.
	 *
	 * Returned rows: `"<guid>\t<class>\t<title>"` — lightweight tab-separated
	 * to avoid introducing yet another USTRUCT for a basic listing.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FString> ListAnimGraphNodes(const FString& AnimBlueprintPath, const FString& GraphName);

	/**
	 * Convenience: return the GUID of the first node of class `ShortClassName`
	 * (e.g. "AnimGraphNode_Root", "AnimGraphNode_StateResult",
	 * "AnimGraphNode_TransitionResult", "AnimStateEntryNode") inside GraphName.
	 * Empty string if no match or graph not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString FindAnimGraphNodeByClass(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& ShortClassName);

	/**
	 * Spawn a UAnimGraphNode_SequencePlayer in `GraphName`, bound to SequencePath.
	 * GraphName may be the top-level "AnimGraph" or any state's BoundGraph name
	 * (from ListAnimGraphs). Returns the new node's GUID (empty on failure).
	 *
	 * SequencePath may be empty — the node is created with no anim asset.
	 * Does NOT auto-compile; call compile_blueprint after a batch.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeSequencePlayer(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& SequencePath, int32 PosX, int32 PosY);

	/**
	 * Spawn a UAnimGraphNode_BlendSpacePlayer in `GraphName`, bound to BlendSpacePath.
	 * Accepts both 1D and 2D blend spaces.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeBlendSpacePlayer(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& BlendSpacePath, int32 PosX, int32 PosY);

	/**
	 * Spawn a UAnimGraphNode_Slot with the given slot name.
	 * The slot will be registered on the target skeleton if it isn't already.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeSlot(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& SlotName, int32 PosX, int32 PosY);

	/**
	 * Spawn a UAnimGraphNode_StateMachine and auto-create its interior
	 * UAnimationStateMachineGraph (renamed to StateMachineName; uniquified if
	 * taken). The interior graph's name is what later write ops use to
	 * target states/transitions inside it. Returns the outer node's GUID.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeStateMachine(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& StateMachineName, int32 PosX, int32 PosY);

	/** Spawn a UAnimGraphNode_LayeredBoneBlend with the requested number of extra blend poses (min 1). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeLayeredBoneBlend(const FString& AnimBlueprintPath, const FString& GraphName,
		int32 NumBlendPoses, int32 PosX, int32 PosY);

	/** Spawn a UAnimGraphNode_BlendListByBool. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeBlendListByBool(const FString& AnimBlueprintPath, const FString& GraphName,
		int32 PosX, int32 PosY);

	/** Spawn a UAnimGraphNode_BlendListByInt with the requested number of poses (min 2). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeBlendListByInt(const FString& AnimBlueprintPath, const FString& GraphName,
		int32 NumPoses, int32 PosX, int32 PosY);

	/** Spawn a UAnimGraphNode_TwoWayBlend (FAnimNode_TwoWayBlend — field is `BlendNode`, not `Node`). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeTwoWayBlend(const FString& AnimBlueprintPath, const FString& GraphName,
		int32 PosX, int32 PosY);

	/**
	 * Spawn a UAnimGraphNode_LinkedAnimLayer bound to (InterfaceClassPath, LayerName).
	 * Call ReconstructNode() so the linked layer's custom pins appear.
	 * InterfaceClassPath may be empty for a layer on the same ABP.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeLinkedAnimLayer(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& InterfaceClassPath, const FString& LayerName, int32 PosX, int32 PosY);

	/**
	 * Generic AnimGraph node factory by short class name (e.g. "AnimGraphNode_ApplyAdditive",
	 * "AnimGraphNode_ModifyBone", "AnimGraphNode_ObserveBone", "AnimGraphNode_SaveCachedPose",
	 * "AnimGraphNode_UseCachedPose", "AnimGraphNode_Root", "AnimGraphNode_ConvertComponentToLocalSpace"
	 * etc). Fallback when the typed helpers don't cover the desired node.
	 * Returns the new node's GUID, or "" on failure (class not found / not a UAnimGraphNode_Base subclass).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimGraphNodeByClassName(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& ShortClassName, int32 PosX, int32 PosY);

	/**
	 * Connect two pins inside an AnimGraph-family graph (includes pose-link pins,
	 * state-transition "In"/"Out" pins, and any K2 pin inside a transition rule
	 * or state's anim graph). Uses the owning graph's schema — the anim schema
	 * handles local↔component pose conversion automatically.
	 * Returns true when the link was created.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool ConnectAnimGraphPins(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& SourceNodeGuid, const FString& SourcePinName,
		const FString& TargetNodeGuid, const FString& TargetPinName);

	/** Break every link on a single named pin of a node. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool DisconnectAnimGraphPin(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& PinName);

	/** Remove a node from an AnimGraph-family graph by GUID. Returns true if a node was removed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RemoveAnimGraphNode(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& NodeGuid);

	/** Set the (NodePosX, NodePosY) of a node in an AnimGraph-family graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimGraphNodePosition(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& NodeGuid, int32 PosX, int32 PosY);

	/** Change a SequencePlayer's bound sequence. Empty path clears the binding. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimSequencePlayerSequence(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& SequencePath);

	/** Change a Slot node's slot name. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimSlotName(const FString& AnimBlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& SlotName);

	// ─── State machine interior write ops ────────────────────

	/**
	 * Add a new state inside the given state-machine graph. The state's
	 * BoundGraph (a UAnimationStateGraph containing a StateResult node) is
	 * auto-created and renamed to StateName (uniquified if taken).
	 * Returns the new AnimStateNode's GUID.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimState(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& StateName, int32 PosX, int32 PosY);

	/**
	 * Add a conduit (branching helper state) inside a state-machine graph.
	 * Its BoundGraph is a bool rule graph rather than an anim graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimConduit(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& ConduitName, int32 PosX, int32 PosY);

	/**
	 * Create a transition edge From -> To (states or conduits by name) inside
	 * a state-machine graph. The transition's rule graph is auto-created.
	 * Returns the transition node's GUID.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString AddAnimTransition(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& FromStateName, const FString& ToStateName);

	/** Remove a named state (or conduit) from a state-machine graph. Cleans up attached transitions. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RemoveAnimState(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& StateName);

	/** Remove the transition From -> To (or first match, when multiple exist). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RemoveAnimTransition(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& FromStateName, const FString& ToStateName);

	/**
	 * Get the name of the transition rule graph for the From -> To edge.
	 * Empty string if the transition doesn't exist.
	 *
	 * Use this as the `graph_name` argument to the Blueprint library when
	 * authoring K2 logic inside the rule (variable reads, comparisons,
	 * boolean ops, etc.). `AddAnimTransition` names rule graphs uniquely
	 * as `Rule_<From>_to_<To>` so this accessor returns a stable handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FString GetAnimTransitionRuleGraphName(const FString& AnimBlueprintPath,
		const FString& StateMachineGraphName,
		const FString& FromStateName, const FString& ToStateName);

	/**
	 * Set the default entry state: links StateMachineGraph->EntryNode's output
	 * pin to StateName's input pin, clearing any prior default link.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimStateDefault(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& StateName);

	/** Rename a state by renaming its BoundGraph (which is the authoritative state name). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool RenameAnimState(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& OldName, const FString& NewName);

	/**
	 * Tune a transition's blend properties.
	 * `CrossfadeDuration` negative = leave unchanged.
	 * `PriorityOrder` INT_MIN (i.e. -2147483648) = leave unchanged.
	 * `bBidirectional` is always written.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimTransitionProperties(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& FromStateName, const FString& ToStateName,
		float CrossfadeDuration, int32 PriorityOrder, bool bBidirectional);

	/**
	 * Set a transition rule to a constant boolean. Replaces the rule-graph
	 * result node's bool input with a literal value — convenient when you
	 * just need "always transition" or "never" without authoring nodes.
	 * For anything more complex, query the transition's rule-graph name via
	 * ListAnimGraphs and use the BP library to author nodes inside it.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static bool SetAnimTransitionConstRule(const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
		const FString& FromStateName, const FString& ToStateName, bool bValue);

	// ─── Layout ──────────────────────────────────────────────

	/**
	 * Auto-layout an AnimGraph (pose-link flow). Nodes are layered L→R by the
	 * pose-link DAG: sinks (AnimGraphNode_Root / StateResult / TransitionResult)
	 * on the right, leaf sources on the left. Within each layer, nodes are
	 * sorted by barycentric order of their successors. Mirrors the BP
	 * Sugiyama-lite layout but tuned for anim graphs.
	 *
	 * @param HorizontalSpacing  Gap between layers. 0 or negative = default (100).
	 * @param VerticalSpacing    Gap between nodes within a layer. 0 or negative = default (60).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FBridgeAnimLayoutResult AutoLayoutAnimGraph(const FString& AnimBlueprintPath, const FString& GraphName,
		int32 HorizontalSpacing, int32 VerticalSpacing);

	/**
	 * Auto-layout the *interior* of a state-machine graph. States are arranged
	 * in a 2-D grid (roughly square). Conduits interleave with states. The
	 * entry node is pinned at the top-left. Transitions follow along.
	 *
	 * Additionally runs AutoLayoutAnimGraph on every state's BoundGraph + every
	 * transition's rule graph, so one call tidies the whole state machine.
	 *
	 * @param HorizontalSpacing  Gap between state columns. 0 or negative = default (300).
	 * @param VerticalSpacing    Gap between state rows. 0 or negative = default (200).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static FBridgeAnimLayoutResult AutoLayoutStateMachine(const FString& AnimBlueprintPath,
		const FString& StateMachineGraphName,
		int32 HorizontalSpacing, int32 VerticalSpacing);
};

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeAnimLibrary.generated.h"

// ─── State Machine structs ──────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	bool bIsConduit = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsDefault = false;
};

USTRUCT(BlueprintType)
struct FBridgeAnimTransition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString FromState;

	UPROPERTY(BlueprintReadOnly)
	FString ToState;

	UPROPERTY(BlueprintReadOnly)
	bool bBidirectional = false;

	UPROPERTY(BlueprintReadOnly)
	float CrossfadeDuration = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 PriorityOrder = 0;
};

USTRUCT(BlueprintType)
struct FBridgeStateMachineInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAnimState> States;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAnimTransition> Transitions;
};

// ─── AnimGraph Node structs ─────────────────────────────────

/** Connection between anim graph nodes (pose link). */
USTRUCT(BlueprintType)
struct FBridgeAnimNodeConnection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SourcePin;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetNodeIndex = -1;

	UPROPERTY(BlueprintReadOnly)
	FString TargetPin;
};

/** A node in the AnimGraph. */
USTRUCT(BlueprintType)
struct FBridgeAnimGraphNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 NodeIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	FString NodeTitle;

	/** e.g. "AnimGraphNode_SequencePlayer", "AnimGraphNode_Slot" */
	UPROPERTY(BlueprintReadOnly)
	FString NodeType;

	/** Extra detail: referenced anim asset, slot name, etc. */
	UPROPERTY(BlueprintReadOnly)
	FString Detail;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAnimNodeConnection> Connections;
};

// ─── Linked Layer structs ───────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimLayerInfo
{
	GENERATED_BODY()

	/** Interface class name, e.g. "ALI_ItemAnimLayers" */
	UPROPERTY(BlueprintReadOnly)
	FString InterfaceName;

	/** Layer function name, e.g. "Hands", "Arms" */
	UPROPERTY(BlueprintReadOnly)
	FString LayerName;

	/** The ABP class implementing this layer (empty if unset) */
	UPROPERTY(BlueprintReadOnly)
	FString ImplementingClass;
};

// ─── Slot struct ────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SlotName;

	/** Which graph contains this slot node */
	UPROPERTY(BlueprintReadOnly)
	FString GraphName;

	UPROPERTY(BlueprintReadOnly)
	FString NodeTitle;
};

// ─── Anim Sequence structs ──────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeAnimNotifyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString NotifyName;

	/** "Notify" or "NotifyState" */
	UPROPERTY(BlueprintReadOnly)
	FString NotifyType;

	UPROPERTY(BlueprintReadOnly)
	float TriggerTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float Duration = 0.f;
};

USTRUCT(BlueprintType)
struct FBridgeAnimCurveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString CurveName;

	/** Number of keys in the curve */
	UPROPERTY(BlueprintReadOnly)
	int32 NumKeys = 0;
};

USTRUCT(BlueprintType)
struct FBridgeAnimSequenceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString SkeletonName;

	UPROPERTY(BlueprintReadOnly)
	float PlayLength = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 NumFrames = 0;

	UPROPERTY(BlueprintReadOnly)
	float FrameRate = 30.f;

	UPROPERTY(BlueprintReadOnly)
	float RateScale = 1.f;

	UPROPERTY(BlueprintReadOnly)
	bool bHasRootMotion = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAnimNotifyInfo> Notifies;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAnimCurveInfo> Curves;
};

// ─── Montage structs ────────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeMontageSectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SectionName;

	UPROPERTY(BlueprintReadOnly)
	float StartTime = 0.f;

	/** Next section name for auto-linking (empty = none) */
	UPROPERTY(BlueprintReadOnly)
	FString NextSectionName;
};

USTRUCT(BlueprintType)
struct FBridgeMontageInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString SkeletonName;

	UPROPERTY(BlueprintReadOnly)
	float PlayLength = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FString SlotName;

	UPROPERTY(BlueprintReadOnly)
	float BlendInTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float BlendOutTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bEnableAutoBlendOut = true;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeMontageSectionInfo> Sections;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeAnimNotifyInfo> Notifies;
};

// ─── BlendSpace structs ─────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeBlendSpaceAxis
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	float Min = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float Max = 100.f;

	UPROPERTY(BlueprintReadOnly)
	int32 GridDivisions = 4;
};

USTRUCT(BlueprintType)
struct FBridgeBlendSpaceSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString AnimationName;

	UPROPERTY(BlueprintReadOnly)
	FVector SampleValue = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float RateScale = 1.f;
};

USTRUCT(BlueprintType)
struct FBridgeBlendSpaceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString SkeletonName;

	/** 1 for BlendSpace1D, 2 for standard BlendSpace */
	UPROPERTY(BlueprintReadOnly)
	int32 NumAxes = 2;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeBlendSpaceAxis> Axes;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeBlendSpaceSample> Samples;
};

// ─── Skeleton structs ───────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeBoneInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString BoneName;

	UPROPERTY(BlueprintReadOnly)
	int32 BoneIndex = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 ParentIndex = -1;

	UPROPERTY(BlueprintReadOnly)
	FString ParentName;
};

// ─── Virtual Bone struct ────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeVirtualBoneInfo
{
	GENERATED_BODY()

	/** Virtual bone name (already includes the "VB " prefix as stored on the skeleton). */
	UPROPERTY(BlueprintReadOnly)
	FString VirtualBoneName;

	UPROPERTY(BlueprintReadOnly)
	FString SourceBoneName;

	UPROPERTY(BlueprintReadOnly)
	FString TargetBoneName;
};

// ─── Montage Slot Segment struct ────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeMontageSlotSegment
{
	GENERATED_BODY()

	/** Slot name that owns this segment. */
	UPROPERTY(BlueprintReadOnly)
	FString SlotName;

	/** Referenced anim asset path (empty if the segment has no anim). */
	UPROPERTY(BlueprintReadOnly)
	FString AnimReferencePath;

	/** Montage-local start position in seconds. */
	UPROPERTY(BlueprintReadOnly)
	float StartPos = 0.f;

	/** Sub-clip start time inside the referenced anim. */
	UPROPERTY(BlueprintReadOnly)
	float AnimStartTime = 0.f;

	/** Sub-clip end time inside the referenced anim. */
	UPROPERTY(BlueprintReadOnly)
	float AnimEndTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float AnimPlayRate = 1.f;

	UPROPERTY(BlueprintReadOnly)
	int32 LoopingCount = 1;
};

// ─── Socket struct ──────────────────────────────────────────

USTRUCT(BlueprintType)
struct FBridgeSocketInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SocketName;

	/** Bone this socket is attached to */
	UPROPERTY(BlueprintReadOnly)
	FString ParentBoneName;

	UPROPERTY(BlueprintReadOnly)
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly)
	FVector RelativeScale = FVector::OneVector;
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

	/** Get non-default properties of a specific anim graph node by its NodeIndex (as returned by GetAnimGraphNodes). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Animation")
	static TArray<FString> GetAnimNodeDetails(const FString& AnimBlueprintPath, int32 NodeIndex);

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
};

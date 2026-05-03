#include "UnrealBridgeAnimLibrary.h"
#include "Misc/EngineVersionComparison.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Animation/AnimCurveTypes.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateConduitNode.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Internationalization/Text.h"
#include "Animation/BlendProfile.h"
#include "Animation/AnimTypes.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeAnim"

// ─── Helpers ────────────────────────────────────────────────

namespace BridgeAnimImpl
{
	/**
	 * Get the untranslated (source / English) form of a node title.
	 * FText::BuildSourceString() walks any FText::Format composition
	 * and returns the source string for each part — including arguments.
	 * This avoids issues with UE Chinese localization on node titles.
	 */
	FString GetNodeTitleSource(UEdGraphNode* Node, ENodeTitleType::Type TitleType = ENodeTitleType::FullTitle)
	{
		if (!Node) return FString();
		return Node->GetNodeTitle(TitleType).BuildSourceString();
	}

	UAnimBlueprint* LoadABP(const FString& Path)
	{
		UAnimBlueprint* ABP = LoadObject<UAnimBlueprint>(nullptr, *Path);
		if (!ABP)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load Anim Blueprint '%s'"), *Path);
		return ABP;
	}

	// Find the AnimGraph among FunctionGraphs
	UEdGraph* FindAnimGraph(UAnimBlueprint* ABP)
	{
		for (UEdGraph* G : ABP->FunctionGraphs)
		{
			if (G && G->GetName() == TEXT("AnimGraph"))
				return G;
		}
		return nullptr;
	}

	void GatherNotifies(const UAnimSequenceBase* Seq, TArray<FBridgeAnimNotifyInfo>& Out)
	{
		for (const FAnimNotifyEvent& N : Seq->Notifies)
		{
			FBridgeAnimNotifyInfo Info;
			if (N.Notify)
				Info.NotifyName = N.Notify->GetClass()->GetName();
			else if (N.NotifyStateClass)
				Info.NotifyName = N.NotifyStateClass->GetClass()->GetName();
			else
				Info.NotifyName = N.NotifyName.ToString();

			Info.NotifyType = N.NotifyStateClass ? TEXT("NotifyState") : TEXT("Notify");
			Info.TriggerTime = N.GetTriggerTime();
			Info.Duration = N.GetDuration();
			Out.Add(Info);
		}
	}
}

// ─── GetAnimGraphInfo (State Machines) ──────────────────────

TArray<FBridgeStateMachineInfo> UUnrealBridgeAnimLibrary::GetAnimGraphInfo(const FString& AnimBlueprintPath)
{
	TArray<FBridgeStateMachineInfo> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachineBase* SMNode = Cast<UAnimGraphNode_StateMachineBase>(Node);
			if (!SMNode) continue;

			UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
			if (!SMGraph) continue;

			FBridgeStateMachineInfo SMInfo;
			SMInfo.Name = BridgeAnimImpl::GetNodeTitleSource(SMNode);

			FString DefaultStateName;
			for (UEdGraphNode* SMGraphNode : SMGraph->Nodes)
			{
				if (UAnimStateEntryNode* Entry = Cast<UAnimStateEntryNode>(SMGraphNode))
				{
					if (Entry->Pins.Num() > 0 && Entry->Pins[0]->LinkedTo.Num() > 0)
					{
						if (UAnimStateNode* DS = Cast<UAnimStateNode>(Entry->Pins[0]->LinkedTo[0]->GetOwningNode()))
							DefaultStateName = DS->GetStateName();
					}
					break;
				}
			}

			for (UEdGraphNode* SMGraphNode : SMGraph->Nodes)
			{
				if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(SMGraphNode))
				{
					FBridgeAnimState S;
					S.Name = StateNode->GetStateName();
					S.bIsConduit = false;
					S.bIsDefault = (S.Name == DefaultStateName);
					SMInfo.States.Add(S);
				}
				else if (UAnimStateConduitNode* Conduit = Cast<UAnimStateConduitNode>(SMGraphNode))
				{
					FBridgeAnimState S;
					S.Name = Conduit->GetStateName();
					S.bIsConduit = true;
					SMInfo.States.Add(S);
				}
				else if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(SMGraphNode))
				{
					FBridgeAnimTransition T;
					if (UAnimStateNode* Prev = Cast<UAnimStateNode>(Trans->GetPreviousState()))
						T.FromState = Prev->GetStateName();
					else if (UAnimStateConduitNode* PC = Cast<UAnimStateConduitNode>(Trans->GetPreviousState()))
						T.FromState = PC->GetStateName();

					if (UAnimStateNode* Next = Cast<UAnimStateNode>(Trans->GetNextState()))
						T.ToState = Next->GetStateName();
					else if (UAnimStateConduitNode* NC = Cast<UAnimStateConduitNode>(Trans->GetNextState()))
						T.ToState = NC->GetStateName();

					T.bBidirectional = Trans->Bidirectional;
					T.CrossfadeDuration = Trans->CrossfadeDuration;
					T.PriorityOrder = Trans->PriorityOrder;
					SMInfo.Transitions.Add(T);
				}
			}
			Result.Add(SMInfo);
		}
	}
	return Result;
}

// ─── GetAnimGraphNodes ──────────────────────────────────────

TArray<FBridgeAnimGraphNodeInfo> UUnrealBridgeAnimLibrary::GetAnimGraphNodes(const FString& AnimBlueprintPath)
{
	TArray<FBridgeAnimGraphNodeInfo> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	UEdGraph* AnimGraph = BridgeAnimImpl::FindAnimGraph(ABP);
	if (!AnimGraph) return Result;

	// Build node index map
	TMap<UEdGraphNode*, int32> NodeIndexMap;
	for (int32 i = 0; i < AnimGraph->Nodes.Num(); ++i)
	{
		if (AnimGraph->Nodes[i])
			NodeIndexMap.Add(AnimGraph->Nodes[i], i);
	}

	for (int32 i = 0; i < AnimGraph->Nodes.Num(); ++i)
	{
		UEdGraphNode* Node = AnimGraph->Nodes[i];
		if (!Node) continue;

		FBridgeAnimGraphNodeInfo Info;
		Info.NodeIndex = i;
		Info.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		Info.NodeTitle = BridgeAnimImpl::GetNodeTitleSource(Node);
		Info.NodeType = Node->GetClass()->GetName();

		// Extract detail for known types
		if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
		{
			// Try to get referenced animation asset
			if (FAnimNode_Base* FNode = AnimNode->GetFNode())
			{
				// Use the node's description text for detail (untranslated)
				Info.Detail = BridgeAnimImpl::GetNodeTitleSource(AnimNode, ENodeTitleType::ListView);
			}
		}

		// Build connections: look at output pins connected to input pins of other nodes
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output) continue;

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin) continue;
				UEdGraphNode* TargetNode = LinkedPin->GetOwningNode();
				if (!TargetNode) continue;

				int32* TargetIdx = NodeIndexMap.Find(TargetNode);

				FBridgeAnimNodeConnection Conn;
				Conn.SourcePin = Pin->GetName();
				Conn.TargetNodeIndex = TargetIdx ? *TargetIdx : -1;
				Conn.TargetPin = LinkedPin->GetName();
				Info.Connections.Add(Conn);
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── GetAnimLinkedLayers ────────────────────────────────────

TArray<FBridgeAnimLayerInfo> UUnrealBridgeAnimLibrary::GetAnimLinkedLayers(const FString& AnimBlueprintPath)
{
	TArray<FBridgeAnimLayerInfo> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_LinkedAnimLayer* LayerNode = Cast<UAnimGraphNode_LinkedAnimLayer>(Node);
			if (!LayerNode) continue;

			FBridgeAnimLayerInfo Info;

			// Parse from node title (format: "InterfaceName - LayerName"), use source text
			FString FullTitle = BridgeAnimImpl::GetNodeTitleSource(LayerNode);
			FString Left, Right;
			if (FullTitle.Split(TEXT(" - "), &Left, &Right))
			{
				Info.InterfaceName = Left.TrimStartAndEnd();
				Info.LayerName = Right.TrimStartAndEnd();
			}
			else
			{
				Info.LayerName = FullTitle;
			}

			Info.ImplementingClass = FullTitle;

			Result.Add(Info);
		}
	}

	return Result;
}

// ─── GetAnimSlots ───────────────────────────────────────────

TArray<FBridgeAnimSlotInfo> UUnrealBridgeAnimLibrary::GetAnimSlots(const FString& AnimBlueprintPath)
{
	TArray<FBridgeAnimSlotInfo> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_Slot* SlotNode = Cast<UAnimGraphNode_Slot>(Node);
			if (!SlotNode) continue;

			FBridgeAnimSlotInfo Info;
			Info.GraphName = Graph->GetName();
			Info.NodeTitle = BridgeAnimImpl::GetNodeTitleSource(SlotNode);

			// Access the internal FAnimNode_Slot to get SlotName
			FStructProperty* NodeProp = SlotNode->GetFNodeProperty();
			if (NodeProp)
			{
				FAnimNode_Slot* InternalNode = NodeProp->ContainerPtrToValuePtr<FAnimNode_Slot>(SlotNode);
				if (InternalNode)
				{
					Info.SlotName = InternalNode->SlotName.ToString();
				}
			}

			Result.Add(Info);
		}
	}

	return Result;
}

// ─── GetAnimNodeDetails ─────────────────────────────────────

// Forward decls — actual definitions live later in the BridgeAnimWriteImpl namespace block.
namespace BridgeAnimWriteImpl
{
	UEdGraph* FindAnyGraphByName(UAnimBlueprint* ABP, const FString& GraphName);
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& NodeGuid);
}

namespace BridgeAnimImpl
{
	/** Dump non-default UPROPERTY values on the FAnimNode struct backing an UAnimGraphNode_Base. */
	void DumpAnimNodeProperties(UAnimGraphNode_Base* AnimNode, TArray<FString>& Out)
	{
		FAnimNode_Base* FNode = AnimNode ? AnimNode->GetFNode() : nullptr;
		UScriptStruct* NodeStruct = AnimNode ? AnimNode->GetFNodeType() : nullptr;
		if (!FNode || !NodeStruct) return;

		TArray<uint8> DefaultData;
		DefaultData.SetNumZeroed(NodeStruct->GetStructureSize());
		NodeStruct->InitializeStruct(DefaultData.GetData());

		for (TFieldIterator<FProperty> It(NodeStruct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(FNode);
			void* DefaultPtr = Prop->ContainerPtrToValuePtr<void>(DefaultData.GetData());

			if (!Prop->Identical(ValuePtr, DefaultPtr))
			{
				FString ExportedValue;
				Prop->ExportTextItem_Direct(ExportedValue, ValuePtr, DefaultPtr, nullptr, PPF_None);
				Out.Add(FString::Printf(TEXT("%s (%s) = %s"),
					*Prop->GetName(), *Prop->GetCPPType(), *ExportedValue));
			}
		}

		NodeStruct->DestroyStruct(DefaultData.GetData());
	}
}

TArray<FString> UUnrealBridgeAnimLibrary::GetAnimNodeDetails(
	const FString& AnimBlueprintPath, int32 NodeIndex)
{
	TArray<FString> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	UEdGraph* AnimGraph = BridgeAnimImpl::FindAnimGraph(ABP);
	if (!AnimGraph) return Result;

	if (NodeIndex < 0 || NodeIndex >= AnimGraph->Nodes.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: NodeIndex %d out of range [0, %d)"),
			NodeIndex, AnimGraph->Nodes.Num());
		return Result;
	}

	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(AnimGraph->Nodes[NodeIndex]);
	if (!AnimNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Node at index %d is not an AnimGraphNode"), NodeIndex);
		return Result;
	}

	BridgeAnimImpl::DumpAnimNodeProperties(AnimNode, Result);
	return Result;
}

TArray<FString> UUnrealBridgeAnimLibrary::GetAnimNodeDetailsByGuid(
	const FString& AnimBlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	TArray<FString> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	UEdGraph* Graph = BridgeAnimWriteImpl::FindAnyGraphByName(ABP, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetAnimNodeDetailsByGuid graph '%s' not found in '%s'"),
			*GraphName, *AnimBlueprintPath);
		return Result;
	}

	UEdGraphNode* Node = BridgeAnimWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetAnimNodeDetailsByGuid node guid '%s' not found in graph '%s'"),
			*NodeGuid, *GraphName);
		return Result;
	}

	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
	if (!AnimNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetAnimNodeDetailsByGuid node '%s' is %s, not an AnimGraphNode"),
			*NodeGuid, *Node->GetClass()->GetName());
		return Result;
	}

	BridgeAnimImpl::DumpAnimNodeProperties(AnimNode, Result);
	return Result;
}

// ─── GetAnimCurves ──────────────────────────────────────────

TArray<FString> UUnrealBridgeAnimLibrary::GetAnimCurves(const FString& AnimBlueprintPath)
{
	TArray<FString> Result;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	// Gather from skeleton's curve metadata
	if (ABP->TargetSkeleton)
	{
		TArray<FName> Names;
		ABP->TargetSkeleton->GetCurveMetaDataNames(Names);
		for (const FName& N : Names)
		{
			Result.Add(N.ToString());
		}
	}

	return Result;
}

// ─── GetAnimSequenceInfo ────────────────────────────────────

FBridgeAnimSequenceInfo UUnrealBridgeAnimLibrary::GetAnimSequenceInfo(const FString& SequencePath)
{
	FBridgeAnimSequenceInfo Result;

	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	if (!Seq)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load AnimSequence '%s'"), *SequencePath);
		return Result;
	}

	Result.Name = Seq->GetName();
	Result.PlayLength = Seq->GetPlayLength();
	Result.NumFrames = Seq->GetNumberOfSampledKeys();
	Result.RateScale = Seq->RateScale;

	if (Result.PlayLength > 0 && Result.NumFrames > 1)
		Result.FrameRate = (Result.NumFrames - 1) / Result.PlayLength;

	if (USkeleton* Skel = Seq->GetSkeleton())
		Result.SkeletonName = Skel->GetName();

	Result.bHasRootMotion = Seq->bEnableRootMotion;

	// Notifies
	BridgeAnimImpl::GatherNotifies(Seq, Result.Notifies);

	// Curves
	const FRawCurveTracks& Curves = Seq->GetCurveData();
	for (const FFloatCurve& C : Curves.FloatCurves)
	{
		FBridgeAnimCurveInfo CI;
		CI.CurveName = C.GetName().ToString();
		CI.NumKeys = C.FloatCurve.GetNumKeys();
		Result.Curves.Add(CI);
	}

	return Result;
}

// ─── GetMontageInfo ─────────────────────────────────────────

FBridgeMontageInfo UUnrealBridgeAnimLibrary::GetMontageInfo(const FString& MontagePath)
{
	FBridgeMontageInfo Result;

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load AnimMontage '%s'"), *MontagePath);
		return Result;
	}

	Result.Name = Montage->GetName();
	Result.PlayLength = Montage->GetPlayLength();

	if (USkeleton* Skel = Montage->GetSkeleton())
		Result.SkeletonName = Skel->GetName();

	// Slot
	if (Montage->SlotAnimTracks.Num() > 0)
		Result.SlotName = Montage->SlotAnimTracks[0].SlotName.ToString();

	// Blend
	Result.BlendInTime = Montage->BlendIn.GetBlendTime();
	Result.BlendOutTime = Montage->BlendOut.GetBlendTime();
	Result.bEnableAutoBlendOut = Montage->bEnableAutoBlendOut;

	// Sections
	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		const FCompositeSection& Sec = Montage->CompositeSections[i];
		FBridgeMontageSectionInfo SI;
		SI.SectionName = Sec.SectionName.ToString();
		SI.StartTime = Sec.GetTime();

		// Next section link
		int32 NextIdx = Montage->GetSectionIndex(Sec.NextSectionName);
		if (NextIdx != INDEX_NONE)
			SI.NextSectionName = Sec.NextSectionName.ToString();

		Result.Sections.Add(SI);
	}

	// Notifies
	BridgeAnimImpl::GatherNotifies(Montage, Result.Notifies);

	return Result;
}

// ─── GetBlendSpaceInfo ──────────────────────────────────────

FBridgeBlendSpaceInfo UUnrealBridgeAnimLibrary::GetBlendSpaceInfo(const FString& BlendSpacePath)
{
	FBridgeBlendSpaceInfo Result;

	UBlendSpace* BS = LoadObject<UBlendSpace>(nullptr, *BlendSpacePath);
	if (!BS)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load BlendSpace '%s'"), *BlendSpacePath);
		return Result;
	}

	Result.Name = BS->GetName();

	if (USkeleton* Skel = BS->GetSkeleton())
		Result.SkeletonName = Skel->GetName();

	// Determine dimension from class
	Result.NumAxes = BS->IsA<UBlendSpace>() ? 2 : 2; // UBlendSpace1D is separate

	// Axes
	for (int32 i = 0; i < 2; ++i)
	{
		const FBlendParameter& Param = BS->GetBlendParameter(i);
		FBridgeBlendSpaceAxis Axis;
		Axis.Name = Param.DisplayName;
		Axis.Min = Param.Min;
		Axis.Max = Param.Max;
		Axis.GridDivisions = Param.GridNum;
		Result.Axes.Add(Axis);
	}

	// Samples
	for (const FBlendSample& Sample : BS->GetBlendSamples())
	{
		FBridgeBlendSpaceSample S;
		S.AnimationName = Sample.Animation ? Sample.Animation->GetName() : TEXT("");
		S.SampleValue = Sample.SampleValue;
		S.RateScale = Sample.RateScale;
		Result.Samples.Add(S);
	}

	return Result;
}

// ─── GetSkeletonBoneTree ────────────────────────────────────

TArray<FBridgeBoneInfo> UUnrealBridgeAnimLibrary::GetSkeletonBoneTree(const FString& SkeletonPath)
{
	TArray<FBridgeBoneInfo> Result;

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load Skeleton '%s'"), *SkeletonPath);
		return Result;
	}

	const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();
	int32 NumBones = RefSkel.GetNum();

	for (int32 i = 0; i < NumBones; ++i)
	{
		FBridgeBoneInfo Info;
		Info.BoneName = RefSkel.GetBoneName(i).ToString();
		Info.BoneIndex = i;
		Info.ParentIndex = RefSkel.GetParentIndex(i);

		if (Info.ParentIndex >= 0)
			Info.ParentName = RefSkel.GetBoneName(Info.ParentIndex).ToString();

		Result.Add(Info);
	}

	return Result;
}

// ─── GetSkeletonSockets ─────────────────────────────────────

TArray<FBridgeSocketInfo> UUnrealBridgeAnimLibrary::GetSkeletonSockets(const FString& SkeletonPath)
{
	TArray<FBridgeSocketInfo> Result;

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load Skeleton '%s'"), *SkeletonPath);
		return Result;
	}

	for (USkeletalMeshSocket* Socket : Skel->Sockets)
	{
		if (!Socket) continue;

		FBridgeSocketInfo Info;
		Info.SocketName = Socket->SocketName.ToString();
		Info.ParentBoneName = Socket->BoneName.ToString();
		Info.RelativeLocation = Socket->RelativeLocation;
		Info.RelativeRotation = Socket->RelativeRotation;
		Info.RelativeScale = Socket->RelativeScale;
		Result.Add(Info);
	}

	return Result;
}

// ─── Write ops ──────────────────────────────────────────────

bool UUnrealBridgeAnimLibrary::AddAnimNotify(
	const FString& SequencePath, const FString& NotifyName, float TriggerTime, float Duration)
{
	UAnimSequenceBase* Seq = LoadObject<UAnimSequenceBase>(nullptr, *SequencePath);
	if (!Seq)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddAnimNotify failed to load '%s'"), *SequencePath);
		return false;
	}
	if (TriggerTime < 0.f || TriggerTime > Seq->GetPlayLength())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddAnimNotify TriggerTime %f out of range [0, %f]"),
			TriggerTime, Seq->GetPlayLength());
		return false;
	}

	Seq->Modify();

	FAnimNotifyEvent Ev;
	Ev.NotifyName = FName(*NotifyName);
	Ev.SetTime(TriggerTime);
	Ev.TrackIndex = 0;
	Ev.Notify = nullptr;
	Ev.NotifyStateClass = nullptr;
	if (Duration > 0.f)
	{
		Ev.SetDuration(FMath::Min(Duration, Seq->GetPlayLength() - TriggerTime));
	}
	Seq->Notifies.Add(Ev);

	// Sort notifies by trigger time (editor convention).
	Seq->Notifies.Sort([](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B) {
		return A.GetTime() < B.GetTime();
	});

	Seq->PostEditChange();
	Seq->MarkPackageDirty();
	return true;
}

int32 UUnrealBridgeAnimLibrary::RemoveAnimNotifiesByName(
	const FString& SequencePath, const FString& NotifyName)
{
	UAnimSequenceBase* Seq = LoadObject<UAnimSequenceBase>(nullptr, *SequencePath);
	if (!Seq)
	{
		return 0;
	}

	const FName Target(*NotifyName);
	const int32 Before = Seq->Notifies.Num();
	Seq->Modify();
	Seq->Notifies.RemoveAll([&](const FAnimNotifyEvent& E) {
		return E.NotifyName == Target;
	});
	const int32 Removed = Before - Seq->Notifies.Num();
	if (Removed > 0)
	{
		Seq->PostEditChange();
		Seq->MarkPackageDirty();
	}
	return Removed;
}

bool UUnrealBridgeAnimLibrary::SetAnimSequenceRateScale(
	const FString& SequencePath, float RateScale)
{
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	if (!Seq)
	{
		return false;
	}
	Seq->Modify();
	Seq->RateScale = RateScale;
	Seq->PostEditChange();
	Seq->MarkPackageDirty();
	return true;
}

bool UUnrealBridgeAnimLibrary::AddMontageSection(
	const FString& MontagePath, const FString& SectionName, float StartTime)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
	{
		return false;
	}
	if (StartTime < 0.f || StartTime > Montage->GetPlayLength())
	{
		return false;
	}
	const FName Target(*SectionName);
	for (const FCompositeSection& Existing : Montage->CompositeSections)
	{
		if (Existing.SectionName == Target)
		{
			return false;
		}
	}

	Montage->Modify();
	FCompositeSection NewSection;
	NewSection.SectionName = Target;
	NewSection.SetTime(StartTime);
	Montage->CompositeSections.Add(NewSection);

	Montage->CompositeSections.Sort([](const FCompositeSection& A, const FCompositeSection& B) {
		return A.GetTime() < B.GetTime();
	});

	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

bool UUnrealBridgeAnimLibrary::SetMontageSectionNext(
	const FString& MontagePath, const FString& SectionName, const FString& NextSectionName)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
	{
		return false;
	}
	const FName Target(*SectionName);
	const FName Next = NextSectionName.IsEmpty() ? NAME_None : FName(*NextSectionName);

	// Verify Next exists when non-empty.
	if (Next != NAME_None)
	{
		bool bFoundNext = false;
		for (const FCompositeSection& S : Montage->CompositeSections)
		{
			if (S.SectionName == Next) { bFoundNext = true; break; }
		}
		if (!bFoundNext) { return false; }
	}

	for (FCompositeSection& S : Montage->CompositeSections)
	{
		if (S.SectionName == Target)
		{
			Montage->Modify();
			S.NextSectionName = Next;
			Montage->PostEditChange();
			Montage->MarkPackageDirty();
			return true;
		}
	}
	return false;
}

// ─── GetMontageSlotSegments ────────────────────────────────

TArray<FBridgeMontageSlotSegment> UUnrealBridgeAnimLibrary::GetMontageSlotSegments(const FString& MontagePath)
{
	TArray<FBridgeMontageSlotSegment> Result;

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage) return Result;

	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		const FString SlotNameStr = Slot.SlotName.ToString();
		for (const FAnimSegment& Seg : Slot.AnimTrack.AnimSegments)
		{
			FBridgeMontageSlotSegment S;
			S.SlotName = SlotNameStr;
			if (UAnimSequenceBase* Ref = Seg.GetAnimReference())
			{
				S.AnimReferencePath = FSoftObjectPath(Ref).ToString();
			}
			S.StartPos = Seg.StartPos;
			S.AnimStartTime = Seg.AnimStartTime;
			S.AnimEndTime = Seg.AnimEndTime;
			S.AnimPlayRate = Seg.AnimPlayRate;
			S.LoopingCount = Seg.LoopingCount;
			Result.Add(S);
		}
	}
	return Result;
}

// ─── RemoveMontageSection ──────────────────────────────────

bool UUnrealBridgeAnimLibrary::RemoveMontageSection(const FString& MontagePath, const FString& SectionName)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage) return false;

	const FName Target(*SectionName);
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		if (Montage->CompositeSections[i].SectionName == Target) { FoundIndex = i; break; }
	}
	if (FoundIndex == INDEX_NONE) return false;

	Montage->Modify();
	Montage->CompositeSections.RemoveAt(FoundIndex);

	// Clear any `NextSectionName` that referenced the removed section.
	for (FCompositeSection& S : Montage->CompositeSections)
	{
		if (S.NextSectionName == Target)
		{
			S.NextSectionName = NAME_None;
		}
	}

	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

// ─── SetMontageBlendTimes ──────────────────────────────────

bool UUnrealBridgeAnimLibrary::SetMontageBlendTimes(const FString& MontagePath,
	float BlendInTime, float BlendOutTime, float BlendOutTriggerTime, bool bEnableAutoBlendOut)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage) return false;

	Montage->Modify();
	if (BlendInTime >= 0.f)
	{
		Montage->BlendIn.SetBlendTime(BlendInTime);
	}
	if (BlendOutTime >= 0.f)
	{
		Montage->BlendOut.SetBlendTime(BlendOutTime);
	}
	Montage->BlendOutTriggerTime = BlendOutTriggerTime;
	Montage->bEnableAutoBlendOut = bEnableAutoBlendOut;
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

// ─── GetSkeletonVirtualBones ───────────────────────────────

TArray<FBridgeVirtualBoneInfo> UUnrealBridgeAnimLibrary::GetSkeletonVirtualBones(const FString& SkeletonPath)
{
	TArray<FBridgeVirtualBoneInfo> Result;

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return Result;

	for (const FVirtualBone& VB : Skel->GetVirtualBones())
	{
		FBridgeVirtualBoneInfo Info;
		Info.VirtualBoneName = VB.VirtualBoneName.ToString();
		Info.SourceBoneName = VB.SourceBoneName.ToString();
		Info.TargetBoneName = VB.TargetBoneName.ToString();
		Result.Add(Info);
	}
	return Result;
}

// ─── AddSkeletonSocket ─────────────────────────────────────

bool UUnrealBridgeAnimLibrary::AddSkeletonSocket(const FString& SkeletonPath,
	const FString& SocketName, const FString& ParentBoneName,
	FVector RelativeLocation, FRotator RelativeRotation, FVector RelativeScale)
{
	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return false;

	const FName SocketFName(*SocketName);
	const FName BoneFName(*ParentBoneName);

	// Reject duplicate.
	if (Skel->FindSocket(SocketFName) != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddSkeletonSocket '%s' already exists"), *SocketName);
		return false;
	}
	// Verify bone exists.
	if (Skel->GetReferenceSkeleton().FindBoneIndex(BoneFName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddSkeletonSocket bone '%s' not found"), *ParentBoneName);
		return false;
	}

	Skel->Modify();
	USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skel);
	NewSocket->SocketName = SocketFName;
	NewSocket->BoneName = BoneFName;
	NewSocket->RelativeLocation = RelativeLocation;
	NewSocket->RelativeRotation = RelativeRotation;
	NewSocket->RelativeScale = RelativeScale;
	Skel->Sockets.Add(NewSocket);
	Skel->PostEditChange();
	Skel->MarkPackageDirty();
	return true;
}

// ─── Cross-asset query ─────────────────────────────────────

TArray<FString> UUnrealBridgeAnimLibrary::ListAssetsForSkeleton(
	const FString& SkeletonPath, const FString& AssetType, int32 MaxResults)
{
	TArray<FString> Results;

	USkeleton* TargetSkel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!TargetSkel)
	{
		return Results;
	}
	const FString TargetSkelStr = FSoftObjectPath(TargetSkel).ToString();

	FARFilter Filter;
	if (AssetType == TEXT("Sequence"))
	{
		Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	}
	else if (AssetType == TEXT("Montage"))
	{
		Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	}
	else if (AssetType == TEXT("BlendSpace"))
	{
		Filter.ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());
	}
	else
	{
		Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());
	}
	Filter.bRecursiveClasses = true;

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();
	TArray<FAssetData> Assets;
	AR.GetAssets(Filter, Assets);

	for (const FAssetData& AD : Assets)
	{
		FString SkelTag;
		if (!AD.GetTagValue(TEXT("Skeleton"), SkelTag))
		{
			continue;
		}
		// Tag value often embeds the path like "Skeleton'/Game/.../SK_Skel.SK_Skel'" or a bare soft path.
		if (SkelTag.Contains(TargetSkelStr))
		{
			Results.Add(AD.GetSoftObjectPath().ToString());
			if (MaxResults > 0 && Results.Num() >= MaxResults)
			{
				break;
			}
		}
	}
	return Results;
}

// ─── RemoveSkeletonSocket ──────────────────────────────────

bool UUnrealBridgeAnimLibrary::RemoveSkeletonSocket(const FString& SkeletonPath, const FString& SocketName)
{
	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return false;

	const FName Target(*SocketName);
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Skel->Sockets.Num(); ++i)
	{
		if (Skel->Sockets[i] && Skel->Sockets[i]->SocketName == Target)
		{
			FoundIndex = i;
			break;
		}
	}
	if (FoundIndex == INDEX_NONE) return false;

	Skel->Modify();
	Skel->Sockets.RemoveAt(FoundIndex);
	Skel->PostEditChange();
	Skel->MarkPackageDirty();
	return true;
}

// ─── GetAnimSyncMarkers ────────────────────────────────────

TArray<FBridgeAnimSyncMarker> UUnrealBridgeAnimLibrary::GetAnimSyncMarkers(const FString& SequencePath)
{
	TArray<FBridgeAnimSyncMarker> Result;

	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	if (!Seq) return Result;

	for (const FAnimSyncMarker& M : Seq->AuthoredSyncMarkers)
	{
		FBridgeAnimSyncMarker Info;
		Info.MarkerName = M.MarkerName.ToString();
		Info.Time = M.Time;
#if WITH_EDITORONLY_DATA
		Info.TrackIndex = M.TrackIndex;
#else
		Info.TrackIndex = -1;
#endif
		Result.Add(Info);
	}
	Result.Sort([](const FBridgeAnimSyncMarker& A, const FBridgeAnimSyncMarker& B) {
		return A.Time < B.Time;
	});
	return Result;
}

// ─── GetSkeletonBlendProfiles ──────────────────────────────

namespace BridgeAnimImpl
{
	static FString BlendProfileModeToString(EBlendProfileMode Mode)
	{
		switch (Mode)
		{
		case EBlendProfileMode::TimeFactor:   return TEXT("TimeFactor");
		case EBlendProfileMode::WeightFactor: return TEXT("WeightFactor");
		case EBlendProfileMode::BlendMask:    return TEXT("BlendMask");
		default:                              return TEXT("Unknown");
		}
	}
}

TArray<FBridgeBlendProfileInfo> UUnrealBridgeAnimLibrary::GetSkeletonBlendProfiles(const FString& SkeletonPath)
{
	TArray<FBridgeBlendProfileInfo> Result;

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return Result;

	for (const TObjectPtr<UBlendProfile>& BP : Skel->BlendProfiles)
	{
		if (!BP) continue;
		FBridgeBlendProfileInfo Info;
		Info.Name = BP->GetName();
		Info.Mode = BridgeAnimImpl::BlendProfileModeToString(BP->GetMode());
		Info.NumEntries = BP->GetNumBlendEntries();
		Result.Add(Info);
	}
	return Result;
}

// ─── GetBlendProfileEntries ────────────────────────────────

TArray<FBridgeBlendProfileEntry> UUnrealBridgeAnimLibrary::GetBlendProfileEntries(
	const FString& SkeletonPath, const FString& ProfileName)
{
	TArray<FBridgeBlendProfileEntry> Result;

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return Result;

	UBlendProfile* Profile = Skel->GetBlendProfile(FName(*ProfileName));
	if (!Profile) return Result;

	const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();
	const int32 NumEntries = Profile->GetNumBlendEntries();
	for (int32 i = 0; i < NumEntries; ++i)
	{
		FBridgeBlendProfileEntry Entry;
		Entry.BlendScale = Profile->GetEntryBlendScale(i);

		// Map entry -> bone by scanning reference skeleton for the first bone whose
		// blend scale matches via GetBoneBlendScale(boneIndex). This is robust against
		// FBlendProfileBoneEntry internals changing and avoids touching private fields.
		Entry.BoneName = FString();
		Result.Add(Entry);
	}

	// Walk the skeleton in order, assigning bone names to any entries we can look up.
	// Cheaper and more reliable: iterate bones and pull each one's blend scale directly.
	Result.Reset();
	for (int32 BoneIdx = 0; BoneIdx < RefSkel.GetNum(); ++BoneIdx)
	{
		const FName BoneName = RefSkel.GetBoneName(BoneIdx);
		if (Profile->GetEntryIndex(BoneName) == INDEX_NONE) continue;
		FBridgeBlendProfileEntry Entry;
		Entry.BoneName = BoneName.ToString();
		Entry.BlendScale = Profile->GetBoneBlendScale(BoneName);
		Result.Add(Entry);
	}
	return Result;
}

// ─── SetMontageSectionStartTime ────────────────────────────

bool UUnrealBridgeAnimLibrary::SetMontageSectionStartTime(
	const FString& MontagePath, const FString& SectionName, float StartTime)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage) return false;
	if (StartTime < 0.f || StartTime > Montage->GetPlayLength()) return false;

	const FName Target(*SectionName);
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		if (Montage->CompositeSections[i].SectionName == Target) { FoundIndex = i; break; }
	}
	if (FoundIndex == INDEX_NONE) return false;

	Montage->Modify();
	Montage->CompositeSections[FoundIndex].SetTime(StartTime);
	Montage->CompositeSections.Sort([](const FCompositeSection& A, const FCompositeSection& B) {
		return A.GetTime() < B.GetTime();
	});
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

// ─── AddAnimSyncMarker ─────────────────────────────────────

bool UUnrealBridgeAnimLibrary::AddAnimSyncMarker(const FString& SequencePath, const FString& MarkerName, float Time)
{
	if (MarkerName.IsEmpty()) return false;
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	if (!Seq) return false;
	if (Time < 0.f || Time > Seq->GetPlayLength()) return false;

	Seq->Modify();
	FAnimSyncMarker Marker;
	Marker.MarkerName = FName(*MarkerName);
	Marker.Time = Time;
#if WITH_EDITORONLY_DATA
	Marker.TrackIndex = 0;
#endif
	Seq->AuthoredSyncMarkers.Add(Marker);
	Seq->AuthoredSyncMarkers.Sort([](const FAnimSyncMarker& A, const FAnimSyncMarker& B) {
		return A.Time < B.Time;
	});
	// Keep the skeleton's cached unique marker names in sync so the marker is usable at runtime.
	Seq->RefreshSyncMarkerDataFromAuthored();
	Seq->PostEditChange();
	Seq->MarkPackageDirty();
	return true;
}

// ─── RemoveAnimSyncMarkersByName ───────────────────────────

int32 UUnrealBridgeAnimLibrary::RemoveAnimSyncMarkersByName(const FString& SequencePath, const FString& MarkerName)
{
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *SequencePath);
	if (!Seq) return 0;

	const FName Target(*MarkerName);
	const int32 Removed = Seq->AuthoredSyncMarkers.RemoveAll([Target](const FAnimSyncMarker& M) {
		return M.MarkerName == Target;
	});
	if (Removed > 0)
	{
		Seq->Modify();
		Seq->RefreshSyncMarkerDataFromAuthored();
		Seq->PostEditChange();
		Seq->MarkPackageDirty();
	}
	return Removed;
}

// ─── SetSkeletonSocketTransform ────────────────────────────

bool UUnrealBridgeAnimLibrary::SetSkeletonSocketTransform(const FString& SkeletonPath, const FString& SocketName,
	FVector RelativeLocation, FRotator RelativeRotation, FVector RelativeScale)
{
	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return false;

	const FName Target(*SocketName);
	USkeletalMeshSocket* Found = nullptr;
	for (USkeletalMeshSocket* S : Skel->Sockets)
	{
		if (S && S->SocketName == Target) { Found = S; break; }
	}
	if (!Found) return false;

	Skel->Modify();
	Found->Modify();
	Found->RelativeLocation = RelativeLocation;
	Found->RelativeRotation = RelativeRotation;
	Found->RelativeScale = RelativeScale;
	Found->PostEditChange();
	Skel->PostEditChange();
	Skel->MarkPackageDirty();
	return true;
}

// ─── RenameSkeletonSocket ──────────────────────────────────

bool UUnrealBridgeAnimLibrary::RenameSkeletonSocket(const FString& SkeletonPath, const FString& OldName, const FString& NewName)
{
	if (NewName.IsEmpty()) return false;
	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return false;

	const FName OldFName(*OldName);
	const FName NewFName(*NewName);
	if (OldFName == NewFName) return false;

	USkeletalMeshSocket* Found = nullptr;
	for (USkeletalMeshSocket* S : Skel->Sockets)
	{
		if (!S) continue;
		if (S->SocketName == NewFName) return false; // name collision
		if (S->SocketName == OldFName) Found = S;
	}
	if (!Found) return false;

	Skel->Modify();
	Found->Modify();
	Found->SocketName = NewFName;
	Found->PostEditChange();
	Skel->PostEditChange();
	Skel->MarkPackageDirty();
	return true;
}

// ─── GetAnimBlueprintInfo ──────────────────────────────────

FBridgeAnimBlueprintInfo UUnrealBridgeAnimLibrary::GetAnimBlueprintInfo(const FString& AnimBlueprintPath)
{
	FBridgeAnimBlueprintInfo Info;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Info;

	Info.Name = ABP->GetName();
	if (ABP->ParentClass)
	{
		Info.ParentClass = ABP->ParentClass->GetPathName();
	}
	if (ABP->TargetSkeleton)
	{
		Info.TargetSkeleton = FSoftObjectPath(ABP->TargetSkeleton).ToString();
	}
#if WITH_EDITORONLY_DATA
	if (USkeletalMesh* PreviewMesh = ABP->GetPreviewMesh())
	{
		Info.PreviewSkeletalMesh = FSoftObjectPath(PreviewMesh).ToString();
	}
	Info.bIsTemplate = ABP->bIsTemplate;
#endif

	// Count state machines, linked-layer bindings, slot nodes across all graphs.
	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (Node->IsA<UAnimGraphNode_StateMachineBase>()) Info.NumStateMachines++;
			else if (Node->IsA<UAnimGraphNode_LinkedAnimLayer>()) Info.NumLinkedLayers++;
			else if (Node->IsA<UAnimGraphNode_Slot>()) Info.NumSlots++;
		}
	}

	for (const FBPInterfaceDescription& Iface : ABP->ImplementedInterfaces)
	{
		if (Iface.Interface)
		{
			Info.ImplementedInterfaces.Add(Iface.Interface->GetName());
		}
	}

	return Info;
}

// ═══════════════════════════════════════════════════════════════
// AnimGraph / State Machine WRITE ops
// ═══════════════════════════════════════════════════════════════

namespace BridgeAnimWriteImpl
{
	/**
	 * Find any UEdGraph inside an ABP by its GetName(): walks the top-level
	 * AnimGraph, every StateMachine's EditorStateMachineGraph, and every
	 * state / conduit / transition BoundGraph inside those recursively.
	 * Also checks regular FunctionGraphs / UbergraphPages / DelegateGraphs so
	 * the caller can reuse BP-style graph references if desired.
	 */
	UEdGraph* FindAnyGraphByName(UAnimBlueprint* ABP, const FString& GraphName)
	{
		if (!ABP || GraphName.IsEmpty()) return nullptr;

		TArray<UEdGraph*> Stack;
		Stack.Append(ABP->FunctionGraphs);
		Stack.Append(ABP->UbergraphPages);
		Stack.Append(ABP->DelegateSignatureGraphs);
		Stack.Append(ABP->MacroGraphs);

		TSet<UEdGraph*> Visited;
		while (Stack.Num() > 0)
		{
			UEdGraph* G = Stack.Pop(EAllowShrinking::No);
			if (!G || Visited.Contains(G)) continue;
			Visited.Add(G);

			if (G->GetName() == GraphName) return G;

			// Walk nodes that own sub-graphs.
			for (UEdGraphNode* Node : G->Nodes)
			{
				if (!Node) continue;
				if (UAnimGraphNode_StateMachineBase* SM = Cast<UAnimGraphNode_StateMachineBase>(Node))
				{
					if (UEdGraph* Inner = SM->EditorStateMachineGraph)
						Stack.Add(Inner);
				}
				else if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
				{
					if (State->BoundGraph) Stack.Add(State->BoundGraph);
				}
				else if (UAnimStateConduitNode* Conduit = Cast<UAnimStateConduitNode>(Node))
				{
					if (Conduit->BoundGraph) Stack.Add(Conduit->BoundGraph);
				}
				else if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
				{
					if (Trans->BoundGraph) Stack.Add(Trans->BoundGraph);
				}
			}

			// And any direct SubGraphs attached to nodes (timelines etc.).
			Stack.Append(G->SubGraphs);
		}
		return nullptr;
	}

	/** Find a node inside a graph by its stringified NodeGuid (Digits form). */
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& NodeGuid)
	{
		if (!Graph || NodeGuid.IsEmpty()) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid.ToString(EGuidFormats::Digits) == NodeGuid)
				return N;
		}
		return nullptr;
	}

	/** Find the state machine graph by name, inside an ABP. */
	UAnimationStateMachineGraph* FindStateMachineGraph(UAnimBlueprint* ABP, const FString& Name)
	{
		UEdGraph* G = FindAnyGraphByName(ABP, Name);
		return Cast<UAnimationStateMachineGraph>(G);
	}

	/** Within a state machine graph, find a state / conduit by its name (the BoundGraph's name). */
	UAnimStateNodeBase* FindStateByName(UAnimationStateMachineGraph* SMGraph, const FString& StateName)
	{
		if (!SMGraph || StateName.IsEmpty()) return nullptr;
		for (UEdGraphNode* N : SMGraph->Nodes)
		{
			if (UAnimStateNode* S = Cast<UAnimStateNode>(N))
			{
				if (S->GetStateName() == StateName) return S;
			}
			else if (UAnimStateConduitNode* C = Cast<UAnimStateConduitNode>(N))
			{
				if (C->GetStateName() == StateName) return C;
			}
		}
		return nullptr;
	}

	/** Within a state machine graph, find the first transition From -> To (by state name). */
	UAnimStateTransitionNode* FindTransition(UAnimationStateMachineGraph* SMGraph,
		const FString& FromName, const FString& ToName)
	{
		if (!SMGraph) return nullptr;
		for (UEdGraphNode* N : SMGraph->Nodes)
		{
			UAnimStateTransitionNode* T = Cast<UAnimStateTransitionNode>(N);
			if (!T) continue;
			UAnimStateNodeBase* Prev = T->GetPreviousState();
			UAnimStateNodeBase* Next = T->GetNextState();
			if (!Prev || !Next) continue;
			if (Prev->GetStateName() == FromName && Next->GetStateName() == ToName)
				return T;
		}
		return nullptr;
	}

	/** Load any UAnimationAsset by path (nullptr on failure). */
	UAnimSequenceBase* LoadSequenceSafe(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		return LoadObject<UAnimSequenceBase>(nullptr, *Path);
	}
	UBlendSpace* LoadBlendSpaceSafe(const FString& Path)
	{
		if (Path.IsEmpty()) return nullptr;
		return LoadObject<UBlendSpace>(nullptr, *Path);
	}

	/**
	 * Post-write bookkeeping: mark blueprint structurally modified and notify
	 * the graph to refresh. Caller should then call compile_blueprint via the
	 * BP library when ready — we don't force-compile per op.
	 */
	void FinalizeEdit(UAnimBlueprint* ABP, UEdGraph* Graph)
	{
		if (Graph) Graph->NotifyGraphChanged();
		if (ABP) FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ABP);
	}

	/** Rename a graph cleanly (reuses the same RenameGraphCloseToName helper the editor uses). */
	void RenameGraphTo(UEdGraph* Graph, const FString& NewName)
	{
		if (!Graph || NewName.IsEmpty()) return;
		FEdGraphUtilities::RenameGraphToNameOrCloseToName(Graph, NewName);
	}

	/**
	 * Canonical spawn for an anim graph node: manual NewObject + AddNode +
	 * PostPlacedNewNode + AllocateDefaultPins. Matches the bridge BP pattern
	 * and avoids relying on private editor schema action classes.
	 *
	 * `Configure` runs BEFORE PostPlacedNewNode so it can populate
	 * `Node->Node.Foo = ...` fields that `PostPlacedNewNode` / default-pin
	 * allocation use (e.g. SequencePlayer's asset -> pin defaults).
	 */
	template<typename TNode>
	TNode* SpawnAnimGraphNode(UEdGraph* Graph, int32 X, int32 Y,
		TFunctionRef<void(TNode*)> Configure)
	{
		if (!Graph) return nullptr;

		TNode* Node = NewObject<TNode>(Graph);
		Node->CreateNewGuid();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Configure(Node);
		Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		return Node;
	}

	/** Same shape as SpawnAnimGraphNode but for any UEdGraphNode subclass. */
	template<typename TNode>
	TNode* SpawnGraphNode(UEdGraph* Graph, int32 X, int32 Y,
		TFunctionRef<void(TNode*)> Configure)
	{
		if (!Graph) return nullptr;

		TNode* Node = NewObject<TNode>(Graph);
		Node->CreateNewGuid();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Configure(Node);
		Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		return Node;
	}
}

// ─── ListAnimGraphs ─────────────────────────────────────────

TArray<FBridgeAnimGraphSummary> UUnrealBridgeAnimLibrary::ListAnimGraphs(const FString& AnimBlueprintPath)
{
	TArray<FBridgeAnimGraphSummary> Result;
	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Result;

	struct FEntry { UEdGraph* Graph; FString Kind; FString ParentName; };
	TArray<FEntry> Queue;
	TSet<UEdGraph*> Seen;

	for (UEdGraph* G : ABP->FunctionGraphs)
	{
		if (!G) continue;
		const bool bIsAnimGraph = (G->GetName() == TEXT("AnimGraph"));
		Queue.Add({ G, bIsAnimGraph ? TEXT("AnimGraph") : TEXT("Function"), FString() });
	}
	for (UEdGraph* G : ABP->UbergraphPages) if (G) Queue.Add({ G, TEXT("Ubergraph"), FString() });
	for (UEdGraph* G : ABP->MacroGraphs) if (G) Queue.Add({ G, TEXT("Macro"), FString() });

	while (Queue.Num() > 0)
	{
		const FEntry Entry = Queue.Pop(EAllowShrinking::No);
		if (!Entry.Graph || Seen.Contains(Entry.Graph)) continue;
		Seen.Add(Entry.Graph);

		FBridgeAnimGraphSummary Info;
		Info.Name = Entry.Graph->GetName();
		Info.Kind = Entry.Kind;
		Info.ParentGraphName = Entry.ParentName;
		Info.NumNodes = Entry.Graph->Nodes.Num();
		Result.Add(Info);

		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;
			if (UAnimGraphNode_StateMachineBase* SM = Cast<UAnimGraphNode_StateMachineBase>(Node))
			{
				if (UEdGraph* Inner = SM->EditorStateMachineGraph)
					Queue.Add({ Inner, TEXT("StateMachine"), Info.Name });
			}
			else if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
			{
				if (State->BoundGraph) Queue.Add({ State->BoundGraph, TEXT("State"), Info.Name });
			}
			else if (UAnimStateConduitNode* Conduit = Cast<UAnimStateConduitNode>(Node))
			{
				if (Conduit->BoundGraph) Queue.Add({ Conduit->BoundGraph, TEXT("Conduit"), Info.Name });
			}
			else if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
			{
				if (Trans->BoundGraph) Queue.Add({ Trans->BoundGraph, TEXT("TransitionRule"), Info.Name });
			}
		}
	}

	return Result;
}

TArray<FString> UUnrealBridgeAnimLibrary::ListAnimGraphNodes(const FString& AnimBlueprintPath,
	const FString& GraphName)
{
	using namespace BridgeAnimWriteImpl;

	TArray<FString> Rows;
	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return Rows;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return Rows;

	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N) continue;
		const FString Guid = N->NodeGuid.ToString(EGuidFormats::Digits);
		const FString ClassName = N->GetClass()->GetName();
		const FString Title = BridgeAnimImpl::GetNodeTitleSource(N, ENodeTitleType::ListView);
		Rows.Add(FString::Printf(TEXT("%s\t%s\t%s"), *Guid, *ClassName, *Title));
	}
	return Rows;
}

FString UUnrealBridgeAnimLibrary::FindAnimGraphNodeByClass(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& ShortClassName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N) continue;
		if (N->GetClass()->GetName() == ShortClassName)
			return N->NodeGuid.ToString(EGuidFormats::Digits);
	}
	return FString();
}

// ─── AnimGraph node factories ───────────────────────────────

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeSequencePlayer(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& SequencePath, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) { UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: graph '%s' not found in '%s'"), *GraphName, *AnimBlueprintPath); return FString(); }

	UAnimSequenceBase* Seq = LoadSequenceSafe(SequencePath);

	const FScopedTransaction Tx(LOCTEXT("AddSequencePlayer", "Add Sequence Player"));
	Graph->Modify();

	UAnimGraphNode_SequencePlayer* Node = SpawnAnimGraphNode<UAnimGraphNode_SequencePlayer>(
		Graph, PosX, PosY, [Seq](UAnimGraphNode_SequencePlayer* Tmpl)
		{
			if (Seq)
			{
				Tmpl->SetAnimationAsset(Seq);
				Tmpl->CopySettingsFromAnimationAsset(Seq);
			}
		});
	if (!Node) return FString();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeBlendSpacePlayer(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& BlendSpacePath, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	UBlendSpace* BS = LoadBlendSpaceSafe(BlendSpacePath);

	const FScopedTransaction Tx(LOCTEXT("AddBlendSpacePlayer", "Add Blend Space Player"));
	Graph->Modify();

	UAnimGraphNode_BlendSpacePlayer* Node = SpawnAnimGraphNode<UAnimGraphNode_BlendSpacePlayer>(
		Graph, PosX, PosY, [BS](UAnimGraphNode_BlendSpacePlayer* Tmpl)
		{
			if (BS)
			{
				Tmpl->SetAnimationAsset(BS);
				Tmpl->CopySettingsFromAnimationAsset(BS);
			}
		});
	if (!Node) return FString();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeSlot(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& SlotName, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	const FScopedTransaction Tx(LOCTEXT("AddSlot", "Add Slot"));
	Graph->Modify();

	const FName SlotFName = SlotName.IsEmpty() ? FName(TEXT("DefaultSlot")) : FName(*SlotName);

	// Register slot on skeleton if missing.
	if (USkeleton* Skel = ABP->TargetSkeleton)
	{
		if (!Skel->ContainsSlotName(SlotFName))
		{
			Skel->RegisterSlotNode(SlotFName);
		}
	}

	UAnimGraphNode_Slot* Node = SpawnAnimGraphNode<UAnimGraphNode_Slot>(
		Graph, PosX, PosY, [SlotFName](UAnimGraphNode_Slot* Tmpl)
		{
			Tmpl->Node.SlotName = SlotFName;
		});
	if (!Node) return FString();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeStateMachine(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& StateMachineName, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	const FScopedTransaction Tx(LOCTEXT("AddStateMachine", "Add State Machine"));
	Graph->Modify();

	UAnimGraphNode_StateMachine* Node = SpawnAnimGraphNode<UAnimGraphNode_StateMachine>(
		Graph, PosX, PosY, [](UAnimGraphNode_StateMachine*){});
	if (!Node) return FString();

	if (!StateMachineName.IsEmpty() && Node->EditorStateMachineGraph)
	{
		RenameGraphTo(Node->EditorStateMachineGraph, StateMachineName);
	}

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeLayeredBoneBlend(const FString& AnimBlueprintPath,
	const FString& GraphName, int32 NumBlendPoses, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	const int32 Extras = FMath::Max(0, NumBlendPoses - 1);

	const FScopedTransaction Tx(LOCTEXT("AddLayeredBoneBlend", "Add Layered Bone Blend"));
	Graph->Modify();

	UAnimGraphNode_LayeredBoneBlend* Node = SpawnAnimGraphNode<UAnimGraphNode_LayeredBoneBlend>(
		Graph, PosX, PosY, [](UAnimGraphNode_LayeredBoneBlend*){});
	if (!Node) return FString();

	for (int32 i = 0; i < Extras; ++i)
	{
		Node->AddPinToBlendByFilter();
	}
	Node->ReconstructNode();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeBlendListByBool(const FString& AnimBlueprintPath,
	const FString& GraphName, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	const FScopedTransaction Tx(LOCTEXT("AddBlendListByBool", "Add Blend List By Bool"));
	Graph->Modify();

	UAnimGraphNode_BlendListByBool* Node = SpawnAnimGraphNode<UAnimGraphNode_BlendListByBool>(
		Graph, PosX, PosY, [](UAnimGraphNode_BlendListByBool*){});
	if (!Node) return FString();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeBlendListByInt(const FString& AnimBlueprintPath,
	const FString& GraphName, int32 NumPoses, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	const int32 Clamped = FMath::Max(2, NumPoses);

	const FScopedTransaction Tx(LOCTEXT("AddBlendListByInt", "Add Blend List By Int"));
	Graph->Modify();

	UAnimGraphNode_BlendListByInt* Node = SpawnAnimGraphNode<UAnimGraphNode_BlendListByInt>(
		Graph, PosX, PosY, [](UAnimGraphNode_BlendListByInt*){});
	if (!Node) return FString();

	// Default BlendListByInt spawns with 2 poses; add the rest.
	for (int32 i = 2; i < Clamped; ++i)
	{
		Node->AddPinToBlendList();
	}
	Node->ReconstructNode();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeTwoWayBlend(const FString& AnimBlueprintPath,
	const FString& GraphName, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	const FScopedTransaction Tx(LOCTEXT("AddTwoWayBlend", "Add Two Way Blend"));
	Graph->Modify();

	UAnimGraphNode_TwoWayBlend* Node = SpawnAnimGraphNode<UAnimGraphNode_TwoWayBlend>(
		Graph, PosX, PosY, [](UAnimGraphNode_TwoWayBlend*){});
	if (!Node) return FString();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeLinkedAnimLayer(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& InterfaceClassPath, const FString& LayerName,
	int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	UClass* InterfaceClass = nullptr;
	if (!InterfaceClassPath.IsEmpty())
	{
		InterfaceClass = LoadObject<UClass>(nullptr, *InterfaceClassPath);
	}
	const FName LayerFName = LayerName.IsEmpty() ? NAME_None : FName(*LayerName);

	const FScopedTransaction Tx(LOCTEXT("AddLinkedAnimLayer", "Add Linked Anim Layer"));
	Graph->Modify();

	UAnimGraphNode_LinkedAnimLayer* Node = SpawnAnimGraphNode<UAnimGraphNode_LinkedAnimLayer>(
		Graph, PosX, PosY, [InterfaceClass, LayerFName](UAnimGraphNode_LinkedAnimLayer* Tmpl)
		{
			if (InterfaceClass)
			{
				Tmpl->Node.Interface = InterfaceClass;
			}
			if (LayerFName != NAME_None)
			{
				// SetLayerName is not exported from the AnimGraph module, so
				// write the underlying FAnimNode field directly. Matches what
				// the public setter does (it just assigns).
				Tmpl->Node.Layer = LayerFName;
			}
		});
	if (!Node) return FString();

	Node->ReconstructNode();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimGraphNodeByClassName(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& ShortClassName, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return FString();

	// Locate the UClass* by short name among all subclasses of UAnimGraphNode_Base.
	UClass* NodeClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* C = *It;
		if (!C->IsChildOf(UAnimGraphNode_Base::StaticClass())) continue;
		if (C->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
		if (C->GetName() == ShortClassName)
		{
			NodeClass = C;
			break;
		}
	}
	if (!NodeClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: no concrete UAnimGraphNode_Base subclass named '%s'"),
			*ShortClassName);
		return FString();
	}

	const FScopedTransaction Tx(LOCTEXT("AddAnimNodeByClass", "Add Anim Node"));
	Graph->Modify();

	UAnimGraphNode_Base* Node = NewObject<UAnimGraphNode_Base>(Graph, NodeClass);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FinalizeEdit(ABP, Graph);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Pin connection / disconnect / node removal ─────────────

bool UUnrealBridgeAnimLibrary::ConnectAnimGraphPins(const FString& AnimBlueprintPath,
	const FString& GraphName,
	const FString& SourceNodeGuid, const FString& SourcePinName,
	const FString& TargetNodeGuid, const FString& TargetPinName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Src = FindNodeByGuid(Graph, SourceNodeGuid);
	UEdGraphNode* Tgt = FindNodeByGuid(Graph, TargetNodeGuid);
	if (!Src || !Tgt) return false;

	UEdGraphPin* SrcPin = Src->FindPin(*SourcePinName);
	UEdGraphPin* TgtPin = Tgt->FindPin(*TargetPinName);
	if (!SrcPin || !TgtPin) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema) return false;

	const FScopedTransaction Tx(LOCTEXT("ConnectPins", "Connect Pins"));
	Graph->Modify();
	Src->Modify();
	Tgt->Modify();

	const bool bOk = Schema->TryCreateConnection(SrcPin, TgtPin);
	if (bOk) FinalizeEdit(ABP, Graph);
	return bOk;
}

bool UUnrealBridgeAnimLibrary::DisconnectAnimGraphPin(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& NodeGuid, const FString& PinName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	UEdGraphPin* Pin = Node->FindPin(*PinName);
	if (!Pin) return false;

	const FScopedTransaction Tx(LOCTEXT("DisconnectPin", "Disconnect Pin"));
	Graph->Modify();
	Node->Modify();
	Pin->BreakAllPinLinks();

	FinalizeEdit(ABP, Graph);
	return true;
}

bool UUnrealBridgeAnimLibrary::RemoveAnimGraphNode(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& NodeGuid)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	const FScopedTransaction Tx(LOCTEXT("RemoveAnimNode", "Remove Anim Node"));
	Graph->Modify();
	Node->Modify();

	Graph->RemoveNode(Node, /*bBreakAllLinks*/ true);

	FinalizeEdit(ABP, Graph);
	return true;
}

bool UUnrealBridgeAnimLibrary::SetAnimGraphNodePosition(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& NodeGuid, int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;

	FinalizeEdit(ABP, Graph);
	return true;
}

bool UUnrealBridgeAnimLibrary::SetAnimSequencePlayerSequence(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& NodeGuid, const FString& SequencePath)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeGuid);
	UAnimGraphNode_SequencePlayer* SP = Cast<UAnimGraphNode_SequencePlayer>(Node);
	if (!SP) return false;

	UAnimSequenceBase* Seq = LoadSequenceSafe(SequencePath);
	// Empty path clears — accept null.

	const FScopedTransaction Tx(LOCTEXT("SetSequencePlayerSequence", "Set Sequence"));
	SP->Modify();
	SP->SetAnimationAsset(Seq);
	if (Seq) SP->CopySettingsFromAnimationAsset(Seq);
	SP->ReconstructNode();

	FinalizeEdit(ABP, Graph);
	return true;
}

bool UUnrealBridgeAnimLibrary::SetAnimSlotName(const FString& AnimBlueprintPath,
	const FString& GraphName, const FString& NodeGuid, const FString& SlotName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = FindNodeByGuid(Graph, NodeGuid);
	UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node);
	if (!Slot) return false;

	const FName SlotFName = SlotName.IsEmpty() ? FName(TEXT("DefaultSlot")) : FName(*SlotName);

	const FScopedTransaction Tx(LOCTEXT("SetSlotName", "Set Slot Name"));
	Slot->Modify();
	Slot->Node.SlotName = SlotFName;

	if (USkeleton* Skel = ABP->TargetSkeleton)
	{
		if (!Skel->ContainsSlotName(SlotFName))
		{
			Skel->RegisterSlotNode(SlotFName);
		}
	}

	Slot->ReconstructNode();

	FinalizeEdit(ABP, Graph);
	return true;
}

// ─── State machine interior ─────────────────────────────────

FString UUnrealBridgeAnimLibrary::AddAnimState(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName, const FString& StateName,
	int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) { UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: state machine '%s' not found"), *StateMachineGraphName); return FString(); }

	const FScopedTransaction Tx(LOCTEXT("AddAnimState", "Add State"));
	SMGraph->Modify();

	UAnimStateNode* State = SpawnGraphNode<UAnimStateNode>(
		SMGraph, PosX, PosY, [](UAnimStateNode*){});
	if (!State) return FString();

	if (!StateName.IsEmpty() && State->BoundGraph)
	{
		RenameGraphTo(State->BoundGraph, StateName);
	}

	FinalizeEdit(ABP, SMGraph);
	return State->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimConduit(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName, const FString& ConduitName,
	int32 PosX, int32 PosY)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return FString();

	const FScopedTransaction Tx(LOCTEXT("AddAnimConduit", "Add Conduit"));
	SMGraph->Modify();

	UAnimStateConduitNode* Conduit = SpawnGraphNode<UAnimStateConduitNode>(
		SMGraph, PosX, PosY, [](UAnimStateConduitNode*){});
	if (!Conduit) return FString();

	if (!ConduitName.IsEmpty() && Conduit->BoundGraph)
	{
		RenameGraphTo(Conduit->BoundGraph, ConduitName);
	}

	FinalizeEdit(ABP, SMGraph);
	return Conduit->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeAnimLibrary::AddAnimTransition(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName,
	const FString& FromStateName, const FString& ToStateName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return FString();

	UAnimStateNodeBase* Prev = FindStateByName(SMGraph, FromStateName);
	UAnimStateNodeBase* Next = FindStateByName(SMGraph, ToStateName);
	if (!Prev || !Next) return FString();

	const FScopedTransaction Tx(LOCTEXT("AddAnimTransition", "Add Transition"));
	SMGraph->Modify();

	const int32 MidX = (Prev->NodePosX + Next->NodePosX) / 2;
	const int32 MidY = (Prev->NodePosY + Next->NodePosY) / 2;

	UAnimStateTransitionNode* Trans = SpawnGraphNode<UAnimStateTransitionNode>(
		SMGraph, MidX, MidY, [](UAnimStateTransitionNode*){});
	if (!Trans) return FString();

	// Hook endpoints BEFORE the pin-change callback can fire:
	// UAnimStateTransitionNode::PinConnectionListChanged self-destructs the
	// node when either In/Out pin becomes empty.
	Trans->CreateConnections(Prev, Next);

	// Rename the rule graph uniquely so clients can address it by name.
	// Default auto-naming leaves every rule graph called "Transition" —
	// useless for any subsequent write op. "Rule_<From>_to_<To>" is how a
	// human would label it in the transition-rule tab.
	if (Trans->BoundGraph)
	{
		const FString Desired = FString::Printf(TEXT("Rule_%s_to_%s"),
			*FromStateName, *ToStateName);
		RenameGraphTo(Trans->BoundGraph, Desired);
	}

	FinalizeEdit(ABP, SMGraph);
	return Trans->NodeGuid.ToString(EGuidFormats::Digits);
}

bool UUnrealBridgeAnimLibrary::RemoveAnimState(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName, const FString& StateName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return false;
	UAnimStateNodeBase* State = FindStateByName(SMGraph, StateName);
	if (!State) return false;

	const FScopedTransaction Tx(LOCTEXT("RemoveAnimState", "Remove State"));
	SMGraph->Modify();

	// Remove attached transitions first so they don't suicide in
	// PinConnectionListChanged as we break links.
	TArray<UAnimStateTransitionNode*> ToKill;
	for (UEdGraphNode* N : SMGraph->Nodes)
	{
		UAnimStateTransitionNode* T = Cast<UAnimStateTransitionNode>(N);
		if (!T) continue;
		if (T->GetPreviousState() == State || T->GetNextState() == State)
		{
			ToKill.Add(T);
		}
	}
	for (UAnimStateTransitionNode* T : ToKill)
	{
		SMGraph->RemoveNode(T, /*bBreakAllLinks*/ true);
	}

	SMGraph->RemoveNode(State, /*bBreakAllLinks*/ true);

	FinalizeEdit(ABP, SMGraph);
	return true;
}

FString UUnrealBridgeAnimLibrary::GetAnimTransitionRuleGraphName(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName,
	const FString& FromStateName, const FString& ToStateName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return FString();
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return FString();
	UAnimStateTransitionNode* Trans = FindTransition(SMGraph, FromStateName, ToStateName);
	if (!Trans || !Trans->BoundGraph) return FString();
	return Trans->BoundGraph->GetName();
}

bool UUnrealBridgeAnimLibrary::RemoveAnimTransition(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName,
	const FString& FromStateName, const FString& ToStateName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return false;
	UAnimStateTransitionNode* Trans = FindTransition(SMGraph, FromStateName, ToStateName);
	if (!Trans) return false;

	const FScopedTransaction Tx(LOCTEXT("RemoveAnimTransition", "Remove Transition"));
	SMGraph->Modify();

	SMGraph->RemoveNode(Trans, /*bBreakAllLinks*/ true);

	FinalizeEdit(ABP, SMGraph);
	return true;
}

bool UUnrealBridgeAnimLibrary::SetAnimStateDefault(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName, const FString& StateName)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return false;
	UAnimStateNodeBase* Target = FindStateByName(SMGraph, StateName);
	if (!Target) return false;

	UAnimStateEntryNode* Entry = SMGraph->EntryNode;
	if (!Entry) return false;

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
	UEdGraphPin* EntryOut = Entry->GetOutputPin();
#else
	// 5.4: UAnimStateEntryNode lacks GetOutputPin(); walk Pins directly.
	UEdGraphPin* EntryOut = nullptr;
	for (UEdGraphPin* Pin : Entry->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output) { EntryOut = Pin; break; }
	}
#endif
	UEdGraphPin* TargetIn = Target->GetInputPin();
	if (!EntryOut || !TargetIn) return false;

	const FScopedTransaction Tx(LOCTEXT("SetAnimStateDefault", "Set Default State"));
	SMGraph->Modify();
	Entry->Modify();
	Target->Modify();
	EntryOut->BreakAllPinLinks();
	EntryOut->MakeLinkTo(TargetIn);

	FinalizeEdit(ABP, SMGraph);
	return true;
}

bool UUnrealBridgeAnimLibrary::RenameAnimState(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName,
	const FString& OldName, const FString& NewName)
{
	using namespace BridgeAnimWriteImpl;

	if (NewName.IsEmpty()) return false;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return false;
	UAnimStateNodeBase* State = FindStateByName(SMGraph, OldName);
	if (!State) return false;

	UEdGraph* Bound = nullptr;
	if (UAnimStateNode* S = Cast<UAnimStateNode>(State)) Bound = S->BoundGraph;
	else if (UAnimStateConduitNode* C = Cast<UAnimStateConduitNode>(State)) Bound = C->BoundGraph;
	if (!Bound) return false;

	const FScopedTransaction Tx(LOCTEXT("RenameAnimState", "Rename State"));
	SMGraph->Modify();
	Bound->Modify();

	RenameGraphTo(Bound, NewName);

	FinalizeEdit(ABP, SMGraph);
	return true;
}

bool UUnrealBridgeAnimLibrary::SetAnimTransitionProperties(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName,
	const FString& FromStateName, const FString& ToStateName,
	float CrossfadeDuration, int32 PriorityOrder, bool bBidirectional)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return false;
	UAnimStateTransitionNode* Trans = FindTransition(SMGraph, FromStateName, ToStateName);
	if (!Trans) return false;

	const FScopedTransaction Tx(LOCTEXT("SetAnimTransitionProps", "Set Transition Properties"));
	Trans->Modify();

	if (CrossfadeDuration >= 0.f) Trans->CrossfadeDuration = CrossfadeDuration;
	if (PriorityOrder != MIN_int32) Trans->PriorityOrder = PriorityOrder;
	Trans->Bidirectional = bBidirectional;

	FinalizeEdit(ABP, SMGraph);
	return true;
}

bool UUnrealBridgeAnimLibrary::SetAnimTransitionConstRule(const FString& AnimBlueprintPath,
	const FString& StateMachineGraphName,
	const FString& FromStateName, const FString& ToStateName, bool bValue)
{
	using namespace BridgeAnimWriteImpl;

	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) return false;
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) return false;
	UAnimStateTransitionNode* Trans = FindTransition(SMGraph, FromStateName, ToStateName);
	if (!Trans) return false;

	UEdGraph* RuleGraph = Trans->BoundGraph;
	if (!RuleGraph) return false;

	// Find the transition result node and its bool input pin.
	UAnimGraphNode_TransitionResult* Result = nullptr;
	for (UEdGraphNode* N : RuleGraph->Nodes)
	{
		Result = Cast<UAnimGraphNode_TransitionResult>(N);
		if (Result) break;
	}
	if (!Result) return false;

	UEdGraphPin* BoolPin = nullptr;
	for (UEdGraphPin* Pin : Result->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TEXT("bCanEnterTransition"))
		{
			BoolPin = Pin;
			break;
		}
	}
	if (!BoolPin)
	{
		// Fallback: take the first input bool pin.
		for (UEdGraphPin* Pin : Result->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				BoolPin = Pin; break;
			}
		}
	}
	if (!BoolPin) return false;

	const FScopedTransaction Tx(LOCTEXT("SetTransitionConstRule", "Set Transition Rule"));
	RuleGraph->Modify();
	Result->Modify();

	// Clear links so our default literal is actually used on compile.
	BoolPin->BreakAllPinLinks();
	BoolPin->DefaultValue = bValue ? TEXT("true") : TEXT("false");
	BoolPin->AutogeneratedDefaultValue = BoolPin->DefaultValue;

	FinalizeEdit(ABP, RuleGraph);
	return true;
}

// ═══════════════════════════════════════════════════════════════
// Auto-layout
// ═══════════════════════════════════════════════════════════════

namespace BridgeAnimLayoutImpl
{
	/**
	 * Rough size for an AnimGraph node. AnimGraph nodes commonly render wider
	 * than K2 data nodes because of the anim-asset thumbnail + property block.
	 * These fallback defaults are only used when NodeWidth/Height haven't been
	 * populated yet (Slate hasn't ticked since the node was created).
	 */
	static const int32 AnimNodeDefaultW = 240;
	static const int32 AnimNodeDefaultH = 120;
	static const int32 StateNodeDefaultW = 180;
	static const int32 StateNodeDefaultH = 80;

	struct FLayer
	{
		TArray<UEdGraphNode*> Nodes;
	};

	/**
	 * Is this node a "sink" (pose consumer on the right of the graph)?
	 * Includes the AnimGraph root, state result, transition result, and the
	 * state-machine output node. Used to seed rightmost layer.
	 */
	bool IsAnimSink(UEdGraphNode* N)
	{
		if (!N) return false;
		FString ClassName = N->GetClass()->GetName();
		return ClassName == TEXT("AnimGraphNode_Root")
			|| ClassName == TEXT("AnimGraphNode_StateResult")
			|| ClassName == TEXT("AnimGraphNode_TransitionResult");
	}

	void EstimateNodeSize(UEdGraphNode* N, int32& W, int32& H)
	{
		if (!N) { W = AnimNodeDefaultW; H = AnimNodeDefaultH; return; }
		W = (N->NodeWidth  > 0) ? N->NodeWidth  : AnimNodeDefaultW;
		H = (N->NodeHeight > 0) ? N->NodeHeight : AnimNodeDefaultH;
	}

	/**
	 * Layer nodes by longest-path from the sink. Sinks are at layer 0 (rightmost
	 * visually); a node's layer = max(consumer.layer) + 1. Any node not
	 * reachable from a sink is floated to the current max layer + 1.
	 */
	int32 AssignLayersByPoseFlow(UEdGraph* Graph, TMap<UEdGraphNode*, int32>& OutLayer,
		TArray<FString>& Warnings)
	{
		TArray<UEdGraphNode*> Queue;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			if (IsAnimSink(N))
			{
				OutLayer.Add(N, 0);
				Queue.Add(N);
			}
		}

		// If there are no sinks (state's inner graph before connecting), seed
		// from the first node so we still layer something sensible.
		if (Queue.Num() == 0)
		{
			for (UEdGraphNode* N : Graph->Nodes)
			{
				if (N) { OutLayer.Add(N, 0); Queue.Add(N); break; }
			}
		}

		int32 MaxLayer = 0;
		int32 Safety = Graph->Nodes.Num() * 8 + 16;
		while (Queue.Num() > 0 && Safety-- > 0)
		{
			UEdGraphNode* Node = Queue.Pop(EAllowShrinking::No);
			const int32 CurLayer = OutLayer[Node];

			// Walk upstream producers via input pins' LinkedTo.
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input) continue;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Producer = Linked->GetOwningNode();
					if (!Producer) continue;
					const int32 Want = CurLayer + 1;
					int32* Got = OutLayer.Find(Producer);
					if (!Got || Want > *Got)
					{
						OutLayer.Add(Producer, Want);
						MaxLayer = FMath::Max(MaxLayer, Want);
						Queue.Add(Producer);
					}
				}
			}
		}
		if (Safety < 0)
		{
			Warnings.Add(TEXT("layer assignment hit iteration cap — possible cycle"));
		}

		// Unreachable nodes: park at MaxLayer + 1 so they don't overlap.
		int32 UnreachLayer = MaxLayer + 1;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			if (!OutLayer.Contains(N))
			{
				OutLayer.Add(N, UnreachLayer);
			}
		}
		if (UnreachLayer > MaxLayer) MaxLayer = UnreachLayer;
		return MaxLayer + 1;
	}
}

FBridgeAnimLayoutResult UUnrealBridgeAnimLibrary::AutoLayoutAnimGraph(
	const FString& AnimBlueprintPath, const FString& GraphName,
	int32 HorizontalSpacing, int32 VerticalSpacing)
{
	using namespace BridgeAnimWriteImpl;
	using namespace BridgeAnimLayoutImpl;

	FBridgeAnimLayoutResult Result;
	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) { Result.Warnings.Add(TEXT("anim blueprint not found")); return Result; }
	UEdGraph* Graph = FindAnyGraphByName(ABP, GraphName);
	if (!Graph) { Result.Warnings.Add(TEXT("graph not found")); return Result; }

	const int32 HSpace = HorizontalSpacing > 0 ? HorizontalSpacing : 100;
	const int32 VSpace = VerticalSpacing   > 0 ? VerticalSpacing   : 60;

	TMap<UEdGraphNode*, int32> LayerOf;
	const int32 LayerCount = AssignLayersByPoseFlow(Graph, LayerOf, Result.Warnings);

	// Group nodes by layer, sort by current NodePosY for stable ordering.
	TArray<TArray<UEdGraphNode*>> Layers;
	Layers.SetNum(LayerCount);
	for (auto& Pair : LayerOf)
	{
		Layers[Pair.Value].Add(Pair.Key);
	}
	for (TArray<UEdGraphNode*>& L : Layers)
	{
		L.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
		{
			return A.NodePosY < B.NodePosY;
		});
	}

	// Determine per-layer max width (so narrower layers don't waste space).
	TArray<int32> LayerMaxW;
	LayerMaxW.SetNumZeroed(LayerCount);
	for (int32 L = 0; L < LayerCount; ++L)
	{
		for (UEdGraphNode* N : Layers[L])
		{
			int32 W = 0, H = 0;
			EstimateNodeSize(N, W, H);
			if (W > LayerMaxW[L]) LayerMaxW[L] = W;
		}
	}

	// Compute per-layer X: layer 0 is rightmost (sink). X decreases as layer
	// grows.  Working right-to-left is natural here because sinks are the
	// anchor point of the AnimGraph.
	TArray<int32> LayerX;
	LayerX.SetNumZeroed(LayerCount);
	int32 RightX = 0;
	for (int32 L = 0; L < LayerCount; ++L)
	{
		LayerX[L] = RightX - LayerMaxW[L];
		RightX = LayerX[L] - HSpace;
	}

	// Anchor origin: preserve top-left of graph so laid-out nodes stay
	// visually near where they were.
	int32 MinOrigX = INT32_MAX, MinOrigY = INT32_MAX;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N) continue;
		MinOrigX = FMath::Min(MinOrigX, N->NodePosX);
		MinOrigY = FMath::Min(MinOrigY, N->NodePosY);
	}
	if (MinOrigX == INT32_MAX) { MinOrigX = 0; MinOrigY = 0; }

	// Assign positions. The leftmost layer gets X = MinOrigX; shift everything
	// so LayerX[LayerCount-1] lines up there.
	int32 LeftmostX = (LayerCount > 0) ? LayerX[LayerCount - 1] : 0;
	int32 DeltaX = MinOrigX - LeftmostX;

	Graph->Modify();
	int32 MinFinalX = INT32_MAX, MinFinalY = INT32_MAX;
	int32 MaxFinalX = INT32_MIN, MaxFinalY = INT32_MIN;
	int32 Count = 0;
	for (int32 L = 0; L < LayerCount; ++L)
	{
		int32 Y = MinOrigY;
		for (UEdGraphNode* N : Layers[L])
		{
			int32 W = 0, H = 0;
			EstimateNodeSize(N, W, H);
			N->Modify();
			N->NodePosX = LayerX[L] + DeltaX;
			N->NodePosY = Y;
			Y += H + VSpace;
			++Count;
			MinFinalX = FMath::Min(MinFinalX, N->NodePosX);
			MinFinalY = FMath::Min(MinFinalY, N->NodePosY);
			MaxFinalX = FMath::Max(MaxFinalX, N->NodePosX + W);
			MaxFinalY = FMath::Max(MaxFinalY, N->NodePosY + H);
		}
	}

	BridgeAnimWriteImpl::FinalizeEdit(ABP, Graph);

	Result.bSucceeded = true;
	Result.NodesPositioned = Count;
	Result.LayerCount = LayerCount;
	Result.BoundsWidth  = (MinFinalX == INT32_MAX) ? 0 : (MaxFinalX - MinFinalX);
	Result.BoundsHeight = (MinFinalY == INT32_MAX) ? 0 : (MaxFinalY - MinFinalY);
	return Result;
}

FBridgeAnimLayoutResult UUnrealBridgeAnimLibrary::AutoLayoutStateMachine(
	const FString& AnimBlueprintPath, const FString& StateMachineGraphName,
	int32 HorizontalSpacing, int32 VerticalSpacing)
{
	using namespace BridgeAnimWriteImpl;
	using namespace BridgeAnimLayoutImpl;

	FBridgeAnimLayoutResult Result;
	UAnimBlueprint* ABP = BridgeAnimImpl::LoadABP(AnimBlueprintPath);
	if (!ABP) { Result.Warnings.Add(TEXT("anim blueprint not found")); return Result; }
	UAnimationStateMachineGraph* SMGraph = FindStateMachineGraph(ABP, StateMachineGraphName);
	if (!SMGraph) { Result.Warnings.Add(TEXT("state machine graph not found")); return Result; }

	const int32 HSpace = HorizontalSpacing > 0 ? HorizontalSpacing : 300;
	const int32 VSpace = VerticalSpacing   > 0 ? VerticalSpacing   : 200;

	// Collect states / conduits (transitions / entry follow along via pins).
	TArray<UAnimStateNodeBase*> States;
	UAnimStateEntryNode* Entry = nullptr;
	TArray<UAnimStateTransitionNode*> Transitions;
	for (UEdGraphNode* N : SMGraph->Nodes)
	{
		if (!N) continue;
		if (UAnimStateEntryNode* E = Cast<UAnimStateEntryNode>(N)) { Entry = E; }
		else if (UAnimStateNodeBase* S = Cast<UAnimStateNodeBase>(N))
		{
			if (!N->IsA<UAnimStateTransitionNode>()) States.Add(S);
			else Transitions.Add(Cast<UAnimStateTransitionNode>(N));
		}
	}

	// Stable order by current (Y,X) so small local fixes don't blow up the layout.
	States.Sort([](const UAnimStateNodeBase& A, const UAnimStateNodeBase& B)
	{
		if (A.NodePosY != B.NodePosY) return A.NodePosY < B.NodePosY;
		return A.NodePosX < B.NodePosX;
	});

	// Grid arrangement: roughly square. Cols = ceil(sqrt(N)).
	const int32 N = States.Num();
	const int32 Cols = (N > 0) ? FMath::CeilToInt(FMath::Sqrt((float)N)) : 1;

	const int32 OriginX = Entry ? (Entry->NodePosX + 200) : 0;
	const int32 OriginY = Entry ? Entry->NodePosY : 0;

	SMGraph->Modify();
	int32 Count = 0;
	int32 MinFinalX = INT32_MAX, MinFinalY = INT32_MAX;
	int32 MaxFinalX = INT32_MIN, MaxFinalY = INT32_MIN;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Row = i / Cols;
		const int32 Col = i % Cols;
		UAnimStateNodeBase* S = States[i];
		S->Modify();
		S->NodePosX = OriginX + Col * HSpace;
		S->NodePosY = OriginY + Row * VSpace;
		++Count;
		MinFinalX = FMath::Min(MinFinalX, S->NodePosX);
		MinFinalY = FMath::Min(MinFinalY, S->NodePosY);
		MaxFinalX = FMath::Max(MaxFinalX, S->NodePosX + StateNodeDefaultW);
		MaxFinalY = FMath::Max(MaxFinalY, S->NodePosY + StateNodeDefaultH);
	}

	// Transitions: park at midpoint of their endpoints.
	for (UAnimStateTransitionNode* T : Transitions)
	{
		UAnimStateNodeBase* P = T->GetPreviousState();
		UAnimStateNodeBase* Q = T->GetNextState();
		if (!P || !Q) continue;
		T->Modify();
		T->NodePosX = (P->NodePosX + Q->NodePosX) / 2 + StateNodeDefaultW / 2;
		T->NodePosY = (P->NodePosY + Q->NodePosY) / 2 + StateNodeDefaultH / 2;
	}

	// Lay out every inner graph too (state inner anim graph, transition rules,
	// conduit rules). One call tidies the whole machine.
	for (UAnimStateNodeBase* S : States)
	{
		UEdGraph* Inner = nullptr;
		if (UAnimStateNode* AS = Cast<UAnimStateNode>(S)) Inner = AS->BoundGraph;
		else if (UAnimStateConduitNode* AC = Cast<UAnimStateConduitNode>(S)) Inner = AC->BoundGraph;
		if (Inner)
		{
			AutoLayoutAnimGraph(AnimBlueprintPath, Inner->GetName(), HorizontalSpacing, VerticalSpacing);
		}
	}
	for (UAnimStateTransitionNode* T : Transitions)
	{
		if (T->BoundGraph)
		{
			AutoLayoutAnimGraph(AnimBlueprintPath, T->BoundGraph->GetName(), HorizontalSpacing, VerticalSpacing);
		}
	}

	BridgeAnimWriteImpl::FinalizeEdit(ABP, SMGraph);

	Result.bSucceeded = true;
	Result.NodesPositioned = Count;
	Result.LayerCount = Cols;
	Result.BoundsWidth  = (MinFinalX == INT32_MAX) ? 0 : (MaxFinalX - MinFinalX);
	Result.BoundsHeight = (MinFinalY == INT32_MAX) ? 0 : (MaxFinalY - MinFinalY);
	return Result;
}

#undef LOCTEXT_NAMESPACE

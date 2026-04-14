#include "UnrealBridgeAnimLibrary.h"
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
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateConduitNode.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Text.h"
#include "Animation/BlendProfile.h"
#include "Animation/AnimTypes.h"

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

	// Get internal FAnimNode and dump non-default properties
	FAnimNode_Base* FNode = AnimNode->GetFNode();
	UScriptStruct* NodeStruct = AnimNode->GetFNodeType();
	if (!FNode || !NodeStruct) return Result;

	// Create CDO-equivalent from struct defaults
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
			Result.Add(FString::Printf(TEXT("%s (%s) = %s"),
				*Prop->GetName(), *Prop->GetCPPType(), *ExportedValue));
		}
	}

	NodeStruct->DestroyStruct(DefaultData.GetData());
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

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

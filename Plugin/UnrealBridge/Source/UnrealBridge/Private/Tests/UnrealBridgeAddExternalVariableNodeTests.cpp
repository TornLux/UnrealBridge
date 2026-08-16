#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Count.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UnrealBridgeBlueprintLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

namespace UnrealBridgeAddExternalVariableNodeTests
{
	UK2Node_Variable* FindVariableNodeByGuid(UEdGraph* Graph, const FString& GuidString)
	{
		FGuid Guid;
		if (!Graph || !FGuid::Parse(GuidString, Guid))
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == Guid)
			{
				return Cast<UK2Node_Variable>(Node);
			}
		}
		return nullptr;
	}

	// 只统计图直接拥有且未标记为垃圾的变量节点，用于证明 preflight candidate 已在返回前清理。
	// Count only live variable nodes directly owned by the graph to prove preflight candidates are cleaned before return.
	int32 CountLiveGraphOwnedVariableNodes(UEdGraph* Graph)
	{
		TArray<UObject*> GraphChildren;
		GetObjectsWithOuter(
			Graph, GraphChildren, false, RF_NoFlags, EInternalObjectFlags::Garbage);
		return Algo::CountIf(GraphChildren, [](const UObject* Object)
		{
			return Object && Object->IsA<UK2Node_Variable>();
		});
	}

	// 测试对象全部放在 TransientPackage 中，并显式清除资产标志，避免保存资产或污染后续自动化用例。
	// Keep every fixture in the TransientPackage and explicitly clear asset flags so no asset is saved or retained by later tests.
	void CleanupTransientBlueprint(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		if (Blueprint->GeneratedClass)
		{
			Blueprint->GeneratedClass->ClearFlags(RF_Public | RF_Standalone);
			Blueprint->GeneratedClass->MarkAsGarbage();
		}
		if (Blueprint->SkeletonGeneratedClass && Blueprint->SkeletonGeneratedClass != Blueprint->GeneratedClass)
		{
			Blueprint->SkeletonGeneratedClass->ClearFlags(RF_Public | RF_Standalone);
			Blueprint->SkeletonGeneratedClass->MarkAsGarbage();
		}
		Blueprint->ClearFlags(RF_Public | RF_Standalone);
		Blueprint->MarkAsGarbage();
	}
}

// 通过纯瞬态蓝图覆盖生产入口的成功与拒绝路径，不依赖内容资产或磁盘状态。
// Exercise production success and rejection paths through a transient Blueprint without content assets or disk state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeAddExternalVariableNodeTransientTest,
	"UnrealBridge.Blueprint.AddExternalVariableNode.Transient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeAddExternalVariableNodeTransientTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeAddExternalVariableNodeTests;
	(void)Parameters;

	const FName BlueprintName = MakeUniqueObjectName(
		GetTransientPackage(), UBlueprint::StaticClass(), TEXT("UB_AddExternalVariableNode_Consumer"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		UObject::StaticClass(), GetTransientPackage(), BlueprintName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	if (!TestNotNull(TEXT("Create a transient consumer Blueprint"), Blueprint))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		CleanupTransientBlueprint(Blueprint);
	};

	const FName ProviderName = MakeUniqueObjectName(
		GetTransientPackage(), UBlueprint::StaticClass(), TEXT("UB_AddExternalVariableNode_Provider"));
	UBlueprint* ProviderBlueprint = FKismetEditorUtilities::CreateBlueprint(
		UObject::StaticClass(), GetTransientPackage(), ProviderName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	if (!TestNotNull(TEXT("Create a transient provider Blueprint"), ProviderBlueprint))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		CleanupTransientBlueprint(ProviderBlueprint);
	};

	const FName PrivateVariableName(TEXT("PrivateExternalValue"));
	FEdGraphPinType PrivateVariableType;
	PrivateVariableType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	if (!TestTrue(TEXT("Add a provider member variable"),
		FBlueprintEditorUtils::AddMemberVariable(ProviderBlueprint, PrivateVariableName, PrivateVariableType)))
	{
		return false;
	}
	FBlueprintEditorUtils::SetBlueprintVariableMetaData(
		ProviderBlueprint, PrivateVariableName, nullptr, FBlueprintMetadata::MD_Private, TEXT("true"));
	FKismetEditorUtilities::CompileBlueprint(ProviderBlueprint);
	if (!TestNotNull(TEXT("Compile the transient provider Blueprint"), ProviderBlueprint->GeneratedClass.Get()))
	{
		return false;
	}
	FProperty* PrivateProperty = FindFProperty<FProperty>(ProviderBlueprint->GeneratedClass, PrivateVariableName);
	if (!TestNotNull(TEXT("Compiled provider exposes the private reflected property"), PrivateProperty))
	{
		return false;
	}
	TestTrue(TEXT("Provider property carries BlueprintPrivate metadata"),
		PrivateProperty && PrivateProperty->GetBoolMetaData(FBlueprintMetadata::MD_Private));

	UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, TEXT("ExternalVariableTestGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!TestNotNull(TEXT("Create a transient K2 graph"), Graph))
	{
		return false;
	}
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);

	const FString BlueprintPath = Blueprint->GetPathName();
	const FString GraphName = Graph->GetName();
	const FString CharacterClassPath = ACharacter::StaticClass()->GetPathName();
	const int32 InitialNodeCount = Graph->Nodes.Num();

	const FString GetterGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, CharacterClassPath, TEXT("Tags"), false, 120, 240);
	UK2Node_Variable* GetterNode = FindVariableNodeByGuid(Graph, GetterGuid);
	TestTrue(TEXT("Getter returns a parseable non-empty GUID"), !GetterGuid.IsEmpty() && GetterNode != nullptr);
	TestTrue(TEXT("Getter creates UK2Node_VariableGet"), GetterNode && GetterNode->IsA<UK2Node_VariableGet>());
	TestTrue(TEXT("Getter node is transactional"), GetterNode && GetterNode->HasAnyFlags(RF_Transactional));
	TestEqual(TEXT("Getter call adds exactly one production node"), Graph->Nodes.Num(), InitialNodeCount + 1);
	TestEqual(TEXT("Getter preflight candidate is not a live graph-owned variable node"),
		CountLiveGraphOwnedVariableNodes(Graph), 1);

	if (GEditor)
	{
		TestTrue(TEXT("Undo getter transaction succeeds"), GEditor->UndoTransaction());
		TestEqual(TEXT("Undo getter removes exactly its created node"), Graph->Nodes.Num(), InitialNodeCount);
		TestNull(TEXT("Getter GUID is absent after Undo"), FindVariableNodeByGuid(Graph, GetterGuid));
		TestTrue(TEXT("Redo getter transaction succeeds"), GEditor->RedoTransaction());
	}
	else
	{
		AddError(TEXT("GEditor is required for behavioral Undo/Redo coverage"));
	}
	GetterNode = FindVariableNodeByGuid(Graph, GetterGuid);
	TestNotNull(TEXT("Redo restores the getter with the same GUID"), GetterNode);
	TestTrue(TEXT("Redo restores the getter member reference"), GetterNode
		&& GetterNode->VariableReference.GetMemberParentClass() == AActor::StaticClass()
		&& GetterNode->VariableReference.GetMemberName() == GET_MEMBER_NAME_CHECKED(AActor, Tags));
	TestTrue(TEXT("Redo restores getter target and value pins"), GetterNode
		&& GetterNode->FindPin(TEXT("self"))
		&& GetterNode->FindPin(TEXT("self"))->Direction == EGPD_Input
		&& GetterNode->FindPin(TEXT("Tags"))
		&& GetterNode->FindPin(TEXT("Tags"))->Direction == EGPD_Output);

	const FString SetterGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, CharacterClassPath, TEXT("Tags"), true, 560, 360);
	UK2Node_Variable* SetterNode = FindVariableNodeByGuid(Graph, SetterGuid);
	TestTrue(TEXT("Setter returns a parseable non-empty GUID"), !SetterGuid.IsEmpty() && SetterNode != nullptr);
	TestTrue(TEXT("Setter creates UK2Node_VariableSet"), SetterNode && SetterNode->IsA<UK2Node_VariableSet>());
	TestTrue(TEXT("Setter node is transactional"), SetterNode && SetterNode->HasAnyFlags(RF_Transactional));
	TestEqual(TEXT("Two successful calls add exactly two production nodes"), Graph->Nodes.Num(), InitialNodeCount + 2);
	TestEqual(TEXT("Setter preflight candidate is not a live graph-owned variable node"),
		CountLiveGraphOwnedVariableNodes(Graph), 2);

	if (GEditor)
	{
		TestTrue(TEXT("Undo setter transaction succeeds"), GEditor->UndoTransaction());
		TestEqual(TEXT("Undo setter leaves the getter intact"), Graph->Nodes.Num(), InitialNodeCount + 1);
		TestNull(TEXT("Setter GUID is absent after Undo"), FindVariableNodeByGuid(Graph, SetterGuid));
		TestNotNull(TEXT("Getter survives setter Undo"), FindVariableNodeByGuid(Graph, GetterGuid));
		TestTrue(TEXT("Redo setter transaction succeeds"), GEditor->RedoTransaction());
	}
	SetterNode = FindVariableNodeByGuid(Graph, SetterGuid);
	TestNotNull(TEXT("Redo restores the setter with the same GUID"), SetterNode);
	TestTrue(TEXT("Redo restores the setter member reference"), SetterNode
		&& SetterNode->VariableReference.GetMemberParentClass() == AActor::StaticClass()
		&& SetterNode->VariableReference.GetMemberName() == GET_MEMBER_NAME_CHECKED(AActor, Tags));
	TestTrue(TEXT("Redo restores setter target and value pins"), SetterNode
		&& SetterNode->FindPin(TEXT("self"))
		&& SetterNode->FindPin(TEXT("self"))->Direction == EGPD_Input
		&& SetterNode->FindPin(TEXT("Tags"))
		&& SetterNode->FindPin(TEXT("Tags"))->Direction == EGPD_Input);

	for (const TPair<UK2Node_Variable*, bool>& Case : {
		TPair<UK2Node_Variable*, bool>(GetterNode, false),
		TPair<UK2Node_Variable*, bool>(SetterNode, true)})
	{
		UK2Node_Variable* Node = Case.Key;
		const bool bIsSetter = Case.Value;
		if (!Node)
		{
			continue;
		}

		TestTrue(TEXT("External variable node remains transactional after Redo"), Node->HasAnyFlags(RF_Transactional));
		TestFalse(TEXT("External variable reference is not self context"), Node->VariableReference.IsSelfContext());
		TestEqual(TEXT("Inherited Tags reference binds to its declaring AActor class"),
			Node->VariableReference.GetMemberParentClass(), AActor::StaticClass());
		TestEqual(TEXT("External variable reference keeps the property name"),
			Node->VariableReference.GetMemberName(), GET_MEMBER_NAME_CHECKED(AActor, Tags));

		UEdGraphPin* SelfPin = Node->FindPin(TEXT("self"));
		UEdGraphPin* ValuePin = Node->FindPin(TEXT("Tags"));
		TestNotNull(TEXT("External node has a self target pin"), SelfPin);
		TestNotNull(TEXT("External node has a Tags value pin"), ValuePin);
		if (SelfPin)
		{
			TestEqual(TEXT("Self target pin is an input"), SelfPin->Direction, EGPD_Input);
		}
		if (ValuePin)
		{
			TestEqual(TEXT("Value pin direction matches getter or setter"),
				ValuePin->Direction, bIsSetter ? EGPD_Input : EGPD_Output);
		}
	}

	if (GetterNode)
	{
		TestEqual(TEXT("Getter X position is preserved"), GetterNode->NodePosX, 120);
		TestEqual(TEXT("Getter Y position is preserved"), GetterNode->NodePosY, 240);
	}
	if (SetterNode)
	{
		TestEqual(TEXT("Setter X position is preserved"), SetterNode->NodePosX, 560);
		TestEqual(TEXT("Setter Y position is preserved"), SetterNode->NodePosY, 360);
	}

	const int32 NodeCountBeforeFailures = Graph->Nodes.Num();
	// 拒绝路径按契约记录警告；将其声明为预期输出，避免成功的失败测试污染 Automation 结果。
	// Rejection paths log warnings by contract; declare them expected so successful negative coverage keeps a clean report.
	AddExpectedError(TEXT("AddExternalVariableNode failed:"), EAutomationExpectedErrorFlags::Contains, 5);
	AddExpectedError(TEXT("Failed to find object 'Class /Script/Engine.DefinitelyMissingOwnerClass'"),
		EAutomationExpectedErrorFlags::Contains, 1);

	const FString ProviderClassPath = ProviderBlueprint->GeneratedClass->GetPathName();
	const FString PrivateGetterGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, ProviderClassPath, PrivateVariableName.ToString(), false, 0, 0);
	TestTrue(TEXT("Getter rejects an unrelated provider's BlueprintPrivate property"), PrivateGetterGuid.IsEmpty());
	TestEqual(TEXT("Private getter rejection leaves node count unchanged"), Graph->Nodes.Num(), NodeCountBeforeFailures);

	const FString PrivateSetterGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, ProviderClassPath, PrivateVariableName.ToString(), true, 0, 0);
	TestTrue(TEXT("Setter rejects an unrelated provider's BlueprintPrivate property"), PrivateSetterGuid.IsEmpty());
	TestEqual(TEXT("Private setter rejection leaves node count unchanged"), Graph->Nodes.Num(), NodeCountBeforeFailures);

	const FString ReadOnlyGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, CharacterClassPath, TEXT("InitialLifeSpan"), true, 0, 0);
	TestTrue(TEXT("Setter rejects BlueprintReadOnly InitialLifeSpan"), ReadOnlyGuid.IsEmpty());
	TestEqual(TEXT("Read-only rejection leaves node count unchanged"), Graph->Nodes.Num(), NodeCountBeforeFailures);

	const FString MissingPropertyGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, CharacterClassPath, TEXT("DefinitelyMissingProperty"), false, 0, 0);
	TestTrue(TEXT("Missing property returns an empty GUID"), MissingPropertyGuid.IsEmpty());
	TestEqual(TEXT("Missing property leaves node count unchanged"), Graph->Nodes.Num(), NodeCountBeforeFailures);

	const FString MissingClassGuid = UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(
		BlueprintPath, GraphName, TEXT("/Script/Engine.DefinitelyMissingOwnerClass"), TEXT("Tags"), false, 0, 0);
	TestTrue(TEXT("Missing owner class returns an empty GUID"), MissingClassGuid.IsEmpty());
	TestEqual(TEXT("Missing owner class leaves node count unchanged"), Graph->Nodes.Num(), NodeCountBeforeFailures);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

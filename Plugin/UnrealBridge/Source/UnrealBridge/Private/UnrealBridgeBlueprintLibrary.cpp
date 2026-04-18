#include "UnrealBridgeBlueprintLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Self.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Message.h"
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Select.h"
#include "K2Node_Knot.h"
#include "EdGraphNode_Comment.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"
#include "GameFramework/Actor.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditor.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/Breakpoint.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeArray.h"
#include "K2Node_EnumLiteral.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "K2Node.h"

// ─── Helpers ─────────────────────────────────────────────────

static UBlueprint* LoadBP(const FString& Path)
{
	return LoadObject<UBlueprint>(nullptr, *Path);
}

static FBridgeClassInfo MakeClassInfo(const UClass* InClass)
{
	FBridgeClassInfo Info;
	if (!InClass) return Info;

	Info.ClassName = InClass->GetName();
	Info.ClassPath = InClass->GetPathName();
	Info.bIsNative = !InClass->IsChildOf<UBlueprintGeneratedClass>()
	              && !InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
	return Info;
}

/** Convert an FProperty's type to a human-readable string. */
static FString PropertyTypeToString(const FProperty* Prop)
{
	if (!Prop) return TEXT("Unknown");

	// Array
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		return FString::Printf(TEXT("Array of %s"), *PropertyTypeToString(ArrayProp->Inner));
	}
	// Set
	if (const FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		return FString::Printf(TEXT("Set of %s"), *PropertyTypeToString(SetProp->ElementProp));
	}
	// Map
	if (const FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		return FString::Printf(TEXT("Map<%s, %s>"),
			*PropertyTypeToString(MapProp->KeyProp),
			*PropertyTypeToString(MapProp->ValueProp));
	}
	// Object reference
	if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		UClass* ObjClass = ObjProp->PropertyClass;
		return ObjClass ? ObjClass->GetName() : TEXT("Object");
	}
	// Struct
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		return StructProp->Struct ? StructProp->Struct->GetName() : TEXT("Struct");
	}
	// Enum
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		UEnum* Enum = EnumProp->GetEnum();
		return Enum ? Enum->GetName() : TEXT("Enum");
	}
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		if (ByteProp->Enum)
		{
			return ByteProp->Enum->GetName();
		}
		return TEXT("Byte");
	}
	// Class/SoftClass reference
	if (const FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
	{
		UClass* MetaClass = ClassProp->MetaClass;
		return FString::Printf(TEXT("Class<%s>"), MetaClass ? *MetaClass->GetName() : TEXT("Object"));
	}
	// Delegate
	if (CastField<FDelegateProperty>(Prop))
	{
		return TEXT("Delegate");
	}
	if (CastField<FMulticastDelegateProperty>(Prop))
	{
		return TEXT("MulticastDelegate");
	}

	// Primitives
	if (CastField<FBoolProperty>(Prop))       return TEXT("Bool");
	if (CastField<FIntProperty>(Prop))         return TEXT("Int");
	if (CastField<FInt64Property>(Prop))       return TEXT("Int64");
	if (CastField<FFloatProperty>(Prop))       return TEXT("Float");
	if (CastField<FDoubleProperty>(Prop))      return TEXT("Double");
	if (CastField<FStrProperty>(Prop))         return TEXT("String");
	if (CastField<FNameProperty>(Prop))        return TEXT("Name");
	if (CastField<FTextProperty>(Prop))        return TEXT("Text");

	return Prop->GetCPPType();
}

/** Best-effort export of a property's default value from a CDO. */
static FString GetDefaultValueString(const FProperty* Prop, const UObject* CDO)
{
	if (!Prop || !CDO) return FString();

	FString Result;
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);
	Prop->ExportTextItem_Direct(Result, ValuePtr, nullptr, nullptr, PPF_None);
	return Result;
}

// ─── Class Hierarchy (existing) ──────────────────────────────

bool UUnrealBridgeBlueprintLibrary::GetBlueprintParentClass(
	const FString& BlueprintPath, FBridgeClassInfo& OutParentInfo)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	UClass* Super = BP->GeneratedClass->GetSuperClass();
	if (!Super) return false;

	OutParentInfo = MakeClassInfo(Super);
	return true;
}

TArray<FBridgeClassInfo> UUnrealBridgeBlueprintLibrary::GetBlueprintClassHierarchy(
	const FString& BlueprintPath)
{
	TArray<FBridgeClassInfo> Hierarchy;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Hierarchy;

	for (const UClass* Cur = BP->GeneratedClass; Cur; Cur = Cur->GetSuperClass())
	{
		Hierarchy.Add(MakeClassInfo(Cur));
	}
	return Hierarchy;
}

// ─── Variables ───────────────────────────────────────────────

TArray<FBridgeVariableInfo> UUnrealBridgeBlueprintLibrary::GetBlueprintVariables(
	const FString& BlueprintPath, bool bIncludeInherited)
{
	TArray<FBridgeVariableInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Result;

	const UClass* GenClass = BP->GeneratedClass;
	const UObject* CDO = GenClass->GetDefaultObject();

	// Collect the set of "new variables" names defined by this BP
	TSet<FName> OwnVariableNames;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		OwnVariableNames.Add(Var.VarName);
	}

	// Iterate the BP's NewVariables for metadata, match with FProperty for type/default
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		const FProperty* Prop = GenClass->FindPropertyByName(Var.VarName);

		FBridgeVariableInfo Info;
		Info.Name = Var.VarName.ToString();
		Info.Type = Prop ? PropertyTypeToString(Prop) : Var.VarType.PinCategory.ToString();
		Info.Category = Var.Category.ToString();
		if (Var.HasMetaData(TEXT("tooltip")))
		{
			Info.Description = Var.GetMetaData(TEXT("tooltip"));
		}
		Info.DefaultValue = Prop ? GetDefaultValueString(Prop, CDO) : Var.DefaultValue;
		Info.bInstanceEditable = Var.PropertyFlags & CPF_Edit ? true : false;
		Info.bBlueprintReadOnly = Var.PropertyFlags & CPF_BlueprintReadOnly ? true : false;

		if (Var.PropertyFlags & CPF_Net)
		{
			Info.ReplicationCondition = (Var.PropertyFlags & CPF_RepNotify)
				? TEXT("RepNotify")
				: TEXT("Replicated");
		}
		else
		{
			Info.ReplicationCondition = TEXT("None");
		}

		Result.Add(Info);
	}

	// Optionally include inherited variables
	if (bIncludeInherited)
	{
		const UClass* SuperClass = GenClass->GetSuperClass();
		for (TFieldIterator<FProperty> It(SuperClass); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Parm) && !OwnVariableNames.Contains(Prop->GetFName()))
			{
				// Skip internal/hidden properties
				if (Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance) && !Prop->HasAnyPropertyFlags(CPF_Edit))
				{
					continue;
				}

				FBridgeVariableInfo Info;
				Info.Name = Prop->GetName();
				Info.Type = PropertyTypeToString(Prop);

				if (const FString* Cat = Prop->FindMetaData(TEXT("Category")))
				{
					Info.Category = *Cat;
				}

				const UObject* SuperCDO = SuperClass->GetDefaultObject();
				Info.DefaultValue = GetDefaultValueString(Prop, SuperCDO);
				Info.bInstanceEditable = Prop->HasAnyPropertyFlags(CPF_Edit);
				Info.bBlueprintReadOnly = Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);

				if (Prop->HasAnyPropertyFlags(CPF_Net))
				{
					Info.ReplicationCondition = Prop->HasAnyPropertyFlags(CPF_RepNotify)
						? TEXT("RepNotify") : TEXT("Replicated");
				}
				else
				{
					Info.ReplicationCondition = TEXT("None");
				}

				Result.Add(Info);
			}
		}
	}

	return Result;
}

// ─── Functions / Events ──────────────────────────────────────

TArray<FBridgeFunctionInfo> UUnrealBridgeBlueprintLibrary::GetBlueprintFunctions(
	const FString& BlueprintPath, bool bIncludeInherited)
{
	TArray<FBridgeFunctionInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Result;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* SuperClass = GenClass->GetSuperClass();

	for (TFieldIterator<UFunction> It(GenClass, bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const UFunction* Func = *It;
		if (!Func) continue;

		// Skip hidden/internal functions
		FString FuncName = Func->GetName();
		if (FuncName.StartsWith(TEXT("ExecuteUbergraph"))) continue;
		if (FuncName.StartsWith(TEXT("UserConstructionScript")) && !bIncludeInherited) continue;

		FBridgeFunctionInfo Info;
		Info.Name = FuncName;

		// Determine kind
		if (Func->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent))
		{
			// Check if it's an override of a parent event
			if (SuperClass && SuperClass->FindFunctionByName(Func->GetFName()))
			{
				Info.Kind = TEXT("Override");
			}
			else
			{
				Info.Kind = TEXT("Event");
			}
		}
		else
		{
			Info.Kind = TEXT("Function");
		}

		// Access
		if (Func->HasAnyFunctionFlags(FUNC_Public))
			Info.Access = TEXT("Public");
		else if (Func->HasAnyFunctionFlags(FUNC_Protected))
			Info.Access = TEXT("Protected");
		else
			Info.Access = TEXT("Private");

		Info.bIsPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
		Info.bIsStatic = Func->HasAnyFunctionFlags(FUNC_Static);

		// Category
		Info.Category = Func->GetMetaData(TEXT("Category"));

		// Description
		Info.Description = Func->GetMetaData(TEXT("ToolTip"));

		// Parameters — only include real function params, not BP graph locals
		for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
		{
			const FProperty* Param = *ParamIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}

			FBridgeFunctionParam P;
			P.Name = Param->GetName();
			P.Type = PropertyTypeToString(Param);
			P.bIsOutput = Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
			Info.Params.Add(P);
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── Components ──────────────────────────────────────────────

TArray<FBridgeComponentInfo> UUnrealBridgeBlueprintLibrary::GetBlueprintComponents(
	const FString& BlueprintPath)
{
	TArray<FBridgeComponentInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	// Components from SimpleConstructionScript (this BP's own components)
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	if (SCS)
	{
		const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
		for (const USCS_Node* Node : AllNodes)
		{
			if (!Node || !Node->ComponentClass) continue;

			FBridgeComponentInfo Info;
			Info.Name = Node->GetVariableName().ToString();
			Info.ComponentClass = Node->ComponentClass->GetName();
			Info.bIsInherited = false;

			// Find parent
			if (Node->ParentComponentOrVariableName != NAME_None)
			{
				Info.ParentName = Node->ParentComponentOrVariableName.ToString();
			}
			else
			{
				// Check if it's a root node
				const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
				Info.bIsRoot = RootNodes.Contains(Node);
			}

			Result.Add(Info);
		}
	}

	// Inherited components from parent CDO
	const UClass* SuperClass = BP->ParentClass;
	if (SuperClass)
	{
		const UObject* SuperCDO = SuperClass->GetDefaultObject();
		if (SuperCDO)
		{
			TArray<UActorComponent*> InheritedComponents;
			// Get components from the parent CDO
			if (const AActor* ActorCDO = Cast<AActor>(SuperCDO))
			{
				ActorCDO->GetComponents(InheritedComponents);
				for (const UActorComponent* Comp : InheritedComponents)
				{
					FBridgeComponentInfo Info;
					Info.Name = Comp->GetName();
					Info.ComponentClass = Comp->GetClass()->GetName();
					Info.bIsInherited = true;
					Info.bIsRoot = (Comp == ActorCDO->GetRootComponent());
					Result.Add(Info);
				}
			}
		}
	}

	return Result;
}

// ─── Interfaces ──────────────────────────────────────────────

TArray<FBridgeInterfaceInfo> UUnrealBridgeBlueprintLibrary::GetBlueprintInterfaces(
	const FString& BlueprintPath)
{
	TArray<FBridgeInterfaceInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
	{
		FBridgeInterfaceInfo Info;

		UClass* IfaceClass = Iface.Interface;
		if (!IfaceClass) continue;

		Info.InterfaceName = IfaceClass->GetName();
		Info.InterfacePath = IfaceClass->GetPathName();

		// Check if the interface itself is Blueprint-generated
		Info.bIsBlueprintImplemented = IfaceClass->IsChildOf<UBlueprintGeneratedClass>()
			|| IfaceClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

		// List functions declared by this interface
		for (TFieldIterator<UFunction> It(IfaceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const UFunction* Func = *It;
			if (Func && !Func->GetName().StartsWith(TEXT("ExecuteUbergraph")))
			{
				Info.Functions.Add(Func->GetName());
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── Graph helpers ───────────────────────────────────────────

/** Find graphs matching a function name in a Blueprint. Empty name = EventGraph / UbergraphPages. */
static TArray<UEdGraph*> FindGraphs(UBlueprint* BP, const FString& FunctionName)
{
	TArray<UEdGraph*> Graphs;

	if (FunctionName.IsEmpty())
	{
		Graphs = BP->UbergraphPages;
	}
	else
	{
		for (UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				Graphs.Add(Graph);
				return Graphs;
			}
		}
		for (UEdGraph* Graph : BP->UbergraphPages)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				Graphs.Add(Graph);
				return Graphs;
			}
		}
		for (UEdGraph* Graph : BP->MacroGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				Graphs.Add(Graph);
				return Graphs;
			}
		}
	}

	return Graphs;
}

/** Classify a node into a simple type string. */
static FString ClassifyNode(const UEdGraphNode* Node)
{
	if (Cast<UK2Node_CallFunction>(Node))       return TEXT("FunctionCall");
	if (Cast<UK2Node_VariableGet>(Node))        return TEXT("VariableGet");
	if (Cast<UK2Node_VariableSet>(Node))        return TEXT("VariableSet");
	if (Cast<UK2Node_IfThenElse>(Node))         return TEXT("Branch");
	if (Cast<UK2Node_DynamicCast>(Node))        return TEXT("Cast");
	if (Cast<UK2Node_MacroInstance>(Node))       return TEXT("Macro");
	if (Cast<UK2Node_CustomEvent>(Node))        return TEXT("Event");
	if (Cast<UK2Node_Event>(Node))              return TEXT("Event");
	if (Cast<UK2Node_FunctionEntry>(Node))      return TEXT("FunctionEntry");

	FString ClassName = Node->GetClass()->GetName();
	ClassName.RemoveFromStart(TEXT("K2Node_"));
	return ClassName;
}

// ─── GetFunctionCallGraph ────────────────────────────────────

TArray<FBridgeCallEdge> UUnrealBridgeBlueprintLibrary::GetFunctionCallGraph(
	const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FBridgeCallEdge> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);
	TSet<FString> Seen;

	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				FBridgeCallEdge Edge;
				Edge.TargetName = CallNode->GetFunctionName().ToString();
				Edge.TargetKind = TEXT("Function");

				UFunction* TargetFunc = CallNode->GetTargetFunction();
				if (TargetFunc && TargetFunc->GetOwnerClass())
				{
					Edge.TargetClass = TargetFunc->GetOwnerClass()->GetName();
				}

				FString Key = Edge.TargetClass + TEXT("::") + Edge.TargetName;
				if (!Seen.Contains(Key))
				{
					Seen.Add(Key);
					Result.Add(Edge);
				}
			}
			else if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
			{
				FBridgeCallEdge Edge;
				UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
				Edge.TargetName = MacroGraph ? MacroGraph->GetName() : TEXT("Unknown");
				Edge.TargetClass = TEXT("Macro");
				Edge.TargetKind = TEXT("Macro");

				FString Key = Edge.TargetKind + TEXT("::") + Edge.TargetName;
				if (!Seen.Contains(Key))
				{
					Seen.Add(Key);
					Result.Add(Edge);
				}
			}
		}
	}

	return Result;
}

// ─── GetFunctionNodes ────────────────────────────────────────

TArray<FBridgeNodeInfo> UUnrealBridgeBlueprintLibrary::GetFunctionNodes(
	const FString& BlueprintPath, const FString& FunctionName, const FString& NodeTypeFilter)
{
	TArray<FBridgeNodeInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);

	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			FString NodeType = ClassifyNode(Node);

			if (!NodeTypeFilter.IsEmpty() && NodeType != NodeTypeFilter)
			{
				continue;
			}

			FBridgeNodeInfo Info;
			Info.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			Info.NodeType = NodeType;
			Info.Comment = Node->NodeComment;
			Info.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);

			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				UFunction* TargetFunc = CallNode->GetTargetFunction();
				if (TargetFunc && TargetFunc->GetOwnerClass())
				{
					Info.TargetClass = TargetFunc->GetOwnerClass()->GetName();
				}
			}
			else if (const UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
			{
				Info.VariableName = VarNode->GetVarNameString();
			}
			else if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				if (CastNode->TargetType)
				{
					Info.TargetClass = CastNode->TargetType->GetName();
				}
			}
			else if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
			{
				UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
				if (MacroGraph)
				{
					Info.Title = MacroGraph->GetName();
				}
			}

			Result.Add(Info);
		}
	}

	return Result;
}

// ─── GetBlueprintOverview ───────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::GetBlueprintOverview(
	const FString& BlueprintPath, FBridgeBlueprintOverview& OutOverview)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* Super = GenClass->GetSuperClass();

	OutOverview.BlueprintName = BP->GetName();
	OutOverview.ParentClass = MakeClassInfo(Super);

	// Determine BP type from first native ancestor
	const UClass* NativeBase = Super;
	while (NativeBase
		&& (NativeBase->IsChildOf<UBlueprintGeneratedClass>()
			|| NativeBase->HasAnyClassFlags(CLASS_CompiledFromBlueprint)))
	{
		NativeBase = NativeBase->GetSuperClass();
	}
	OutOverview.BlueprintType = NativeBase ? NativeBase->GetName() : TEXT("Object");

	// ── Compact variables (skip event dispatchers) ──
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		const FProperty* Prop = GenClass->FindPropertyByName(Var.VarName);
		if (Prop && CastField<FMulticastDelegateProperty>(Prop))
			continue;

		FBridgeVariableSummary VS;
		VS.Name = Var.VarName.ToString();
		VS.Type = Prop ? PropertyTypeToString(Prop) : Var.VarType.PinCategory.ToString();
		OutOverview.Variables.Add(VS);
	}

	// ── Compact functions ──
	for (TFieldIterator<UFunction> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const UFunction* Func = *It;
		if (!Func) continue;
		FString FuncName = Func->GetName();
		if (FuncName.StartsWith(TEXT("ExecuteUbergraph"))) continue;
		if (FuncName.Contains(TEXT("__DelegateSignature"))) continue;

		FBridgeFunctionSummary FS;
		FS.Name = FuncName;

		if (Func->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent))
		{
			FS.Kind = (Super && Super->FindFunctionByName(Func->GetFName()))
				? TEXT("Override") : TEXT("Event");
		}
		else
		{
			FS.Kind = TEXT("Function");
		}

		// Build compact signature
		TArray<FString> InParams, OutParams;
		for (TFieldIterator<FProperty> PIt(Func); PIt; ++PIt)
		{
			const FProperty* Param = *PIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;
			FString T = PropertyTypeToString(Param);
			if (Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
				OutParams.Add(T);
			else
				InParams.Add(T);
		}
		FS.Signature = TEXT("(") + FString::Join(InParams, TEXT(", ")) + TEXT(")");
		if (OutParams.Num() > 0)
			FS.Signature += TEXT(" -> ") + FString::Join(OutParams, TEXT(", "));

		OutOverview.Functions.Add(FS);
	}

	// ── Compact components ──
	if (USimpleConstructionScript* SCS = BP->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (!Node || !Node->ComponentClass) continue;
			FBridgeComponentSummary CS;
			CS.Name = Node->GetVariableName().ToString();
			CS.ComponentClass = Node->ComponentClass->GetName();
			if (Node->ParentComponentOrVariableName != NAME_None)
				CS.ParentName = Node->ParentComponentOrVariableName.ToString();
			OutOverview.Components.Add(CS);
		}
	}

	// ── Interface names ──
	for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
	{
		if (Iface.Interface)
			OutOverview.Interfaces.Add(Iface.Interface->GetName());
	}

	// ── Event dispatcher names ──
	for (UEdGraph* SigGraph : BP->DelegateSignatureGraphs)
	{
		if (!SigGraph) continue;
		FString Name = SigGraph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (Name.EndsWith(DelegateSuffix))
			Name = Name.LeftChop(DelegateSuffix.Len());
		OutOverview.EventDispatchers.Add(Name);
	}

	// ── Graph names ──
	for (UEdGraph* G : BP->UbergraphPages)
		if (G) OutOverview.GraphNames.Add(G->GetName());
	for (UEdGraph* G : BP->FunctionGraphs)
		if (G) OutOverview.GraphNames.Add(G->GetName());
	for (UEdGraph* G : BP->MacroGraphs)
		if (G) OutOverview.GraphNames.Add(G->GetName());

	return true;
}

// ─── GetEventDispatchers ────────────────────────────────────

TArray<FBridgeEventDispatcherInfo> UUnrealBridgeBlueprintLibrary::GetEventDispatchers(
	const FString& BlueprintPath)
{
	TArray<FBridgeEventDispatcherInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Result;

	const UClass* GenClass = BP->GeneratedClass;

	for (UEdGraph* SigGraph : BP->DelegateSignatureGraphs)
	{
		if (!SigGraph) continue;

		FBridgeEventDispatcherInfo Info;
		FString GraphName = SigGraph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (GraphName.EndsWith(DelegateSuffix))
			Info.Name = GraphName.LeftChop(DelegateSuffix.Len());
		else
			Info.Name = GraphName;

		// Get params from the delegate property's signature function
		FProperty* Prop = GenClass->FindPropertyByName(FName(*Info.Name));
		FMulticastDelegateProperty* MCDProp = CastField<FMulticastDelegateProperty>(Prop);
		if (MCDProp && MCDProp->SignatureFunction)
		{
			for (TFieldIterator<FProperty> PIt(MCDProp->SignatureFunction); PIt; ++PIt)
			{
				const FProperty* Param = *PIt;
				if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;

				FBridgeFunctionParam P;
				P.Name = Param->GetName();
				P.Type = PropertyTypeToString(Param);
				P.bIsOutput = Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
				Info.Params.Add(P);
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── GetGraphNames ──────────────────────────────────────────

TArray<FBridgeGraphInfo> UUnrealBridgeBlueprintLibrary::GetGraphNames(
	const FString& BlueprintPath)
{
	TArray<FBridgeGraphInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		if (!Graph) continue;
		FBridgeGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = TEXT("EventGraph");
		Result.Add(Info);
	}

	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		if (!Graph) continue;
		FBridgeGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = TEXT("Function");
		Result.Add(Info);
	}

	for (UEdGraph* Graph : BP->MacroGraphs)
	{
		if (!Graph) continue;
		FBridgeGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = TEXT("Macro");
		Result.Add(Info);
	}

	for (UEdGraph* Graph : BP->DelegateSignatureGraphs)
	{
		if (!Graph) continue;
		FBridgeGraphInfo Info;
		FString Name = Graph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (Name.EndsWith(DelegateSuffix))
			Name = Name.LeftChop(DelegateSuffix.Len());
		Info.Name = Name;
		Info.GraphType = TEXT("EventDispatcher");
		Result.Add(Info);
	}

	return Result;
}

// ─── Exec flow helper ───────────────────────────────────────

/** Get a compact detail string for a node (function name, variable, cast target, etc.). */
static FString GetNodeDetail(const UEdGraphNode* Node)
{
	if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		UFunction* Func = CallNode->GetTargetFunction();
		if (Func && Func->GetOwnerClass())
			return Func->GetOwnerClass()->GetName() + TEXT("::") + CallNode->GetFunctionName().ToString();
		return CallNode->GetFunctionName().ToString();
	}
	if (const UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
	{
		return VarNode->GetVarNameString();
	}
	if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
	{
		return CastNode->TargetType ? CastNode->TargetType->GetName() : FString();
	}
	if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
	{
		UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
		return MacroGraph ? MacroGraph->GetName() : FString();
	}
	if (const UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node))
	{
		return EventNode->CustomFunctionName.ToString();
	}
	if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		return EventNode->GetFunctionName().ToString();
	}
	return FString();
}

// ─── GetFunctionExecutionFlow ───────────────────────────────

TArray<FBridgeExecStep> UUnrealBridgeBlueprintLibrary::GetFunctionExecutionFlow(
	const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FBridgeExecStep> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);

	// Collect all nodes across matched graphs
	TArray<UEdGraphNode*> AllNodes;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
			AllNodes.Append(Graph->Nodes);
	}

	// Find entry points: FunctionEntry, Event, CustomEvent
	TArray<UEdGraphNode*> EntryNodes;
	for (UEdGraphNode* Node : AllNodes)
	{
		if (!Node) continue;
		if (Cast<UK2Node_FunctionEntry>(Node)
			|| Cast<UK2Node_Event>(Node)
			|| Cast<UK2Node_CustomEvent>(Node))
		{
			EntryNodes.Add(Node);
		}
	}

	// BFS along exec pins from all entry points
	TMap<const UEdGraphNode*, int32> NodeToStep;
	TArray<UEdGraphNode*> Ordered;
	TArray<UEdGraphNode*> Queue;
	int32 QueueHead = 0;

	for (UEdGraphNode* Entry : EntryNodes)
		Queue.AddUnique(Entry);

	while (QueueHead < Queue.Num())
	{
		UEdGraphNode* Current = Queue[QueueHead++];
		if (NodeToStep.Contains(Current)) continue;

		int32 Idx = Ordered.Num();
		NodeToStep.Add(Current, Idx);
		Ordered.Add(Current);

		// Enqueue nodes connected via exec output pins
		for (const UEdGraphPin* Pin : Current->Pins)
		{
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;

			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				UEdGraphNode* Target = Linked ? Linked->GetOwningNode() : nullptr;
				if (Target && !NodeToStep.Contains(Target))
					Queue.AddUnique(Target);
			}
		}
	}

	// Build result array
	for (int32 i = 0; i < Ordered.Num(); i++)
	{
		const UEdGraphNode* Node = Ordered[i];

		FBridgeExecStep Step;
		Step.StepIndex = i;
		Step.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		Step.NodeType = ClassifyNode(Node);
		Step.Detail = GetNodeDetail(Node);

		// Record exec output connections with branching
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;

			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				const UEdGraphNode* Target = Linked ? Linked->GetOwningNode() : nullptr;
				if (Target && NodeToStep.Contains(Target))
				{
					FBridgeExecConnection Conn;
					Conn.PinName = Pin->PinName.ToString();
					Conn.TargetStepIndex = NodeToStep[Target];
					Step.ExecOutputs.Add(Conn);
				}
			}
		}

		Result.Add(Step);
	}

	return Result;
}

// ─── GetNodePinConnections ──────────────────────────────────

TArray<FBridgePinConnection> UUnrealBridgeBlueprintLibrary::GetNodePinConnections(
	const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FBridgePinConnection> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);

	// Build node index map (same iteration order as GetFunctionNodes with empty filter)
	TMap<const UEdGraphNode*, int32> NodeIndexMap;
	int32 CurrentIndex = 0;
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
				NodeIndexMap.Add(Node, CurrentIndex++);
		}
	}

	// Collect all output→input connections (output side only to avoid duplicates)
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			const int32* SourceIdxPtr = NodeIndexMap.Find(Node);
			if (!SourceIdxPtr) continue;

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction != EGPD_Output) continue;

				const bool bExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);

				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin) continue;
					const UEdGraphNode* TargetNode = LinkedPin->GetOwningNode();
					if (!TargetNode) continue;

					const int32* TargetIdxPtr = NodeIndexMap.Find(TargetNode);
					if (!TargetIdxPtr) continue;

					FBridgePinConnection Conn;
					Conn.SourceNodeIndex = *SourceIdxPtr;
					Conn.SourcePinName = Pin->PinName.ToString();
					Conn.TargetNodeIndex = *TargetIdxPtr;
					Conn.TargetPinName = LinkedPin->PinName.ToString();
					Conn.bIsExec = bExec;
					Result.Add(Conn);
				}
			}
		}
	}

	return Result;
}

// ─── GetComponentPropertyValues ─────────────────────────────

TArray<FBridgePropertyValue> UUnrealBridgeBlueprintLibrary::GetComponentPropertyValues(
	const FString& BlueprintPath, const FString& ComponentName)
{
	TArray<FBridgePropertyValue> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	UActorComponent* Template = nullptr;

	// Search SCS components
	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				Template = Node->ComponentTemplate;
				break;
			}
		}
	}

	// Search inherited components on CDO
	if (!Template && BP->GeneratedClass)
	{
		if (AActor* ActorCDO = Cast<AActor>(BP->GeneratedClass->GetDefaultObject()))
		{
			TArray<UActorComponent*> Components;
			ActorCDO->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->GetName() == ComponentName)
				{
					Template = Comp;
					break;
				}
			}
		}
	}

	if (!Template) return Result;

	// Compare against component class CDO
	const UObject* CompCDO = Template->GetClass()->GetDefaultObject();

	for (TFieldIterator<FProperty> It(Template->GetClass()); It; ++It)
	{
		const FProperty* Prop = *It;
		if (!Prop) continue;
		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) continue;

		const void* TemplateVal = Prop->ContainerPtrToValuePtr<void>(Template);
		const void* CDOVal = Prop->ContainerPtrToValuePtr<void>(CompCDO);

		if (!Prop->Identical(TemplateVal, CDOVal))
		{
			FBridgePropertyValue PV;
			PV.Name = Prop->GetName();
			PV.Type = PropertyTypeToString(Prop);
			Prop->ExportTextItem_Direct(PV.Value, TemplateVal, nullptr, nullptr, PPF_None);

			if (const FString* Cat = Prop->FindMetaData(TEXT("Category")))
				PV.Category = *Cat;

			Result.Add(PV);
		}
	}

	return Result;
}

// ─── SearchBlueprintNodes ───────────────────────────────────

TArray<FBridgeNodeSearchResult> UUnrealBridgeBlueprintLibrary::SearchBlueprintNodes(
	const FString& BlueprintPath, const FString& Query, const FString& NodeTypeFilter)
{
	TArray<FBridgeNodeSearchResult> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	FString QueryLower = Query.ToLower();

	// Helper: collect graphs with type labels
	struct FGraphEntry { UEdGraph* Graph; FString Type; };
	TArray<FGraphEntry> AllGraphs;

	for (UEdGraph* G : BP->UbergraphPages)
		if (G) AllGraphs.Add({G, TEXT("EventGraph")});
	for (UEdGraph* G : BP->FunctionGraphs)
		if (G) AllGraphs.Add({G, TEXT("Function")});
	for (UEdGraph* G : BP->MacroGraphs)
		if (G) AllGraphs.Add({G, TEXT("Macro")});

	for (const FGraphEntry& Entry : AllGraphs)
	{
		for (const UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;

			FString NType = ClassifyNode(Node);
			if (!NodeTypeFilter.IsEmpty() && NType != NodeTypeFilter) continue;

			FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			FString Detail = GetNodeDetail(Node);

			// Match query against title or detail (case-insensitive)
			bool bMatch = QueryLower.IsEmpty()
				|| Title.ToLower().Contains(QueryLower)
				|| Detail.ToLower().Contains(QueryLower);

			if (bMatch)
			{
				FBridgeNodeSearchResult SR;
				SR.GraphName = Entry.Graph->GetName();
				SR.GraphType = Entry.Type;
				SR.NodeTitle = Title;
				SR.NodeType = NType;
				SR.Detail = Detail;
				Result.Add(SR);
			}
		}
	}

	return Result;
}

// ─── GetTimelineInfo ────────────────────────────────────────

TArray<FBridgeTimelineInfo> UUnrealBridgeBlueprintLibrary::GetTimelineInfo(
	const FString& BlueprintPath)
{
	TArray<FBridgeTimelineInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	for (const UTimelineTemplate* TL : BP->Timelines)
	{
		if (!TL) continue;

		FBridgeTimelineInfo Info;
		Info.Name = TL->GetName();
		// Strip trailing "_Template" if present
		static const FString TemplateSuffix = TEXT("_Template");
		if (Info.Name.EndsWith(TemplateSuffix))
			Info.Name = Info.Name.LeftChop(TemplateSuffix.Len());

		Info.Length = TL->TimelineLength;
		Info.bAutoPlay = TL->bAutoPlay;
		Info.bLoop = TL->bLoop;
		Info.bReplicated = TL->bReplicated;

		for (const FTTFloatTrack& Track : TL->FloatTracks)
		{
			FBridgeTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("Float");
			Info.Tracks.Add(T);
		}
		for (const FTTVectorTrack& Track : TL->VectorTracks)
		{
			FBridgeTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("Vector");
			Info.Tracks.Add(T);
		}
		for (const FTTLinearColorTrack& Track : TL->LinearColorTracks)
		{
			FBridgeTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("LinearColor");
			Info.Tracks.Add(T);
		}
		for (const FTTEventTrack& Track : TL->EventTracks)
		{
			FBridgeTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("Event");
			Info.Tracks.Add(T);
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── Type string parser ─────────────────────────────────────

static bool ParseTypeString(const FString& TypeStr, FEdGraphPinType& OutPinType)
{
	OutPinType = FEdGraphPinType();

	FString Type = TypeStr.TrimStartAndEnd();

	// Array prefix
	static const FString ArrayPrefix = TEXT("Array of ");
	if (Type.StartsWith(ArrayPrefix))
	{
		Type = Type.Mid(ArrayPrefix.Len());
		OutPinType.ContainerType = EPinContainerType::Array;
	}

	if (Type.Equals(TEXT("Bool"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Type.Equals(TEXT("Byte"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (Type.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Type.Equals(TEXT("Int64"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (Type.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = TEXT("float");
	}
	else if (Type.Equals(TEXT("Double"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = TEXT("double");
	}
	else if (Type.Equals(TEXT("String"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Type.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Type.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Type.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (Type.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (Type.Equals(TEXT("Transform"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (Type.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
	}
	else if (Type.Equals(TEXT("GameplayTag"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = FindObject<UScriptStruct>(nullptr, TEXT("/Script/GameplayTags.GameplayTag"));
	}
	else
	{
		// Try as struct
		UScriptStruct* FoundStruct = FindObject<UScriptStruct>(nullptr, *Type);
		if (!FoundStruct)
			FoundStruct = FindObject<UScriptStruct>(nullptr, *(FString(TEXT("/Script/CoreUObject.")) + Type));
		if (!FoundStruct)
			FoundStruct = FindObject<UScriptStruct>(nullptr, *(FString(TEXT("/Script/Engine.")) + Type));

		if (FoundStruct)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			OutPinType.PinSubCategoryObject = FoundStruct;
			return true;
		}

		// Try as class (object reference)
		UClass* FoundClass = FindObject<UClass>(nullptr, *Type);
		if (!FoundClass)
			FoundClass = FindObject<UClass>(nullptr, *(FString(TEXT("/Script/Engine.")) + Type));
		if (!FoundClass)
			FoundClass = FindObject<UClass>(nullptr, *(FString(TEXT("/Script/CoreUObject.")) + Type));

		if (FoundClass)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = FoundClass;
			return true;
		}

		return false;
	}

	return true;
}

// ─── SetBlueprintVariableDefault ────────────────────────────

bool UUnrealBridgeBlueprintLibrary::SetBlueprintVariableDefault(
	const FString& BlueprintPath, const FString& VariableName, const FString& Value)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	FName VarFName(*VariableName);

	// Try CDO path first (best: validates the value)
	if (BP->GeneratedClass)
	{
		UObject* CDO = BP->GeneratedClass->GetDefaultObject();
		FProperty* Prop = BP->GeneratedClass->FindPropertyByName(VarFName);
		if (Prop && CDO)
		{
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);
			if (Prop->ImportText_Direct(*Value, ValuePtr, CDO, PPF_None))
			{
				FString ExportedValue;
				Prop->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
				for (FBPVariableDescription& Var : BP->NewVariables)
				{
					if (Var.VarName == VarFName)
					{
						Var.DefaultValue = ExportedValue;
						break;
					}
				}
				FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
				return true;
			}
		}
	}

	// Fallback: update metadata directly (e.g. after AddVariable before compile)
	for (FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarName == VarFName)
		{
			Var.DefaultValue = Value;
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			return true;
		}
	}

	return false;
}

// ─── SetComponentProperty ───────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::SetComponentProperty(
	const FString& BlueprintPath, const FString& ComponentName,
	const FString& PropertyName, const FString& Value)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UActorComponent* Template = nullptr;

	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				Template = Node->ComponentTemplate;
				break;
			}
		}
	}

	if (!Template) return false;

	FProperty* Prop = Template->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop) return false;

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Template);
	if (!Prop->ImportText_Direct(*Value, ValuePtr, Template, PPF_None))
		return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── AddBlueprintVariable ───────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::AddBlueprintVariable(
	const FString& BlueprintPath, const FString& Name,
	const FString& TypeString, const FString& DefaultValue)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	FName VarName(*Name);

	// Check for existing variable with same name
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarName == VarName)
			return false;
	}

	FEdGraphPinType PinType;
	if (!ParseTypeString(TypeString, PinType))
		return false;

	bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(BP, VarName, PinType, DefaultValue);
	if (bSuccess)
	{
		FKismetEditorUtilities::CompileBlueprint(BP);
	}

	return bSuccess;
}

// ─── RemoveBlueprintVariable ────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::RemoveBlueprintVariable(
	const FString& BlueprintPath, const FString& VariableName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	const FName VarName(*VariableName);
	const int32 Idx = FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName);
	if (Idx == INDEX_NONE) return false;

	FBlueprintEditorUtils::RemoveMemberVariable(BP, VarName);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── RenameBlueprintVariable ────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::RenameBlueprintVariable(
	const FString& BlueprintPath, const FString& OldName, const FString& NewName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	const FName OldVar(*OldName);
	const FName NewVar(*NewName);
	if (OldVar == NewVar || NewVar.IsNone()) return false;

	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, OldVar) == INDEX_NONE) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, NewVar) != INDEX_NONE) return false;

	FBlueprintEditorUtils::RenameMemberVariable(BP, OldVar, NewVar);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── Function-scope local variables ─────────────────────────

namespace BridgeBPLocalVarImpl
{
	/** Find a function graph by name. Restricted to FunctionGraphs — local
	 *  variables don't apply to ubergraph pages or macros. */
	static UEdGraph* FindFunctionGraph(UBlueprint* BP, const FString& FunctionName)
	{
		if (!BP) return nullptr;
		for (UEdGraph* G : BP->FunctionGraphs)
		{
			if (G && G->GetName() == FunctionName) return G;
		}
		return nullptr;
	}

	/** The function-entry node owns the LocalVariables array. */
	static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N))
			{
				return E;
			}
		}
		return nullptr;
	}

	/** Resolve the UStruct scope used by FBlueprintEditorUtils' local-var
	 *  helpers. For a function graph this is the UFunction on the skeleton
	 *  class (which exists even before a successful compile of the body). */
	static UStruct* FindFunctionScope(UBlueprint* BP, UEdGraph* Graph)
	{
		if (!BP || !Graph) return nullptr;
		UClass* Skeleton = BP->SkeletonGeneratedClass ? BP->SkeletonGeneratedClass : BP->GeneratedClass;
		if (!Skeleton) return nullptr;
		if (UFunction* Fn = Skeleton->FindFunctionByName(Graph->GetFName()))
		{
			return Fn;
		}
		return nullptr;
	}

	/** Build FBridgeVariableInfo from a local FBPVariableDescription. Looks up
	 *  the skeleton UFunction's corresponding FProperty (if any) for a more
	 *  precise type string; falls back to pin-category string. */
	static FBridgeVariableInfo MakeLocalVarInfo(
		const FBPVariableDescription& Var, const UStruct* Scope)
	{
		FBridgeVariableInfo Info;
		Info.Name = Var.VarName.ToString();

		const FProperty* Prop = Scope ? Scope->FindPropertyByName(Var.VarName) : nullptr;
		Info.Type = Prop ? PropertyTypeToString(Prop) : Var.VarType.PinCategory.ToString();
		Info.Category = Var.Category.ToString();
		if (Var.HasMetaData(TEXT("tooltip")))
		{
			Info.Description = Var.GetMetaData(TEXT("tooltip"));
		}
		Info.DefaultValue = Var.DefaultValue;
		Info.bInstanceEditable  = (Var.PropertyFlags & CPF_Edit) != 0;
		Info.bBlueprintReadOnly = (Var.PropertyFlags & CPF_BlueprintReadOnly) != 0;
		Info.ReplicationCondition = TEXT("None");
		return Info;
	}
}

TArray<FBridgeVariableInfo> UUnrealBridgeBlueprintLibrary::GetFunctionLocalVariables(
	const FString& BlueprintPath, const FString& FunctionName)
{
	using namespace BridgeBPLocalVarImpl;
	TArray<FBridgeVariableInfo> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return Out;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return Out;

	const UStruct* Scope = FindFunctionScope(BP, Graph);
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		Out.Add(MakeLocalVarInfo(V, Scope));
	}
	return Out;
}

bool UUnrealBridgeBlueprintLibrary::AddFunctionLocalVariable(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& VariableName, const FString& TypeString, const FString& DefaultValue)
{
	using namespace BridgeBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName VarFName(*VariableName);
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName) return false;
	}

	FEdGraphPinType PinType;
	if (!ParseTypeString(TypeString, PinType)) return false;

	// FBlueprintEditorUtils::AddLocalVariable handles the full propagation
	// (entry-node modify, MarkBlueprintAsModified, variable visibility).
	FBlueprintEditorUtils::AddLocalVariable(BP, Graph, VarFName, PinType, DefaultValue);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RemoveFunctionLocalVariable(
	const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName)
{
	using namespace BridgeBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName VarFName(*VariableName);
	const UStruct* Scope = FindFunctionScope(BP, Graph);

	bool bFound = false;
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName) { bFound = true; break; }
	}
	if (!bFound) return false;

	if (Scope)
	{
		FBlueprintEditorUtils::RemoveLocalVariable(BP, Scope, VarFName);
	}
	else
	{
		// Fallback: scope resolution failed (skeleton not compiled yet) — drop
		// the entry directly. Safe because LocalVariables is the source of truth.
		Entry->Modify();
		Entry->LocalVariables.RemoveAll([&](const FBPVariableDescription& V)
		{
			return V.VarName == VarFName;
		});
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RenameFunctionLocalVariable(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& OldName, const FString& NewName)
{
	using namespace BridgeBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName OldFName(*OldName);
	const FName NewFName(*NewName);
	if (OldFName == NewFName || NewFName.IsNone()) return false;

	bool bFoundOld = false;
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == OldFName) { bFoundOld = true; }
		if (V.VarName == NewFName) { return false; }
	}
	if (!bFoundOld) return false;

	const UStruct* Scope = FindFunctionScope(BP, Graph);
	if (Scope)
	{
		FBlueprintEditorUtils::RenameLocalVariable(BP, Scope, OldFName, NewFName);
	}
	else
	{
		Entry->Modify();
		for (FBPVariableDescription& V : Entry->LocalVariables)
		{
			if (V.VarName == OldFName) { V.VarName = NewFName; break; }
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::SetFunctionLocalVariableDefault(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& VariableName, const FString& Value)
{
	using namespace BridgeBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName VarFName(*VariableName);
	bool bFound = false;
	Entry->Modify();
	for (FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName)
		{
			V.DefaultValue = Value;
			bFound = true;
			break;
		}
	}
	if (!bFound) return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::AddFunctionLocalVariableNode(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& VariableName, bool bIsSet, int32 NodePosX, int32 NodePosY)
{
	using namespace BridgeBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return FString();

	const FName VarFName(*VariableName);
	const FBPVariableDescription* Found = nullptr;
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName) { Found = &V; break; }
	}
	if (!Found) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_Variable* Node = bIsSet
		? (UK2Node_Variable*)NewObject<UK2Node_VariableSet>(Graph)
		: (UK2Node_Variable*)NewObject<UK2Node_VariableGet>(Graph);
	Node->CreateNewGuid();
	// Local-member scope is the top-level function graph's *name* (string),
	// with the variable's declared FGuid — see K2Node_LocalVariable.cpp.
	Node->VariableReference.SetLocalMember(VarFName, Graph->GetName(), Found->VarGuid);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Interface helpers ──────────────────────────────────────

namespace BridgeBpInterfaceOps
{
	static UClass* ResolveInterfaceClass(const FString& InterfacePath)
	{
		if (InterfacePath.IsEmpty()) return nullptr;

		// Try as a loaded / native class first.
		if (UClass* Cls = FindObject<UClass>(nullptr, *InterfacePath))
		{
			return Cls;
		}

		// Try as a Blueprint interface asset path → GeneratedClass.
		if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *InterfacePath))
		{
			return BP->GeneratedClass;
		}

		// Try LoadObject<UClass> for TopLevelAssetPath-style strings.
		if (UClass* Cls = LoadObject<UClass>(nullptr, *InterfacePath))
		{
			return Cls;
		}

		// Try appending "_C" for Blueprint class paths.
		const FString WithC = InterfacePath + TEXT("_C");
		if (UClass* Cls = LoadObject<UClass>(nullptr, *WithC))
		{
			return Cls;
		}
		return nullptr;
	}
}

// ─── AddBlueprintInterface ──────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::AddBlueprintInterface(
	const FString& BlueprintPath, const FString& InterfacePath)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UClass* InterfaceClass = BridgeBpInterfaceOps::ResolveInterfaceClass(InterfacePath);
	if (!InterfaceClass || !InterfaceClass->HasAnyClassFlags(CLASS_Interface))
	{
		return false;
	}

	const FString InterfaceClassName = InterfaceClass->GetPathName();
	if (!FBlueprintEditorUtils::ImplementNewInterface(BP, FTopLevelAssetPath(InterfaceClassName)))
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── RemoveBlueprintInterface ───────────────────────────────

bool UUnrealBridgeBlueprintLibrary::RemoveBlueprintInterface(
	const FString& BlueprintPath, const FString& InterfaceNameOrPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UClass* InterfaceClass = BridgeBpInterfaceOps::ResolveInterfaceClass(InterfaceNameOrPath);

	// Fallback: match by short name against implemented interfaces.
	if (!InterfaceClass)
	{
		for (const FBPInterfaceDescription& Impl : BP->ImplementedInterfaces)
		{
			if (Impl.Interface && Impl.Interface->GetName() == InterfaceNameOrPath)
			{
				InterfaceClass = Impl.Interface;
				break;
			}
		}
	}
	if (!InterfaceClass) return false;

	const bool bWasImplemented = BP->ImplementedInterfaces.ContainsByPredicate(
		[InterfaceClass](const FBPInterfaceDescription& D) { return D.Interface == InterfaceClass; });
	if (!bWasImplemented) return false;

	FBlueprintEditorUtils::RemoveInterface(BP, FTopLevelAssetPath(InterfaceClass->GetPathName()), /*bPreserveFunctions*/ false);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── AddBlueprintComponent ──────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::AddBlueprintComponent(
	const FString& BlueprintPath,
	const FString& ComponentClassPath,
	const FString& ComponentName,
	const FString& ParentComponentName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	if (!SCS) return false;

	// Resolve component class.
	UClass* CompClass = FindObject<UClass>(nullptr, *ComponentClassPath);
	if (!CompClass)
	{
		CompClass = LoadObject<UClass>(nullptr, *ComponentClassPath);
	}
	if (!CompClass)
	{
		CompClass = LoadObject<UClass>(nullptr, *(ComponentClassPath + TEXT("_C")));
	}
	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return false;
	}

	const FName DesiredName(*ComponentName);
	if (DesiredName.IsNone()) return false;

	// Reject duplicate names.
	for (USCS_Node* Existing : SCS->GetAllNodes())
	{
		if (Existing && Existing->GetVariableName() == DesiredName)
		{
			return false;
		}
	}

	USCS_Node* NewNode = SCS->CreateNode(CompClass, DesiredName);
	if (!NewNode) return false;

	USCS_Node* ParentNode = nullptr;
	if (!ParentComponentName.IsEmpty())
	{
		const FName ParentName(*ParentComponentName);
		for (USCS_Node* Candidate : SCS->GetAllNodes())
		{
			if (Candidate && Candidate->GetVariableName() == ParentName)
			{
				ParentNode = Candidate;
				break;
			}
		}
	}

	if (ParentNode)
	{
		ParentNode->AddChildNode(NewNode);
	}
	else
	{
		// Attach under an existing root, or become the root if none yet.
		const TArray<USCS_Node*> Roots = SCS->GetRootNodes();
		if (Roots.Num() > 0 && Roots[0])
		{
			Roots[0]->AddChildNode(NewNode);
		}
		else
		{
			SCS->AddNode(NewNode);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── Graph node write ops ──────────────────────────────────

namespace BridgeBlueprintGraphWriteImpl
{
	UEdGraph* FindGraphByName(UBlueprint* BP, const FString& GraphName)
	{
		if (!BP) return nullptr;
		for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetName() == GraphName) return G; }
		for (UEdGraph* G : BP->UbergraphPages) { if (G && G->GetName() == GraphName) return G; }
		for (UEdGraph* G : BP->MacroGraphs)    { if (G && G->GetName() == GraphName) return G; }
		for (UEdGraph* G : BP->DelegateSignatureGraphs) { if (G && G->GetName() == GraphName) return G; }
		return nullptr;
	}

	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr)
	{
		if (!Graph) return nullptr;
		FGuid Guid;
		if (!FGuid::Parse(GuidStr, Guid)) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid == Guid) return N;
		}
		return nullptr;
	}

	UClass* ResolveTargetClass(UBlueprint* BP, const FString& TargetClassPath)
	{
		if (TargetClassPath.IsEmpty())
		{
			return BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass;
		}
		if (UClass* C = FindObject<UClass>(nullptr, *TargetClassPath))
		{
			return C;
		}
		if (UClass* C = LoadObject<UClass>(nullptr, *TargetClassPath))
		{
			return C;
		}
		// Try BP asset path (auto-append _C).
		const FString WithC = TargetClassPath.EndsWith(TEXT("_C"))
			? TargetClassPath : TargetClassPath + TEXT("_C");
		if (UClass* C = LoadObject<UClass>(nullptr, *WithC))
		{
			return C;
		}
		if (UBlueprint* Other = LoadObject<UBlueprint>(nullptr, *TargetClassPath))
		{
			return Other->GeneratedClass;
		}
		return nullptr;
	}
}

FString UUnrealBridgeBlueprintLibrary::AddCallFunctionNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TargetClassPath, const FString& FunctionName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	UClass* TargetClass = BridgeBlueprintGraphWriteImpl::ResolveTargetClass(BP, TargetClassPath);
	if (!TargetClass) return FString();

	UFunction* Fn = TargetClass->FindFunctionByName(FName(*FunctionName));
	if (!Fn) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->CreateNewGuid();
	Node->SetFromFunction(Fn);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddVariableNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& VariableName, bool bIsSet,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	const FName VarFName(*VariableName);

	// Resolve against either this BP's declared vars or an inherited property.
	bool bSelfDeclared = false;
	for (const FBPVariableDescription& V : BP->NewVariables)
	{
		if (V.VarName == VarFName) { bSelfDeclared = true; break; }
	}
	UClass* SearchClass = BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass;
	FProperty* Prop = SearchClass ? FindFProperty<FProperty>(SearchClass, VarFName) : nullptr;
	if (!bSelfDeclared && !Prop)
	{
		return FString();
	}

	Graph->Modify();
	BP->Modify();

	UK2Node_Variable* Node = bIsSet
		? (UK2Node_Variable*)NewObject<UK2Node_VariableSet>(Graph)
		: (UK2Node_Variable*)NewObject<UK2Node_VariableGet>(Graph);
	Node->CreateNewGuid();
	Node->VariableReference.SetSelfMember(VarFName);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

bool UUnrealBridgeBlueprintLibrary::ConnectGraphPins(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& SourceNodeGuid, const FString& SourcePinName,
	const FString& TargetNodeGuid, const FString& TargetPinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* SrcNode = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, SourceNodeGuid);
	UEdGraphNode* DstNode = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, TargetNodeGuid);
	if (!SrcNode || !DstNode) return false;

	UEdGraphPin* SrcPin = SrcNode->FindPin(SourcePinName);
	UEdGraphPin* DstPin = DstNode->FindPin(TargetPinName);
	if (!SrcPin || !DstPin) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema) return false;

	Graph->Modify();
	BP->Modify();

	const bool bConnected = Schema->TryCreateConnection(SrcPin, DstPin);
	if (bConnected)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	return bConnected;
}

bool UUnrealBridgeBlueprintLibrary::RemoveGraphNode(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Graph->Modify();
	BP->Modify();
	Node->BreakAllNodeLinks();
	Graph->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::SetGraphNodePosition(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::AddEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ParentClassPath, const FString& EventName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	UClass* ParentClass = ParentClassPath.IsEmpty()
		? static_cast<UClass*>(BP->ParentClass)
		: BridgeBlueprintGraphWriteImpl::ResolveTargetClass(BP, ParentClassPath);
	if (!ParentClass) return FString();

	UFunction* Fn = ParentClass->FindFunctionByName(FName(*EventName));
	if (!Fn) return FString();

	const FName EventFName = Fn->GetFName();

	// Reuse existing event (e.g. the default ghost ReceiveTick) instead of creating a duplicate.
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (UK2Node_Event* Existing = Cast<UK2Node_Event>(N))
		{
			if (Existing->EventReference.GetMemberName() == EventFName)
			{
				Existing->Modify();
				Existing->bOverrideFunction = true;
				Existing->NodePosX = NodePosX;
				Existing->NodePosY = NodePosY;
				FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
				return Existing->NodeGuid.ToString(EGuidFormats::Digits);
			}
		}
	}

	Graph->Modify();
	BP->Modify();

	UK2Node_Event* Node = NewObject<UK2Node_Event>(Graph);
	Node->CreateNewGuid();
	Node->EventReference.SetExternalMember(EventFName, ParentClass);
	Node->bOverrideFunction = true;
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

bool UUnrealBridgeBlueprintLibrary::SetPinDefaultValue(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName, const FString& NewDefaultValue)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema) return false;

	Node->Modify();
	Schema->TrySetDefaultValue(*Pin, NewDefaultValue);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ═════════════════════════════════════════════════════════════════
//   P0 extensions: control flow, functions, dispatchers, interface,
//   variable metadata, compile feedback
// ═════════════════════════════════════════════════════════════════

namespace BridgeBpP0Impl
{
	// Shared helper: finalize a newly-constructed K2Node and add it to Graph.
	template<typename TNode>
	TNode* FinalizeNewNode(UEdGraph* Graph, TNode* Node, int32 X, int32 Y)
	{
		Node->CreateNewGuid();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		return Node;
	}

	// Parse "public"/"protected"/"private" to UE access flags applied on FunctionEntry.
	// Returns true if AccessSpec is non-empty and understood.
	bool ApplyAccessSpecifier(UK2Node_FunctionEntry* Entry, const FString& AccessSpec)
	{
		if (!Entry || AccessSpec.IsEmpty()) return false;
		int32 Flags = Entry->GetExtraFlags();
		Flags &= ~(FUNC_Public | FUNC_Protected | FUNC_Private);
		if (AccessSpec.Equals(TEXT("public"), ESearchCase::IgnoreCase))    Flags |= FUNC_Public;
		else if (AccessSpec.Equals(TEXT("protected"), ESearchCase::IgnoreCase)) Flags |= FUNC_Protected;
		else if (AccessSpec.Equals(TEXT("private"), ESearchCase::IgnoreCase))   Flags |= FUNC_Private;
		else return false;
		Entry->SetExtraFlags(Flags);
		return true;
	}

	UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) return E;
		}
		return nullptr;
	}

	UK2Node_FunctionResult* FindOrCreateFunctionResult(UEdGraph* Graph, UBlueprint* BP)
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* R = Cast<UK2Node_FunctionResult>(N)) return R;
		}
		// Create a result node, positioned right of the entry.
		UK2Node_FunctionResult* Result = NewObject<UK2Node_FunctionResult>(Graph);
		int32 X = 600, Y = 0;
		if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph))
		{
			X = Entry->NodePosX + 600;
			Y = Entry->NodePosY;
		}
		return FinalizeNewNode(Graph, Result, X, Y);
	}

	// Resolve a member variable's FMulticastDelegateProperty on the BP's generated class.
	FMulticastDelegateProperty* FindDispatcherProp(UBlueprint* BP, const FString& DispatcherName)
	{
		if (!BP || !BP->SkeletonGeneratedClass) return nullptr;
		return FindFProperty<FMulticastDelegateProperty>(BP->SkeletonGeneratedClass, FName(*DispatcherName));
	}
}

// ─── Control-flow / basic nodes ─────────────────────────────────

FString UUnrealBridgeBlueprintLibrary::AddBranchNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_IfThenElse* Node = BridgeBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_IfThenElse>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddSequenceNode(
	const FString& BlueprintPath, const FString& GraphName, int32 PinCount, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	const int32 Want = FMath::Clamp(PinCount, 2, 16);
	Graph->Modify(); BP->Modify();
	UK2Node_ExecutionSequence* Node = BridgeBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_ExecutionSequence>(Graph), X, Y);
	// AllocateDefaultPins already creates "Then 0" + "Then 1".
	for (int32 i = 2; i < Want; ++i)
	{
		Node->AddInputPin();
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddCastNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TargetClassPath, bool bPure, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UClass* Target = BridgeBlueprintGraphWriteImpl::ResolveTargetClass(BP, TargetClassPath);
	if (!Target) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_DynamicCast* Node = NewObject<UK2Node_DynamicCast>(Graph);
	Node->TargetType = Target;
	Node->SetPurity(bPure);
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddSelfNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_Self* Node = BridgeBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_Self>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddCustomEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& EventName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	if (EventName.IsEmpty()) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
	Node->CustomFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(BP, EventName);
	Node->bIsEditable = true;
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Function/event graph management ────────────────────────────

bool UUnrealBridgeBlueprintLibrary::CreateFunctionGraph(
	const FString& BlueprintPath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName FnName(*FunctionName);
	if (FnName.IsNone()) return false;

	// Reject if already present.
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) return false; }

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, FnName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, NewGraph, /*bIsUserCreated*/true, /*SignatureFromObject*/nullptr);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RemoveFunctionGraph(
	const FString& BlueprintPath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetFName() == FnName)
		{
			FBlueprintEditorUtils::RemoveGraph(BP, G, EGraphRemoveFlags::Recompile);
			return true;
		}
	}
	return false;
}

bool UUnrealBridgeBlueprintLibrary::RenameFunctionGraph(
	const FString& BlueprintPath, const FString& OldName, const FString& NewName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Old(*OldName), New(*NewName);
	if (Old == New || New.IsNone()) return false;
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetFName() == Old)
		{
			FBlueprintEditorUtils::RenameGraph(G, NewName);
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			return true;
		}
	}
	return false;
}

bool UUnrealBridgeBlueprintLibrary::AddFunctionParameter(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& ParamName, const FString& TypeString, bool bIsReturn)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	FEdGraphPinType PinType;
	if (!ParseTypeString(TypeString, PinType)) return false;

	UK2Node_EditablePinBase* Target = bIsReturn
		? static_cast<UK2Node_EditablePinBase*>(BridgeBpP0Impl::FindOrCreateFunctionResult(Graph, BP))
		: static_cast<UK2Node_EditablePinBase*>(BridgeBpP0Impl::FindFunctionEntry(Graph));
	if (!Target) return false;

	Target->Modify();
	UEdGraphPin* NewPin = Target->CreateUserDefinedPin(FName(*ParamName), PinType,
		bIsReturn ? EGPD_Input : EGPD_Output);
	if (!NewPin) return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::SetFunctionMetadata(
	const FString& BlueprintPath, const FString& FunctionName,
	bool bPure, bool bConst, const FString& Category, const FString& AccessSpecifier)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	UK2Node_FunctionEntry* Entry = BridgeBpP0Impl::FindFunctionEntry(Graph);
	if (!Entry) return false;

	Entry->Modify();
	int32 Flags = Entry->GetExtraFlags();
	if (bPure)  Flags |= FUNC_BlueprintPure; else Flags &= ~FUNC_BlueprintPure;
	if (bConst) Flags |= FUNC_Const;         else Flags &= ~FUNC_Const;
	Entry->SetExtraFlags(Flags);

	if (!Category.IsEmpty())
	{
		Entry->MetaData.Category = FText::FromString(Category);
	}
	BridgeBpP0Impl::ApplyAccessSpecifier(Entry, AccessSpecifier);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── Event Dispatcher write ops ─────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::AddEventDispatcher(
	const FString& BlueprintPath, const FString& DispatcherName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Name(*DispatcherName);
	if (Name.IsNone()) return false;
	// Reject duplicate against any existing dispatcher signature graph or other kismet name.
	for (UEdGraph* G : BP->DelegateSignatureGraphs) { if (G && G->GetFName() == Name) return false; }
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE) return false;

	BP->Modify();

	// Step 1: add the matching member variable of MCDelegate type.
	FEdGraphPinType DelegateType;
	DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
	if (!FBlueprintEditorUtils::AddMemberVariable(BP, Name, DelegateType))
	{
		return false;
	}

	// Step 2: create the signature graph.
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		FBlueprintEditorUtils::RemoveMemberVariable(BP, Name);
		return false;
	}
	NewGraph->bEditable = false;

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	K2->CreateDefaultNodesForGraph(*NewGraph);
	K2->CreateFunctionGraphTerminators(*NewGraph, static_cast<UClass*>(nullptr));
	K2->AddExtraFunctionFlags(NewGraph, (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public));
	K2->MarkFunctionEntryAsEditable(NewGraph, true);

	BP->DelegateSignatureGraphs.Add(NewGraph);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RemoveEventDispatcher(
	const FString& BlueprintPath, const FString& DispatcherName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Name(*DispatcherName);

	UEdGraph* SigGraph = nullptr;
	for (UEdGraph* G : BP->DelegateSignatureGraphs)
	{
		if (G && G->GetFName() == Name) { SigGraph = G; break; }
	}
	const bool bHasVar = FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE;
	if (!SigGraph && !bHasVar) return false;

	BP->Modify();
	if (SigGraph)
	{
		BP->DelegateSignatureGraphs.Remove(SigGraph);
		FBlueprintEditorUtils::RemoveGraph(BP, SigGraph, EGraphRemoveFlags::Recompile);
	}
	if (bHasVar)
	{
		FBlueprintEditorUtils::RemoveMemberVariable(BP, Name);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RenameEventDispatcher(
	const FString& BlueprintPath, const FString& OldName, const FString& NewName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Old(*OldName), New(*NewName);
	if (Old == New || New.IsNone()) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, New) != INDEX_NONE) return false;

	UEdGraph* SigGraph = nullptr;
	for (UEdGraph* G : BP->DelegateSignatureGraphs)
	{
		if (G && G->GetFName() == Old) { SigGraph = G; break; }
	}
	if (!SigGraph) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Old) == INDEX_NONE) return false;

	BP->Modify();
	FBlueprintEditorUtils::RenameMemberVariable(BP, Old, New);
	FBlueprintEditorUtils::RenameGraph(SigGraph, NewName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::AddDispatcherCallNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& DispatcherName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	FMulticastDelegateProperty* Prop = BridgeBpP0Impl::FindDispatcherProp(BP, DispatcherName);
	if (!Prop) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CallDelegate* Node = NewObject<UK2Node_CallDelegate>(Graph);
	Node->SetFromProperty(Prop, /*bSelfContext*/true, Prop->GetOwnerClass());
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddDispatcherBindNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& DispatcherName, bool bUnbind, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	FMulticastDelegateProperty* Prop = BridgeBpP0Impl::FindDispatcherProp(BP, DispatcherName);
	if (!Prop) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_BaseMCDelegate* Node = bUnbind
		? static_cast<UK2Node_BaseMCDelegate*>(NewObject<UK2Node_RemoveDelegate>(Graph))
		: static_cast<UK2Node_BaseMCDelegate*>(NewObject<UK2Node_AddDelegate>(Graph));
	Node->SetFromProperty(Prop, /*bSelfContext*/true, Prop->GetOwnerClass());
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Interface override ─────────────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::ImplementInterfaceFunction(
	const FString& BlueprintPath, const FString& InterfacePath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UClass* IFace = BridgeBpInterfaceOps::ResolveInterfaceClass(InterfacePath);
	if (!IFace) return false;

	FBPInterfaceDescription* Desc = nullptr;
	for (FBPInterfaceDescription& D : BP->ImplementedInterfaces)
	{
		if (D.Interface == IFace) { Desc = &D; break; }
	}
	if (!Desc) return false;

	UFunction* Fn = IFace->FindFunctionByName(FName(*FunctionName));
	if (!Fn) return false;

	// Event-type interface members (BlueprintImplementableEvent with no return, no out params)
	// don't use a dedicated function graph — caller should use AddEventNode on the EventGraph.
	if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Fn)) return false;

	// Already implemented?
	for (UEdGraph* G : Desc->Graphs) { if (G && G->GetFName() == Fn->GetFName()) return true; }

	BP->Modify();
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, Fn->GetFName(), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!NewGraph) return false;
	NewGraph->bAllowDeletion = false;
	NewGraph->InterfaceGuid = FBlueprintEditorUtils::FindInterfaceFunctionGuid(Fn, IFace);
	Desc->Graphs.Add(NewGraph);
	FBlueprintEditorUtils::AddInterfaceGraph(BP, NewGraph, IFace);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::AddInterfaceMessageNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& InterfacePath, const FString& FunctionName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UClass* IFace = BridgeBpInterfaceOps::ResolveInterfaceClass(InterfacePath);
	if (!IFace || !IFace->HasAnyClassFlags(CLASS_Interface)) return FString();
	UFunction* Fn = IFace->FindFunctionByName(FName(*FunctionName));
	if (!Fn) return FString();

	Graph->Modify(); BP->Modify();
	UK2Node_Message* Node = NewObject<UK2Node_Message>(Graph);
	Node->FunctionReference.SetExternalMember(Fn->GetFName(), IFace);
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Variable metadata / type ───────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::SetVariableMetadata(
	const FString& BlueprintPath, const FString& VariableName,
	bool bInstanceEditable, bool bBlueprintReadOnly, bool bExposeOnSpawn, bool bPrivate,
	const FString& Category, const FString& Tooltip, const FString& ReplicationMode)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName VarName(*VariableName);
	const int32 Idx = FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName);
	if (Idx == INDEX_NONE) return false;

	FBPVariableDescription& Var = BP->NewVariables[Idx];

	auto SetBit = [&](uint64 Bit, bool bOn)
	{
		if (bOn)  Var.PropertyFlags |= Bit;
		else      Var.PropertyFlags &= ~Bit;
	};
	// InstanceEditable = !DisableEditOnInstance
	SetBit(CPF_DisableEditOnInstance, !bInstanceEditable);
	SetBit(CPF_BlueprintReadOnly,      bBlueprintReadOnly);
	SetBit(CPF_ExposeOnSpawn,          bExposeOnSpawn);
	// Private = DisableEditOnInstance && ~BlueprintVisible via metadata? The common flag is
	// CPF_Protected via metadata "BlueprintPrivate". Use meta key.
	if (bPrivate) Var.SetMetaData(TEXT("BlueprintPrivate"), TEXT("true"));
	else          Var.RemoveMetaData(TEXT("BlueprintPrivate"));

	if (!Category.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, VarName, nullptr,
			Category.TrimStartAndEnd().IsEmpty() ? FText::GetEmpty() : FText::FromString(Category));
	}
	if (!Tooltip.IsEmpty())
	{
		Var.SetMetaData(TEXT("tooltip"), Tooltip.TrimStartAndEnd().IsEmpty() ? TEXT("") : *Tooltip);
	}
	if (!ReplicationMode.IsEmpty())
	{
		Var.PropertyFlags &= ~(CPF_Net | CPF_RepNotify);
		Var.ReplicationCondition = ELifetimeCondition::COND_None;
		if (ReplicationMode.Equals(TEXT("Replicated"), ESearchCase::IgnoreCase))
		{
			Var.PropertyFlags |= CPF_Net;
		}
		else if (ReplicationMode.Equals(TEXT("RepNotify"), ESearchCase::IgnoreCase))
		{
			Var.PropertyFlags |= (CPF_Net | CPF_RepNotify);
			// Caller is expected to set RepNotifyFunc via a separate call (not in P0 scope).
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::SetVariableType(
	const FString& BlueprintPath, const FString& VariableName, const FString& NewTypeString)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName VarName(*VariableName);
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName) == INDEX_NONE) return false;

	FEdGraphPinType NewType;
	if (!ParseTypeString(NewTypeString, NewType)) return false;

	FBlueprintEditorUtils::ChangeMemberVariableType(BP, VarName, NewType);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── Compile feedback ───────────────────────────────────────────

TArray<FBridgeCompileMessage> UUnrealBridgeBlueprintLibrary::GetCompileErrors(const FString& BlueprintPath)
{
	TArray<FBridgeCompileMessage> Out;
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return Out;

	FCompilerResultsLog Log;
	Log.SetSourcePath(BP->GetPathName());
	Log.BeginEvent(TEXT("Compile"));
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
	Log.EndEvent();

	auto Sev = [](EMessageSeverity::Type S) -> const TCHAR*
	{
		switch (S)
		{
		case EMessageSeverity::Error:             return TEXT("Error");
		case EMessageSeverity::PerformanceWarning: // fallthrough
		case EMessageSeverity::Warning:           return TEXT("Warning");
		case EMessageSeverity::Info:              return TEXT("Info");
		default:                                  return TEXT("Note");
		}
	};

	for (const TSharedRef<FTokenizedMessage>& Msg : Log.Messages)
	{
		FBridgeCompileMessage Entry;
		Entry.Severity = Sev(Msg->GetSeverity());
		Entry.Message  = Msg->ToText().ToString();
		// Try to extract a node guid from token objects.
		for (const TSharedRef<IMessageToken>& Tok : Msg->GetMessageTokens())
		{
			if (Tok->GetType() == EMessageToken::Object)
			{
				const TSharedRef<FUObjectToken> ObjTok = StaticCastSharedRef<FUObjectToken>(Tok);
				if (const UObject* Obj = ObjTok->GetObject().Get())
				{
					if (const UEdGraphNode* N = Cast<UEdGraphNode>(Obj))
					{
						Entry.NodeGuid = N->NodeGuid.ToString(EGuidFormats::Digits);
						break;
					}
				}
			}
		}
		Out.Add(MoveTemp(Entry));
	}
	return Out;
}

// ═══════════════════════════════════════════════════════════════════
//   P1 helpers
// ═══════════════════════════════════════════════════════════════════

namespace BridgeBpP1Impl
{
	UEdGraph* FindStandardMacro(const TCHAR* MacroName)
	{
		UBlueprint* MacrosBP = LoadObject<UBlueprint>(nullptr,
			TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
		if (!MacrosBP) return nullptr;
		const FName Target(MacroName);
		for (UEdGraph* G : MacrosBP->MacroGraphs)
		{
			if (G && G->GetFName() == Target) return G;
		}
		return nullptr;
	}

	FString AddMacroNodeByName(UBlueprint* BP, UEdGraph* Graph,
		const TCHAR* MacroName, int32 X, int32 Y)
	{
		UEdGraph* Macro = FindStandardMacro(MacroName);
		if (!Macro) return FString();
		Graph->Modify(); BP->Modify();
		UK2Node_MacroInstance* Node = NewObject<UK2Node_MacroInstance>(Graph);
		Node->SetMacroGraph(Macro);
		BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		return Node->NodeGuid.ToString(EGuidFormats::Digits);
	}

	// Resolve {component name} → SCS_Node in the BP's SimpleConstructionScript.
	// Walks across parent BPs too (inherited components live there).
	USCS_Node* FindSCSNodeInHierarchy(UBlueprint* BP, const FName& CompName, UBlueprint*& OutOwnerBP)
	{
		OutOwnerBP = nullptr;
		for (UBlueprint* Cur = BP; Cur; )
		{
			if (Cur->SimpleConstructionScript)
			{
				if (USCS_Node* N = Cur->SimpleConstructionScript->FindSCSNode(CompName))
				{
					OutOwnerBP = Cur;
					return N;
				}
			}
			UClass* PC = Cur->ParentClass;
			Cur = (PC && PC->ClassGeneratedBy) ? Cast<UBlueprint>(PC->ClassGeneratedBy) : nullptr;
		}
		return nullptr;
	}

	// Return parent SCS_Node (same SCS) that currently owns InNode in its ChildNodes, or nullptr if root.
	USCS_Node* FindSCSParent(USimpleConstructionScript* SCS, USCS_Node* InNode)
	{
		if (!SCS || !InNode) return nullptr;
		TArray<USCS_Node*> All = SCS->GetAllNodes();
		for (USCS_Node* N : All)
		{
			if (N && N != InNode && N->GetChildNodes().Contains(InNode)) return N;
		}
		return nullptr;
	}
}

// ─── Control-flow: loops / select / literal ──────────────────────

FString UUnrealBridgeBlueprintLibrary::AddForeachNode(
	const FString& BlueprintPath, const FString& GraphName, bool bWithBreak, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	return BridgeBpP1Impl::AddMacroNodeByName(BP, Graph,
		bWithBreak ? TEXT("ForEachLoopWithBreak") : TEXT("ForEachLoop"), X, Y);
}

FString UUnrealBridgeBlueprintLibrary::AddForLoopNode(
	const FString& BlueprintPath, const FString& GraphName, bool bWithBreak, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	return BridgeBpP1Impl::AddMacroNodeByName(BP, Graph,
		bWithBreak ? TEXT("ForLoopWithBreak") : TEXT("ForLoop"), X, Y);
}

FString UUnrealBridgeBlueprintLibrary::AddWhileLoopNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	return BridgeBpP1Impl::AddMacroNodeByName(BP, Graph, TEXT("WhileLoop"), X, Y);
}

FString UUnrealBridgeBlueprintLibrary::AddSelectNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_Select* Node = BridgeBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_Select>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddMakeLiteralNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TypeString, const FString& Value, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	FString FnName;
	const FString T = TypeString.TrimStartAndEnd();
	if      (T.Equals(TEXT("Int"),    ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralInt");
	else if (T.Equals(TEXT("Int64"),  ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralInt64");
	else if (T.Equals(TEXT("Float"),  ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralDouble"); // UE5 float=double
	else if (T.Equals(TEXT("Double"), ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralDouble");
	else if (T.Equals(TEXT("Bool"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralBool");
	else if (T.Equals(TEXT("Byte"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralByte");
	else if (T.Equals(TEXT("Name"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralName");
	else if (T.Equals(TEXT("String"), ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralString");
	else if (T.Equals(TEXT("Text"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralText");
	else return FString();

	UFunction* Fn = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FnName));
	if (!Fn) return FString();

	Graph->Modify(); BP->Modify();
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->FunctionReference.SetExternalMember(Fn->GetFName(), UKismetSystemLibrary::StaticClass());
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);

	// Set the Value pin default if provided.
	if (!Value.IsEmpty())
	{
		if (UEdGraphPin* ValuePin = Node->FindPin(TEXT("Value")))
		{
			GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*ValuePin, Value);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Graph layout ────────────────────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::AlignNodes(
	const FString& BlueprintPath, const FString& GraphName,
	const TArray<FString>& NodeGuids, const FString& Axis)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	if (NodeGuids.Num() < 2) return false;

	TArray<UEdGraphNode*> Nodes;
	for (const FString& G : NodeGuids)
	{
		if (UEdGraphNode* N = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, G)) Nodes.Add(N);
	}
	if (Nodes.Num() < 2) return false;

	Graph->Modify();
	for (UEdGraphNode* N : Nodes) N->Modify();

	const FString A = Axis.TrimStartAndEnd();
	if (A.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		int32 Min = Nodes[0]->NodePosX;
		for (UEdGraphNode* N : Nodes) Min = FMath::Min(Min, N->NodePosX);
		for (UEdGraphNode* N : Nodes) N->NodePosX = Min;
	}
	else if (A.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		int32 Max = Nodes[0]->NodePosX;
		for (UEdGraphNode* N : Nodes) Max = FMath::Max(Max, N->NodePosX);
		for (UEdGraphNode* N : Nodes) N->NodePosX = Max;
	}
	else if (A.Equals(TEXT("Top"), ESearchCase::IgnoreCase))
	{
		int32 Min = Nodes[0]->NodePosY;
		for (UEdGraphNode* N : Nodes) Min = FMath::Min(Min, N->NodePosY);
		for (UEdGraphNode* N : Nodes) N->NodePosY = Min;
	}
	else if (A.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase))
	{
		int32 Max = Nodes[0]->NodePosY;
		for (UEdGraphNode* N : Nodes) Max = FMath::Max(Max, N->NodePosY);
		for (UEdGraphNode* N : Nodes) N->NodePosY = Max;
	}
	else if (A.Equals(TEXT("CenterHorizontal"), ESearchCase::IgnoreCase))
	{
		int64 Sum = 0; for (UEdGraphNode* N : Nodes) Sum += N->NodePosX;
		int32 Avg = (int32)(Sum / Nodes.Num());
		for (UEdGraphNode* N : Nodes) N->NodePosX = Avg;
	}
	else if (A.Equals(TEXT("CenterVertical"), ESearchCase::IgnoreCase))
	{
		int64 Sum = 0; for (UEdGraphNode* N : Nodes) Sum += N->NodePosY;
		int32 Avg = (int32)(Sum / Nodes.Num());
		for (UEdGraphNode* N : Nodes) N->NodePosY = Avg;
	}
	else if (A.Equals(TEXT("DistributeHorizontal"), ESearchCase::IgnoreCase))
	{
		Nodes.Sort([](const UEdGraphNode& L, const UEdGraphNode& R){ return L.NodePosX < R.NodePosX; });
		const int32 First = Nodes[0]->NodePosX;
		const int32 Last  = Nodes.Last()->NodePosX;
		const int32 N     = Nodes.Num();
		for (int32 i = 1; i < N - 1; ++i) Nodes[i]->NodePosX = First + (Last - First) * i / (N - 1);
	}
	else if (A.Equals(TEXT("DistributeVertical"), ESearchCase::IgnoreCase))
	{
		Nodes.Sort([](const UEdGraphNode& L, const UEdGraphNode& R){ return L.NodePosY < R.NodePosY; });
		const int32 First = Nodes[0]->NodePosY;
		const int32 Last  = Nodes.Last()->NodePosY;
		const int32 N     = Nodes.Num();
		for (int32 i = 1; i < N - 1; ++i) Nodes[i]->NodePosY = First + (Last - First) * i / (N - 1);
	}
	else
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::AddCommentBox(
	const FString& BlueprintPath, const FString& GraphName,
	const TArray<FString>& NodeGuids, const FString& Text,
	int32 X, int32 Y, int32 Width, int32 Height)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	Graph->Modify(); BP->Modify();
	UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph);
	Comment->CreateNewGuid();

	// If node GUIDs were provided, fit around them.
	if (NodeGuids.Num() > 0)
	{
		int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
		bool bHit = false;
		for (const FString& G : NodeGuids)
		{
			if (UEdGraphNode* N = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, G))
			{
				MinX = FMath::Min(MinX, N->NodePosX);
				MinY = FMath::Min(MinY, N->NodePosY);
				MaxX = FMath::Max(MaxX, N->NodePosX + FMath::Max(N->NodeWidth, 200));
				MaxY = FMath::Max(MaxY, N->NodePosY + FMath::Max(N->NodeHeight, 80));
				bHit = true;
			}
		}
		if (bHit)
		{
			const int32 Pad = 32;
			Comment->NodePosX = MinX - Pad;
			Comment->NodePosY = MinY - Pad - 32;
			Comment->NodeWidth  = (MaxX - MinX) + Pad * 2;
			Comment->NodeHeight = (MaxY - MinY) + Pad * 2 + 32;
		}
	}
	else
	{
		Comment->NodePosX = X; Comment->NodePosY = Y;
		Comment->NodeWidth  = Width  > 0 ? Width  : 400;
		Comment->NodeHeight = Height > 0 ? Height : 200;
	}
	Graph->AddNode(Comment, /*bFromUI*/false, /*bSelectNewNode*/false);
	Comment->PostPlacedNewNode();
	// NodeComment must be assigned AFTER PostPlacedNewNode — that call resets it
	// to the localized default "Comment" string, clobbering the caller's text.
	Comment->NodeComment = Text;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Comment->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddRerouteNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_Knot* Node = BridgeBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_Knot>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

bool UUnrealBridgeBlueprintLibrary::SetNodeEnabled(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& EnabledState)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid); if (!Node) return false;

	const FString S = EnabledState.TrimStartAndEnd();
	ENodeEnabledState NewState;
	if      (S.Equals(TEXT("Enabled"),         ESearchCase::IgnoreCase)) NewState = ENodeEnabledState::Enabled;
	else if (S.Equals(TEXT("Disabled"),        ESearchCase::IgnoreCase)) NewState = ENodeEnabledState::Disabled;
	else if (S.Equals(TEXT("DevelopmentOnly"), ESearchCase::IgnoreCase)) NewState = ENodeEnabledState::DevelopmentOnly;
	else return false;

	Node->Modify();
	Node->SetEnabledState(NewState, /*bUserAction*/true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── Class settings ──────────────────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::ReparentBlueprint(
	const FString& BlueprintPath, const FString& NewParentPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UClass* NewParent = BridgeBlueprintGraphWriteImpl::ResolveTargetClass(BP, NewParentPath);
	if (!NewParent) return false;
	if (NewParent == BP->ParentClass) return true;
	if (NewParent == BP->GeneratedClass) return false; // prevent self-parent

	BP->Modify();
	BP->ParentClass = NewParent;
	if (BP->SimpleConstructionScript) BP->SimpleConstructionScript->ValidateSceneRootNodes();
	FBlueprintEditorUtils::RefreshAllNodes(BP);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::SetBlueprintMetadata(
	const FString& BlueprintPath,
	const FString& DisplayName, const FString& Description,
	const FString& Category, const FString& Namespace)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	BP->Modify();
	if (!DisplayName.IsEmpty()) BP->BlueprintDisplayName = DisplayName;
	if (!Description.IsEmpty()) BP->BlueprintDescription = Description;
	if (!Category.IsEmpty())    BP->BlueprintCategory    = Category;
	if (!Namespace.IsEmpty())   BP->BlueprintNamespace   = Namespace;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── Component tree ──────────────────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::ReparentComponent(
	const FString& BlueprintPath, const FString& ComponentName, const FString& NewParentName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (!BP->SimpleConstructionScript) return false;

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* Node = SCS->FindSCSNode(FName(*ComponentName)); if (!Node) return false;

	// Detach from current parent (or root).
	USCS_Node* CurParent = BridgeBpP1Impl::FindSCSParent(SCS, Node);
	BP->Modify();
	if (CurParent) { CurParent->Modify(); CurParent->RemoveChildNode(Node, /*bRemoveFromAllNodes*/false); }
	else           { SCS->Modify();        SCS->RemoveNode(Node, /*bValidateSceneRoot*/false); }

	// Attach to new parent, or promote to root.
	if (NewParentName.IsEmpty())
	{
		SCS->AddNode(Node);
	}
	else
	{
		USCS_Node* NewParent = SCS->FindSCSNode(FName(*NewParentName));
		if (!NewParent)
		{
			// Roll back: re-add as root so we don't orphan.
			SCS->AddNode(Node);
			return false;
		}
		NewParent->Modify();
		NewParent->AddChildNode(Node, /*bAddToAllNodes*/false);
	}
	SCS->ValidateSceneRootNodes();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::ReorderComponent(
	const FString& BlueprintPath, const FString& ComponentName, int32 NewIndex)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (!BP->SimpleConstructionScript) return false;

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* Node = SCS->FindSCSNode(FName(*ComponentName)); if (!Node) return false;

	USCS_Node* Parent = BridgeBpP1Impl::FindSCSParent(SCS, Node);
	BP->Modify();

	// Acquire the current sibling list reference.
	const TArray<USCS_Node*>& Siblings = Parent ? Parent->GetChildNodes() : SCS->GetRootNodes();
	const int32 Count = Siblings.Num();
	if (Count == 0) return false;
	const int32 Clamped = FMath::Clamp(NewIndex, 0, Count - 1);

	if (Parent)
	{
		Parent->Modify();
		Parent->RemoveChildNode(Node, /*bRemoveFromAllNodes*/false);
		// Re-insert at new position.
		Parent->AddChildNode(Node, /*bAddToAllNodes*/false);
		// AddChildNode appends — pull to desired index.
		// Access mutable via const_cast since API doesn't expose mutator directly.
		TArray<USCS_Node*>& MChildren = const_cast<TArray<USCS_Node*>&>(Parent->GetChildNodes());
		MChildren.Remove(Node);
		MChildren.Insert(Node, Clamped);
	}
	else
	{
		SCS->Modify();
		SCS->RemoveNode(Node, /*bValidateSceneRoot*/false);
		SCS->AddNode(Node);
		TArray<USCS_Node*>& MRoots = const_cast<TArray<USCS_Node*>&>(SCS->GetRootNodes());
		MRoots.Remove(Node);
		MRoots.Insert(Node, Clamped);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RemoveComponent(
	const FString& BlueprintPath, const FString& ComponentName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (!BP->SimpleConstructionScript) return false;
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* Node = SCS->FindSCSNode(FName(*ComponentName)); if (!Node) return false;

	BP->Modify(); SCS->Modify();
	USCS_Node* Parent = BridgeBpP1Impl::FindSCSParent(SCS, Node);
	if (Parent) { Parent->Modify(); Parent->RemoveChildNode(Node, /*bRemoveFromAllNodes*/true); }
	else        { SCS->RemoveNode(Node); }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}

// ─── Dispatcher event node ───────────────────────────────────────

FString UUnrealBridgeBlueprintLibrary::AddDispatcherEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& DispatcherName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	FMulticastDelegateProperty* Prop = BridgeBpP0Impl::FindDispatcherProp(BP, DispatcherName);
	if (!Prop || !Prop->SignatureFunction) return FString();
	UFunction* Sig = Prop->SignatureFunction;

	Graph->Modify(); BP->Modify();
	UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
	Node->CustomFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(
		BP, FString::Printf(TEXT("On%s"), *DispatcherName));
	Node->bIsEditable = true;
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	for (TFieldIterator<FProperty> It(Sig); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
		FEdGraphPinType PinType;
		if (K2->ConvertPropertyToPinType(*It, PinType))
		{
			Node->CreateUserDefinedPin(It->GetFName(), PinType, EGPD_Output, /*bUseUniqueName*/false);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ═══ P2 implementation ═══════════════════════════════════════════

namespace BridgeBpP2Impl
{
	UScriptStruct* ResolveStruct(const FString& StructPath)
	{
		if (StructPath.IsEmpty()) return nullptr;
		if (UScriptStruct* S = FindObject<UScriptStruct>(nullptr, *StructPath)) return S;
		if (UScriptStruct* S = LoadObject<UScriptStruct>(nullptr, *StructPath)) return S;
		if (UScriptStruct* S = FindFirstObject<UScriptStruct>(*StructPath, EFindFirstObjectOptions::NativeFirst))
		{
			return S;
		}
		return nullptr;
	}

	UK2Node_CallFunction* AddKSLCall(UBlueprint* BP, UEdGraph* Graph, const TCHAR* FnName, int32 X, int32 Y)
	{
		UFunction* Fn = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(FnName));
		if (!Fn) return nullptr;
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
		Node->FunctionReference.SetExternalMember(Fn->GetFName(), UKismetSystemLibrary::StaticClass());
		BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
		return Node;
	}
}

// ─── Batch A: CallFunction wrappers ────────────────────────────

FString UUnrealBridgeBlueprintLibrary::AddDelayNode(
	const FString& BlueprintPath, const FString& GraphName,
	float DurationSeconds, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CallFunction* Node = BridgeBpP2Impl::AddKSLCall(BP, Graph, TEXT("Delay"), X, Y);
	if (!Node) return FString();
	if (UEdGraphPin* P = Node->FindPin(TEXT("Duration")))
	{
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P, FString::SanitizeFloat(DurationSeconds));
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddSetTimerByFunctionNameNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& FunctionName, float TimeSeconds, bool bLooping, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CallFunction* Node = BridgeBpP2Impl::AddKSLCall(BP, Graph, TEXT("K2_SetTimer"), X, Y);
	if (!Node) return FString();
	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* P = Node->FindPin(TEXT("FunctionName"))) K2->TrySetDefaultValue(*P, FunctionName);
	if (UEdGraphPin* P = Node->FindPin(TEXT("Time")))         K2->TrySetDefaultValue(*P, FString::SanitizeFloat(TimeSeconds));
	if (UEdGraphPin* P = Node->FindPin(TEXT("bLooping")))     K2->TrySetDefaultValue(*P, bLooping ? TEXT("true") : TEXT("false"));
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddSpawnActorFromClassNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ActorClassPath, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UClass* SpawnClass = BridgeBlueprintGraphWriteImpl::ResolveTargetClass(BP, ActorClassPath);
	if (!SpawnClass || !SpawnClass->IsChildOf(AActor::StaticClass())) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_SpawnActorFromClass* Node = NewObject<UK2Node_SpawnActorFromClass>(Graph);
	// SpawnActorFromClass::PostPlacedNewNode requires pins to exist (uses FindPinChecked),
	// so allocate pins first, add to graph, then post-place.
	Node->CreateNewGuid();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->AllocateDefaultPins();
	Node->PostPlacedNewNode();
	if (UEdGraphPin* ClassPin = Node->GetClassPin())
	{
		ClassPin->DefaultObject = SpawnClass;
		ClassPin->DefaultValue.Empty();
		// Trigger pin regeneration (exposed spawn vars) without a full reconstruct.
		Node->PinDefaultValueChanged(ClassPin);
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Batch B: Struct Make / Break ─────────────────────────────

FString UUnrealBridgeBlueprintLibrary::AddMakeStructNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& StructPath, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UScriptStruct* S = BridgeBpP2Impl::ResolveStruct(StructPath);
	// Allow native-make structs too by passing bForInternalUse=true (matches "advanced" UI path).
	if (!S || !UK2Node_MakeStruct::CanBeMade(S, /*bForInternalUse*/true)) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_MakeStruct* Node = NewObject<UK2Node_MakeStruct>(Graph);
	Node->StructType = S;
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddBreakStructNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& StructPath, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UScriptStruct* S = BridgeBpP2Impl::ResolveStruct(StructPath);
	if (!S) return FString();
	// Note: UK2Node_BreakStruct::CanBeBroken is not DLL-exported; rely on compile-time validation instead.
	Graph->Modify(); BP->Modify();
	UK2Node_BreakStruct* Node = NewObject<UK2Node_BreakStruct>(Graph);
	Node->StructType = S;
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Batch C: Graph extras ─────────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::CreateMacroGraph(
	const FString& BlueprintPath, const FString& MacroName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName MName(*MacroName);
	if (MName.IsNone()) return false;
	for (UEdGraph* G : BP->MacroGraphs) { if (G && G->GetFName() == MName) return false; }

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, MName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(BP, NewGraph, /*bIsUserCreated*/true, /*SignatureFromClass*/nullptr);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::AddBreakpoint(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, bool bEnabled)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	FKismetDebugUtilities::CreateBreakpoint(BP, Node, bEnabled);
	FKismetDebugUtilities::SetBreakpointEnabled(Node, BP, bEnabled);
	return true;
}

// ─── Batch D: Timeline ─────────────────────────────────────────

FString UUnrealBridgeBlueprintLibrary::AddTimelineNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TimelineTemplateName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	if (!FBlueprintEditorUtils::DoesSupportTimelines(BP)) return FString();

	FName TLName;
	if (TimelineTemplateName.IsEmpty())
	{
		TLName = FBlueprintEditorUtils::FindUniqueTimelineName(BP);
	}
	else
	{
		TLName = FName(*TimelineTemplateName);
	}

	Graph->Modify(); BP->Modify();

	UTimelineTemplate* Template = nullptr;
	const int32 ExistingIdx = FBlueprintEditorUtils::FindTimelineIndex(BP, TLName);
	if (ExistingIdx != INDEX_NONE)
	{
		Template = BP->Timelines[ExistingIdx];
	}
	else
	{
		Template = FBlueprintEditorUtils::AddNewTimeline(BP, TLName);
	}
	if (!Template) return FString();

	UK2Node_Timeline* Node = NewObject<UK2Node_Timeline>(Graph);
	Node->TimelineName = TLName;
	Node->TimelineGuid = Template->TimelineGuid;
	Node->bAutoPlay = Template->bAutoPlay;
	Node->bLoop = Template->bLoop;
	Node->bReplicated = Template->bReplicated;
	Node->bIgnoreTimeDilation = Template->bIgnoreTimeDilation;
	BridgeBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

bool UUnrealBridgeBlueprintLibrary::SetTimelineProperties(
	const FString& BlueprintPath, const FString& TimelineName,
	float Length, bool bAutoPlay, bool bLoop, bool bReplicated, bool bIgnoreTimeDilation)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (TimelineName.IsEmpty()) return false;
	const FName TLName(*TimelineName);
	const int32 Idx = FBlueprintEditorUtils::FindTimelineIndex(BP, TLName);
	if (Idx == INDEX_NONE) return false;
	UTimelineTemplate* Template = BP->Timelines[Idx];
	if (!Template) return false;

	BP->Modify();
	Template->Modify();
	if (Length >= 0.0f)
	{
		Template->TimelineLength = Length;
	}
	Template->bAutoPlay = bAutoPlay;
	Template->bLoop = bLoop;
	Template->bReplicated = bReplicated;
	Template->bIgnoreTimeDilation = bIgnoreTimeDilation;

	// Mirror into the K2Node_Timeline instance so the graph node reflects the new settings.
	if (UK2Node_Timeline* TLNode = FBlueprintEditorUtils::FindNodeForTimeline(BP, Template))
	{
		TLNode->Modify();
		TLNode->bAutoPlay = bAutoPlay;
		TLNode->bLoop = bLoop;
		TLNode->bReplicated = bReplicated;
		TLNode->bIgnoreTimeDilation = bIgnoreTimeDilation;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── Batch E: Macro / Debug management ─────────────────────────

bool UUnrealBridgeBlueprintLibrary::RemoveMacroGraph(
	const FString& BlueprintPath, const FString& MacroName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName MName(*MacroName);
	if (MName.IsNone()) return false;
	UEdGraph* Target = nullptr;
	for (UEdGraph* G : BP->MacroGraphs)
	{
		if (G && G->GetFName() == MName) { Target = G; break; }
	}
	if (!Target) return false;
	BP->Modify();
	FBlueprintEditorUtils::RemoveGraph(BP, Target, EGraphRemoveFlags::Recompile);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::RemoveBreakpoint(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	if (!FKismetDebugUtilities::FindBreakpointForNode(Node, BP)) return false;
	FKismetDebugUtilities::RemoveBreakpointFromNode(Node, BP);
	return true;
}

int32 UUnrealBridgeBlueprintLibrary::ClearAllBreakpoints(const FString& BlueprintPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return 0;
	int32 Count = 0;
	FKismetDebugUtilities::ForeachBreakpoint(BP, [&Count](FBlueprintBreakpoint&) { ++Count; });
	if (Count > 0)
	{
		FKismetDebugUtilities::ClearBreakpoints(BP);
	}
	return Count;
}

TArray<FBridgeBreakpointInfo> UUnrealBridgeBlueprintLibrary::GetBreakpoints(const FString& BlueprintPath)
{
	TArray<FBridgeBreakpointInfo> Out;
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return Out;
	FKismetDebugUtilities::ForeachBreakpoint(BP, [&Out](FBlueprintBreakpoint& BP_BP)
	{
		FBridgeBreakpointInfo Info;
		UEdGraphNode* Node = BP_BP.GetLocation();
		if (Node)
		{
			Info.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
			Info.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			if (UEdGraph* OwningGraph = Node->GetGraph())
			{
				Info.GraphName = OwningGraph->GetName();
			}
		}
		Info.bEnabled = BP_BP.IsEnabledByUser();
		Out.Add(Info);
	});
	return Out;
}

// ─── Batch F: Node utilities ───────────────────────────────────

bool UUnrealBridgeBlueprintLibrary::SetNodeComment(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& Comment, bool bCommentBubbleVisible)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	Node->NodeComment = Comment;
	Node->bCommentBubbleVisible = bCommentBubbleVisible && !Comment.IsEmpty();
	Node->bCommentBubblePinned = bCommentBubbleVisible && !Comment.IsEmpty();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::DuplicateGraphNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UEdGraphNode* Src = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Src) return FString();

	Graph->Modify();
	BP->Modify();

	UEdGraphNode* Dup = DuplicateObject<UEdGraphNode>(Src, Graph);
	if (!Dup) return FString();
	Dup->CreateNewGuid();
	Dup->NodePosX = NodePosX;
	Dup->NodePosY = NodePosY;
	// Break any links that the duplicate inherited from Src.
	for (UEdGraphPin* Pin : Dup->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}
	Graph->AddNode(Dup, /*bFromUI*/false, /*bSelectNewNode*/false);
	Dup->PostPlacedNewNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Dup->NodeGuid.ToString(EGuidFormats::Digits);
}

bool UUnrealBridgeBlueprintLibrary::DisconnectGraphPin(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin) return false;
	if (Pin->LinkedTo.Num() == 0) return false;

	Graph->Modify();
	BP->Modify();
	Pin->BreakAllPinLinks();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

FString UUnrealBridgeBlueprintLibrary::AddMakeArrayNode(
	const FString& BlueprintPath, const FString& GraphName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_MakeArray* Node = NewObject<UK2Node_MakeArray>(Graph);
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

FString UUnrealBridgeBlueprintLibrary::AddEnumLiteralNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& EnumPath, const FString& ValueName, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	UEnum* EnumObj = FindObject<UEnum>(nullptr, *EnumPath);
	if (!EnumObj) EnumObj = LoadObject<UEnum>(nullptr, *EnumPath);
	if (!EnumObj) return FString();

	// Resolve the chosen entry. Enum entries are stored as "EnumName::EntryName" — match on short name.
	FString ResolvedEntry;
	const int32 NumEntries = EnumObj->NumEnums();
	if (NumEntries <= 1) return FString(); // only the hidden _MAX sentinel
	if (ValueName.IsEmpty())
	{
		ResolvedEntry = EnumObj->GetNameStringByIndex(0);
	}
	else
	{
		for (int32 i = 0; i < NumEntries; ++i)
		{
			const FString ShortName = EnumObj->GetNameStringByIndex(i);
			if (ShortName == ValueName || EnumObj->GetNameByIndex(i).ToString() == ValueName)
			{
				ResolvedEntry = ShortName;
				break;
			}
		}
	}
	if (ResolvedEntry.IsEmpty()) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_EnumLiteral* Node = NewObject<UK2Node_EnumLiteral>(Graph);
	Node->Enum = EnumObj;
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	// Default the Enum input pin to the chosen entry.
	if (UEdGraphPin* EnumPin = Node->FindPin(UK2Node_EnumLiteral::GetEnumInputPinName()))
	{
		EnumPin->DefaultValue = ResolvedEntry;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}

// ═════════════════════════════════════════════════════════════════
// Semantic summary layer (agent-understanding helpers)
// ═════════════════════════════════════════════════════════════════

namespace BridgeBPSummaryImpl
{
	/** Classify a graph against a loaded BP. */
	static FString ClassifyGraph(const UBlueprint* BP, const UEdGraph* Graph)
	{
		if (!BP || !Graph) return TEXT("Unknown");
		UEdGraph* G = const_cast<UEdGraph*>(Graph);
		if (BP->UbergraphPages.Contains(G)) return TEXT("EventGraph");
		if (BP->FunctionGraphs.Contains(G))  return TEXT("Function");
		if (BP->MacroGraphs.Contains(G))     return TEXT("Macro");
		return TEXT("Unknown");
	}

	struct FAllGraphs { UEdGraph* Graph; FString Type; };
	static TArray<FAllGraphs> CollectAllGraphs(UBlueprint* BP)
	{
		TArray<FAllGraphs> Out;
		for (UEdGraph* G : BP->UbergraphPages) if (G) Out.Add({G, TEXT("EventGraph")});
		for (UEdGraph* G : BP->FunctionGraphs)  if (G) Out.Add({G, TEXT("Function")});
		for (UEdGraph* G : BP->MacroGraphs)     if (G) Out.Add({G, TEXT("Macro")});
		return Out;
	}

	/** Pull an object-ref asset path out of a pin's default value if it's an
	 *  asset reference; returns empty if not an object pin or unset. */
	static FString PinAssetReference(const UEdGraphPin* Pin)
	{
		if (!Pin) return FString();
		if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_SoftObject &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Class &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_SoftClass)
		{
			return FString();
		}
		if (Pin->DefaultObject)
		{
			return Pin->DefaultObject->GetPathName();
		}
		// Plain path defaults (soft refs etc.)
		if (!Pin->DefaultValue.IsEmpty() &&
			(Pin->DefaultValue.StartsWith(TEXT("/")) || Pin->DefaultValue.Contains(TEXT("."))))
		{
			return Pin->DefaultValue;
		}
		return FString();
	}

	/** Sort a name→count map into a TArray of names by descending count, cap N. */
	static TArray<FString> TopN(const TMap<FString, int32>& Counter, int32 N)
	{
		TArray<TPair<FString, int32>> Pairs;
		for (const auto& Pair : Counter) { Pairs.Add({Pair.Key, Pair.Value}); }
		Pairs.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
		{
			return A.Value > B.Value;
		});
		TArray<FString> Out;
		for (int32 i = 0; i < FMath::Min(N, Pairs.Num()); ++i) { Out.Add(Pairs[i].Key); }
		return Out;
	}
}

bool UUnrealBridgeBlueprintLibrary::GetBlueprintSummary(
	const FString& BlueprintPath, FBridgeBlueprintSummary& OutSummary)
{
	using namespace BridgeBPSummaryImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* Super = GenClass->GetSuperClass();

	OutSummary.Name = BP->GetName();
	OutSummary.Path = BP->GetPathName();
	if (Super)
	{
		OutSummary.ParentClass = Super->GetName();
		OutSummary.ParentClassPath = Super->GetPathName();
	}

	// First native ancestor.
	const UClass* NativeBase = Super;
	while (NativeBase
		&& (NativeBase->IsChildOf<UBlueprintGeneratedClass>()
			|| NativeBase->HasAnyClassFlags(CLASS_CompiledFromBlueprint)))
	{
		NativeBase = NativeBase->GetSuperClass();
	}
	OutSummary.BlueprintType = NativeBase ? NativeBase->GetName() : TEXT("Object");

	// Variables.
	TSet<FString> CategorySet;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		const FProperty* Prop = GenClass->FindPropertyByName(Var.VarName);
		if (Prop && CastField<FMulticastDelegateProperty>(Prop)) continue;

		OutSummary.VariableCount += 1;
		if (Prop && Prop->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit))
		{
			OutSummary.InstanceEditableCount += 1;
		}
		if (Var.ReplicationCondition != COND_None || (Prop && Prop->HasAnyPropertyFlags(CPF_Net)))
		{
			OutSummary.ReplicatedVariableCount += 1;
		}
		const FString Cat = Var.Category.ToString();
		if (!Cat.IsEmpty() && Cat != TEXT("Default")) CategorySet.Add(Cat);
	}
	OutSummary.VariableCategories = CategorySet.Array();
	OutSummary.VariableCategories.Sort();

	// Functions + events handled.
	TSet<FString> EventsSet;
	for (TFieldIterator<UFunction> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const UFunction* Func = *It;
		if (!Func) continue;
		const FString FuncName = Func->GetName();
		if (FuncName.StartsWith(TEXT("ExecuteUbergraph"))) continue;
		if (FuncName.Contains(TEXT("__DelegateSignature"))) continue;
		OutSummary.FunctionCount += 1;

		const bool bIsEventOrOverride = Func->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent);
		const bool bIsOverride = Super && Super->FindFunctionByName(Func->GetFName()) != nullptr;
		if (bIsEventOrOverride && bIsOverride) EventsSet.Add(FuncName);

		OutSummary.PublicFunctions.Add(FuncName);
	}

	// Event nodes in Ubergraph are another source of "events handled".
	for (UEdGraph* G : BP->UbergraphPages)
	{
		if (!G) continue;
		for (UEdGraphNode* N : G->Nodes)
		{
			if (const UK2Node_Event* Evt = Cast<UK2Node_Event>(N))
			{
				EventsSet.Add(Evt->GetFunctionName().ToString());
			}
		}
	}
	OutSummary.EventsHandled = EventsSet.Array();
	OutSummary.EventsHandled.Sort();
	OutSummary.PublicFunctions.Sort();

	// Macros.
	for (UEdGraph* G : BP->MacroGraphs)
	{
		if (G) OutSummary.MacroCount += 1;
	}

	// Components.
	if (USimpleConstructionScript* SCS = BP->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->ComponentClass) OutSummary.ComponentCount += 1;
		}
	}

	// Timelines.
	OutSummary.TimelineCount = BP->Timelines.Num();

	// Event dispatchers.
	for (UEdGraph* SigGraph : BP->DelegateSignatureGraphs)
	{
		if (!SigGraph) continue;
		FString Name = SigGraph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (Name.EndsWith(DelegateSuffix)) Name = Name.LeftChop(DelegateSuffix.Len());
		OutSummary.EventDispatchers.Add(Name);
	}

	// Interfaces.
	for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
	{
		if (Iface.Interface) OutSummary.Interfaces.Add(Iface.Interface->GetName());
	}

	// Walk every node across every graph once.
	TMap<FString, int32> ClassCallFreq;
	TMap<FString, int32> AssetRefFreq;
	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;
			OutSummary.TotalNodeCount += 1;

			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (UFunction* Func = CallNode->GetTargetFunction())
				{
					if (UClass* Own = Func->GetOwnerClass())
					{
						// Exclude self-class calls — agent cares about "what other systems does this BP talk to".
						if (Own != GenClass)
						{
							ClassCallFreq.FindOrAdd(Own->GetName()) += 1;
						}
					}
				}
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				const FString Ref = PinAssetReference(Pin);
				if (!Ref.IsEmpty()) AssetRefFreq.FindOrAdd(Ref) += 1;
			}
		}
	}
	// Component class refs are "references" too.
	if (USimpleConstructionScript* SCS = BP->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->ComponentClass)
			{
				AssetRefFreq.FindOrAdd(Node->ComponentClass->GetPathName()) += 1;
			}
		}
	}

	OutSummary.KeyReferencedClasses = TopN(ClassCallFreq, 10);
	OutSummary.KeyReferencedAssets  = TopN(AssetRefFreq, 10);

	return true;
}

// ─── GetFunctionSummary ─────────────────────────────────────────

namespace BridgeBPSummaryImpl
{
	/** Cap on emitted outline lines; prevents blow-up on 500-node event graphs. */
	constexpr int32 MaxOutlineLines = 200;
	constexpr int32 MaxOutlineDepth = 8;

	/** Short human-readable describe of a node for one outline line, no indent. */
	static FString DescribeNodeOneLine(const UEdGraphNode* Node)
	{
		if (const UK2Node_IfThenElse* /*Branch*/ _ = Cast<UK2Node_IfThenElse>(Node))
		{
			return TEXT("Branch");
		}
		if (const UK2Node_ExecutionSequence* Seq = Cast<UK2Node_ExecutionSequence>(Node))
		{
			int32 Count = 0;
			for (const UEdGraphPin* P : Seq->Pins)
			{
				if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) ++Count;
			}
			return FString::Printf(TEXT("Sequence (%d outputs)"), Count);
		}
		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			UFunction* Func = Call->GetTargetFunction();
			FString FuncName = Func ? Func->GetName() : Call->GetFunctionName().ToString();
			FString ClassName = (Func && Func->GetOwnerClass()) ? Func->GetOwnerClass()->GetName() : TEXT("?");
			return FString::Printf(TEXT("Call %s.%s"), *ClassName, *FuncName);
		}
		if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
		{
			return FString::Printf(TEXT("Set %s"), *Set->GetVarNameString());
		}
		if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
		{
			return FString::Printf(TEXT("Get %s"), *Get->GetVarNameString());
		}
		if (const UK2Node_DynamicCast* CastN = Cast<UK2Node_DynamicCast>(Node))
		{
			FString T = CastN->TargetType ? CastN->TargetType->GetName() : TEXT("?");
			return FString::Printf(TEXT("Cast to %s"), *T);
		}
		if (const UK2Node_MacroInstance* Mac = Cast<UK2Node_MacroInstance>(Node))
		{
			UEdGraph* MG = Mac->GetMacroGraph();
			return FString::Printf(TEXT("Macro %s"), MG ? *MG->GetName() : TEXT("?"));
		}
		if (const UK2Node_SpawnActorFromClass* Spawn = Cast<UK2Node_SpawnActorFromClass>(Node))
		{
			if (const UEdGraphPin* ClassPin = Spawn->GetClassPin())
			{
				FString C = ClassPin->DefaultObject ? ClassPin->DefaultObject->GetName() : TEXT("?");
				return FString::Printf(TEXT("Spawn %s"), *C);
			}
			return TEXT("Spawn ?");
		}
		if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node))
		{
			return FString::Printf(TEXT("Fire dispatcher %s"), *CD->GetPropertyName().ToString());
		}
		if (const UK2Node_AddDelegate* AD = Cast<UK2Node_AddDelegate>(Node))
		{
			return FString::Printf(TEXT("Bind dispatcher %s"), *AD->GetPropertyName().ToString());
		}
		if (const UK2Node_RemoveDelegate* RD = Cast<UK2Node_RemoveDelegate>(Node))
		{
			return FString::Printf(TEXT("Unbind dispatcher %s"), *RD->GetPropertyName().ToString());
		}
		if (const UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
		{
			return FString::Printf(TEXT("Event %s (custom)"), *CE->CustomFunctionName.ToString());
		}
		if (const UK2Node_Event* Ev = Cast<UK2Node_Event>(Node))
		{
			return FString::Printf(TEXT("Event %s"), *Ev->GetFunctionName().ToString());
		}
		if (Cast<UK2Node_FunctionEntry>(Node))
		{
			return TEXT("Entry");
		}
		if (Cast<UK2Node_FunctionResult>(Node))
		{
			return TEXT("Return");
		}
		if (Cast<UK2Node_Message>(Node))
		{
			return FString::Printf(TEXT("Interface msg %s"),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		}
		// Skip reroute knots in outline.
		if (Cast<UK2Node_Knot>(Node)) return FString();
		return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	}

	/** Detect loop macros by name match. */
	static bool IsLoopMacro(const UK2Node_MacroInstance* Mac)
	{
		if (!Mac) return false;
		UEdGraph* G = Mac->GetMacroGraph();
		if (!G) return false;
		const FString N = G->GetName();
		return N.Contains(TEXT("ForEachLoop")) || N.Contains(TEXT("ForLoop"))
			|| N.Contains(TEXT("WhileLoop")) || N.Contains(TEXT("ReverseForEachLoop"));
	}

	/** Exec-output pins of a node, in declaration order, labelled with pin name. */
	struct FExecOut { FName PinName; UEdGraphNode* Target = nullptr; };
	static TArray<FExecOut> GetExecOutputs(const UEdGraphNode* Node)
	{
		TArray<FExecOut> Out;
		if (!Node) return Out;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked)
				{
					// Follow through knot (reroute) nodes transparently.
					UEdGraphNode* T = Linked->GetOwningNode();
					while (const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(T))
					{
						const UEdGraphPin* Next = Knot->GetOutputPin();
						if (!Next || Next->LinkedTo.Num() == 0) { T = nullptr; break; }
						T = Next->LinkedTo[0]->GetOwningNode();
					}
					Out.Add({ Pin->PinName, T });
				}
			}
		}
		return Out;
	}

	/** Recursive outline builder. */
	static void BuildOutline(
		const UEdGraphNode* Node,
		int32 Depth,
		TSet<const UEdGraphNode*>& Visited,
		TArray<FString>& OutLines)
	{
		if (!Node) return;
		if (OutLines.Num() >= MaxOutlineLines) return;
		if (Depth > MaxOutlineDepth) return;
		if (Visited.Contains(Node)) return;
		Visited.Add(Node);

		const FString Line = DescribeNodeOneLine(Node);
		if (!Line.IsEmpty())
		{
			FString Indent;
			for (int32 i = 0; i < Depth; ++i) Indent += TEXT("  ");
			OutLines.Add(Indent + Line);
		}

		// Stop recursion at Return — nothing follows.
		if (Cast<UK2Node_FunctionResult>(Node)) return;

		const TArray<FExecOut> Outputs = GetExecOutputs(Node);
		if (Outputs.Num() == 0) return;
		// Single linear path: recurse without extra indent.
		if (Outputs.Num() == 1)
		{
			BuildOutline(Outputs[0].Target, Depth, Visited, OutLines);
			return;
		}
		// Multiple outputs: label each and recurse at +1 indent.
		for (const FExecOut& Out : Outputs)
		{
			if (OutLines.Num() >= MaxOutlineLines) return;
			FString Label = Out.PinName.ToString();
			// Prettify common pin names.
			if (Label.Equals(TEXT("then"), ESearchCase::IgnoreCase))         Label = TEXT("True");
			else if (Label.Equals(TEXT("else"), ESearchCase::IgnoreCase))    Label = TEXT("False");
			FString Indent;
			for (int32 i = 0; i <= Depth; ++i) Indent += TEXT("  ");
			OutLines.Add(Indent + Label + TEXT(" →"));
			BuildOutline(Out.Target, Depth + 2, Visited, OutLines);
		}
	}

	/** Find the entry node in a graph for exec outline purposes. */
	static UEdGraphNode* FindEntry(const UEdGraph* Graph, const FString& FunctionName)
	{
		if (!Graph) return nullptr;
		// Prefer FunctionEntry (present in user function graphs).
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (Cast<UK2Node_FunctionEntry>(N)) return N;
		}
		// For event graphs, look for a specific event by name; else first event.
		UEdGraphNode* FirstEvent = nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_Event* E = Cast<UK2Node_Event>(N))
			{
				if (!FirstEvent) FirstEvent = E;
				if (E->GetFunctionName().ToString() == FunctionName) return E;
			}
			else if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(N))
			{
				if (CE->CustomFunctionName.ToString() == FunctionName) return CE;
			}
		}
		return FirstEvent;
	}
}

bool UUnrealBridgeBlueprintLibrary::GetFunctionSummary(
	const FString& BlueprintPath, const FString& FunctionName,
	FBridgeFunctionSemantics& OutSemantics)
{
	using namespace BridgeBPSummaryImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	OutSemantics.Name = FunctionName;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* Super = GenClass->GetSuperClass();

	// Try to find the UFunction first (normal function or event).
	const UFunction* UFunc = GenClass->FindFunctionByName(FName(*FunctionName));
	if (UFunc)
	{
		// Kind + flags.
		if (UFunc->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent))
		{
			OutSemantics.Kind = (Super && Super->FindFunctionByName(UFunc->GetFName()))
				? TEXT("Override") : TEXT("Event");
			OutSemantics.bIsOverride = (Super && Super->FindFunctionByName(UFunc->GetFName()) != nullptr);
		}
		else
		{
			OutSemantics.Kind = TEXT("Function");
		}
		OutSemantics.bIsPure = UFunc->HasAnyFunctionFlags(FUNC_BlueprintPure);
		OutSemantics.Access  = UFunc->HasAnyFunctionFlags(FUNC_Public)    ? TEXT("Public")
							  : UFunc->HasAnyFunctionFlags(FUNC_Protected) ? TEXT("Protected")
							  : UFunc->HasAnyFunctionFlags(FUNC_Private)   ? TEXT("Private")
							  : TEXT("Public");
		// Tooltip / description.
		OutSemantics.Description = UFunc->GetMetaData(TEXT("ToolTip"));

		// Params.
		for (TFieldIterator<FProperty> PIt(UFunc); PIt; ++PIt)
		{
			const FProperty* Param = *PIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;
			FBridgeFunctionParam P;
			P.Name = Param->GetName();
			P.Type = PropertyTypeToString(Param);
			P.bIsOutput = Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
			OutSemantics.Params.Add(P);
		}
	}

	// Locate graph (function / event / macro / ubergraph entry).
	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);
	// If FindGraphs returned nothing but the name matches an event, fall back
	// to searching UbergraphPages for an event node by that name.
	if (Graphs.Num() == 0)
	{
		for (UEdGraph* G : BP->UbergraphPages)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_Event* E = Cast<UK2Node_Event>(N))
				{
					if (E->GetFunctionName().ToString() == FunctionName)
					{
						Graphs.AddUnique(G);
					}
				}
			}
		}
	}
	if (Graphs.Num() == 0)
	{
		// Macro?
		for (UEdGraph* G : BP->MacroGraphs)
		{
			if (G && G->GetName() == FunctionName) { Graphs.Add(G); OutSemantics.Kind = TEXT("Macro"); break; }
		}
	}
	if (Graphs.Num() == 0) return false;

	if (OutSemantics.Kind.IsEmpty())
	{
		OutSemantics.Kind = ClassifyGraph(BP, Graphs[0]);
	}

	// Aggregates + outline across the matching graph(s).
	TSet<FString> ReadsSet, WritesSet, CallsSet, FiresSet, SpawnsSet;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			OutSemantics.NodeCount += 1;

			if (const UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(Node)) ReadsSet.Add(VG->GetVarNameString());
			else if (const UK2Node_VariableSet* VS = Cast<UK2Node_VariableSet>(Node)) WritesSet.Add(VS->GetVarNameString());
			else if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				UFunction* TF = Call->GetTargetFunction();
				const FString Name = TF ? TF->GetName() : Call->GetFunctionName().ToString();
				const FString Own  = (TF && TF->GetOwnerClass()) ? TF->GetOwnerClass()->GetName() : TEXT("?");
				CallsSet.Add(FString::Printf(TEXT("%s.%s"), *Own, *Name));
			}
			else if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node)) FiresSet.Add(CD->GetPropertyName().ToString());
			else if (const UK2Node_SpawnActorFromClass* SP = Cast<UK2Node_SpawnActorFromClass>(Node))
			{
				if (const UEdGraphPin* CP = SP->GetClassPin())
				{
					if (CP->DefaultObject) SpawnsSet.Add(CP->DefaultObject->GetName());
				}
			}
			else if (const UK2Node_IfThenElse* /*Branch*/ _ = Cast<UK2Node_IfThenElse>(Node))
			{
				OutSemantics.bHasBranches = true;
			}
			else if (const UK2Node_MacroInstance* Mac = Cast<UK2Node_MacroInstance>(Node))
			{
				if (IsLoopMacro(Mac)) OutSemantics.bHasLoops = true;
			}
			else if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				if (!Comment->NodeComment.IsEmpty())
					OutSemantics.CommentBlocks.Add(Comment->NodeComment);
			}
		}
	}
	OutSemantics.ReadsVariables    = ReadsSet.Array();  OutSemantics.ReadsVariables.Sort();
	OutSemantics.WritesVariables   = WritesSet.Array(); OutSemantics.WritesVariables.Sort();
	OutSemantics.CallsFunctions    = CallsSet.Array();  OutSemantics.CallsFunctions.Sort();
	OutSemantics.FiresDispatchers  = FiresSet.Array();  OutSemantics.FiresDispatchers.Sort();
	OutSemantics.SpawnsClasses     = SpawnsSet.Array(); OutSemantics.SpawnsClasses.Sort();

	// Outline: start from the entry node of the first matching graph.
	UEdGraphNode* Entry = FindEntry(Graphs[0], FunctionName);
	if (Entry)
	{
		TSet<const UEdGraphNode*> Visited;
		BuildOutline(Entry, 0, Visited, OutSemantics.ExecOutline);
	}

	return true;
}

// ─── Find* cross-reference queries ──────────────────────────────

namespace BridgeBPSummaryImpl
{
	static FBridgeReference MakeRefFromNode(
		const UEdGraph* Graph, const FString& GraphType,
		const UEdGraphNode* Node, const FString& Kind)
	{
		FBridgeReference Ref;
		Ref.GraphName = Graph ? Graph->GetName() : FString();
		Ref.GraphType = GraphType;
		Ref.NodeGuid  = Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
		Ref.NodeTitle = Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
		Ref.Kind      = Kind;
		return Ref;
	}
}

TArray<FBridgeReference> UUnrealBridgeBlueprintLibrary::FindVariableReferences(
	const FString& BlueprintPath, const FString& VariableName)
{
	using namespace BridgeBPSummaryImpl;
	TArray<FBridgeReference> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || VariableName.IsEmpty()) return Result;

	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;
			if (const UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(Node))
			{
				if (VG->GetVarNameString() == VariableName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("read")));
				}
			}
			else if (const UK2Node_VariableSet* VS = Cast<UK2Node_VariableSet>(Node))
			{
				if (VS->GetVarNameString() == VariableName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("write")));
				}
			}
		}
	}
	return Result;
}

TArray<FBridgeReference> UUnrealBridgeBlueprintLibrary::FindFunctionCallSites(
	const FString& BlueprintPath, const FString& FunctionName)
{
	using namespace BridgeBPSummaryImpl;
	TArray<FBridgeReference> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || FunctionName.IsEmpty()) return Result;

	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				UFunction* TF = Call->GetTargetFunction();
				const FString Name = TF ? TF->GetName() : Call->GetFunctionName().ToString();
				if (Name == FunctionName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
			else if (const UK2Node_Message* Msg = Cast<UK2Node_Message>(Node))
			{
				// Interface messages: match by message function name too.
				if (Msg->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(FunctionName))
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
			else if (const UK2Node_MacroInstance* Mac = Cast<UK2Node_MacroInstance>(Node))
			{
				UEdGraph* MG = Mac->GetMacroGraph();
				if (MG && MG->GetName() == FunctionName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
		}
	}
	return Result;
}

// ─── Pin introspection helpers ─────────────────────────────────

namespace BridgeBPSummaryImpl
{
	/** Locate a node inside a graph by its digits-form guid. */
	static UEdGraphNode* FindNodeInGraphByGuid(UEdGraph* Graph, const FString& NodeGuid)
	{
		if (!Graph) return nullptr;
		FGuid Target;
		if (!FGuid::ParseExact(NodeGuid, EGuidFormats::Digits, Target)) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid == Target) return N;
		}
		return nullptr;
	}

	/** Search all of a BP's graphs (ubergraph + functions + macros) by name. */
	static UEdGraph* FindSingleGraphByName(UBlueprint* BP, const FString& Name)
	{
		if (!BP) return nullptr;
		auto Probe = [&Name](const TArray<UEdGraph*>& Arr) -> UEdGraph*
		{
			for (UEdGraph* G : Arr) if (G && G->GetName() == Name) return G;
			return nullptr;
		};
		if (UEdGraph* G = Probe(BP->FunctionGraphs)) return G;
		if (UEdGraph* G = Probe(BP->UbergraphPages)) return G;
		if (UEdGraph* G = Probe(BP->MacroGraphs))    return G;
		return nullptr;
	}

	/** Short human-readable type for a pin from its FEdGraphPinType. */
	static FString PinTypeToHuman(const FEdGraphPinType& PT)
	{
		auto BaseType = [&]() -> FString
		{
			const FName Cat = PT.PinCategory;
			if (Cat == UEdGraphSchema_K2::PC_Exec)      return TEXT("Exec");
			if (Cat == UEdGraphSchema_K2::PC_Boolean)   return TEXT("Bool");
			if (Cat == UEdGraphSchema_K2::PC_Byte)
			{
				if (UEnum* E = Cast<UEnum>(PT.PinSubCategoryObject.Get()))
				{
					return FString::Printf(TEXT("Enum<%s>"), *E->GetName());
				}
				return TEXT("Byte");
			}
			if (Cat == UEdGraphSchema_K2::PC_Int)       return TEXT("Int");
			if (Cat == UEdGraphSchema_K2::PC_Int64)     return TEXT("Int64");
			if (Cat == UEdGraphSchema_K2::PC_Float)     return TEXT("Float");
			if (Cat == UEdGraphSchema_K2::PC_Double)    return TEXT("Double");
			if (Cat == UEdGraphSchema_K2::PC_Real)
			{
				if (PT.PinSubCategory == UEdGraphSchema_K2::PC_Float) return TEXT("Float");
				if (PT.PinSubCategory == UEdGraphSchema_K2::PC_Double) return TEXT("Double");
				return TEXT("Real");
			}
			if (Cat == UEdGraphSchema_K2::PC_String)    return TEXT("String");
			if (Cat == UEdGraphSchema_K2::PC_Name)      return TEXT("Name");
			if (Cat == UEdGraphSchema_K2::PC_Text)      return TEXT("Text");
			if (Cat == UEdGraphSchema_K2::PC_Object ||
				Cat == UEdGraphSchema_K2::PC_Interface)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("%s"), *C->GetName());
				return TEXT("Object");
			}
			if (Cat == UEdGraphSchema_K2::PC_SoftObject)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("SoftObject<%s>"), *C->GetName());
				return TEXT("SoftObject");
			}
			if (Cat == UEdGraphSchema_K2::PC_Class)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("Class<%s>"), *C->GetName());
				return TEXT("Class");
			}
			if (Cat == UEdGraphSchema_K2::PC_SoftClass)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("SoftClass<%s>"), *C->GetName());
				return TEXT("SoftClass");
			}
			if (Cat == UEdGraphSchema_K2::PC_Struct)
			{
				if (UScriptStruct* S = Cast<UScriptStruct>(PT.PinSubCategoryObject.Get()))
					return S->GetName();
				return TEXT("Struct");
			}
			if (Cat == UEdGraphSchema_K2::PC_Enum)
			{
				if (UEnum* E = Cast<UEnum>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("Enum<%s>"), *E->GetName());
				return TEXT("Enum");
			}
			if (Cat == UEdGraphSchema_K2::PC_Delegate)  return TEXT("Delegate");
			if (Cat == UEdGraphSchema_K2::PC_MCDelegate) return TEXT("MulticastDelegate");
			if (Cat == UEdGraphSchema_K2::PC_Wildcard)  return TEXT("Wildcard");
			return Cat.ToString();
		}();
		// Containers.
		if (PT.ContainerType == EPinContainerType::Array) return FString::Printf(TEXT("Array of %s"), *BaseType);
		if (PT.ContainerType == EPinContainerType::Set)   return FString::Printf(TEXT("Set of %s"),   *BaseType);
		if (PT.ContainerType == EPinContainerType::Map)
		{
			// Recurse on a non-container proxy of the value-side terminal so
			// the V-side runs through the full BaseType matrix (handles
			// PC_Real → Float/Double, PC_Struct, PC_Enum, PC_Class, etc.)
			// instead of a hand-maintained whitelist.
			FEdGraphPinType ValueProxy;
			ValueProxy.PinCategory          = PT.PinValueType.TerminalCategory;
			ValueProxy.PinSubCategory       = PT.PinValueType.TerminalSubCategory;
			ValueProxy.PinSubCategoryObject = PT.PinValueType.TerminalSubCategoryObject;
			ValueProxy.ContainerType        = EPinContainerType::None;
			return FString::Printf(TEXT("Map<%s, %s>"), *BaseType, *PinTypeToHuman(ValueProxy));
		}
		return BaseType;
	}

	/** Extract a pin's effective default as a string. */
	static FString GetPinDefaultString(const UEdGraphPin* Pin)
	{
		if (!Pin) return FString();
		if (Pin->DefaultObject) return Pin->DefaultObject->GetPathName();
		if (!Pin->DefaultTextValue.IsEmpty()) return Pin->DefaultTextValue.ToString();
		return Pin->DefaultValue;
	}
}

FString UUnrealBridgeBlueprintLibrary::GetPinDefaultValue(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName)
{
	using namespace BridgeBPSummaryImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return FString();
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return FString();
	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin) return FString();
	return GetPinDefaultString(Pin);
}

// ─── Node layout ───────────────────────────────────────────────

namespace BridgeBPSummaryImpl
{
	// Rough UE K2 node layout constants — match typical rendered sizes
	// within ±20 px, enough for relative adjacency placement.
	constexpr int32 HeaderHeight  = 40;  // title bar above pins
	constexpr int32 PinRowHeight  = 22;
	constexpr int32 FooterHeight  = 12;
	constexpr int32 MinNodeWidth  = 180;
	constexpr int32 MinNodeHeight = 60;
	constexpr int32 MaxNodeWidth  = 520;
	constexpr int32 PinInsetX     = 10;

	struct FVisiblePinTally
	{
		int32 InputCount = 0;
		int32 OutputCount = 0;
		int32 LongestLabel = 0;
	};

	static FVisiblePinTally TallyVisiblePins(const UEdGraphNode* Node)
	{
		FVisiblePinTally Out;
		if (!Node) return Out;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bHidden) continue;
			if (Pin->Direction == EGPD_Input)  Out.InputCount  += 1;
			else                                Out.OutputCount += 1;
			const int32 Len = Pin->GetDisplayName().ToString().Len();
			if (Len > Out.LongestLabel) Out.LongestLabel = Len;
		}
		return Out;
	}

	static void EstimateNodeSize(const UEdGraphNode* Node, int32& W, int32& H)
	{
		if (!Node) { W = MinNodeWidth; H = MinNodeHeight; return; }
		const FVisiblePinTally Tally = TallyVisiblePins(Node);
		const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		// Char-width heuristic: ~7 px per char in default font. Title bar
		// + longest pin label on either side plus padding drives width.
		const int32 TitleWidth = Title.Len() * 7 + 40;
		const int32 PinWidth   = Tally.LongestLabel * 7 + 80;  // both sides get padding
		W = FMath::Clamp(FMath::Max(TitleWidth, PinWidth), MinNodeWidth, MaxNodeWidth);
		// Height: header + max-rows per side * row height + footer.
		const int32 RowCount = FMath::Max(Tally.InputCount, Tally.OutputCount);
		H = FMath::Max(MinNodeHeight, HeaderHeight + RowCount * PinRowHeight + FooterHeight);
	}

	/** True if the node renders as a compact K2 node (e.g. `>=`, `OR`, `+`).
	 *  Compact nodes have no standalone title bar and lay out their pins
	 *  differently from standard nodes — specifically, the direction with
	 *  fewer pins is vertically centered inside the node body. */
	static bool IsCompactK2Node(const UEdGraphNode* Node)
	{
		const UK2Node* K2 = Cast<const UK2Node>(Node);
		return K2 && K2->ShouldDrawCompact();
	}

	/** Local Y offset (relative to NodePosY) of a visible pin. For standard
	 *  nodes this is the straightforward `HeaderHeight + DirIdx * PinRowHeight`.
	 *  For compact nodes: the minority-side pins are centered in the node
	 *  body, so e.g. a compact `>=` with 2 inputs + 1 output puts the output
	 *  at (InputA.Y + InputB.Y) / 2 rather than at InputA.Y. Matches what
	 *  UE renders visually — critical for pin-aligned layout to actually
	 *  align in the editor.
	 *
	 *  @param DirIdx  Zero-based index of this pin among visible pins of its
	 *                 own direction (input or output), same convention as
	 *                 BridgeBPPinAlignedImpl::PinDirIndex. */
	static int32 ComputePinLocalY(const UEdGraphNode* Node,
		EEdGraphPinDirection Dir, int32 DirIdx)
	{
		int32 Y = HeaderHeight + DirIdx * PinRowHeight;
		if (IsCompactK2Node(Node))
		{
			const FVisiblePinTally Tally = TallyVisiblePins(Node);
			const int32 MyCount    = (Dir == EGPD_Input) ? Tally.InputCount : Tally.OutputCount;
			const int32 OtherCount = (Dir == EGPD_Input) ? Tally.OutputCount : Tally.InputCount;
			const int32 MaxCount   = FMath::Max(MyCount, OtherCount);
			if (MyCount > 0 && MyCount < MaxCount)
			{
				// Center this direction's pin column inside the taller one:
				// shift down by (MaxCount - MyCount) * PinRowHeight / 2.
				Y += (MaxCount - MyCount) * PinRowHeight / 2;
			}
		}
		return Y;
	}
}

FBridgeNodeLayout UUnrealBridgeBlueprintLibrary::GetNodeLayout(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace BridgeBPSummaryImpl;
	FBridgeNodeLayout Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Out;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Out;

	Out.PosX         = Node->NodePosX;
	Out.PosY         = Node->NodePosY;
	Out.StoredWidth  = Node->NodeWidth;
	Out.StoredHeight = Node->NodeHeight;

	Out.bIsCommentBox = Node->IsA<UEdGraphNode_Comment>();

	int32 EstW = 0, EstH = 0;
	EstimateNodeSize(Node, EstW, EstH);
	Out.EstimatedWidth  = EstW;
	Out.EstimatedHeight = EstH;

	// Use stored if sane (>0). Comments always have authored dims.
	const bool bStoredValid = (Out.StoredWidth > 0 && Out.StoredHeight > 0);
	Out.bSizeIsAuthoritative = bStoredValid || Out.bIsCommentBox;
	Out.EffectiveWidth  = bStoredValid ? Out.StoredWidth  : Out.EstimatedWidth;
	Out.EffectiveHeight = bStoredValid ? Out.StoredHeight : Out.EstimatedHeight;

	const float X = static_cast<float>(Out.PosX);
	const float Y = static_cast<float>(Out.PosY);
	const float W = static_cast<float>(Out.EffectiveWidth);
	const float H = static_cast<float>(Out.EffectiveHeight);
	Out.TopLeft     = FVector2D(X,     Y);
	Out.TopRight    = FVector2D(X + W, Y);
	Out.BottomLeft  = FVector2D(X,     Y + H);
	Out.BottomRight = FVector2D(X + W, Y + H);
	Out.Center      = FVector2D(X + W * 0.5f, Y + H * 0.5f);
	return Out;
}

TArray<FBridgePinLayout> UUnrealBridgeBlueprintLibrary::GetNodePinLayouts(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace BridgeBPSummaryImpl;
	TArray<FBridgePinLayout> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Out;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Out;

	// Layout derived from EffectiveWidth for correct right-edge position.
	int32 EstW = 0, EstH = 0;
	EstimateNodeSize(Node, EstW, EstH);
	const int32 EffWidth = (Node->NodeWidth > 0) ? Node->NodeWidth : EstW;

	int32 InputIdx = 0;
	int32 OutputIdx = 0;
	const float Ox = static_cast<float>(Node->NodePosX);
	const float Oy = static_cast<float>(Node->NodePosY);

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		FBridgePinLayout L;
		L.Name      = Pin->PinName.ToString();
		L.Direction = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
		L.bIsExec   = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		L.bIsHidden = Pin->bHidden;
		L.bIsEstimated = true;

		// Hidden pins keep index -1 and local offset = (0, 0) — they don't
		// occupy a visible row. Still surfaced so callers see the full list.
		if (Pin->bHidden)
		{
			L.DirectionIndex = -1;
			L.LocalOffset    = FVector2D::ZeroVector;
			L.Position       = FVector2D(Ox, Oy);
			Out.Add(L);
			continue;
		}

		const int32 DirIdx = (Pin->Direction == EGPD_Input) ? InputIdx++ : OutputIdx++;
		L.DirectionIndex = DirIdx;
		const float Lx = (Pin->Direction == EGPD_Input)
			? static_cast<float>(PinInsetX)
			: static_cast<float>(EffWidth - PinInsetX);
		const float Ly = static_cast<float>(ComputePinLocalY(Node, Pin->Direction, DirIdx));
		L.LocalOffset = FVector2D(Lx, Ly);
		L.Position    = FVector2D(Ox + Lx, Oy + Ly);
		Out.Add(L);
	}
	return Out;
}

bool UUnrealBridgeBlueprintLibrary::OpenFunctionGraphForRender(
	const FString& BlueprintPath, const FString& GraphName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	// Open the Blueprint editor, then ask its BlueprintEditor instance to
	// open the specific graph tab. Regular BringKismetToFocusAttentionOnObject
	// opens the editor but can leave the tab stale; OpenGraphAndBringToFront
	// forces the graph's SGraphEditor widget to be constructed or focused.
	UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!Sub) return false;
	Sub->OpenEditorForAsset(BP);
	IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, false);
	if (!Inst) return false;
	FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
	TSharedPtr<SGraphEditor> GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
	return GraphEd.IsValid();
}

TArray<FBridgeRenderedNode> UUnrealBridgeBlueprintLibrary::GetRenderedNodeInfo(
	const FString& BlueprintPath, const FString& GraphName)
{
	TArray<FBridgeRenderedNode> Out;

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return Out;

	UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!Sub) return Out;
	IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, /*bFocusIfOpen=*/false);
	if (!Inst) return Out;
	FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);

	// Resolve the SGraphEditor for the requested graph. If it happens to be
	// the currently-focused graph, use it directly; otherwise ask the
	// BlueprintEditor to open it — returns the same widget if already open.
	TSharedPtr<SGraphEditor> GraphEd;
	if (BPEd->GetFocusedGraph() == Graph)
	{
		// Access via editor's focused pointer is internal; simpler to just
		// call OpenGraphAndBringToFront which returns the live widget and is
		// idempotent for an already-open graph.
		GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
	}
	else
	{
		GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
	}
	if (!GraphEd.IsValid()) return Out;

	SGraphPanel* Panel = GraphEd->GetGraphPanel();
	if (!Panel) return Out;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;
		FBridgeRenderedNode R;
		R.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		R.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		R.GraphPosition = FVector2D(Node->NodePosX, Node->NodePosY);

		TSharedPtr<SGraphNode> NodeWidget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid);
		if (NodeWidget.IsValid())
		{
			// Desired size = what Slate plans to render the node at; this is
			// the authoritative pre-layout geometry. Cached geometry (post-
			// layout) would be equivalent once the panel has ticked, but
			// desired size is always valid after widget construction.
			const FVector2D Desired = FVector2D(NodeWidget->GetDesiredSize());
			R.Size = Desired;
			R.bIsLive = true;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				FBridgeRenderedPin P;
				P.Name       = Pin->PinName.ToString();
				P.Direction  = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
				P.bIsExec    = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				P.bIsHidden  = Pin->bHidden;

				// Compute direction-index (visible pins only) to match the
				// layout helpers' convention.
				int32 DirIdx = 0;
				for (UEdGraphPin* Q : Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != Pin->Direction) continue;
					if (Q == Pin) break;
					DirIdx += 1;
				}
				P.DirectionIndex = DirIdx;

				TSharedPtr<SGraphPin> PinWidget = NodeWidget->FindWidgetForPin(Pin);
				if (PinWidget.IsValid())
				{
					const FVector2D NodeOff = FVector2D(PinWidget->GetNodeOffset());
					P.NodeOffset    = NodeOff;
					P.GraphPosition = R.GraphPosition + NodeOff;
				}
				R.Pins.Add(P);
			}
		}
		Out.Add(R);
	}
	return Out;
}

// ─── Pre-spawn size prediction + graph auto-layout ─────────────

namespace BridgeBPLayoutImpl
{
	using namespace BridgeBPSummaryImpl;

	/** Apply the shared size formula once pin counts + title are known. */
	static void ApplySizeFormula(
		int32 InputCount, int32 OutputCount,
		int32 LongestLabelLen, const FString& Title,
		int32& OutW, int32& OutH)
	{
		const int32 TitleWidth = Title.Len() * 7 + 40;
		const int32 PinWidth   = LongestLabelLen * 7 + 80;
		OutW = FMath::Clamp(FMath::Max(TitleWidth, PinWidth), MinNodeWidth, MaxNodeWidth);
		const int32 RowCount = FMath::Max(InputCount, OutputCount);
		OutH = FMath::Max(MinNodeHeight, HeaderHeight + RowCount * PinRowHeight + FooterHeight);
	}

	/** Count parameters on a UFunction as they'd appear on a K2Node_CallFunction. */
	static void TallyFunctionParams(
		const UFunction* Fn, int32& InPins, int32& OutPins, int32& LongestLabel)
	{
		if (!Fn) return;
		const bool bPure = Fn->HasAnyFunctionFlags(FUNC_BlueprintPure);
		if (!bPure)
		{
			InPins  += 1;  // exec in
			OutPins += 1;  // exec out (then)
		}
		if (!Fn->HasAnyFunctionFlags(FUNC_Static))
		{
			InPins += 1;  // self
			if (4 > LongestLabel) LongestLabel = 4;
		}
		for (TFieldIterator<FProperty> It(Fn); It; ++It)
		{
			const FProperty* P = *It;
			if (!P || !P->HasAnyPropertyFlags(CPF_Parm)) continue;
			const bool bReturn = P->HasAnyPropertyFlags(CPF_ReturnParm);
			const bool bOut    = P->HasAnyPropertyFlags(CPF_OutParm)
			                  && !P->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bReturn || bOut) OutPins += 1;
			else                 InPins  += 1;
			const int32 Len = P->GetName().Len();
			if (Len > LongestLabel) LongestLabel = Len;
		}
	}

	/** Resolve a variable's name length for a get/set node title. */
	static int32 VariableTitleLen(UBlueprint* BP, const FString& VarName)
	{
		// K2Node_VariableGet / Set titles are just the variable name.
		return VarName.Len();
	}

	/** Width-of-member labels heuristic for a struct by path. */
	static void TallyStructMembers(
		const FString& StructPath, int32& MemberCount, int32& LongestLabel)
	{
		if (StructPath.IsEmpty()) return;
		UScriptStruct* S = FindObject<UScriptStruct>(nullptr, *StructPath);
		if (!S) S = LoadObject<UScriptStruct>(nullptr, *StructPath);
		if (!S) return;
		for (TFieldIterator<FProperty> It(S); It; ++It)
		{
			const FProperty* P = *It;
			if (!P) continue;
			MemberCount += 1;
			const int32 Len = P->GetName().Len();
			if (Len > LongestLabel) LongestLabel = Len;
		}
	}
}

FBridgeNodeSizeEstimate UUnrealBridgeBlueprintLibrary::PredictNodeSize(
	const FString& Kind, const FString& ParamA,
	const FString& ParamB, int32 ParamInt)
{
	using namespace BridgeBPLayoutImpl;
	using namespace BridgeBPSummaryImpl;

	FBridgeNodeSizeEstimate Out;
	Out.Kind = Kind;
	Out.Width = MinNodeWidth;
	Out.Height = MinNodeHeight;

	auto Finish = [&](int32 InCount, int32 OutCount, int32 LongestLabel, const FString& Title)
	{
		int32 W = MinNodeWidth, H = MinNodeHeight;
		ApplySizeFormula(InCount, OutCount, LongestLabel, Title, W, H);
		Out.Width = W;
		Out.Height = H;
		Out.InputPinCount = InCount;
		Out.OutputPinCount = OutCount;
		Out.bResolved = true;
	};

	const int32 ClampedInt = FMath::Max(0, ParamInt);
	const FString LowerKind = Kind.ToLower();

	if (LowerKind == TEXT("function_call") || LowerKind == TEXT("event"))
	{
		UClass* Target = nullptr;
		if (!ParamA.IsEmpty())
		{
			Target = FindObject<UClass>(nullptr, *ParamA);
			if (!Target) Target = LoadObject<UClass>(nullptr, *ParamA);
		}
		UFunction* Fn = Target ? Target->FindFunctionByName(FName(*ParamB)) : nullptr;
		if (!Fn)
		{
			Out.Notes = TEXT("function not found — fallback default");
			Out.Width = 220; Out.Height = 80;
			return Out;
		}
		int32 In = 0, OutP = 0, LongestLabel = ParamB.Len();
		TallyFunctionParams(Fn, In, OutP, LongestLabel);
		if (LowerKind == TEXT("event"))
		{
			// Event entry nodes: no exec-in, only exec-out + data outs for params.
			In = 0;
			OutP = 1;  // exec out
			LongestLabel = ParamB.Len();
			for (TFieldIterator<FProperty> It(Fn); It; ++It)
			{
				const FProperty* P = *It;
				if (!P || !P->HasAnyPropertyFlags(CPF_Parm)) continue;
				if (P->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
				OutP += 1;
				const int32 L = P->GetName().Len();
				if (L > LongestLabel) LongestLabel = L;
			}
		}
		Finish(In, OutP, LongestLabel, ParamB);
		return Out;
	}

	if (LowerKind == TEXT("variable_get"))
	{
		const int32 Len = VariableTitleLen(nullptr, ParamB);
		Finish(/*in*/0, /*out*/1, Len, ParamB);
		return Out;
	}
	if (LowerKind == TEXT("variable_set"))
	{
		const int32 Len = VariableTitleLen(nullptr, ParamB);
		Finish(/*in*/2, /*out*/1, Len, FString::Printf(TEXT("SET %s"), *ParamB));
		return Out;
	}
	if (LowerKind == TEXT("custom_event"))
	{
		Finish(/*in*/0, /*out*/1 + ClampedInt, 8, ParamA.IsEmpty() ? TEXT("CustomEvent") : ParamA);
		return Out;
	}
	if (LowerKind == TEXT("branch"))
	{
		Finish(2, 2, 5, TEXT("Branch"));
		return Out;
	}
	if (LowerKind == TEXT("sequence"))
	{
		const int32 ThenCount = FMath::Max(2, ClampedInt);
		Finish(1, ThenCount, 5, TEXT("Sequence"));
		return Out;
	}
	if (LowerKind == TEXT("cast"))
	{
		FString Title = TEXT("Cast To ");
		if (!ParamA.IsEmpty())
		{
			// Use last path segment for brevity.
			int32 Dot = INDEX_NONE;
			ParamA.FindLastChar(TEXT('.'), Dot);
			Title += (Dot != INDEX_NONE ? ParamA.RightChop(Dot + 1) : ParamA);
		}
		Finish(2, 3, 8, Title);
		return Out;
	}
	if (LowerKind == TEXT("self"))       { Finish(0, 1, 4, TEXT("Self")); return Out; }
	if (LowerKind == TEXT("reroute"))
	{
		// Reroute renders as a small diamond in the editor, but NodeWidth/Height
		// follow the shared estimate formula (min-size for a 1-in/1-out node).
		// Match what GetNodeLayout's estimated path returns so Predict+Actual agree.
		Finish(1, 1, 0, TEXT(""));
		return Out;
	}
	if (LowerKind == TEXT("delay"))      { Finish(2, 1, 10, TEXT("Delay")); return Out; }
	if (LowerKind == TEXT("foreach"))    { Finish(3, 4, 14, TEXT("ForEachLoop")); return Out; }
	if (LowerKind == TEXT("forloop"))    { Finish(3, 3, 10, TEXT("ForLoop")); return Out; }
	if (LowerKind == TEXT("whileloop"))  { Finish(2, 2, 10, TEXT("WhileLoop")); return Out; }
	if (LowerKind == TEXT("select"))
	{
		const int32 Opts = FMath::Max(2, ClampedInt);
		Finish(1 + Opts, 1, 8, TEXT("Select"));
		return Out;
	}
	if (LowerKind == TEXT("make_array"))
	{
		const int32 Elems = FMath::Max(1, ClampedInt);
		Finish(Elems, 1, 6, TEXT("Make Array"));
		return Out;
	}
	if (LowerKind == TEXT("make_struct") || LowerKind == TEXT("break_struct"))
	{
		int32 Members = 0, Longest = 0;
		TallyStructMembers(ParamA, Members, Longest);
		if (Members == 0) { Out.Notes = TEXT("struct not found — fallback"); Members = 3; Longest = 8; }
		FString TitlePrefix = (LowerKind == TEXT("make_struct")) ? TEXT("Make ") : TEXT("Break ");
		int32 Dot = INDEX_NONE;
		ParamA.FindLastChar(TEXT('.'), Dot);
		FString ShortName = (Dot != INDEX_NONE) ? ParamA.RightChop(Dot + 1) : ParamA;
		if (LowerKind == TEXT("make_struct"))  Finish(Members, 1, Longest, TitlePrefix + ShortName);
		else                                    Finish(1, Members, Longest, TitlePrefix + ShortName);
		return Out;
	}
	if (LowerKind == TEXT("enum_literal"))
	{
		int32 Dot = INDEX_NONE;
		ParamA.FindLastChar(TEXT('.'), Dot);
		FString Short = (Dot != INDEX_NONE) ? ParamA.RightChop(Dot + 1) : ParamA;
		Finish(0, 1, Short.Len(), Short);
		return Out;
	}
	if (LowerKind == TEXT("make_literal"))
	{
		Finish(0, 1, ParamA.Len(), FString::Printf(TEXT("Literal %s"), *ParamA));
		return Out;
	}
	if (LowerKind == TEXT("spawn_actor"))
	{
		int32 Dot = INDEX_NONE;
		ParamA.FindLastChar(TEXT('.'), Dot);
		FString Short = (Dot != INDEX_NONE) ? ParamA.RightChop(Dot + 1) : ParamA;
		// SpawnActorFromClass: exec in, class, transform, collision, owner; out exec + return.
		Finish(5, 2, 16, FString::Printf(TEXT("SpawnActor %s"), *Short));
		return Out;
	}
	if (LowerKind == TEXT("dispatcher_call"))
	{
		Finish(2, 1, ParamB.Len(), FString::Printf(TEXT("Call %s"), *ParamB));
		return Out;
	}
	if (LowerKind == TEXT("dispatcher_bind"))
	{
		Finish(3, 1, ParamB.Len(), FString::Printf(TEXT("Bind %s"), *ParamB));
		return Out;
	}
	if (LowerKind == TEXT("dispatcher_event"))
	{
		Finish(0, 1, ParamB.Len(), FString::Printf(TEXT("Event %s"), *ParamB));
		return Out;
	}

	Out.Notes = TEXT("unknown kind — fallback default");
	Out.Width = MinNodeWidth;
	Out.Height = MinNodeHeight;
	return Out;
}

// ─── AutoLayoutGraph (Sugiyama-lite) ─────────────────────────

namespace BridgeBPAutoLayoutImpl
{
	using namespace BridgeBPSummaryImpl;

	struct FLayoutNode
	{
		UEdGraphNode* Node = nullptr;
		int32 Width = MinNodeWidth;
		int32 Height = MinNodeHeight;
		int32 Layer = -1;
		int32 OrderInLayer = 0;
		TArray<int32> ExecSuccessors;   // indices into nodes array
		TArray<int32> ExecPredecessors;
		TArray<int32> DataSuccessors;   // for pure-node pull-in
		bool bHasExecPins = false;
	};

	static bool PinIsExec(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	/** Build layout graph. Exec edges drive layering; data edges used for
	 *  pulling pure nodes near their consumers. */
	static void BuildGraph(
		UEdGraph* Graph, TArray<FLayoutNode>& Out,
		TMap<UEdGraphNode*, int32>& IndexOf)
	{
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			FLayoutNode LN;
			LN.Node = N;
			int32 EstW = 0, EstH = 0;
			EstimateNodeSize(N, EstW, EstH);
			LN.Width  = (N->NodeWidth  > 0) ? N->NodeWidth  : EstW;
			LN.Height = (N->NodeHeight > 0) ? N->NodeHeight : EstH;
			// Comment boxes: keep their authored size; they're not laid out.
			IndexOf.Add(N, Out.Num());
			Out.Add(LN);
		}
		for (int32 i = 0; i < Out.Num(); ++i)
		{
			FLayoutNode& LN = Out[i];
			for (UEdGraphPin* Pin : LN.Node->Pins)
			{
				if (!Pin) continue;
				const bool bExec = PinIsExec(Pin);
				if (bExec) LN.bHasExecPins = true;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Other = Linked->GetOwningNode();
					if (!Other) continue;
					const int32* J = IndexOf.Find(Other);
					if (!J) continue;
					if (bExec && Pin->Direction == EGPD_Output)
					{
						LN.ExecSuccessors.AddUnique(*J);
						Out[*J].ExecPredecessors.AddUnique(i);
					}
					else if (!bExec && Pin->Direction == EGPD_Input)
					{
						// This node consumes data from Other → Other is a producer;
						// we want Other placed at layer(this) − 1 (handled later).
						Out[*J].DataSuccessors.AddUnique(i);
					}
				}
			}
		}
	}

	/** Assign layers by longest-path from exec sources. Cycles are broken by
	 *  ignoring back-edges (reported as warnings). */
	static int32 AssignLayers(TArray<FLayoutNode>& Nodes, TArray<FString>& Warnings)
	{
		int32 MaxLayer = 0;
		// Kahn-style longest-path: iterate until stable, with a cap.
		const int32 Cap = Nodes.Num() * 8 + 16;
		int32 Iter = 0;
		bool bChanged = true;
		// Seed: nodes with no exec preds get layer 0.
		for (FLayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.ExecPredecessors.Num() == 0)
			{
				LN.Layer = 0;
			}
		}
		while (bChanged && Iter++ < Cap)
		{
			bChanged = false;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				FLayoutNode& LN = Nodes[i];
				if (!LN.bHasExecPins) continue;
				int32 Best = LN.Layer;
				for (int32 P : LN.ExecPredecessors)
				{
					if (Nodes[P].Layer >= 0)
					{
						Best = FMath::Max(Best, Nodes[P].Layer + 1);
					}
				}
				if (Best > LN.Layer)
				{
					LN.Layer = Best;
					bChanged = true;
				}
			}
		}
		if (Iter >= Cap)
		{
			Warnings.Add(TEXT("layer assignment hit iteration cap — possible exec cycle"));
		}
		// Any exec node still at -1 is in a cycle island — stick it at layer 0.
		for (FLayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.Layer < 0) LN.Layer = 0;
		}
		// Pure / data-only nodes: layer = min(consumer.layer) − 1, default 0.
		for (FLayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins) continue;
			int32 MinConsumer = INT32_MAX;
			for (int32 C : LN.DataSuccessors)
			{
				if (Nodes[C].Layer >= 0)
				{
					MinConsumer = FMath::Min(MinConsumer, Nodes[C].Layer);
				}
			}
			LN.Layer = (MinConsumer == INT32_MAX) ? 0 : FMath::Max(0, MinConsumer - 1);
		}
		for (const FLayoutNode& LN : Nodes)
		{
			if (LN.Layer > MaxLayer) MaxLayer = LN.Layer;
		}
		return MaxLayer + 1;
	}

	/** Barycentric ordering within each layer: sort by average Y of the
	 *  predecessors' OrderInLayer. Two passes is usually enough. */
	static void BarycentricOrder(TArray<FLayoutNode>& Nodes, int32 LayerCount)
	{
		TArray<TArray<int32>> Layers;
		Layers.SetNum(LayerCount);
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const int32 L = FMath::Clamp(Nodes[i].Layer, 0, LayerCount - 1);
			Layers[L].Add(i);
		}
		// Seed: stable by original Y so untouched graphs don't scramble.
		for (TArray<int32>& Layer : Layers)
		{
			Layer.Sort([&](int32 A, int32 B) {
				return Nodes[A].Node->NodePosY < Nodes[B].Node->NodePosY;
			});
			for (int32 k = 0; k < Layer.Num(); ++k) Nodes[Layer[k]].OrderInLayer = k;
		}
		// Two sweeps: forward (use predecessors) then back (use successors).
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 L = 1; L < LayerCount; ++L)
			{
				Layers[L].Sort([&](int32 A, int32 B) {
					auto Bary = [&](int32 I)
					{
						const TArray<int32>& Preds = Nodes[I].ExecPredecessors.Num() > 0
							? Nodes[I].ExecPredecessors : Nodes[I].DataSuccessors;
						if (Preds.Num() == 0) return (float)Nodes[I].OrderInLayer;
						float Sum = 0.f;
						for (int32 P : Preds) Sum += Nodes[P].OrderInLayer;
						return Sum / Preds.Num();
					};
					return Bary(A) < Bary(B);
				});
				for (int32 k = 0; k < Layers[L].Num(); ++k) Nodes[Layers[L][k]].OrderInLayer = k;
			}
		}
	}
}

// ─── PinAligned strategy: pin-to-pin exec backbone + data pull ─────

namespace BridgeBPPinAlignedImpl
{
	using namespace BridgeBPSummaryImpl;

	/** Direction-index of a pin among its own side's visible pins. Mirrors the
	 *  convention used by StraightenExecChain / AutoInsertReroutes so pin Y
	 *  estimates stay consistent across tools. Returns -1 if not found. */
	static int32 PinDirIndex(const UEdGraphNode* N, const UEdGraphPin* P)
	{
		if (!N || !P) return -1;
		int32 Idx = 0;
		for (const UEdGraphPin* Q : N->Pins)
		{
			if (!Q || Q->bHidden) continue;
			if (Q->Direction != P->Direction) continue;
			if (Q == P) return Idx;
			Idx += 1;
		}
		return -1;
	}

	/** Ground-truth geometry for a single node, queried from the live Slate
	 *  widget. Populated by BuildRenderedCache when the graph is open in BP
	 *  editor and has ticked at least once. Width/Height come from
	 *  SGraphNode::GetDesiredSize; InputPinLocalYs[i] / OutputPinLocalYs[i]
	 *  come from SGraphPin::GetNodeOffset for the i-th visible input/output
	 *  pin. When the cache has no entry for a node, placement falls back to
	 *  the EstimateNodeSize + ComputePinLocalY formula. */
	struct FRenderedGeom
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<int32> InputPinLocalYs;
		TArray<int32> OutputPinLocalYs;
		TArray<int32> InputPinLocalXs;
		TArray<int32> OutputPinLocalXs;
		bool bValid = false;
	};

	/** Query the live SGraphPanel for each node's desired size and each pin's
	 *  node-offset. Requires the graph to already be open in a Blueprint
	 *  editor AND to have been ticked by Slate at least once (this function
	 *  runs on the game thread, so call `open_function_graph_for_render`
	 *  in a previous exec and wait a moment before calling the layout). */
	static void BuildRenderedCache(UEdGraph* Graph, TMap<UEdGraphNode*, FRenderedGeom>& OutCache)
	{
		if (!Graph || !GEditor) return;
		UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (!Sub) return;
		UBlueprint* OwningBP = Cast<UBlueprint>(Graph->GetOutermostObject());
		// Walk up the outer chain to find the Blueprint (functions sit below UFunction).
		UObject* Outer = Graph->GetOuter();
		while (Outer && !OwningBP)
		{
			OwningBP = Cast<UBlueprint>(Outer);
			Outer = Outer->GetOuter();
		}
		if (!OwningBP) return;
		IAssetEditorInstance* Inst = Sub->FindEditorForAsset(OwningBP, false);
		if (!Inst) return;
		FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
		TSharedPtr<SGraphEditor> GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
		if (!GraphEd.IsValid()) return;
		SGraphPanel* Panel = GraphEd->GetGraphPanel();
		if (!Panel) return;

		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			TSharedPtr<SGraphNode> NW = Panel->GetNodeWidgetFromGuid(N->NodeGuid);
			if (!NW.IsValid()) continue;
			FRenderedGeom G;
			const FVector2D Size = FVector2D(NW->GetDesiredSize());
			G.Width  = int32(Size.X);
			G.Height = int32(Size.Y);
			if (G.Width <= 0 && G.Height <= 0) continue;  // widget not yet ticked

			// Collect visible pins in two parallel arrays — the i-th visible
			// input pin's offset goes at InputPinLocalYs[i].
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				TSharedPtr<SGraphPin> PW = NW->FindWidgetForPin(Pin);
				if (!PW.IsValid()) continue;
				const FVector2D Off = FVector2D(PW->GetNodeOffset());
				if (Pin->Direction == EGPD_Input)
				{
					G.InputPinLocalXs.Add(int32(Off.X));
					G.InputPinLocalYs.Add(int32(Off.Y));
				}
				else
				{
					G.OutputPinLocalXs.Add(int32(Off.X));
					G.OutputPinLocalYs.Add(int32(Off.Y));
				}
			}
			G.bValid = true;
			OutCache.Add(N, G);
		}
	}

	/** Fetch the local-Y of a visible pin (direction-indexed). Uses the
	 *  live-rendered cache if populated for this node, falls back to the
	 *  EstimateNodeSize formula otherwise. */
	static int32 PinLocalYFor(UEdGraphNode* Node, EEdGraphPinDirection Dir, int32 DirIdx,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (const FRenderedGeom* G = Cache.Find(Node))
		{
			if (G->bValid)
			{
				const TArray<int32>& Arr = (Dir == EGPD_Input) ? G->InputPinLocalYs : G->OutputPinLocalYs;
				if (DirIdx >= 0 && DirIdx < Arr.Num()) return Arr[DirIdx];
			}
		}
		return ComputePinLocalY(Node, Dir, DirIdx);
	}

	/** Node width from live cache, else from EstimateNodeSize. */
	static int32 NodeWidthFor(UEdGraphNode* Node,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (const FRenderedGeom* G = Cache.Find(Node))
		{
			if (G->bValid && G->Width > 0) return G->Width;
		}
		int32 W = 0, H = 0;
		EstimateNodeSize(Node, W, H);
		return (Node->NodeWidth > 0) ? Node->NodeWidth : W;
	}

	/** Node height from live cache, else from EstimateNodeSize. */
	static int32 NodeHeightFor(UEdGraphNode* Node,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (const FRenderedGeom* G = Cache.Find(Node))
		{
			if (G->bValid && G->Height > 0) return G->Height;
		}
		int32 W = 0, H = 0;
		EstimateNodeSize(Node, W, H);
		return (Node->NodeHeight > 0) ? Node->NodeHeight : H;
	}

	struct FPALayoutNode
	{
		UEdGraphNode* Node = nullptr;
		int32 Width = MinNodeWidth;
		int32 Height = MinNodeHeight;
		int32 Layer = -1;         // exec layer; -1 until assigned
		int32 NewX = 0;
		int32 NewY = 0;
		bool bHasExecPins = false;
		bool bPlaced = false;

		// Primary exec predecessor (for pin-alignment propagation).
		int32 PrimaryExecPredIdx = INDEX_NONE;
		// Dir-index of the exec-OUT pin on the primary predecessor connecting to us.
		int32 PredExecOutDirIdx = -1;
		// Dir-index of our own exec-IN pin receiving from the primary pred.
		int32 MyExecInDirIdx = -1;

		// Primary exec SUCCESSOR (the .then / first-exec-out path). Used for
		// downstream-driven Y alignment: each node aligns its primary exec-out
		// pin Y to its successor's exec-in pin Y, so the primary flow reads
		// as a clean horizontal rail. The .else branch and other non-primary
		// successors become diagonal (knotted if |ΔY| > threshold).
		int32 PrimaryExecSuccIdx = INDEX_NONE;
		int32 MyExecOutDirIdxToSucc = -1;  // dir-index of my exec-out pin feeding the succ
		int32 SuccExecInDirIdx = -1;       // dir-index of succ's exec-in pin receiving

		// Data-slot assignment (for pure/data nodes only).
		int32 DataConsumerLayer = INT32_MAX;  // exec layer of nearest exec consumer
		int32 DataDepth = INT32_MAX;          // 1 = direct producer, 2 = two hops, ...
		// Primary consumer for Y alignment (any node — exec or pure — whose input
		// pin this node feeds; picked by closest DataConsumerLayer/Depth).
		int32 PrimaryConsumerIdx = INDEX_NONE;
		int32 PrimaryConsumerInDirIdx = -1;  // which of consumer's input pins we feed
		int32 MyOutDirIdxToPrimary = -1;     // which of our output pins feeds it

		TArray<int32> ExecPredecessors;
		TArray<int32> ExecSuccessors;

		// Data-flow: producers feeding this node's input pins.
		// (producer_idx, this_node's_input_pin_dir_idx)
		TArray<TPair<int32, int32>> DataProducers;
	};

	static void BuildGraph(UEdGraph* Graph, TArray<FPALayoutNode>& Out,
		TMap<UEdGraphNode*, int32>& IndexOf,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			// Comment boxes: snapshot but never place (kept at original XY).
			FPALayoutNode LN;
			LN.Node = N;
			LN.Width  = NodeWidthFor(N, Cache);
			LN.Height = NodeHeightFor(N, Cache);
			IndexOf.Add(N, Out.Num());
			Out.Add(LN);
		}

		for (int32 i = 0; i < Out.Num(); ++i)
		{
			FPALayoutNode& LN = Out[i];
			for (UEdGraphPin* Pin : LN.Node->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				const bool bExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				if (bExec) LN.bHasExecPins = true;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Other = Linked->GetOwningNode();
					if (!Other) continue;
					const int32* J = IndexOf.Find(Other);
					if (!J) continue;
					if (bExec)
					{
						// exec-out on us → that's a successor
						if (Pin->Direction == EGPD_Output)
						{
							LN.ExecSuccessors.AddUnique(*J);
							Out[*J].ExecPredecessors.AddUnique(i);
						}
					}
					else
					{
						// data-in on us → Other is a producer
						if (Pin->Direction == EGPD_Input)
						{
							const int32 MyInDirIdx = PinDirIndex(LN.Node, Pin);
							LN.DataProducers.Add(TPair<int32,int32>(*J, MyInDirIdx));
						}
					}
				}
			}
		}
	}

	/** Longest-path layering on exec-only subgraph. */
	static int32 AssignExecLayers(TArray<FPALayoutNode>& Nodes,
		TArray<FString>& Warnings)
	{
		for (FPALayoutNode& LN : Nodes) LN.Layer = -1;

		// Seed: exec sources (have exec pins, no exec predecessors) → layer 0.
		for (FPALayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.ExecPredecessors.Num() == 0) LN.Layer = 0;
		}

		const int32 Cap = Nodes.Num() * 8 + 16;
		int32 Iter = 0;
		bool bChanged = true;
		while (bChanged && Iter++ < Cap)
		{
			bChanged = false;
			for (FPALayoutNode& LN : Nodes)
			{
				if (!LN.bHasExecPins) continue;
				int32 Best = LN.Layer;
				for (int32 P : LN.ExecPredecessors)
				{
					if (Nodes[P].Layer >= 0) Best = FMath::Max(Best, Nodes[P].Layer + 1);
				}
				if (Best > LN.Layer) { LN.Layer = Best; bChanged = true; }
			}
		}
		if (Iter >= Cap)
		{
			Warnings.Add(TEXT("pin_aligned: exec layer assignment hit iter cap — exec cycle suspected"));
		}
		// Unlayered exec nodes (cycle island): park at 0.
		for (FPALayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.Layer < 0) LN.Layer = 0;
		}

		int32 MaxLayer = 0;
		for (const FPALayoutNode& LN : Nodes)
		{
			if (LN.Layer > MaxLayer) MaxLayer = LN.Layer;
		}
		return MaxLayer + 1;
	}

	/** BFS from each exec node outward via DataProducers. Assigns every pure
	 *  node a (ConsumerLayer, Depth) slot; shared pure nodes get the closest
	 *  slot (smallest layer, then smallest depth). Also records each pure
	 *  node's primary consumer and the specific pin pair used for Y alignment.
	 *  Nodes with no exec ancestor (orphan pure chains) keep the sentinels
	 *  and are parked later. */
	static void AssignDataSlots(TArray<FPALayoutNode>& Nodes)
	{
		struct FFrontier { int32 NodeIdx; int32 ConsumerIdx; int32 ConsumerInDirIdx; int32 Depth; };
		TArray<FFrontier> Queue;

		// Seed: every exec node emits one frontier entry per direct data producer.
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& Exec = Nodes[i];
			if (!Exec.bHasExecPins) continue;
			if (Exec.Layer < 0) continue;
			for (const TPair<int32,int32>& Prod : Exec.DataProducers)
			{
				if (Nodes[Prod.Key].bHasExecPins) continue;  // exec→exec handled elsewhere
				Queue.Add({ Prod.Key, i, Prod.Value, 1 });
			}
		}

		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const FFrontier F = Queue[Head++];
			FPALayoutNode& N = Nodes[F.NodeIdx];
			if (N.bHasExecPins) continue;

			const int32 OwningExecLayer = Nodes[F.ConsumerIdx].bHasExecPins
				? Nodes[F.ConsumerIdx].Layer
				: Nodes[F.ConsumerIdx].DataConsumerLayer;
			if (OwningExecLayer == INT32_MAX) continue;

			const bool bBetter = (OwningExecLayer < N.DataConsumerLayer) ||
				(OwningExecLayer == N.DataConsumerLayer && F.Depth < N.DataDepth);
			if (!bBetter && N.PrimaryConsumerIdx != INDEX_NONE) continue;

			N.DataConsumerLayer = OwningExecLayer;
			N.DataDepth         = F.Depth;
			N.PrimaryConsumerIdx        = F.ConsumerIdx;
			N.PrimaryConsumerInDirIdx   = F.ConsumerInDirIdx;

			// Resolve the actual output pin on us that feeds the consumer.
			const FPALayoutNode& Cons = Nodes[F.ConsumerIdx];
			UEdGraphPin* ConsInPin = nullptr;
			{
				int32 DirIdx = 0;
				for (UEdGraphPin* Q : Cons.Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != EGPD_Input) continue;
					if (DirIdx == F.ConsumerInDirIdx) { ConsInPin = Q; break; }
					++DirIdx;
				}
			}
			N.MyOutDirIdxToPrimary = -1;
			if (ConsInPin)
			{
				for (UEdGraphPin* Linked : ConsInPin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode() == N.Node)
					{
						N.MyOutDirIdxToPrimary = PinDirIndex(N.Node, Linked);
						break;
					}
				}
			}

			// Recurse into this node's own data producers (depth+1).
			for (const TPair<int32,int32>& Prod : N.DataProducers)
			{
				if (Nodes[Prod.Key].bHasExecPins) continue;
				Queue.Add({ Prod.Key, F.NodeIdx, Prod.Value, F.Depth + 1 });
			}
		}
	}

	/** Compute per-slot max width and per-slot X position. Slot keys are
	 *  encoded as (ConsumerLayer * LargeStride + Depth) packed into int64
	 *  to stay in a single TMap. Returns the per-exec-layer cumulative data
	 *  budget (sum of slot widths + gaps) so AutoLayoutGraph can space the
	 *  exec columns apart. */
	static int64 MakeSlotKey(int32 ConsumerLayer, int32 Depth)
	{
		return (int64(ConsumerLayer) << 20) | int64(Depth & 0xFFFFF);
	}

	static void ComputeDataSlots(
		const TArray<FPALayoutNode>& Nodes,
		TMap<int64, int32>& OutSlotWidth,
		TMap<int32, int32>& OutLayerMaxDepth)
	{
		for (const FPALayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins) continue;
			if (LN.DataConsumerLayer == INT32_MAX) continue;
			if (LN.DataDepth == INT32_MAX) continue;
			const int64 Key = MakeSlotKey(LN.DataConsumerLayer, LN.DataDepth);
			int32& W = OutSlotWidth.FindOrAdd(Key, 0);
			if (LN.Width > W) W = LN.Width;
			int32& MaxD = OutLayerMaxDepth.FindOrAdd(LN.DataConsumerLayer, 0);
			if (LN.DataDepth > MaxD) MaxD = LN.DataDepth;
		}
	}

	/** Pick the primary exec predecessor for each exec node; record the exact
	 *  pins on both sides for Y alignment. Preference: leftmost layer; tie
	 *  break by lowest original Y (matches "main rail" intuition). */
	static void SelectPrimaryExecPreds(TArray<FPALayoutNode>& Nodes)
	{
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			FPALayoutNode& LN = Nodes[i];
			if (!LN.bHasExecPins || LN.ExecPredecessors.Num() == 0) continue;

			int32 Best = LN.ExecPredecessors[0];
			for (int32 P : LN.ExecPredecessors)
			{
				const FPALayoutNode& Cand = Nodes[P];
				const FPALayoutNode& Cur  = Nodes[Best];
				if (Cand.Layer < Cur.Layer ||
					(Cand.Layer == Cur.Layer && Cand.Node->NodePosY < Cur.Node->NodePosY))
				{
					Best = P;
				}
			}
			LN.PrimaryExecPredIdx = Best;

			// Find exec pin pair (pred.out → me.in).
			const FPALayoutNode& Pred = Nodes[Best];
			bool bResolved = false;
			for (UEdGraphPin* OutPin : Pred.Node->Pins)
			{
				if (!OutPin || OutPin->bHidden) continue;
				if (OutPin->Direction != EGPD_Output) continue;
				if (OutPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				for (UEdGraphPin* Linked : OutPin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode() == LN.Node)
					{
						LN.PredExecOutDirIdx = PinDirIndex(Pred.Node, OutPin);
						LN.MyExecInDirIdx    = PinDirIndex(LN.Node, Linked);
						bResolved = true;
						break;
					}
				}
				if (bResolved) break;
			}
		}
	}

	/** Pick each exec node's primary SUCCESSOR: the node reached through the
	 *  lowest-dir-index exec-output pin (the .then of a Branch, the single
	 *  .then of a SET, the Entry's .then). Downstream Y alignment follows
	 *  this chain so the primary path reads as a horizontal rail. Non-primary
	 *  successors (e.g. Branch.else) stay visually distinct — their wires
	 *  pick up a single reroute knot if they end up diagonal. */
	static void SelectPrimaryExecSuccs(TArray<FPALayoutNode>& Nodes)
	{
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			FPALayoutNode& LN = Nodes[i];
			if (!LN.bHasExecPins) continue;
			// Walk exec-output pins in dir order (PinDirIndex ascending). First
			// one with a link = primary successor. LinkedTo[0] if multi-link.
			UEdGraphPin* OutPin = nullptr;
			int32 OutDirIdx = -1;
			int32 CurDirIdx = 0;
			for (UEdGraphPin* P : LN.Node->Pins)
			{
				if (!P || P->bHidden) continue;
				if (P->Direction != EGPD_Output) continue;
				if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) { continue; }
				if (P->LinkedTo.Num() > 0)
				{
					OutPin = P;
					OutDirIdx = CurDirIdx;
					break;
				}
				// Advance dir counter for exec outputs we've seen.
				// (Non-exec outputs have their own dir-counter; we only care
				// about exec outputs here.)
				++CurDirIdx;
			}
			// Actually we need dir-index across ALL visible output pins, not
			// just exec ones. Re-derive properly:
			if (OutPin)
			{
				OutDirIdx = PinDirIndex(LN.Node, OutPin);
			}
			if (!OutPin || OutDirIdx < 0) continue;

			UEdGraphPin* Linked = OutPin->LinkedTo[0];
			if (!Linked) continue;
			UEdGraphNode* SuccNode = Linked->GetOwningNode();
			if (!SuccNode) continue;

			// Resolve successor's index in our flat node array.
			for (int32 j = 0; j < Nodes.Num(); ++j)
			{
				if (Nodes[j].Node == SuccNode)
				{
					LN.PrimaryExecSuccIdx = j;
					LN.MyExecOutDirIdxToSucc = OutDirIdx;
					LN.SuccExecInDirIdx = PinDirIndex(SuccNode, Linked);
					break;
				}
			}
		}
	}

	/** Layer 0 exec nodes stacked vertically (original-Y order), then each
	 *  later layer placed pin-aligned to its primary predecessor. Within each
	 *  layer, nodes that share a tentative Y are pushed down to clear overlaps. */
	static void PlaceExecBackbone(TArray<FPALayoutNode>& Nodes, int32 LayerCount,
		const TArray<int32>& LayerX, int32 VSpace,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache,
		const TMap<UEdGraphNode*, int32>& IndexOf)
	{
		// DOWNSTREAM-driven Y via DFS of the exec tree:
		//  1. DFS from the entry following exec-out pins in dir order (.then
		//     first, then .else). Leaves (Return / terminal nodes) are
		//     pushed to a flat list in the order we meet them — this is
		//     the natural top-to-bottom order humans draw BPs in.
		//  2. Leaves get stacked top-to-bottom in that DFS order, spanning
		//     layers (crouch returns at layer 5 stack below walk return at
		//     layer 4, etc.).
		//  3. Non-leaves pull Y from their primary successor so the .then
		//     wire reads as a horizontal rail. Processed deepest-first so
		//     successors are placed before predecessors.
		//  4. Collision-resolve per-layer as a safety net.

		TArray<int32> DFSLeaves;
		TSet<int32> Visited;
		TFunction<void(int32)> DFS;
		DFS = [&](int32 Idx)
		{
			if (Visited.Contains(Idx)) return;
			Visited.Add(Idx);
			FPALayoutNode& LN = Nodes[Idx];
			if (!LN.bHasExecPins) return;
			if (LN.ExecSuccessors.Num() == 0)
			{
				DFSLeaves.Add(Idx);
				return;
			}
			// Visit exec-output pins in direction order; each LinkedTo in pin
			// order. For a Branch that's .then (dir 0) first, then .else.
			for (UEdGraphPin* P : LN.Node->Pins)
			{
				if (!P || P->bHidden) continue;
				if (P->Direction != EGPD_Output) continue;
				if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				for (UEdGraphPin* Linked : P->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Succ = Linked->GetOwningNode();
					if (!Succ) continue;
					const int32* J = IndexOf.Find(Succ);
					if (J) DFS(*J);
				}
			}
		};

		// Seed DFS at root exec nodes (layer 0). Multiple roots in original-Y order.
		TArray<int32> Roots;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bHasExecPins && Nodes[i].Layer == 0) Roots.Add(i);
		}
		Roots.Sort([&](int32 A, int32 B) {
			return Nodes[A].Node->NodePosY < Nodes[B].Node->NodePosY;
		});
		for (int32 R : Roots) DFS(R);
		// Handle disconnected exec subgraphs (cycle islands, etc.).
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bHasExecPins && !Visited.Contains(i)) DFS(i);
		}

		// Step 2: stack leaves top-to-bottom in DFS order.
		int32 Cursor = 0;
		for (int32 Idx : DFSLeaves)
		{
			FPALayoutNode& LN = Nodes[Idx];
			LN.NewX = LayerX[FMath::Clamp(LN.Layer, 0, LayerCount - 1)];
			LN.NewY = Cursor;
			LN.bPlaced = true;
			Cursor += LN.Height + VSpace;
		}

		// Step 3: place non-leaves from deepest layer back to root, each
		// aligned to its primary exec successor's input pin Y so the .then
		// wire is horizontal.
		for (int32 L = LayerCount - 1; L >= 0; --L)
		{
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				FPALayoutNode& LN = Nodes[i];
				if (!LN.bHasExecPins) continue;
				if (LN.Layer != L) continue;
				if (LN.bPlaced) continue;
				LN.NewX = LayerX[L];
				if (LN.PrimaryExecSuccIdx != INDEX_NONE &&
					Nodes[LN.PrimaryExecSuccIdx].bPlaced &&
					LN.MyExecOutDirIdxToSucc >= 0 && LN.SuccExecInDirIdx >= 0)
				{
					const FPALayoutNode& Succ = Nodes[LN.PrimaryExecSuccIdx];
					const int32 SuccPinY = Succ.NewY + PinLocalYFor(
						Succ.Node, EGPD_Input, LN.SuccExecInDirIdx, Cache);
					const int32 MyOutLocalY = PinLocalYFor(
						LN.Node, EGPD_Output, LN.MyExecOutDirIdxToSucc, Cache);
					LN.NewY = SuccPinY - MyOutLocalY;
				}
				else
				{
					LN.NewY = LN.Node->NodePosY;
				}
				LN.bPlaced = true;
			}
		}

		// Step 4: collision-resolve per layer. After DFS-ordered leaves +
		// downstream-pulled non-leaves, overlaps should be rare — this is
		// a safety net that pushes overlapping nodes in a layer downward.
		for (int32 L = 0; L < LayerCount; ++L)
		{
			TArray<int32> LayerNodes;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				if (Nodes[i].bHasExecPins && Nodes[i].Layer == L && Nodes[i].bPlaced)
					LayerNodes.Add(i);
			}
			LayerNodes.Sort([&](int32 A, int32 B) { return Nodes[A].NewY < Nodes[B].NewY; });
			for (int32 k = 1; k < LayerNodes.Num(); ++k)
			{
				const FPALayoutNode& Prev = Nodes[LayerNodes[k-1]];
				FPALayoutNode& Curr = Nodes[LayerNodes[k]];
				const int32 MinY = Prev.NewY + Prev.Height + VSpace;
				if (Curr.NewY < MinY) Curr.NewY = MinY;
			}
		}
	}

	/** Place pure nodes per-consumer: each producer sits `HSpace` px to the
	 *  left of its primary consumer, with its own width (not the slot's max
	 *  width). All nodes whose primary consumer shares an X naturally share
	 *  a right edge, so sibling output pins stay aligned. Narrow siblings of
	 *  wide ones no longer pay for the wide one's extra width on their input
	 *  side — wire lengths to deeper producers become HSpace + 2×PinInset
	 *  regardless of slot composition.
	 *
	 *  Y is pin-aligned to the primary consumer's input pin; collisions among
	 *  any pair of placed nodes with overlapping X-range are resolved by
	 *  downward bump. Shallowest depth first so deeper producers align to
	 *  already-placed intermediates. */
	static void PlaceDataProducersPerConsumer(TArray<FPALayoutNode>& Nodes,
		int32 HSpace, int32 VSpace,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		// Gather pure data nodes, bucket by DataDepth ascending so we process
		// shallowest (depth 1) first — their consumers are exec nodes already
		// at their final positions. Running collision resolution per-depth
		// means deeper-placed nodes align to the POST-collision Y of their
		// consumer, avoiding the classic "placed before consumer moved" bug.
		int32 MaxDepth = 0;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& LN = Nodes[i];
			if (LN.bHasExecPins) continue;
			if (LN.DataConsumerLayer == INT32_MAX) continue;
			if (LN.DataDepth == INT32_MAX) continue;
			if (LN.PrimaryConsumerIdx == INDEX_NONE) continue;
			if (LN.DataDepth > MaxDepth) MaxDepth = LN.DataDepth;
		}

		auto PlaceOne = [&](int32 Idx)
		{
			FPALayoutNode& LN = Nodes[Idx];
			const FPALayoutNode& Cons = Nodes[LN.PrimaryConsumerIdx];
			if (!Cons.bPlaced)
			{
				LN.NewX = LN.Node->NodePosX;
				LN.NewY = LN.Node->NodePosY;
				LN.bPlaced = true;
				return;
			}
			LN.NewX = Cons.NewX - HSpace - LN.Width;
			if (LN.PrimaryConsumerInDirIdx >= 0 && LN.MyOutDirIdxToPrimary >= 0)
			{
				const int32 ConsPinY = Cons.NewY + PinLocalYFor(
					Cons.Node, EGPD_Input, LN.PrimaryConsumerInDirIdx, Cache);
				const int32 MyOutLocalY = PinLocalYFor(
					LN.Node, EGPD_Output, LN.MyOutDirIdxToPrimary, Cache);
				LN.NewY = ConsPinY - MyOutLocalY;
			}
			else
			{
				LN.NewY = LN.Node->NodePosY;
			}
			LN.bPlaced = true;
		};

		// Per-depth: place, then resolve collisions against ALL already-placed
		// nodes (exec + earlier data depths). X-range overlap + Y-range overlap
		// triggers a downward bump.
		TArray<int32> AllPlaced;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bPlaced) AllPlaced.Add(i);  // exec backbone pre-placed
		}

		for (int32 d = 1; d <= MaxDepth; ++d)
		{
			// Collect nodes at this depth and place them (pin-aligned to their
			// consumers, which are by now at their final Y).
			TArray<int32> ThisDepth;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				const FPALayoutNode& LN = Nodes[i];
				if (LN.bHasExecPins) continue;
				if (LN.DataDepth != d) continue;
				if (LN.PrimaryConsumerIdx == INDEX_NONE) continue;
				PlaceOne(i);
				ThisDepth.Add(i);
			}
			if (ThisDepth.Num() == 0) continue;

			// Collision-sort this depth's nodes (lowest Y first), then push
			// any that overlap an earlier-placed node horizontally AND
			// vertically. "Earlier" = placed in a prior iteration (exec +
			// shallower depths + already-processed siblings in this depth).
			ThisDepth.Sort([&](int32 A, int32 B) {
				if (Nodes[A].NewY != Nodes[B].NewY) return Nodes[A].NewY < Nodes[B].NewY;
				return Nodes[A].NewX < Nodes[B].NewX;
			});
			for (int32 Idx : ThisDepth)
			{
				FPALayoutNode& Curr = Nodes[Idx];
				int32 MinY = INT32_MIN;
				for (int32 Other : AllPlaced)
				{
					if (Other == Idx) continue;
					const FPALayoutNode& P = Nodes[Other];
					if (!P.bPlaced) continue;
					const bool bXOverlap =
						!(Curr.NewX + Curr.Width <= P.NewX ||
						  P.NewX + P.Width <= Curr.NewX);
					if (!bXOverlap) continue;
					// Only react when the actual bounding boxes overlap (with
					// a VSpace cushion). If Curr is already fully above or
					// fully below P, no adjustment needed — otherwise we'd
					// push a perfectly-placed node down for no reason when a
					// previously-placed node was collision-bumped higher Y
					// than Curr's natural position.
					const bool bAboveP = Curr.NewY + Curr.Height + VSpace <= P.NewY;
					const bool bBelowP = P.NewY + P.Height + VSpace <= Curr.NewY;
					if (bAboveP || bBelowP) continue;
					const int32 PrevBottomPlusGap = P.NewY + P.Height + VSpace;
					if (PrevBottomPlusGap > MinY) MinY = PrevBottomPlusGap;
				}
				if (MinY != INT32_MIN && Curr.NewY < MinY) Curr.NewY = MinY;
				AllPlaced.Add(Idx);
			}
		}
	}

	/** For each primary exec edge, if the Y offset between pred's output exec
	 *  pin and successor's input exec pin exceeds YThreshold (indicating that
	 *  pin alignment was broken by collision resolution), insert a pair of
	 *  reroute knots to convert the diagonal wire into a clean L-shape:
	 *    src ─┐   (knot1 at mid-X, src pin Y — horizontal segment)
	 *         │
	 *         └─ dst   (knot2 at mid-X + offset, dst pin Y)
	 *  Must be called AFTER final Node positions (NodePosX/Y) are applied —
	 *  reads those directly. Returns the number of L-route edges knotted. */
	static int32 InsertLRouteKnotsForDiagonalExec(
		UEdGraph* Graph, const TArray<FPALayoutNode>& Nodes, int32 YThreshold,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (!Graph) return 0;

		// Single-knot routing: place ONE knot near the destination at the
		// destination pin's Y so the final segment is horizontal and the
		// first segment is a natural diagonal Bezier. Matches the way UE's
		// auto-routed wires look — cleaner than stacking two knots in an
		// L-shape for every diagonal.
		struct FSpec { UEdGraphNode* SrcNode; FName SrcPin; UEdGraphNode* DstNode; FName DstPin;
			int32 KnotX, KnotY; };
		TArray<FSpec> Specs;

		constexpr int32 KnotWidth = 42;
		constexpr int32 KnotPinCenterOffset = 12;

		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& LN = Nodes[i];
			if (!LN.bPlaced || !LN.bHasExecPins) continue;
			if (LN.PrimaryExecPredIdx == INDEX_NONE) continue;
			if (LN.PredExecOutDirIdx < 0 || LN.MyExecInDirIdx < 0) continue;

			const FPALayoutNode& Pred = Nodes[LN.PrimaryExecPredIdx];
			if (!Pred.bPlaced) continue;

			const int32 PredPinY = Pred.Node->NodePosY + PinLocalYFor(
				Pred.Node, EGPD_Output, LN.PredExecOutDirIdx, Cache);
			const int32 MyPinY   = LN.Node->NodePosY + PinLocalYFor(
				LN.Node, EGPD_Input, LN.MyExecInDirIdx, Cache);
			if (FMath::Abs(PredPinY - MyPinY) < YThreshold) continue;

			UEdGraphPin* SrcPin = nullptr;
			{
				int32 VIdx = 0;
				for (UEdGraphPin* Q : Pred.Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != EGPD_Output) continue;
					if (VIdx == LN.PredExecOutDirIdx) { SrcPin = Q; break; }
					++VIdx;
				}
			}
			UEdGraphPin* DstPin = nullptr;
			{
				int32 VIdx = 0;
				for (UEdGraphPin* Q : LN.Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != EGPD_Input) continue;
					if (VIdx == LN.MyExecInDirIdx) { DstPin = Q; break; }
					++VIdx;
				}
			}
			if (!SrcPin || !DstPin) continue;
			if (!SrcPin->LinkedTo.Contains(DstPin)) continue;

			const int32 PredRight = Pred.Node->NodePosX + Pred.Width;
			const int32 DstLeft = LN.Node->NodePosX;
			// Needs room for the knot itself + a margin either side; otherwise
			// skip and let the bare diagonal render.
			if (DstLeft - PredRight < KnotWidth + 40) continue;

			// Place the knot near the destination, at the destination pin's
			// Y. The final leg (knot.out → dst.in) is then horizontal; the
			// first leg (src.out → knot.in) bends as a natural Bezier curve.
			// Bias X toward the destination (~70%) so the horizontal tail
			// stays short relative to the curved approach.
			const int32 GapRange = DstLeft - PredRight;
			const int32 KnotX = PredRight + (GapRange * 70) / 100 - KnotWidth / 2;
			const int32 KnotY = MyPinY - KnotPinCenterOffset;

			FSpec S;
			S.SrcNode = Pred.Node; S.SrcPin = SrcPin->PinName;
			S.DstNode = LN.Node;   S.DstPin = DstPin->PinName;
			S.KnotX = KnotX;
			S.KnotY = KnotY;
			Specs.Add(S);
		}

		int32 Inserted = 0;
		for (const FSpec& S : Specs)
		{
			UEdGraphPin* SrcPin = S.SrcNode->FindPin(S.SrcPin);
			UEdGraphPin* DstPin = S.DstNode->FindPin(S.DstPin);
			if (!SrcPin || !DstPin) continue;
			if (!SrcPin->LinkedTo.Contains(DstPin)) continue;

			UK2Node_Knot* K = NewObject<UK2Node_Knot>(Graph);
			K->CreateNewGuid();
			K->NodePosX = S.KnotX;
			K->NodePosY = S.KnotY;
			Graph->AddNode(K, false, false);
			K->PostPlacedNewNode();
			K->AllocateDefaultPins();

			UEdGraphPin* KIn  = K->GetInputPin();
			UEdGraphPin* KOut = K->GetOutputPin();
			if (!KIn || !KOut)
			{
				K->DestroyNode();
				continue;
			}

			SrcPin->BreakLinkTo(DstPin);
			SrcPin->MakeLinkTo(KIn);
			KOut->MakeLinkTo(DstPin);
			Inserted += 1;
		}
		return Inserted;
	}

	/** Iterate every non-exec wire in the graph. For each one whose source
	 *  pin Y doesn't match destination pin Y (|Δ| >= YThreshold), insert
	 *  two reroute knots to convert the diagonal into a src-horizontal /
	 *  vertical / horizontal-to-dst L. Skips wires that already pass
	 *  through a knot, wires that involve comments/knots, and wires whose
	 *  endpoints aren't placed. Handles primary AND fan-out edges — e.g.
	 *  a single output feeding two pins of the same consumer at different
	 *  Y, where one wire aligns pin-to-pin but the other needs a knot. */
	static int32 InsertLRouteKnotsForAllDataWires(
		UEdGraph* Graph, int32 YThreshold,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (!Graph) return 0;

		constexpr int32 KnotWidth = 42;
		constexpr int32 KnotPinCenterOffset = 12;

		auto DirIndex = [](const UEdGraphNode* N, const UEdGraphPin* P) -> int32
		{
			if (!N || !P) return -1;
			int32 Idx = 0;
			for (const UEdGraphPin* Q : N->Pins)
			{
				if (!Q || Q->bHidden) continue;
				if (Q->Direction != P->Direction) continue;
				if (Q == P) return Idx;
				Idx += 1;
			}
			return -1;
		};

		// Snapshot edges first — we'll mutate the graph and don't want
		// iteration to include the knots we just inserted.
		struct FEdge { UEdGraphNode* SrcNode; FName SrcPin; UEdGraphNode* DstNode; FName DstPin;
			FEdGraphPinType PinType;
			int32 SrcPinY; int32 DstPinY;
			int32 SrcNodeRight; int32 DstNodeLeft; };
		TArray<FEdge> Edges;

		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			if (N->IsA<UEdGraphNode_Comment>()) continue;
			if (N->IsA<UK2Node_Knot>()) continue;
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				if (Pin->Direction != EGPD_Output) continue;
				// Data-only: skip exec category (exec uses its own L-route pass).
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				const int32 SrcDirIdx = DirIndex(N, Pin);
				if (SrcDirIdx < 0) continue;
				const int32 SrcPinY = N->NodePosY + PinLocalYFor(N, EGPD_Output, SrcDirIdx, Cache);
				const int32 SrcRight = N->NodePosX + NodeWidthFor(N, Cache);
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Dst = Linked->GetOwningNode();
					if (!Dst) continue;
					if (Dst->IsA<UEdGraphNode_Comment>()) continue;
					if (Dst->IsA<UK2Node_Knot>()) continue;
					const int32 DstDirIdx = DirIndex(Dst, Linked);
					if (DstDirIdx < 0) continue;
					const int32 DstPinY = Dst->NodePosY + PinLocalYFor(Dst, EGPD_Input, DstDirIdx, Cache);
					if (FMath::Abs(SrcPinY - DstPinY) < YThreshold) continue;
					FEdge E;
					E.SrcNode = N; E.SrcPin = Pin->PinName;
					E.DstNode = Dst; E.DstPin = Linked->PinName;
					E.PinType = Pin->PinType;
					E.SrcPinY = SrcPinY; E.DstPinY = DstPinY;
					E.SrcNodeRight = SrcRight; E.DstNodeLeft = Dst->NodePosX;
					Edges.Add(E);
				}
			}
		}

		int32 Inserted = 0;
		for (const FEdge& E : Edges)
		{
			UEdGraphPin* SrcPin = E.SrcNode->FindPin(E.SrcPin);
			UEdGraphPin* DstPin = E.DstNode->FindPin(E.DstPin);
			if (!SrcPin || !DstPin) continue;
			if (!SrcPin->LinkedTo.Contains(DstPin)) continue;
			if (E.DstNodeLeft <= E.SrcNodeRight) continue;  // wire goes right-to-left, skip
			// Need at least 2 × KnotWidth of horizontal gap to fit both knots.
			// Without that, the second knot would overshoot the destination's
			// left edge and the outbound wire would curl backwards.
			if (E.DstNodeLeft - E.SrcNodeRight < KnotWidth + 40) continue;

			// Single knot near the destination at the destination pin's Y —
			// final leg horizontal, first leg a natural Bezier curve.
			const int32 GapRange = E.DstNodeLeft - E.SrcNodeRight;
			const int32 KnotX = E.SrcNodeRight + (GapRange * 70) / 100 - KnotWidth / 2;
			const int32 KnotY = E.DstPinY - KnotPinCenterOffset;

			UK2Node_Knot* K = NewObject<UK2Node_Knot>(Graph);
			K->CreateNewGuid();
			K->NodePosX = KnotX;
			K->NodePosY = KnotY;
			Graph->AddNode(K, false, false);
			K->PostPlacedNewNode();
			K->AllocateDefaultPins();

			UEdGraphPin* KIn  = K->GetInputPin();
			UEdGraphPin* KOut = K->GetOutputPin();
			if (!KIn || !KOut) { K->DestroyNode(); continue; }

			// Make the knot carry the edge's actual value type (default is
			// wildcard). Without this, compile errors may appear on save.
			KIn->PinType  = E.PinType;
			KOut->PinType = E.PinType;

			SrcPin->BreakLinkTo(DstPin);
			SrcPin->MakeLinkTo(KIn);
			KOut->MakeLinkTo(DstPin);
			Inserted += 1;
		}
		return Inserted;
	}

	/** Park any still-unplaced, non-comment nodes (disconnected islands, etc.)
	 *  in a spillover strip below the main layout so they don't get lost. */
	static void ParkUnplacedNodes(TArray<FPALayoutNode>& Nodes, int32 MinX, int32 MaxY,
		int32 VSpace)
	{
		int32 SpillX = MinX;
		int32 SpillY = MaxY + VSpace * 3;
		for (FPALayoutNode& LN : Nodes)
		{
			if (LN.bPlaced) continue;
			if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			LN.NewX = SpillX;
			LN.NewY = SpillY;
			LN.bPlaced = true;
			SpillX += LN.Width + 40;
			if (SpillX > MinX + 2000)
			{
				SpillX = MinX;
				SpillY += LN.Height + VSpace;
			}
		}
	}
}

FBridgeLayoutResult UUnrealBridgeBlueprintLibrary::AutoLayoutGraph(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& Strategy, const FString& AnchorNodeGuid,
	int32 HorizontalSpacing, int32 VerticalSpacing)
{
	using namespace BridgeBPSummaryImpl;

	FBridgeLayoutResult Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) { Result.Warnings.Add(TEXT("blueprint not found")); return Result; }
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) { Result.Warnings.Add(TEXT("graph not found")); return Result; }

	const FString LowerStrat = Strategy.IsEmpty() ? TEXT("exec_flow") : Strategy.ToLower();
	if (LowerStrat != TEXT("exec_flow") && LowerStrat != TEXT("pin_aligned"))
	{
		Result.Warnings.Add(FString::Printf(
			TEXT("unknown strategy '%s' — supported: 'exec_flow', 'pin_aligned'"), *Strategy));
		return Result;
	}

	const int32 HSpace = HorizontalSpacing > 0 ? HorizontalSpacing : 80;
	const int32 VSpace = VerticalSpacing   > 0 ? VerticalSpacing   : 40;

	// ─── pin_aligned strategy ─────────────────────────────────
	if (LowerStrat == TEXT("pin_aligned"))
	{
		using namespace BridgeBPPinAlignedImpl;

		// Query live Slate widgets for accurate node sizes + pin offsets.
		// Requires the graph to already be open + ticked; if not, Cache is
		// empty and all helpers fall back to EstimateNodeSize/ComputePinLocalY.
		TMap<UEdGraphNode*, FRenderedGeom> Cache;
		BuildRenderedCache(Graph, Cache);
		if (Cache.Num() > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("pin_aligned: using live Slate geometry for %d nodes"), Cache.Num()));
		}

		TArray<FPALayoutNode> Nodes;
		TMap<UEdGraphNode*, int32> IndexOf;
		BuildGraph(Graph, Nodes, IndexOf, Cache);
		if (Nodes.Num() == 0) { Result.bSucceeded = true; return Result; }

		const int32 LayerCount = AssignExecLayers(Nodes, Result.Warnings);
		SelectPrimaryExecPreds(Nodes);
		SelectPrimaryExecSuccs(Nodes);
		AssignDataSlots(Nodes);

		// Per-layer widest width (exec nodes only).
		TArray<int32> LayerMaxW; LayerMaxW.SetNumZeroed(LayerCount);
		for (const FPALayoutNode& LN : Nodes)
		{
			if (!LN.bHasExecPins) continue;
			if (LN.Layer < 0 || LN.Layer >= LayerCount) continue;
			if (LN.Width > LayerMaxW[LN.Layer]) LayerMaxW[LN.Layer] = LN.Width;
		}
		// Per-slot max width + per-layer max depth.
		TMap<int64, int32> SlotWidth;
		TMap<int32, int32> LayerMaxDepth;
		ComputeDataSlots(Nodes, SlotWidth, LayerMaxDepth);

		// Per-layer data budget = sum of slot widths + HSpace gaps for this
		// layer's depth range.
		TArray<int32> LayerDataBudget; LayerDataBudget.SetNumZeroed(LayerCount);
		for (int32 L = 0; L < LayerCount; ++L)
		{
			const int32* MaxD = LayerMaxDepth.Find(L);
			if (!MaxD) continue;
			int32 Sum = 0;
			for (int32 d = 1; d <= *MaxD; ++d)
			{
				const int64 Key = MakeSlotKey(L, d);
				const int32* W = SlotWidth.Find(Key);
				const int32 SlotW = W ? *W : 0;
				Sum += SlotW + HSpace;
			}
			LayerDataBudget[L] = Sum;
		}

		// Layer X positions (cumulative). Layer 0 has no data feeding it from
		// the left (that'd be cyclic). Each later layer's X = prev layer's
		// right edge + that layer's data budget (+HSpace is already in budget).
		TArray<int32> LayerX; LayerX.SetNumZeroed(LayerCount);
		int32 Cur = 0;
		for (int32 L = 0; L < LayerCount; ++L)
		{
			LayerX[L] = Cur;
			Cur += LayerMaxW[L] + HSpace;
			if (L + 1 < LayerCount) Cur += LayerDataBudget[L + 1];
		}

		// Slot X positions: walk each layer's data depths right-to-left.
		TMap<int64, int32> SlotX;
		for (int32 L = 0; L < LayerCount; ++L)
		{
			const int32* MaxD = LayerMaxDepth.Find(L);
			if (!MaxD) continue;
			int32 SX = LayerX[L] - HSpace;
			for (int32 d = 1; d <= *MaxD; ++d)
			{
				const int64 Key = MakeSlotKey(L, d);
				const int32* W = SlotWidth.Find(Key);
				const int32 SlotW = W ? *W : 0;
				SX -= SlotW;
				SlotX.Add(Key, SX);
				SX -= HSpace;
			}
		}

		PlaceExecBackbone(Nodes, LayerCount, LayerX, VSpace, Cache, IndexOf);
		PlaceDataProducersPerConsumer(Nodes, HSpace, VSpace, Cache);

		// Compute current-layout bounds, then park anything still unplaced.
		int32 MinNewX = INT32_MAX, MinNewY = INT32_MAX, MaxNewX = INT32_MIN, MaxNewY = INT32_MIN;
		for (const FPALayoutNode& LN : Nodes)
		{
			if (!LN.bPlaced) continue;
			if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			MinNewX = FMath::Min(MinNewX, LN.NewX);
			MinNewY = FMath::Min(MinNewY, LN.NewY);
			MaxNewX = FMath::Max(MaxNewX, LN.NewX + LN.Width);
			MaxNewY = FMath::Max(MaxNewY, LN.NewY + LN.Height);
		}
		if (MinNewX == INT32_MAX) { MinNewX = MinNewY = MaxNewX = MaxNewY = 0; }
		ParkUnplacedNodes(Nodes, MinNewX, MaxNewY, VSpace);

		// Anchor origin: preserve anchor node's current graph position, else
		// reuse the graph's original bounding-box top-left.
		int32 OriginX = 0, OriginY = 0;
		const FPALayoutNode* Anchor = nullptr;
		if (!AnchorNodeGuid.IsEmpty())
		{
			for (const FPALayoutNode& LN : Nodes)
			{
				if (LN.Node && LN.Node->NodeGuid.ToString(EGuidFormats::Digits) == AnchorNodeGuid)
				{
					Anchor = &LN; break;
				}
			}
			if (!Anchor)
			{
				Result.Warnings.Add(FString::Printf(
					TEXT("anchor node %s not found"), *AnchorNodeGuid));
			}
			else
			{
				OriginX = Anchor->Node->NodePosX;
				OriginY = Anchor->Node->NodePosY;
			}
		}
		if (!Anchor)
		{
			int32 MinX = INT32_MAX, MinY = INT32_MAX;
			for (const FPALayoutNode& LN : Nodes)
			{
				if (!LN.Node) continue;
				if (LN.Node->IsA<UEdGraphNode_Comment>()) continue;
				MinX = FMath::Min(MinX, LN.Node->NodePosX);
				MinY = FMath::Min(MinY, LN.Node->NodePosY);
			}
			if (MinX != INT32_MAX) { OriginX = MinX; OriginY = MinY; }
		}

		int32 DeltaX, DeltaY;
		if (Anchor)
		{
			DeltaX = OriginX - Anchor->NewX;
			DeltaY = OriginY - Anchor->NewY;
		}
		else
		{
			DeltaX = OriginX - MinNewX;
			DeltaY = OriginY - MinNewY;
		}

		Graph->Modify();
		BP->Modify();

		int32 MaxXOut = INT32_MIN, MaxYOut = INT32_MIN;
		int32 MinXOut = INT32_MAX, MinYOut = INT32_MAX;
		for (FPALayoutNode& LN : Nodes)
		{
			if (!LN.bPlaced) continue;
			if (!LN.Node) continue;
			if (LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			const int32 NX = LN.NewX + DeltaX;
			const int32 NY = LN.NewY + DeltaY;
			LN.Node->Modify();
			LN.Node->NodePosX = NX;
			LN.Node->NodePosY = NY;
			MinXOut = FMath::Min(MinXOut, NX);
			MinYOut = FMath::Min(MinYOut, NY);
			MaxXOut = FMath::Max(MaxXOut, NX + LN.Width);
			MaxYOut = FMath::Max(MaxYOut, NY + LN.Height);
			Result.NodesPositioned += 1;
		}

		// Post-placement: L-route diagonal EXEC edges only. Exec flow needs
		// rectilinear turns to read cleanly, and diagonal Bezier on an exec
		// wire reads as a misdirection. Data wires, by contrast, are
		// comfortable as gentle Beziers — hand-authored BPs regularly leave
		// data diagonals with no knots (see reference BPs); adding knots for
		// every small Y offset just creates visual clutter. Keep the data
		// layout pin-aligned at the node level (which PlaceDataProducersPerConsumer
		// already does) and let UE's bezier render the unavoidable diagonals.
		const int32 LKnotsExec = InsertLRouteKnotsForDiagonalExec(Graph, Nodes, 30, Cache);
		const int32 LKnotsData = 0;

		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

		Result.bSucceeded = true;
		Result.LayerCount = LayerCount;
		Result.BoundsWidth  = (MaxXOut == INT32_MIN) ? 0 : (MaxXOut - MinXOut);
		Result.BoundsHeight = (MaxYOut == INT32_MIN) ? 0 : (MaxYOut - MinYOut);
		if (LKnotsExec > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("pin_aligned: inserted %d reroute knots for diagonal exec edges"), LKnotsExec));
		}
		if (LKnotsData > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("pin_aligned: inserted %d reroute knots for diagonal data edges"), LKnotsData));
		}
		return Result;
	}

	// ─── exec_flow strategy (original) ────────────────────────
	using namespace BridgeBPAutoLayoutImpl;

	TArray<FLayoutNode> Nodes;
	TMap<UEdGraphNode*, int32> IndexOf;
	BuildGraph(Graph, Nodes, IndexOf);
	if (Nodes.Num() == 0) { Result.bSucceeded = true; return Result; }

	// Skip comment boxes — they enclose other nodes; moving them breaks intent.
	TArray<int32> Layoutable;
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		if (Nodes[i].Node && !Nodes[i].Node->IsA<UEdGraphNode_Comment>())
		{
			Layoutable.Add(i);
		}
	}

	const int32 LayerCount = AssignLayers(Nodes, Result.Warnings);
	BarycentricOrder(Nodes, LayerCount);

	// Compute per-layer widest + per-layer cumulative X offset.
	TArray<int32> LayerWidth; LayerWidth.SetNumZeroed(LayerCount);
	TArray<TArray<int32>> NodesByLayer; NodesByLayer.SetNum(LayerCount);
	for (int32 Idx : Layoutable)
	{
		const FLayoutNode& LN = Nodes[Idx];
		const int32 L = FMath::Clamp(LN.Layer, 0, LayerCount - 1);
		if (LN.Width > LayerWidth[L]) LayerWidth[L] = LN.Width;
		NodesByLayer[L].Add(Idx);
	}

	// Order within each layer by OrderInLayer (already set).
	for (TArray<int32>& Layer : NodesByLayer)
	{
		Layer.Sort([&](int32 A, int32 B) {
			return Nodes[A].OrderInLayer < Nodes[B].OrderInLayer;
		});
	}

	// Compute anchor origin: use anchor node's current position if given, else
	// top-left of existing bounding box.
	int32 OriginX = 0, OriginY = 0;
	const FLayoutNode* Anchor = nullptr;
	if (!AnchorNodeGuid.IsEmpty())
	{
		for (const FLayoutNode& LN : Nodes)
		{
			if (LN.Node && LN.Node->NodeGuid.ToString(EGuidFormats::Digits) == AnchorNodeGuid)
			{
				Anchor = &LN; break;
			}
		}
		if (!Anchor)
		{
			Result.Warnings.Add(FString::Printf(TEXT("anchor node %s not found"), *AnchorNodeGuid));
		}
		else
		{
			OriginX = Anchor->Node->NodePosX;
			OriginY = Anchor->Node->NodePosY;
		}
	}
	if (!Anchor)
	{
		int32 MinX = INT32_MAX, MinY = INT32_MAX;
		for (int32 Idx : Layoutable)
		{
			MinX = FMath::Min(MinX, Nodes[Idx].Node->NodePosX);
			MinY = FMath::Min(MinY, Nodes[Idx].Node->NodePosY);
		}
		if (MinX != INT32_MAX) { OriginX = MinX; OriginY = MinY; }
	}

	// Walk layers: compute X offset per layer, then stack nodes vertically.
	TArray<int32> LayerX; LayerX.SetNum(LayerCount);
	int32 Cursor = 0;
	for (int32 L = 0; L < LayerCount; ++L)
	{
		LayerX[L] = Cursor;
		Cursor += LayerWidth[L] + HSpace;
	}

	// Vertically center each layer around the bounding box midpoint.
	int32 MaxLayerHeight = 0;
	TArray<int32> LayerHeight; LayerHeight.SetNumZeroed(LayerCount);
	for (int32 L = 0; L < LayerCount; ++L)
	{
		int32 H = 0;
		for (int32 Idx : NodesByLayer[L])
		{
			H += Nodes[Idx].Height + VSpace;
		}
		if (H > 0) H -= VSpace;  // no trailing spacing
		LayerHeight[L] = H;
		if (H > MaxLayerHeight) MaxLayerHeight = H;
	}

	// If anchor is set, preserve anchor's (NodePosX, NodePosY) exactly. Compute
	// the delta between anchor's would-be layout position and its current one,
	// then translate everyone by the delta.
	int32 DeltaX = OriginX;
	int32 DeltaY = OriginY;
	if (Anchor)
	{
		const int32 AnchorIdx = IndexOf[Anchor->Node];
		const int32 AL = FMath::Clamp(Nodes[AnchorIdx].Layer, 0, LayerCount - 1);
		const int32 AnchorLayoutX = LayerX[AL];
		int32 AnchorLayoutY = 0;
		for (int32 Idx : NodesByLayer[AL])
		{
			if (Idx == AnchorIdx) break;
			AnchorLayoutY += Nodes[Idx].Height + VSpace;
		}
		// Center layer vertically: layer y starts at -LayerHeight/2.
		AnchorLayoutY -= LayerHeight[AL] / 2;
		DeltaX = OriginX - AnchorLayoutX;
		DeltaY = OriginY - AnchorLayoutY;
	}

	Graph->Modify();
	BP->Modify();

	int32 MaxXOut = 0, MaxYOut = 0;
	for (int32 L = 0; L < LayerCount; ++L)
	{
		int32 Y = -LayerHeight[L] / 2;
		for (int32 Idx : NodesByLayer[L])
		{
			FLayoutNode& LN = Nodes[Idx];
			const int32 NewX = DeltaX + LayerX[L];
			const int32 NewY = DeltaY + Y;
			LN.Node->Modify();
			LN.Node->NodePosX = NewX;
			LN.Node->NodePosY = NewY;
			if (NewX + LN.Width  > MaxXOut) MaxXOut = NewX + LN.Width;
			if (NewY + LN.Height > MaxYOut) MaxYOut = NewY + LN.Height;
			Y += LN.Height + VSpace;
			Result.NodesPositioned += 1;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	Result.bSucceeded = true;
	Result.LayerCount = LayerCount;
	Result.BoundsWidth  = MaxXOut - DeltaX;
	Result.BoundsHeight = MaxYOut - DeltaY;
	return Result;
}

TArray<FBridgePinInfo> UUnrealBridgeBlueprintLibrary::GetNodePins(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace BridgeBPSummaryImpl;
	TArray<FBridgePinInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Result;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Result;

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		FBridgePinInfo Info;
		Info.Name        = Pin->PinName.ToString();
		Info.DisplayName = Pin->GetDisplayName().ToString();
		Info.Type        = PinTypeToHuman(Pin->PinType);
		Info.Direction   = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
		Info.Category    = Pin->PinType.PinCategory.ToString();
		Info.SubCategory = Pin->PinType.PinSubCategory.ToString();
		if (UObject* SubObj = Pin->PinType.PinSubCategoryObject.Get())
		{
			Info.SubCategoryObjectPath = SubObj->GetPathName();
		}
		Info.DefaultValue     = GetPinDefaultString(Pin);
		Info.bHasDefaultObject = (Pin->DefaultObject != nullptr);
		Info.bIsExec          = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		Info.bIsConnected     = (Pin->LinkedTo.Num() > 0);
		Info.LinkCount        = Pin->LinkedTo.Num();
		Info.bIsArray         = (Pin->PinType.ContainerType == EPinContainerType::Array);
		Info.bIsSet           = (Pin->PinType.ContainerType == EPinContainerType::Set);
		Info.bIsMap           = (Pin->PinType.ContainerType == EPinContainerType::Map);
		Info.bIsReference     = Pin->PinType.bIsReference;
		Info.bIsConst         = Pin->PinType.bIsConst;
		Info.bIsHidden        = Pin->bHidden;
		Info.bIsSelfPin       = (K2 && K2->IsSelfPin(*Pin));

		switch (Pin->PinType.ContainerType)
		{
			case EPinContainerType::Array: Info.ContainerKind = TEXT("Array"); break;
			case EPinContainerType::Set:   Info.ContainerKind = TEXT("Set");   break;
			case EPinContainerType::Map:   Info.ContainerKind = TEXT("Map");   break;
			default:                        Info.ContainerKind = TEXT("None");  break;
		}

		// Map V-side: only meaningful when this is a Map pin. PinValueType
		// holds the value-side terminal info (key side lives in regular
		// PinSubCategory*). Re-use PinTypeToHuman by faking a non-container
		// FEdGraphPinType built from the value terminal.
		if (Pin->PinType.ContainerType == EPinContainerType::Map)
		{
			FEdGraphPinType ValueProxy;
			ValueProxy.PinCategory          = Pin->PinType.PinValueType.TerminalCategory;
			ValueProxy.PinSubCategory       = Pin->PinType.PinValueType.TerminalSubCategory;
			ValueProxy.PinSubCategoryObject = Pin->PinType.PinValueType.TerminalSubCategoryObject;
			ValueProxy.ContainerType        = EPinContainerType::None;
			Info.MapValueType        = PinTypeToHuman(ValueProxy);
			Info.MapValueCategory    = ValueProxy.PinCategory.ToString();
			Info.MapValueSubCategory = ValueProxy.PinSubCategory.ToString();
			if (UObject* VObj = ValueProxy.PinSubCategoryObject.Get())
			{
				Info.MapValueSubCategoryObjectPath = VObj->GetPathName();
			}
		}
		Result.Add(Info);
	}
	return Result;
}

TArray<FBridgeReference> UUnrealBridgeBlueprintLibrary::FindEventHandlerSites(
	const FString& BlueprintPath, const FString& EventName)
{
	using namespace BridgeBPSummaryImpl;
	TArray<FBridgeReference> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || EventName.IsEmpty()) return Result;

	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (const UK2Node_Event* E = Cast<UK2Node_Event>(Node))
			{
				if (E->GetFunctionName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("event")));
				}
			}
			else if (const UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
			{
				if (CE->CustomFunctionName.ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("event")));
				}
			}
			else if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node))
			{
				if (CD->GetPropertyName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
			else if (const UK2Node_AddDelegate* AD = Cast<UK2Node_AddDelegate>(Node))
			{
				if (AD->GetPropertyName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("bind")));
				}
			}
			else if (const UK2Node_RemoveDelegate* RD = Cast<UK2Node_RemoveDelegate>(Node))
			{
				if (RD->GetPropertyName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("unbind")));
				}
			}
		}
	}
	return Result;
}

// ─── Universal node spawner (FBlueprintActionDatabase) ─────────

namespace BridgeBPActionDBImpl
{
	/** Resolve the spawner's owning class/asset path for filtering + reporting.
	 *  Function spawners report the function's owning class; variable spawners
	 *  the variable's owner; for everything else we use the action-DB key
	 *  (typically the K2Node class itself or the asset that registered it). */
	static void ResolveOwner(const UBlueprintNodeSpawner* Spawner, UObject* RegistryKey,
		FString& OutOwnerName, FString& OutOwnerPath)
	{
		if (const UBlueprintFunctionNodeSpawner* FS = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
		{
			if (const UFunction* Fn = FS->GetFunction())
			{
				if (UClass* OwnerCls = Fn->GetOwnerClass())
				{
					OutOwnerName = OwnerCls->GetName();
					OutOwnerPath = OwnerCls->GetPathName();
					return;
				}
			}
		}
		if (RegistryKey)
		{
			OutOwnerName = RegistryKey->GetName();
			OutOwnerPath = RegistryKey->GetPathName();
		}
	}

	/** Build a UI signature for a spawner without touching the template-node
	 *  cache. PrimeDefaultUiSpec() is unsafe to call in a loop because it
	 *  spawns a transient template node and asserts when the spawner's
	 *  NodeClass has no schema-compatible cached graph (e.g. AnimGraph,
	 *  SoundCue, Niagara nodes registered alongside K2 ones).
	 *
	 *  Strategy:
	 *    1. Use DefaultMenuSignature if a registrar already populated it.
	 *    2. Otherwise synthesise from the spawner subtype:
	 *       - Function spawners → reflect on the UFunction (DisplayName,
	 *         Category, Tooltip, Keywords metadata).
	 *       - Anything else → use NodeClass display name as the title and
	 *         leave the rest blank. Caller can still spawn by Key.
	 */
	static FBlueprintActionUiSpec BuildUiSpecSafe(const UBlueprintNodeSpawner* Spawner)
	{
		FBlueprintActionUiSpec Out = Spawner->DefaultMenuSignature;
		if (!Out.MenuName.IsEmpty())
		{
			return Out;
		}

		if (const UBlueprintFunctionNodeSpawner* FS = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
		{
			if (const UFunction* Fn = FS->GetFunction())
			{
				const FString DisplayName = Fn->HasMetaData(TEXT("DisplayName"))
					? Fn->GetMetaData(TEXT("DisplayName")) : Fn->GetName();
				Out.MenuName = FText::FromString(DisplayName);
				Out.Category = FText::FromString(Fn->GetMetaData(TEXT("Category")));
				Out.Tooltip  = Fn->GetToolTipText();
				Out.Keywords = FText::FromString(Fn->GetMetaData(TEXT("Keywords")));
				return Out;
			}
		}

		if (Spawner->NodeClass)
		{
			Out.MenuName = Spawner->NodeClass->GetDisplayNameText();
		}
		return Out;
	}
}

TArray<FBridgeSpawnableAction> UUnrealBridgeBlueprintLibrary::ListSpawnableActions(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& Keyword, const FString& CategoryContains,
	const FString& OwningClassPath, const FString& NodeType,
	int32 MaxResults)
{
	TArray<FBridgeSpawnableAction> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return Out;

	const int32 Cap = FMath::Max(1, MaxResults);

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	// Make sure asset-defined actions for this BP are present (custom events,
	// user functions etc.). Cheap if already cached.
	Database.RefreshAssetActions(BP);
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		UObject* RegistryKey = Pair.Key.ResolveObjectPtr();

		// Per-key OwningClassPath fast-path: skip the whole bucket when the
		// filter is set and the bucket's key doesn't match. (Function spawners
		// override the owner with the function's owning class, so we only
		// short-circuit here when both the bucket key and any function-owner
		// would also miss — to stay correct, just compare on RegistryKey.)
		if (!OwningClassPath.IsEmpty() && RegistryKey
			&& RegistryKey->GetPathName() != OwningClassPath)
		{
			// Function spawners may still match via their function owner;
			// fall through and let the per-spawner check decide.
		}

		for (UBlueprintNodeSpawner* Spawner : Pair.Value)
		{
			if (!Spawner || !Spawner->NodeClass) continue;

			// Skip non-K2 spawners (anim/niagara/sound graph nodes etc.).
			// They share the database but their template-node lookup asserts
			// when probed against a K2 graph; keeping them out also prevents
			// the resulting Spawn from ever succeeding on a BP graph.
			if (!Spawner->NodeClass->IsChildOf(UK2Node::StaticClass())) continue;

			// NodeType filter
			if (!NodeType.IsEmpty() && Spawner->NodeClass->GetName() != NodeType) continue;

			// Owner resolution + OwningClassPath filter
			FString OwnerName, OwnerPath;
			BridgeBPActionDBImpl::ResolveOwner(Spawner, RegistryKey, OwnerName, OwnerPath);
			if (!OwningClassPath.IsEmpty() && OwnerPath != OwningClassPath) continue;

			// Pull the UI signature without touching the template-node cache.
			const FBlueprintActionUiSpec UiSpec = BridgeBPActionDBImpl::BuildUiSpecSafe(Spawner);

			const FString TitleStr    = UiSpec.MenuName.ToString();
			const FString CategoryStr = UiSpec.Category.ToString();
			const FString TooltipStr  = UiSpec.Tooltip.ToString();
			const FString KeywordsStr = UiSpec.Keywords.ToString();

			if (!CategoryContains.IsEmpty()
				&& !CategoryStr.Contains(CategoryContains, ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (!Keyword.IsEmpty())
			{
				const bool bHit =
					   TitleStr.Contains(Keyword,    ESearchCase::IgnoreCase)
					|| TooltipStr.Contains(Keyword,  ESearchCase::IgnoreCase)
					|| KeywordsStr.Contains(Keyword, ESearchCase::IgnoreCase)
					|| CategoryStr.Contains(Keyword, ESearchCase::IgnoreCase);
				if (!bHit) continue;
			}

			FBridgeSpawnableAction Action;
			Action.Key             = Spawner->GetSpawnerSignature().ToString();
			Action.Title           = TitleStr;
			Action.Category        = CategoryStr;
			Action.Tooltip         = TooltipStr;
			Action.Keywords        = KeywordsStr;
			Action.NodeType        = Spawner->NodeClass ? Spawner->NodeClass->GetName() : FString();
			Action.OwningClass     = OwnerName;
			Action.OwningClassPath = OwnerPath;
			Out.Add(MoveTemp(Action));

			if (Out.Num() >= Cap) return Out;
		}
	}
	return Out;
}

FString UUnrealBridgeBlueprintLibrary::SpawnNodeByActionKey(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ActionKey, int32 NodePosX, int32 NodePosY)
{
	if (ActionKey.IsEmpty()) return FString();
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	Database.RefreshAssetActions(BP);
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	UBlueprintNodeSpawner* Found = nullptr;
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		for (UBlueprintNodeSpawner* Spawner : Pair.Value)
		{
			if (!Spawner || !Spawner->NodeClass) continue;
			if (!Spawner->NodeClass->IsChildOf(UK2Node::StaticClass())) continue;
			if (Spawner->GetSpawnerSignature().ToString() == ActionKey)
			{
				Found = Spawner;
				break;
			}
		}
		if (Found) break;
	}
	if (!Found) return FString();

	Graph->Modify();
	BP->Modify();

	IBlueprintNodeBinder::FBindingSet Bindings;
	UEdGraphNode* NewNode = Found->Invoke(Graph, Bindings,
		FVector2D(static_cast<float>(NodePosX), static_cast<float>(NodePosY)));
	if (!NewNode) return FString();

	// Spawner::Invoke usually adds + allocates pins itself, but a few spawners
	// rely on the schema's PostPlacedNewNode hook — call it defensively.
	NewNode->NodePosX = NodePosX;
	NewNode->NodePosY = NodePosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return NewNode->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Fine-grained pin link control ─────────────────────────────

bool UUnrealBridgeBlueprintLibrary::DisconnectPinLink(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& SourceNodeGuid, const FString& SourcePinName,
	const FString& TargetNodeGuid, const FString& TargetPinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = BridgeBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* SrcNode = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, SourceNodeGuid);
	UEdGraphNode* DstNode = BridgeBlueprintGraphWriteImpl::FindNodeByGuid(Graph, TargetNodeGuid);
	if (!SrcNode || !DstNode) return false;

	UEdGraphPin* SrcPin = SrcNode->FindPin(SourcePinName);
	UEdGraphPin* DstPin = DstNode->FindPin(TargetPinName);
	if (!SrcPin || !DstPin) return false;

	if (!SrcPin->LinkedTo.Contains(DstPin)) return false;

	Graph->Modify();
	BP->Modify();

	// BreakLinkTo is symmetric — removes the edge from both pins' LinkedTo lists.
	SrcPin->BreakLinkTo(DstPin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── Lint / quality review ─────────────────────────────────────

namespace BridgeBPLintImpl
{
	struct FLintCtx
	{
		UBlueprint* BP = nullptr;
		TArray<FBridgeLintIssue>* Out = nullptr;
		int32 OversizedFnThreshold = 20;
		int32 LongExecChainThreshold = 15;
		int32 LargeGraphThreshold = 10;
	};

	static void Emit(FLintCtx& C, const FString& Severity, const FString& Code,
		const FString& Message, const FString& GraphName,
		const FString& NodeGuid, const FString& VarName, const FString& FnName)
	{
		FBridgeLintIssue I;
		I.Severity = Severity;
		I.Code = Code;
		I.Message = Message;
		I.GraphName = GraphName;
		I.NodeGuid = NodeGuid;
		I.VariableName = VarName;
		I.FunctionName = FnName;
		C.Out->Add(I);
	}

	/** True if every pin on the node has zero links and zero default pin-object. */
	static bool IsFullyOrphan(const UEdGraphNode* Node)
	{
		if (!Node) return false;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->LinkedTo.Num() > 0) return false;
		}
		return true;
	}

	/** Heuristic: name matches default UE naming for new custom events. */
	static bool LooksDefaultCustomEventName(const FString& Name)
	{
		// e.g. "CustomEvent", "CustomEvent_0", "Event", "Event_0"
		if (Name.Equals(TEXT("CustomEvent"), ESearchCase::IgnoreCase)) return true;
		if (Name.Equals(TEXT("Event"),       ESearchCase::IgnoreCase)) return true;
		for (const TCHAR* Prefix : { TEXT("CustomEvent_"), TEXT("Event_") })
		{
			if (Name.StartsWith(Prefix))
			{
				const FString Tail = Name.RightChop(FCString::Strlen(Prefix));
				if (!Tail.IsEmpty() && Tail.IsNumeric()) return true;
			}
		}
		return false;
	}

	static bool LooksDefaultFunctionName(const FString& Name)
	{
		if (Name.Equals(TEXT("NewFunction"), ESearchCase::IgnoreCase)) return true;
		if (Name.StartsWith(TEXT("NewFunction_")))
		{
			const FString Tail = Name.RightChop(12);
			if (!Tail.IsEmpty() && Tail.IsNumeric()) return true;
		}
		return false;
	}

	/** Count non-comment, non-ghost nodes. */
	static int32 CountRealNodes(const UEdGraph* Graph)
	{
		int32 Count = 0;
		if (!Graph) return 0;
		for (const UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			if (N->IsA<UEdGraphNode_Comment>()) continue;
			Count += 1;
		}
		return Count;
	}

	static int32 CountCommentBoxes(const UEdGraph* Graph)
	{
		int32 Count = 0;
		if (!Graph) return 0;
		for (const UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->IsA<UEdGraphNode_Comment>()) Count += 1;
		}
		return Count;
	}

	/** Find the longest linear exec chain starting from any node. "Linear"
	 *  means each successor has exactly 1 exec-input link AND the upstream
	 *  emits exactly 1 exec-output link — i.e. no branching, no merging. */
	static int32 LongestLinearExecChain(UEdGraph* Graph)
	{
		if (!Graph) return 0;
		int32 Best = 0;
		TMap<UEdGraphNode*, int32> Memo;
		TFunction<int32(UEdGraphNode*)> Walk = [&](UEdGraphNode* N) -> int32
		{
			if (!N) return 0;
			if (int32* M = Memo.Find(N)) return *M;
			Memo.Add(N, 1);  // break cycles by reserving

			int32 MyBest = 1;
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin) continue;
				if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->Direction != EGPD_Output) continue;
				if (Pin->LinkedTo.Num() != 1) continue;  // branching → not linear
				UEdGraphPin* DownIn = Pin->LinkedTo[0];
				if (!DownIn || DownIn->LinkedTo.Num() != 1) continue;  // merging
				UEdGraphNode* Next = DownIn->GetOwningNode();
				if (!Next || Next == N) continue;
				MyBest = FMath::Max(MyBest, 1 + Walk(Next));
			}
			Memo.Add(N, MyBest);  // overwrite placeholder
			return MyBest;
		};
		for (UEdGraphNode* N : Graph->Nodes)
		{
			Best = FMath::Max(Best, Walk(N));
		}
		return Best;
	}

	// ─── Individual checks ────────────────────────────────

	static void CheckOrphanNodes(FLintCtx& C)
	{
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* G : C.BP->UbergraphPages)  Graphs.Add(G);
		for (UEdGraph* G : C.BP->FunctionGraphs)  Graphs.Add(G);
		for (UEdGraph* G : C.BP->MacroGraphs)     Graphs.Add(G);

		for (UEdGraph* G : Graphs)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N) continue;
				if (N->IsA<UEdGraphNode_Comment>()) continue;
				if (N->IsA<UK2Node_FunctionEntry>()) continue;
				if (N->IsA<UK2Node_FunctionResult>()) continue;

				// "Tunnel" nodes on macro graphs are entry/exit — always orphan-ish.
				if (N->GetClass()->GetName().Contains(TEXT("Tunnel"))) continue;

				if (!IsFullyOrphan(N)) continue;

				// Events with no "then" connection ARE dead code and worth flagging.
				const FString Msg = FString::Printf(
					TEXT("Node '%s' (%s) has no connections — dead code or left-over spawn"),
					*N->GetNodeTitle(ENodeTitleType::ListView).ToString(),
					*N->GetClass()->GetName());
				Emit(C, TEXT("warning"), TEXT("OrphanNode"), Msg,
					G->GetName(), N->NodeGuid.ToString(EGuidFormats::Digits),
					FString(), FString());
			}
		}
	}

	static void CheckOversizedFunctions(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->FunctionGraphs)
		{
			if (!G) continue;
			const int32 N = CountRealNodes(G);
			if (N <= C.OversizedFnThreshold) continue;
			const FString Msg = FString::Printf(
				TEXT("Function '%s' has %d nodes (threshold %d) — consider extracting sub-functions"),
				*G->GetName(), N, C.OversizedFnThreshold);
			Emit(C, TEXT("warning"), TEXT("OversizedFunction"), Msg,
				G->GetName(), FString(), FString(), G->GetName());
		}
	}

	static void CheckUnnamedCustomEvents(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->UbergraphPages)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(N))
				{
					if (LooksDefaultCustomEventName(CE->CustomFunctionName.ToString()))
					{
						const FString Msg = FString::Printf(
							TEXT("CustomEvent '%s' still uses a default name — rename to describe intent"),
							*CE->CustomFunctionName.ToString());
						Emit(C, TEXT("warning"), TEXT("UnnamedCustomEvent"), Msg,
							G->GetName(), CE->NodeGuid.ToString(EGuidFormats::Digits),
							FString(), FString());
					}
				}
			}
		}
	}

	static void CheckUnnamedFunctions(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->FunctionGraphs)
		{
			if (!G) continue;
			if (LooksDefaultFunctionName(G->GetName()))
			{
				const FString Msg = FString::Printf(
					TEXT("Function graph '%s' still uses the default name — rename to describe intent"),
					*G->GetName());
				Emit(C, TEXT("warning"), TEXT("UnnamedFunction"), Msg,
					G->GetName(), FString(), FString(), G->GetName());
			}
		}
	}

	static void CheckVariableMetadata(FLintCtx& C)
	{
		for (const FBPVariableDescription& V : C.BP->NewVariables)
		{
			const bool bEditable = (V.PropertyFlags & CPF_Edit) != 0;
			if (!bEditable) continue;

			const FString Cat = V.Category.ToString();
			const bool bDefaultCat = Cat.IsEmpty()
			                       || Cat.Equals(TEXT("Default"), ESearchCase::IgnoreCase)
			                       || Cat.Equals(V.VarName.ToString(), ESearchCase::IgnoreCase);
			if (bDefaultCat)
			{
				const FString Msg = FString::Printf(
					TEXT("Editable variable '%s' lacks a custom Category — set one to group it in Details"),
					*V.VarName.ToString());
				Emit(C, TEXT("info"), TEXT("InstanceEditableNoCategory"), Msg,
					FString(), FString(), V.VarName.ToString(), FString());
			}
			const FString Tip = V.HasMetaData(TEXT("tooltip"))
				? V.GetMetaData(TEXT("tooltip")) : FString();
			if (Tip.IsEmpty())
			{
				const FString Msg = FString::Printf(
					TEXT("Editable variable '%s' has no tooltip — designers won't know what it does"),
					*V.VarName.ToString());
				Emit(C, TEXT("info"), TEXT("InstanceEditableNoTooltip"), Msg,
					FString(), FString(), V.VarName.ToString(), FString());
			}
		}
	}

	/** Walk every graph, count K2Node_Variable references to VarName. */
	static int32 CountVariableRefs(UBlueprint* BP, const FName& VarName, UEdGraph* RestrictToGraph = nullptr)
	{
		int32 Count = 0;
		TArray<UEdGraph*> Graphs;
		if (RestrictToGraph) { Graphs.Add(RestrictToGraph); }
		else
		{
			for (UEdGraph* G : BP->UbergraphPages) Graphs.Add(G);
			for (UEdGraph* G : BP->FunctionGraphs) Graphs.Add(G);
			for (UEdGraph* G : BP->MacroGraphs)    Graphs.Add(G);
		}
		for (UEdGraph* G : Graphs)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_Variable* V = Cast<UK2Node_Variable>(N))
				{
					if (V->VariableReference.GetMemberName() == VarName) Count += 1;
				}
			}
		}
		return Count;
	}

	static void CheckUnusedClassVariables(FLintCtx& C)
	{
		for (const FBPVariableDescription& V : C.BP->NewVariables)
		{
			const int32 Refs = CountVariableRefs(C.BP, V.VarName);
			if (Refs > 0) continue;
			// Skip editable vars — they're a legitimate external API even with 0 internal refs.
			if ((V.PropertyFlags & CPF_Edit) != 0) continue;
			const FString Msg = FString::Printf(
				TEXT("Variable '%s' is never read or written inside this Blueprint"),
				*V.VarName.ToString());
			Emit(C, TEXT("info"), TEXT("UnusedVariable"), Msg,
				FString(), FString(), V.VarName.ToString(), FString());
		}
	}

	static void CheckUnusedLocalVariables(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->FunctionGraphs)
		{
			if (!G) continue;
			UK2Node_FunctionEntry* Entry = nullptr;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) { Entry = E; break; }
			}
			if (!Entry) continue;
			for (const FBPVariableDescription& V : Entry->LocalVariables)
			{
				const int32 Refs = CountVariableRefs(C.BP, V.VarName, G);
				if (Refs > 0) continue;
				const FString Msg = FString::Printf(
					TEXT("Local variable '%s' in function '%s' is never read or written"),
					*V.VarName.ToString(), *G->GetName());
				Emit(C, TEXT("info"), TEXT("UnusedLocalVariable"), Msg,
					G->GetName(), FString(), V.VarName.ToString(), G->GetName());
			}
		}
	}

	static void CheckLargeUncommentedGraphs(FLintCtx& C)
	{
		auto Check = [&](UEdGraph* G)
		{
			if (!G) return;
			const int32 NReal = CountRealNodes(G);
			if (NReal < C.LargeGraphThreshold) return;
			if (CountCommentBoxes(G) > 0) return;
			const FString Msg = FString::Printf(
				TEXT("Graph '%s' has %d nodes but no comment boxes — add section labels to orient readers"),
				*G->GetName(), NReal);
			Emit(C, TEXT("info"), TEXT("LargeUncommentedGraph"), Msg,
				G->GetName(), FString(), FString(), FString());
		};
		for (UEdGraph* G : C.BP->UbergraphPages) Check(G);
		for (UEdGraph* G : C.BP->FunctionGraphs) Check(G);
	}

	static void CheckLongExecChains(FLintCtx& C)
	{
		auto Check = [&](UEdGraph* G)
		{
			if (!G) return;
			const int32 Len = LongestLinearExecChain(G);
			if (Len < C.LongExecChainThreshold) return;
			const FString Msg = FString::Printf(
				TEXT("Graph '%s' has a linear exec chain of %d nodes — consider extracting a function"),
				*G->GetName(), Len);
			Emit(C, TEXT("warning"), TEXT("LongExecChain"), Msg,
				G->GetName(), FString(), FString(), FString());
		};
		for (UEdGraph* G : C.BP->UbergraphPages) Check(G);
		for (UEdGraph* G : C.BP->FunctionGraphs) Check(G);
	}
}

TArray<FBridgeLintIssue> UUnrealBridgeBlueprintLibrary::LintBlueprint(
	const FString& BlueprintPath, const FString& SeverityFilter,
	int32 OversizedFunctionThreshold, int32 LongExecChainThreshold,
	int32 LargeGraphThreshold)
{
	using namespace BridgeBPLintImpl;
	TArray<FBridgeLintIssue> Issues;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Issues;

	FLintCtx C;
	C.BP = BP;
	C.Out = &Issues;
	C.OversizedFnThreshold   = OversizedFunctionThreshold > 0 ? OversizedFunctionThreshold : 20;
	C.LongExecChainThreshold = LongExecChainThreshold   > 0 ? LongExecChainThreshold   : 15;
	C.LargeGraphThreshold    = LargeGraphThreshold      > 0 ? LargeGraphThreshold      : 10;

	CheckOrphanNodes(C);
	CheckOversizedFunctions(C);
	CheckUnnamedCustomEvents(C);
	CheckUnnamedFunctions(C);
	CheckVariableMetadata(C);
	CheckUnusedClassVariables(C);
	CheckUnusedLocalVariables(C);
	CheckLargeUncommentedGraphs(C);
	CheckLongExecChains(C);

	if (!SeverityFilter.IsEmpty())
	{
		Issues.RemoveAll([&](const FBridgeLintIssue& I)
		{
			return !I.Severity.Equals(SeverityFilter, ESearchCase::IgnoreCase);
		});
	}
	return Issues;
}

// ─── Collapse to function ──────────────────────────────────────

namespace BridgeBPCollapseImpl
{
	/** Resolve a node GUID in a graph. */
	static UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr)
	{
		if (!Graph) return nullptr;
		FGuid Guid;
		if (!FGuid::Parse(GuidStr, Guid)) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid == Guid) return N;
		}
		return nullptr;
	}

	/** Sort pins for stable user-facing order: inputs first by Y, outputs by Y. */
	static void SortPinsByPosition(TArray<UEdGraphPin*>& Pins)
	{
		Pins.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
		{
			const UEdGraphNode* NA = A.GetOwningNode();
			const UEdGraphNode* NB = B.GetOwningNode();
			if (A.Direction != B.Direction) return A.Direction == EGPD_Input;
			if (!NA || !NB) return false;
			if (NA->NodePosY != NB->NodePosY) return NA->NodePosY < NB->NodePosY;
			return NA->NodePosX < NB->NodePosX;
		});
	}
}

FString UUnrealBridgeBlueprintLibrary::CollapseNodesToFunction(
	const FString& BlueprintPath, const FString& SourceGraphName,
	const TArray<FString>& NodeGuids, const FString& NewFunctionName,
	FString& OutNewGraphName)
{
	using namespace BridgeBPCollapseImpl;
	using namespace BridgeBlueprintGraphWriteImpl;

	OutNewGraphName.Empty();
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* SourceGraph = FindGraphByName(BP, SourceGraphName);
	if (!SourceGraph) return FString();

	// Resolve selection; reject entry/result/tunnel/orphaned-event scaffolding.
	TSet<UEdGraphNode*> Selection;
	int32 MinX = INT32_MAX, MinY = INT32_MAX, MaxX = INT32_MIN, MaxY = INT32_MIN;
	int32 SumX = 0, SumY = 0;
	for (const FString& GuidStr : NodeGuids)
	{
		UEdGraphNode* N = BridgeBPCollapseImpl::FindNodeByGuid(SourceGraph, GuidStr);
		if (!N) return FString();
		if (N->IsA<UK2Node_FunctionEntry>() || N->IsA<UK2Node_FunctionResult>()) return FString();
		if (N->GetClass()->GetName().Contains(TEXT("Tunnel"))) return FString();
		Selection.Add(N);
		SumX += N->NodePosX; SumY += N->NodePosY;
		MinX = FMath::Min(MinX, N->NodePosX); MinY = FMath::Min(MinY, N->NodePosY);
		MaxX = FMath::Max(MaxX, N->NodePosX); MaxY = FMath::Max(MaxY, N->NodePosY);
	}
	if (Selection.Num() == 0) return FString();

	// Create the new function graph.
	const FName BaseName = FBlueprintEditorUtils::FindUniqueKismetName(
		BP, NewFunctionName.IsEmpty() ? TEXT("ExtractedFunction") : NewFunctionName);
	UEdGraph* DestGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, BaseName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!DestGraph) return FString();
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, DestGraph, /*bIsUserCreated*/true, nullptr);

	// Fetch the auto-created entry; result is created on demand below.
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* N : DestGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) { EntryNode = E; break; }
	}
	if (!EntryNode) return FString();

	FGraphNodeCreator<UK2Node_FunctionResult> ResultNodeCreator(*DestGraph);
	UK2Node_FunctionResult* ResultNode = ResultNodeCreator.CreateNode();
	ResultNode->NodePosX = EntryNode->NodePosX + 300;
	ResultNode->NodePosY = EntryNode->NodePosY;
	ResultNodeCreator.Finalize();

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

	// Spawn the gateway CallFunction node in the source graph. We set it up
	// against the skeleton's new UFunction after moving nodes (the function
	// shape depends on the boundary pins we discover below).
	UK2Node_CallFunction* Gateway = NewObject<UK2Node_CallFunction>(SourceGraph);
	Gateway->CreateNewGuid();
	const int32 CenterX = Selection.Num() > 0 ? (SumX / Selection.Num()) : 0;
	const int32 CenterY = Selection.Num() > 0 ? (SumY / Selection.Num()) : 0;
	Gateway->NodePosX = CenterX;
	Gateway->NodePosY = CenterY;
	SourceGraph->AddNode(Gateway, false, false);

	// Point the gateway at the skeleton's freshly-created function so it can
	// materialise default exec pins. AddFunctionGraph registers the skeleton
	// signature immediately — FindFunctionByName should succeed.
	UClass* SkelClass = BP->SkeletonGeneratedClass ? BP->SkeletonGeneratedClass : BP->GeneratedClass;
	UFunction* SkelFn = SkelClass ? SkelClass->FindFunctionByName(BaseName) : nullptr;
	if (SkelFn)
	{
		Gateway->SetFromFunction(SkelFn);
	}
	Gateway->PostPlacedNewNode();
	Gateway->AllocateDefaultPins();

	// Move the nodes and collect boundary pins in the process.
	TArray<UEdGraphPin*> GatewayPins;  // pins on the *moved* nodes that cross the boundary
	SourceGraph->Modify();
	DestGraph->Modify();
	BP->Modify();

	for (UEdGraphNode* N : Selection)
	{
		N->Modify();
		SourceGraph->Nodes.Remove(N);
		DestGraph->Nodes.Add(N);
		N->Rename(nullptr, DestGraph);

		for (UEdGraphPin* P : N->Pins)
		{
			if (!P || P->LinkedTo.Num() == 0) continue;
			bool bCrosses = false;
			for (UEdGraphPin* Linked : P->LinkedTo)
			{
				if (!Linked) continue;
				if (!Selection.Contains(Linked->GetOwningNode())) { bCrosses = true; break; }
			}
			if (bCrosses) GatewayPins.Add(P);
		}
	}
	SortPinsByPosition(GatewayPins);

	bool bDiscardResult = true;

	// Thunk each boundary pin through entry/result + gateway.
	for (UEdGraphPin* LocalPin : GatewayPins)
	{
		UK2Node_EditablePinBase* LocalPort = (LocalPin->Direction == EGPD_Input) ? (UK2Node_EditablePinBase*)EntryNode : (UK2Node_EditablePinBase*)ResultNode;
		UEdGraphPin* LocalPortPin = nullptr;
		UEdGraphPin* RemotePortPin = nullptr;

		const bool bIsExec = (LocalPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		if (bIsExec)
		{
			// Functions have fixed exec in/out. For an input exec on a moved
			// node (consumed from the outside), we want Entry.exec-out on the
			// inside and Gateway.exec-in on the outside — directions mirror.
			LocalPortPin = K2->FindExecutionPin(*LocalPort,
				(LocalPin->Direction == EGPD_Input) ? EGPD_Output : EGPD_Input);
			RemotePortPin = K2->FindExecutionPin(*Gateway, LocalPin->Direction);
		}
		else
		{
			const FName UniquePortName = Gateway->CreateUniquePinName(LocalPin->PinName);
			FEdGraphPinType PinType = LocalPin->PinType;
			if (PinType.bIsWeakPointer && !PinType.IsContainer()) PinType.bIsWeakPointer = false;
			RemotePortPin = Gateway->CreatePin(LocalPin->Direction, PinType, UniquePortName);
			LocalPortPin  = LocalPort->CreateUserDefinedPin(UniquePortName, PinType,
				(LocalPin->Direction == EGPD_Input) ? EGPD_Output : EGPD_Input);
			if (LocalPin->Direction == EGPD_Output) bDiscardResult = false;
		}
		if (!LocalPortPin || !RemotePortPin) continue;

		LocalPin->Modify();
		// Re-route each external link: external↔Gateway, internal↔Entry/Result.
		for (int32 i = LocalPin->LinkedTo.Num() - 1; i >= 0; --i)
		{
			UEdGraphPin* RemotePin = LocalPin->LinkedTo[i];
			if (!RemotePin) continue;
			UEdGraphNode* RemoteOwner = RemotePin->GetOwningNode();
			if (!RemoteOwner) continue;
			if (Selection.Contains(RemoteOwner)) continue;  // purely internal — leave it
			if (RemoteOwner == EntryNode || RemoteOwner == ResultNode) continue;

			RemotePin->Modify();
			RemotePin->LinkedTo.Remove(LocalPin);
			if (RemotePin->GetOwningNode()->GetOuter() == RemotePortPin->GetOwningNode()->GetOuter())
			{
				RemotePin->MakeLinkTo(RemotePortPin);
			}
			if (LocalPort == EntryNode) LocalPortPin->BreakAllPinLinks();
			LocalPin->LinkedTo.Remove(RemotePin);
			LocalPin->MakeLinkTo(LocalPortPin);
		}
	}

	// Ensure the new function has a walkable exec path even if the selection
	// had no exec boundary pins (pure-data extraction).
	if (UEdGraphPin* ResultExecIn = K2->FindExecutionPin(*ResultNode, EGPD_Input))
	{
		if (ResultExecIn->LinkedTo.Num() == 0)
		{
			if (UEdGraphPin* EntryExecOut = K2->FindExecutionPin(*EntryNode, EGPD_Output))
			{
				if (EntryExecOut->LinkedTo.Num() == 0) EntryExecOut->MakeLinkTo(ResultExecIn);
			}
		}
	}

	// Reposition entry / result around the moved nodes.
	if (Selection.Num() > 0)
	{
		EntryNode->NodePosX = MinX - 260;
		EntryNode->NodePosY = CenterY;
		ResultNode->NodePosX = MaxX + 300;
		ResultNode->NodePosY = CenterY;
	}
	if (bDiscardResult && ResultNode->UserDefinedPins.Num() == 0)
	{
		ResultNode->DestroyNode();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	Gateway->ReconstructNode();
	FKismetEditorUtilities::CompileBlueprint(BP);

	OutNewGraphName = DestGraph->GetName();
	return Gateway->NodeGuid.ToString(EGuidFormats::Digits);
}

// ─── Straighten exec rail ──────────────────────────────────────

int32 UUnrealBridgeBlueprintLibrary::StraightenExecChain(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& StartNodeGuid, const FString& StartExecPinName)
{
	using namespace BridgeBPSummaryImpl;
	using namespace BridgeBlueprintGraphWriteImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return 0;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return 0;
	UEdGraphNode* Start = BridgeBPCollapseImpl::FindNodeByGuid(Graph, StartNodeGuid);
	if (!Start) return 0;

	// Find the starting exec-output pin.
	UEdGraphPin* StartPin = nullptr;
	if (!StartExecPinName.IsEmpty())
	{
		StartPin = Start->FindPin(StartExecPinName);
		if (StartPin && (StartPin->Direction != EGPD_Output || StartPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec))
		{
			StartPin = nullptr;
		}
	}
	if (!StartPin)
	{
		for (UEdGraphPin* P : Start->Pins)
		{
			if (P && P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && P->LinkedTo.Num() == 1)
			{
				StartPin = P; break;
			}
		}
	}
	if (!StartPin || StartPin->LinkedTo.Num() != 1) return 0;

	Graph->Modify();
	BP->Modify();

	int32 Adjusted = 0;
	int32 IterCap = 512;
	UEdGraphNode* Current = Start;
	UEdGraphPin* CurrentOutPin = StartPin;

	while (Current && IterCap-- > 0)
	{
		if (!CurrentOutPin || CurrentOutPin->LinkedTo.Num() != 1) break;
		UEdGraphPin* NextInPin = CurrentOutPin->LinkedTo[0];
		if (!NextInPin || NextInPin->LinkedTo.Num() != 1) break;  // merge — stop
		UEdGraphNode* Next = NextInPin->GetOwningNode();
		if (!Next || Next == Current) break;

		// Compute Y offset: align Next's exec-input pin Y to Current's exec-output pin Y.
		// Pin Y = NodePosY + HeaderHeight + (direction-index * PinRowHeight).
		auto PinRowIndex = [](const UEdGraphNode* N, const UEdGraphPin* P) -> int32
		{
			int32 Idx = 0;
			for (const UEdGraphPin* Q : N->Pins)
			{
				if (!Q || Q->bHidden) continue;
				if (Q->Direction != P->Direction) continue;
				if (Q == P) return Idx;
				Idx += 1;
			}
			return 0;
		};
		const int32 OutIdx = PinRowIndex(Current, CurrentOutPin);
		const int32 InIdx  = PinRowIndex(Next, NextInPin);
		const int32 OutPinY = Current->NodePosY + ComputePinLocalY(Current, EGPD_Output, OutIdx);
		const int32 DesiredNextY = OutPinY - ComputePinLocalY(Next, EGPD_Input, InIdx);
		if (Next->NodePosY != DesiredNextY)
		{
			Next->Modify();
			Next->NodePosY = DesiredNextY;
			Adjusted += 1;
		}

		// Pick Next's single exec-output (if any, unambiguous) and continue.
		UEdGraphPin* NextOut = nullptr;
		int32 ExecOutCount = 0;
		for (UEdGraphPin* P : Next->Pins)
		{
			if (!P || P->bHidden) continue;
			if (P->Direction != EGPD_Output) continue;
			if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
			if (P->LinkedTo.Num() == 1) { NextOut = P; ExecOutCount += 1; }
			else if (P->LinkedTo.Num() > 1) { ExecOutCount += 2; break; }
		}
		if (ExecOutCount != 1) break;  // branch — stop

		Current = Next;
		CurrentOutPin = NextOut;
	}

	if (Adjusted > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	return Adjusted;
}

// ─── Comment-box color + node tint ─────────────────────────────

namespace BridgeBPColorImpl
{
	static bool ParseColorOrPreset(const FString& In, FLinearColor& Out)
	{
		if (In.IsEmpty()) return false;
		// Presets first.
		const FString Lower = In.ToLower();
		struct FPreset { const TCHAR* Name; FLinearColor C; };
		static const FPreset Presets[] = {
			{ TEXT("section"),    FLinearColor(0.35f, 0.35f, 0.35f, 1.f) },
			{ TEXT("validation"), FLinearColor(0.90f, 0.75f, 0.10f, 1.f) },
			{ TEXT("danger"),     FLinearColor(0.85f, 0.18f, 0.18f, 1.f) },
			{ TEXT("network"),    FLinearColor(0.52f, 0.25f, 0.80f, 1.f) },
			{ TEXT("ui"),         FLinearColor(0.18f, 0.68f, 0.70f, 1.f) },
			{ TEXT("debug"),      FLinearColor(0.28f, 0.72f, 0.28f, 1.f) },
			{ TEXT("setup"),      FLinearColor(0.22f, 0.48f, 0.82f, 1.f) },
		};
		for (const FPreset& P : Presets)
		{
			if (Lower.Equals(P.Name)) { Out = P.C; return true; }
		}
		// Hex string: #RRGGBB or #RRGGBBAA, with or without leading #.
		FString Hex = In;
		if (Hex.StartsWith(TEXT("#"))) Hex = Hex.RightChop(1);
		if (Hex.Len() != 6 && Hex.Len() != 8) return false;
		auto HexByte = [](TCHAR A, TCHAR B, uint8& Out) -> bool
		{
			auto Nib = [](TCHAR C, uint8& N) -> bool {
				if (C >= '0' && C <= '9') { N = uint8(C - '0'); return true; }
				if (C >= 'a' && C <= 'f') { N = uint8(C - 'a' + 10); return true; }
				if (C >= 'A' && C <= 'F') { N = uint8(C - 'A' + 10); return true; }
				return false;
			};
			uint8 Hi = 0, Lo = 0;
			if (!Nib(A, Hi) || !Nib(B, Lo)) return false;
			Out = (Hi << 4) | Lo;
			return true;
		};
		uint8 R=0, G=0, B=0, A=255;
		if (!HexByte(Hex[0], Hex[1], R)) return false;
		if (!HexByte(Hex[2], Hex[3], G)) return false;
		if (!HexByte(Hex[4], Hex[5], B)) return false;
		if (Hex.Len() == 8 && !HexByte(Hex[6], Hex[7], A)) return false;
		Out = FLinearColor(FColor(R, G, B, A));
		return true;
	}
}

bool UUnrealBridgeBlueprintLibrary::SetCommentBoxColor(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& ColorOrPreset)
{
	using namespace BridgeBlueprintGraphWriteImpl;
	using namespace BridgeBPColorImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = BridgeBPCollapseImpl::FindNodeByGuid(Graph, NodeGuid);
	UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
	if (!Comment) return false;

	FLinearColor C;
	if (!ParseColorOrPreset(ColorOrPreset, C)) return false;

	Comment->Modify();
	Comment->CommentColor = C;
	Comment->bColorCommentBubble = true;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

bool UUnrealBridgeBlueprintLibrary::SetNodeColor(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& ColorOrPreset)
{
	using namespace BridgeBlueprintGraphWriteImpl;
	using namespace BridgeBPColorImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = BridgeBPCollapseImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	if (ColorOrPreset.IsEmpty())
	{
		// Clear: UE 5.x uses NodeColorMetaData on K2Nodes. Using node-title-color
		// instead is hacky; easiest path is to clear bHasCustomNodeColor via meta.
		Node->SetEnabledState(Node->GetDesiredEnabledState());  // no-op; just Modify + mark
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		return true;
	}
	FLinearColor C;
	if (!ParseColorOrPreset(ColorOrPreset, C)) return false;

	// K2Node titles draw using GetNodeTitleColor; individual override isn't
	// exposed as a property on UEdGraphNode. The cleanest user-visible hook is
	// setting a node comment with that color via bColorCommentBubble.
	Node->NodeComment = ColorOrPreset.StartsWith(TEXT("#")) ? ColorOrPreset : ColorOrPreset;
	Node->bCommentBubbleVisible = true;
	Node->bCommentBubblePinned  = true;
	// Fall through: without engine-level tint, emit a coloured comment bubble
	// on the node so agents still have a visible "this is UI/Network/..." cue.
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── Auto-insert reroute knots for crossing wires ──────────────

namespace BridgeBPRerouteImpl
{
	using namespace BridgeBPSummaryImpl;

	struct FNodeBox
	{
		UEdGraphNode* Node = nullptr;
		int32 X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;
	};

	static FNodeBox BoxOf(UEdGraphNode* N)
	{
		FNodeBox B;
		B.Node = N;
		int32 W = 0, H = 0;
		EstimateNodeSize(N, W, H);
		if (N->NodeWidth  > 0) W = N->NodeWidth;
		if (N->NodeHeight > 0) H = N->NodeHeight;
		B.X0 = N->NodePosX;
		B.Y0 = N->NodePosY;
		B.X1 = N->NodePosX + W;
		B.Y1 = N->NodePosY + H;
		return B;
	}

	/** Segment (x0,y0)→(x1,y1) intersects rect? Simple AABB clip test. */
	static bool SegmentHitsBox(int32 x0, int32 y0, int32 x1, int32 y1, const FNodeBox& B)
	{
		// Broad reject.
		const int32 SMinX = FMath::Min(x0, x1);
		const int32 SMaxX = FMath::Max(x0, x1);
		const int32 SMinY = FMath::Min(y0, y1);
		const int32 SMaxY = FMath::Max(y0, y1);
		if (SMaxX < B.X0 || SMinX > B.X1) return false;
		if (SMaxY < B.Y0 || SMinY > B.Y1) return false;

		// Liang-Barsky-ish: test full line against box.
		const float dx = float(x1 - x0);
		const float dy = float(y1 - y0);
		float t0 = 0.f, t1 = 1.f;
		auto Clip = [&](float p, float q) -> bool
		{
			if (FMath::IsNearlyZero(p))
			{
				return q >= 0.f;  // parallel — inside if non-negative
			}
			const float t = q / p;
			if (p < 0.f)      { if (t > t1) return false; if (t > t0) t0 = t; }
			else              { if (t < t0) return false; if (t < t1) t1 = t; }
			return true;
		};
		if (!Clip(-dx, float(x0 - B.X0))) return false;
		if (!Clip( dx, float(B.X1 - x0))) return false;
		if (!Clip(-dy, float(y0 - B.Y0))) return false;
		if (!Clip( dy, float(B.Y1 - y0))) return false;
		return t0 < t1;
	}
}

int32 UUnrealBridgeBlueprintLibrary::AutoInsertReroutes(
	const FString& BlueprintPath, const FString& GraphName)
{
	using namespace BridgeBlueprintGraphWriteImpl;
	using namespace BridgeBPRerouteImpl;
	using namespace BridgeBPSummaryImpl;

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return 0;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return 0;

	// Snapshot boxes first (we'll mutate the graph).
	TArray<FNodeBox> Boxes;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N) continue;
		if (N->IsA<UEdGraphNode_Comment>()) continue;
		if (N->IsA<UK2Node_Knot>()) continue;
		Boxes.Add(BoxOf(N));
	}

	// Snapshot wires before mutating. Each wire = (src-node, src-pin-name,
	// dst-node, dst-pin-name) so we can resolve after mutation.
	struct FWire { UEdGraphNode* SrcNode; FName SrcPin; UEdGraphNode* DstNode; FName DstPin; };
	TArray<FWire> Wires;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N || N->IsA<UK2Node_Knot>()) continue;
		for (UEdGraphPin* P : N->Pins)
		{
			if (!P || P->Direction != EGPD_Output) continue;
			for (UEdGraphPin* Linked : P->LinkedTo)
			{
				if (!Linked) continue;
				UEdGraphNode* DstNode = Linked->GetOwningNode();
				if (!DstNode || DstNode->IsA<UK2Node_Knot>()) continue;
				FWire W; W.SrcNode = N; W.SrcPin = P->PinName; W.DstNode = DstNode; W.DstPin = Linked->PinName;
				Wires.Add(W);
			}
		}
	}

	Graph->Modify();
	BP->Modify();

	int32 KnotsInserted = 0;
	for (const FWire& W : Wires)
	{
		UEdGraphPin* SrcPin = W.SrcNode->FindPin(W.SrcPin);
		UEdGraphPin* DstPin = W.DstNode->FindPin(W.DstPin);
		if (!SrcPin || !DstPin) continue;

		// Current endpoint positions (pin Y = node Y + compact-aware local Y).
		auto PinLocalY = [](const UEdGraphNode* N, const UEdGraphPin* P) -> int32
		{
			int32 Idx = 0;
			for (const UEdGraphPin* Q : N->Pins)
			{
				if (!Q || Q->bHidden) continue;
				if (Q->Direction != P->Direction) continue;
				if (Q == P) break;
				Idx += 1;
			}
			return ComputePinLocalY(N, P->Direction, Idx);
		};
		int32 SrcW = 0, SrcH = 0, DstW = 0, DstH = 0;
		EstimateNodeSize(W.SrcNode, SrcW, SrcH);
		EstimateNodeSize(W.DstNode, DstW, DstH);
		if (W.SrcNode->NodeWidth  > 0) SrcW = W.SrcNode->NodeWidth;
		if (W.DstNode->NodeWidth  > 0) DstW = W.DstNode->NodeWidth;

		const int32 SX = W.SrcNode->NodePosX + SrcW;
		const int32 SY = W.SrcNode->NodePosY + PinLocalY(W.SrcNode, SrcPin);
		const int32 DX = W.DstNode->NodePosX;
		const int32 DY = W.DstNode->NodePosY + PinLocalY(W.DstNode, DstPin);

		// Is any non-endpoint node's box intersected by this line?
		const FNodeBox* Blocker = nullptr;
		for (const FNodeBox& Box : Boxes)
		{
			if (Box.Node == W.SrcNode || Box.Node == W.DstNode) continue;
			if (SegmentHitsBox(SX, SY, DX, DY, Box))
			{
				Blocker = &Box; break;
			}
		}
		if (!Blocker) continue;

		// Insert a knot at X just past the blocker's right edge, Y a bit above
		// the blocker's top (keeps the wire clear). Break original link, route
		// src→knot→dst.
		const int32 KnotX = (Blocker->X1 + 40);
		const int32 KnotY = Blocker->Y0 - 40;

		UK2Node_Knot* Knot = NewObject<UK2Node_Knot>(Graph);
		Knot->CreateNewGuid();
		Knot->NodePosX = KnotX;
		Knot->NodePosY = KnotY;
		Graph->AddNode(Knot, false, false);
		Knot->PostPlacedNewNode();
		Knot->AllocateDefaultPins();

		UEdGraphPin* KnotIn  = Knot->GetInputPin();
		UEdGraphPin* KnotOut = Knot->GetOutputPin();
		if (!KnotIn || !KnotOut) { Knot->DestroyNode(); continue; }

		SrcPin->BreakLinkTo(DstPin);
		SrcPin->MakeLinkTo(KnotIn);
		KnotOut->MakeLinkTo(DstPin);
		KnotsInserted += 1;
	}

	if (KnotsInserted > 0) FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return KnotsInserted;
}

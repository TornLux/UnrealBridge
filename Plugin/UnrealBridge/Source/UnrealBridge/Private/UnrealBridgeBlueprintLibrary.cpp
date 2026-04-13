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
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/Breakpoint.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"

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
	Comment->NodeComment = Text;
	Graph->AddNode(Comment, /*bFromUI*/false, /*bSelectNewNode*/false);
	Comment->PostPlacedNewNode();

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

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
#include "K2Node_Timeline.h"
#include "GameFramework/Actor.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

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

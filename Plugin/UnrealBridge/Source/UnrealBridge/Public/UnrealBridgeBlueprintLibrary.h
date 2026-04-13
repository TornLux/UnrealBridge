#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeBlueprintLibrary.generated.h"

// ─── Structs ─────────────────────────────────────────────────

/** Single entry in a class hierarchy chain. */
USTRUCT(BlueprintType)
struct FBridgeClassInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	UPROPERTY(BlueprintReadOnly)
	FString ClassPath;

	UPROPERTY(BlueprintReadOnly)
	bool bIsNative = false;
};

/** Describes a single variable defined in a Blueprint. */
USTRUCT(BlueprintType)
struct FBridgeVariableInfo
{
	GENERATED_BODY()

	/** Variable name */
	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Type as displayed in the editor (e.g. "Float", "Vector", "MyStruct", "Array of Int") */
	UPROPERTY(BlueprintReadOnly)
	FString Type;

	/** Category assigned in the Blueprint editor */
	UPROPERTY(BlueprintReadOnly)
	FString Category;

	/** Whether this variable is marked Instance Editable (visible in Details panel) */
	UPROPERTY(BlueprintReadOnly)
	bool bInstanceEditable = false;

	/** Whether this variable is marked Blueprint Read Only */
	UPROPERTY(BlueprintReadOnly)
	bool bBlueprintReadOnly = false;

	/** Default value as string (best-effort serialization) */
	UPROPERTY(BlueprintReadOnly)
	FString DefaultValue;

	/** Tooltip / description set in the editor */
	UPROPERTY(BlueprintReadOnly)
	FString Description;

	/** The replication condition (None, Replicated, RepNotify) */
	UPROPERTY(BlueprintReadOnly)
	FString ReplicationCondition;
};

/** Describes a single parameter of a function. */
USTRUCT(BlueprintType)
struct FBridgeFunctionParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Type;

	/** True if this is a return / output parameter */
	UPROPERTY(BlueprintReadOnly)
	bool bIsOutput = false;
};

/** Describes a function or event defined in a Blueprint. */
USTRUCT(BlueprintType)
struct FBridgeFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "Function", "Event", "Override" */
	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	/** Access level: "Public", "Protected", "Private" */
	UPROPERTY(BlueprintReadOnly)
	FString Access;

	/** Whether this is a pure function (no exec pin) */
	UPROPERTY(BlueprintReadOnly)
	bool bIsPure = false;

	/** Whether this is a static function */
	UPROPERTY(BlueprintReadOnly)
	bool bIsStatic = false;

	/** Category assigned in editor */
	UPROPERTY(BlueprintReadOnly)
	FString Category;

	/** Input and output parameters */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeFunctionParam> Params;

	/** Description / tooltip */
	UPROPERTY(BlueprintReadOnly)
	FString Description;
};

/** Describes a single component in a Blueprint's component tree. */
USTRUCT(BlueprintType)
struct FBridgeComponentInfo
{
	GENERATED_BODY()

	/** Component variable name */
	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Component class (e.g. "StaticMeshComponent", "CapsuleComponent") */
	UPROPERTY(BlueprintReadOnly)
	FString ComponentClass;

	/** Parent component name (empty for root) */
	UPROPERTY(BlueprintReadOnly)
	FString ParentName;

	/** Whether this is the scene root component */
	UPROPERTY(BlueprintReadOnly)
	bool bIsRoot = false;

	/** Whether this component is inherited from a parent class */
	UPROPERTY(BlueprintReadOnly)
	bool bIsInherited = false;
};

/** Describes an interface implemented by a Blueprint. */
USTRUCT(BlueprintType)
struct FBridgeInterfaceInfo
{
	GENERATED_BODY()

	/** Interface class name (e.g. "BPI_Interactable") */
	UPROPERTY(BlueprintReadOnly)
	FString InterfaceName;

	/** Full class path */
	UPROPERTY(BlueprintReadOnly)
	FString InterfacePath;

	/** Whether this interface is implemented via a Blueprint (vs C++) */
	UPROPERTY(BlueprintReadOnly)
	bool bIsBlueprintImplemented = false;

	/** Function names declared by this interface */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Functions;
};

/** A single call edge: "function A calls function B". */
USTRUCT(BlueprintType)
struct FBridgeCallEdge
{
	GENERATED_BODY()

	/** The function/event being called */
	UPROPERTY(BlueprintReadOnly)
	FString TargetName;

	/** Target class or object (e.g. "KismetMathLibrary", "Self", "OtherActor") */
	UPROPERTY(BlueprintReadOnly)
	FString TargetClass;

	/** Whether the target is a function, event, or macro */
	UPROPERTY(BlueprintReadOnly)
	FString TargetKind;
};

/** A single node in a Blueprint function graph. */
USTRUCT(BlueprintType)
struct FBridgeNodeInfo
{
	GENERATED_BODY()

	/** Node title as shown in the editor (e.g. "Branch", "Print String", "Set Timer by Event") */
	UPROPERTY(BlueprintReadOnly)
	FString Title;

	/** Node type category: "FunctionCall", "VariableGet", "VariableSet", "Branch", "ForEach",
	    "Cast", "Event", "Macro", "Spawn", "Timeline", "Knot", "Other" */
	UPROPERTY(BlueprintReadOnly)
	FString NodeType;

	/** For function calls: the target class (e.g. "KismetSystemLibrary") */
	UPROPERTY(BlueprintReadOnly)
	FString TargetClass;

	/** For variable nodes: the variable name */
	UPROPERTY(BlueprintReadOnly)
	FString VariableName;

	/** Node comment if any */
	UPROPERTY(BlueprintReadOnly)
	FString Comment;
};

// ─── Overview structs ───────────────────────────────────────

/** Compact variable entry for blueprint overview. */
USTRUCT(BlueprintType)
struct FBridgeVariableSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Type;
};

/** Compact function entry for blueprint overview. */
USTRUCT(BlueprintType)
struct FBridgeFunctionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "Function", "Event", "Override" */
	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	/** Compact signature, e.g. "(Int, Float) -> Bool" */
	UPROPERTY(BlueprintReadOnly)
	FString Signature;
};

/** Compact component entry for blueprint overview. */
USTRUCT(BlueprintType)
struct FBridgeComponentSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString ComponentClass;

	UPROPERTY(BlueprintReadOnly)
	FString ParentName;
};

/** Full blueprint overview — one call replaces Variables + Functions + Components + Interfaces. */
USTRUCT(BlueprintType)
struct FBridgeBlueprintOverview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString BlueprintName;

	/** First native ancestor class name: "Actor", "Character", "AnimInstance", "UserWidget", etc. */
	UPROPERTY(BlueprintReadOnly)
	FString BlueprintType;

	UPROPERTY(BlueprintReadOnly)
	FBridgeClassInfo ParentClass;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeVariableSummary> Variables;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeFunctionSummary> Functions;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeComponentSummary> Components;

	/** Interface class names */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Interfaces;

	/** Event dispatcher names */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> EventDispatchers;

	/** All graph names (EventGraph, functions, macros) */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> GraphNames;
};

// ─── Event dispatcher structs ───────────────────────────────

/** Describes an event dispatcher with its parameter signature. */
USTRUCT(BlueprintType)
struct FBridgeEventDispatcherInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Parameters of the dispatcher delegate */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeFunctionParam> Params;
};

// ─── Graph listing structs ──────────────────────────────────

/** Lightweight description of a graph in a Blueprint. */
USTRUCT(BlueprintType)
struct FBridgeGraphInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "EventGraph", "Function", "Macro", "EventDispatcher" */
	UPROPERTY(BlueprintReadOnly)
	FString GraphType;
};

// ─── Execution flow structs ─────────────────────────────────

/** A single outgoing exec-pin connection from an execution step. */
USTRUCT(BlueprintType)
struct FBridgeExecConnection
{
	GENERATED_BODY()

	/** Exec output pin name (e.g. "then", "True", "False", "Completed") */
	UPROPERTY(BlueprintReadOnly)
	FString PinName;

	/** Index of the target step in the result array (-1 = unconnected) */
	UPROPERTY(BlueprintReadOnly)
	int32 TargetStepIndex = -1;
};

/** A single step in the execution flow of a function graph. */
USTRUCT(BlueprintType)
struct FBridgeExecStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 StepIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	FString NodeTitle;

	UPROPERTY(BlueprintReadOnly)
	FString NodeType;

	/** Extra context: called function, variable name, cast target, etc. */
	UPROPERTY(BlueprintReadOnly)
	FString Detail;

	/** Outgoing exec-pin connections (branching info) */
	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeExecConnection> ExecOutputs;
};

// ─── Pin connection structs ─────────────────────────────────

/** A single pin-to-pin connection between two nodes in a graph. */
USTRUCT(BlueprintType)
struct FBridgePinConnection
{
	GENERATED_BODY()

	/** Source node index (matches GetFunctionNodes order with empty filter) */
	UPROPERTY(BlueprintReadOnly)
	int32 SourceNodeIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	FString SourcePinName;

	/** Target node index */
	UPROPERTY(BlueprintReadOnly)
	int32 TargetNodeIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	FString TargetPinName;

	/** True for exec wires, false for data wires */
	UPROPERTY(BlueprintReadOnly)
	bool bIsExec = false;
};

// ─── Component property value ───────────────────────────────

/** A single non-default property value on a component or CDO. */
USTRUCT(BlueprintType)
struct FBridgePropertyValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Type;

	/** Value as export-text string */
	UPROPERTY(BlueprintReadOnly)
	FString Value;

	UPROPERTY(BlueprintReadOnly)
	FString Category;
};

// ─── Node search result ─────────────────────────────────────

/** A node found by cross-graph search. */
USTRUCT(BlueprintType)
struct FBridgeNodeSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString GraphName;

	/** "EventGraph", "Function", "Macro" */
	UPROPERTY(BlueprintReadOnly)
	FString GraphType;

	UPROPERTY(BlueprintReadOnly)
	FString NodeTitle;

	UPROPERTY(BlueprintReadOnly)
	FString NodeType;

	/** Variable name, function name, cast target, etc. */
	UPROPERTY(BlueprintReadOnly)
	FString Detail;
};

// ─── Timeline structs ───────────────────────────────────────

/** A single track in a Timeline. */
USTRUCT(BlueprintType)
struct FBridgeTimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TrackName;

	/** "Float", "Vector", "LinearColor", "Event" */
	UPROPERTY(BlueprintReadOnly)
	FString TrackType;
};

/** Describes a Timeline component in a Blueprint. */
USTRUCT(BlueprintType)
struct FBridgeTimelineInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	float Length = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bAutoPlay = false;

	UPROPERTY(BlueprintReadOnly)
	bool bLoop = false;

	UPROPERTY(BlueprintReadOnly)
	bool bReplicated = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeTimelineTrack> Tracks;
};

// ─── Function Library ────────────────────────────────────────

UCLASS()
class UNREALBRIDGE_API UUnrealBridgeBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── Class hierarchy ──

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool GetBlueprintParentClass(const FString& BlueprintPath, FBridgeClassInfo& OutParentInfo);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeClassInfo> GetBlueprintClassHierarchy(const FString& BlueprintPath);

	// ── Variables ──

	/** Get all variables defined in a Blueprint (not inherited ones). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeVariableInfo> GetBlueprintVariables(const FString& BlueprintPath, bool bIncludeInherited = false);

	// ── Functions / Events ──

	/** Get all functions and events defined in a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeFunctionInfo> GetBlueprintFunctions(const FString& BlueprintPath, bool bIncludeInherited = false);

	// ── Components ──

	/** Get the component tree of an Actor Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeComponentInfo> GetBlueprintComponents(const FString& BlueprintPath);

	// ── Interfaces ──

	/** Get all interfaces implemented by a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeInterfaceInfo> GetBlueprintInterfaces(const FString& BlueprintPath);

	// ── Graph analysis ──

	/**
	 * Get the call graph of a specific function in a Blueprint.
	 * Returns only the outgoing call edges (what functions/events this function calls).
	 * Lightweight — no node details, just the call relationships.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of the function/event to analyze. Empty string = EventGraph.
	 * @return Array of call edges
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeCallEdge> GetFunctionCallGraph(const FString& BlueprintPath, const FString& FunctionName);

	/**
	 * Get all nodes in a specific function graph.
	 * Can be filtered by node type to reduce output size.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of the function/event. Empty string = EventGraph.
	 * @param NodeTypeFilter Optional filter: "FunctionCall", "VariableGet", "VariableSet",
	 *                       "Branch", "Cast", "Macro", "Event", etc. Empty = all nodes.
	 * @return Array of node info
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeNodeInfo> GetFunctionNodes(const FString& BlueprintPath, const FString& FunctionName, const FString& NodeTypeFilter);

	// ── Overview ──

	/**
	 * Get a compact overview of a Blueprint in a single call.
	 * Replaces separate calls to Variables + Functions + Components + Interfaces.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool GetBlueprintOverview(const FString& BlueprintPath, FBridgeBlueprintOverview& OutOverview);

	// ── Event Dispatchers ──

	/** Get all event dispatchers defined in a Blueprint with their parameter signatures. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeEventDispatcherInfo> GetEventDispatchers(const FString& BlueprintPath);

	// ── Graph listing ──

	/** Get a lightweight list of all graphs (EventGraph, functions, macros, dispatchers). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeGraphInfo> GetGraphNames(const FString& BlueprintPath);

	// ── Execution flow ──

	/**
	 * Walk exec pins to produce an ordered execution flow for a function.
	 * Much more compact than GetFunctionNodes — only includes nodes on exec wires,
	 * with branching info preserved.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of function/event. Empty string = EventGraph.
	 * @return Ordered steps with branching info
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeExecStep> GetFunctionExecutionFlow(const FString& BlueprintPath, const FString& FunctionName);

	// ── Pin connections ──

	/**
	 * Get all pin-to-pin connections in a function graph.
	 * Node indices match the order of GetFunctionNodes(path, funcName, "").
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of function/event. Empty string = EventGraph.
	 * @return All connections (exec + data wires)
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgePinConnection> GetNodePinConnections(const FString& BlueprintPath, const FString& FunctionName);

	// ── Component properties ──

	/**
	 * Get all non-default property values on a specific component.
	 * Only returns properties that differ from the component class CDO.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param ComponentName  Component variable name (from GetBlueprintComponents)
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgePropertyValue> GetComponentPropertyValues(const FString& BlueprintPath, const FString& ComponentName);

	// ── Cross-graph search ──

	/**
	 * Search for nodes across all graphs in a Blueprint.
	 * Matches node title, variable name, or function name against the query.
	 *
	 * @param BlueprintPath   Content path to the Blueprint
	 * @param Query           Search string (matched against title/detail, case-insensitive)
	 * @param NodeTypeFilter  Optional: "FunctionCall", "VariableGet", "VariableSet", etc. Empty = all.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeNodeSearchResult> SearchBlueprintNodes(const FString& BlueprintPath, const FString& Query, const FString& NodeTypeFilter);

	// ── Timelines ──

	/** Get all Timelines defined in a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeTimelineInfo> GetTimelineInfo(const FString& BlueprintPath);

	// ── Write operations ──

	/**
	 * Set the default value of a Blueprint variable.
	 * Use the same export-text format returned by GetBlueprintVariables DefaultValue field.
	 *
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetBlueprintVariableDefault(const FString& BlueprintPath, const FString& VariableName, const FString& Value);

	/**
	 * Set a property value on a Blueprint component template.
	 * Use export-text format for the value (same format as GetComponentPropertyValues).
	 *
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetComponentProperty(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName, const FString& Value);

	/**
	 * Add a new variable to a Blueprint.
	 * Type string: "Bool", "Int", "Float", "Double", "String", "Name", "Text",
	 *              "Vector", "Rotator", "Transform", "LinearColor",
	 *              or a class/struct name for object/struct references.
	 *              Prefix with "Array of " for arrays (e.g. "Array of Float").
	 *
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AddBlueprintVariable(const FString& BlueprintPath, const FString& Name, const FString& TypeString, const FString& DefaultValue);

	/**
	 * Remove a member variable from a Blueprint by name. Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveBlueprintVariable(const FString& BlueprintPath, const FString& VariableName);

	/**
	 * Rename a member variable on a Blueprint. Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RenameBlueprintVariable(const FString& BlueprintPath, const FString& OldName, const FString& NewName);

	/**
	 * Add an interface implementation to a Blueprint. InterfacePath can be a content path
	 * to a Blueprint interface (e.g. "/Game/BPI_Foo") or a native class path
	 * ("/Script/MyModule.UMyInterface"). Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AddBlueprintInterface(const FString& BlueprintPath, const FString& InterfacePath);

	/**
	 * Remove an interface implementation from a Blueprint by class name or path. Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveBlueprintInterface(const FString& BlueprintPath, const FString& InterfaceNameOrPath);

	/**
	 * Add a new component to an Actor Blueprint's SimpleConstructionScript.
	 * ComponentClassPath: native class path (e.g. "/Script/Engine.StaticMeshComponent")
	 * or Blueprint component class path with trailing _C.
	 * ComponentName: variable name for the new component (must be unique within the BP).
	 * ParentComponentName: optional — attach under this component (empty = attach under root, or become root if none).
	 * Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AddBlueprintComponent(const FString& BlueprintPath, const FString& ComponentClassPath, const FString& ComponentName, const FString& ParentComponentName);
};

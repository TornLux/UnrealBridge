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

	/** NodeGuid (digits form, 32-hex); pass to connect_graph_pins / remove_graph_node / etc. */
	UPROPERTY(BlueprintReadOnly)
	FString NodeGuid;
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

/** A single message produced by the Blueprint compiler. */
USTRUCT(BlueprintType)
struct FBridgeCompileMessage
{
	GENERATED_BODY()

	/** "Error" | "Warning" | "Note" | "Info" */
	UPROPERTY(BlueprintReadOnly)
	FString Severity;

	/** Plain-text message with object/node tokens flattened to names. */
	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** NodeGuid (digits) of the first graph node referenced by the message, or "" if none. */
	UPROPERTY(BlueprintReadOnly)
	FString NodeGuid;
};

USTRUCT(BlueprintType)
struct FBridgeBreakpointInfo
{
	GENERATED_BODY()

	/** Graph name the node lives in (e.g. "EventGraph", "MyFunction"). Empty if graph unresolved. */
	UPROPERTY(BlueprintReadOnly)
	FString GraphName;

	/** NodeGuid (digits) of the node the breakpoint is attached to. */
	UPROPERTY(BlueprintReadOnly)
	FString NodeGuid;

	/** User-facing node title — best-effort, may be empty if the node was deleted. */
	UPROPERTY(BlueprintReadOnly)
	FString NodeTitle;

	/** True if the breakpoint is enabled (as requested by the user — ignores single-step transient state). */
	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = false;
};

// ─── Semantic summary structs (understanding layer) ──────────

/**
 * LLM-ready high-level digest of a Blueprint. Replaces the 5-6 round-trips
 * an agent would otherwise need (overview + variables + functions +
 * components + interfaces + dispatchers) with a single call that also
 * adds aggregate stats (variable categories, key referenced classes,
 * referenced assets) not available anywhere else.
 */
USTRUCT(BlueprintType)
struct FBridgeBlueprintSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Path;

	/** Short name of immediate superclass. */
	UPROPERTY(BlueprintReadOnly) FString ParentClass;
	UPROPERTY(BlueprintReadOnly) FString ParentClassPath;

	/** First native ancestor ("Actor", "Character", "UserWidget", etc.). */
	UPROPERTY(BlueprintReadOnly) FString BlueprintType;

	/** Interface class short names. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> Interfaces;

	/** Names of events this BP actually overrides / handles
	 *  (entry nodes found in UbergraphPages + AnimGraph). */
	UPROPERTY(BlueprintReadOnly) TArray<FString> EventsHandled;

	/** Non-internal function names defined on this BP. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> PublicFunctions;

	/** Event dispatcher names declared on this BP. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> EventDispatchers;

	UPROPERTY(BlueprintReadOnly) int32 VariableCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 InstanceEditableCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 ReplicatedVariableCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 FunctionCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 MacroCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 ComponentCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 TimelineCount = 0;

	/** Sum of nodes across every graph (ubergraph + functions + macros). */
	UPROPERTY(BlueprintReadOnly) int32 TotalNodeCount = 0;

	/** Deduped variable categories across all variables. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> VariableCategories;

	/** Most-called external classes (e.g. "KismetSystemLibrary",
	 *  "GameplayStatics"), sorted by call-site frequency, top 10. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> KeyReferencedClasses;

	/** Asset paths referenced by pin defaults + component class refs,
	 *  deduped, top 10. Gives a sense of what content this BP "uses". */
	UPROPERTY(BlueprintReadOnly) TArray<FString> KeyReferencedAssets;
};

/** Per-function semantic digest — pre-formatted exec outline + aggregate
 *  reads/writes/calls/fires. Replaces GetFunctionNodes +
 *  GetFunctionExecutionFlow + GetNodePinConnections + manual assembly. */
USTRUCT(BlueprintType)
struct FBridgeFunctionSemantics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;

	/** "Function" | "Event" | "Override" | "Macro" | "EventGraph". */
	UPROPERTY(BlueprintReadOnly) FString Kind;

	/** "Public" | "Protected" | "Private". */
	UPROPERTY(BlueprintReadOnly) FString Access;

	UPROPERTY(BlueprintReadOnly) bool bIsPure = false;
	UPROPERTY(BlueprintReadOnly) bool bIsOverride = false;

	UPROPERTY(BlueprintReadOnly) TArray<FBridgeFunctionParam> Params;

	/** Indented human-readable outline of the exec flow. Each entry is
	 *  one line, two-space indent per nesting level. e.g.
	 *    "Branch (IsValid(Target))"
	 *    "  True → Call GameplayStatics.ApplyDamage"
	 *    "  False → Log 'no target'" */
	UPROPERTY(BlueprintReadOnly) TArray<FString> ExecOutline;

	/** Variable names read in this function body (deduped). */
	UPROPERTY(BlueprintReadOnly) TArray<FString> ReadsVariables;

	/** Variable names written (Set) in this function body. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> WritesVariables;

	/** Functions called (formatted "ClassName.FuncName", deduped). */
	UPROPERTY(BlueprintReadOnly) TArray<FString> CallsFunctions;

	/** Event dispatchers fired (Call). */
	UPROPERTY(BlueprintReadOnly) TArray<FString> FiresDispatchers;

	/** Classes spawned via SpawnActorFromClass / similar. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> SpawnsClasses;

	UPROPERTY(BlueprintReadOnly) int32 NodeCount = 0;
	UPROPERTY(BlueprintReadOnly) bool bHasLoops = false;
	UPROPERTY(BlueprintReadOnly) bool bHasBranches = false;

	/** Text from UEdGraphNode_Comment boxes inside this graph. */
	UPROPERTY(BlueprintReadOnly) TArray<FString> CommentBlocks;

	/** One-line tooltip / metadata description if any. */
	UPROPERTY(BlueprintReadOnly) FString Description;
};

/** A single reference site surfaced by Find* cross-reference queries. */
USTRUCT(BlueprintType)
struct FBridgeReference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString GraphName;

	/** "EventGraph" | "Function" | "Macro". */
	UPROPERTY(BlueprintReadOnly) FString GraphType;

	/** NodeGuid (digits). */
	UPROPERTY(BlueprintReadOnly) FString NodeGuid;

	UPROPERTY(BlueprintReadOnly) FString NodeTitle;

	/** "read" | "write" | "call" | "bind" | "unbind" | "event". */
	UPROPERTY(BlueprintReadOnly) FString Kind;
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

	// ── Graph node write ops ──
	// Intentionally return minimal values (GUID or bool) to keep round-trip cost low.
	// None auto-compile; call CompileBlueprint once after a batch of graph edits.
	// Callers drive layout via explicit (X, Y) — recommended: 300px column, 150px row spacing.

	/**
	 * Add a Call-Function node to a graph.
	 * @param TargetClassPath  Empty for self (the Blueprint's own generated/parent class); otherwise a class path.
	 * @return node GUID on success, empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddCallFunctionNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TargetClassPath, const FString& FunctionName, int32 NodePosX, int32 NodePosY);

	/**
	 * Add a VariableGet (bIsSet=false) or VariableSet (bIsSet=true) node for a self-member variable.
	 * Variable may be declared on the Blueprint or inherited from a parent.
	 * @return node GUID on success, empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddVariableNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& VariableName, bool bIsSet, int32 NodePosX, int32 NodePosY);

	/**
	 * Connect two pins identified by node GUID + pin name. Uses the K2 schema's TryCreateConnection
	 * so it respects type coercion and exec-link rules.
	 * @return true on success; false when nodes/pins missing or types incompatible.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool ConnectGraphPins(const FString& BlueprintPath, const FString& GraphName,
		const FString& SourceNodeGuid, const FString& SourcePinName,
		const FString& TargetNodeGuid, const FString& TargetPinName);

	/** Remove a node by GUID, breaking all its pin links. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveGraphNode(const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid);

	/** Reposition a node by GUID for tidy layout. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetGraphNodePosition(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, int32 NodePosX, int32 NodePosY);

	/**
	 * Add a K2Node_Event that overrides a parent-class BlueprintImplementableEvent / BlueprintNativeEvent
	 * (e.g. "ReceiveTick", "ReceiveBeginPlay" on AActor). If a matching event already exists on the graph
	 * (including a "ghost" default event), its existing GUID is returned and it is re-enabled/repositioned.
	 * @param ParentClassPath  Empty = BP's ParentClass; otherwise full class path.
	 * @return node GUID on success, empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddEventNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& ParentClassPath, const FString& EventName, int32 NodePosX, int32 NodePosY);

	/**
	 * Set a pin's default (literal) value via the K2 schema. Accepts the same text form the Details
	 * panel uses — e.g. "1.0", "(X=1,Y=0,Z=0)", "true".
	 * @return true on success; false if node/pin missing or the schema rejects the value.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetPinDefaultValue(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& PinName, const FString& NewDefaultValue);

	// ═══ P0 — Control-flow / basic nodes ═══════════════════════════

	/** Add a Branch (If-Then-Else) node. Returns node GUID or "" on failure. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddBranchNode(const FString& BlueprintPath, const FString& GraphName,
		int32 NodePosX, int32 NodePosY);

	/** Add a Sequence node with the given number of Then pins (clamped 2..16). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddSequenceNode(const FString& BlueprintPath, const FString& GraphName,
		int32 PinCount, int32 NodePosX, int32 NodePosY);

	/** Add a DynamicCast node. `bPure` → no exec pins. `TargetClassPath` can be a native or BP class path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddCastNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TargetClassPath, bool bPure, int32 NodePosX, int32 NodePosY);

	/** Add a Self reference node. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddSelfNode(const FString& BlueprintPath, const FString& GraphName,
		int32 NodePosX, int32 NodePosY);

	/** Add a Custom Event node (K2Node_CustomEvent) with the given name. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddCustomEventNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& EventName, int32 NodePosX, int32 NodePosY);

	// ═══ P0 — Function/event graph management ═══════════════════════

	/** Create an empty user-defined function graph (with default entry/return). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool CreateFunctionGraph(const FString& BlueprintPath, const FString& FunctionName);

	/** Remove a user-defined function graph by name. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveFunctionGraph(const FString& BlueprintPath, const FString& FunctionName);

	/** Rename a user-defined function graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RenameFunctionGraph(const FString& BlueprintPath, const FString& OldName, const FString& NewName);

	/**
	 * Add a parameter to a function graph.
	 * `bIsReturn=false` → input pin on FunctionEntry. `bIsReturn=true` → output pin on FunctionResult
	 * (result node auto-created if absent). TypeString uses the same format as AddBlueprintVariable.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AddFunctionParameter(const FString& BlueprintPath, const FString& FunctionName,
		const FString& ParamName, const FString& TypeString, bool bIsReturn);

	/**
	 * Set flags on a function (pure, const, category, access).
	 * AccessSpecifier: "public" | "protected" | "private" (empty = leave unchanged).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetFunctionMetadata(const FString& BlueprintPath, const FString& FunctionName,
		bool bPure, bool bConst, const FString& Category, const FString& AccessSpecifier);

	// ═══ P0 — Event Dispatcher write ops ════════════════════════════

	/** Add a new Event Dispatcher (multicast delegate) with no parameters. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AddEventDispatcher(const FString& BlueprintPath, const FString& DispatcherName);

	/** Remove an Event Dispatcher by name. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveEventDispatcher(const FString& BlueprintPath, const FString& DispatcherName);

	/** Rename an Event Dispatcher. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RenameEventDispatcher(const FString& BlueprintPath, const FString& OldName, const FString& NewName);

	/** Add a Call (Broadcast) node for a self Event Dispatcher. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddDispatcherCallNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& DispatcherName, int32 NodePosX, int32 NodePosY);

	/** Add a Bind/Unbind node for a self Event Dispatcher. `bUnbind=true` → unbind. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddDispatcherBindNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& DispatcherName, bool bUnbind, int32 NodePosX, int32 NodePosY);

	// NOTE: for "assign custom event to dispatcher", use AddCustomEventNode and connect
	// its OutputDelegate to AddDispatcherBindNode's event pin manually.

	// ═══ P0 — Interface override ════════════════════════════════════

	/**
	 * Materialize an interface function as an editable graph on this Blueprint.
	 * No-op (returns true) if the function is already implemented or is an event-type member.
	 * The interface must already be added via AddBlueprintInterface.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool ImplementInterfaceFunction(const FString& BlueprintPath,
		const FString& InterfacePath, const FString& FunctionName);

	/** Add a K2Node_Message ("Call Function (Message)") for an interface method. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddInterfaceMessageNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& InterfacePath, const FString& FunctionName, int32 NodePosX, int32 NodePosY);

	// ═══ P0 — Variable metadata / type ══════════════════════════════

	/**
	 * Set common flags on a Blueprint member variable.
	 * ReplicationMode: ""|"None"|"Replicated"|"RepNotify" (empty = leave unchanged).
	 * Empty Category/Tooltip strings leave existing values untouched; pass " " (single space) to clear.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetVariableMetadata(const FString& BlueprintPath, const FString& VariableName,
		bool bInstanceEditable, bool bBlueprintReadOnly, bool bExposeOnSpawn, bool bPrivate,
		const FString& Category, const FString& Tooltip, const FString& ReplicationMode);

	/** Change the type (and container kind) of an existing member variable. Uses AddBlueprintVariable's type syntax. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetVariableType(const FString& BlueprintPath, const FString& VariableName,
		const FString& NewTypeString);

	// ═══ P0 — Compile feedback ══════════════════════════════════════

	/** Compile and return all messages (errors + warnings + notes). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeCompileMessage> GetCompileErrors(const FString& BlueprintPath);

	// ═══ P1 — Control-flow: loops / select / literal ════════════════

	/** Insert a ForEachLoop (or ForEachLoopWithBreak if bWithBreak) macro node. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddForeachNode(const FString& BlueprintPath, const FString& GraphName,
		bool bWithBreak, int32 X, int32 Y);

	/** Insert a ForLoop (or ForLoopWithBreak if bWithBreak) macro node. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddForLoopNode(const FString& BlueprintPath, const FString& GraphName,
		bool bWithBreak, int32 X, int32 Y);

	/** Insert a WhileLoop macro node. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddWhileLoopNode(const FString& BlueprintPath, const FString& GraphName,
		int32 X, int32 Y);

	/** Insert a Select node. Wildcard by default; discriminator/options wire up via pin connections. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddSelectNode(const FString& BlueprintPath, const FString& GraphName,
		int32 X, int32 Y);

	/** Insert a Make Literal <Type> call node. TypeString: Int|Int64|Float|Double|Bool|Byte|Name|String|Text. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddMakeLiteralNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TypeString, const FString& Value, int32 X, int32 Y);

	// ═══ P1 — Graph layout ═════════════════════════════════════════

	/**
	 * Align / distribute nodes. Axis: Left, Right, Top, Bottom, CenterHorizontal,
	 * CenterVertical, DistributeHorizontal, DistributeVertical.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AlignNodes(const FString& BlueprintPath, const FString& GraphName,
		const TArray<FString>& NodeGuids, const FString& Axis);

	/** Add a comment box wrapping the provided node guids (pass empty to just position). Returns new comment GUID. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddCommentBox(const FString& BlueprintPath, const FString& GraphName,
		const TArray<FString>& NodeGuids, const FString& Text,
		int32 X, int32 Y, int32 Width, int32 Height);

	/** Insert a reroute (knot) node. Caller wires pins afterwards via ConnectGraphPins. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddRerouteNode(const FString& BlueprintPath, const FString& GraphName,
		int32 X, int32 Y);

	/** State: Enabled | Disabled | DevelopmentOnly. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetNodeEnabled(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& EnabledState);

	// ═══ P1 — Class settings ═══════════════════════════════════════

	/** Change a Blueprint's parent class. Recompiles. HIGH RISK — may discard incompatible components/vars. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool ReparentBlueprint(const FString& BlueprintPath, const FString& NewParentPath);

	/** Set BP display name / description / category / namespace. Empty string = leave unchanged. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetBlueprintMetadata(const FString& BlueprintPath,
		const FString& DisplayName, const FString& Description,
		const FString& Category, const FString& Namespace);

	// ═══ P1 — Component tree ═══════════════════════════════════════

	/** Move a component under a new parent (empty NewParentName = make it a scene root). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool ReparentComponent(const FString& BlueprintPath,
		const FString& ComponentName, const FString& NewParentName);

	/** Reorder a component within its current parent's child list (or root list). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool ReorderComponent(const FString& BlueprintPath,
		const FString& ComponentName, int32 NewIndex);

	/** Remove a component node from the SCS. Children are also removed (use carefully). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveComponent(const FString& BlueprintPath, const FString& ComponentName);

	// ═══ P1 — Dispatcher event node ════════════════════════════════

	/**
	 * Create a CustomEvent whose signature matches the dispatcher's. Caller still needs
	 * an AddDelegate node + a Self-typed target pin to actually bind it.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddDispatcherEventNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& DispatcherName, int32 X, int32 Y);

	// ═══ P2 — CallFunction convenience wrappers ═════════════════════

	/** Insert a Delay latent node (KismetSystemLibrary::Delay) with Duration pin default. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddDelayNode(const FString& BlueprintPath, const FString& GraphName,
		float DurationSeconds, int32 X, int32 Y);

	/** Insert a "Set Timer by Function Name" node (K2_SetTimer) with pin defaults set. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddSetTimerByFunctionNameNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& FunctionName, float TimeSeconds, bool bLooping, int32 X, int32 Y);

	/** Insert a SpawnActorFromClass node with its Class pin defaulted to the given actor class. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddSpawnActorFromClassNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& ActorClassPath, int32 X, int32 Y);

	// ═══ P2 — Struct Make / Break ═══════════════════════════════════

	/** Insert a MakeStruct node for the given UScriptStruct path (e.g. /Script/CoreUObject.Vector). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddMakeStructNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& StructPath, int32 X, int32 Y);

	/** Insert a BreakStruct node for the given UScriptStruct path. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddBreakStructNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& StructPath, int32 X, int32 Y);

	// ═══ P2 — Graph extras ══════════════════════════════════════════

	/** Create an empty user-defined macro graph. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool CreateMacroGraph(const FString& BlueprintPath, const FString& MacroName);

	/** Create / toggle a debug breakpoint on a node (by GUID). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool AddBreakpoint(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, bool bEnabled);

	// ═══ P2 — Timeline ══════════════════════════════════════════════

	/**
	 * Insert a Timeline node. If TimelineTemplateName is empty, a unique name is chosen.
	 * The node auto-creates a new UTimelineTemplate on the Blueprint if none exists with that name.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddTimelineNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TimelineTemplateName, int32 X, int32 Y);

	/**
	 * Update a timeline's template-level settings (length, auto-play, loop, replicated, ignore-time-dilation).
	 * Pass -1.0 for Length to leave unchanged. Syncs to any existing K2Node_Timeline instance that references this template.
	 * Returns false if the timeline template cannot be found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetTimelineProperties(const FString& BlueprintPath, const FString& TimelineName,
		float Length, bool bAutoPlay, bool bLoop, bool bReplicated, bool bIgnoreTimeDilation);

	// ═══ P2 — Macro / Debug management ══════════════════════════════

	/** Remove a user-defined macro graph by name. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveMacroGraph(const FString& BlueprintPath, const FString& MacroName);

	/** Remove a breakpoint previously set on a node (by GUID). Returns true if a breakpoint was removed. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool RemoveBreakpoint(const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid);

	/** Remove every breakpoint on the Blueprint. Returns the number of breakpoints cleared. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static int32 ClearAllBreakpoints(const FString& BlueprintPath);

	/** Enumerate all breakpoints on the Blueprint — graph name, node GUID, node title, enabled state. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeBreakpointInfo> GetBreakpoints(const FString& BlueprintPath);

	// ═══ P2 — Node utilities ════════════════════════════════════════

	/**
	 * Set the inline comment text shown above a graph node (the "Node Comment" in Details).
	 * Pass empty string to clear. bCommentBubbleVisible controls whether the bubble is pinned open.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool SetNodeComment(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& Comment, bool bCommentBubbleVisible);

	/**
	 * Duplicate a node in the same graph at (X, Y). The new node gets a fresh GUID and no pin
	 * connections (caller rewires via ConnectGraphPins). Returns new node GUID or "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString DuplicateGraphNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, int32 NodePosX, int32 NodePosY);

	/**
	 * Break every link on a single pin (by node GUID + pin name). Returns true if the pin was
	 * found and any links were broken (false if pin missing or already unlinked).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool DisconnectGraphPin(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& PinName);

	/**
	 * Insert a Make Array node (K2Node_MakeArray) — wildcard element type until a pin is connected.
	 * Returns node GUID on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddMakeArrayNode(const FString& BlueprintPath, const FString& GraphName,
		int32 NodePosX, int32 NodePosY);

	/**
	 * Insert a Literal Enum node (K2Node_EnumLiteral) for the given UEnum asset/native path.
	 * EnumPath: e.g. "/Script/Engine.EComponentMobility" or a user-defined enum path.
	 * ValueName: the short enum entry name (e.g. "Static"). Leave empty to use the first entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static FString AddEnumLiteralNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& EnumPath, const FString& ValueName, int32 NodePosX, int32 NodePosY);

	// ─── Understanding / semantic summary ──────────────────

	/**
	 * One-call high-level digest of a Blueprint. Aggregates parent class,
	 * interfaces, events handled, public functions, dispatchers, variable
	 * stats, component / timeline / macro counts, total node count, the
	 * variable categories in use, the most-called external classes, and the
	 * top referenced assets. Replaces 5-6 separate queries with ~500 bytes
	 * of structured output — the first thing an agent should call to "read"
	 * an unfamiliar BP.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool GetBlueprintSummary(const FString& BlueprintPath, FBridgeBlueprintSummary& OutSummary);

	/**
	 * Per-function / per-event semantic digest. Walks the graph's exec chain
	 * from its entry node to produce an indented human-readable outline,
	 * plus aggregates: variables read/written, functions called, dispatchers
	 * fired, classes spawned, loop/branch flags, comment text. For event
	 * graphs, pass the event name (e.g. "ReceiveBeginPlay"); for functions,
	 * pass the function name; for macros, the macro name.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static bool GetFunctionSummary(const FString& BlueprintPath,
		const FString& FunctionName, FBridgeFunctionSemantics& OutSemantics);

	/**
	 * All sites (graph + node guid) where `VariableName` is read or written
	 * in this Blueprint. Walks UbergraphPages + FunctionGraphs + MacroGraphs.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeReference> FindVariableReferences(
		const FString& BlueprintPath, const FString& VariableName);

	/**
	 * All sites where `FunctionName` is called in this Blueprint.
	 * Matches any CallFunction node whose target function's name equals
	 * `FunctionName`, regardless of owning class.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeReference> FindFunctionCallSites(
		const FString& BlueprintPath, const FString& FunctionName);

	/**
	 * All event-handler / dispatcher bind sites matching `EventName`.
	 * Covers K2Node_Event / K2Node_CustomEvent entries (kind="event"),
	 * K2Node_CallDelegate (kind="call"), K2Node_AddDelegate (kind="bind"),
	 * K2Node_RemoveDelegate (kind="unbind").
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Blueprint")
	static TArray<FBridgeReference> FindEventHandlerSites(
		const FString& BlueprintPath, const FString& EventName);
};

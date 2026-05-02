#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeChooserLibrary.generated.h"

USTRUCT(BlueprintType)
struct FBridgeCHTColumn
{
	GENERATED_BODY()

	/** Column struct short name, e.g. "FloatRangeColumn", "EnumColumn". */
	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	/** Property binding chain joined by "." — empty when bound to root or unbound. */
	UPROPERTY(BlueprintReadOnly)
	FString BindingPath;

	/** Editor display name from the binding (often friendlier than BindingPath). */
	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	bool bDisabled = false;

	/** True when the column produces outputs into the eval context (Output* family). */
	UPROPERTY(BlueprintReadOnly)
	bool bIsOutput = false;
};

USTRUCT(BlueprintType)
struct FBridgeCHTRowResult
{
	GENERATED_BODY()

	/** "Asset" / "Class" / "NestedChooser" / "EvaluateChooser" / "None" / "<Other>". */
	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	/** Asset / class / sub-chooser path, "" when none/unsupported. */
	UPROPERTY(BlueprintReadOnly)
	FString ResultPath;
};

USTRUCT(BlueprintType)
struct FBridgeCHTRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bDisabled = false;

	UPROPERTY(BlueprintReadOnly)
	FBridgeCHTRowResult Result;
};

USTRUCT(BlueprintType)
struct FBridgeCHTInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 RowCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ColumnCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NestedChooserCount = 0;

	/** Full path of the OutputObjectType class (UClass*). */
	UPROPERTY(BlueprintReadOnly)
	FString OutputClassPath;

	/** "ObjectResult" / "ClassResult". */
	UPROPERTY(BlueprintReadOnly)
	FString ResultType;

	/** True when FallbackResult is set (used when no row passes filters). */
	UPROPERTY(BlueprintReadOnly)
	bool bHasFallback = false;

	UPROPERTY(BlueprintReadOnly)
	FBridgeCHTRowResult Fallback;
};

USTRUCT(BlueprintType)
struct FBridgeCHTEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly)
	FString ResultPath;

	UPROPERTY(BlueprintReadOnly)
	FString ResultKind;

	/** -1 if nothing matched (fallback was used or null returned). */
	UPROPERTY(BlueprintReadOnly)
	int32 MatchedRow = -1;

	UPROPERTY(BlueprintReadOnly)
	bool bUsedFallback = false;
};

UCLASS()
class UNREALBRIDGE_API UUnrealBridgeChooserLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── Read ──

	/** High-level summary of a ChooserTable: row/column counts, output type, fallback. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static FBridgeCHTInfo GetChooserInfo(const FString& ChooserTablePath);

	/** List columns (kind, binding, disabled flag) — top-to-bottom in editor order. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static TArray<FBridgeCHTColumn> ListChooserColumns(const FString& ChooserTablePath);

	/** List rows including disabled flag and resolved result info. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static TArray<FBridgeCHTRow> ListChooserRows(const FString& ChooserTablePath);

	/** Fetch a single row's result (asset / class / nested chooser path). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static FBridgeCHTRowResult GetChooserRowResult(const FString& ChooserTablePath, int32 RowIndex);

	/** Read raw T3D for a single cell (column.RowValues[row]). Empty on failure. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static FString GetChooserCellRaw(const FString& ChooserTablePath, int32 ColumnIndex, int32 RowIndex);

	// ── Row writes ──

	/** Append a new row at the end with default cell values + None result. Returns new row index, -1 on failure. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserRow(const FString& ChooserTablePath);

	/** Insert a row before BeforeRow (== row count to append). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 InsertChooserRow(const FString& ChooserTablePath, int32 BeforeRow);

	/** Remove a row (and its parallel result + disabled flag). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool RemoveChooserRow(const FString& ChooserTablePath, int32 RowIndex);

	/** Toggle a row's disabled flag. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserRowDisabled(const FString& ChooserTablePath, int32 RowIndex, bool bDisabled);

	/** Set a row's result to an asset (FAssetChooser). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserRowResultAsset(const FString& ChooserTablePath, int32 RowIndex, const FString& AssetPath);

	/** Set a row's result to a class (FClassChooser-equivalent). Empty class path clears it. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserRowResultClass(const FString& ChooserTablePath, int32 RowIndex, const FString& ClassPath);

	/** Set a row's result to delegate to another ChooserTable asset (FEvaluateChooser). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserRowResultEvaluateChooser(const FString& ChooserTablePath, int32 RowIndex, const FString& SubChooserPath);

	/** Clear a row's result (returns null at runtime; falls back to FallbackResult if set). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool ClearChooserRowResult(const FString& ChooserTablePath, int32 RowIndex);

	// ── Fallback ──

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserFallbackAsset(const FString& ChooserTablePath, const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool ClearChooserFallback(const FString& ChooserTablePath);

	// ── Column writes (typed) ──

	/**
	 * Add a column of the given UScriptStruct type by full path
	 * (e.g. "/Script/Chooser.FloatRangeColumn" or "/Script/Chooser.EnumColumn").
	 * The column's RowValues array is resized to match existing row count.
	 * Returns the new column index, -1 on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnByStructPath(const FString& ChooserTablePath, const FString& ColumnStructPath,
		const FString& BindingPropertyChain, int32 ContextIndex);

	/** Convenience: AddColumn for FFloatRangeColumn. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnFloatRange(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	/**
	 * Convenience: AddColumn for FEnumColumn. Pass enum_path like "/Game/Blueprints/Data/E_Stance.E_Stance"
	 * (UserDefinedEnum) or "/Script/Engine.EMyEnum" (native UEnum).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnEnum(const FString& ChooserTablePath, const FString& BindingPropertyChain, const FString& EnumPath, int32 ContextIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnBool(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnObject(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnGameplayTag(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnRandomize(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnOutputFloat(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static int32 AddChooserColumnOutputObject(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex);

	/** Remove a column. Cell data on every row for this column is dropped. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool RemoveChooserColumn(const FString& ChooserTablePath, int32 ColumnIndex);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserColumnDisabled(const FString& ChooserTablePath, int32 ColumnIndex, bool bDisabled);

	/**
	 * Set a single cell (column.RowValues[row]) by importing T3D text — same format as `unreal.StructName` export.
	 * Example for FloatRange: "(Min=0.0,Max=10.0,bNoMin=False,bNoMax=False)".
	 * Example for Enum:       "(ValueName=\"E_Gait::NewEnumerator2\",Value=2,Comparison=MatchEqual)".
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static bool SetChooserCellRaw(const FString& ChooserTablePath, int32 ColumnIndex, int32 RowIndex, const FString& T3DValue);

	// ── Evaluation ──

	/**
	 * Run the chooser and return the picked result. ContextObjectPath is loaded and added as the single
	 * context UObject — works for choosers that bind via "/Script/Engine.AnimInstance" or similar property bag.
	 * For deeper testing pass a small context UObject instance whose properties match the column bindings.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static FBridgeCHTEvaluation EvaluateChooserWithContextObject(const FString& ChooserTablePath, const FString& ContextObjectPath);

	/**
	 * Multi-result variant. Many AnimBP-driven choosers (e.g. GASP's
	 * `CHT_PoseSearchDatabases`) feed their full output array to a downstream
	 * node — `MotionMatchingAnimNodeLibrary::SetDatabasesToSearch` takes
	 * `Array<PoseSearchDatabase>`, not a single one. The single-result
	 * `EvaluateChooserWithContextObject` only returns the first hit; this
	 * variant collects every row whose filters pass.
	 *
	 * Same context-object semantics: pass an empty path to fire the chooser
	 * with no context (default-only filtering).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static TArray<FBridgeCHTRowResult> EvaluateChooserMultiWithContextObject(const FString& ChooserTablePath, const FString& ContextObjectPath);

	/** List every potential row result + fallback as paths — useful for "what could this CHT possibly return?" without filter inputs. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Chooser")
	static TArray<FBridgeCHTRowResult> ListPossibleResults(const FString& ChooserTablePath);
};

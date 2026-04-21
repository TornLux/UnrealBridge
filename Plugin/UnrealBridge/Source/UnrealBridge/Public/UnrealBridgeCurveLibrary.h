#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeCurveLibrary.generated.h"

/**
 * One key of a rich curve, wire-format for the bridge.
 *
 * Enum fields are strings so Python callers don't have to deal with TEnumAsByte:
 *   InterpMode         — "Linear" / "Constant" / "Cubic" / "None"
 *   TangentMode        — "Auto" / "User" / "Break" / "None" / "SmartAuto"
 *   TangentWeightMode  — "None" / "Arrive" / "Leave" / "Both"
 *
 * Case-insensitive on the way in; writes return the canonical spelling on the
 * way out.
 */
USTRUCT(BlueprintType)
struct FBridgeRichCurveKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	float Time = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	float Value = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	FString InterpMode;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	FString TangentMode;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	FString TangentWeightMode;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	float ArriveTangent = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	float LeaveTangent = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	float ArriveTangentWeight = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "UnrealBridge|Curve")
	float LeaveTangentWeight = 0.f;
};

/** High-level info for one curve asset. */
USTRUCT(BlueprintType)
struct FBridgeCurveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "CurveFloat" / "CurveVector" / "CurveLinearColor" (or the UClass FName for unknown UCurveBase subclasses). */
	UPROPERTY(BlueprintReadOnly)
	FString ClassName;

	/** Channel labels, parallel to NumKeysPerChannel. One entry for UCurveFloat; three for Vector; four for LinearColor. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ChannelNames;

	UPROPERTY(BlueprintReadOnly)
	TArray<int32> NumKeysPerChannel;

	/**
	 * Pre/post-infinity extrapolation, read from channel 0. Rich curves let each channel have its own,
	 * but in practice they're set together from the curve editor, so channel 0 is representative.
	 * Values: "Cycle" / "CycleWithOffset" / "Oscillate" / "Linear" / "Constant" / "None".
	 */
	UPROPERTY(BlueprintReadOnly)
	FString PreInfinityExtrap;

	UPROPERTY(BlueprintReadOnly)
	FString PostInfinityExtrap;
};

/** High-level info for one curve table asset. */
USTRUCT(BlueprintType)
struct FBridgeCurveTableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** "Empty" / "SimpleCurves" / "RichCurves". */
	UPROPERTY(BlueprintReadOnly)
	FString CurveTableMode;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> RowNames;

	/** Key counts parallel to RowNames. */
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> NumKeysPerRow;
};

/**
 * Curve-asset introspection and editing via UnrealBridge.
 *
 * Why this library exists on top of UE's Python reflection:
 *   • Batch Eval — one round-trip for N samples instead of N GameThread bounces.
 *   • UCurveTable row CRUD — SimpleCurves / RichCurves dispatch isn't cleanly exposed in Python.
 *   • Atomic tangent writes — setting TangentMode + weights in one call avoids intermediate
 *     bad-interp states and fires OnCurveChanged() exactly once.
 *   • Curve editor refresh — calls ModifyOwner()/OnCurveChanged() so an open Curve Editor tab
 *     redraws after the edit.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeCurveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ─── Read ─────────────────────────────────────────────────

	/** Identify a curve asset and report per-channel key counts + infinity extrap. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static FBridgeCurveInfo GetCurveInfo(const FString& CurvePath);

	/**
	 * Read all keys from one channel as FBridgeRichCurveKey structs.
	 * ChannelIndex: 0 for UCurveFloat, 0..2 for UCurveVector (X/Y/Z), 0..3 for UCurveLinearColor (R/G/B/A).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static TArray<FBridgeRichCurveKey> GetCurveKeys(const FString& CurvePath, int32 ChannelIndex);

	/**
	 * Return the full curve (all channels, all keys) as a compact JSON string.
	 * Cheaper to serialize + send than a USTRUCT array when you want the whole asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static FString GetCurveAsJSONString(const FString& CurvePath);

	// ─── Write ────────────────────────────────────────────────

	/**
	 * Replace the entire key list on a channel. Keys don't need to be pre-sorted —
	 * they're sorted by Time before SetKeys(). One FScopedTransaction covers the whole change.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static bool SetCurveKeys(
		const FString& CurvePath,
		int32 ChannelIndex,
		const TArray<FBridgeRichCurveKey>& Keys);

	/**
	 * Append one key. Returns the inserted index (0-based, after sort) or -1 on failure.
	 * If a key already exists at the same Time within UE_KINDA_SMALL_NUMBER tolerance,
	 * the existing key is updated and its index returned (matches FRichCurve::UpdateOrAddKey).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static int32 AddCurveKey(
		const FString& CurvePath,
		int32 ChannelIndex,
		const FBridgeRichCurveKey& Key);

	/** Remove the key at the given index. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static bool RemoveCurveKeyByIndex(const FString& CurvePath, int32 ChannelIndex, int32 Index);

	/** Clear every key on a channel. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static bool ClearCurveKeys(const FString& CurvePath, int32 ChannelIndex);

	/**
	 * Atomic tangent write for a single key. All fields are set together under one
	 * FScopedTransaction, so Ctrl+Z restores all of them as a unit.
	 *
	 * Pass "" (empty string) for TangentMode / TangentWeightMode to leave them untouched.
	 * Pass NaN for any of the four tangent scalars to leave them untouched (Python can send
	 * float('nan')).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static bool SetCurveKeyTangents(
		const FString& CurvePath,
		int32 ChannelIndex,
		int32 Index,
		const FString& TangentMode,
		const FString& TangentWeightMode,
		float ArriveTangent,
		float LeaveTangent,
		float ArriveTangentWeight,
		float LeaveTangentWeight);

	/**
	 * Set pre/post-infinity extrapolation. Applies to every channel on the asset
	 * (UCurveVector / UCurveLinearColor have multiple channels — they're all updated).
	 * Pass "" to skip one side.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static bool SetCurveInfinityExtrap(
		const FString& CurvePath,
		const FString& PreInfinityExtrap,
		const FString& PostInfinityExtrap);

	/**
	 * Auto-compute tangents for every Auto-mode key on every channel. Useful after
	 * SetCurveKeys() if the caller didn't precompute cubic tangents.
	 * @param Tension  0.0 = default; negative tightens, positive slackens.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static bool AutoSetCurveTangents(const FString& CurvePath, float Tension);

	// ─── Eval ─────────────────────────────────────────────────

	/**
	 * Batch-evaluate a channel at N times. Single round-trip; returns a float per input time.
	 * On failure returns an empty array.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static TArray<float> EvaluateCurve(
		const FString& CurvePath,
		int32 ChannelIndex,
		const TArray<float>& Times);

	/**
	 * Evaluate a channel at NumSamples evenly spaced times in [StartTime, EndTime] (both inclusive).
	 * For NumSamples <= 1 returns a single sample at StartTime.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Curve")
	static TArray<float> SampleCurveUniform(
		const FString& CurvePath,
		int32 ChannelIndex,
		float StartTime,
		float EndTime,
		int32 NumSamples);

	// ─── Curve Table ──────────────────────────────────────────

	/** Identify a curve-table asset + list row names with per-row key counts. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static FBridgeCurveTableInfo GetCurveTableInfo(const FString& CurveTablePath);

	/**
	 * Read one row's keys as FBridgeRichCurveKey. For SimpleCurves tables, the simple-curve
	 * linear/constant interp is converted into the equivalent RichCurveKey (tangent fields
	 * default to 0, InterpMode mirrors the SimpleCurve's).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static TArray<FBridgeRichCurveKey> GetCurveTableRowKeys(
		const FString& CurveTablePath,
		const FString& RowName);

	/**
	 * Replace the keys on an existing curve-table row. The table must already contain RowName;
	 * this does not create new rows (use AddCurveTableRow for that).
	 * For SimpleCurves tables, tangent fields on the input keys are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static bool SetCurveTableRowKeys(
		const FString& CurveTablePath,
		const FString& RowName,
		const TArray<FBridgeRichCurveKey>& Keys);

	/**
	 * Add a new row. Table must not be in ECurveTableMode::Empty — it must already be in
	 * RichCurves or SimpleCurves mode (set by its first row). For empty tables, RichCurves
	 * is assumed.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static bool AddCurveTableRow(
		const FString& CurveTablePath,
		const FString& RowName,
		const TArray<FBridgeRichCurveKey>& Keys);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static bool RemoveCurveTableRow(const FString& CurveTablePath, const FString& RowName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static bool RenameCurveTableRow(
		const FString& CurveTablePath,
		const FString& OldRowName,
		const FString& NewRowName);

	/** Batch-evaluate one curve-table row at N times. Empty array on failure. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|CurveTable")
	static TArray<float> EvaluateCurveTableRow(
		const FString& CurveTablePath,
		const FString& RowName,
		const TArray<float>& Times);
};

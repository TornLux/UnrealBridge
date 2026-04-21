#include "UnrealBridgeCurveLibrary.h"

#include "Curves/CurveBase.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "Curves/RealCurve.h"
#include "Curves/SimpleCurve.h"
#include "Engine/CurveTable.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeCurve"

// ─── Enum <-> string mapping ────────────────────────────────

namespace BridgeCurveImpl
{
	const TCHAR* InterpModeToStr(ERichCurveInterpMode M)
	{
		switch (M)
		{
		case RCIM_Linear:   return TEXT("Linear");
		case RCIM_Constant: return TEXT("Constant");
		case RCIM_Cubic:    return TEXT("Cubic");
		case RCIM_None:     return TEXT("None");
		default:            return TEXT("None");
		}
	}

	bool InterpModeFromStr(const FString& S, ERichCurveInterpMode& Out)
	{
		if (S.Equals(TEXT("Linear"),   ESearchCase::IgnoreCase)) { Out = RCIM_Linear;   return true; }
		if (S.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) { Out = RCIM_Constant; return true; }
		if (S.Equals(TEXT("Cubic"),    ESearchCase::IgnoreCase)) { Out = RCIM_Cubic;    return true; }
		if (S.Equals(TEXT("None"),     ESearchCase::IgnoreCase)) { Out = RCIM_None;     return true; }
		return false;
	}

	const TCHAR* TangentModeToStr(ERichCurveTangentMode M)
	{
		switch (M)
		{
		case RCTM_Auto:      return TEXT("Auto");
		case RCTM_User:      return TEXT("User");
		case RCTM_Break:     return TEXT("Break");
		case RCTM_None:      return TEXT("None");
		case RCTM_SmartAuto: return TEXT("SmartAuto");
		default:             return TEXT("None");
		}
	}

	bool TangentModeFromStr(const FString& S, ERichCurveTangentMode& Out)
	{
		if (S.Equals(TEXT("Auto"),      ESearchCase::IgnoreCase)) { Out = RCTM_Auto;      return true; }
		if (S.Equals(TEXT("User"),      ESearchCase::IgnoreCase)) { Out = RCTM_User;      return true; }
		if (S.Equals(TEXT("Break"),     ESearchCase::IgnoreCase)) { Out = RCTM_Break;     return true; }
		if (S.Equals(TEXT("None"),      ESearchCase::IgnoreCase)) { Out = RCTM_None;      return true; }
		if (S.Equals(TEXT("SmartAuto"), ESearchCase::IgnoreCase)) { Out = RCTM_SmartAuto; return true; }
		return false;
	}

	const TCHAR* TangentWeightModeToStr(ERichCurveTangentWeightMode M)
	{
		switch (M)
		{
		case RCTWM_WeightedNone:   return TEXT("None");
		case RCTWM_WeightedArrive: return TEXT("Arrive");
		case RCTWM_WeightedLeave:  return TEXT("Leave");
		case RCTWM_WeightedBoth:   return TEXT("Both");
		default:                   return TEXT("None");
		}
	}

	bool TangentWeightModeFromStr(const FString& S, ERichCurveTangentWeightMode& Out)
	{
		if (S.Equals(TEXT("None"),   ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedNone;   return true; }
		if (S.Equals(TEXT("Arrive"), ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedArrive; return true; }
		if (S.Equals(TEXT("Leave"),  ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedLeave;  return true; }
		if (S.Equals(TEXT("Both"),   ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedBoth;   return true; }
		return false;
	}

	const TCHAR* ExtrapToStr(ERichCurveExtrapolation E)
	{
		switch (E)
		{
		case RCCE_Cycle:           return TEXT("Cycle");
		case RCCE_CycleWithOffset: return TEXT("CycleWithOffset");
		case RCCE_Oscillate:       return TEXT("Oscillate");
		case RCCE_Linear:          return TEXT("Linear");
		case RCCE_Constant:        return TEXT("Constant");
		case RCCE_None:            return TEXT("None");
		default:                   return TEXT("None");
		}
	}

	bool ExtrapFromStr(const FString& S, ERichCurveExtrapolation& Out)
	{
		if (S.Equals(TEXT("Cycle"),           ESearchCase::IgnoreCase)) { Out = RCCE_Cycle;           return true; }
		if (S.Equals(TEXT("CycleWithOffset"), ESearchCase::IgnoreCase)) { Out = RCCE_CycleWithOffset; return true; }
		if (S.Equals(TEXT("Oscillate"),       ESearchCase::IgnoreCase)) { Out = RCCE_Oscillate;       return true; }
		if (S.Equals(TEXT("Linear"),          ESearchCase::IgnoreCase)) { Out = RCCE_Linear;          return true; }
		if (S.Equals(TEXT("Constant"),        ESearchCase::IgnoreCase)) { Out = RCCE_Constant;        return true; }
		if (S.Equals(TEXT("None"),            ESearchCase::IgnoreCase)) { Out = RCCE_None;            return true; }
		return false;
	}

	// ─── Channel access on UCurveBase subclasses ────────────

	/**
	 * Returns the channel name for a UCurveBase subclass. Index-in-bounds per class:
	 *   UCurveFloat       — {"Value"}
	 *   UCurveVector      — {"X","Y","Z"}
	 *   UCurveLinearColor — {"R","G","B","A"}
	 */
	TArray<FString> GetChannelNames(const UCurveBase* Curve)
	{
		TArray<FString> Names;
		if (!Curve) return Names;
		if (Curve->IsA<UCurveFloat>())             Names = { TEXT("Value") };
		else if (Curve->IsA<UCurveVector>())       Names = { TEXT("X"), TEXT("Y"), TEXT("Z") };
		else if (Curve->IsA<UCurveLinearColor>())  Names = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
		return Names;
	}

	FRichCurve* GetChannel(UCurveBase* Curve, int32 ChannelIndex)
	{
		if (!Curve) return nullptr;
		if (UCurveFloat* CF = Cast<UCurveFloat>(Curve))
		{
			if (ChannelIndex == 0) return &CF->FloatCurve;
		}
		else if (UCurveVector* CV = Cast<UCurveVector>(Curve))
		{
			if (ChannelIndex >= 0 && ChannelIndex < 3) return &CV->FloatCurves[ChannelIndex];
		}
		else if (UCurveLinearColor* CL = Cast<UCurveLinearColor>(Curve))
		{
			if (ChannelIndex >= 0 && ChannelIndex < 4) return &CL->FloatCurves[ChannelIndex];
		}
		return nullptr;
	}

	int32 GetChannelCount(const UCurveBase* Curve)
	{
		if (!Curve) return 0;
		if (Curve->IsA<UCurveFloat>())             return 1;
		if (Curve->IsA<UCurveVector>())            return 3;
		if (Curve->IsA<UCurveLinearColor>())       return 4;
		return 0;
	}

	/** Collect all channels of a UCurveBase for bulk ops (e.g. SetCurveInfinityExtrap). */
	TArray<FRichCurve*> GetAllChannels(UCurveBase* Curve)
	{
		TArray<FRichCurve*> Out;
		const int32 N = GetChannelCount(Curve);
		for (int32 i = 0; i < N; ++i)
		{
			if (FRichCurve* C = GetChannel(Curve, i)) Out.Add(C);
		}
		return Out;
	}

	// ─── FBridgeRichCurveKey <-> FRichCurveKey ───────────────

	FBridgeRichCurveKey ToBridgeKey(const FRichCurveKey& K)
	{
		FBridgeRichCurveKey Out;
		Out.Time                 = K.Time;
		Out.Value                = K.Value;
		Out.InterpMode           = InterpModeToStr(K.InterpMode);
		Out.TangentMode          = TangentModeToStr(K.TangentMode);
		Out.TangentWeightMode    = TangentWeightModeToStr(K.TangentWeightMode);
		Out.ArriveTangent        = K.ArriveTangent;
		Out.LeaveTangent         = K.LeaveTangent;
		Out.ArriveTangentWeight  = K.ArriveTangentWeight;
		Out.LeaveTangentWeight   = K.LeaveTangentWeight;
		return Out;
	}

	FRichCurveKey FromBridgeKey(const FBridgeRichCurveKey& K)
	{
		FRichCurveKey Out;
		Out.Time  = K.Time;
		Out.Value = K.Value;

		ERichCurveInterpMode Interp = RCIM_Linear;
		if (!K.InterpMode.IsEmpty()) InterpModeFromStr(K.InterpMode, Interp);
		Out.InterpMode = Interp;

		ERichCurveTangentMode Tangent = RCTM_Auto;
		if (!K.TangentMode.IsEmpty()) TangentModeFromStr(K.TangentMode, Tangent);
		Out.TangentMode = Tangent;

		ERichCurveTangentWeightMode WeightMode = RCTWM_WeightedNone;
		if (!K.TangentWeightMode.IsEmpty()) TangentWeightModeFromStr(K.TangentWeightMode, WeightMode);
		Out.TangentWeightMode = WeightMode;

		Out.ArriveTangent        = K.ArriveTangent;
		Out.LeaveTangent         = K.LeaveTangent;
		Out.ArriveTangentWeight  = K.ArriveTangentWeight;
		Out.LeaveTangentWeight   = K.LeaveTangentWeight;
		return Out;
	}

	// ─── Asset loaders ───────────────────────────────────────

	UCurveBase* LoadCurve(const FString& Path)
	{
		UCurveBase* C = LoadObject<UCurveBase>(nullptr, *Path);
		if (!C)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load UCurveBase '%s'"), *Path);
		return C;
	}

	UCurveTable* LoadCT(const FString& Path)
	{
		UCurveTable* T = LoadObject<UCurveTable>(nullptr, *Path);
		if (!T)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load UCurveTable '%s'"), *Path);
		return T;
	}

	/** Broadcast curve-owner notifications so open editors redraw. */
	void NotifyCurveEdited(UCurveBase* Curve)
	{
		if (!Curve) return;
		Curve->ModifyOwner();                 // marks package dirty via FCurveOwnerInterface
		TArray<FRichCurveEditInfo> Changed = Curve->GetCurves();
		Curve->OnCurveChanged(Changed);       // fires refresh on open editors
		Curve->MarkPackageDirty();
	}

	void NotifyCurveTableEdited(UCurveTable* Table)
	{
		if (!Table) return;
		Table->ModifyOwner();
		Table->OnCurveTableChanged().Broadcast();
		Table->MarkPackageDirty();
	}
}

// ─── GetCurveInfo ───────────────────────────────────────────

FBridgeCurveInfo UUnrealBridgeCurveLibrary::GetCurveInfo(const FString& CurvePath)
{
	FBridgeCurveInfo Result;
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Result;

	Result.Name = Curve->GetName();
	Result.ClassName = Curve->GetClass()->GetName();
	Result.ChannelNames = BridgeCurveImpl::GetChannelNames(Curve);

	const int32 N = BridgeCurveImpl::GetChannelCount(Curve);
	Result.NumKeysPerChannel.Reserve(N);
	for (int32 i = 0; i < N; ++i)
	{
		if (FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, i))
			Result.NumKeysPerChannel.Add(Ch->Keys.Num());
		else
			Result.NumKeysPerChannel.Add(0);
	}

	if (FRichCurve* Ch0 = BridgeCurveImpl::GetChannel(Curve, 0))
	{
		Result.PreInfinityExtrap  = BridgeCurveImpl::ExtrapToStr(Ch0->PreInfinityExtrap);
		Result.PostInfinityExtrap = BridgeCurveImpl::ExtrapToStr(Ch0->PostInfinityExtrap);
	}
	return Result;
}

// ─── GetCurveKeys ───────────────────────────────────────────

TArray<FBridgeRichCurveKey> UUnrealBridgeCurveLibrary::GetCurveKeys(
	const FString& CurvePath, int32 ChannelIndex)
{
	TArray<FBridgeRichCurveKey> Out;
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Out;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: channel %d out of range for '%s'"),
			ChannelIndex, *CurvePath);
		return Out;
	}

	Out.Reserve(Ch->Keys.Num());
	for (const FRichCurveKey& K : Ch->Keys)
		Out.Add(BridgeCurveImpl::ToBridgeKey(K));
	return Out;
}

// ─── GetCurveAsJSONString ───────────────────────────────────

FString UUnrealBridgeCurveLibrary::GetCurveAsJSONString(const FString& CurvePath)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return FString();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), Curve->GetName());
	Root->SetStringField(TEXT("class"), Curve->GetClass()->GetName());

	const TArray<FString> Channels = BridgeCurveImpl::GetChannelNames(Curve);
	TArray<TSharedPtr<FJsonValue>> ChannelsJson;
	for (int32 i = 0; i < Channels.Num(); ++i)
	{
		FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, i);
		if (!Ch) continue;

		TSharedPtr<FJsonObject> ChObj = MakeShared<FJsonObject>();
		ChObj->SetStringField(TEXT("name"), Channels[i]);
		ChObj->SetStringField(TEXT("pre_infinity"),  BridgeCurveImpl::ExtrapToStr(Ch->PreInfinityExtrap));
		ChObj->SetStringField(TEXT("post_infinity"), BridgeCurveImpl::ExtrapToStr(Ch->PostInfinityExtrap));

		TArray<TSharedPtr<FJsonValue>> KeysJson;
		for (const FRichCurveKey& K : Ch->Keys)
		{
			TSharedPtr<FJsonObject> KObj = MakeShared<FJsonObject>();
			KObj->SetNumberField(TEXT("time"), K.Time);
			KObj->SetNumberField(TEXT("value"), K.Value);
			KObj->SetStringField(TEXT("interp_mode"),        BridgeCurveImpl::InterpModeToStr(K.InterpMode));
			KObj->SetStringField(TEXT("tangent_mode"),       BridgeCurveImpl::TangentModeToStr(K.TangentMode));
			KObj->SetStringField(TEXT("tangent_weight_mode"),BridgeCurveImpl::TangentWeightModeToStr(K.TangentWeightMode));
			KObj->SetNumberField(TEXT("arrive_tangent"),        K.ArriveTangent);
			KObj->SetNumberField(TEXT("leave_tangent"),         K.LeaveTangent);
			KObj->SetNumberField(TEXT("arrive_tangent_weight"), K.ArriveTangentWeight);
			KObj->SetNumberField(TEXT("leave_tangent_weight"),  K.LeaveTangentWeight);
			KeysJson.Add(MakeShared<FJsonValueObject>(KObj));
		}
		ChObj->SetArrayField(TEXT("keys"), KeysJson);
		ChannelsJson.Add(MakeShared<FJsonValueObject>(ChObj));
	}
	Root->SetArrayField(TEXT("channels"), ChannelsJson);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

// ─── SetCurveKeys ───────────────────────────────────────────

bool UUnrealBridgeCurveLibrary::SetCurveKeys(
	const FString& CurvePath, int32 ChannelIndex,
	const TArray<FBridgeRichCurveKey>& Keys)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: channel %d out of range for '%s'"),
			ChannelIndex, *CurvePath);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SetCurveKeys", "Set Curve Keys"));
	Curve->Modify();

	TArray<FRichCurveKey> NewKeys;
	NewKeys.Reserve(Keys.Num());
	for (const FBridgeRichCurveKey& K : Keys)
		NewKeys.Add(BridgeCurveImpl::FromBridgeKey(K));

	// FRichCurve::SetKeys expects sorted input — honor that contract.
	NewKeys.Sort([](const FRichCurveKey& A, const FRichCurveKey& B) { return A.Time < B.Time; });
	Ch->SetKeys(NewKeys);

	BridgeCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── AddCurveKey ────────────────────────────────────────────

int32 UUnrealBridgeCurveLibrary::AddCurveKey(
	const FString& CurvePath, int32 ChannelIndex, const FBridgeRichCurveKey& Key)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return -1;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return -1;

	FScopedTransaction Transaction(LOCTEXT("AddCurveKey", "Add Curve Key"));
	Curve->Modify();

	const FKeyHandle H = Ch->UpdateOrAddKey(Key.Time, Key.Value);

	// Apply non-default fields from the bridge key onto the newly added key.
	FRichCurveKey& AddedKey = Ch->GetKey(H);
	const FRichCurveKey Source = BridgeCurveImpl::FromBridgeKey(Key);
	AddedKey.InterpMode           = Source.InterpMode;
	AddedKey.TangentMode          = Source.TangentMode;
	AddedKey.TangentWeightMode    = Source.TangentWeightMode;
	AddedKey.ArriveTangent        = Source.ArriveTangent;
	AddedKey.LeaveTangent         = Source.LeaveTangent;
	AddedKey.ArriveTangentWeight  = Source.ArriveTangentWeight;
	AddedKey.LeaveTangentWeight   = Source.LeaveTangentWeight;

	BridgeCurveImpl::NotifyCurveEdited(Curve);

	// Find the index of the new key in the (already-sorted) array.
	const TArray<FRichCurveKey>& Arr = Ch->Keys;
	for (int32 i = 0; i < Arr.Num(); ++i)
	{
		if (FMath::IsNearlyEqual(Arr[i].Time, Key.Time))
			return i;
	}
	return -1;
}

// ─── RemoveCurveKeyByIndex ──────────────────────────────────

bool UUnrealBridgeCurveLibrary::RemoveCurveKeyByIndex(
	const FString& CurvePath, int32 ChannelIndex, int32 Index)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return false;
	if (!Ch->Keys.IsValidIndex(Index)) return false;

	FScopedTransaction Transaction(LOCTEXT("RemoveCurveKey", "Remove Curve Key"));
	Curve->Modify();

	const float KeyTime = Ch->Keys[Index].Time;
	const FKeyHandle H = Ch->FindKey(KeyTime);
	if (!Ch->IsKeyHandleValid(H)) return false;
	Ch->DeleteKey(H);

	BridgeCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── ClearCurveKeys ─────────────────────────────────────────

bool UUnrealBridgeCurveLibrary::ClearCurveKeys(const FString& CurvePath, int32 ChannelIndex)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return false;

	FScopedTransaction Transaction(LOCTEXT("ClearCurveKeys", "Clear Curve Keys"));
	Curve->Modify();

	Ch->Reset();

	BridgeCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── SetCurveKeyTangents ────────────────────────────────────

bool UUnrealBridgeCurveLibrary::SetCurveKeyTangents(
	const FString& CurvePath, int32 ChannelIndex, int32 Index,
	const FString& TangentMode, const FString& TangentWeightMode,
	float ArriveTangent, float LeaveTangent,
	float ArriveTangentWeight, float LeaveTangentWeight)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch || !Ch->Keys.IsValidIndex(Index)) return false;

	FScopedTransaction Transaction(LOCTEXT("SetCurveKeyTangents", "Set Curve Key Tangents"));
	Curve->Modify();

	FRichCurveKey& K = Ch->Keys[Index];
	if (!TangentMode.IsEmpty())
	{
		ERichCurveTangentMode Mode = RCTM_Auto;
		if (BridgeCurveImpl::TangentModeFromStr(TangentMode, Mode))
			K.TangentMode = Mode;
	}
	if (!TangentWeightMode.IsEmpty())
	{
		ERichCurveTangentWeightMode Wm = RCTWM_WeightedNone;
		if (BridgeCurveImpl::TangentWeightModeFromStr(TangentWeightMode, Wm))
			K.TangentWeightMode = Wm;
	}
	if (!FMath::IsNaN(ArriveTangent))        K.ArriveTangent        = ArriveTangent;
	if (!FMath::IsNaN(LeaveTangent))         K.LeaveTangent         = LeaveTangent;
	if (!FMath::IsNaN(ArriveTangentWeight))  K.ArriveTangentWeight  = ArriveTangentWeight;
	if (!FMath::IsNaN(LeaveTangentWeight))   K.LeaveTangentWeight   = LeaveTangentWeight;

	BridgeCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── SetCurveInfinityExtrap ─────────────────────────────────

bool UUnrealBridgeCurveLibrary::SetCurveInfinityExtrap(
	const FString& CurvePath,
	const FString& PreInfinityExtrap,
	const FString& PostInfinityExtrap)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	ERichCurveExtrapolation Pre = RCCE_Constant, Post = RCCE_Constant;
	const bool bHasPre  = !PreInfinityExtrap.IsEmpty()  && BridgeCurveImpl::ExtrapFromStr(PreInfinityExtrap,  Pre);
	const bool bHasPost = !PostInfinityExtrap.IsEmpty() && BridgeCurveImpl::ExtrapFromStr(PostInfinityExtrap, Post);
	if (!bHasPre && !bHasPost) return false;

	FScopedTransaction Transaction(LOCTEXT("SetCurveInfinityExtrap", "Set Curve Infinity Extrap"));
	Curve->Modify();

	for (FRichCurve* Ch : BridgeCurveImpl::GetAllChannels(Curve))
	{
		if (bHasPre)  Ch->PreInfinityExtrap  = Pre;
		if (bHasPost) Ch->PostInfinityExtrap = Post;
	}

	BridgeCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── AutoSetCurveTangents ───────────────────────────────────

bool UUnrealBridgeCurveLibrary::AutoSetCurveTangents(const FString& CurvePath, float Tension)
{
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FScopedTransaction Transaction(LOCTEXT("AutoSetCurveTangents", "Auto Set Curve Tangents"));
	Curve->Modify();

	for (FRichCurve* Ch : BridgeCurveImpl::GetAllChannels(Curve))
		Ch->AutoSetTangents(Tension);

	BridgeCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── EvaluateCurve ──────────────────────────────────────────

TArray<float> UUnrealBridgeCurveLibrary::EvaluateCurve(
	const FString& CurvePath, int32 ChannelIndex, const TArray<float>& Times)
{
	TArray<float> Out;
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Out;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return Out;

	Out.Reserve(Times.Num());
	for (float T : Times)
		Out.Add(Ch->Eval(T, 0.f));
	return Out;
}

// ─── SampleCurveUniform ─────────────────────────────────────

TArray<float> UUnrealBridgeCurveLibrary::SampleCurveUniform(
	const FString& CurvePath, int32 ChannelIndex,
	float StartTime, float EndTime, int32 NumSamples)
{
	TArray<float> Out;
	UCurveBase* Curve = BridgeCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Out;

	FRichCurve* Ch = BridgeCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return Out;

	if (NumSamples <= 1)
	{
		Out.Add(Ch->Eval(StartTime, 0.f));
		return Out;
	}
	Out.Reserve(NumSamples);
	const float Step = (EndTime - StartTime) / static_cast<float>(NumSamples - 1);
	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = StartTime + Step * static_cast<float>(i);
		Out.Add(Ch->Eval(T, 0.f));
	}
	return Out;
}

// ─── Curve Table helpers ────────────────────────────────────

namespace BridgeCurveImpl
{
	/** Copy a FSimpleCurve row's keys into bridge rich-curve-key form. */
	void SimpleCurveToBridgeKeys(const FSimpleCurve& Src, TArray<FBridgeRichCurveKey>& Out)
	{
		const TCHAR* InterpStr = InterpModeToStr(Src.GetKeyInterpMode());
		for (const FSimpleCurveKey& Sk : Src.GetConstRefOfKeys())
		{
			FBridgeRichCurveKey K;
			K.Time = Sk.Time;
			K.Value = Sk.Value;
			K.InterpMode = InterpStr;
			K.TangentMode = TEXT("None");
			K.TangentWeightMode = TEXT("None");
			Out.Add(K);
		}
	}

	/** Write bridge keys into a FRichCurve row, sorted by Time. */
	void BridgeKeysToRichCurve(const TArray<FBridgeRichCurveKey>& Src, FRichCurve& Dst)
	{
		TArray<FRichCurveKey> New;
		New.Reserve(Src.Num());
		for (const FBridgeRichCurveKey& K : Src) New.Add(FromBridgeKey(K));
		New.Sort([](const FRichCurveKey& A, const FRichCurveKey& B) { return A.Time < B.Time; });
		Dst.SetKeys(New);
	}

	/** Write bridge keys into a FSimpleCurve row (tangent fields ignored, uniform interp). */
	void BridgeKeysToSimpleCurve(const TArray<FBridgeRichCurveKey>& Src, FSimpleCurve& Dst)
	{
		Dst.Reset();
		// Use the interp from the first key as the row-wide interp (SimpleCurve is uniform).
		if (Src.Num() > 0 && !Src[0].InterpMode.IsEmpty())
		{
			ERichCurveInterpMode M = RCIM_Linear;
			if (InterpModeFromStr(Src[0].InterpMode, M))
				Dst.SetKeyInterpMode(M);
		}
		TArray<FBridgeRichCurveKey> Sorted = Src;
		Sorted.Sort([](const FBridgeRichCurveKey& A, const FBridgeRichCurveKey& B) { return A.Time < B.Time; });
		for (const FBridgeRichCurveKey& K : Sorted)
			Dst.AddKey(K.Time, K.Value);
	}
}

// ─── GetCurveTableInfo ──────────────────────────────────────

FBridgeCurveTableInfo UUnrealBridgeCurveLibrary::GetCurveTableInfo(const FString& CurveTablePath)
{
	FBridgeCurveTableInfo Result;
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return Result;

	Result.Name = Table->GetName();
	switch (Table->GetCurveTableMode())
	{
	case ECurveTableMode::Empty:        Result.CurveTableMode = TEXT("Empty");        break;
	case ECurveTableMode::SimpleCurves: Result.CurveTableMode = TEXT("SimpleCurves"); break;
	case ECurveTableMode::RichCurves:   Result.CurveTableMode = TEXT("RichCurves");   break;
	default:                            Result.CurveTableMode = TEXT("Unknown");      break;
	}

	for (const auto& Pair : Table->GetRowMap())
	{
		Result.RowNames.Add(Pair.Key.ToString());
		Result.NumKeysPerRow.Add(Pair.Value ? Pair.Value->GetNumKeys() : 0);
	}
	return Result;
}

// ─── GetCurveTableRowKeys ───────────────────────────────────

TArray<FBridgeRichCurveKey> UUnrealBridgeCurveLibrary::GetCurveTableRowKeys(
	const FString& CurveTablePath, const FString& RowName)
{
	TArray<FBridgeRichCurveKey> Out;
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return Out;

	const FName Key(*RowName);
	if (Table->GetCurveTableMode() == ECurveTableMode::RichCurves)
	{
		if (FRichCurve* R = Table->FindRichCurve(Key, TEXT("BridgeGetCurveTableRowKeys")))
		{
			Out.Reserve(R->Keys.Num());
			for (const FRichCurveKey& K : R->Keys)
				Out.Add(BridgeCurveImpl::ToBridgeKey(K));
		}
	}
	else if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
	{
		if (FSimpleCurve* S = Table->FindSimpleCurve(Key, TEXT("BridgeGetCurveTableRowKeys")))
			BridgeCurveImpl::SimpleCurveToBridgeKeys(*S, Out);
	}
	return Out;
}

// ─── SetCurveTableRowKeys ───────────────────────────────────

bool UUnrealBridgeCurveLibrary::SetCurveTableRowKeys(
	const FString& CurveTablePath, const FString& RowName,
	const TArray<FBridgeRichCurveKey>& Keys)
{
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	const FName Key(*RowName);
	const ECurveTableMode Mode = Table->GetCurveTableMode();

	FScopedTransaction Transaction(LOCTEXT("SetCurveTableRowKeys", "Set Curve Table Row Keys"));
	Table->Modify();

	if (Mode == ECurveTableMode::RichCurves)
	{
		FRichCurve* R = Table->FindRichCurve(Key, TEXT("BridgeSetCurveTableRowKeys"));
		if (!R) return false;
		BridgeCurveImpl::BridgeKeysToRichCurve(Keys, *R);
	}
	else if (Mode == ECurveTableMode::SimpleCurves)
	{
		FSimpleCurve* S = Table->FindSimpleCurve(Key, TEXT("BridgeSetCurveTableRowKeys"));
		if (!S) return false;
		BridgeCurveImpl::BridgeKeysToSimpleCurve(Keys, *S);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: table '%s' is empty — use AddCurveTableRow first"),
			*CurveTablePath);
		return false;
	}

	BridgeCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── AddCurveTableRow ───────────────────────────────────────

bool UUnrealBridgeCurveLibrary::AddCurveTableRow(
	const FString& CurveTablePath, const FString& RowName,
	const TArray<FBridgeRichCurveKey>& Keys)
{
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	const FName NewKey(*RowName);
	// UCurveTable::AddRichCurve / AddSimpleCurve assumes the name is not present; check up front.
	if (Table->GetRowMap().Contains(NewKey))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: row '%s' already exists in '%s'"),
			*RowName, *CurveTablePath);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("AddCurveTableRow", "Add Curve Table Row"));
	Table->Modify();

	// Empty tables default to RichCurves — the flexible choice for bridge callers.
	if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
	{
		FSimpleCurve& S = Table->AddSimpleCurve(NewKey);
		BridgeCurveImpl::BridgeKeysToSimpleCurve(Keys, S);
	}
	else
	{
		FRichCurve& R = Table->AddRichCurve(NewKey);
		BridgeCurveImpl::BridgeKeysToRichCurve(Keys, R);
	}

	BridgeCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── RemoveCurveTableRow ────────────────────────────────────

bool UUnrealBridgeCurveLibrary::RemoveCurveTableRow(
	const FString& CurveTablePath, const FString& RowName)
{
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	const FName Key(*RowName);
	if (!Table->GetRowMap().Contains(Key)) return false;

	FScopedTransaction Transaction(LOCTEXT("RemoveCurveTableRow", "Remove Curve Table Row"));
	Table->Modify();
	Table->RemoveRow(Key);

	BridgeCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── RenameCurveTableRow ────────────────────────────────────

bool UUnrealBridgeCurveLibrary::RenameCurveTableRow(
	const FString& CurveTablePath,
	const FString& OldRowName, const FString& NewRowName)
{
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	FName Old(*OldRowName);
	FName New(*NewRowName);
	if (!Table->GetRowMap().Contains(Old))  return false;
	if (Table->GetRowMap().Contains(New))   return false;

	FScopedTransaction Transaction(LOCTEXT("RenameCurveTableRow", "Rename Curve Table Row"));
	Table->Modify();
	Table->RenameRow(Old, New);

	BridgeCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── EvaluateCurveTableRow ──────────────────────────────────

TArray<float> UUnrealBridgeCurveLibrary::EvaluateCurveTableRow(
	const FString& CurveTablePath, const FString& RowName, const TArray<float>& Times)
{
	TArray<float> Out;
	UCurveTable* Table = BridgeCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return Out;

	FRealCurve* Curve = Table->FindCurveUnchecked(FName(*RowName));
	if (!Curve) return Out;

	Out.Reserve(Times.Num());
	for (float T : Times)
		Out.Add(Curve->Eval(T, 0.f));
	return Out;
}

#undef LOCTEXT_NAMESPACE

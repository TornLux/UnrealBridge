#include "UnrealBridgeChooserLibrary.h"

#include "Chooser.h"
#include "IObjectChooser.h"
#include "IChooserColumn.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooser_Class.h"
#include "ChooserPropertyAccess.h"

#include "FloatRangeColumn.h"
#include "EnumColumn.h"
#include "BoolColumn.h"
#include "ObjectColumn.h"
#include "GameplayTagColumn.h"
#include "RandomizeColumn.h"
#include "OutputFloatColumn.h"
#include "OutputObjectColumn.h"

#include "StructUtils/InstancedStruct.h"
#include "ScopedTransaction.h"
#include "Engine/UserDefinedEnum.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeChooser"

namespace BridgeChooserImpl
{
	UChooserTable* LoadCHT(const FString& Path)
	{
		UChooserTable* CHT = LoadObject<UChooserTable>(nullptr, *Path);
		if (!CHT)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load ChooserTable '%s'"), *Path);
		return CHT;
	}

	int32 GetRowCount(const UChooserTable* CHT)
	{
#if WITH_EDITORONLY_DATA
		return CHT ? CHT->ResultsStructs.Num() : 0;
#else
		return CHT ? CHT->CookedResults.Num() : 0;
#endif
	}

	UEnum* LoadEnumByPath(const FString& EnumPath)
	{
		// Native UEnum: "/Script/Engine.EMyEnum" → load via FindObject
		// UserDefinedEnum: "/Game/.../E_Foo.E_Foo" → LoadObject<UUserDefinedEnum>
		UEnum* AsNative = FindObject<UEnum>(nullptr, *EnumPath);
		if (AsNative) return AsNative;
		return Cast<UEnum>(LoadObject<UUserDefinedEnum>(nullptr, *EnumPath));
	}

	TArray<FName> SplitBindingChain(const FString& Chain)
	{
		TArray<FName> Out;
		TArray<FString> Parts;
		Chain.ParseIntoArray(Parts, TEXT("."), /*InCullEmpty*/ true);
		for (const FString& P : Parts) Out.Add(*P);
		return Out;
	}

	void ApplyBinding(FChooserPropertyBinding& Binding, const FString& Chain, int32 ContextIndex)
	{
		Binding.PropertyBindingChain = SplitBindingChain(Chain);
		Binding.ContextIndex = ContextIndex;
		Binding.IsBoundToRoot = false;
#if WITH_EDITORONLY_DATA
		Binding.DisplayName = Chain;
#endif
	}

	FString DescribeRowResult(const FInstancedStruct& Result, FBridgeCHTRowResult& Out)
	{
		const UScriptStruct* Type = Result.GetScriptStruct();
		if (!Type)
		{
			Out.Kind = TEXT("None");
			return Out.Kind;
		}

		if (Type == FAssetChooser::StaticStruct())
		{
			const FAssetChooser& A = Result.Get<FAssetChooser>();
			Out.Kind = TEXT("Asset");
			Out.ResultPath = A.Asset ? A.Asset->GetPathName() : FString();
			return Out.Kind;
		}
		if (Type == FSoftAssetChooser::StaticStruct())
		{
			const FSoftAssetChooser& A = Result.Get<FSoftAssetChooser>();
			Out.Kind = TEXT("SoftAsset");
			Out.ResultPath = A.Asset.ToString();
			return Out.Kind;
		}
		if (Type == FClassChooser::StaticStruct())
		{
			const FClassChooser& C = Result.Get<FClassChooser>();
			Out.Kind = TEXT("Class");
			Out.ResultPath = C.Class ? C.Class->GetPathName() : FString();
			return Out.Kind;
		}
		if (Type == FEvaluateChooser::StaticStruct())
		{
			const FEvaluateChooser& E = Result.Get<FEvaluateChooser>();
			Out.Kind = TEXT("EvaluateChooser");
			Out.ResultPath = E.Chooser ? E.Chooser->GetPathName() : FString();
			return Out.Kind;
		}
		if (Type == FNestedChooser::StaticStruct())
		{
			const FNestedChooser& N = Result.Get<FNestedChooser>();
			Out.Kind = TEXT("NestedChooser");
			Out.ResultPath = N.Chooser ? N.Chooser->GetPathName() : FString();
			return Out.Kind;
		}
		Out.Kind = Type->GetName();
		Out.ResultPath = FString();
		return Out.Kind;
	}

	FBridgeCHTRowResult MakeRowResult(const FInstancedStruct& Result)
	{
		FBridgeCHTRowResult Out;
		DescribeRowResult(Result, Out);
		return Out;
	}

	FString GetBindingPath(const FInstancedStruct& Column)
	{
		const UScriptStruct* Type = Column.GetScriptStruct();
		if (!Type) return FString();

		// All filter columns have an InputValue: FInstancedStruct (binding wrapper).
		const FStructProperty* InputProp = CastField<FStructProperty>(Type->FindPropertyByName(TEXT("InputValue")));
		if (!InputProp || InputProp->Struct != FInstancedStruct::StaticStruct())
		{
			return FString();
		}
		const FInstancedStruct* Input = InputProp->ContainerPtrToValuePtr<FInstancedStruct>(Column.GetMemory());
		if (!Input) return FString();
		const UScriptStruct* InputType = Input->GetScriptStruct();
		if (!InputType) return FString();

		// All bindings derive from FChooserPropertyBinding.
		const FStructProperty* BindingProp = CastField<FStructProperty>(InputType->FindPropertyByName(TEXT("Binding")));
		if (!BindingProp || !BindingProp->Struct->IsChildOf(TBaseStructure<FChooserPropertyBinding>::Get()))
		{
			return FString();
		}
		const FChooserPropertyBinding* Binding = BindingProp->ContainerPtrToValuePtr<FChooserPropertyBinding>(Input->GetMemory());
		if (!Binding) return FString();

		FString Joined;
		for (int32 i = 0; i < Binding->PropertyBindingChain.Num(); ++i)
		{
			if (i > 0) Joined += TEXT(".");
			Joined += Binding->PropertyBindingChain[i].ToString();
		}
		return Joined;
	}

	FString GetBindingDisplayName(const FInstancedStruct& Column)
	{
#if WITH_EDITORONLY_DATA
		const UScriptStruct* Type = Column.GetScriptStruct();
		if (!Type) return FString();
		const FStructProperty* InputProp = CastField<FStructProperty>(Type->FindPropertyByName(TEXT("InputValue")));
		if (!InputProp || InputProp->Struct != FInstancedStruct::StaticStruct()) return FString();
		const FInstancedStruct* Input = InputProp->ContainerPtrToValuePtr<FInstancedStruct>(Column.GetMemory());
		if (!Input) return FString();
		const UScriptStruct* InputType = Input->GetScriptStruct();
		if (!InputType) return FString();
		const FStructProperty* BindingProp = CastField<FStructProperty>(InputType->FindPropertyByName(TEXT("Binding")));
		if (!BindingProp || !BindingProp->Struct->IsChildOf(TBaseStructure<FChooserPropertyBinding>::Get())) return FString();
		const FChooserPropertyBinding* Binding = BindingProp->ContainerPtrToValuePtr<FChooserPropertyBinding>(Input->GetMemory());
		return Binding ? Binding->DisplayName : FString();
#else
		return FString();
#endif
	}

	bool IsOutputColumnKind(const FName& StructName)
	{
		const FString S = StructName.ToString();
		return S.StartsWith(TEXT("Output"));
	}

	/**
	 * Finalize a chooser write. ALWAYS call this at the end of any UFUNCTION that
	 * mutates ContextData / ColumnsStructs / ResultsStructs / DisabledRows /
	 * FallbackResult. Without it, the editor's binding-resolution widget keeps
	 * stale FCompiledBinding caches and shows NoPropertyBound on perfectly valid
	 * data — runtime works, editor lies. Three steps:
	 *   1. Compile(true) — recurses into every column/result and resolves binding
	 *      chains against current ContextData (populates FCompiledBinding).
	 *   2. PostEditChangeProperty(nullptr) — broadcasts OnContextClassChanged +
	 *      OnOutputObjectTypeChanged so detail widgets refresh.
	 *   3. MarkPackageDirty so the asset gets saved.
	 *
	 * Replaces the old "CHT->MarkPackageDirty();" tail call on every write op.
	 */
	void FinishChooserWrite(UChooserTable* CHT)
	{
		if (!CHT) return;
		CHT->Compile(true);
		FPropertyChangedEvent Event(nullptr);
		CHT->PostEditChangeProperty(Event);
		CHT->MarkPackageDirty();
	}
}

// ─── Reads ─────────────────────────────────────────────────

FBridgeCHTInfo UUnrealBridgeChooserLibrary::GetChooserInfo(const FString& ChooserTablePath)
{
	using namespace BridgeChooserImpl;
	FBridgeCHTInfo Out;

	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;

	Out.RowCount = GetRowCount(CHT);
	Out.ColumnCount = CHT->ColumnsStructs.Num();
	Out.OutputClassPath = CHT->OutputObjectType ? CHT->OutputObjectType->GetPathName() : FString();
	switch (CHT->ResultType)
	{
	case EObjectChooserResultType::ObjectResult:    Out.ResultType = TEXT("ObjectResult"); break;
	case EObjectChooserResultType::ClassResult:     Out.ResultType = TEXT("ClassResult"); break;
	case EObjectChooserResultType::NoPrimaryResult: Out.ResultType = TEXT("NoPrimaryResult"); break;
	default:                                        Out.ResultType = TEXT("Unknown"); break;
	}

#if WITH_EDITORONLY_DATA
	Out.NestedChooserCount = CHT->NestedChoosers.Num();
	Out.bHasFallback = CHT->FallbackResult.IsValid();
	if (Out.bHasFallback)
	{
		Out.Fallback = MakeRowResult(CHT->FallbackResult);
	}
#endif
	return Out;
}

TArray<FBridgeCHTColumn> UUnrealBridgeChooserLibrary::ListChooserColumns(const FString& ChooserTablePath)
{
	using namespace BridgeChooserImpl;
	TArray<FBridgeCHTColumn> Out;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;

	for (const FInstancedStruct& Col : CHT->ColumnsStructs)
	{
		FBridgeCHTColumn Row;
		const UScriptStruct* Type = Col.GetScriptStruct();
		Row.Kind = Type ? Type->GetName() : TEXT("(invalid)");
		Row.BindingPath = GetBindingPath(Col);
		Row.DisplayName = GetBindingDisplayName(Col);
		Row.bIsOutput = Type ? IsOutputColumnKind(Type->GetFName()) : false;
		if (const FChooserColumnBase* Base = Col.GetPtr<FChooserColumnBase>())
		{
#if WITH_EDITORONLY_DATA
			Row.bDisabled = Base->bDisabled;
#endif
		}
		Out.Add(Row);
	}
	return Out;
}

TArray<FBridgeCHTRow> UUnrealBridgeChooserLibrary::ListChooserRows(const FString& ChooserTablePath)
{
	using namespace BridgeChooserImpl;
	TArray<FBridgeCHTRow> Out;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;

#if WITH_EDITORONLY_DATA
	for (int32 i = 0; i < CHT->ResultsStructs.Num(); ++i)
	{
		FBridgeCHTRow Row;
		Row.bDisabled = CHT->DisabledRows.IsValidIndex(i) ? CHT->DisabledRows[i] : false;
		Row.Result = MakeRowResult(CHT->ResultsStructs[i]);
		Out.Add(Row);
	}
#endif
	return Out;
}

FBridgeCHTRowResult UUnrealBridgeChooserLibrary::GetChooserRowResult(const FString& ChooserTablePath, int32 RowIndex)
{
	using namespace BridgeChooserImpl;
	FBridgeCHTRowResult Out;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;
#if WITH_EDITORONLY_DATA
	if (CHT->ResultsStructs.IsValidIndex(RowIndex))
	{
		return MakeRowResult(CHT->ResultsStructs[RowIndex]);
	}
#endif
	return Out;
}

FString UUnrealBridgeChooserLibrary::GetChooserCellRaw(const FString& ChooserTablePath, int32 ColumnIndex, int32 RowIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT || !CHT->ColumnsStructs.IsValidIndex(ColumnIndex)) return FString();

	FInstancedStruct& Col = CHT->ColumnsStructs[ColumnIndex];
	const UScriptStruct* ColType = Col.GetScriptStruct();
	if (!ColType) return FString();

	FName RowValuesName = TEXT("RowValues");
	if (FChooserColumnBase* Base = Col.GetMutablePtr<FChooserColumnBase>())
	{
		const FName VirtualName = Base->RowValuesPropertyName();
		if (!VirtualName.IsNone()) RowValuesName = VirtualName;
	}
	const FArrayProperty* ArrProp = CastField<FArrayProperty>(ColType->FindPropertyByName(RowValuesName));
	if (!ArrProp) return FString();

	FScriptArrayHelper Helper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(Col.GetMutableMemory()));
	if (RowIndex < 0 || RowIndex >= Helper.Num()) return FString();

	FString Out;
	ArrProp->Inner->ExportTextItem_Direct(Out, Helper.GetRawPtr(RowIndex), nullptr, nullptr, PPF_None);
	return Out;
}

// ─── Row writes ────────────────────────────────────────────

namespace BridgeChooserImpl
{
	void InsertRowAt(UChooserTable* CHT, int32 RowIndex)
	{
		// Sync columns first; column.InsertRows ensures cells exist for the new row.
		for (FInstancedStruct& Col : CHT->ColumnsStructs)
		{
#if WITH_EDITOR
			if (FChooserColumnBase* Base = Col.GetMutablePtr<FChooserColumnBase>())
			{
				Base->InsertRows(RowIndex, 1);
			}
#endif
		}
#if WITH_EDITORONLY_DATA
		CHT->ResultsStructs.Insert(FInstancedStruct(), RowIndex);
		CHT->DisabledRows.Insert(false, RowIndex);
#endif
	}

	bool RemoveRowAt(UChooserTable* CHT, int32 RowIndex)
	{
#if WITH_EDITORONLY_DATA
		if (!CHT->ResultsStructs.IsValidIndex(RowIndex)) return false;

		for (FInstancedStruct& Col : CHT->ColumnsStructs)
		{
#if WITH_EDITOR
			if (FChooserColumnBase* Base = Col.GetMutablePtr<FChooserColumnBase>())
			{
				TArray<uint32> ToDelete = { static_cast<uint32>(RowIndex) };
				Base->DeleteRows(ToDelete);
			}
#endif
		}
		CHT->ResultsStructs.RemoveAt(RowIndex);
		if (CHT->DisabledRows.IsValidIndex(RowIndex))
		{
			CHT->DisabledRows.RemoveAt(RowIndex);
		}
		return true;
#else
		return false;
#endif
	}
}

int32 UUnrealBridgeChooserLibrary::AddChooserRow(const FString& ChooserTablePath)
{
	return InsertChooserRow(ChooserTablePath, /*BeforeRow*/ -1);
}

int32 UUnrealBridgeChooserLibrary::InsertChooserRow(const FString& ChooserTablePath, int32 BeforeRow)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;

	const int32 N = GetRowCount(CHT);
	const int32 InsertAt = (BeforeRow < 0 || BeforeRow > N) ? N : BeforeRow;

	const FScopedTransaction Tx(LOCTEXT("AddChooserRow", "Add Chooser Row"));
	CHT->Modify();
	InsertRowAt(CHT, InsertAt);
	FinishChooserWrite(CHT);
	return InsertAt;
}

bool UUnrealBridgeChooserLibrary::RemoveChooserRow(const FString& ChooserTablePath, int32 RowIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;

	const FScopedTransaction Tx(LOCTEXT("RemoveChooserRow", "Remove Chooser Row"));
	CHT->Modify();
	const bool bOK = RemoveRowAt(CHT, RowIndex);
	if (bOK) FinishChooserWrite(CHT);
	return bOK;
}

bool UUnrealBridgeChooserLibrary::SetChooserRowDisabled(const FString& ChooserTablePath, int32 RowIndex, bool bDisabled)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	if (!CHT->ResultsStructs.IsValidIndex(RowIndex)) return false;

	const FScopedTransaction Tx(LOCTEXT("ChooserRowDisabled", "Toggle Chooser Row Disabled"));
	CHT->Modify();
	while (CHT->DisabledRows.Num() <= RowIndex) CHT->DisabledRows.Add(false);
	CHT->DisabledRows[RowIndex] = bDisabled;
	FinishChooserWrite(CHT);
	return true;
#else
	return false;
#endif
}

namespace BridgeChooserImpl
{
	template <typename TChooserStruct>
	bool SetRowResultTyped(UChooserTable* CHT, int32 RowIndex,
		const TFunction<void(TChooserStruct&)>& Configure, const FText& Why)
	{
#if WITH_EDITORONLY_DATA
		if (!CHT->ResultsStructs.IsValidIndex(RowIndex)) return false;
		const FScopedTransaction Tx(Why);
		CHT->Modify();
		FInstancedStruct New;
		New.InitializeAs(TChooserStruct::StaticStruct());
		Configure(New.GetMutable<TChooserStruct>());
		CHT->ResultsStructs[RowIndex] = MoveTemp(New);
		FinishChooserWrite(CHT);
		return true;
#else
		return false;
#endif
	}
}

bool UUnrealBridgeChooserLibrary::SetChooserRowResultAsset(const FString& ChooserTablePath, int32 RowIndex, const FString& AssetPath)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetChooserRowResultAsset cannot load '%s'"), *AssetPath);
		return false;
	}
	return SetRowResultTyped<FAssetChooser>(CHT, RowIndex, [Asset](FAssetChooser& A)
	{
		A.Asset = Asset;
	}, LOCTEXT("ChooserRowAsset", "Set Chooser Row Result (Asset)"));
}

bool UUnrealBridgeChooserLibrary::SetChooserRowResultClass(const FString& ChooserTablePath, int32 RowIndex, const FString& ClassPath)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	UClass* Cls = ClassPath.IsEmpty() ? nullptr : LoadObject<UClass>(nullptr, *ClassPath);
	return SetRowResultTyped<FClassChooser>(CHT, RowIndex, [Cls](FClassChooser& C)
	{
		C.Class = Cls;
	}, LOCTEXT("ChooserRowClass", "Set Chooser Row Result (Class)"));
}

bool UUnrealBridgeChooserLibrary::SetChooserRowResultEvaluateChooser(const FString& ChooserTablePath, int32 RowIndex, const FString& SubChooserPath)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	UChooserTable* Sub = SubChooserPath.IsEmpty() ? nullptr : LoadObject<UChooserTable>(nullptr, *SubChooserPath);
	return SetRowResultTyped<FEvaluateChooser>(CHT, RowIndex, [Sub](FEvaluateChooser& E)
	{
		E.Chooser = Sub;
	}, LOCTEXT("ChooserRowEval", "Set Chooser Row Result (EvaluateChooser)"));
}

bool UUnrealBridgeChooserLibrary::ClearChooserRowResult(const FString& ChooserTablePath, int32 RowIndex)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	if (!CHT->ResultsStructs.IsValidIndex(RowIndex)) return false;
	const FScopedTransaction Tx(LOCTEXT("ChooserRowClear", "Clear Chooser Row Result"));
	CHT->Modify();
	CHT->ResultsStructs[RowIndex].Reset();
	FinishChooserWrite(CHT);
	return true;
#else
	return false;
#endif
}

// ─── Fallback ──────────────────────────────────────────────

bool UUnrealBridgeChooserLibrary::SetChooserFallbackAsset(const FString& ChooserTablePath, const FString& AssetPath)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset) return false;

	const FScopedTransaction Tx(LOCTEXT("ChooserFallback", "Set Chooser Fallback"));
	CHT->Modify();
	CHT->FallbackResult.InitializeAs(FAssetChooser::StaticStruct());
	CHT->FallbackResult.GetMutable<FAssetChooser>().Asset = Asset;
	FinishChooserWrite(CHT);
	return true;
}

bool UUnrealBridgeChooserLibrary::SetChooserContextObjectClass(const FString& ChooserTablePath, const FString& ContextClassPath, const FString& Direction)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;

	UClass* ContextClass = LoadObject<UClass>(nullptr, *ContextClassPath);
	if (!ContextClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetChooserContextObjectClass cannot load class '%s'"), *ContextClassPath);
		return false;
	}

	EContextObjectDirection Dir = EContextObjectDirection::ReadWrite;
	const FString D = Direction.ToLower();
	if      (D == TEXT("read")  || D == TEXT("input"))                 Dir = EContextObjectDirection::Read;
	else if (D == TEXT("write") || D == TEXT("output"))                Dir = EContextObjectDirection::Write;
	else if (D == TEXT("readwrite") || D == TEXT("input/output") || D.IsEmpty()) Dir = EContextObjectDirection::ReadWrite;
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetChooserContextObjectClass invalid Direction '%s' — expected Read / Write / ReadWrite"), *Direction);
		return false;
	}

	const FScopedTransaction Tx(LOCTEXT("CHTSetContext", "Set Chooser Context Object Class"));
	CHT->Modify();
	CHT->ContextData.Reset();
	FInstancedStruct New;
	New.InitializeAs(FContextObjectTypeClass::StaticStruct());
	FContextObjectTypeClass& Entry = New.GetMutable<FContextObjectTypeClass>();
	Entry.Class = ContextClass;
	Entry.Direction = Dir;
	CHT->ContextData.Add(MoveTemp(New));
	FinishChooserWrite(CHT);
	return true;
}

bool UUnrealBridgeChooserLibrary::CompileChooser(const FString& ChooserTablePath)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	CHT->Compile(true);
	FPropertyChangedEvent Event(nullptr);
	CHT->PostEditChangeProperty(Event);
	return true;
}

bool UUnrealBridgeChooserLibrary::ClearChooserFallback(const FString& ChooserTablePath)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	const FScopedTransaction Tx(LOCTEXT("ChooserFallbackClear", "Clear Chooser Fallback"));
	CHT->Modify();
	CHT->FallbackResult.Reset();
	FinishChooserWrite(CHT);
	return true;
}

// ─── Column writes ─────────────────────────────────────────

namespace BridgeChooserImpl
{
	int32 PushColumn(UChooserTable* CHT, FInstancedStruct&& NewCol)
	{
		const int32 NewIdx = CHT->ColumnsStructs.Add(MoveTemp(NewCol));
#if WITH_EDITOR
		// Resize to match existing row count.
		const int32 N = GetRowCount(CHT);
		if (N > 0)
		{
			if (FChooserColumnBase* Base = CHT->ColumnsStructs[NewIdx].GetMutablePtr<FChooserColumnBase>())
			{
				Base->SetNumRows(N);
			}
		}
#endif
		return NewIdx;
	}

	template <typename TColumn, typename TBindingStruct>
	int32 AddTypedColumnWithBinding(UChooserTable* CHT, const FString& ChainStr, int32 ContextIndex,
		const TFunction<void(TColumn&, TBindingStruct&)>& ExtraConfigure)
	{
		FInstancedStruct New;
		New.InitializeAs(TColumn::StaticStruct());
		TColumn& Col = New.GetMutable<TColumn>();

		// Set up the InputValue InstancedStruct with binding subtype and chain.
		Col.InputValue.InitializeAs(TBindingStruct::StaticStruct());
		ApplyBinding(Col.InputValue.template GetMutable<TBindingStruct>().Binding, ChainStr, ContextIndex);

		if (ExtraConfigure)
		{
			ExtraConfigure(Col, Col.InputValue.template GetMutable<TBindingStruct>());
		}
		return PushColumn(CHT, MoveTemp(New));
	}
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnByStructPath(const FString& ChooserTablePath, const FString& ColumnStructPath,
	const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;

	UScriptStruct* ColumnType = LoadObject<UScriptStruct>(nullptr, *ColumnStructPath);
	if (!ColumnType || !ColumnType->IsChildOf(TBaseStructure<FChooserColumnBase>::Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddChooserColumnByStructPath '%s' is not a ChooserColumn"), *ColumnStructPath);
		return INDEX_NONE;
	}

	const FScopedTransaction Tx(LOCTEXT("AddChooserCol", "Add Chooser Column"));
	CHT->Modify();
	FInstancedStruct New;
	New.InitializeAs(ColumnType);

	// Best-effort: fill the InputValue binding chain for any column that exposes one.
	if (!BindingPropertyChain.IsEmpty())
	{
		const FStructProperty* InputProp = CastField<FStructProperty>(ColumnType->FindPropertyByName(TEXT("InputValue")));
		if (InputProp && InputProp->Struct == FInstancedStruct::StaticStruct())
		{
			FInstancedStruct* Input = InputProp->ContainerPtrToValuePtr<FInstancedStruct>(New.GetMutableMemory());
			const UScriptStruct* InputType = Input ? Input->GetScriptStruct() : nullptr;
			if (Input && InputType)
			{
				const FStructProperty* BindingProp = CastField<FStructProperty>(InputType->FindPropertyByName(TEXT("Binding")));
				if (BindingProp && BindingProp->Struct->IsChildOf(TBaseStructure<FChooserPropertyBinding>::Get()))
				{
					FChooserPropertyBinding* Binding = BindingProp->ContainerPtrToValuePtr<FChooserPropertyBinding>(Input->GetMutableMemory());
					if (Binding) ApplyBinding(*Binding, BindingPropertyChain, ContextIndex);
				}
			}
		}
	}

	const int32 NewIdx = PushColumn(CHT, MoveTemp(New));
	FinishChooserWrite(CHT);
	return NewIdx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnFloatRange(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTFloatRange", "Add FloatRange Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FFloatRangeColumn, FFloatContextProperty>(
		CHT, BindingPropertyChain, ContextIndex, nullptr);
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnEnum(const FString& ChooserTablePath, const FString& BindingPropertyChain, const FString& EnumPath, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;

	UEnum* Enum = EnumPath.IsEmpty() ? nullptr : LoadEnumByPath(EnumPath);
	if (!EnumPath.IsEmpty() && !Enum)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: AddChooserColumnEnum cannot load enum '%s'"), *EnumPath);
		return INDEX_NONE;
	}
	const FScopedTransaction Tx(LOCTEXT("AddCHTEnum", "Add Enum Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FEnumColumn, FEnumContextProperty>(
		CHT, BindingPropertyChain, ContextIndex,
		[Enum](FEnumColumn&, FEnumContextProperty& B)
		{
#if WITH_EDITORONLY_DATA
			B.Binding.Enum = Enum;
#endif
		});
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnBool(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTBool", "Add Bool Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FBoolColumn, FBoolContextProperty>(
		CHT, BindingPropertyChain, ContextIndex, nullptr);
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnObject(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTObject", "Add Object Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FObjectColumn, FObjectContextProperty>(
		CHT, BindingPropertyChain, ContextIndex, nullptr);
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnGameplayTag(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTTag", "Add GameplayTag Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FGameplayTagColumn, FGameplayTagContextProperty>(
		CHT, BindingPropertyChain, ContextIndex, nullptr);
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnRandomize(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTRandom", "Add Randomize Column"));
	CHT->Modify();

	// RandomizeColumn doesn't follow the InputValue/Binding pattern — fill via reflection
	// when present; otherwise just push an empty default-initialized column.
	FInstancedStruct New;
	New.InitializeAs(FRandomizeColumn::StaticStruct());
	const int32 Idx = PushColumn(CHT, MoveTemp(New));
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnOutputFloat(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTOutFloat", "Add Output Float Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FOutputFloatColumn, FFloatContextProperty>(
		CHT, BindingPropertyChain, ContextIndex, nullptr);
	FinishChooserWrite(CHT);
	return Idx;
}

int32 UUnrealBridgeChooserLibrary::AddChooserColumnOutputObject(const FString& ChooserTablePath, const FString& BindingPropertyChain, int32 ContextIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return INDEX_NONE;
	const FScopedTransaction Tx(LOCTEXT("AddCHTOutObject", "Add Output Object Column"));
	CHT->Modify();
	const int32 Idx = AddTypedColumnWithBinding<FOutputObjectColumn, FObjectContextProperty>(
		CHT, BindingPropertyChain, ContextIndex, nullptr);
	FinishChooserWrite(CHT);
	return Idx;
}

bool UUnrealBridgeChooserLibrary::RemoveChooserColumn(const FString& ChooserTablePath, int32 ColumnIndex)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	if (!CHT->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;

	const FScopedTransaction Tx(LOCTEXT("RemoveCHTCol", "Remove Chooser Column"));
	CHT->Modify();
	CHT->ColumnsStructs.RemoveAt(ColumnIndex);
	FinishChooserWrite(CHT);
	return true;
}

bool UUnrealBridgeChooserLibrary::SetChooserColumnDisabled(const FString& ChooserTablePath, int32 ColumnIndex, bool bDisabled)
{
#if WITH_EDITORONLY_DATA
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	if (!CHT->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;
	FChooserColumnBase* Base = CHT->ColumnsStructs[ColumnIndex].GetMutablePtr<FChooserColumnBase>();
	if (!Base) return false;
	const FScopedTransaction Tx(LOCTEXT("CHTColDisabled", "Toggle Chooser Column Disabled"));
	CHT->Modify();
	Base->bDisabled = bDisabled;
	FinishChooserWrite(CHT);
	return true;
#else
	return false;
#endif
}

bool UUnrealBridgeChooserLibrary::SetChooserCellRaw(const FString& ChooserTablePath, int32 ColumnIndex, int32 RowIndex, const FString& T3DValue)
{
	using namespace BridgeChooserImpl;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return false;
	if (!CHT->ColumnsStructs.IsValidIndex(ColumnIndex)) return false;

	FInstancedStruct& Col = CHT->ColumnsStructs[ColumnIndex];
	const UScriptStruct* ColType = Col.GetScriptStruct();
	if (!ColType) return false;

	FName RowValuesName = TEXT("RowValues");
#if WITH_EDITOR
	if (FChooserColumnBase* Base = Col.GetMutablePtr<FChooserColumnBase>())
	{
		const FName VirtualName = Base->RowValuesPropertyName();
		if (!VirtualName.IsNone()) RowValuesName = VirtualName;
	}
#endif
	const FArrayProperty* ArrProp = CastField<FArrayProperty>(ColType->FindPropertyByName(RowValuesName));
	if (!ArrProp) return false;

	FScriptArrayHelper Helper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(Col.GetMutableMemory()));
	if (RowIndex < 0 || RowIndex >= Helper.Num()) return false;

	const FScopedTransaction Tx(LOCTEXT("CHTSetCell", "Set Chooser Cell"));
	CHT->Modify();
	const TCHAR* Buf = *T3DValue;
	const TCHAR* Result = ArrProp->Inner->ImportText_Direct(Buf, Helper.GetRawPtr(RowIndex), nullptr, PPF_None);
	if (!Result)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetChooserCellRaw failed to import '%s'"), *T3DValue);
		return false;
	}
	FinishChooserWrite(CHT);
	return true;
}

// ─── Evaluation ────────────────────────────────────────────

FBridgeCHTEvaluation UUnrealBridgeChooserLibrary::EvaluateChooserWithContextObject(const FString& ChooserTablePath, const FString& ContextObjectPath)
{
	using namespace BridgeChooserImpl;
	FBridgeCHTEvaluation Out;

	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;

	UObject* ContextObj = ContextObjectPath.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *ContextObjectPath);

	FChooserEvaluationContext Context;
	if (ContextObj)
	{
		Context.AddObjectParam(ContextObj);
	}

	UObject* Picked = nullptr;
	int32 MatchedRow = -1;
	auto Cb = FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
		[&Picked](UObject* Obj) -> FObjectChooserBase::EIteratorStatus
		{
			Picked = Obj;
			return FObjectChooserBase::EIteratorStatus::Stop;
		});

	UChooserTable::EvaluateChooser(Context, CHT, Cb);

#if WITH_EDITOR
	MatchedRow = CHT->GetDebugSelectedRow();
#endif

	Out.bSucceeded = (Picked != nullptr);
	Out.MatchedRow = MatchedRow;
	if (Picked)
	{
		Out.ResultPath = Picked->GetPathName();
		Out.ResultKind = Picked->GetClass()->GetName();
	}
#if WITH_EDITORONLY_DATA
	if (Out.bSucceeded && MatchedRow == -1 && CHT->FallbackResult.IsValid())
	{
		Out.bUsedFallback = true;
	}
#endif
	return Out;
}

TArray<FBridgeCHTRowResult> UUnrealBridgeChooserLibrary::EvaluateChooserMultiWithContextObject(const FString& ChooserTablePath, const FString& ContextObjectPath)
{
	using namespace BridgeChooserImpl;
	TArray<FBridgeCHTRowResult> Out;

	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;

	UObject* ContextObj = ContextObjectPath.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *ContextObjectPath);

	FChooserEvaluationContext Context;
	if (ContextObj)
	{
		Context.AddObjectParam(ContextObj);
	}

	auto Cb = FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
		[&Out](UObject* Obj) -> FObjectChooserBase::EIteratorStatus
		{
			FBridgeCHTRowResult R;
			if (Obj)
			{
				R.Kind = Obj->GetClass()->GetName();
				R.ResultPath = Obj->GetPathName();
			}
			else
			{
				R.Kind = TEXT("None");
			}
			Out.Add(R);
			// Continue tells the chooser machinery to keep walking — we want every passing row.
			return FObjectChooserBase::EIteratorStatus::Continue;
		});

	UChooserTable::EvaluateChooser(Context, CHT, Cb);
	return Out;
}

TArray<FBridgeCHTRowResult> UUnrealBridgeChooserLibrary::ListPossibleResults(const FString& ChooserTablePath)
{
	using namespace BridgeChooserImpl;
	TArray<FBridgeCHTRowResult> Out;
	UChooserTable* CHT = LoadCHT(ChooserTablePath);
	if (!CHT) return Out;
#if WITH_EDITORONLY_DATA
	for (const FInstancedStruct& R : CHT->ResultsStructs)
	{
		Out.Add(MakeRowResult(R));
	}
	if (CHT->FallbackResult.IsValid())
	{
		FBridgeCHTRowResult Fallback = MakeRowResult(CHT->FallbackResult);
		Fallback.Kind = TEXT("Fallback:") + Fallback.Kind;
		Out.Add(Fallback);
	}
#endif
	return Out;
}

#undef LOCTEXT_NAMESPACE

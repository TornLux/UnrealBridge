#include "UnrealBridgeDataTableLibrary.h"
#include "Misc/EngineVersionComparison.h"
#include "Engine/DataTable.h"
#include "DataTableEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeDataTable"

// ─── Helpers ────────────────────────────────────────────────

namespace BridgeDataTableImpl
{
	UDataTable* LoadDT(const FString& Path)
	{
		UDataTable* DT = LoadObject<UDataTable>(nullptr, *Path);
		if (!DT)
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load DataTable '%s'"), *Path);
		return DT;
	}

	/** Export a single field value to text. */
	FString ExportFieldValue(const FProperty* Prop, const uint8* RowData)
	{
		FString Out;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		Prop->ExportTextItem_Direct(Out, ValuePtr, nullptr, nullptr, PPF_None);
		return Out;
	}

	/** Build a row entry honoring an optional column allow-list. */
	FBridgeDataTableRow BuildRow(
		FName RowKey,
		const uint8* RowData,
		const UScriptStruct* RowStruct,
		const TSet<FString>* ColumnAllowList)
	{
		FBridgeDataTableRow Row;
		Row.RowName = RowKey.ToString();
		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			FProperty* Prop = *It;
			const FString PropName = Prop->GetName();
			if (ColumnAllowList && ColumnAllowList->Num() > 0 && !ColumnAllowList->Contains(PropName))
				continue;
			Row.Fields.Add(FString::Printf(TEXT("%s = %s"), *PropName, *ExportFieldValue(Prop, RowData)));
		}
		return Row;
	}

	/** Find a property by name on the row struct. */
	FProperty* FindProperty(const UScriptStruct* RowStruct, const FString& FieldName)
	{
		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			if (It->GetName().Equals(FieldName, ESearchCase::IgnoreCase))
				return *It;
		}
		return nullptr;
	}

	/** Import text into a single field. */
	bool ImportFieldText(FProperty* Prop, uint8* RowData, const FString& Value)
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
		const TCHAR* Buffer = *Value;
		const TCHAR* After = Prop->ImportText_Direct(Buffer, ValuePtr, nullptr, PPF_None, GLog);
		return After != nullptr;
	}

	/** Build a DataTable JSON string without using UDataTable::GetTableAsJSON(). */
	bool BuildDataTableJSONString(const UDataTable* DT, FString& OutJson)
	{
		OutJson.Reset();
		if (!DT)
		{
			return false;
		}

		const UScriptStruct* RowStruct = DT->GetRowStruct();
		if (!RowStruct)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: DataTable '%s' has no row struct"), *DT->GetPathName());
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Rows;
		const TMap<FName, uint8*>& RowMap = DT->GetRowMap();
		Rows.Reserve(RowMap.Num());

		for (const TPair<FName, uint8*>& Pair : RowMap)
		{
			const uint8* RowData = Pair.Value;
			if (!RowData)
			{
				UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: DataTable '%s' row '%s' has null row data"), *DT->GetPathName(), *Pair.Key.ToString());
				continue;
			}

			TSharedRef<FJsonObject> RowObject = MakeShared<FJsonObject>();
			RowObject->SetStringField(TEXT("Name"), Pair.Key.ToString());

			for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
			{
				FProperty* Prop = *It;
				if (!Prop)
				{
					continue;
				}

				const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(RowData);
				TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Prop, ValuePtr, 0, 0);
				if (JsonValue.IsValid())
				{
					RowObject->SetField(Prop->GetName(), JsonValue);
				}
				else
				{
					FString ExportedText;
					Prop->ExportTextItem_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
					RowObject->SetStringField(Prop->GetName(), ExportedText);
				}
			}

			Rows.Add(MakeShared<FJsonValueObject>(RowObject));
		}

		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		return FJsonSerializer::Serialize(Rows, Writer);
	}
}

// ─── GetDataTableRows ───────────────────────────────────────

FBridgeDataTableInfo UUnrealBridgeDataTableLibrary::GetDataTableRows(const FString& DataTablePath)
{
	FBridgeDataTableInfo Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	Result.Name = DT->GetName();
	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	Result.RowStructName = RowStruct->GetName();
	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		Result.ColumnNames.Add(It->GetName());

	const TMap<FName, uint8*>& RowMap = DT->GetRowMap();
	Result.NumRows = RowMap.Num();
	for (const auto& Pair : RowMap)
		Result.Rows.Add(BridgeDataTableImpl::BuildRow(Pair.Key, Pair.Value, RowStruct, nullptr));

	return Result;
}

// ─── GetDataTableSummary ────────────────────────────────────

FBridgeDataTableInfo UUnrealBridgeDataTableLibrary::GetDataTableSummary(const FString& DataTablePath)
{
	FBridgeDataTableInfo Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	Result.Name = DT->GetName();
	if (const UScriptStruct* RowStruct = DT->GetRowStruct())
	{
		Result.RowStructName = RowStruct->GetName();
		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
			Result.ColumnNames.Add(It->GetName());
	}
	Result.NumRows = DT->GetRowMap().Num();
	return Result;
}

// ─── GetDataTableRowNames ───────────────────────────────────

TArray<FString> UUnrealBridgeDataTableLibrary::GetDataTableRowNames(const FString& DataTablePath)
{
	TArray<FString> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	for (const auto& Pair : DT->GetRowMap())
		Result.Add(Pair.Key.ToString());
	return Result;
}

// ─── GetDataTableRow ────────────────────────────────────────

FBridgeDataTableRow UUnrealBridgeDataTableLibrary::GetDataTableRow(const FString& DataTablePath, const FString& RowName)
{
	FBridgeDataTableRow Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	uint8* const* RowPtr = DT->GetRowMap().Find(FName(*RowName));
	if (!RowPtr) return Result;

	return BridgeDataTableImpl::BuildRow(FName(*RowName), *RowPtr, RowStruct, nullptr);
}

// ─── GetDataTableRowField ───────────────────────────────────

FString UUnrealBridgeDataTableLibrary::GetDataTableRowField(
	const FString& DataTablePath, const FString& RowName, const FString& FieldName)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return FString();

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return FString();

	uint8* const* RowPtr = DT->GetRowMap().Find(FName(*RowName));
	if (!RowPtr) return FString();

	FProperty* Prop = BridgeDataTableImpl::FindProperty(RowStruct, FieldName);
	if (!Prop) return FString();

	return BridgeDataTableImpl::ExportFieldValue(Prop, *RowPtr);
}

// ─── GetDataTableColumn ─────────────────────────────────────

TArray<FString> UUnrealBridgeDataTableLibrary::GetDataTableColumn(const FString& DataTablePath, const FString& FieldName)
{
	TArray<FString> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	FProperty* Prop = BridgeDataTableImpl::FindProperty(RowStruct, FieldName);
	if (!Prop)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Field '%s' not found on DataTable '%s'"), *FieldName, *DataTablePath);
		return Result;
	}

	for (const auto& Pair : DT->GetRowMap())
	{
		Result.Add(FString::Printf(TEXT("%s = %s"), *Pair.Key.ToString(), *BridgeDataTableImpl::ExportFieldValue(Prop, Pair.Value)));
	}
	return Result;
}

// ─── GetDataTableRowsFiltered ───────────────────────────────

FBridgeDataTableInfo UUnrealBridgeDataTableLibrary::GetDataTableRowsFiltered(
	const FString& DataTablePath,
	const TArray<FString>& RowFilter,
	const TArray<FString>& ColumnFilter)
{
	FBridgeDataTableInfo Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	Result.Name = DT->GetName();
	Result.RowStructName = RowStruct->GetName();

	TSet<FString> ColumnAllow;
	for (const FString& C : ColumnFilter) ColumnAllow.Add(C);

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		const FString N = It->GetName();
		if (ColumnAllow.Num() == 0 || ColumnAllow.Contains(N))
			Result.ColumnNames.Add(N);
	}

	TSet<FName> RowAllow;
	for (const FString& R : RowFilter) RowAllow.Add(FName(*R));

	for (const auto& Pair : DT->GetRowMap())
	{
		if (RowAllow.Num() > 0 && !RowAllow.Contains(Pair.Key)) continue;
		Result.Rows.Add(BridgeDataTableImpl::BuildRow(Pair.Key, Pair.Value, RowStruct, ColumnAllow.Num() > 0 ? &ColumnAllow : nullptr));
	}
	Result.NumRows = Result.Rows.Num();
	return Result;
}

// ─── SearchDataTableRows ────────────────────────────────────

TArray<FString> UUnrealBridgeDataTableLibrary::SearchDataTableRows(
	const FString& DataTablePath,
	const FString& Keyword,
	const TArray<FString>& ColumnFilter)
{
	TArray<FString> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT || Keyword.IsEmpty()) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	TSet<FString> ColumnAllow;
	for (const FString& C : ColumnFilter) ColumnAllow.Add(C);

	for (const auto& Pair : DT->GetRowMap())
	{
		bool bMatched = false;
		// Row name itself is also searchable
		if (Pair.Key.ToString().Contains(Keyword, ESearchCase::IgnoreCase))
			bMatched = true;

		if (!bMatched)
		{
			for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
			{
				if (ColumnAllow.Num() > 0 && !ColumnAllow.Contains(It->GetName())) continue;
				FString V = BridgeDataTableImpl::ExportFieldValue(*It, Pair.Value);
				if (V.Contains(Keyword, ESearchCase::IgnoreCase))
				{
					bMatched = true;
					break;
				}
			}
		}
		if (bMatched) Result.Add(Pair.Key.ToString());
	}
	return Result;
}

// ─── GetDataTablesUsingStruct ───────────────────────────────

TArray<FString> UUnrealBridgeDataTableLibrary::GetDataTablesUsingStruct(const FString& RowStructName)
{
	TArray<FString> Result;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& Registry = ARM.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	// No PackagePaths filter — scan all mounted content roots (incl. plugin content)

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	for (const FAssetData& AD : Assets)
	{
		UDataTable* DT = Cast<UDataTable>(AD.GetAsset());
		if (!DT) continue;
		const UScriptStruct* RS = DT->GetRowStruct();
		if (RS && RS->GetName().Equals(RowStructName, ESearchCase::IgnoreCase))
		{
			Result.Add(AD.GetObjectPathString());
		}
	}
	return Result;
}

// ─── SetDataTableRowField ───────────────────────────────────

bool UUnrealBridgeDataTableLibrary::SetDataTableRowField(
	const FString& DataTablePath,
	const FString& RowName,
	const FString& FieldName,
	const FString& ExportedValue)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return false;

	uint8* const* RowPtr = DT->GetRowMap().Find(FName(*RowName));
	if (!RowPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Row '%s' not found"), *RowName);
		return false;
	}

	FProperty* Prop = BridgeDataTableImpl::FindProperty(RowStruct, FieldName);
	if (!Prop)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Field '%s' not found"), *FieldName);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SetDataTableRowField", "Set DataTable Row Field"));
	DT->Modify();
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	const bool bOk = BridgeDataTableImpl::ImportFieldText(Prop, *RowPtr, ExportedValue);

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	DT->MarkPackageDirty();
	return bOk;
}

// ─── AddDataTableRow ────────────────────────────────────────

bool UUnrealBridgeDataTableLibrary::AddDataTableRow(
	const FString& DataTablePath,
	const FString& RowName,
	const TMap<FString, FString>& FieldValues)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return false;

	const FName NewKey(*RowName);
	if (DT->GetRowMap().Contains(NewKey))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Row '%s' already exists"), *RowName);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("AddDataTableRow", "Add DataTable Row"));
	uint8* NewRow = FDataTableEditorUtils::AddRow(DT, NewKey);
	if (!NewRow) return false;

	for (const TPair<FString, FString>& KV : FieldValues)
	{
		if (FProperty* Prop = BridgeDataTableImpl::FindProperty(RowStruct, KV.Key))
			BridgeDataTableImpl::ImportFieldText(Prop, NewRow, KV.Value);
	}

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	DT->MarkPackageDirty();
	return true;
}

// ─── RemoveDataTableRow ─────────────────────────────────────

bool UUnrealBridgeDataTableLibrary::RemoveDataTableRow(const FString& DataTablePath, const FString& RowName)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FScopedTransaction Transaction(LOCTEXT("RemoveDataTableRow", "Remove DataTable Row"));
	const bool bOk = FDataTableEditorUtils::RemoveRow(DT, FName(*RowName));
	if (bOk) DT->MarkPackageDirty();
	return bOk;
}

// ─── DuplicateDataTableRow ──────────────────────────────────

bool UUnrealBridgeDataTableLibrary::DuplicateDataTableRow(
	const FString& DataTablePath,
	const FString& SourceRowName,
	const FString& NewRowName)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FScopedTransaction Transaction(LOCTEXT("DuplicateDataTableRow", "Duplicate DataTable Row"));
	uint8* New = FDataTableEditorUtils::DuplicateRow(DT, FName(*SourceRowName), FName(*NewRowName));
	if (New) DT->MarkPackageDirty();
	return New != nullptr;
}

// ─── RenameDataTableRow ─────────────────────────────────────

bool UUnrealBridgeDataTableLibrary::RenameDataTableRow(
	const FString& DataTablePath,
	const FString& OldRowName,
	const FString& NewRowName)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FScopedTransaction Transaction(LOCTEXT("RenameDataTableRow", "Rename DataTable Row"));
	const bool bOk = FDataTableEditorUtils::RenameRow(DT, FName(*OldRowName), FName(*NewRowName));
	if (bOk) DT->MarkPackageDirty();
	return bOk;
}

// ─── ReorderDataTableRows ───────────────────────────────────

bool UUnrealBridgeDataTableLibrary::ReorderDataTableRows(
	const FString& DataTablePath,
	const TArray<FString>& OrderedNames)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FScopedTransaction Transaction(LOCTEXT("ReorderDataTableRows", "Reorder DataTable Rows"));

	for (int32 TargetIdx = 0; TargetIdx < OrderedNames.Num(); ++TargetIdx)
	{
		const FName Target(*OrderedNames[TargetIdx]);

		// Find current index of this row
		int32 CurrentIdx = -1;
		int32 Idx = 0;
		for (const auto& Pair : DT->GetRowMap())
		{
			if (Pair.Key == Target) { CurrentIdx = Idx; break; }
			++Idx;
		}
		if (CurrentIdx < 0) continue; // row not found — skip

		const int32 Delta = CurrentIdx - TargetIdx;
		if (Delta > 0)
		{
			FDataTableEditorUtils::MoveRow(DT, Target, FDataTableEditorUtils::ERowMoveDirection::Up, Delta);
		}
		else if (Delta < 0)
		{
			FDataTableEditorUtils::MoveRow(DT, Target, FDataTableEditorUtils::ERowMoveDirection::Down, -Delta);
		}
	}

	DT->MarkPackageDirty();
	return true;
}

// ─── ExportDataTableToCSV ───────────────────────────────────

bool UUnrealBridgeDataTableLibrary::ExportDataTableToCSV(const FString& DataTablePath, const FString& OutCsvFilePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	const FString Csv = DT->GetTableAsCSV();
	if (Csv.IsEmpty()) return false;

	return FFileHelper::SaveStringToFile(Csv, *OutCsvFilePath);
}

// ─── ImportDataTableFromCSV ─────────────────────────────────

bool UUnrealBridgeDataTableLibrary::ImportDataTableFromCSV(const FString& DataTablePath, const FString& CsvFilePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *CsvFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not read CSV file '%s'"), *CsvFilePath);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("ImportDataTableFromCSV", "Import DataTable From CSV"));
	DT->Modify();
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	TArray<FString> Errors = DT->CreateTableFromCSVString(Content);

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	DT->MarkPackageDirty();

	for (const FString& E : Errors)
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge CSV import: %s"), *E);
	return Errors.Num() == 0;
}

// ─── DoesDataTableRowExist ──────────────────────────────────

bool UUnrealBridgeDataTableLibrary::DoesDataTableRowExist(
	const FString& DataTablePath, const FString& RowName)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;
	return DT->GetRowMap().Contains(FName(*RowName));
}

// ─── SetDataTableRowFields ──────────────────────────────────

bool UUnrealBridgeDataTableLibrary::SetDataTableRowFields(
	const FString& DataTablePath,
	const FString& RowName,
	const TMap<FString, FString>& FieldValues)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return false;

	uint8* const* RowPtr = DT->GetRowMap().Find(FName(*RowName));
	if (!RowPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Row '%s' not found"), *RowName);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SetDataTableRowFields", "Set DataTable Row Fields"));
	DT->Modify();
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);

	int32 NumApplied = 0;
	for (const TPair<FString, FString>& Entry : FieldValues)
	{
		FProperty* Prop = BridgeDataTableImpl::FindProperty(RowStruct, Entry.Key);
		if (!Prop)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Field '%s' not found"), *Entry.Key);
			continue;
		}
		if (BridgeDataTableImpl::ImportFieldText(Prop, *RowPtr, Entry.Value))
		{
			++NumApplied;
		}
	}

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	DT->MarkPackageDirty();
	return NumApplied > 0;
}

// ─── GetDataTableAsJSONString ───────────────────────────────

FString UUnrealBridgeDataTableLibrary::GetDataTableAsJSONString(const FString& DataTablePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return FString();

	FString Contents;
	if (!BridgeDataTableImpl::BuildDataTableJSONString(DT, Contents))
	{
		return FString();
	}
	return Contents;
}

// ─── ExportDataTableToJSON ──────────────────────────────────

bool UUnrealBridgeDataTableLibrary::ExportDataTableToJSON(
	const FString& DataTablePath, const FString& OutJsonFilePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FString Contents;
	if (!BridgeDataTableImpl::BuildDataTableJSONString(DT, Contents) || Contents.IsEmpty())
	{
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Contents, *OutJsonFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not write JSON file '%s'"), *OutJsonFilePath);
		return false;
	}
	return true;
}

// ─── ImportDataTableFromJSON ────────────────────────────────

bool UUnrealBridgeDataTableLibrary::ImportDataTableFromJSON(
	const FString& DataTablePath, const FString& JsonFilePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return false;

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *JsonFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not read JSON file '%s'"), *JsonFilePath);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("ImportDataTableFromJSON", "Import DataTable From JSON"));
	DT->Modify();
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	TArray<FString> Errors = DT->CreateTableFromJSONString(Content);

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	DT->MarkPackageDirty();

	for (const FString& E : Errors)
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge JSON import: %s"), *E);
	return Errors.Num() == 0;
}

// ─── GetDataTableColumnTypes ────────────────────────────────

TArray<FBridgeDataTableColumn> UUnrealBridgeDataTableLibrary::GetDataTableColumnTypes(const FString& DataTablePath)
{
	TArray<FBridgeDataTableColumn> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		FBridgeDataTableColumn Col;
		Col.Name = Prop->GetName();
		Col.TypeName = Prop->GetClass()->GetName();

		if (const FStructProperty* SP = CastField<FStructProperty>(Prop))
		{
			Col.InnerTypeName = SP->Struct ? SP->Struct->GetName() : FString();
		}
		else if (const FObjectPropertyBase* OP = CastField<FObjectPropertyBase>(Prop))
		{
			Col.InnerTypeName = OP->PropertyClass ? OP->PropertyClass->GetName() : FString();
		}
		else if (const FEnumProperty* EP = CastField<FEnumProperty>(Prop))
		{
			Col.InnerTypeName = EP->GetEnum() ? EP->GetEnum()->GetName() : FString();
		}
		else if (const FByteProperty* BP = CastField<FByteProperty>(Prop))
		{
			if (BP->Enum) Col.InnerTypeName = BP->Enum->GetName();
		}
		else if (const FArrayProperty* AP = CastField<FArrayProperty>(Prop))
		{
			Col.InnerTypeName = AP->Inner ? AP->Inner->GetClass()->GetName() : FString();
		}

		Result.Add(Col);
	}
	return Result;
}

// ─── GetDataTableRowAsMap ───────────────────────────────────

TMap<FString, FString> UUnrealBridgeDataTableLibrary::GetDataTableRowAsMap(
	const FString& DataTablePath, const FString& RowName)
{
	TMap<FString, FString> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	uint8* const* RowPtr = DT->GetRowMap().Find(FName(*RowName));
	if (!RowPtr) return Result;

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		Result.Add(Prop->GetName(), BridgeDataTableImpl::ExportFieldValue(Prop, *RowPtr));
	}
	return Result;
}

// ─── GetDataTableRowAsJSONString ────────────────────────────

FString UUnrealBridgeDataTableLibrary::GetDataTableRowAsJSONString(
	const FString& DataTablePath, const FString& RowName)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return FString();

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return FString();

	uint8* const* RowPtr = DT->GetRowMap().Find(FName(*RowName));
	if (!RowPtr) return FString();

	FString Out = TEXT("{");
	bool bFirst = true;
	Out += FString::Printf(TEXT("\"Name\":\"%s\""), *RowName);
	bFirst = false;
	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		FString Value = BridgeDataTableImpl::ExportFieldValue(Prop, *RowPtr);
		// Escape backslashes and quotes for JSON
		Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Out += FString::Printf(TEXT(",\"%s\":\"%s\""), *Prop->GetName(), *Value);
	}
	Out += TEXT("}");
	return Out;
}

// ─── ClearDataTable ─────────────────────────────────────────

int32 UUnrealBridgeDataTableLibrary::ClearDataTable(const FString& DataTablePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return 0;

	FScopedTransaction Transaction(LOCTEXT("ClearDataTable", "Clear DataTable"));
	DT->Modify();
	FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	TArray<FName> Keys;
	DT->GetRowMap().GetKeys(Keys);
	const int32 Count = Keys.Num();

	DT->EmptyTable();

	FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	DT->MarkPackageDirty();
	return Count;
}

// ─── CopyDataTableRows ──────────────────────────────────────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
int32 UUnrealBridgeDataTableLibrary::CopyDataTableRows(
	const FString& SourceDataTablePath,
	const FString& DestDataTablePath,
	const TArray<FString>& RowNames,
	bool bOverwrite)
{
	UDataTable* Src = BridgeDataTableImpl::LoadDT(SourceDataTablePath);
	UDataTable* Dst = BridgeDataTableImpl::LoadDT(DestDataTablePath);
	if (!Src || !Dst) return 0;

	const UScriptStruct* RowStruct = Src->GetRowStruct();
	if (!RowStruct || Dst->GetRowStruct() != RowStruct)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: CopyDataTableRows requires matching row structs"));
		return 0;
	}

	TSet<FName> Filter;
	for (const FString& N : RowNames) Filter.Add(FName(*N));

	FScopedTransaction Transaction(LOCTEXT("CopyDataTableRows", "Copy DataTable Rows"));
	Dst->Modify();
	FDataTableEditorUtils::BroadcastPreChange(Dst, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	int32 NumCopied = 0;
	for (const auto& Pair : Src->GetRowMap())
	{
		if (Filter.Num() > 0 && !Filter.Contains(Pair.Key)) continue;

		const bool bExists = Dst->GetRowMap().Contains(Pair.Key);
		if (bExists)
		{
			if (!bOverwrite) continue;
			Dst->RemoveRow(Pair.Key);
		}
		// AddRow allocates + copies into a fresh row buffer.
		Dst->AddRow(Pair.Key, Pair.Value, RowStruct);
		++NumCopied;
	}

	FDataTableEditorUtils::BroadcastPostChange(Dst, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	Dst->MarkPackageDirty();
	return NumCopied;
}
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

// ─── GetDataTableRowStructPath ──────────────────────────────

FString UUnrealBridgeDataTableLibrary::GetDataTableRowStructPath(const FString& DataTablePath)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return FString();
	const UScriptStruct* RS = DT->GetRowStruct();
	if (!RS) return FString();
	// Only user-defined structs live under /Game — native structs will yield a /Script/... path.
	return RS->GetPathName();
}

// ─── FindDataTableRowsByFieldValue ──────────────────────────

TArray<FString> UUnrealBridgeDataTableLibrary::FindDataTableRowsByFieldValue(
	const FString& DataTablePath,
	const FString& FieldName,
	const FString& Value,
	bool bCaseSensitive)
{
	TArray<FString> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return Result;

	FProperty* Prop = BridgeDataTableImpl::FindProperty(RowStruct, FieldName);
	if (!Prop)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Field '%s' not found on '%s'"), *FieldName, *DataTablePath);
		return Result;
	}

	const ESearchCase::Type Case = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
	for (const auto& Pair : DT->GetRowMap())
	{
		const FString V = BridgeDataTableImpl::ExportFieldValue(Prop, Pair.Value);
		if (V.Equals(Value, Case))
			Result.Add(Pair.Key.ToString());
	}
	return Result;
}

// ─── GetDataTableRowsAsJSONArray ────────────────────────────

FString UUnrealBridgeDataTableLibrary::GetDataTableRowsAsJSONArray(
	const FString& DataTablePath,
	const TArray<FString>& RowFilter,
	const TArray<FString>& ColumnFilter)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return TEXT("[]");

	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct) return TEXT("[]");

	TSet<FName> RowAllow;
	for (const FString& R : RowFilter) RowAllow.Add(FName(*R));
	TSet<FString> ColAllow;
	for (const FString& C : ColumnFilter) ColAllow.Add(C);

	auto EscapeJson = [](FString S)
	{
		S.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		S.ReplaceInline(TEXT("\""), TEXT("\\\""));
		S.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		S.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		S.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return S;
	};

	FString Out = TEXT("[");
	bool bFirstRow = true;
	for (const auto& Pair : DT->GetRowMap())
	{
		if (RowAllow.Num() > 0 && !RowAllow.Contains(Pair.Key)) continue;
		if (!bFirstRow) Out += TEXT(",");
		bFirstRow = false;
		Out += FString::Printf(TEXT("{\"Name\":\"%s\""), *EscapeJson(Pair.Key.ToString()));
		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			FProperty* Prop = *It;
			const FString PropName = Prop->GetName();
			if (ColAllow.Num() > 0 && !ColAllow.Contains(PropName)) continue;
			const FString V = BridgeDataTableImpl::ExportFieldValue(Prop, Pair.Value);
			Out += FString::Printf(TEXT(",\"%s\":\"%s\""), *EscapeJson(PropName), *EscapeJson(V));
		}
		Out += TEXT("}");
	}
	Out += TEXT("]");
	return Out;
}

// ─── DiffDataTableRows ──────────────────────────────────────

TArray<FString> UUnrealBridgeDataTableLibrary::DiffDataTableRows(
	const FString& DataTablePathA,
	const FString& RowNameA,
	const FString& DataTablePathB,
	const FString& RowNameB)
{
	TArray<FString> Result;
	UDataTable* A = BridgeDataTableImpl::LoadDT(DataTablePathA);
	UDataTable* B = BridgeDataTableImpl::LoadDT(DataTablePathB);
	if (!A || !B) return Result;

	const UScriptStruct* SA = A->GetRowStruct();
	const UScriptStruct* SB = B->GetRowStruct();
	if (!SA || SA != SB)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: DiffDataTableRows requires matching row structs"));
		return Result;
	}

	uint8* const* PA = A->GetRowMap().Find(FName(*RowNameA));
	uint8* const* PB = B->GetRowMap().Find(FName(*RowNameB));
	if (!PA || !PB) return Result;

	for (TFieldIterator<FProperty> It(SA); It; ++It)
	{
		FProperty* Prop = *It;
		const FString VA = BridgeDataTableImpl::ExportFieldValue(Prop, *PA);
		const FString VB = BridgeDataTableImpl::ExportFieldValue(Prop, *PB);
		if (!VA.Equals(VB, ESearchCase::CaseSensitive))
			Result.Add(Prop->GetName());
	}
	return Result;
}

// ─── GetDataTableRowDefaults ────────────────────────────────

TMap<FString, FString> UUnrealBridgeDataTableLibrary::GetDataTableRowDefaults(const FString& DataTablePath)
{
	TMap<FString, FString> Result;
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	if (!DT) return Result;

	UScriptStruct* RowStruct = const_cast<UScriptStruct*>(DT->GetRowStruct());
	if (!RowStruct) return Result;

	const int32 Size = RowStruct->GetStructureSize();
	if (Size <= 0) return Result;

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Size);
	RowStruct->InitializeStruct(Buffer.GetData());

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		Result.Add(Prop->GetName(), BridgeDataTableImpl::ExportFieldValue(Prop, Buffer.GetData()));
	}

	RowStruct->DestroyStruct(Buffer.GetData());
	return Result;
}

// ─── Text-input + create-from-text imports ──────────────────

namespace BridgeDataTableImpl
{
	/** Resolve a row struct by content path (e.g. "/Game/Data/F_Item")
	 *  or short name (e.g. "F_Item"). Returns nullptr on failure. */
	UScriptStruct* ResolveRowStruct(const FString& PathOrShortName)
	{
		if (PathOrShortName.IsEmpty()) return nullptr;
		// Path form: try LoadObject directly.
		if (PathOrShortName.StartsWith(TEXT("/")))
		{
			if (UScriptStruct* S = LoadObject<UScriptStruct>(nullptr, *PathOrShortName)) return S;
		}
		// Short-name form: scan all loaded UScriptStructs.
		return FindFirstObject<UScriptStruct>(*PathOrShortName, EFindFirstObjectOptions::None);
	}

	/** Run a CSV/JSON importer against an existing DataTable; pack the result. */
	template <typename TImporter>
	FBridgeDataTableImportResult RunImport(UDataTable* DT, const FString& AssetPath, TImporter Importer)
	{
		FBridgeDataTableImportResult Out;
		Out.AssetPath = AssetPath;
		if (!DT) return Out;

		FScopedTransaction Transaction(LOCTEXT("ImportRowsText", "Import DataTable Rows"));
		DT->Modify();
		FDataTableEditorUtils::BroadcastPreChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

		const TArray<FString> Errors = Importer(DT);

		FDataTableEditorUtils::BroadcastPostChange(DT, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		DT->MarkPackageDirty();

		Out.Errors = Errors;
		Out.RowsImported = DT->GetRowMap().Num();
		Out.bSuccess = Errors.Num() == 0;
		return Out;
	}

	/** Create a brand-new UDataTable asset at AssetPath with RowStruct. Returns
	 *  nullptr (with a populated error string) if the asset already exists or
	 *  the package can't be created. */
	UDataTable* CreateDataTableAsset(const FString& AssetPath, UScriptStruct* RowStruct, FString& OutError)
	{
		if (!RowStruct) { OutError = TEXT("Row struct could not be resolved"); return nullptr; }
		if (AssetPath.IsEmpty()) { OutError = TEXT("AssetPath is empty"); return nullptr; }
		if (!AssetPath.StartsWith(TEXT("/")))
		{
			OutError = FString::Printf(TEXT("AssetPath must be a content path (e.g. /Game/...), got '%s'"), *AssetPath);
			return nullptr;
		}

		// Refuse to clobber an existing asset.
		if (LoadObject<UObject>(nullptr, *AssetPath))
		{
			OutError = FString::Printf(TEXT("Asset already exists at %s"), *AssetPath);
			return nullptr;
		}

		// /Game/Data/DT_Items → package name = full path, asset name = short tail.
		const FString AssetName = FPackageName::GetShortName(AssetPath);
		UPackage* Package = CreatePackage(*AssetPath);
		if (!Package) { OutError = TEXT("CreatePackage failed"); return nullptr; }
		Package->FullyLoad();

		UDataTable* DT = NewObject<UDataTable>(
			Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (!DT) { OutError = TEXT("NewObject<UDataTable> failed"); return nullptr; }

		DT->RowStruct = RowStruct;
		FAssetRegistryModule::AssetCreated(DT);
		Package->MarkPackageDirty();
		return DT;
	}
}

FBridgeDataTableImportResult UUnrealBridgeDataTableLibrary::ImportDataTableFromCSVText(
	const FString& DataTablePath, const FString& CsvContent)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	return BridgeDataTableImpl::RunImport(DT, DataTablePath,
		[&](UDataTable* T) { return T->CreateTableFromCSVString(CsvContent); });
}

FBridgeDataTableImportResult UUnrealBridgeDataTableLibrary::ImportDataTableFromJSONText(
	const FString& DataTablePath, const FString& JsonContent)
{
	UDataTable* DT = BridgeDataTableImpl::LoadDT(DataTablePath);
	return BridgeDataTableImpl::RunImport(DT, DataTablePath,
		[&](UDataTable* T) { return T->CreateTableFromJSONString(JsonContent); });
}

FBridgeDataTableImportResult UUnrealBridgeDataTableLibrary::CreateDataTableFromCSV(
	const FString& AssetPath, const FString& RowStructPath, const FString& CsvContent)
{
	FBridgeDataTableImportResult Out;
	Out.AssetPath = AssetPath;

	UScriptStruct* RowStruct = BridgeDataTableImpl::ResolveRowStruct(RowStructPath);
	FString CreateError;
	UDataTable* DT = BridgeDataTableImpl::CreateDataTableAsset(AssetPath, RowStruct, CreateError);
	if (!DT)
	{
		Out.Errors.Add(CreateError);
		return Out;
	}

	const TArray<FString> Errors = DT->CreateTableFromCSVString(CsvContent);
	Out.Errors = Errors;
	Out.RowsImported = DT->GetRowMap().Num();
	Out.bSuccess = Errors.Num() == 0;
	return Out;
}

FBridgeDataTableImportResult UUnrealBridgeDataTableLibrary::CreateDataTableFromJSON(
	const FString& AssetPath, const FString& RowStructPath, const FString& JsonContent)
{
	FBridgeDataTableImportResult Out;
	Out.AssetPath = AssetPath;

	UScriptStruct* RowStruct = BridgeDataTableImpl::ResolveRowStruct(RowStructPath);
	FString CreateError;
	UDataTable* DT = BridgeDataTableImpl::CreateDataTableAsset(AssetPath, RowStruct, CreateError);
	if (!DT)
	{
		Out.Errors.Add(CreateError);
		return Out;
	}

	const TArray<FString> Errors = DT->CreateTableFromJSONString(JsonContent);
	Out.Errors = Errors;
	Out.RowsImported = DT->GetRowMap().Num();
	Out.bSuccess = Errors.Num() == 0;
	return Out;
}

#undef LOCTEXT_NAMESPACE

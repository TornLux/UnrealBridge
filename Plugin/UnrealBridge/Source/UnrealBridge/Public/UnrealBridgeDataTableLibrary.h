#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeDataTableLibrary.generated.h"

/** A single row in a DataTable. */
USTRUCT(BlueprintType)
struct FBridgeDataTableRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString RowName;

	/** Key-value pairs: "PropertyName = Value" */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Fields;
};

/** Overview of a DataTable. */
USTRUCT(BlueprintType)
struct FBridgeDataTableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Row struct type name */
	UPROPERTY(BlueprintReadOnly)
	FString RowStructName;

	UPROPERTY(BlueprintReadOnly)
	int32 NumRows = 0;

	/** Column names (property names of the row struct) */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> ColumnNames;

	UPROPERTY(BlueprintReadOnly)
	TArray<FBridgeDataTableRow> Rows;
};

/**
 * DataTable introspection and editing via UnrealBridge.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeDataTableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ─── Read ─────────────────────────────────────────────

	/**
	 * Get all rows from a DataTable as structured text.
	 * Returns row struct info, column names, and all rows with field values.
	 *
	 * ⚠️ HIGH TOKEN COST — dumps the entire table including all nested struct text.
	 * Avoid unless absolutely necessary. Prefer the cheaper alternatives first:
	 *   • GetDataTableSummary       — schema only (no row data)
	 *   • GetDataTableRowNames      — just the row keys
	 *   • GetDataTableRow           — a single row
	 *   • GetDataTableRowField      — a single field
	 *   • GetDataTableColumn        — one column across all rows
	 *   • GetDataTableRowsFiltered  — precise row/column slicing
	 *   • SearchDataTableRows       — locate rows by keyword first
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static FBridgeDataTableInfo GetDataTableRows(const FString& DataTablePath);

	/**
	 * Get just the DataTable summary: name, row struct, num rows, column names.
	 * Does NOT include row data — use this when you only need schema info.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static FBridgeDataTableInfo GetDataTableSummary(const FString& DataTablePath);

	/** Return only row names (keys) of a DataTable. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static TArray<FString> GetDataTableRowNames(const FString& DataTablePath);

	/** Return a single row by name (empty Fields if not found). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static FBridgeDataTableRow GetDataTableRow(const FString& DataTablePath, const FString& RowName);

	/** Return a single field value from a row (empty string if not found). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static FString GetDataTableRowField(const FString& DataTablePath, const FString& RowName, const FString& FieldName);

	/**
	 * Return values of a single column across all rows as "RowName = Value" entries.
	 * (Parallel arrays would require USTRUCT; using a flat list keeps it simple.)
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static TArray<FString> GetDataTableColumn(const FString& DataTablePath, const FString& FieldName);

	/**
	 * Filtered view of a DataTable.
	 * @param RowFilter       If non-empty, only include rows whose name is in this list.
	 * @param ColumnFilter    If non-empty, only include these columns per row.
	 *
	 * ⚠️ With BOTH filters empty this is equivalent to GetDataTableRows and has
	 * the same HIGH TOKEN COST. Always pass at least one filter. If you only need
	 * a single row/field/column, prefer GetDataTableRow / GetDataTableRowField /
	 * GetDataTableColumn instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static FBridgeDataTableInfo GetDataTableRowsFiltered(
		const FString& DataTablePath,
		const TArray<FString>& RowFilter,
		const TArray<FString>& ColumnFilter);

	/**
	 * Search for rows whose values contain the keyword (case-insensitive substring).
	 * @param ColumnFilter    If non-empty, only search in these columns.
	 * Returns matching row names.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static TArray<FString> SearchDataTableRows(
		const FString& DataTablePath,
		const FString& Keyword,
		const TArray<FString>& ColumnFilter);

	/** Find all DataTable asset paths that use the given row struct (by struct name). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static TArray<FString> GetDataTablesUsingStruct(const FString& RowStructName);

	// ─── Write ────────────────────────────────────────────

	/** Set a single field on a row by exported-text value. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool SetDataTableRowField(
		const FString& DataTablePath,
		const FString& RowName,
		const FString& FieldName,
		const FString& ExportedValue);

	/**
	 * Add a new row to the DataTable.
	 * @param FieldValues  Map of FieldName -> exported-text value.
	 *                     Unspecified fields keep struct defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool AddDataTableRow(
		const FString& DataTablePath,
		const FString& RowName,
		const TMap<FString, FString>& FieldValues);

	/** Remove a row from the DataTable. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool RemoveDataTableRow(const FString& DataTablePath, const FString& RowName);

	/** Duplicate an existing row under a new name. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool DuplicateDataTableRow(
		const FString& DataTablePath,
		const FString& SourceRowName,
		const FString& NewRowName);

	/** Rename a row key. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool RenameDataTableRow(
		const FString& DataTablePath,
		const FString& OldRowName,
		const FString& NewRowName);

	/** Reorder the DataTable rows to match OrderedNames (missing names left in current order at end). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool ReorderDataTableRows(
		const FString& DataTablePath,
		const TArray<FString>& OrderedNames);

	/** Export the DataTable to a CSV file (absolute path). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool ExportDataTableToCSV(const FString& DataTablePath, const FString& OutCsvFilePath);

	/** Import rows from a CSV file (absolute path). Replaces existing rows. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool ImportDataTableFromCSV(const FString& DataTablePath, const FString& CsvFilePath);

	// ─── Extra reads / bulk write / JSON IO ───────────────────

	/** Cheap row-exists check (no field data returned). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool DoesDataTableRowExist(const FString& DataTablePath, const FString& RowName);

	/**
	 * Atomically set multiple fields on a single row from an exported-text map.
	 * Unknown fields are ignored; the row must already exist. One scoped transaction
	 * covers the whole change.
	 * @return true if the row existed and at least one field was updated
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool SetDataTableRowFields(
		const FString& DataTablePath,
		const FString& RowName,
		const TMap<FString, FString>& FieldValues);

	/** Return the full DataTable as a pretty-printed JSON string. */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static FString GetDataTableAsJSONString(const FString& DataTablePath);

	/** Export the DataTable to a pretty-printed JSON file (absolute path). */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool ExportDataTableToJSON(const FString& DataTablePath, const FString& OutJsonFilePath);

	/**
	 * Import rows from a JSON file (absolute path). Replaces existing rows.
	 * Returns false if the file is unreadable or the importer reports errors.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|DataTable")
	static bool ImportDataTableFromJSON(const FString& DataTablePath, const FString& JsonFilePath);
};

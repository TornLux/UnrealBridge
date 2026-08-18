#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "DataTableUtils.h"
#include "Dom/JsonObject.h"
#include "EdGraphSchema_K2.h"
#include "Engine/DataTable.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealBridgeDataTableLibrary.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

#if UE_VERSION_OLDER_THAN(5, 5, 0)
#include "Engine/UserDefinedStruct.h"
#else
#include "StructUtils/UserDefinedStruct.h"
#endif

namespace UnrealBridgeDataTableJsonTests
{
	UUserDefinedStruct* CreateTransientRowStruct()
	{
		const FName StructName = MakeUniqueObjectName(
			GetTransientPackage(), UUserDefinedStruct::StaticClass(), TEXT("UB_DataTableJsonRow"));
		UUserDefinedStruct* RowStruct = FStructureEditorUtils::CreateUserDefinedStruct(
			GetTransientPackage(), StructName, RF_Transient);
		if (!RowStruct)
		{
			return nullptr;
		}

		TArray<FStructVariableDescription>* Variables = FStructureEditorUtils::GetVarDescPtr(RowStruct);
		if (!Variables || Variables->Num() != 1)
		{
			return nullptr;
		}

		FEdGraphPinType StringType;
		StringType.PinCategory = UEdGraphSchema_K2::PC_String;
		const FGuid NameGuid = (*Variables)[0].VarGuid;
		if (!FStructureEditorUtils::ChangeVariableType(RowStruct, NameGuid, StringType)
			|| !FStructureEditorUtils::RenameVariable(RowStruct, NameGuid, TEXT("Name")))
		{
			return nullptr;
		}

		FEdGraphPinType IntType;
		IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
		if (!FStructureEditorUtils::AddVariable(RowStruct, IntType))
		{
			return nullptr;
		}
		Variables = FStructureEditorUtils::GetVarDescPtr(RowStruct);
		if (!Variables || Variables->Num() != 2
			|| !FStructureEditorUtils::RenameVariable(RowStruct, Variables->Last().VarGuid, TEXT("Damage")))
		{
			return nullptr;
		}

		FStructureEditorUtils::CompileStructure(RowStruct);
		return RowStruct;
	}

	UDataTable* CreateTransientTable(UUserDefinedStruct* RowStruct, const FString& ImportKeyField)
	{
		const FName TableName = MakeUniqueObjectName(
			GetTransientPackage(), UDataTable::StaticClass(), TEXT("UB_DataTableJson"));
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), TableName, RF_Transient);
		if (Table)
		{
			Table->RowStruct = RowStruct;
			Table->ImportKeyField = ImportKeyField;
		}
		return Table;
	}

	FProperty* FindAuthoredProperty(const UScriptStruct* RowStruct, const FString& AuthoredName)
	{
		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			if (DataTableUtils::GetPropertyExportName(*It) == AuthoredName)
			{
				return *It;
			}
		}
		return nullptr;
	}

	TSharedPtr<FJsonObject> ParseSingleRow(const FString& Json)
	{
		TArray<TSharedPtr<FJsonValue>> Rows;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Rows) || Rows.Num() != 1 || !Rows[0].IsValid())
		{
			return nullptr;
		}
		return Rows[0]->AsObject();
	}

	void Cleanup(UDataTable* Table)
	{
		if (Table)
		{
			Table->EmptyTable();
			Table->MarkAsGarbage();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeDataTableJsonRoundTripTest,
	"UnrealBridge.DataTable.JSON.AuthoredNamesAndRowKeyRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeDataTableJsonRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeDataTableJsonTests;
	(void)Parameters;

	UUserDefinedStruct* RowStruct = CreateTransientRowStruct();
	if (!TestNotNull(TEXT("Create a transient Blueprint UserDefinedStruct row type"), RowStruct))
	{
		return false;
	}
	UDataTable* Source = CreateTransientTable(RowStruct, TEXT("__RowName"));
	UDataTable* RoundTrip = nullptr;
	UDataTable* CollisionRoundTrip = nullptr;
	ON_SCOPE_EXIT
	{
		Cleanup(CollisionRoundTrip);
		Cleanup(RoundTrip);
		Cleanup(Source);
		RowStruct->MarkAsGarbage();
	};
	if (!TestNotNull(TEXT("Create a transient source DataTable"), Source))
	{
		return false;
	}

	const TArray<FString> SeedErrors = Source->CreateTableFromJSONString(
		TEXT("[{\"__RowName\":\"Row_A\",\"Name\":\"PayloadName\",\"Damage\":42}]"));
	TestEqual(TEXT("Seed JSON imports without errors"), SeedErrors.Num(), 0);
	if (SeedErrors.Num() != 0)
	{
		return false;
	}

	FProperty* NameProperty = FindAuthoredProperty(RowStruct, TEXT("Name"));
	FProperty* DamageProperty = FindAuthoredProperty(RowStruct, TEXT("Damage"));
	TestNotNull(TEXT("Authored Name property is discoverable"), NameProperty);
	TestNotNull(TEXT("Authored Damage property is discoverable"), DamageProperty);
	TestTrue(TEXT("UserDefinedStruct keeps a GUID-mangled internal property name"),
		NameProperty && NameProperty->GetName() != TEXT("Name"));

	const FString Exported = UUnrealBridgeDataTableLibrary::GetDataTableAsJSONString(Source->GetPathName());
	TSharedPtr<FJsonObject> ExportedRow = ParseSingleRow(Exported);
	if (!TestTrue(TEXT("Production JSON export returns one object row"), ExportedRow.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("Configured row-key field is exported"), ExportedRow->GetStringField(TEXT("__RowName")), FString(TEXT("Row_A")));
	TestEqual(TEXT("Authored Name column is exported"), ExportedRow->GetStringField(TEXT("Name")), FString(TEXT("PayloadName")));
	TestEqual(TEXT("Authored Damage column is exported as a number"), ExportedRow->GetIntegerField(TEXT("Damage")), 42);
	TestFalse(TEXT("GUID-mangled Name key is never exposed"), NameProperty && ExportedRow->HasField(NameProperty->GetName()));
	TestFalse(TEXT("GUID-mangled Damage key is never exposed"), DamageProperty && ExportedRow->HasField(DamageProperty->GetName()));

	RoundTrip = CreateTransientTable(RowStruct, TEXT("__RowName"));
	if (!TestNotNull(TEXT("Create a transient round-trip DataTable"), RoundTrip))
	{
		return false;
	}
	const TArray<FString> RoundTripErrors = RoundTrip->CreateTableFromJSONString(Exported);
	TestEqual(TEXT("Authored-name JSON re-imports without errors"), RoundTripErrors.Num(), 0);
	uint8* const* RoundTripRow = RoundTrip->GetRowMap().Find(TEXT("Row_A"));
	if (!TestNotNull(TEXT("Round trip preserves the DataTable row key"), RoundTripRow ? *RoundTripRow : nullptr))
	{
		return false;
	}
	const FStrProperty* NameStringProperty = CastField<FStrProperty>(NameProperty);
	const FIntProperty* DamageIntProperty = CastField<FIntProperty>(DamageProperty);
	TestNotNull(TEXT("Name uses a string property"), NameStringProperty);
	TestNotNull(TEXT("Damage uses an integer property"), DamageIntProperty);
	if (NameStringProperty && DamageIntProperty)
	{
		TestEqual(TEXT("Round trip preserves the Name payload"),
			NameStringProperty->GetPropertyValue_InContainer(*RoundTripRow), FString(TEXT("PayloadName")));
		TestEqual(TEXT("Round trip preserves the Damage payload"),
			DamageIntProperty->GetPropertyValue_InContainer(*RoundTripRow), 42);
	}

	// An empty ImportKeyField uses Unreal's standard "Name" row key. The
	// colliding property must not overwrite Row_A with its PayloadName value.
	Source->ImportKeyField.Reset();
	AddExpectedError(TEXT("JSON row-key field 'Name' conflicts with row property"),
		EAutomationExpectedErrorFlags::Contains, 1);
	const FString CollisionJson = UUnrealBridgeDataTableLibrary::GetDataTableAsJSONString(Source->GetPathName());
	TSharedPtr<FJsonObject> CollisionRow = ParseSingleRow(CollisionJson);
	if (!TestTrue(TEXT("Collision-safe export remains valid JSON"), CollisionRow.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("Row key wins the Name collision"), CollisionRow->GetStringField(TEXT("Name")), FString(TEXT("Row_A")));
	TestEqual(TEXT("Only the row key and non-conflicting column are emitted"), CollisionRow->Values.Num(), 2);
	TestFalse(TEXT("Conflicting payload cannot overwrite the row key"), CollisionJson.Contains(TEXT("PayloadName")));

	CollisionRoundTrip = CreateTransientTable(RowStruct, FString());
	if (!TestNotNull(TEXT("Create a default-key collision round-trip table"), CollisionRoundTrip))
	{
		return false;
	}
	const TArray<FString> CollisionErrors = CollisionRoundTrip->CreateTableFromJSONString(CollisionJson);
	TestEqual(TEXT("Collision-safe JSON re-imports without errors"), CollisionErrors.Num(), 0);
	TestTrue(TEXT("Collision-safe round trip preserves Row_A"), CollisionRoundTrip->GetRowMap().Contains(TEXT("Row_A")));
	TestFalse(TEXT("Collision-safe round trip never substitutes the payload as a row key"),
		CollisionRoundTrip->GetRowMap().Contains(TEXT("PayloadName")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeDataTableJsonUtf8Test,
	"UnrealBridge.DataTable.JSON.FileExportUsesUtf8WithoutBom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeDataTableJsonUtf8Test::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeDataTableJsonTests;
	(void)Parameters;

	UUserDefinedStruct* RowStruct = CreateTransientRowStruct();
	UDataTable* Table = RowStruct ? CreateTransientTable(RowStruct, TEXT("__RowName")) : nullptr;
	const FString JsonPath = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(), TEXT("UnrealBridgeDataTableJson_"), TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*JsonPath, false, true, true);
		Cleanup(Table);
		if (RowStruct)
		{
			RowStruct->MarkAsGarbage();
		}
	};
	if (!TestNotNull(TEXT("Create a transient UTF-8 test row struct"), RowStruct)
		|| !TestNotNull(TEXT("Create a transient UTF-8 test table"), Table))
	{
		return false;
	}

	const TArray<FString> SeedErrors = Table->CreateTableFromJSONString(
		TEXT("[{\"__RowName\":\"Row_Unicode\",\"Name\":\"\\u5251\",\"Damage\":7}]"));
	TestEqual(TEXT("Unicode seed JSON imports without errors"), SeedErrors.Num(), 0);
	if (SeedErrors.Num() != 0)
	{
		return false;
	}

	TestTrue(TEXT("Production file export succeeds"),
		UUnrealBridgeDataTableLibrary::ExportDataTableToJSON(Table->GetPathName(), JsonPath));
	TArray<uint8> Bytes;
	TestTrue(TEXT("Exported JSON bytes are readable"), FFileHelper::LoadFileToArray(Bytes, *JsonPath));
	TestTrue(TEXT("Exported JSON contains data"), Bytes.Num() > 3);
	if (Bytes.Num() >= 3)
	{
		TestFalse(TEXT("Exported JSON has no UTF-8 BOM"),
			Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF);
	}
	FString Reloaded;
	TestTrue(TEXT("Exported Unicode JSON decodes through Unreal"), FFileHelper::LoadFileToString(Reloaded, *JsonPath));
	TestTrue(TEXT("Unicode payload survives file export"), Reloaded.Contains(TEXT("\u5251")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

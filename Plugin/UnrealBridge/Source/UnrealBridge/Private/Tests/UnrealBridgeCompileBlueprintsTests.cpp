#include "UnrealBridgeEditorLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeCompileBlueprintsStructuralInvalidationTest,
	"UnrealBridge.Editor.CompileBlueprints.StructuralInvalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeCompileBlueprintsStructuralInvalidationTest::RunTest(const FString& Parameters)
{
	const FString PackagePath = FString::Printf(
		TEXT("/Temp/UnrealBridgeCompileBlueprints_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UPackage* BlueprintPackage = CreatePackage(*PackagePath);
	if (!TestNotNull(TEXT("CompileBlueprints test creates a transient package"), BlueprintPackage))
	{
		return false;
	}
	BlueprintPackage->SetFlags(RF_Transient);

	const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		UObject::StaticClass(),
		BlueprintPackage,
		BlueprintName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("UnrealBridgeCompileBlueprintsTests"));
	if (!TestNotNull(TEXT("CompileBlueprints test creates a transient Blueprint"), Blueprint))
	{
		BlueprintPackage->MarkAsGarbage();
		return false;
	}
	Blueprint->SetFlags(RF_Transient);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// 测试对象只存在于瞬态包中；离开作用域时清理生成类并执行垃圾回收，避免重复运行互相污染。
	// The fixture exists only in a transient package; clean up generated classes and collect garbage so repeated runs remain isolated.
	ON_SCOPE_EXIT
	{
		BlueprintPackage->SetDirtyFlag(false);
		if (Blueprint->GeneratedClass)
		{
			Blueprint->GeneratedClass->MarkAsGarbage();
		}
		if (Blueprint->SkeletonGeneratedClass)
		{
			Blueprint->SkeletonGeneratedClass->MarkAsGarbage();
		}
		Blueprint->MarkAsGarbage();
		BlueprintPackage->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
	};

	const FName ReflectedMemberName(TEXT("BridgeStructuralMember"));
	if (!TestNotNull(
			TEXT("Initial Blueprint compile produces a generated class"),
			Blueprint->GeneratedClass.Get())
		|| !TestNotNull(
			TEXT("Initial Blueprint compile produces a skeleton class"),
			Blueprint->SkeletonGeneratedClass.Get()))
	{
		return false;
	}
	TestNull(
		TEXT("Generated class does not contain the member before mutation"),
		FindFProperty<FIntProperty>(Blueprint->GeneratedClass.Get(), ReflectedMemberName));

	// 模拟只标记普通修改的程序化成员编辑，刻意绕过会自动结构化失效的高层 AddMemberVariable API。
	// Simulate a programmatic member edit marked only as modified, deliberately bypassing AddMemberVariable because it invalidates structure itself.
	FBPVariableDescription AddedMember;
	AddedMember.VarName = ReflectedMemberName;
	AddedMember.VarGuid = FGuid(0x4f39f476, 0xb18e42e5, 0x88a79b6f, 0x95a43519);
	AddedMember.VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
	Blueprint->NewVariables.Add(AddedMember);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TestNull(
		TEXT("Ordinary modification leaves the generated class structurally stale"),
		FindFProperty<FIntProperty>(Blueprint->GeneratedClass.Get(), ReflectedMemberName));
	TestNull(
		TEXT("Ordinary modification leaves the skeleton class structurally stale"),
		FindFProperty<FIntProperty>(Blueprint->SkeletonGeneratedClass.Get(), ReflectedMemberName));

	TArray<FString> BlueprintPaths;
	BlueprintPaths.Add(Blueprint->GetPathName());
	const TArray<FBridgeCompileResult> Results =
		UUnrealBridgeEditorLibrary::CompileBlueprints(BlueprintPaths);

	if (!TestEqual(TEXT("CompileBlueprints returns one result"), Results.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("CompileBlueprints reports success"), Results[0].bSuccess);
	TestEqual(
		TEXT("CompileBlueprints preserves the requested object path"),
		Results[0].Path,
		BlueprintPaths[0]);
	TestNotNull(
		TEXT("CompileBlueprints exposes the new member on the generated class"),
		FindFProperty<FIntProperty>(Blueprint->GeneratedClass.Get(), ReflectedMemberName));
	TestNotNull(
		TEXT("CompileBlueprints exposes the new member on the skeleton class"),
		FindFProperty<FIntProperty>(Blueprint->SkeletonGeneratedClass.Get(), ReflectedMemberName));

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS

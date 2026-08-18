#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "Editor.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Paths.h"
#include "UnrealBridgeMaterialLibrary.h"
#include "UnrealBridgeMaterialParameterHelpers.h"
#include "UObject/Package.h"

using namespace BridgeMaterialParameterHelpers;

namespace UnrealBridgeMaterialParameterTests
{
	static FBridgeMIParamSet MakeRequest(
		const TCHAR* Name,
		const TCHAR* Type,
		const TCHAR* Value,
		const TCHAR* Association = TEXT("Global"),
		int32 Index = INDEX_NONE)
	{
		FBridgeMIParamSet Request;
		Request.Name = Name;
		Request.Type = Type;
		Request.Value = Value;
		Request.Association = Association;
		Request.Index = Index;
		return Request;
	}

	/**
	 * 真实瞬态材质夹具通过编辑器生产路径注册参数，并保留包以检查脏状态。
	 * A real transient material fixture registers parameters through editor production paths and retains its package for dirty-state checks.
	 */
	struct FTransientMaterialFixture
	{
		UPackage* Package = nullptr;
		UMaterial* Material = nullptr;
		UMaterialInstanceConstant* Instance = nullptr;
		FGuid StaticSwitchGuid;

		FTransientMaterialFixture()
		{
			const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Package = CreatePackage(*FString::Printf(TEXT("/Temp/UnrealBridgeMaterialParameterTest_%s"), *Suffix));
			Material = NewObject<UMaterial>(Package, *FString::Printf(TEXT("M_%s"), *Suffix), RF_Transient | RF_Transactional);

			UMaterialExpressionScalarParameter* Scalar = CastChecked<UMaterialExpressionScalarParameter>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass()));
			Scalar->ParameterName = TEXT("ScalarParam");
			Scalar->DefaultValue = 0.1f;

			UMaterialExpressionVectorParameter* Vector = CastChecked<UMaterialExpressionVectorParameter>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass()));
			Vector->ParameterName = TEXT("VectorParam");
			Vector->DefaultValue = FLinearColor::Black;

			UMaterialExpressionTextureSampleParameter2D* Texture = CastChecked<UMaterialExpressionTextureSampleParameter2D>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass()));
			Texture->ParameterName = TEXT("TextureParam");

			UMaterialExpressionStaticSwitchParameter* StaticSwitch = CastChecked<UMaterialExpressionStaticSwitchParameter>(
				UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionStaticSwitchParameter::StaticClass()));
			StaticSwitch->ParameterName = TEXT("StaticSwitchParam");
			StaticSwitch->DefaultValue = false;
			StaticSwitch->ExpressionGUID = FGuid::NewGuid();
			StaticSwitchGuid = StaticSwitch->ExpressionGUID;

			Material->PostEditChange();
			Instance = NewObject<UMaterialInstanceConstant>(Package, *FString::Printf(TEXT("MI_%s"), *Suffix), RF_Transient | RF_Transactional);
			Instance->SetParentEditorOnly(Material);
			Instance->PostEditChange();
			Package->SetDirtyFlag(false);
		}

		FString InstancePath() const
		{
			return Instance->GetPathName();
		}
	};

	static const FBridgeMaterialParam* FindReadParameter(
		const FBridgeMaterialInstanceInfo& Info,
		const TCHAR* Type,
		const FMaterialParameterInfo& Identity)
	{
		return Info.Parameters.FindByPredicate([Type, &Identity](const FBridgeMaterialParam& Parameter)
		{
			return Parameter.ParamType == Type
				&& Parameter.Name == Identity.Name.ToString()
				&& Parameter.Association == AssociationToString(Identity.Association)
				&& Parameter.Index == Identity.Index;
		});
	}

	static void AddScalarOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& Identity,
		float Value)
	{
		FScalarParameterValue& Entry = Instance->ScalarParameterValues.AddDefaulted_GetRef();
		Entry.ParameterInfo = Identity;
		Entry.ParameterValue = Value;
	}

	static void AddVectorOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& Identity,
		const FLinearColor& Value)
	{
		FVectorParameterValue& Entry = Instance->VectorParameterValues.AddDefaulted_GetRef();
		Entry.ParameterInfo = Identity;
		Entry.ParameterValue = Value;
	}
}

/**
 * 使用纯参数身份验证同名参数不会跨 Global/Layer/Blend 关联匹配。
 * Verify with parameter identities that duplicate names never match across Global/Layer/Blend associations.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeMaterialParameterIdentityTest,
	"UnrealBridge.Material.Parameters.ExactAssociationIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeMaterialParameterIdentityTest::RunTest(const FString& Parameters)
{
	const FName DuplicateName(TEXT("Duplicate"));
	const FMaterialParameterInfo Global(DuplicateName, EMaterialParameterAssociation::GlobalParameter, INDEX_NONE);
	const FMaterialParameterInfo LayerOne(DuplicateName, EMaterialParameterAssociation::LayerParameter, 1);
	const FMaterialParameterInfo BlendZero(DuplicateName, EMaterialParameterAssociation::BlendParameter, 0);
	const TArray<FMaterialParameterInfo> Infos{Global, LayerOne, BlendZero};

	FMaterialParameterInfo Parsed;
	FString Error;
	TestTrue(TEXT("Omitted association resolves to Global/-1"),
		TryMakeParameterInfo(TEXT("Duplicate"), TEXT(""), INDEX_NONE, Parsed, Error));
	TestTrue(TEXT("Omitted selector is exactly global"), Parsed == Global);
	TestTrue(TEXT("Exact layer selector is found"), ContainsExactInfo(Infos, LayerOne));
	TestFalse(TEXT("Missing layer index does not fall back to a duplicate name"),
		ContainsExactInfo(Infos, FMaterialParameterInfo(DuplicateName, EMaterialParameterAssociation::LayerParameter, 2)));

	Error.Reset();
	TestFalse(TEXT("Global selector rejects a non-global index"),
		TryMakeParameterInfo(TEXT("Duplicate"), TEXT("Global"), 0, Parsed, Error));
	TestFalse(TEXT("Invalid selector reports an error"), Error.IsEmpty());
	return true;
}

/**
 * 差异与组合预览必须使用完整参数身份，并在原子设置失败时停止而不是渲染旧状态。
 * Diff and combined preview must use complete parameter identity and stop after atomic-set failure instead of rendering stale state.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeMaterialParameterDiffAndPreviewFailureTest,
	"UnrealBridge.Material.Parameters.DiffIdentityAndPreviewFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeMaterialParameterDiffAndPreviewFailureTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeMaterialParameterTests;

	FTransientMaterialFixture Fixture;
	UMaterialInstanceConstant* DiffA = NewObject<UMaterialInstanceConstant>(
		Fixture.Package, TEXT("MI_DiffA"), RF_Transient | RF_Transactional);
	UMaterialInstanceConstant* DiffB = NewObject<UMaterialInstanceConstant>(
		Fixture.Package, TEXT("MI_DiffB"), RF_Transient | RF_Transactional);
	DiffA->SetParentEditorOnly(Fixture.Material);
	DiffB->SetParentEditorOnly(Fixture.Material);

	const FName AssociationName(TEXT("SameNameAcrossAssociations"));
	const FMaterialParameterInfo Global(AssociationName, EMaterialParameterAssociation::GlobalParameter, INDEX_NONE);
	const FMaterialParameterInfo LayerOne(AssociationName, EMaterialParameterAssociation::LayerParameter, 1);
	const FMaterialParameterInfo BlendZero(AssociationName, EMaterialParameterAssociation::BlendParameter, 0);
	AddScalarOverride(DiffA, Global, 1.0f);
	AddScalarOverride(DiffA, LayerOne, 2.0f);
	AddScalarOverride(DiffA, BlendZero, 3.0f);
	AddScalarOverride(DiffB, Global, 1.0f);
	AddScalarOverride(DiffB, LayerOne, 20.0f);
	AddScalarOverride(DiffB, BlendZero, 3.0f);

	const FMaterialParameterInfo CrossType(FName(TEXT("SameNameAcrossTypes")));
	AddScalarOverride(DiffA, CrossType, 4.0f);
	AddScalarOverride(DiffB, CrossType, 5.0f);
	AddVectorOverride(DiffA, CrossType, FLinearColor::White);
	AddVectorOverride(DiffB, CrossType, FLinearColor::White);

	const FString Diff = UUnrealBridgeMaterialLibrary::DiffMIParams(DiffA->GetPathName(), DiffB->GetPathName());
	TArray<FString> DiffLines;
	Diff.ParseIntoArrayLines(DiffLines, false);
	TestEqual(TEXT("Only the two exact changed identities are reported"), DiffLines.Num(), 2);
	TestTrue(TEXT("Layer-scoped duplicate name remains independently visible"),
		Diff.Contains(TEXT("~ Scalar SameNameAcrossAssociations [LayerParameter,1]:")));
	TestTrue(TEXT("Scalar identity is not overwritten by a same-named vector"),
		Diff.Contains(TEXT("~ Scalar SameNameAcrossTypes [Global,-1]:")));
	TestFalse(TEXT("Equal global association is not falsely reported"),
		Diff.Contains(TEXT("SameNameAcrossAssociations [Global,-1]")));
	TestFalse(TEXT("Equal blend association is not falsely reported"),
		Diff.Contains(TEXT("SameNameAcrossAssociations [BlendParameter,0]")));

	int32 RenderCallCount = 0;
	FBridgeMIParamResult FailedSet;
	TestFalse(TEXT("Failed atomic set rejects the combined operation"),
		SetMIAndPreviewAfterSuccessfulSet(
			[&]() { return FailedSet; },
			[&]() { ++RenderCallCount; return true; }));
	TestEqual(TEXT("Failed atomic set never invokes the render operation"), RenderCallCount, 0);

	FBridgeMIParamResult SuccessfulSet;
	SuccessfulSet.bSuccess = true;
	TestTrue(TEXT("Successful atomic set forwards the render result"),
		SetMIAndPreviewAfterSuccessfulSet(
			[&]() { return SuccessfulSet; },
			[&]() { ++RenderCallCount; return true; }));
	TestEqual(TEXT("Successful atomic set invokes rendering exactly once"), RenderCallCount, 1);

	const FString PreviewPath = FPaths::Combine(
		FPaths::ProjectIntermediateDir(),
		FString::Printf(TEXT("UnrealBridgeRejectedPreview_%s.png"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	IFileManager::Get().Delete(*PreviewPath, false, true, true);
	Fixture.Package->SetDirtyFlag(false);
	const bool bPreviewResult = UUnrealBridgeMaterialLibrary::SetMIAndPreview(
		Fixture.InstancePath(),
		{MakeRequest(TEXT("MissingScalar"), TEXT("Scalar"), TEXT("1.0"))},
		TEXT("sphere"), TEXT("studio"), 32, 0.0f, 0.0f, 0.0f, PreviewPath);
	TestFalse(TEXT("Combined preview fails when the atomic parameter batch fails"), bPreviewResult);
	TestFalse(TEXT("Rejected parameter batch does not render a stale preview file"),
		IFileManager::Get().FileExists(*PreviewPath));
	TestFalse(TEXT("Rejected parameter batch does not dirty the material package"), Fixture.Package->IsDirty());
	IFileManager::Get().Delete(*PreviewPath, false, true, true);
	return true;
}

/**
 * 在真实瞬态 UMaterial/UMaterialInstanceConstant 上覆盖读取、原子预检、幂等、静态排列和事务撤销/重做。
 * Exercise readback, atomic preflight, idempotence, static permutations, and transaction undo/redo on real transient UMaterial/UMaterialInstanceConstant objects.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeMaterialParameterProductionTest,
	"UnrealBridge.Material.Parameters.ProductionSetReadAtomicityAndTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeMaterialParameterProductionTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeMaterialParameterTests;

	FTransientMaterialFixture Fixture;
	const FMaterialParameterInfo ScalarInfo(FName(TEXT("ScalarParam")));
	const FMaterialParameterInfo SwitchInfo(FName(TEXT("StaticSwitchParam")));

	// 查询接口必须逐项保留真实覆盖表中的完整关联身份。
	// The query API must preserve complete association identity from real override tables entry by entry.
	const FName QueryName(TEXT("AssociationQueryOnly"));
	for (const FMaterialParameterInfo& Info : {
		FMaterialParameterInfo(QueryName, EMaterialParameterAssociation::GlobalParameter, INDEX_NONE),
		FMaterialParameterInfo(QueryName, EMaterialParameterAssociation::LayerParameter, 1),
		FMaterialParameterInfo(QueryName, EMaterialParameterAssociation::BlendParameter, 0)})
	{
		FScalarParameterValue& Entry = Fixture.Instance->ScalarParameterValues.AddDefaulted_GetRef();
		Entry.ParameterInfo = Info;
		Entry.ParameterValue = static_cast<float>(Info.Index + 2);
	}
	FBridgeMaterialInstanceInfo Read = UUnrealBridgeMaterialLibrary::GetMaterialInstanceParameters(Fixture.InstancePath());
	TestNotNull(TEXT("Global query identity is returned exactly"), FindReadParameter(Read, TEXT("Scalar"), FMaterialParameterInfo(QueryName, EMaterialParameterAssociation::GlobalParameter, INDEX_NONE)));
	TestNotNull(TEXT("Layer query identity is returned exactly"), FindReadParameter(Read, TEXT("Scalar"), FMaterialParameterInfo(QueryName, EMaterialParameterAssociation::LayerParameter, 1)));
	TestNotNull(TEXT("Blend query identity is returned exactly"), FindReadParameter(Read, TEXT("Scalar"), FMaterialParameterInfo(QueryName, EMaterialParameterAssociation::BlendParameter, 0)));

	FBridgeMIParamResult Initial = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("ScalarParam"), TEXT("Scalar"), TEXT("0.25")),
		MakeRequest(TEXT("VectorParam"), TEXT("Vector"), TEXT("(R=0.1,G=0.2,B=0.3,A=1.0)"))});
	TestTrue(TEXT("Initial real-MI batch succeeds"), Initial.bSuccess);
	TestEqual(TEXT("Initial batch applies both changes"), Initial.Applied, 2);
	Read = UUnrealBridgeMaterialLibrary::GetMaterialInstanceParameters(Fixture.InstancePath());
	const FBridgeMaterialParam* ScalarRead = FindReadParameter(Read, TEXT("Scalar"), ScalarInfo);
	TestNotNull(TEXT("Changed scalar is readable through production query"), ScalarRead);
	if (ScalarRead)
	{
		TestEqual(TEXT("Changed scalar readback is exact"), FCString::Atof(*ScalarRead->Value), 0.25f);
	}

	FBridgeMIParamResult Mixed = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("ScalarParam"), TEXT("Scalar"), TEXT("0.75")),
		MakeRequest(TEXT("VectorParam"), TEXT("Vector"), TEXT("(R=0.1,G=0.2,B=0.3,A=1.0)"))});
	TestTrue(TEXT("Mixed changed/unchanged batch succeeds"), Mixed.bSuccess);
	TestEqual(TEXT("Mixed batch counts only the changed entry"), Mixed.Applied, 1);
	TestEqual(TEXT("Changed mixed entry reports Applied"), Mixed.Outcomes[0].Status, FString(TEXT("Applied")));
	TestEqual(TEXT("Idempotent mixed entry reports Unchanged"), Mixed.Outcomes[1].Status, FString(TEXT("Unchanged")));
	TestFalse(TEXT("Unchanged mixed entry is not marked applied"), Mixed.Outcomes[1].bApplied);

	Fixture.Package->SetDirtyFlag(false);
	FBridgeMIParamResult Duplicate = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("ScalarParam"), TEXT("Scalar"), TEXT("0.9")),
		MakeRequest(TEXT("ScalarParam"), TEXT(""), TEXT("0.8")),
		MakeRequest(TEXT("VectorParam"), TEXT("Vector"), TEXT("(R=1,G=1,B=1,A=1)"))});
	TestFalse(TEXT("Duplicate typed identity rejects the entire batch"), Duplicate.bSuccess);
	TestEqual(TEXT("First duplicate is identified truthfully"), Duplicate.Outcomes[0].Status, FString(TEXT("Duplicate")));
	TestEqual(TEXT("Alias duplicate is identified truthfully"), Duplicate.Outcomes[1].Status, FString(TEXT("Duplicate")));
	TestEqual(TEXT("Otherwise-valid peer reports atomic NotApplied"), Duplicate.Outcomes[2].Status, FString(TEXT("NotApplied")));
	TestFalse(TEXT("Duplicate rejection does not dirty the package"), Fixture.Package->IsDirty());
	float ScalarValue = 0.0f;
	TestTrue(TEXT("Scalar remains queryable after duplicate rejection"), Fixture.Instance->GetScalarParameterValue(ScalarInfo, ScalarValue));
	TestEqual(TEXT("Duplicate rejection performs zero scalar mutation"), ScalarValue, 0.75f);

	Fixture.Package->SetDirtyFlag(false);
	AddExpectedError(
		TEXT("LoadPackage: SkipPackage: /Game/DefinitelyMissing/T_DoesNotExist"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	AddExpectedError(
		TEXT("Failed to find object 'Texture /Game/DefinitelyMissing/T_DoesNotExist.T_DoesNotExist'"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FBridgeMIParamResult LateFailure = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("ScalarParam"), TEXT("Scalar"), TEXT("0.95")),
		MakeRequest(TEXT("TextureParam"), TEXT("Texture"), TEXT("/Game/DefinitelyMissing/T_DoesNotExist.T_DoesNotExist"))});
	TestFalse(TEXT("Late texture load failure rejects the batch"), LateFailure.bSuccess);
	TestEqual(TEXT("Earlier valid entry reports NotApplied"), LateFailure.Outcomes[0].Status, FString(TEXT("NotApplied")));
	TestEqual(TEXT("Texture load failure reports InvalidValue"), LateFailure.Outcomes[1].Status, FString(TEXT("InvalidValue")));
	TestFalse(TEXT("Late failure does not dirty the package"), Fixture.Package->IsDirty());
	Fixture.Instance->GetScalarParameterValue(ScalarInfo, ScalarValue);
	TestEqual(TEXT("Late failure performs zero earlier mutation"), ScalarValue, 0.75f);

	FStaticParameterSet BeforeStatic;
	Fixture.Instance->GetStaticParameterValues(BeforeStatic);
	FStaticSwitchParameter* BeforeSwitch = FindExactStaticSwitch(BeforeStatic, SwitchInfo);
	TestNotNull(TEXT("Fixture exposes a real static switch"), BeforeSwitch);
	const FGuid OriginalSwitchGuid = BeforeSwitch ? BeforeSwitch->ExpressionGUID : Fixture.StaticSwitchGuid;
	FBridgeMIParamResult StaticResult = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("StaticSwitchParam"), TEXT("StaticSwitch"), TEXT("true"))});
	TestTrue(TEXT("Static switch update succeeds"), StaticResult.bSuccess);
	FStaticParameterSet AfterStatic;
	Fixture.Instance->GetStaticParameterValues(AfterStatic);
	FStaticSwitchParameter* AfterSwitch = FindExactStaticSwitch(AfterStatic, SwitchInfo);
	TestNotNull(TEXT("Static switch remains after UpdateStaticPermutation"), AfterSwitch);
	if (AfterSwitch)
	{
		TestTrue(TEXT("Static switch value and override are applied"), AfterSwitch->Value && AfterSwitch->bOverride);
		TestEqual(TEXT("UpdateStaticPermutation preserves ExpressionGUID"), AfterSwitch->ExpressionGUID, OriginalSwitchGuid);
	}

	FBridgeMIParamResult TransactionalChange = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("ScalarParam"), TEXT("Scalar"), TEXT("0.9"))});
	TestTrue(TEXT("Transactional scalar change succeeds"), TransactionalChange.bSuccess);
	Fixture.Package->SetDirtyFlag(false);
	FBridgeMIParamResult NoOp = UUnrealBridgeMaterialLibrary::SetMIParams(Fixture.InstancePath(), {
		MakeRequest(TEXT("ScalarParam"), TEXT("Scalar"), TEXT("0.9"))});
	TestTrue(TEXT("Idempotent request succeeds"), NoOp.bSuccess);
	TestEqual(TEXT("Idempotent request applies zero entries"), NoOp.Applied, 0);
	TestEqual(TEXT("Idempotent request reports Unchanged"), NoOp.Outcomes[0].Status, FString(TEXT("Unchanged")));
	TestFalse(TEXT("Idempotent request does not dirty the package"), Fixture.Package->IsDirty());

	TestNotNull(TEXT("Editor transaction subsystem is available"), GEditor);
	if (GEditor)
	{
		TestTrue(TEXT("Undo succeeds and skips any no-op transaction"), GEditor->UndoTransaction());
		Fixture.Instance->GetScalarParameterValue(ScalarInfo, ScalarValue);
		TestEqual(TEXT("Undo restores the preceding changed value"), ScalarValue, 0.75f);
		TestTrue(TEXT("Redo succeeds"), GEditor->RedoTransaction());
		Fixture.Instance->GetScalarParameterValue(ScalarInfo, ScalarValue);
		TestEqual(TEXT("Redo reapplies the changed value"), ScalarValue, 0.9f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && UE 5.7+

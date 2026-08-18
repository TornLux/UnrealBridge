#include "Misc/EngineVersionComparison.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "UnrealBridgeMaterialLibrary.h"
#include "UnrealBridgeTextureRefreshOperations.h"

#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComposite.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionPinBase.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace BridgeTextureRefreshTests
{
	/**
	 * 为每个用例创建独立的临时包，使对象路径可解析且包标脏状态不会泄漏到共享 TransientPackage。
	 * Creates an independent temporary package per case so object paths resolve and dirty state never leaks through the shared TransientPackage.
	 */
	static TStrongObjectPtr<UPackage> MakePackage(const TCHAR* BaseName, bool bInitiallyDirty = false)
	{
		const FString PackageName = FString::Printf(
			TEXT("/Temp/UnrealBridgeTests/%s_%s"),
			BaseName,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		TStrongObjectPtr<UPackage> Package(CreatePackage(*PackageName));
		if (Package.IsValid())
		{
			Package->SetDirtyFlag(bInitiallyDirty);
		}
		return Package;
	}

	/**
	 * 在独立测试包中创建临时贴图，并在准备完成后恢复该包原有的标脏状态。
	 * Creates a temporary texture in an independent test package and restores the package's original dirty state after setup.
	 */
	static TStrongObjectPtr<UTexture2D> MakeTexture(UPackage& Package, const TCHAR* BaseName)
	{
		const bool bInitialDirty = Package.IsDirty();
		const FName Name = MakeUniqueObjectName(&Package, UTexture2D::StaticClass(), FName(BaseName));
		TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>(&Package, Name, RF_Transient));
		Package.SetDirtyFlag(bInitialDirty);
		return Texture;
	}

	enum class ERecordedCall : uint8
	{
		IsCompiling,
		BlockOnAnyAsyncBuild,
		UpdateResource,
		UpdateResourceWithParams,
	};

	/**
	 * 仅记录刷新分支的调用顺序和参数，不触发渲染资源或异步派生数据工作。
	 * Records refresh-branch ordering and flags without starting render-resource or asynchronous derived-data work.
	 */
	class FRecordingOperations final : public IUnrealBridgeTextureRefreshOperations
	{
	public:
		explicit FRecordingOperations(bool bInCompiling)
			: bCompiling(bInCompiling)
		{
		}

		virtual bool IsCompilingTexture(UTexture& Texture) const override
		{
			Calls.Add(ERecordedCall::IsCompiling);
			return bCompiling;
		}

		virtual void BlockOnAnyAsyncBuild(UTexture& Texture) const override
		{
			Calls.Add(ERecordedCall::BlockOnAnyAsyncBuild);
		}

		virtual void UpdateResource(UTexture& Texture) const override
		{
			Calls.Add(ERecordedCall::UpdateResource);
		}

		virtual void UpdateResourceWithParams(UTexture& Texture, UTexture::EUpdateResourceFlags Flags) const override
		{
			Calls.Add(ERecordedCall::UpdateResourceWithParams);
			UpdateFlags = Flags;
		}

		bool bCompiling = false;
		mutable TArray<ERecordedCall> Calls;
		mutable UTexture::EUpdateResourceFlags UpdateFlags = UTexture::EUpdateResourceFlags::None;
	};

	static bool TestCall(
		FAutomationTestBase& Test,
		const TArray<ERecordedCall>& Calls,
		int32 Index,
		ERecordedCall Expected,
		const TCHAR* Description)
	{
		return Test.TestEqual(
			Description,
			Index < Calls.Num() ? static_cast<uint8>(Calls[Index]) : MAX_uint8,
			static_cast<uint8>(Expected));
	}
}

namespace BridgeMaterialGraphSafetyTests
{
	static TStrongObjectPtr<UMaterial> MakeMaterial(UPackage& Package, const TCHAR* BaseName)
	{
		const FName Name = MakeUniqueObjectName(&Package, UMaterial::StaticClass(), FName(BaseName));
		return TStrongObjectPtr<UMaterial>(NewObject<UMaterial>(
			&Package, Name, RF_Transient | RF_Transactional));
	}

	template <typename TExpression>
	static TExpression* AddExpression(UMaterial& Material, int32 X = 0, int32 Y = 0)
	{
		TExpression* Expression = Cast<TExpression>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				&Material, TExpression::StaticClass(), X, Y));
		if (Expression && !Expression->MaterialExpressionGuid.IsValid())
		{
			Expression->MaterialExpressionGuid = FGuid::NewGuid();
		}
		return Expression;
	}

	template <typename TExpression>
	static TExpression* AddInternalExpression(UMaterial& Material, UMaterialExpressionComposite& Composite)
	{
		TExpression* Expression = NewObject<TExpression>(
			&Material, NAME_None, RF_Transient | RF_Transactional);
		Expression->Material = &Material;
		Expression->SubgraphExpression = &Composite;
		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Material.GetExpressionCollection().AddExpression(Expression);
		return Expression;
	}

	static FBridgeMaterialGraphOp MakeOp(const TCHAR* Name)
	{
		FBridgeMaterialGraphOp Op;
		Op.Op = Name;
		return Op;
	}

	static bool ContainsExpression(const UMaterial& Material, const FGuid& Guid)
	{
		for (const TObjectPtr<UMaterialExpression>& Expression : Material.GetExpressions())
		{
			if (Expression && Expression->MaterialExpressionGuid == Guid)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeTextureRefreshInvalidPathTest,
	"UnrealBridge.Material.TextureRefresh.InvalidAndNonTexturePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeTextureRefreshInvalidPathTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(
		TEXT("InvalidAndNonTexturePaths"), true);
	if (!TestTrue(TEXT("Independent test package was created"), Package.IsValid()))
	{
		return false;
	}

	const FString MissingPath = Package->GetName() + TEXT(".Missing");
	const FBridgeTextureRefreshResult Missing = UUnrealBridgeMaterialLibrary::RefreshTextureResource(
		MissingPath, false);
	TestFalse(TEXT("Missing object is not found"), Missing.bFound);
	TestFalse(TEXT("Missing object does not succeed"), Missing.bSuccess);
	TestFalse(TEXT("Missing object reports an error"), Missing.Error.IsEmpty());

	const bool bInitialDirty = Package->IsDirty();
	const FName ObjectName = MakeUniqueObjectName(Package.Get(), UMaterial::StaticClass(), TEXT("BridgeTextureRefreshNonTexture"));
	TStrongObjectPtr<UMaterial> NonTexture(NewObject<UMaterial>(Package.Get(), ObjectName, RF_Transient));
	Package->SetDirtyFlag(bInitialDirty);
	const FBridgeTextureRefreshResult WrongType = UUnrealBridgeMaterialLibrary::RefreshTextureResource(
		NonTexture->GetPathName(), false);
	TestTrue(TEXT("Non-texture object is found"), WrongType.bFound);
	TestFalse(TEXT("Non-texture object does not succeed"), WrongType.bSuccess);
	TestFalse(TEXT("Non-texture object reports an error"), WrongType.Error.IsEmpty());
	TestEqual(TEXT("Validation preserves the package's initial dirty state"), Package->IsDirty(), bInitialDirty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeTextureRefreshOrdinarySubmissionTest,
	"UnrealBridge.Material.TextureRefresh.OrdinarySubmissionOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeTextureRefreshOrdinarySubmissionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("OrdinarySubmission"), true);
	if (!TestTrue(TEXT("Independent test package was created"), Package.IsValid()))
	{
		return false;
	}
	TStrongObjectPtr<UTexture2D> Texture = BridgeTextureRefreshTests::MakeTexture(
		*Package, TEXT("BridgeTextureRefreshOrdinary"));
	if (!TestTrue(TEXT("Temporary texture was created"), Texture.IsValid()))
	{
		return false;
	}

	const bool bInitialDirty = Package->IsDirty();
	BridgeTextureRefreshTests::FRecordingOperations Operations(true);
	FBridgeTextureRefreshResult Result;
	Result.bFound = true;
	UnrealBridgeTextureRefresh::Submit(*Texture, false, Operations, Result);

	TestTrue(TEXT("Ordinary refresh remains found"), Result.bFound);
	TestTrue(TEXT("Success means the ordinary refresh was submitted"), Result.bSuccess);
	TestTrue(TEXT("Ordinary refresh records pre-submit compiling state"), Result.bWasCompiling);
	TestEqual(TEXT("Ordinary refresh records two calls"), Operations.Calls.Num(), 2);
	BridgeTextureRefreshTests::TestCall(*this, Operations.Calls, 0, BridgeTextureRefreshTests::ERecordedCall::IsCompiling,
		TEXT("Ordinary refresh checks compilation first"));
	BridgeTextureRefreshTests::TestCall(*this, Operations.Calls, 1, BridgeTextureRefreshTests::ERecordedCall::UpdateResource,
		TEXT("Ordinary refresh calls UpdateResource"));
	TestEqual(TEXT("Ordinary submission preserves initial package dirty state"), Package->IsDirty(), bInitialDirty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeTextureRefreshForcedSubmissionTest,
	"UnrealBridge.Material.TextureRefresh.ForcedSubmissionOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeTextureRefreshForcedSubmissionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("ForcedSubmission"));
	if (!TestTrue(TEXT("Independent test package was created"), Package.IsValid()))
	{
		return false;
	}
	TStrongObjectPtr<UTexture2D> Texture = BridgeTextureRefreshTests::MakeTexture(
		*Package, TEXT("BridgeTextureRefreshForced"));
	if (!TestTrue(TEXT("Temporary texture was created"), Texture.IsValid()))
	{
		return false;
	}

	const bool bInitialDirty = Package->IsDirty();
	BridgeTextureRefreshTests::FRecordingOperations Operations(false);
	FBridgeTextureRefreshResult Result;
	Result.bFound = true;
	UnrealBridgeTextureRefresh::Submit(*Texture, true, Operations, Result);

	TestTrue(TEXT("Forced refresh remains found"), Result.bFound);
	TestTrue(TEXT("Success means the forced refresh was submitted"), Result.bSuccess);
	TestFalse(TEXT("Forced refresh records pre-submit non-compiling state"), Result.bWasCompiling);
	TestEqual(TEXT("Forced refresh records three calls"), Operations.Calls.Num(), 3);
	BridgeTextureRefreshTests::TestCall(*this, Operations.Calls, 0, BridgeTextureRefreshTests::ERecordedCall::IsCompiling,
		TEXT("Forced refresh checks compilation first"));
	BridgeTextureRefreshTests::TestCall(*this, Operations.Calls, 1, BridgeTextureRefreshTests::ERecordedCall::BlockOnAnyAsyncBuild,
		TEXT("Forced refresh blocks its texture's async build second"));
	BridgeTextureRefreshTests::TestCall(*this, Operations.Calls, 2, BridgeTextureRefreshTests::ERecordedCall::UpdateResourceWithParams,
		TEXT("Forced refresh calls UpdateResourceWithParams last"));
	TestEqual(
		TEXT("Forced refresh passes only ForceRebuild"),
		static_cast<uint32>(Operations.UpdateFlags),
		static_cast<uint32>(UTexture::EUpdateResourceFlags::ForceRebuild));
	TestEqual(TEXT("Forced submission preserves initial package dirty state"), Package->IsDirty(), bInitialDirty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeMaterialGraphPreflightAtomicityTest,
	"UnrealBridge.Material.GraphSafety.PreflightLeavesLiveMaterialUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeMaterialGraphPreflightAtomicityTest::RunTest(const FString& Parameters)
{
	using namespace BridgeMaterialGraphSafetyTests;
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("GraphPreflight"));
	TStrongObjectPtr<UMaterial> Material = MakeMaterial(*Package, TEXT("M_GraphPreflight"));
	UMaterialExpressionConstant* Constant = AddExpression<UMaterialExpressionConstant>(*Material, -200, 0);
	UMaterialExpressionAdd* Add = AddExpression<UMaterialExpressionAdd>(*Material, 0, 0);
	if (!TestNotNull(TEXT("Constant created"), Constant)
		|| !TestNotNull(TEXT("Add created"), Add))
	{
		return false;
	}
	Constant->R = 1.25f;

	FBridgeMaterialGraphOp Set = MakeOp(TEXT("set_prop"));
	Set.DstRef = Constant->MaterialExpressionGuid.ToString();
	Set.Property = TEXT("R");
	Set.Value = TEXT("2.5");
	FBridgeMaterialGraphOp InvalidConnect = MakeOp(TEXT("connect"));
	InvalidConnect.SrcRef = Constant->MaterialExpressionGuid.ToString();
	InvalidConnect.DstRef = Add->MaterialExpressionGuid.ToString();
	InvalidConnect.DstInput = TEXT("DefinitelyMissing");
	const TArray<FBridgeMaterialGraphOp> Ops{Set, InvalidConnect};

	const FBridgeMaterialGraphOpResult Validation =
		UUnrealBridgeMaterialLibrary::ValidateMaterialGraphOps(Material->GetPathName(), Ops);
	TestTrue(TEXT("Validation is explicitly preflight-only"), Validation.bPreflightOnly);
	TestFalse(TEXT("Invalid pin fails validation"), Validation.bSuccess);
	TestEqual(TEXT("Validation identifies the invalid op"), Validation.FailedAtIndex, 1);
	TestEqual(TEXT("Validation does not mutate the live property"), Constant->R, 1.25f);

	const FBridgeMaterialGraphOpResult Apply =
		UUnrealBridgeMaterialLibrary::ApplyMaterialGraphOps(Material->GetPathName(), Ops, false);
	TestFalse(TEXT("Invalid batch is rejected"), Apply.bSuccess);
	TestFalse(TEXT("Preflight did not pass"), Apply.bPreflightPassed);
	TestFalse(TEXT("No partial live mutation is reported"), Apply.bPartialApplied);
	TestEqual(TEXT("No live ops were applied"), Apply.OpsApplied, 0);
	TestEqual(TEXT("Apply preserves the original property"), Constant->R, 1.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeMaterialGraphConnectedDeleteTest,
	"UnrealBridge.Material.GraphSafety.DeleteRequiresOrderedDisconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeMaterialGraphConnectedDeleteTest::RunTest(const FString& Parameters)
{
	using namespace BridgeMaterialGraphSafetyTests;
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("GraphDelete"));
	TStrongObjectPtr<UMaterial> Material = MakeMaterial(*Package, TEXT("M_GraphDelete"));
	UMaterialExpressionConstant* Constant = AddExpression<UMaterialExpressionConstant>(*Material, -200, 0);
	UMaterialExpressionAdd* Add = AddExpression<UMaterialExpressionAdd>(*Material, 0, 0);
	if (!Constant || !Add)
	{
		AddError(TEXT("Could not create graph expressions"));
		return false;
	}
	Add->A.Expression = Constant;
	const FGuid ConstantGuid = Constant->MaterialExpressionGuid;

	FBridgeMaterialGraphOp Delete = MakeOp(TEXT("delete"));
	Delete.DstRef = ConstantGuid.ToString();
	const FBridgeMaterialGraphOpResult Refused =
		UUnrealBridgeMaterialLibrary::ApplyMaterialGraphOps(
			Material->GetPathName(), TArray<FBridgeMaterialGraphOp>{Delete}, false);
	TestFalse(TEXT("Connected delete is refused"), Refused.bSuccess);
	TestTrue(TEXT("Delete diagnostic names a consumer"), Refused.Error.Contains(TEXT("consumed")));
	TestTrue(TEXT("Connected source remains in the graph"), ContainsExpression(*Material, ConstantGuid));
	TestTrue(TEXT("Connected input remains intact"), Add->A.Expression == Constant);

	FBridgeMaterialGraphOp Disconnect = MakeOp(TEXT("disconnect_in"));
	Disconnect.DstRef = Add->MaterialExpressionGuid.ToString();
	Disconnect.DstInput = TEXT("A");
	const FBridgeMaterialGraphOpResult Removed =
		UUnrealBridgeMaterialLibrary::ApplyMaterialGraphOps(
			Material->GetPathName(), TArray<FBridgeMaterialGraphOp>{Disconnect, Delete}, false);
	TestTrue(TEXT("Disconnect-then-delete succeeds"), Removed.bSuccess);
	TestTrue(TEXT("Preflight passed"), Removed.bPreflightPassed);
	TestTrue(TEXT("All live ops succeeded"), Removed.bOpsSuccess);
	TestFalse(TEXT("Removed source no longer exists"), ContainsExpression(*Material, ConstantGuid));
	TestNull(TEXT("Destination input is disconnected"), Add->A.Expression);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeMaterialGraphNamedRerouteTest,
	"UnrealBridge.Material.GraphSafety.NamedRerouteDependencyIsExplicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeMaterialGraphNamedRerouteTest::RunTest(const FString& Parameters)
{
	using namespace BridgeMaterialGraphSafetyTests;
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("GraphNamedReroute"));
	TStrongObjectPtr<UMaterial> Material = MakeMaterial(*Package, TEXT("M_GraphNamedReroute"));
	UMaterialExpressionNamedRerouteDeclaration* Declaration =
		AddExpression<UMaterialExpressionNamedRerouteDeclaration>(*Material, -200, 0);
	UMaterialExpressionNamedRerouteUsage* Usage =
		AddExpression<UMaterialExpressionNamedRerouteUsage>(*Material, 0, 0);
	UMaterialExpressionConstant* Source =
		AddExpression<UMaterialExpressionConstant>(*Material, -400, 0);
	if (!Declaration || !Usage || !Source)
	{
		AddError(TEXT("Could not create named reroute expressions"));
		return false;
	}
	Declaration->Name = TEXT("BridgeValue");
	Declaration->Input.Expression = Source;
	Usage->Declaration = Declaration;
	Usage->DeclarationGuid = Declaration->VariableGuid;
	FExpressionInput* Emissive = Material->GetExpressionInputForProperty(MP_EmissiveColor);
	if (!TestNotNull(TEXT("Emissive material input exists"), Emissive))
	{
		return false;
	}
	Emissive->Expression = Usage;

	const FBridgeMaterialGraph Graph =
		UUnrealBridgeMaterialLibrary::GetMaterialGraph(Material->GetPathName());
	TestTrue(TEXT("Resolved named reroute graph is complete"), Graph.bGraphComplete);
	const bool bHasLogicalEdge = Graph.Connections.ContainsByPredicate(
		[&](const FBridgeMaterialGraphConnection& Connection)
		{
			return Connection.EdgeKind == TEXT("named_reroute")
				&& Connection.SrcGuid == Declaration->MaterialExpressionGuid
				&& Connection.DstGuid == Usage->MaterialExpressionGuid;
		});
	TestTrue(TEXT("Declaration-to-usage dependency is explicit"), bHasLogicalEdge);
	const FBridgeMaterialAnalysis Analysis =
		UUnrealBridgeMaterialLibrary::AnalyzeMaterial(Material->GetPathName(), 0, 0);
	const bool bFalseUnusedFinding = Analysis.Findings.ContainsByPredicate(
		[&](const FBridgeMaterialFinding& Finding)
		{
			return Finding.RuleId == TEXT("M5-3")
				&& (Finding.ExpressionGuid == Source->MaterialExpressionGuid
					|| Finding.ExpressionGuid == Declaration->MaterialExpressionGuid
					|| Finding.ExpressionGuid == Usage->MaterialExpressionGuid);
		});
	TestFalse(TEXT("Named-reroute path is reachable to the material output"), bFalseUnusedFinding);
	TestFalse(
		TEXT("Single-delete API refuses a declaration with live usages"),
		UUnrealBridgeMaterialLibrary::DeleteMaterialExpression(
			Material->GetPathName(), Declaration->MaterialExpressionGuid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeMaterialGraphIncompleteDependencyTest,
	"UnrealBridge.Material.GraphSafety.IncompleteDependencyFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeMaterialGraphIncompleteDependencyTest::RunTest(const FString& Parameters)
{
	using namespace BridgeMaterialGraphSafetyTests;
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("GraphIncomplete"));
	TStrongObjectPtr<UMaterial> Material = MakeMaterial(*Package, TEXT("M_GraphIncomplete"));
	UMaterialExpressionNamedRerouteUsage* Usage =
		AddExpression<UMaterialExpressionNamedRerouteUsage>(*Material, 0, 0);
	if (!Usage)
	{
		AddError(TEXT("Could not create named reroute usage"));
		return false;
	}
	const int32 InitialCount = Material->GetExpressions().Num();
	const FBridgeMaterialGraph Graph =
		UUnrealBridgeMaterialLibrary::GetMaterialGraph(Material->GetPathName());
	TestFalse(TEXT("Unresolved named reroute marks graph incomplete"), Graph.bGraphComplete);
	TestTrue(TEXT("Opaque dependency diagnostic is present"), Graph.OpaqueDependencies.Num() > 0);

	FBridgeMaterialGraphOp Add = MakeOp(TEXT("add"));
	Add.ClassName = TEXT("Constant");
	const FBridgeMaterialGraphOpResult Refused =
		UUnrealBridgeMaterialLibrary::ApplyMaterialGraphOps(
			Material->GetPathName(), TArray<FBridgeMaterialGraphOp>{Add}, false);
	TestFalse(TEXT("All mutation fails closed on incomplete data"), Refused.bSuccess);
	TestFalse(TEXT("Incomplete preflight never passed"), Refused.bPreflightPassed);
	TestEqual(TEXT("Live expression collection is unchanged"), Material->GetExpressions().Num(), InitialCount);

	const FBridgeMaterialAutoFixResult AutoFix =
		UUnrealBridgeMaterialLibrary::AutoFixMaterial(
			Material->GetPathName(), TArray<FString>{TEXT("drop_unused")}, false);
	TestFalse(TEXT("Auto-fix also fails closed on incomplete data"), AutoFix.bSuccess);
	TestTrue(TEXT("Auto-fix reports the incomplete dependency"),
		AutoFix.Log.ContainsByPredicate([](const FString& Line)
		{
			return Line.Contains(TEXT("dependency data is incomplete"));
		}));
	TestEqual(TEXT("Auto-fix preserves the live expression collection"),
		Material->GetExpressions().Num(), InitialCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBridgeMaterialGraphCompositeExpansionTest,
	"UnrealBridge.Material.GraphSafety.CompositeGatewaysAndMembershipAreExplicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBridgeMaterialGraphCompositeExpansionTest::RunTest(const FString& Parameters)
{
	using namespace BridgeMaterialGraphSafetyTests;
	TStrongObjectPtr<UPackage> Package = BridgeTextureRefreshTests::MakePackage(TEXT("GraphComposite"));
	TStrongObjectPtr<UMaterial> Material = MakeMaterial(*Package, TEXT("M_GraphComposite"));
	UMaterialExpressionComposite* Composite = AddExpression<UMaterialExpressionComposite>(*Material, 0, 0);
	if (!Composite)
	{
		AddError(TEXT("Could not create composite expression"));
		return false;
	}

	UMaterialExpressionPinBase* InputBase =
		AddInternalExpression<UMaterialExpressionPinBase>(*Material, *Composite);
	UMaterialExpressionPinBase* OutputBase =
		AddInternalExpression<UMaterialExpressionPinBase>(*Material, *Composite);
	UMaterialExpressionReroute* InputReroute =
		AddInternalExpression<UMaterialExpressionReroute>(*Material, *Composite);
	UMaterialExpressionReroute* OutputReroute =
		AddInternalExpression<UMaterialExpressionReroute>(*Material, *Composite);
	UMaterialExpressionConstant* Member =
		AddInternalExpression<UMaterialExpressionConstant>(*Material, *Composite);
	InputBase->PinDirection = EGPD_Output;
	OutputBase->PinDirection = EGPD_Input;
	InputBase->ReroutePins.Add(FCompositeReroute(FName(TEXT("In")), InputReroute));
	OutputBase->ReroutePins.Add(FCompositeReroute(FName(TEXT("Out")), OutputReroute));
	Composite->InputExpressions = InputBase;
	Composite->OutputExpressions = OutputBase;

	const FBridgeMaterialGraph Graph =
		UUnrealBridgeMaterialLibrary::GetMaterialGraph(Material->GetPathName());
	TestTrue(TEXT("Fully described composite graph is complete"), Graph.bGraphComplete);
	TestEqual(TEXT("No composite remains unexpanded"), Graph.UnexpandedComposites.Num(), 0);
	const FBridgeMaterialGraphNode* MemberNode = Graph.Nodes.FindByPredicate(
		[&](const FBridgeMaterialGraphNode& Node)
		{
			return Node.Guid == Member->MaterialExpressionGuid;
		});
	TestNotNull(TEXT("Composite member is listed"), MemberNode);
	if (MemberNode)
	{
		TestEqual(TEXT("Composite parent guid is explicit"),
			MemberNode->ParentSubgraphGuid, Composite->MaterialExpressionGuid);
	}
	TestTrue(TEXT("Composite input mapping is explicit"), Graph.Connections.ContainsByPredicate(
		[](const FBridgeMaterialGraphConnection& Connection)
		{
			return Connection.EdgeKind == TEXT("composite_input");
		}));
	TestTrue(TEXT("Composite output mapping is explicit"), Graph.Connections.ContainsByPredicate(
		[](const FBridgeMaterialGraphConnection& Connection)
		{
			return Connection.EdgeKind == TEXT("composite_output");
		}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

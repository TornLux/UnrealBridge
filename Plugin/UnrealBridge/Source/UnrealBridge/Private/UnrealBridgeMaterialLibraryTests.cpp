#include "Misc/EngineVersionComparison.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "UnrealBridgeMaterialLibrary.h"
#include "UnrealBridgeTextureRefreshOperations.h"

#include "Engine/Texture2D.h"
#include "Materials/Material.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

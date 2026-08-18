#include "Misc/EngineVersionComparison.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UnrealBridgeMaterialLibrary.h"

#include "Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialLayersFunctions.h"
#include "UObject/Package.h"

namespace UnrealBridgeMaterialInstanceLayerStackTests
{
	// 聚合一个根材质、图层函数与实例，确保每条行为断言共享同一瞬态所有权边界。
	// Group one root material, its layer functions, and an instance so every behavior assertion shares one transient ownership boundary.
	struct FTransientLayerFixture
	{
		UMaterialFunctionMaterialLayer* RootBackground = nullptr;
		UMaterialFunctionMaterialLayer* RootLayer = nullptr;
		UMaterialFunctionMaterialLayer* LocalLayer = nullptr;
		UMaterialFunctionMaterialLayerInstance* LocalLayerInstance = nullptr;
		UMaterialFunctionMaterialLayerBlend* RootBlend = nullptr;
		UMaterialFunctionMaterialLayerBlend* LocalBlend = nullptr;
		UMaterialFunctionMaterialLayerBlendInstance* LocalBlendInstance = nullptr;
		UMaterial* RootMaterial = nullptr;
		UMaterialInstanceConstant* Instance = nullptr;
		UMaterialInstanceConstant* CopyDestination = nullptr;
	};

	// 所有夹具仅存在于 TransientPackage，避免创建、保存或污染内容资产。
	// Every fixture lives only in the TransientPackage so no content asset is created, saved, or polluted.
	template <typename TObjectType>
	TObjectType* NewTransientObject(const TCHAR* BaseName, UObject* Outer = GetTransientPackage())
	{
		return NewObject<TObjectType>(
			Outer,
			MakeUniqueObjectName(Outer, TObjectType::StaticClass(), FName(BaseName)),
			RF_Transactional);
	}

	UMaterial* CreateLayeredRoot(
		UMaterialFunctionMaterialLayer* Background,
		UMaterialFunctionMaterialLayer* Layer,
		UMaterialFunctionMaterialLayerBlend* Blend)
	{
		UMaterial* Material = NewTransientObject<UMaterial>(TEXT("UB_LayerStack_Root"));
		UMaterialExpressionMaterialAttributeLayers* Expression =
			NewTransientObject<UMaterialExpressionMaterialAttributeLayers>(TEXT("UB_LayerStack_Expression"), Material);
		Expression->Material = Material;
		Expression->DefaultLayers.Empty();
		Expression->DefaultLayers.AddDefaultBackgroundLayer();
		Expression->DefaultLayers.Layers[0] = Background;
		const int32 LayerIndex = Expression->DefaultLayers.AppendBlendedLayer();
		Expression->DefaultLayers.Layers[LayerIndex] = Layer;
		Expression->DefaultLayers.Blends[LayerIndex - 1] = Blend;
		Expression->DefaultLayers.EditorOnly.RestrictToLayerRelatives[0] = true;
		Expression->DefaultLayers.EditorOnly.RestrictToLayerRelatives[LayerIndex] = true;
		Expression->DefaultLayers.EditorOnly.RestrictToBlendRelatives[LayerIndex - 1] = true;
		Material->GetExpressionCollection().AddExpression(Expression);
		Material->UpdateCachedExpressionData();
		return Material;
	}

	FTransientLayerFixture CreateFixture()
	{
		FTransientLayerFixture Fixture;
		Fixture.RootBackground = NewTransientObject<UMaterialFunctionMaterialLayer>(TEXT("UB_RootBackground"));
		Fixture.RootLayer = NewTransientObject<UMaterialFunctionMaterialLayer>(TEXT("UB_RootLayer"));
		Fixture.LocalLayer = NewTransientObject<UMaterialFunctionMaterialLayer>(TEXT("UB_LocalLayer"));
		Fixture.LocalLayerInstance = NewTransientObject<UMaterialFunctionMaterialLayerInstance>(TEXT("UB_LocalLayerInstance"));
		Fixture.LocalLayerInstance->SetParent(Fixture.LocalLayer);
		Fixture.RootBlend = NewTransientObject<UMaterialFunctionMaterialLayerBlend>(TEXT("UB_RootBlend"));
		Fixture.LocalBlend = NewTransientObject<UMaterialFunctionMaterialLayerBlend>(TEXT("UB_LocalBlend"));
		Fixture.LocalBlendInstance = NewTransientObject<UMaterialFunctionMaterialLayerBlendInstance>(TEXT("UB_LocalBlendInstance"));
		Fixture.LocalBlendInstance->SetParent(Fixture.LocalBlend);
		Fixture.RootMaterial = CreateLayeredRoot(
			Fixture.RootBackground, Fixture.RootLayer, Fixture.RootBlend);
		Fixture.Instance = NewTransientObject<UMaterialInstanceConstant>(TEXT("UB_LayerStack_Instance"));
		Fixture.Instance->SetParentEditorOnly(Fixture.RootMaterial, false);
		Fixture.CopyDestination = NewTransientObject<UMaterialInstanceConstant>(TEXT("UB_LayerStack_CopyDestination"));
		Fixture.CopyDestination->SetParentEditorOnly(Fixture.RootMaterial, false);
		return Fixture;
	}

	void CleanupFixture(FTransientLayerFixture& Fixture)
	{
		for (UObject* Object : {
			static_cast<UObject*>(Fixture.CopyDestination),
			static_cast<UObject*>(Fixture.Instance),
			static_cast<UObject*>(Fixture.RootMaterial),
			static_cast<UObject*>(Fixture.RootBackground),
			static_cast<UObject*>(Fixture.RootLayer),
			static_cast<UObject*>(Fixture.LocalLayerInstance),
			static_cast<UObject*>(Fixture.LocalLayer),
			static_cast<UObject*>(Fixture.RootBlend),
			static_cast<UObject*>(Fixture.LocalBlendInstance),
			static_cast<UObject*>(Fixture.LocalBlend)})
		{
			if (Object)
			{
				Object->ClearFlags(RF_Public | RF_Standalone | RF_Transactional);
				Object->MarkAsGarbage();
			}
		}
	}

	bool EntriesEqual(const FBridgeMaterialLayerEntry& A, const FBridgeMaterialLayerEntry& B)
	{
		return A.Index == B.Index
			&& A.Name == B.Name
			&& A.Guid == B.Guid
			&& A.LinkState == B.LinkState
			&& A.bEnabled == B.bEnabled
			&& A.bRestrictToLayerRelatives == B.bRestrictToLayerRelatives
			&& A.bRestrictToBlendRelatives == B.bRestrictToBlendRelatives
			&& A.LayerAssetPath == B.LayerAssetPath
			&& A.BlendAssetPath == B.BlendAssetPath;
	}

	bool SnapshotsEqual(const FBridgeMaterialLayerStack& A, const FBridgeMaterialLayerStack& B)
	{
		if (A.bFound != B.bFound || A.bHasLayers != B.bHasLayers
			|| A.Error != B.Error || A.Layers.Num() != B.Layers.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Layers.Num(); ++Index)
		{
			if (!EntriesEqual(A.Layers[Index], B.Layers[Index]))
			{
				return false;
			}
		}
		return true;
	}
}

// 通过瞬态材质覆盖限制标志、函数实例、复制、空删除持久性、无操作脏标记与 Undo/Redo，不依赖内容资产。
// Exercise restrictions, function instances, copy, empty-deletion persistence, no-op dirtying, and Undo/Redo using transient materials.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeMaterialInstanceLayerStackTransientTest,
	"UnrealBridge.Material.InstanceLayerStack.Transient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeMaterialInstanceLayerStackTransientTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeMaterialInstanceLayerStackTests;
	(void)Parameters;

	UPackage* TransientPackage = GetTransientPackage();
	const bool bTransientPackageWasDirty = TransientPackage->IsDirty();
	ON_SCOPE_EXIT
	{
		TransientPackage->SetDirtyFlag(bTransientPackageWasDirty);
	};

	FTransientLayerFixture Fixture = CreateFixture();
	ON_SCOPE_EXIT
	{
		CleanupFixture(Fixture);
	};

	const FString InstancePath = Fixture.Instance->GetPathName();
	const FBridgeMaterialLayerStack Initial =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
	if (!TestTrue(TEXT("Inherited layered instance snapshot succeeds"),
		Initial.bFound && Initial.bHasLayers && Initial.Error.IsEmpty()))
	{
		return false;
	}
	if (!TestEqual(TEXT("Root fixture exposes background plus one layer"), Initial.Layers.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Inherited background is linked"), Initial.Layers[0].LinkState, FString(TEXT("LinkedToParent")));
	TestEqual(TEXT("Inherited layer is linked"), Initial.Layers[1].LinkState, FString(TEXT("LinkedToParent")));
	TestTrue(TEXT("Background layer restriction is reflected"), Initial.Layers[0].bRestrictToLayerRelatives);
	TestFalse(TEXT("Background has no reflected blend restriction"), Initial.Layers[0].bRestrictToBlendRelatives);
	TestTrue(TEXT("Parent layer restriction is reflected"), Initial.Layers[1].bRestrictToLayerRelatives);
	TestTrue(TEXT("Parent blend restriction is reflected at layer index minus one"), Initial.Layers[1].bRestrictToBlendRelatives);

	TArray<FBridgeMaterialLayerEntry> InvalidLinked = Initial.Layers;
	InvalidLinked[1].bRestrictToLayerRelatives = !InvalidLinked[1].bRestrictToLayerRelatives;
	const FBridgeMaterialLayerStackOpResult InvalidLinkedResult =
		UUnrealBridgeMaterialLibrary::SetMaterialInstanceLayerStack(InstancePath, InvalidLinked);
	TestFalse(TEXT("Linked parent slot rejects a local restriction change"), InvalidLinkedResult.bSuccess);
	TestTrue(TEXT("Linked restriction rejection explains the required unlink"),
		InvalidLinkedResult.Error.Contains(TEXT("UnlinkedFromParent")));
	const FBridgeMaterialLayerStack AfterInvalidLinked =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
	TestTrue(TEXT("Rejected linked restriction change performs no mutation"),
		SnapshotsEqual(Initial, AfterInvalidLinked));

	TArray<FBridgeMaterialLayerEntry> Replacement = Initial.Layers;
	Replacement[1].Name = TEXT("Overridden duplicate A");
	Replacement[1].LinkState = TEXT("UnlinkedFromParent");
	Replacement[1].bEnabled = false;
	Replacement[1].LayerAssetPath = Fixture.LocalLayer->GetPathName();
	Replacement[1].BlendAssetPath = Fixture.LocalBlend->GetPathName();

	FBridgeMaterialLayerEntry FunctionInstanceEntry;
	FunctionInstanceEntry.Index = 2;
	FunctionInstanceEntry.Name = TEXT("Local function instances");
	FunctionInstanceEntry.Guid = FGuid::NewGuid();
	FunctionInstanceEntry.LinkState = TEXT("NotFromParent");
	FunctionInstanceEntry.bEnabled = true;
	FunctionInstanceEntry.bRestrictToLayerRelatives = true;
	FunctionInstanceEntry.bRestrictToBlendRelatives = true;
	FunctionInstanceEntry.LayerAssetPath = Fixture.LocalLayerInstance->GetPathName();
	FunctionInstanceEntry.BlendAssetPath = Fixture.LocalBlendInstance->GetPathName();
	Replacement.Add(FunctionInstanceEntry);

	FBridgeMaterialLayerEntry DuplicatePointerEntry;
	DuplicatePointerEntry.Index = 3;
	DuplicatePointerEntry.Name = TEXT("Duplicate function pointers");
	DuplicatePointerEntry.Guid = FGuid::NewGuid();
	DuplicatePointerEntry.LinkState = TEXT("NotFromParent");
	DuplicatePointerEntry.bEnabled = true;
	DuplicatePointerEntry.LayerAssetPath = Fixture.LocalLayer->GetPathName();
	DuplicatePointerEntry.BlendAssetPath = Fixture.LocalBlend->GetPathName();
	Replacement.Add(DuplicatePointerEntry);

	FBridgeMaterialLayerEntry NullFunctionEntry;
	NullFunctionEntry.Index = 4;
	NullFunctionEntry.Name = TEXT("Nullable function paths");
	NullFunctionEntry.Guid = FGuid::NewGuid();
	NullFunctionEntry.LinkState = TEXT("NotFromParent");
	NullFunctionEntry.bEnabled = true;
	Replacement.Add(NullFunctionEntry);

	const FBridgeMaterialLayerStackOpResult SetResult =
		UUnrealBridgeMaterialLibrary::SetMaterialInstanceLayerStack(InstancePath, Replacement);
	TestTrue(TEXT("Validated full replacement succeeds"), SetResult.bSuccess);
	TestTrue(TEXT("Validated full replacement reports mutation"), SetResult.bChanged);
	TestEqual(TEXT("Replacement reports all applied slots"), SetResult.LayersApplied, Replacement.Num());
	TestTrue(TEXT("Replacement failure text stays empty"), SetResult.Error.IsEmpty());

	const FBridgeMaterialLayerStack RoundTrip =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
	TestEqual(TEXT("Round-trip preserves slot count"), RoundTrip.Layers.Num(), Replacement.Num());
	for (int32 Index = 0; Index < Replacement.Num() && RoundTrip.Layers.IsValidIndex(Index); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Round-trip preserves exposed slot %d"), Index),
			EntriesEqual(RoundTrip.Layers[Index], Replacement[Index]));
	}
	TestTrue(TEXT("Base and instanced layer functions both remain valid slots"),
		RoundTrip.Layers[1].LayerAssetPath != RoundTrip.Layers[2].LayerAssetPath);
	TestTrue(TEXT("Distinct slots retain distinct GUIDs"),
		RoundTrip.Layers[1].Guid != RoundTrip.Layers[2].Guid);
	TestEqual(TEXT("Material layer function instance round-trips"),
		RoundTrip.Layers[2].LayerAssetPath, Fixture.LocalLayerInstance->GetPathName());
	TestEqual(TEXT("Material layer blend function instance round-trips"),
		RoundTrip.Layers[2].BlendAssetPath, Fixture.LocalBlendInstance->GetPathName());
	TestTrue(TEXT("Layer restriction round-trips"), RoundTrip.Layers[2].bRestrictToLayerRelatives);
	TestTrue(TEXT("Blend restriction round-trips with its preceding blend"), RoundTrip.Layers[2].bRestrictToBlendRelatives);
	TestEqual(TEXT("Duplicate layer function pointers remain valid"),
		RoundTrip.Layers[3].LayerAssetPath, RoundTrip.Layers[1].LayerAssetPath);
	TestEqual(TEXT("Duplicate blend function pointers remain valid"),
		RoundTrip.Layers[3].BlendAssetPath, RoundTrip.Layers[1].BlendAssetPath);
	TestTrue(TEXT("Duplicate function pointers retain distinct slot GUIDs"),
		RoundTrip.Layers[3].Guid != RoundTrip.Layers[1].Guid);
	TestTrue(TEXT("Nullable layer function path round-trips empty"),
		RoundTrip.Layers[4].LayerAssetPath.IsEmpty());
	TestTrue(TEXT("Nullable blend function path round-trips empty"),
		RoundTrip.Layers[4].BlendAssetPath.IsEmpty());
	TestEqual(TEXT("Unlinked parent state is preserved"),
		RoundTrip.Layers[1].LinkState, FString(TEXT("UnlinkedFromParent")));
	TestEqual(TEXT("Local link state is preserved"),
		RoundTrip.Layers[2].LinkState, FString(TEXT("NotFromParent")));

	TArray<FBridgeMaterialLayerEntry> Invalid = Replacement;
	Invalid[2].Guid = Invalid[1].Guid;
	const FBridgeMaterialLayerStack BeforeInvalid =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
	const FBridgeMaterialLayerStackOpResult InvalidResult =
		UUnrealBridgeMaterialLibrary::SetMaterialInstanceLayerStack(InstancePath, Invalid);
	TestFalse(TEXT("Duplicate GUID replacement is rejected"), InvalidResult.bSuccess);
	TestTrue(TEXT("Rejected replacement returns a diagnostic"), !InvalidResult.Error.IsEmpty());
	const FBridgeMaterialLayerStack AfterInvalid =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
	TestTrue(TEXT("Rejected replacement performs no mutation"),
		SnapshotsEqual(BeforeInvalid, AfterInvalid));

	TransientPackage->SetDirtyFlag(false);
	const FBridgeMaterialLayerStackOpResult NoOpResult =
		UUnrealBridgeMaterialLibrary::SetMaterialInstanceLayerStack(InstancePath, RoundTrip.Layers);
	TestTrue(TEXT("Identical replacement succeeds"), NoOpResult.bSuccess);
	TestFalse(TEXT("Identical replacement reports no mutation"), NoOpResult.bChanged);
	TestFalse(TEXT("Identical replacement does not dirty the package"), TransientPackage->IsDirty());

	if (GEditor)
	{
		TestTrue(TEXT("One Undo reverses the complete replacement transaction"),
			GEditor->UndoTransaction());
		const FBridgeMaterialLayerStack AfterUndo =
			UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
		TestTrue(TEXT("Undo restores the inherited stack exactly"),
			SnapshotsEqual(Initial, AfterUndo));
		TestTrue(TEXT("One Redo reapplies the complete replacement transaction"),
			GEditor->RedoTransaction());
		const FBridgeMaterialLayerStack AfterRedo =
			UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
		TestTrue(TEXT("Redo restores the replacement exactly"),
			SnapshotsEqual(RoundTrip, AfterRedo));
	}
	else
	{
		AddError(TEXT("GEditor is required for material-layer-stack Undo coverage"));
	}

	const FString CopyDestinationPath = Fixture.CopyDestination->GetPathName();
	const FBridgeMaterialLayerStackOpResult SameRootCopyResult =
		UUnrealBridgeMaterialLibrary::CopyMaterialInstanceLayerStack(InstancePath, CopyDestinationPath);
	TestTrue(TEXT("Copy succeeds for instances with the same ultimate root"), SameRootCopyResult.bSuccess);
	TestTrue(TEXT("Same-root copy reports destination mutation"), SameRootCopyResult.bChanged);
	const FBridgeMaterialLayerStack Copied =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(CopyDestinationPath);
	TestTrue(TEXT("Same-root copy preserves the complete reflected snapshot"),
		SnapshotsEqual(RoundTrip, Copied));

	const FBridgeMaterialLayerStackOpResult EmptyResult =
		UUnrealBridgeMaterialLibrary::SetMaterialInstanceLayerStack(InstancePath, {});
	TestTrue(TEXT("Empty full replacement succeeds"), EmptyResult.bSuccess);
	TestTrue(TEXT("Empty full replacement reports mutation"), EmptyResult.bChanged);
	TestEqual(TEXT("Empty full replacement reports zero slots"), EmptyResult.LayersApplied, 0);
	const FBridgeMaterialLayerStack EmptySnapshot =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(InstancePath);
	TestTrue(TEXT("Empty replacement remains a valid MIC snapshot"), EmptySnapshot.bFound);
	TestFalse(TEXT("Empty replacement is represented as no stack"), EmptySnapshot.bHasLayers);
	TestTrue(TEXT("Empty replacement snapshot has no error"), EmptySnapshot.Error.IsEmpty());

	FMaterialLayersFunctions StoredEmpty;
	const FStaticParameterSet StoredStaticParameters =
		static_cast<const UMaterialInstanceConstant*>(Fixture.Instance)->GetStaticParameters();
	TestTrue(TEXT("Empty replacement remains an explicit local static override"),
		StoredStaticParameters.bHasMaterialLayers);
	StoredEmpty.GetRuntime() = StoredStaticParameters.MaterialLayers;
	StoredEmpty.EditorOnly = StoredStaticParameters.EditorOnly.MaterialLayers;
	FMaterialLayersFunctions ParentForResolve;
	TestTrue(TEXT("Layered parent is available for explicit re-resolution"),
		Fixture.RootMaterial->GetMaterialLayers(ParentForResolve));
	const FGuid OmittedParentGuid = ParentForResolve.EditorOnly.LayerGuids[1];
	TestTrue(TEXT("Empty replacement records every omitted non-background parent GUID"),
		StoredEmpty.EditorOnly.DeletedParentLayerGuids.Contains(OmittedParentGuid));
	TArray<int32> RemapLayerIndices;
	StoredEmpty.ResolveParent(ParentForResolve, RemapLayerIndices);
	TestEqual(TEXT("Parent re-resolution does not merge an explicitly deleted layer"),
		StoredEmpty.Layers.Num(), 0);
	TestTrue(TEXT("Parent re-resolution preserves the deleted-parent GUID"),
		StoredEmpty.EditorOnly.DeletedParentLayerGuids.Contains(OmittedParentGuid));

	TransientPackage->SetDirtyFlag(false);
	const FBridgeMaterialLayerStackOpResult EmptyNoOpResult =
		UUnrealBridgeMaterialLibrary::SetMaterialInstanceLayerStack(InstancePath, {});
	TestTrue(TEXT("Identical empty replacement succeeds"), EmptyNoOpResult.bSuccess);
	TestFalse(TEXT("Identical empty replacement reports no mutation"), EmptyNoOpResult.bChanged);
	TestFalse(TEXT("Identical empty replacement does not dirty the package"), TransientPackage->IsDirty());

	FTransientLayerFixture IncompatibleFixture = CreateFixture();
	ON_SCOPE_EXIT
	{
		CleanupFixture(IncompatibleFixture);
	};
	const FString IncompatiblePath = IncompatibleFixture.Instance->GetPathName();
	const FBridgeMaterialLayerStack IncompatibleBefore =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(IncompatiblePath);
	const FBridgeMaterialLayerStackOpResult CopyResult =
		UUnrealBridgeMaterialLibrary::CopyMaterialInstanceLayerStack(InstancePath, IncompatiblePath);
	TestFalse(TEXT("Copy rejects a different ultimate root material"), CopyResult.bSuccess);
	TestTrue(TEXT("Incompatible copy returns a diagnostic"), !CopyResult.Error.IsEmpty());
	const FBridgeMaterialLayerStack IncompatibleAfter =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(IncompatiblePath);
	TestTrue(TEXT("Incompatible copy performs no destination mutation"),
		SnapshotsEqual(IncompatibleBefore, IncompatibleAfter));

	UMaterial* PlainRoot = NewTransientObject<UMaterial>(TEXT("UB_PlainRoot"));
	UMaterialInstanceConstant* PlainInstance =
		NewTransientObject<UMaterialInstanceConstant>(TEXT("UB_PlainInstance"));
	PlainInstance->SetParentEditorOnly(PlainRoot, false);
	const FBridgeMaterialLayerStack NoStack =
		UUnrealBridgeMaterialLibrary::GetMaterialInstanceLayerStack(PlainInstance->GetPathName());
	TestTrue(TEXT("No-stack snapshot still finds the MIC"), NoStack.bFound);
	TestFalse(TEXT("No-stack snapshot distinguishes absence of layers"), NoStack.bHasLayers);
	TestTrue(TEXT("No-stack snapshot has no error"), NoStack.Error.IsEmpty());
	PlainInstance->ClearFlags(RF_Transactional);
	PlainInstance->MarkAsGarbage();
	PlainRoot->ClearFlags(RF_Transactional);
	PlainRoot->MarkAsGarbage();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_VERSION_OLDER_THAN(5, 7, 0)

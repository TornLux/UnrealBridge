#include "UnrealBridgeMaterialLibrary.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture.h"
#include "Engine/SubsurfaceProfile.h"
#include "VT/RuntimeVirtualTexture.h"
#include "SceneTypes.h"
#include "MaterialShared.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"

namespace BridgeMaterialImpl
{
	static FString DomainToString(EMaterialDomain Domain)
	{
		switch (Domain)
		{
			case MD_Surface:                return TEXT("Surface");
			case MD_DeferredDecal:          return TEXT("DeferredDecal");
			case MD_LightFunction:          return TEXT("LightFunction");
			case MD_Volume:                 return TEXT("Volume");
			case MD_PostProcess:            return TEXT("PostProcess");
			case MD_UI:                     return TEXT("UI");
			case MD_RuntimeVirtualTexture:  return TEXT("RuntimeVirtualTexture");
			default:                        return FString::Printf(TEXT("Unknown(%d)"), (int32)Domain);
		}
	}

	static FString BlendModeToString(EBlendMode Mode)
	{
		switch (Mode)
		{
			case BLEND_Opaque:         return TEXT("Opaque");
			case BLEND_Masked:         return TEXT("Masked");
			case BLEND_Translucent:    return TEXT("Translucent");
			case BLEND_Additive:       return TEXT("Additive");
			case BLEND_Modulate:       return TEXT("Modulate");
			case BLEND_AlphaComposite: return TEXT("AlphaComposite");
			case BLEND_AlphaHoldout:   return TEXT("AlphaHoldout");
			default:                   return FString::Printf(TEXT("Unknown(%d)"), (int32)Mode);
		}
	}

	static FString ShadingModelToString(EMaterialShadingModel Model)
	{
		switch (Model)
		{
			case MSM_Unlit:                return TEXT("Unlit");
			case MSM_DefaultLit:           return TEXT("DefaultLit");
			case MSM_Subsurface:           return TEXT("Subsurface");
			case MSM_PreintegratedSkin:    return TEXT("PreintegratedSkin");
			case MSM_ClearCoat:            return TEXT("ClearCoat");
			case MSM_SubsurfaceProfile:    return TEXT("SubsurfaceProfile");
			case MSM_TwoSidedFoliage:      return TEXT("TwoSidedFoliage");
			case MSM_Hair:                 return TEXT("Hair");
			case MSM_Cloth:                return TEXT("Cloth");
			case MSM_Eye:                  return TEXT("Eye");
			case MSM_SingleLayerWater:     return TEXT("SingleLayerWater");
			case MSM_ThinTranslucent:      return TEXT("ThinTranslucent");
			case MSM_Strata:               return TEXT("Strata");
			case MSM_FromMaterialExpression: return TEXT("FromMaterialExpression");
			default:                       return FString::Printf(TEXT("Unknown(%d)"), (int32)Model);
		}
	}

	static FString FunctionInputTypeToString(EFunctionInputType Type)
	{
		switch (Type)
		{
			case FunctionInput_Scalar:             return TEXT("Scalar");
			case FunctionInput_Vector2:            return TEXT("Vector2");
			case FunctionInput_Vector3:            return TEXT("Vector3");
			case FunctionInput_Vector4:            return TEXT("Vector4");
			case FunctionInput_Texture2D:          return TEXT("Texture2D");
			case FunctionInput_TextureCube:        return TEXT("TextureCube");
			case FunctionInput_Texture2DArray:     return TEXT("Texture2DArray");
			case FunctionInput_VolumeTexture:      return TEXT("VolumeTexture");
			case FunctionInput_StaticBool:         return TEXT("StaticBool");
			case FunctionInput_MaterialAttributes: return TEXT("MaterialAttributes");
			case FunctionInput_TextureExternal:    return TEXT("TextureExternal");
			default:                               return FString::Printf(TEXT("Type%d"), (int32)Type);
		}
	}

	static FString PreviewValueToString(const UMaterialExpressionFunctionInput* Input)
	{
		if (!Input)
		{
			return FString();
		}
		switch (Input->InputType)
		{
			case FunctionInput_Scalar:
				return FString::SanitizeFloat(Input->PreviewValue.X);
			case FunctionInput_Vector2:
				return FString::Printf(TEXT("(X=%.4f,Y=%.4f)"),
					Input->PreviewValue.X, Input->PreviewValue.Y);
			case FunctionInput_Vector3:
				return FString::Printf(TEXT("(X=%.4f,Y=%.4f,Z=%.4f)"),
					Input->PreviewValue.X, Input->PreviewValue.Y, Input->PreviewValue.Z);
			case FunctionInput_Vector4:
				return FString::Printf(TEXT("(X=%.4f,Y=%.4f,Z=%.4f,W=%.4f)"),
					Input->PreviewValue.X, Input->PreviewValue.Y, Input->PreviewValue.Z, Input->PreviewValue.W);
			case FunctionInput_StaticBool:
				return Input->PreviewValue.X != 0.f ? TEXT("True") : TEXT("False");
			default:
				return FString();
		}
	}

	static FString UsageFlagName(EMaterialUsage Usage)
	{
		// Mirrors UMaterial::GetUsageName (UE 5.7, Engine/Private/Materials/Material.cpp).
		// That function is not ENGINE_API so we can't call it from outside Engine; we keep this
		// table in sync with the engine version when EMaterialUsage gains new values.
		switch (Usage)
		{
			case MATUSAGE_SkeletalMesh:           return TEXT("SkeletalMesh");
			case MATUSAGE_ParticleSprites:        return TEXT("ParticleSprites");
			case MATUSAGE_BeamTrails:             return TEXT("BeamTrails");
			case MATUSAGE_MeshParticles:          return TEXT("MeshParticles");
			case MATUSAGE_NiagaraSprites:         return TEXT("NiagaraSprites");
			case MATUSAGE_NiagaraRibbons:         return TEXT("NiagaraRibbons");
			case MATUSAGE_NiagaraMeshParticles:   return TEXT("NiagaraMeshParticles");
			case MATUSAGE_StaticLighting:         return TEXT("StaticLighting");
			case MATUSAGE_MorphTargets:           return TEXT("MorphTargets");
			case MATUSAGE_SplineMesh:             return TEXT("SplineMesh");
			case MATUSAGE_InstancedStaticMeshes:  return TEXT("InstancedStaticMeshes");
			case MATUSAGE_GeometryCollections:    return TEXT("GeometryCollections");
			case MATUSAGE_Clothing:               return TEXT("Clothing");
			case MATUSAGE_GeometryCache:          return TEXT("GeometryCache");
			case MATUSAGE_Water:                  return TEXT("Water");
			case MATUSAGE_HairStrands:            return TEXT("HairStrands");
			case MATUSAGE_LidarPointCloud:        return TEXT("LidarPointCloud");
			case MATUSAGE_VirtualHeightfieldMesh: return TEXT("VirtualHeightfieldMesh");
			case MATUSAGE_Nanite:                 return TEXT("Nanite");
			case MATUSAGE_Voxels:                 return TEXT("Voxels");
			case MATUSAGE_VolumetricCloud:        return TEXT("VolumetricCloud");
			case MATUSAGE_HeterogeneousVolumes:   return TEXT("HeterogeneousVolumes");
			case MATUSAGE_StaticMesh:             return TEXT("StaticMesh");
			default:                              return FString::Printf(TEXT("Usage%d"), (int32)Usage);
		}
	}
}

FBridgeMaterialInstanceInfo UUnrealBridgeMaterialLibrary::GetMaterialInstanceParameters(
	const FString& MaterialPath)
{
	FBridgeMaterialInstanceInfo Result;

	UMaterialInstance* MI = LoadObject<UMaterialInstance>(nullptr, *MaterialPath);
	if (!MI)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load MaterialInstance '%s'"), *MaterialPath);
		return Result;
	}

	Result.Name = MI->GetName();

	if (UMaterialInterface* Parent = MI->Parent)
	{
		Result.ParentName = Parent->GetName();
		Result.ParentPath = Parent->GetPathName();
	}

	for (const FScalarParameterValue& P : MI->ScalarParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("Scalar");
		Param.Value = FString::SanitizeFloat(P.ParameterValue);
		Result.Parameters.Add(Param);
	}

	for (const FVectorParameterValue& P : MI->VectorParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("Vector");
		Param.Value = FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"),
			P.ParameterValue.R, P.ParameterValue.G, P.ParameterValue.B, P.ParameterValue.A);
		Result.Parameters.Add(Param);
	}

	for (const FDoubleVectorParameterValue& P : MI->DoubleVectorParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("DoubleVector");
		Param.Value = P.ParameterValue.ToString();
		Result.Parameters.Add(Param);
	}

	for (const FTextureParameterValue& P : MI->TextureParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("Texture");
		Param.Value = P.ParameterValue ? P.ParameterValue->GetPathName() : TEXT("None");
		Result.Parameters.Add(Param);
	}

	for (const FRuntimeVirtualTextureParameterValue& P : MI->RuntimeVirtualTextureParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("RuntimeVirtualTexture");
		Param.Value = P.ParameterValue ? P.ParameterValue->GetPathName() : TEXT("None");
		Result.Parameters.Add(Param);
	}

	return Result;
}

FBridgeMaterialInfo UUnrealBridgeMaterialLibrary::GetMaterialInfo(const FString& MaterialPath)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialInfo Info;

	UMaterialInterface* MatInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!MatInterface)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetMaterialInfo could not load '%s'"), *MaterialPath);
		return Info;
	}

	Info.bFound = true;
	Info.Name = MatInterface->GetName();
	Info.Path = MatInterface->GetPathName();

	UMaterialInstance* MI = Cast<UMaterialInstance>(MatInterface);
	Info.bIsMaterialInstance = (MI != nullptr);
	if (MI && MI->Parent)
	{
		Info.ParentPath = MI->Parent->GetPathName();
	}

	UMaterial* BaseMat = MatInterface->GetMaterial();
	if (!BaseMat)
	{
		return Info;
	}
	Info.BasePath = BaseMat->GetPathName();

	Info.MaterialDomain = DomainToString(BaseMat->MaterialDomain);
	Info.BlendMode = BlendModeToString(MatInterface->GetBlendMode());

	FMaterialShadingModelField Models = MatInterface->GetShadingModels();
	for (int32 i = 0; i < MSM_NUM; ++i)
	{
		const EMaterialShadingModel M = (EMaterialShadingModel)i;
		if (Models.HasShadingModel(M))
		{
			Info.ShadingModels.Add(ShadingModelToString(M));
		}
	}

	Info.bTwoSided = MatInterface->IsTwoSided();
	Info.bUseMaterialAttributes = BaseMat->bUseMaterialAttributes;

	for (int32 i = 0; i < MATUSAGE_MAX; ++i)
	{
		const EMaterialUsage Usage = (EMaterialUsage)i;
		if (BaseMat->GetUsageByFlag(Usage))
		{
			Info.UsageFlags.Add(UsageFlagName(Usage));
		}
	}

	if (BaseMat->SubsurfaceProfile)
	{
		Info.SubsurfaceProfilePath = BaseMat->SubsurfaceProfile->GetPathName();
	}

	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		MatInterface->GetAllScalarParameterInfo(ParamInfos, ParamGuids);
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			FBridgeMaterialParamDefault P;
			P.Name = ParamInfos[i].Name.ToString();
			P.ParamType = TEXT("Scalar");
			P.Guid = ParamGuids.IsValidIndex(i) ? ParamGuids[i] : FGuid();
			float Val = 0.f;
			MatInterface->GetScalarParameterDefaultValue(ParamInfos[i], Val);
			P.Value = FString::SanitizeFloat(Val);
			Info.ScalarParameters.Add(P);
		}
	}

	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		MatInterface->GetAllVectorParameterInfo(ParamInfos, ParamGuids);
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			FBridgeMaterialParamDefault P;
			P.Name = ParamInfos[i].Name.ToString();
			P.ParamType = TEXT("Vector");
			P.Guid = ParamGuids.IsValidIndex(i) ? ParamGuids[i] : FGuid();
			FLinearColor Val(FLinearColor::Black);
			MatInterface->GetVectorParameterDefaultValue(ParamInfos[i], Val);
			P.Value = FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"), Val.R, Val.G, Val.B, Val.A);
			Info.VectorParameters.Add(P);
		}
	}

	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		MatInterface->GetAllTextureParameterInfo(ParamInfos, ParamGuids);
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			FBridgeMaterialParamDefault P;
			P.Name = ParamInfos[i].Name.ToString();
			P.ParamType = TEXT("Texture");
			P.Guid = ParamGuids.IsValidIndex(i) ? ParamGuids[i] : FGuid();
			UTexture* Val = nullptr;
			MatInterface->GetTextureParameterDefaultValue(ParamInfos[i], Val);
			P.Value = Val ? Val->GetPathName() : TEXT("None");
			Info.TextureParameters.Add(P);
		}
	}

	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		MatInterface->GetAllStaticSwitchParameterInfo(ParamInfos, ParamGuids);
		for (int32 i = 0; i < ParamInfos.Num(); ++i)
		{
			FBridgeMaterialParamDefault P;
			P.Name = ParamInfos[i].Name.ToString();
			P.ParamType = TEXT("StaticSwitch");
			P.Guid = ParamGuids.IsValidIndex(i) ? ParamGuids[i] : FGuid();
			bool Val = false;
			FGuid ExprGuid;
			MatInterface->GetStaticSwitchParameterDefaultValue(ParamInfos[i], Val, ExprGuid);
			P.Value = Val ? TEXT("True") : TEXT("False");
			Info.StaticSwitchParameters.Add(P);
		}
	}

	const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = BaseMat->GetExpressions();
	Info.NumExpressions = Expressions.Num();
	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (Expr && Expr->IsA<UMaterialExpressionMaterialFunctionCall>())
		{
			++Info.NumFunctionCalls;
		}
	}

	return Info;
}

TArray<FBridgeMaterialFunctionSummary> UUnrealBridgeMaterialLibrary::ListMaterialFunctions(
	const FString& PathPrefix,
	int32 MaxResults)
{
	TArray<FBridgeMaterialFunctionSummary> Result;

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = false;
	if (!PathPrefix.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathPrefix));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	Result.Reserve(Assets.Num());
	for (const FAssetData& AssetData : Assets)
	{
		if (MaxResults > 0 && Result.Num() >= MaxResults)
		{
			break;
		}

		FBridgeMaterialFunctionSummary Summary;
		Summary.Name = AssetData.AssetName.ToString();
		Summary.Path = AssetData.GetObjectPathString();

		// Description / bExposeToLibrary / LibraryCategory — pull from asset tags if possible,
		// otherwise load to fetch. Asset tags save us from loading; fall back to load.
		FString TagValue;
		if (AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UMaterialFunction, Description), TagValue))
		{
			Summary.Description = TagValue;
		}
		if (AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UMaterialFunction, bExposeToLibrary), TagValue))
		{
			Summary.bExposeToLibrary = TagValue.Equals(TEXT("True"), ESearchCase::IgnoreCase) ||
				TagValue == TEXT("1");
		}

		// Only touch the asset if we still need the library category (and list is short, user opted in).
		if (Summary.bExposeToLibrary && Summary.LibraryCategory.IsEmpty())
		{
			if (UMaterialFunction* MF = Cast<UMaterialFunction>(AssetData.GetAsset()))
			{
				if (MF->LibraryCategoriesText.Num() > 0)
				{
					Summary.LibraryCategory = MF->LibraryCategoriesText[0].ToString();
				}
				if (Summary.Description.IsEmpty())
				{
					Summary.Description = MF->Description;
				}
			}
		}

		Result.Add(MoveTemp(Summary));
	}

	return Result;
}

FBridgeMaterialFunctionInfo UUnrealBridgeMaterialLibrary::GetMaterialFunction(const FString& FunctionPath)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialFunctionInfo Info;

	UMaterialFunction* MF = LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
	if (!MF)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetMaterialFunction could not load '%s'"), *FunctionPath);
		return Info;
	}

	Info.bFound = true;
	Info.Name = MF->GetName();
	Info.Path = MF->GetPathName();
	Info.Description = MF->Description;
	Info.bExposeToLibrary = MF->bExposeToLibrary;
	if (MF->LibraryCategoriesText.Num() > 0)
	{
		Info.LibraryCategory = MF->LibraryCategoriesText[0].ToString();
	}

	const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = MF->GetExpressions();
	Info.NumExpressions = Expressions.Num();

	for (const TObjectPtr<UMaterialExpression>& Expr : Expressions)
	{
		if (UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expr.Get()))
		{
			FBridgeMaterialFunctionPort Port;
			Port.Name = Input->InputName.ToString();
			Port.Description = Input->Description;
			Port.PortType = FunctionInputTypeToString(Input->InputType);
			Port.SortPriority = Input->SortPriority;
			Port.bUsePreviewValueAsDefault = Input->bUsePreviewValueAsDefault != 0;
			Port.DefaultValue = PreviewValueToString(Input);
			Info.Inputs.Add(MoveTemp(Port));
		}
		else if (UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Expr.Get()))
		{
			FBridgeMaterialFunctionPort Port;
			Port.Name = Output->OutputName.ToString();
			Port.Description = Output->Description;
			Port.PortType = TEXT(""); // Determined by upstream connection — unknown without graph walk.
			Port.SortPriority = Output->SortPriority;
			Info.Outputs.Add(MoveTemp(Port));
		}
	}

	Info.Inputs.Sort([](const FBridgeMaterialFunctionPort& A, const FBridgeMaterialFunctionPort& B)
	{
		return A.SortPriority < B.SortPriority;
	});
	Info.Outputs.Sort([](const FBridgeMaterialFunctionPort& A, const FBridgeMaterialFunctionPort& B)
	{
		return A.SortPriority < B.SortPriority;
	});

	return Info;
}

namespace BridgeMaterialImpl
{
	static void AppendMILayerOverrides(const UMaterialInstance* MI, FBridgeMaterialInstanceLayer& Layer)
	{
		for (const FScalarParameterValue& P : MI->ScalarParameterValues)
		{
			FBridgeMaterialParam Param;
			Param.Name = P.ParameterInfo.Name.ToString();
			Param.ParamType = TEXT("Scalar");
			Param.Value = FString::SanitizeFloat(P.ParameterValue);
			Layer.OverrideParameters.Add(MoveTemp(Param));
		}
		for (const FVectorParameterValue& P : MI->VectorParameterValues)
		{
			FBridgeMaterialParam Param;
			Param.Name = P.ParameterInfo.Name.ToString();
			Param.ParamType = TEXT("Vector");
			Param.Value = FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"),
				P.ParameterValue.R, P.ParameterValue.G, P.ParameterValue.B, P.ParameterValue.A);
			Layer.OverrideParameters.Add(MoveTemp(Param));
		}
		for (const FDoubleVectorParameterValue& P : MI->DoubleVectorParameterValues)
		{
			FBridgeMaterialParam Param;
			Param.Name = P.ParameterInfo.Name.ToString();
			Param.ParamType = TEXT("DoubleVector");
			Param.Value = P.ParameterValue.ToString();
			Layer.OverrideParameters.Add(MoveTemp(Param));
		}
		for (const FTextureParameterValue& P : MI->TextureParameterValues)
		{
			FBridgeMaterialParam Param;
			Param.Name = P.ParameterInfo.Name.ToString();
			Param.ParamType = TEXT("Texture");
			Param.Value = P.ParameterValue ? P.ParameterValue->GetPathName() : TEXT("None");
			Layer.OverrideParameters.Add(MoveTemp(Param));
		}
		for (const FRuntimeVirtualTextureParameterValue& P : MI->RuntimeVirtualTextureParameterValues)
		{
			FBridgeMaterialParam Param;
			Param.Name = P.ParameterInfo.Name.ToString();
			Param.ParamType = TEXT("RuntimeVirtualTexture");
			Param.Value = P.ParameterValue ? P.ParameterValue->GetPathName() : TEXT("None");
			Layer.OverrideParameters.Add(MoveTemp(Param));
		}
	}
}

FBridgeMaterialInstanceChain UUnrealBridgeMaterialLibrary::ListMaterialInstanceChain(const FString& MaterialPath)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialInstanceChain Chain;

	UMaterialInterface* Current = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Current)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: ListMaterialInstanceChain could not load '%s'"), *MaterialPath);
		return Chain;
	}

	Chain.bFound = true;
	Chain.Path = Current->GetPathName();

	// Guard against pathological circular Parent chains: cap at a generous depth.
	constexpr int32 MaxDepth = 64;
	int32 Depth = 0;
	while (Current && Depth++ < MaxDepth)
	{
		FBridgeMaterialInstanceLayer Layer;
		Layer.Name = Current->GetName();
		Layer.Path = Current->GetPathName();

		if (UMaterialInstance* MI = Cast<UMaterialInstance>(Current))
		{
			Layer.bIsBaseMaterial = false;
			AppendMILayerOverrides(MI, Layer);
			Chain.Layers.Add(MoveTemp(Layer));
			Current = MI->Parent;
		}
		else if (UMaterial* BaseMat = Cast<UMaterial>(Current))
		{
			Layer.bIsBaseMaterial = true;
			Chain.Layers.Add(MoveTemp(Layer));
			break;
		}
		else
		{
			// UMaterialInterface subclass we don't recognize — record and stop.
			Chain.Layers.Add(MoveTemp(Layer));
			break;
		}
	}

	return Chain;
}

FBridgeMaterialParameterCollectionInfo UUnrealBridgeMaterialLibrary::GetMaterialParameterCollection(const FString& CollectionPath)
{
	FBridgeMaterialParameterCollectionInfo Info;

	UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, *CollectionPath);
	if (!MPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetMaterialParameterCollection could not load '%s'"), *CollectionPath);
		return Info;
	}

	Info.bFound = true;
	Info.Name = MPC->GetName();
	Info.Path = MPC->GetPathName();

	for (const FCollectionScalarParameter& P : MPC->ScalarParameters)
	{
		FBridgeMPCScalarParam Out;
		Out.Name = P.ParameterName.ToString();
		Out.DefaultValue = P.DefaultValue;
		Out.Id = P.Id;
		Info.ScalarParameters.Add(MoveTemp(Out));
	}

	for (const FCollectionVectorParameter& P : MPC->VectorParameters)
	{
		FBridgeMPCVectorParam Out;
		Out.Name = P.ParameterName.ToString();
		Out.DefaultValue = P.DefaultValue;
		Out.Id = P.Id;
		Info.VectorParameters.Add(MoveTemp(Out));
	}

	return Info;
}

#include "UnrealBridgeMaterialLibrary.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "ScopedTransaction.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionReroute.h"
#include "MaterialExpressionIO.h"
#include "MaterialShared.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "RHIDefinitions.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MaterialStatsCommon.h"
#include "PreviewScene.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialLayersFunctions.h"
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

namespace BridgeMaterialImpl
{
	static FString StripClassPrefix(const FString& In)
	{
		// UMaterialExpressionConstant3Vector -> Constant3Vector
		FString Out = In;
		Out.RemoveFromStart(TEXT("UMaterialExpression"));
		Out.RemoveFromStart(TEXT("MaterialExpression"));
		return Out;
	}

	static FString JoinKV(const TArray<TPair<FString, FString>>& Pairs)
	{
		FString Out;
		for (const TPair<FString, FString>& KV : Pairs)
		{
			if (!Out.IsEmpty())
			{
				Out.Append(TEXT("; "));
			}
			Out.Appendf(TEXT("%s=%s"), *KV.Key, *KV.Value);
		}
		return Out;
	}

	static FString TruncateForSummary(const FString& In, int32 MaxLen)
	{
		if (In.Len() <= MaxLen)
		{
			return In;
		}
		return In.Left(MaxLen) + TEXT("...");
	}

	static FString DescribeExpressionKeyProps(const UMaterialExpression* Expr)
	{
		TArray<TPair<FString, FString>> KV;

		if (const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			KV.Emplace(TEXT("ParameterName"), Scalar->ParameterName.ToString());
			KV.Emplace(TEXT("DefaultValue"), FString::SanitizeFloat(Scalar->DefaultValue));
			KV.Emplace(TEXT("Group"), Scalar->Group.ToString());
			KV.Emplace(TEXT("SortPriority"), FString::FromInt(Scalar->SortPriority));
		}
		else if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			KV.Emplace(TEXT("ParameterName"), Vector->ParameterName.ToString());
			KV.Emplace(TEXT("DefaultValue"), FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"),
				Vector->DefaultValue.R, Vector->DefaultValue.G, Vector->DefaultValue.B, Vector->DefaultValue.A));
			KV.Emplace(TEXT("Group"), Vector->Group.ToString());
		}
		else if (const UMaterialExpressionStaticSwitchParameter* SSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
		{
			KV.Emplace(TEXT("ParameterName"), SSwitch->ParameterName.ToString());
			KV.Emplace(TEXT("DefaultValue"), SSwitch->DefaultValue ? TEXT("True") : TEXT("False"));
		}
		else if (const UMaterialExpressionStaticBoolParameter* SBool = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
		{
			KV.Emplace(TEXT("ParameterName"), SBool->ParameterName.ToString());
			KV.Emplace(TEXT("DefaultValue"), SBool->DefaultValue ? TEXT("True") : TEXT("False"));
		}
		else if (const UMaterialExpressionConstant* C1 = Cast<UMaterialExpressionConstant>(Expr))
		{
			KV.Emplace(TEXT("R"), FString::SanitizeFloat(C1->R));
		}
		else if (const UMaterialExpressionConstant2Vector* C2 = Cast<UMaterialExpressionConstant2Vector>(Expr))
		{
			KV.Emplace(TEXT("R"), FString::SanitizeFloat(C2->R));
			KV.Emplace(TEXT("G"), FString::SanitizeFloat(C2->G));
		}
		else if (const UMaterialExpressionConstant3Vector* C3 = Cast<UMaterialExpressionConstant3Vector>(Expr))
		{
			KV.Emplace(TEXT("Constant"), FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f)"),
				C3->Constant.R, C3->Constant.G, C3->Constant.B));
		}
		else if (const UMaterialExpressionConstant4Vector* C4 = Cast<UMaterialExpressionConstant4Vector>(Expr))
		{
			KV.Emplace(TEXT("Constant"), FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"),
				C4->Constant.R, C4->Constant.G, C4->Constant.B, C4->Constant.A));
		}
		else if (const UMaterialExpressionMaterialFunctionCall* MFCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
		{
			KV.Emplace(TEXT("MaterialFunction"),
				MFCall->MaterialFunction ? MFCall->MaterialFunction->GetPathName() : TEXT("None"));
		}
		else if (const UMaterialExpressionTextureSampleParameter* TSP = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
		{
			KV.Emplace(TEXT("ParameterName"), TSP->ParameterName.ToString());
			KV.Emplace(TEXT("Texture"), TSP->Texture ? TSP->Texture->GetPathName() : TEXT("None"));
			KV.Emplace(TEXT("SamplerType"), StaticEnum<EMaterialSamplerType>()->GetNameStringByValue((int64)TSP->SamplerType));
		}
		else if (const UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr))
		{
			KV.Emplace(TEXT("Texture"), TS->Texture ? TS->Texture->GetPathName() : TEXT("None"));
			KV.Emplace(TEXT("SamplerType"), StaticEnum<EMaterialSamplerType>()->GetNameStringByValue((int64)TS->SamplerType));
		}
		else if (const UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expr))
		{
			KV.Emplace(TEXT("Text"), TruncateForSummary(Comment->Text, 80));
			KV.Emplace(TEXT("Size"), FString::Printf(TEXT("%dx%d"), Comment->SizeX, Comment->SizeY));
		}
		else if (const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
		{
			KV.Emplace(TEXT("Description"), Custom->Description);
			KV.Emplace(TEXT("CodeLen"), FString::FromInt(Custom->Code.Len()));
			KV.Emplace(TEXT("OutputType"), StaticEnum<ECustomMaterialOutputType>()->GetNameStringByValue((int64)Custom->OutputType));
			KV.Emplace(TEXT("NumInputs"), FString::FromInt(Custom->Inputs.Num()));
		}
		else if (const UMaterialExpressionFunctionInput* FuncIn = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			KV.Emplace(TEXT("InputName"), FuncIn->InputName.ToString());
			KV.Emplace(TEXT("InputType"), FunctionInputTypeToString(FuncIn->InputType));
			KV.Emplace(TEXT("SortPriority"), FString::FromInt(FuncIn->SortPriority));
		}
		else if (const UMaterialExpressionFunctionOutput* FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expr))
		{
			KV.Emplace(TEXT("OutputName"), FuncOut->OutputName.ToString());
			KV.Emplace(TEXT("SortPriority"), FString::FromInt(FuncOut->SortPriority));
		}

		return JoinKV(KV);
	}

	static FString PropertyNameFromEnum(EMaterialProperty Prop)
	{
		UEnum* PropEnum = StaticEnum<EMaterialProperty>();
		if (!PropEnum)
		{
			return FString::Printf(TEXT("MP_%d"), (int32)Prop);
		}
		FString Raw = PropEnum->GetNameStringByValue((int64)Prop);
		Raw.RemoveFromStart(TEXT("MP_"));
		return Raw;
	}

	static FBridgeMaterialGraphNode BuildGraphNode(UMaterialExpression* Expr)
	{
		FBridgeMaterialGraphNode Node;
		Node.Guid = Expr->MaterialExpressionGuid;
		Node.ClassName = StripClassPrefix(Expr->GetClass()->GetName());
		Node.X = Expr->MaterialExpressionEditorX;
		Node.Y = Expr->MaterialExpressionEditorY;
		Node.Desc = Expr->Desc;

		TArray<FString> Captions;
		Expr->GetCaption(Captions);
		if (Captions.Num() > 0)
		{
			Node.Caption = Captions[0];
		}

		// Input names — emit UI-shortened forms ("UVs" not "Coordinates",
		// "A > B" not "AGreaterThanB") so what the agent reads here matches
		// both the Material Editor UI *and* what ConnectMaterialExpressions
		// expects on the write side.
		for (FExpressionInputIterator It{Expr}; It; ++It)
		{
			const FName Short = UMaterialGraphNode::GetShortenPinName(Expr->GetInputName(It.Index));
			Node.InputNames.Add(Short.IsNone() ? FString() : Short.ToString());
		}

		// Output names
		for (const FExpressionOutput& Out : Expr->GetOutputs())
		{
			Node.OutputNames.Add(Out.OutputName.ToString());
		}

		Node.KeyProperties = DescribeExpressionKeyProps(Expr);

		return Node;
	}

	static void CollectConnectionsFromExpressions(
		const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions,
		TArray<FBridgeMaterialGraphConnection>& OutConnections)
	{
		for (const TObjectPtr<UMaterialExpression>& ExprPtr : Expressions)
		{
			UMaterialExpression* Expr = ExprPtr.Get();
			if (!Expr)
			{
				continue;
			}

			for (FExpressionInputIterator It{Expr}; It; ++It)
			{
				FExpressionInput* Input = It.Input;
				if (!Input || !Input->Expression)
				{
					continue;
				}

				FBridgeMaterialGraphConnection Conn;
				Conn.SrcGuid = Input->Expression->MaterialExpressionGuid;
				Conn.SrcOutputIndex = Input->OutputIndex;
				TArray<FExpressionOutput>& SrcOutputs = Input->Expression->GetOutputs();
				if (SrcOutputs.IsValidIndex(Input->OutputIndex))
				{
					Conn.SrcOutputName = SrcOutputs[Input->OutputIndex].OutputName.ToString();
				}

				Conn.DstGuid = Expr->MaterialExpressionGuid;
				Conn.DstInputIndex = It.Index;
				{
					const FName ShortDst = UMaterialGraphNode::GetShortenPinName(Expr->GetInputName(It.Index));
					Conn.DstInputName = ShortDst.IsNone() ? FString() : ShortDst.ToString();
				}

				OutConnections.Add(MoveTemp(Conn));
			}
		}
	}
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

namespace BridgeMaterialImpl
{
	static ERHIFeatureLevel::Type ParseFeatureLevel(const FString& Str)
	{
		if (Str.IsEmpty() || Str.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
		{
			return GMaxRHIFeatureLevel;
		}
		if (Str.Equals(TEXT("SM5"), ESearchCase::IgnoreCase)) return ERHIFeatureLevel::SM5;
		if (Str.Equals(TEXT("SM6"), ESearchCase::IgnoreCase)) return ERHIFeatureLevel::SM6;
		if (Str.Equals(TEXT("ES3_1"), ESearchCase::IgnoreCase) || Str.Equals(TEXT("ES31"), ESearchCase::IgnoreCase))
		{
			return ERHIFeatureLevel::ES3_1;
		}
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown FeatureLevel '%s' — falling back to GMaxRHIFeatureLevel"), *Str);
		return GMaxRHIFeatureLevel;
	}

	static EMaterialQualityLevel::Type ParseQuality(const FString& Str)
	{
		if (Str.IsEmpty() || Str.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
		{
			return EMaterialQualityLevel::High;
		}
		if (Str.Equals(TEXT("Low"), ESearchCase::IgnoreCase))    return EMaterialQualityLevel::Low;
		if (Str.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) return EMaterialQualityLevel::Medium;
		if (Str.Equals(TEXT("High"), ESearchCase::IgnoreCase))   return EMaterialQualityLevel::High;
		if (Str.Equals(TEXT("Epic"), ESearchCase::IgnoreCase))   return EMaterialQualityLevel::Epic;
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown Quality '%s' — falling back to High"), *Str);
		return EMaterialQualityLevel::High;
	}

	static FString FeatureLevelToString(ERHIFeatureLevel::Type FL)
	{
		switch (FL)
		{
			case ERHIFeatureLevel::ES3_1: return TEXT("ES3_1");
			case ERHIFeatureLevel::SM5:   return TEXT("SM5");
			case ERHIFeatureLevel::SM6:   return TEXT("SM6");
			default:                      return FString::Printf(TEXT("FL%d"), (int32)FL);
		}
	}

	static FString QualityToString(EMaterialQualityLevel::Type Q)
	{
		switch (Q)
		{
			case EMaterialQualityLevel::Low:    return TEXT("Low");
			case EMaterialQualityLevel::Medium: return TEXT("Medium");
			case EMaterialQualityLevel::High:   return TEXT("High");
			case EMaterialQualityLevel::Epic:   return TEXT("Epic");
			default:                            return FString::Printf(TEXT("Q%d"), (int32)Q);
		}
	}

	/**
	 * Resolve an MI/Master to the FMaterialResource for the given feature level + quality.
	 * Returns nullptr if the material can't be resolved or hasn't been compiled for this combo.
	 */
	static const FMaterialResource* ResolveMaterialResource(
		UMaterialInterface* MatInterface,
		ERHIFeatureLevel::Type FeatureLevel,
		EMaterialQualityLevel::Type Quality)
	{
		if (!MatInterface)
		{
			return nullptr;
		}
		const EShaderPlatform Platform = GShaderPlatformForFeatureLevel[FeatureLevel];
		if (Platform == SP_NumPlatforms)
		{
			return nullptr;
		}
		return MatInterface->GetMaterialResource(Platform, Quality);
	}

	/** Local copy of FMaterialStatsUtils::RepresentativeShaderTypeToString — not exported from MaterialEditor. */
	static FString RepresentativeShaderTypeName(ERepresentativeShader ShaderType)
	{
		switch (ShaderType)
		{
			case ERepresentativeShader::StationarySurface:            return TEXT("Stationary surface");
			case ERepresentativeShader::StationarySurfaceCSM:         return TEXT("Stationary surface + CSM");
			case ERepresentativeShader::StationarySurfaceNPointLights:return TEXT("Stationary surface + Point Lights");
			case ERepresentativeShader::DynamicallyLitObject:         return TEXT("Dynamically lit object");
			case ERepresentativeShader::StaticMesh:                   return TEXT("Static Mesh");
			case ERepresentativeShader::SkeletalMesh:                 return TEXT("Skeletal Mesh");
			case ERepresentativeShader::SkinnedCloth:                 return TEXT("Skinned Cloth");
			case ERepresentativeShader::NaniteMesh:                   return TEXT("Nanite Mesh");
			case ERepresentativeShader::UIDefaultFragmentShader:      return TEXT("UI Pixel Shader");
			case ERepresentativeShader::UIDefaultVertexShader:        return TEXT("UI Vertex Shader");
			case ERepresentativeShader::UIInstancedVertexShader:      return TEXT("UI Instanced Vertex Shader");
			case ERepresentativeShader::RuntimeVirtualTextureOutput:  return TEXT("Runtime Virtual Texture Output");
			default:                                                  return FString::Printf(TEXT("Shader%d"), (int32)ShaderType);
		}
	}

	/**
	 * Reimplementation of FMaterialStatsUtils::GetRepresentativeInstructionCounts.
	 * The original is a non-exported static in MaterialStatsCommon.cpp, but the helpers it
	 * depends on (GetRepresentativeShaderTypesAndDescriptions / FindShaderTypeByName /
	 * FindVertexFactoryType / GetMaxNumInstructionsForShader) are all ENGINE_API or
	 * MATERIALEDITOR_API, so we can walk the shader map directly.
	 */
	static void CollectShaderInstructionStats(
		const FMaterialResource* Resource,
		TArray<FBridgeMaterialShaderStat>& OutStats)
	{
		if (!Resource)
		{
			return;
		}
		const FMaterialShaderMap* ShaderMap = Resource->GetGameThreadShaderMap();
		if (!ShaderMap)
		{
			return;
		}

		TMap<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>> ShaderTypeNamesAndDescriptions;
		FMaterialStatsUtils::GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNamesAndDescriptions, Resource);

		// We de-dup by ERepresentativeShader to avoid listing the same variant twice across factories.
		TSet<int32> EmittedShaderTypes;

		if (Resource->IsUIMaterial())
		{
			for (const TPair<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>>& Pair : ShaderTypeNamesAndDescriptions)
			{
				for (const FMaterialStatsUtils::FRepresentativeShaderInfo& ShaderInfo : Pair.Value)
				{
					const int32 Key = (int32)ShaderInfo.ShaderType;
					if (EmittedShaderTypes.Contains(Key))
					{
						continue;
					}
					FShaderType* ShaderType = FindShaderTypeByName(ShaderInfo.ShaderName);
					if (!ShaderType)
					{
						continue;
					}
					const int32 NumInstructions = ShaderMap->GetMaxNumInstructionsForShader(ShaderType);

					FBridgeMaterialShaderStat Stat;
					Stat.ShaderType = RepresentativeShaderTypeName(ShaderInfo.ShaderType);
					Stat.ShaderDescription = ShaderInfo.ShaderDescription;
					Stat.InstructionCount = NumInstructions;
					OutStats.Add(MoveTemp(Stat));
					EmittedShaderTypes.Add(Key);
				}
			}
		}
		else
		{
			for (const TPair<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>>& Pair : ShaderTypeNamesAndDescriptions)
			{
				FVertexFactoryType* FactoryType = FindVertexFactoryType(Pair.Key);
				const FMeshMaterialShaderMap* MeshShaderMap = ShaderMap->GetMeshShaderMap(FactoryType);
				if (!MeshShaderMap)
				{
					continue;
				}
				TMap<FHashedName, TShaderRef<FShader>> MeshShaderList;
				MeshShaderMap->GetShaderList(*ShaderMap, MeshShaderList);

				for (const FMaterialStatsUtils::FRepresentativeShaderInfo& ShaderInfo : Pair.Value)
				{
					const int32 Key = (int32)ShaderInfo.ShaderType;
					if (EmittedShaderTypes.Contains(Key))
					{
						continue;
					}
					TShaderRef<FShader>* Found = MeshShaderList.Find(ShaderInfo.ShaderName);
					if (!Found)
					{
						continue;
					}
					FShaderType* ShaderType = Found->GetType();
					if (!ShaderType)
					{
						continue;
					}
					const int32 NumInstructions = MeshShaderMap->GetMaxNumInstructionsForShader(*ShaderMap, ShaderType);

					FBridgeMaterialShaderStat Stat;
					Stat.ShaderType = RepresentativeShaderTypeName(ShaderInfo.ShaderType);
					Stat.ShaderDescription = ShaderInfo.ShaderDescription;
					Stat.InstructionCount = NumInstructions;
					OutStats.Add(MoveTemp(Stat));
					EmittedShaderTypes.Add(Key);
				}
			}
		}
	}
}

FBridgeMaterialGraph UUnrealBridgeMaterialLibrary::GetMaterialGraph(const FString& MaterialPath)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialGraph Graph;

	UObject* LoadedObj = LoadObject<UObject>(nullptr, *MaterialPath);
	if (!LoadedObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetMaterialGraph could not load '%s'"), *MaterialPath);
		return Graph;
	}

	Graph.bFound = true;
	Graph.Path = LoadedObj->GetPathName();

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions;

	if (UMaterial* Mat = Cast<UMaterial>(LoadedObj))
	{
		Expressions = Mat->GetExpressions();

		// Main-output wiring — walk EMaterialProperty and emit a connection for each connected slot.
		// Material can also use MaterialAttributes pin; GetExpressionInputForProperty handles that transparently.
		UEnum* PropEnum = StaticEnum<EMaterialProperty>();
		if (PropEnum)
		{
			for (int32 EnumIdx = 0; EnumIdx < PropEnum->NumEnums(); ++EnumIdx)
			{
				const int64 Val = PropEnum->GetValueByIndex(EnumIdx);
				if (Val == INDEX_NONE || Val == (int64)MP_MAX)
				{
					continue;
				}
				const EMaterialProperty Prop = (EMaterialProperty)Val;
				FExpressionInput* Input = Mat->GetExpressionInputForProperty(Prop);
				if (!Input || !Input->Expression)
				{
					continue;
				}
				FBridgeMaterialGraphConnection Conn;
				Conn.SrcGuid = Input->Expression->MaterialExpressionGuid;
				Conn.SrcOutputIndex = Input->OutputIndex;
				TArray<FExpressionOutput>& SrcOutputs = Input->Expression->GetOutputs();
				if (SrcOutputs.IsValidIndex(Input->OutputIndex))
				{
					Conn.SrcOutputName = SrcOutputs[Input->OutputIndex].OutputName.ToString();
				}
				Conn.DstGuid = FGuid(); // no dst expression — it's the material itself
				Conn.DstPropertyName = PropertyNameFromEnum(Prop);
				Graph.OutputConnections.Add(MoveTemp(Conn));
			}
		}
	}
	else if (UMaterialFunction* MF = Cast<UMaterialFunction>(LoadedObj))
	{
		Graph.bIsMaterialFunction = true;
		Expressions = MF->GetExpressions();
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge: GetMaterialGraph: '%s' is neither UMaterial nor UMaterialFunction (%s)"),
			*MaterialPath, *LoadedObj->GetClass()->GetName());
		Graph.bFound = false;
		return Graph;
	}

	Graph.Nodes.Reserve(Expressions.Num());
	for (const TObjectPtr<UMaterialExpression>& ExprPtr : Expressions)
	{
		if (UMaterialExpression* Expr = ExprPtr.Get())
		{
			Graph.Nodes.Add(BuildGraphNode(Expr));
		}
	}

	CollectConnectionsFromExpressions(Expressions, Graph.Connections);

	return Graph;
}

FBridgeMaterialStats UUnrealBridgeMaterialLibrary::GetMaterialStats(
	const FString& MaterialPath,
	const FString& FeatureLevelStr,
	const FString& QualityStr)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialStats Stats;

	UMaterialInterface* MatInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!MatInterface)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetMaterialStats could not load '%s'"), *MaterialPath);
		return Stats;
	}

	const ERHIFeatureLevel::Type FeatureLevel = ParseFeatureLevel(FeatureLevelStr);
	const EMaterialQualityLevel::Type Quality = ParseQuality(QualityStr);

	Stats.bFound = true;
	Stats.Path = MatInterface->GetPathName();
	Stats.FeatureLevel = FeatureLevelToString(FeatureLevel);
	Stats.QualityLevel = QualityToString(Quality);

	const FMaterialResource* Resource = ResolveMaterialResource(MatInterface, FeatureLevel, Quality);
	if (!Resource)
	{
		// No compiled resource yet — report what we can.
		return Stats;
	}

	Stats.bShaderMapReady = (Resource->GetGameThreadShaderMap() != nullptr);
	Stats.CompileErrors = Resource->GetCompileErrors();

	CollectShaderInstructionStats(Resource, Stats.Shaders);
	Stats.VirtualTextureStackCount = (int32)Resource->GetNumVirtualTextureStacks();

	return Stats;
}

namespace BridgeMaterialImpl
{
	static UStaticMesh* LoadPreviewMesh(const FString& Preset)
	{
		FString Path;
		const FString Key = Preset.ToLower();
		if (Key.IsEmpty() || Key == TEXT("sphere"))   Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		else if (Key == TEXT("plane"))                Path = TEXT("/Engine/BasicShapes/Plane.Plane");
		else if (Key == TEXT("cube"))                 Path = TEXT("/Engine/BasicShapes/Cube.Cube");
		else if (Key == TEXT("cylinder"))             Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		else if (Key == TEXT("cone"))                 Path = TEXT("/Engine/BasicShapes/Cone.Cone");
		// UE's own Material Editor preview uses this "shader ball" — varied geometry
		// (rounded box + cutouts + flat + curved) that reveals normal / roughness /
		// specular variation much better than a sphere.
		else if (Key == TEXT("shaderball") || Key == TEXT("shader_ball") || Key == TEXT("matpreview"))
		{
			Path = TEXT("/Engine/EngineMeshes/SM_MatPreviewMesh_01.SM_MatPreviewMesh_01");
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown preview mesh '%s' — falling back to shaderball"), *Preset);
			Path = TEXT("/Engine/EngineMeshes/SM_MatPreviewMesh_01.SM_MatPreviewMesh_01");
		}
		return LoadObject<UStaticMesh>(nullptr, *Path);
	}

	/**
	 * UE Material Editor's AssetViewer uses this HDRI for the preview viewport. It's an
	 * outdoor studio environment — strong gradient sky with shaped highlights — which
	 * is exactly what's needed to make metallic / glossy materials look metallic.
	 * If missing (shouldn't happen), falls back to GrayLightTextureCube.
	 */
	static UTextureCube* LoadDefaultHDRICubemap()
	{
		UTextureCube* HDR = LoadObject<UTextureCube>(
			nullptr, TEXT("/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1.EpicQuadPanorama_CC+EV1"));
		if (HDR) return HDR;
		return LoadObject<UTextureCube>(nullptr, TEXT("/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube"));
	}

	static void SetupPreviewLighting(FPreviewScene& Scene, const FString& Preset)
	{
		const FString Key = Preset.ToLower();

		if (Key.IsEmpty() || Key == TEXT("studio"))
		{
			// 3-point directional setup — matches the anim pose-grid capture.
			UDirectionalLightComponent* Key3P = NewObject<UDirectionalLightComponent>(
				GetTransientPackage(), UDirectionalLightComponent::StaticClass());
			Key3P->SetIntensity(4.0f);
			Key3P->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));
			Scene.AddComponent(Key3P, FTransform(FRotator(-45.0f, 30.0f, 0.0f)));

			UDirectionalLightComponent* Fill3P = NewObject<UDirectionalLightComponent>(
				GetTransientPackage(), UDirectionalLightComponent::StaticClass());
			Fill3P->SetIntensity(1.5f);
			Fill3P->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f));
			Scene.AddComponent(Fill3P, FTransform(FRotator(-30.0f, -150.0f, 0.0f)));

			UDirectionalLightComponent* Rim3P = NewObject<UDirectionalLightComponent>(
				GetTransientPackage(), UDirectionalLightComponent::StaticClass());
			Rim3P->SetIntensity(2.0f);
			Rim3P->SetLightColor(FLinearColor(0.95f, 0.95f, 1.0f));
			Scene.AddComponent(Rim3P, FTransform(FRotator(-20.0f, 180.0f, 0.0f)));
		}
		else if (Key == TEXT("hdri") || Key == TEXT("outdoor"))
		{
			// SkyLight with a real HDR cubemap, not SLS_CapturedScene (which captures
			// an empty PreviewScene = black reflections, metals look flat).
			UTextureCube* HDR = LoadDefaultHDRICubemap();

			USkyLightComponent* Sky = NewObject<USkyLightComponent>(
				GetTransientPackage(), USkyLightComponent::StaticClass());
			Sky->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
			Sky->Cubemap = HDR;
			Sky->SetIntensity(1.0f);
			// Skylight captures the cubemap once on AddComponent; SetCaptureIsDirty forces
			// a refresh so the real-time capture path picks it up.
			Scene.AddComponent(Sky, FTransform::Identity);
			Sky->SetCaptureIsDirty();
			Sky->MarkRenderStateDirty();

			UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(
				GetTransientPackage(), UDirectionalLightComponent::StaticClass());
			Sun->SetIntensity(5.0f);
			Sun->SetLightColor(FLinearColor(1.0f, 0.96f, 0.88f));
			Scene.AddComponent(Sun, FTransform(FRotator(-40.0f, 60.0f, 0.0f)));
		}
		else if (Key == TEXT("night"))
		{
			UDirectionalLightComponent* Moon = NewObject<UDirectionalLightComponent>(
				GetTransientPackage(), UDirectionalLightComponent::StaticClass());
			Moon->SetIntensity(0.3f);
			Moon->SetLightColor(FLinearColor(0.6f, 0.75f, 1.0f));
			Scene.AddComponent(Moon, FTransform(FRotator(-65.0f, 45.0f, 0.0f)));

			USkyLightComponent* Sky = NewObject<USkyLightComponent>(
				GetTransientPackage(), USkyLightComponent::StaticClass());
			Sky->SetIntensity(0.15f);
			Sky->SourceType = ESkyLightSourceType::SLS_CapturedScene;
			Scene.AddComponent(Sky, FTransform::Identity);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown lighting preset '%s' — falling back to studio"), *Preset);
			SetupPreviewLighting(Scene, TEXT("studio"));
		}
	}

	static bool SaveBGRAToPng(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		const FString& OutPath)
	{
		FImage Img;
		Img.Init(Width, Height, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		// FPreviewScene's RT readback is already top-down — no flip needed here, unlike
		// the editor-world capture in UnrealBridgeLevelLibrary where ReadPixels lands
		// bottom-up. The preview-world render path seems to correct the Y-flip internally.
		FMemory::Memcpy(Img.RawData.GetData(), Pixels.GetData(), Width * Height * sizeof(FColor));

		FString Resolved = OutPath;
		if (FPaths::IsRelative(Resolved))
		{
			Resolved = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Resolved);
		}
		return FImageUtils::SaveImageByExtension(*Resolved, Img, /*CompressionQuality=*/ 0);
	}

	static bool RenderMaterialPreview(
		const FString& MaterialPath,
		const FString& MeshPreset,
		const FString& LightingPreset,
		int32 Resolution,
		float CameraYawDeg,
		float CameraPitchDeg,
		float CameraDistance,
		const FString& OutPngPath,
		bool bShaderComplexity)
	{
		UMaterialInterface* MatInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!MatInterface)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: PreviewMaterial could not load '%s'"), *MaterialPath);
			return false;
		}

		UStaticMesh* PreviewMesh = LoadPreviewMesh(MeshPreset);
		if (!PreviewMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: PreviewMaterial failed to load preview mesh for preset '%s'"), *MeshPreset);
			return false;
		}

		const int32 Res = FMath::Clamp(Resolution > 0 ? Resolution : 512, 32, 4096);

		FPreviewScene::ConstructionValues CVS;
		CVS.SetCreatePhysicsScene(false);
		CVS.SetTransactional(false);
		FPreviewScene PreviewScene(CVS);
		UWorld* PreviewWorld = PreviewScene.GetWorld();
		if (!PreviewWorld)
		{
			return false;
		}

		SetupPreviewLighting(PreviewScene, LightingPreset);

		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(
			GetTransientPackage(), UStaticMeshComponent::StaticClass());
		Comp->SetStaticMesh(PreviewMesh);
		Comp->SetMaterial(0, MatInterface);
		PreviewScene.AddComponent(Comp, FTransform::Identity);

		// Compute framing: use the mesh bounds radius + requested / auto distance.
		const FBoxSphereBounds Bounds = PreviewMesh->GetBounds();
		const float Radius = FMath::Max(Bounds.SphereRadius, 1.0f);

		constexpr float FOVDeg = 35.0f;
		const float AutoDistance = Radius / FMath::Tan(FMath::DegreesToRadians(FOVDeg * 0.5f));
		const float Distance = CameraDistance > 0.0f ? CameraDistance : AutoDistance;

		// Polar → cartesian camera position. Yaw rotates around Z, pitch up from equator.
		const float YawRad = FMath::DegreesToRadians(CameraYawDeg);
		const float PitchRad = FMath::DegreesToRadians(CameraPitchDeg);
		const FVector CamOffset(
			FMath::Cos(PitchRad) * FMath::Cos(YawRad),
			FMath::Cos(PitchRad) * FMath::Sin(YawRad),
			FMath::Sin(PitchRad));
		const FVector Origin = Bounds.Origin;
		const FVector CamLoc = Origin + CamOffset * Distance;
		const FRotator CamRot = (Origin - CamLoc).Rotation();

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		SpawnInfo.Name = MakeUniqueObjectName(PreviewWorld, ASceneCapture2D::StaticClass(), TEXT("BridgeMatPreviewCam"));
		ASceneCapture2D* CaptureActor = PreviewWorld->SpawnActor<ASceneCapture2D>(CamLoc, CamRot, SpawnInfo);
		if (!CaptureActor || !CaptureActor->GetCaptureComponent2D())
		{
			return false;
		}

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass(), TEXT("BridgeMatPreviewRT")));
		RT->ClearColor = FLinearColor(0.12f, 0.12f, 0.13f, 1.0f);
		RT->bAutoGenerateMips = false;
		RT->InitCustomFormat(Res, Res, PF_B8G8R8A8, /*bForceLinearGamma=*/ false);
		RT->UpdateResourceImmediate(true);

		USceneCaptureComponent2D* SCC = CaptureActor->GetCaptureComponent2D();
		SCC->ProjectionType = ECameraProjectionMode::Perspective;
		SCC->FOVAngle = FOVDeg;
		SCC->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		SCC->bCaptureEveryFrame = false;
		SCC->bCaptureOnMovement = false;
		SCC->TextureTarget = RT;

		if (bShaderComplexity)
		{
			// Toggle ShaderComplexity in the capture's show flags — SCS_FinalColorLDR
			// reads back the post-processed frame, which honours this view mode.
			// Use SetShowFlagSettings to avoid the UE 5.5 deprecation on direct field access.
			TArray<FEngineShowFlagsSetting> Settings = SCC->GetShowFlagSettings();
			FEngineShowFlagsSetting Setting;
			Setting.ShowFlagName = TEXT("ShaderComplexity");
			Setting.Enabled = true;
			Settings.Add(Setting);
			SCC->SetShowFlagSettings(Settings);
		}

		// 1. Block until all texture builds / shader compiles / mesh builds finish.
		//    Without this, fresh-editor captures render with placeholder / low-mip textures.
		FAssetCompilingManager::Get().FinishAllCompilation();

		// 2. Prestream the mesh component's textures to mip 0.
		Comp->PrestreamTextures(/*Seconds=*/ 30.0f, /*bEnableStreaming=*/ true);

		// 3. Warm-up capture — marks textures visible in the streaming system so the
		//    mip chain promotes. Without this the first real capture often still reads
		//    blurry mips on a cold scene.
		SCC->CaptureScene();
		FlushRenderingCommands();

		// 4. Force a global streaming update with generous timeout; blocks until the
		//    streamer catches up with the mip levels required by the just-rendered view.
		IStreamingManager::Get().StreamAllResources(/*TimeoutSec=*/ 5.0f);
		FlushRenderingCommands();

		// 5. Real capture with everything warmed up.
		SCC->CaptureScene();
		FlushRenderingCommands();

		TArray<FColor> Pixels;
		FTextureRenderTargetResource* Res2 = RT->GameThread_GetRenderTargetResource();
		if (!Res2)
		{
			CaptureActor->Destroy();
			return false;
		}
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
		ReadFlags.SetLinearToGamma(false);
		if (!Res2->ReadPixels(Pixels, ReadFlags) || Pixels.Num() != Res * Res)
		{
			CaptureActor->Destroy();
			return false;
		}

		const bool bSaved = SaveBGRAToPng(Pixels, Res, Res, OutPngPath);
		CaptureActor->Destroy();
		return bSaved;
	}
}

namespace BridgeMaterialImpl
{
	// ─── Enum parsers for write ops ───────────────────────────────

	static bool ParseDomain(const FString& Str, EMaterialDomain& Out)
	{
		const FString S = Str.ToLower();
		if (S == TEXT("surface") || S.IsEmpty()) { Out = MD_Surface; return true; }
		if (S == TEXT("deferreddecal"))          { Out = MD_DeferredDecal; return true; }
		if (S == TEXT("lightfunction"))          { Out = MD_LightFunction; return true; }
		if (S == TEXT("volume"))                 { Out = MD_Volume; return true; }
		if (S == TEXT("postprocess"))            { Out = MD_PostProcess; return true; }
		if (S == TEXT("ui"))                     { Out = MD_UI; return true; }
		if (S == TEXT("runtimevirtualtexture"))  { Out = MD_RuntimeVirtualTexture; return true; }
		return false;
	}

	static bool ParseBlendMode(const FString& Str, EBlendMode& Out)
	{
		const FString S = Str.ToLower();
		if (S == TEXT("opaque") || S.IsEmpty()) { Out = BLEND_Opaque; return true; }
		if (S == TEXT("masked"))                { Out = BLEND_Masked; return true; }
		if (S == TEXT("translucent"))           { Out = BLEND_Translucent; return true; }
		if (S == TEXT("additive"))              { Out = BLEND_Additive; return true; }
		if (S == TEXT("modulate"))              { Out = BLEND_Modulate; return true; }
		if (S == TEXT("alphacomposite"))        { Out = BLEND_AlphaComposite; return true; }
		if (S == TEXT("alphaholdout"))          { Out = BLEND_AlphaHoldout; return true; }
		return false;
	}

	static bool ParseShadingModel(const FString& Str, EMaterialShadingModel& Out)
	{
		const FString S = Str.ToLower();
		if (S == TEXT("defaultlit") || S.IsEmpty()) { Out = MSM_DefaultLit; return true; }
		if (S == TEXT("unlit"))                     { Out = MSM_Unlit; return true; }
		if (S == TEXT("subsurface"))                { Out = MSM_Subsurface; return true; }
		if (S == TEXT("preintegratedskin"))         { Out = MSM_PreintegratedSkin; return true; }
		if (S == TEXT("clearcoat"))                 { Out = MSM_ClearCoat; return true; }
		if (S == TEXT("subsurfaceprofile"))         { Out = MSM_SubsurfaceProfile; return true; }
		if (S == TEXT("twosidedfoliage"))           { Out = MSM_TwoSidedFoliage; return true; }
		if (S == TEXT("hair"))                      { Out = MSM_Hair; return true; }
		if (S == TEXT("cloth"))                     { Out = MSM_Cloth; return true; }
		if (S == TEXT("eye"))                       { Out = MSM_Eye; return true; }
		if (S == TEXT("singlelayerwater"))          { Out = MSM_SingleLayerWater; return true; }
		if (S == TEXT("thintranslucent"))           { Out = MSM_ThinTranslucent; return true; }
		if (S == TEXT("strata"))                    { Out = MSM_Strata; return true; }
		if (S == TEXT("frommaterialexpression"))    { Out = MSM_FromMaterialExpression; return true; }
		return false;
	}

	static bool ParseMaterialProperty(const FString& Str, EMaterialProperty& Out)
	{
		UEnum* PropEnum = StaticEnum<EMaterialProperty>();
		if (!PropEnum)
		{
			return false;
		}
		// Try "MP_BaseColor" first, then the stripped "BaseColor" form.
		int64 V = PropEnum->GetValueByName(FName(*FString::Printf(TEXT("MP_%s"), *Str)));
		if (V == INDEX_NONE)
		{
			V = PropEnum->GetValueByName(FName(*Str));
		}
		if (V == INDEX_NONE || V == (int64)MP_MAX)
		{
			return false;
		}
		Out = (EMaterialProperty)V;
		return true;
	}

	// ─── Class / path resolution ──────────────────────────────────

	static UClass* ResolveExpressionClass(const FString& PathOrShortName)
	{
		if (PathOrShortName.IsEmpty())
		{
			return nullptr;
		}

		// Fully-qualified path → LoadClass.
		if (PathOrShortName.StartsWith(TEXT("/")))
		{
			return LoadClass<UMaterialExpression>(nullptr, *PathOrShortName);
		}

		// Try the name as-is, then with the standard prefixes prepended.
		const FString Candidates[] = {
			PathOrShortName,
			FString::Printf(TEXT("MaterialExpression%s"), *PathOrShortName),
			FString::Printf(TEXT("UMaterialExpression%s"), *PathOrShortName),
		};
		for (const FString& Name : Candidates)
		{
			if (UClass* C = FindFirstObject<UClass>(*Name, EFindFirstObjectOptions::NativeFirst))
			{
				if (C->IsChildOf(UMaterialExpression::StaticClass()) &&
					!C->HasAnyClassFlags(CLASS_Abstract))
				{
					return C;
				}
			}
		}
		return nullptr;
	}

	static bool SplitAssetPath(const FString& Path, FString& OutPackagePath, FString& OutAssetName)
	{
		int32 LastSlash = INDEX_NONE;
		if (!Path.FindLastChar(TEXT('/'), LastSlash) || LastSlash == 0)
		{
			return false;
		}
		OutPackagePath = Path.Left(LastSlash);
		OutAssetName = Path.Mid(LastSlash + 1);
		// Strip trailing ".Name" form if present.
		int32 Dot = INDEX_NONE;
		if (OutAssetName.FindChar(TEXT('.'), Dot))
		{
			OutAssetName = OutAssetName.Left(Dot);
		}
		return !OutAssetName.IsEmpty() && !OutPackagePath.IsEmpty();
	}

	// ─── Expression lookup by guid ────────────────────────────────

	static UMaterialExpression* FindExpressionByGuid(UMaterial* Material, const FGuid& Guid)
	{
		if (!Material || !Guid.IsValid())
		{
			return nullptr;
		}
		for (const TObjectPtr<UMaterialExpression>& ExprPtr : Material->GetExpressions())
		{
			UMaterialExpression* Expr = ExprPtr.Get();
			if (Expr && Expr->MaterialExpressionGuid == Guid)
			{
				return Expr;
			}
		}
		return nullptr;
	}

	// ─── Pin name normalization + lookup ──────────────────────────

	/**
	 * Normalize a pin name for comparison.
	 *
	 * "" and "None" both mean the default anonymous output. We also apply
	 * UMaterialGraphNode::GetShortenPinName so the bridge accepts both the
	 * raw UPROPERTY name ("Coordinates") and the UI-shortened name ("UVs") —
	 * the engine's own ConnectMaterialExpressions only accepts the short form,
	 * so without this normalization the bridge would silently reject the raw
	 * field name that `get_material_graph` used to return.
	 */
	static FString NormalizePinName(const FString& Name)
	{
		if (Name.IsEmpty() || Name.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return FString();
		}
		const FName Short = UMaterialGraphNode::GetShortenPinName(FName(*Name));
		return Short.IsNone() ? FString() : Short.ToString();
	}

	/** Result of redirecting every downstream consumer of one expression output to another. */
	struct FRewireCounts
	{
		int32 ExpressionInputs = 0;
		int32 MainOutputs = 0;
	};

	/** Redirect every FExpressionInput / main-output that currently reads
	 *  OldSource[OldOutputIdx] to instead read NewSource[NewOutputIdx].
	 *  Used by static_switch_conversion + inline_trivial_custom to splice a
	 *  replacement node in without hunting for each consumer by hand.
	 *  FExpressionInput has no ``OutputName`` field in UE 5.7 — the output
	 *  label lives on the source expression's ``FExpressionOutput``, so we
	 *  only rewrite ``Expression`` + ``OutputIndex``.
	 */
	static FRewireCounts RedirectUsageToNewSource(
		UMaterial* Material,
		UMaterialExpression* OldSource, int32 OldOutputIdx,
		UMaterialExpression* NewSource, int32 NewOutputIdx)
	{
		FRewireCounts Counts;
		if (!Material || !OldSource || !NewSource || OldSource == NewSource)
		{
			return Counts;
		}

		for (const TObjectPtr<UMaterialExpression>& EP : Material->GetExpressions())
		{
			UMaterialExpression* E = EP.Get();
			if (!E || E == OldSource) continue;

			bool bModified = false;
			for (FExpressionInputIterator It{E}; It; ++It)
			{
				FExpressionInput* Input = It.Input;
				if (!Input) continue;
				if (Input->Expression == OldSource && Input->OutputIndex == OldOutputIdx)
				{
					if (!bModified)
					{
						E->Modify();
						bModified = true;
					}
					Input->Expression = NewSource;
					Input->OutputIndex = NewOutputIdx;
					++Counts.ExpressionInputs;
				}
			}
		}

		for (int32 P = 0; P < MP_MAX; ++P)
		{
			const EMaterialProperty Prop = (EMaterialProperty)P;
			FExpressionInput* MI = Material->GetExpressionInputForProperty(Prop);
			if (MI && MI->Expression == OldSource && MI->OutputIndex == OldOutputIdx)
			{
				MI->Expression = NewSource;
				MI->OutputIndex = NewOutputIdx;
				++Counts.MainOutputs;
			}
		}

		return Counts;
	}

	/** Returns true if at least one expression input (anywhere in the material)
	 *  still references the given expression as its source.
	 */
	static bool IsExpressionReferenced(UMaterial* Material, UMaterialExpression* Target)
	{
		if (!Material || !Target) return false;
		for (const TObjectPtr<UMaterialExpression>& EP : Material->GetExpressions())
		{
			UMaterialExpression* E = EP.Get();
			if (!E || E == Target) continue;
			for (FExpressionInputIterator It{E}; It; ++It)
			{
				if (It.Input && It.Input->Expression == Target)
				{
					return true;
				}
			}
		}
		for (int32 P = 0; P < MP_MAX; ++P)
		{
			FExpressionInput* MI = Material->GetExpressionInputForProperty((EMaterialProperty)P);
			if (MI && MI->Expression == Target) return true;
		}
		return false;
	}

	/** Find the index of an input pin by name on an expression, or INDEX_NONE. */
	static int32 FindInputIndexByName(UMaterialExpression* Expr, const FString& Name)
	{
		if (!Expr)
		{
			return INDEX_NONE;
		}
		const FString Target = NormalizePinName(Name);
		int32 i = 0;
		for (FExpressionInputIterator It{Expr}; It; ++It)
		{
			const FName Got = Expr->GetInputName(It.Index);
			const FString GotStr = NormalizePinName(Got.ToString());
			if (GotStr.Equals(Target, ESearchCase::IgnoreCase))
			{
				return It.Index;
			}
			++i;
		}
		// If the target is empty (default), fall back to index 0 when there's exactly one input.
		if (Target.IsEmpty() && i == 1)
		{
			return 0;
		}
		return INDEX_NONE;
	}
}

bool UUnrealBridgeMaterialLibrary::PreviewMaterial(
	const FString& MaterialPath,
	const FString& Mesh,
	const FString& Lighting,
	int32 Resolution,
	float CameraYawDeg,
	float CameraPitchDeg,
	float CameraDistance,
	const FString& OutPngPath)
{
	return BridgeMaterialImpl::RenderMaterialPreview(
		MaterialPath, Mesh, Lighting, Resolution,
		CameraYawDeg, CameraPitchDeg, CameraDistance,
		OutPngPath, /*bShaderComplexity=*/ false);
}

bool UUnrealBridgeMaterialLibrary::PreviewMaterialComplexity(
	const FString& MaterialPath,
	const FString& Mesh,
	const FString& Lighting,
	int32 Resolution,
	float CameraYawDeg,
	float CameraPitchDeg,
	float CameraDistance,
	const FString& OutPngPath)
{
	return BridgeMaterialImpl::RenderMaterialPreview(
		MaterialPath, Mesh, Lighting, Resolution,
		CameraYawDeg, CameraPitchDeg, CameraDistance,
		OutPngPath, /*bShaderComplexity=*/ true);
}

// ─── M2-1 / M2-2: asset creation ──────────────────────────────────

FBridgeCreateAssetResult UUnrealBridgeMaterialLibrary::CreateMaterial(
	const FString& Path,
	const FString& Domain,
	const FString& ShadingModel,
	const FString& BlendMode,
	bool bTwoSided,
	bool bUseMaterialAttributes)
{
	using namespace BridgeMaterialImpl;

	FBridgeCreateAssetResult Result;

	FString PackagePath, AssetName;
	if (!SplitAssetPath(Path, PackagePath, AssetName))
	{
		Result.Error = FString::Printf(TEXT("invalid path '%s' — expected /Game/Folder/AssetName"), *Path);
		return Result;
	}

	EMaterialDomain DomainVal;
	if (!ParseDomain(Domain, DomainVal))
	{
		Result.Error = FString::Printf(TEXT("unknown domain '%s'"), *Domain);
		return Result;
	}

	EBlendMode BlendVal;
	if (!ParseBlendMode(BlendMode, BlendVal))
	{
		Result.Error = FString::Printf(TEXT("unknown blend mode '%s'"), *BlendMode);
		return Result;
	}

	EMaterialShadingModel ShadingVal;
	if (!ParseShadingModel(ShadingModel, ShadingVal))
	{
		Result.Error = FString::Printf(TEXT("unknown shading model '%s'"), *ShadingModel);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);
	UMaterial* Material = Cast<UMaterial>(NewAsset);
	if (!Material)
	{
		Result.Error = FString::Printf(TEXT("AssetTools.CreateAsset returned null for %s/%s — path may already be occupied"),
			*PackagePath, *AssetName);
		return Result;
	}

	Material->MaterialDomain = DomainVal;
	Material->BlendMode = BlendVal;
	Material->SetShadingModel(ShadingVal);
	Material->TwoSided = bTwoSided;
	Material->bUseMaterialAttributes = bUseMaterialAttributes;

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Path = Material->GetPathName();
	return Result;
}

FBridgeCreateAssetResult UUnrealBridgeMaterialLibrary::CreateMaterialInstance(
	const FString& ParentPath,
	const FString& InstancePath)
{
	using namespace BridgeMaterialImpl;

	FBridgeCreateAssetResult Result;

	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
	if (!Parent)
	{
		Result.Error = FString::Printf(TEXT("could not load parent material '%s'"), *ParentPath);
		return Result;
	}

	FString PackagePath, AssetName;
	if (!SplitAssetPath(InstancePath, PackagePath, AssetName))
	{
		Result.Error = FString::Printf(TEXT("invalid instance path '%s'"), *InstancePath);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = Parent;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(NewAsset);
	if (!MI)
	{
		Result.Error = FString::Printf(TEXT("AssetTools.CreateAsset returned null for %s/%s"),
			*PackagePath, *AssetName);
		return Result;
	}

	// Factory should have wired Parent; ensure it.
	if (!MI->Parent)
	{
		MI->SetParentEditorOnly(Parent);
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Path = MI->GetPathName();
	return Result;
}

// ─── M2-4: expression factory ─────────────────────────────────────

FBridgeAddExpressionResult UUnrealBridgeMaterialLibrary::AddMaterialExpression(
	const FString& MaterialPath,
	const FString& ExpressionClass,
	int32 X,
	int32 Y)
{
	using namespace BridgeMaterialImpl;

	FBridgeAddExpressionResult Result;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
	{
		Result.Error = FString::Printf(TEXT("could not load material '%s'"), *MaterialPath);
		return Result;
	}

	UClass* Cls = ResolveExpressionClass(ExpressionClass);
	if (!Cls)
	{
		Result.Error = FString::Printf(TEXT("could not resolve expression class '%s'"), *ExpressionClass);
		return Result;
	}

	UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(Material, Cls, X, Y);
	if (!Expr)
	{
		Result.Error = FString::Printf(TEXT("CreateMaterialExpression returned null for class %s"), *Cls->GetName());
		return Result;
	}

	// Ensure the new node carries a stable guid (required so connections can reference it).
	if (!Expr->MaterialExpressionGuid.IsValid())
	{
		Expr->MaterialExpressionGuid = FGuid::NewGuid();
	}

	Material->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Guid = Expr->MaterialExpressionGuid;
	Result.ResolvedClass = Cls->GetName();
	return Result;
}

// ─── M2-5: expression ↔ expression wiring ─────────────────────────

bool UUnrealBridgeMaterialLibrary::ConnectMaterialExpressions(
	const FString& MaterialPath,
	FGuid SrcGuid, const FString& SrcOutputName,
	FGuid DstGuid, const FString& DstInputName)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	UMaterialExpression* Src = FindExpressionByGuid(Material, SrcGuid);
	UMaterialExpression* Dst = FindExpressionByGuid(Material, DstGuid);
	if (!Src || !Dst)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: ConnectMaterialExpressions: guid not found (src=%s dst=%s)"),
			*SrcGuid.ToString(), *DstGuid.ToString());
		return false;
	}

	const FString FromOut = NormalizePinName(SrcOutputName);
	const FString ToIn = NormalizePinName(DstInputName);

	const bool bOK = UMaterialEditingLibrary::ConnectMaterialExpressions(Src, FromOut, Dst, ToIn);
	if (bOK)
	{
		Material->MarkPackageDirty();
	}
	return bOK;
}

bool UUnrealBridgeMaterialLibrary::DisconnectMaterialInput(
	const FString& MaterialPath,
	FGuid DstGuid, const FString& DstInputName)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	UMaterialExpression* Dst = FindExpressionByGuid(Material, DstGuid);
	if (!Dst) return false;

	const int32 Idx = FindInputIndexByName(Dst, DstInputName);
	if (Idx == INDEX_NONE) return false;

	FExpressionInput* Input = Dst->GetInput(Idx);
	if (!Input || !Input->Expression)
	{
		return false;
	}

	Input->Expression = nullptr;
	Input->OutputIndex = 0;

	Dst->Modify();
	Material->PostEditChange();
	Material->MarkPackageDirty();
	return true;
}

// ─── M2-6: main-output wiring ─────────────────────────────────────

bool UUnrealBridgeMaterialLibrary::ConnectMaterialOutput(
	const FString& MaterialPath,
	FGuid SrcGuid, const FString& SrcOutputName,
	const FString& PropertyName)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	UMaterialExpression* Src = FindExpressionByGuid(Material, SrcGuid);
	if (!Src)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: ConnectMaterialOutput: src guid not found (%s)"),
			*SrcGuid.ToString());
		return false;
	}

	EMaterialProperty Prop;
	if (!ParseMaterialProperty(PropertyName, Prop))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: ConnectMaterialOutput: unknown property '%s'"), *PropertyName);
		return false;
	}

	const bool bOK = UMaterialEditingLibrary::ConnectMaterialProperty(Src, NormalizePinName(SrcOutputName), Prop);
	if (bOK)
	{
		Material->MarkPackageDirty();
	}
	return bOK;
}

bool UUnrealBridgeMaterialLibrary::DisconnectMaterialOutput(
	const FString& MaterialPath,
	const FString& PropertyName)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	EMaterialProperty Prop;
	if (!ParseMaterialProperty(PropertyName, Prop))
	{
		return false;
	}

	FExpressionInput* Input = Material->GetExpressionInputForProperty(Prop);
	if (!Input || !Input->Expression)
	{
		return false;
	}

	Input->Expression = nullptr;
	Input->OutputIndex = 0;

	Material->PostEditChange();
	Material->MarkPackageDirty();
	return true;
}

// ─── M2-4 companion: delete ───────────────────────────────────────

bool UUnrealBridgeMaterialLibrary::DeleteMaterialExpression(
	const FString& MaterialPath,
	FGuid Guid)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	UMaterialExpression* Expr = FindExpressionByGuid(Material, Guid);
	if (!Expr) return false;

	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);
	Material->MarkPackageDirty();
	return true;
}

// ─── M2-7: property setter (reflection / ImportText) ──────────────

namespace BridgeMaterialImpl
{
	/**
	 * Apply one ImportText-style value to a UPROPERTY on the expression.
	 * Returns true on success.
	 */
	static bool ApplyPropertyString(UMaterialExpression* Expr, const FString& PropertyName, const FString& Value)
	{
		if (!Expr)
		{
			return false;
		}
		FProperty* Prop = Expr->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Prop)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: property '%s' not found on %s"),
				*PropertyName, *Expr->GetClass()->GetName());
			return false;
		}

		void* Container = Prop->ContainerPtrToValuePtr<void>(Expr);
		const TCHAR* Remaining = Prop->ImportText_Direct(*Value, Container, Expr, PPF_None);
		if (Remaining == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: ImportText failed for %s::%s = '%s'"),
				*Expr->GetClass()->GetName(), *PropertyName, *Value);
			return false;
		}
		return true;
	}
}

bool UUnrealBridgeMaterialLibrary::SetMaterialExpressionProperty(
	const FString& MaterialPath,
	FGuid Guid,
	const FString& PropertyName,
	const FString& Value)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	UMaterialExpression* Expr = FindExpressionByGuid(Material, Guid);
	if (!Expr) return false;

	Expr->PreEditChange(nullptr);
	const bool bOK = ApplyPropertyString(Expr, PropertyName, Value);
	Expr->PostEditChange();

	if (bOK)
	{
		Material->MarkPackageDirty();
	}
	return bOK;
}

int32 UUnrealBridgeMaterialLibrary::SetMaterialExpressionProperties(
	const FString& MaterialPath,
	FGuid Guid,
	const TArray<FBridgeExpressionPropSet>& Properties)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return 0;

	UMaterialExpression* Expr = FindExpressionByGuid(Material, Guid);
	if (!Expr) return 0;

	Expr->PreEditChange(nullptr);
	int32 Applied = 0;
	for (const FBridgeExpressionPropSet& P : Properties)
	{
		if (ApplyPropertyString(Expr, P.Name, P.Value))
		{
			++Applied;
		}
	}
	Expr->PostEditChange();

	if (Applied > 0)
	{
		Material->MarkPackageDirty();
	}
	return Applied;
}

// ─── M2-8: comment + reroute factories ────────────────────────────

FGuid UUnrealBridgeMaterialLibrary::AddMaterialComment(
	const FString& MaterialPath,
	int32 X, int32 Y,
	int32 Width, int32 Height,
	const FString& Text,
	FLinearColor Color)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return FGuid();

	UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionComment::StaticClass(), X, Y);
	UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expr);
	if (!Comment) return FGuid();

	Comment->SizeX = FMath::Max(Width, 32);
	Comment->SizeY = FMath::Max(Height, 32);
	Comment->Text = Text;
	Comment->CommentColor = Color;

	if (!Comment->MaterialExpressionGuid.IsValid())
	{
		Comment->MaterialExpressionGuid = FGuid::NewGuid();
	}
	Comment->PostEditChange();
	Material->MarkPackageDirty();
	return Comment->MaterialExpressionGuid;
}

FGuid UUnrealBridgeMaterialLibrary::AddMaterialReroute(
	const FString& MaterialPath,
	int32 X, int32 Y)
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return FGuid();

	UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionReroute::StaticClass(), X, Y);
	if (!Expr) return FGuid();

	if (!Expr->MaterialExpressionGuid.IsValid())
	{
		Expr->MaterialExpressionGuid = FGuid::NewGuid();
	}
	Material->MarkPackageDirty();
	return Expr->MaterialExpressionGuid;
}

// ─── M2-11: compile ───────────────────────────────────────────────

// ─── M2-3: MaterialFunction asset creation ────────────────────────

FBridgeCreateAssetResult UUnrealBridgeMaterialLibrary::CreateMaterialFunction(
	const FString& Path,
	const FString& Description,
	bool bExposeToLibrary,
	const FString& LibraryCategory)
{
	using namespace BridgeMaterialImpl;

	FBridgeCreateAssetResult Result;

	FString PackagePath, AssetName;
	if (!SplitAssetPath(Path, PackagePath, AssetName))
	{
		Result.Error = FString::Printf(TEXT("invalid path '%s'"), *Path);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialFunctionFactoryNew* Factory = NewObject<UMaterialFunctionFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterialFunction::StaticClass(), Factory);
	UMaterialFunction* MF = Cast<UMaterialFunction>(NewAsset);
	if (!MF)
	{
		Result.Error = FString::Printf(TEXT("AssetTools.CreateAsset returned null for %s/%s"),
			*PackagePath, *AssetName);
		return Result;
	}

	MF->Description = Description;
	MF->bExposeToLibrary = bExposeToLibrary;
	if (!LibraryCategory.IsEmpty())
	{
		MF->LibraryCategoriesText.Add(FText::FromString(LibraryCategory));
	}

	MF->PostEditChange();
	MF->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Path = MF->GetPathName();
	return Result;
}

// ─── M2-10: batched graph ops ─────────────────────────────────────

namespace BridgeMaterialImpl
{
	static bool ResolveRef(const FString& Ref, const TArray<FGuid>& OpGuids, FGuid& Out)
	{
		if (Ref.IsEmpty())
		{
			return false;
		}
		if (Ref.StartsWith(TEXT("$")))
		{
			const int32 Idx = FCString::Atoi(*Ref + 1);
			if (!OpGuids.IsValidIndex(Idx))
			{
				return false;
			}
			Out = OpGuids[Idx];
			return Out.IsValid();
		}
		return FGuid::Parse(Ref, Out);
	}
}

FBridgeMaterialGraphOpResult UUnrealBridgeMaterialLibrary::ApplyMaterialGraphOps(
	const FString& MaterialPath,
	const TArray<FBridgeMaterialGraphOp>& Ops,
	bool bCompile)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialGraphOpResult Result;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
	{
		Result.Error = FString::Printf(TEXT("could not load material '%s'"), *MaterialPath);
		return Result;
	}

	Result.Guids.SetNum(Ops.Num());

	auto Fail = [&](int32 Idx, const FString& Msg) -> FBridgeMaterialGraphOpResult&
	{
		Result.FailedAtIndex = Idx;
		Result.Error = Msg;
		return Result;
	};

	for (int32 i = 0; i < Ops.Num(); ++i)
	{
		const FBridgeMaterialGraphOp& O = Ops[i];
		const FString Op = O.Op.ToLower();

		if (Op == TEXT("add"))
		{
			UClass* Cls = ResolveExpressionClass(O.ClassName);
			if (!Cls) return Fail(i, FString::Printf(TEXT("op %d (add): could not resolve class '%s'"), i, *O.ClassName));

			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(Material, Cls, O.X, O.Y);
			if (!Expr) return Fail(i, FString::Printf(TEXT("op %d (add): CreateMaterialExpression returned null"), i));

			if (!Expr->MaterialExpressionGuid.IsValid())
			{
				Expr->MaterialExpressionGuid = FGuid::NewGuid();
			}
			Result.Guids[i] = Expr->MaterialExpressionGuid;
		}
		else if (Op == TEXT("comment"))
		{
			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
				Material, UMaterialExpressionComment::StaticClass(), O.X, O.Y);
			UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expr);
			if (!Comment) return Fail(i, FString::Printf(TEXT("op %d (comment): create failed"), i));

			Comment->SizeX = FMath::Max(O.Width, 32);
			Comment->SizeY = FMath::Max(O.Height, 32);
			Comment->Text = O.Text;
			Comment->CommentColor = O.Color;
			if (!Comment->MaterialExpressionGuid.IsValid())
			{
				Comment->MaterialExpressionGuid = FGuid::NewGuid();
			}
			Result.Guids[i] = Comment->MaterialExpressionGuid;
		}
		else if (Op == TEXT("reroute"))
		{
			UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
				Material, UMaterialExpressionReroute::StaticClass(), O.X, O.Y);
			if (!Expr) return Fail(i, FString::Printf(TEXT("op %d (reroute): create failed"), i));
			if (!Expr->MaterialExpressionGuid.IsValid())
			{
				Expr->MaterialExpressionGuid = FGuid::NewGuid();
			}
			Result.Guids[i] = Expr->MaterialExpressionGuid;
		}
		else if (Op == TEXT("connect"))
		{
			FGuid SrcGuid, DstGuid;
			if (!ResolveRef(O.SrcRef, Result.Guids, SrcGuid)) return Fail(i, FString::Printf(TEXT("op %d (connect): bad src_ref '%s'"), i, *O.SrcRef));
			if (!ResolveRef(O.DstRef, Result.Guids, DstGuid)) return Fail(i, FString::Printf(TEXT("op %d (connect): bad dst_ref '%s'"), i, *O.DstRef));

			UMaterialExpression* Src = FindExpressionByGuid(Material, SrcGuid);
			UMaterialExpression* Dst = FindExpressionByGuid(Material, DstGuid);
			if (!Src || !Dst) return Fail(i, FString::Printf(TEXT("op %d (connect): expression not found"), i));

			if (!UMaterialEditingLibrary::ConnectMaterialExpressions(
				Src, NormalizePinName(O.SrcOutput), Dst, NormalizePinName(O.DstInput)))
			{
				return Fail(i, FString::Printf(TEXT("op %d (connect): pin mismatch (src_out='%s' dst_in='%s')"),
					i, *O.SrcOutput, *O.DstInput));
			}
		}
		else if (Op == TEXT("connect_out"))
		{
			FGuid SrcGuid;
			if (!ResolveRef(O.SrcRef, Result.Guids, SrcGuid)) return Fail(i, FString::Printf(TEXT("op %d (connect_out): bad src_ref '%s'"), i, *O.SrcRef));

			UMaterialExpression* Src = FindExpressionByGuid(Material, SrcGuid);
			if (!Src) return Fail(i, FString::Printf(TEXT("op %d (connect_out): source not found"), i));

			EMaterialProperty Prop;
			if (!ParseMaterialProperty(O.Property, Prop)) return Fail(i, FString::Printf(TEXT("op %d (connect_out): unknown property '%s'"), i, *O.Property));

			if (!UMaterialEditingLibrary::ConnectMaterialProperty(Src, NormalizePinName(O.SrcOutput), Prop))
			{
				return Fail(i, FString::Printf(TEXT("op %d (connect_out): ConnectMaterialProperty returned false"), i));
			}
		}
		else if (Op == TEXT("disconnect_in"))
		{
			FGuid DstGuid;
			if (!ResolveRef(O.DstRef, Result.Guids, DstGuid)) return Fail(i, FString::Printf(TEXT("op %d (disconnect_in): bad dst_ref '%s'"), i, *O.DstRef));

			UMaterialExpression* Dst = FindExpressionByGuid(Material, DstGuid);
			if (!Dst) return Fail(i, FString::Printf(TEXT("op %d (disconnect_in): not found"), i));

			const int32 Idx = FindInputIndexByName(Dst, O.DstInput);
			if (Idx == INDEX_NONE) return Fail(i, FString::Printf(TEXT("op %d (disconnect_in): unknown input '%s'"), i, *O.DstInput));

			FExpressionInput* Input = Dst->GetInput(Idx);
			if (Input)
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
			}
		}
		else if (Op == TEXT("disconnect_out"))
		{
			EMaterialProperty Prop;
			if (!ParseMaterialProperty(O.Property, Prop)) return Fail(i, FString::Printf(TEXT("op %d (disconnect_out): unknown property '%s'"), i, *O.Property));
			FExpressionInput* Input = Material->GetExpressionInputForProperty(Prop);
			if (Input)
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
			}
		}
		else if (Op == TEXT("set_prop"))
		{
			FGuid DstGuid;
			if (!ResolveRef(O.DstRef, Result.Guids, DstGuid)) return Fail(i, FString::Printf(TEXT("op %d (set_prop): bad dst_ref '%s'"), i, *O.DstRef));

			UMaterialExpression* Dst = FindExpressionByGuid(Material, DstGuid);
			if (!Dst) return Fail(i, FString::Printf(TEXT("op %d (set_prop): not found"), i));

			if (!ApplyPropertyString(Dst, O.Property, O.Value))
			{
				return Fail(i, FString::Printf(TEXT("op %d (set_prop): could not set %s = %s"), i, *O.Property, *O.Value));
			}
		}
		else if (Op == TEXT("delete"))
		{
			FGuid DstGuid;
			if (!ResolveRef(O.DstRef, Result.Guids, DstGuid)) return Fail(i, FString::Printf(TEXT("op %d (delete): bad dst_ref '%s'"), i, *O.DstRef));

			UMaterialExpression* Dst = FindExpressionByGuid(Material, DstGuid);
			if (!Dst) return Fail(i, FString::Printf(TEXT("op %d (delete): not found"), i));

			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Dst);
		}
		else
		{
			return Fail(i, FString::Printf(TEXT("op %d: unknown op '%s'"), i, *O.Op));
		}

		Result.OpsApplied = i + 1;
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	if (bCompile)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		FAssetCompilingManager::Get().FinishAllCompilation();
	}

	Result.bSuccess = true;
	return Result;
}

// ─── M2-9: auto-layout ────────────────────────────────────────────

int32 UUnrealBridgeMaterialLibrary::AutoLayoutMaterialGraph(
	const FString& MaterialPath,
	int32 ColumnSpacing,
	int32 RowSpacing)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return 0;

	const int32 ColStep = ColumnSpacing > 0 ? ColumnSpacing : 260;
	const int32 RowStep = RowSpacing > 0 ? RowSpacing : 120;

	TMap<UMaterialExpression*, int32> Column;

	TArray<UMaterialExpression*> Frontier;
	UEnum* PropEnum = StaticEnum<EMaterialProperty>();
	if (PropEnum)
	{
		for (int32 EnumIdx = 0; EnumIdx < PropEnum->NumEnums(); ++EnumIdx)
		{
			const int64 V = PropEnum->GetValueByIndex(EnumIdx);
			if (V == INDEX_NONE || V == (int64)MP_MAX) continue;
			const EMaterialProperty P = (EMaterialProperty)V;
			FExpressionInput* Input = Material->GetExpressionInputForProperty(P);
			if (Input && Input->Expression)
			{
				if (!Column.Contains(Input->Expression))
				{
					Column.Add(Input->Expression, 0);
					Frontier.Add(Input->Expression);
				}
			}
		}
	}

	while (Frontier.Num() > 0)
	{
		UMaterialExpression* Cur = Frontier.Pop(EAllowShrinking::No);
		const int32 Depth = Column[Cur];
		for (FExpressionInputIterator It{Cur}; It; ++It)
		{
			if (It.Input && It.Input->Expression)
			{
				UMaterialExpression* Up = It.Input->Expression;
				int32& ExistingDepth = Column.FindOrAdd(Up, INT32_MIN);
				if (Depth + 1 > ExistingDepth)
				{
					ExistingDepth = Depth + 1;
					Frontier.Add(Up);
				}
			}
		}
	}

	int32 MaxDepth = 0;
	for (const TPair<UMaterialExpression*, int32>& KV : Column)
	{
		MaxDepth = FMath::Max(MaxDepth, KV.Value);
	}
	const int32 LimboCol = MaxDepth + 2;

	const TConstArrayView<TObjectPtr<UMaterialExpression>> AllExprs = Material->GetExpressions();
	for (const TObjectPtr<UMaterialExpression>& EP : AllExprs)
	{
		UMaterialExpression* E = EP.Get();
		if (!E || Column.Contains(E)) continue;
		if (E->IsA<UMaterialExpressionComment>()) continue;
		Column.Add(E, LimboCol);
	}

	TMap<int32, TArray<UMaterialExpression*>> ByColumn;
	for (const TPair<UMaterialExpression*, int32>& KV : Column)
	{
		ByColumn.FindOrAdd(KV.Value).Add(KV.Key);
	}
	for (TPair<int32, TArray<UMaterialExpression*>>& Bucket : ByColumn)
	{
		Bucket.Value.Sort([](const UMaterialExpression& A, const UMaterialExpression& B)
		{
			return A.MaterialExpressionEditorY < B.MaterialExpressionEditorY;
		});
	}

	int32 Moved = 0;
	for (TPair<int32, TArray<UMaterialExpression*>>& Bucket : ByColumn)
	{
		const int32 Col = Bucket.Key;
		const int32 X = -Col * ColStep;
		const int32 Count = Bucket.Value.Num();
		const int32 TotalHeight = (Count - 1) * RowStep;
		const int32 YStart = -TotalHeight / 2;
		for (int32 i = 0; i < Count; ++i)
		{
			UMaterialExpression* E = Bucket.Value[i];
			E->MaterialExpressionEditorX = X;
			E->MaterialExpressionEditorY = YStart + i * RowStep;
			++Moved;
		}
	}

	// Do NOT call Material->PostEditChange here — it invalidates the material resource and
	// forces a shader recompile, which isn't needed for a position-only change and
	// temporarily falls back to DefaultMaterial rendering until the recompile finishes.
	Material->MarkPackageDirty();
	return Moved;
}

// ─── M2-12: snapshot + diff ───────────────────────────────────────

namespace BridgeMaterialImpl
{
	static TSharedPtr<FJsonObject> BuildGraphJson(UMaterial* Material)
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

		struct FNodeEntry
		{
			FString GuidStr;
			FString ClassName;
			int32 X, Y;
			FString Desc;
			FString KeyProps;
			TArray<FString> InputNames;
			TArray<FString> OutputNames;
		};
		TArray<FNodeEntry> Nodes;

		for (const TObjectPtr<UMaterialExpression>& EP : Material->GetExpressions())
		{
			UMaterialExpression* E = EP.Get();
			if (!E) continue;
			FNodeEntry N;
			N.GuidStr = E->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens);
			FString Cls = E->GetClass()->GetName();
			Cls.RemoveFromStart(TEXT("UMaterialExpression"));
			Cls.RemoveFromStart(TEXT("MaterialExpression"));
			N.ClassName = Cls;
			N.X = E->MaterialExpressionEditorX;
			N.Y = E->MaterialExpressionEditorY;
			N.Desc = E->Desc;
			N.KeyProps = DescribeExpressionKeyProps(E);
			for (FExpressionInputIterator It{E}; It; ++It)
			{
				N.InputNames.Add(NormalizePinName(E->GetInputName(It.Index).ToString()));
			}
			for (const FExpressionOutput& Out : E->GetOutputs())
			{
				N.OutputNames.Add(NormalizePinName(Out.OutputName.ToString()));
			}
			Nodes.Add(MoveTemp(N));
		}

		Nodes.Sort([](const FNodeEntry& A, const FNodeEntry& B) { return A.GuidStr < B.GuidStr; });

		TArray<TSharedPtr<FJsonValue>> NodesArr;
		for (const FNodeEntry& N : Nodes)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("guid"), N.GuidStr);
			Obj->SetStringField(TEXT("class"), N.ClassName);
			Obj->SetNumberField(TEXT("x"), N.X);
			Obj->SetNumberField(TEXT("y"), N.Y);
			Obj->SetStringField(TEXT("desc"), N.Desc);
			Obj->SetStringField(TEXT("key_props"), N.KeyProps);

			TArray<TSharedPtr<FJsonValue>> InArr;
			for (const FString& S : N.InputNames) InArr.Add(MakeShared<FJsonValueString>(S));
			Obj->SetArrayField(TEXT("inputs"), InArr);

			TArray<TSharedPtr<FJsonValue>> OutArr;
			for (const FString& S : N.OutputNames) OutArr.Add(MakeShared<FJsonValueString>(S));
			Obj->SetArrayField(TEXT("outputs"), OutArr);

			NodesArr.Add(MakeShared<FJsonValueObject>(Obj));
		}
		Root->SetArrayField(TEXT("nodes"), NodesArr);

		struct FConnEntry
		{
			FString Key;
			FString SrcGuid, SrcOut, DstGuid, DstIn;
		};
		TArray<FConnEntry> Conns;
		for (const TObjectPtr<UMaterialExpression>& EP : Material->GetExpressions())
		{
			UMaterialExpression* E = EP.Get();
			if (!E) continue;
			for (FExpressionInputIterator It{E}; It; ++It)
			{
				if (!It.Input || !It.Input->Expression) continue;
				FConnEntry C;
				C.SrcGuid = It.Input->Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens);
				TArray<FExpressionOutput>& Outs = It.Input->Expression->GetOutputs();
				C.SrcOut = Outs.IsValidIndex(It.Input->OutputIndex)
					? NormalizePinName(Outs[It.Input->OutputIndex].OutputName.ToString())
					: FString();
				C.DstGuid = E->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens);
				C.DstIn = NormalizePinName(E->GetInputName(It.Index).ToString());
				C.Key = FString::Printf(TEXT("%s|%s|%s|%s"), *C.SrcGuid, *C.SrcOut, *C.DstGuid, *C.DstIn);
				Conns.Add(MoveTemp(C));
			}
		}
		Conns.Sort([](const FConnEntry& A, const FConnEntry& B) { return A.Key < B.Key; });

		TArray<TSharedPtr<FJsonValue>> ConnArr;
		for (const FConnEntry& C : Conns)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("src"), C.SrcGuid);
			Obj->SetStringField(TEXT("src_out"), C.SrcOut);
			Obj->SetStringField(TEXT("dst"), C.DstGuid);
			Obj->SetStringField(TEXT("dst_in"), C.DstIn);
			ConnArr.Add(MakeShared<FJsonValueObject>(Obj));
		}
		Root->SetArrayField(TEXT("connections"), ConnArr);

		struct FOutEntry
		{
			FString Key;
			FString Property;
			FString SrcGuid;
			FString SrcOut;
		};
		TArray<FOutEntry> Outs;
		if (UEnum* PE = StaticEnum<EMaterialProperty>())
		{
			for (int32 EnumIdx = 0; EnumIdx < PE->NumEnums(); ++EnumIdx)
			{
				const int64 V = PE->GetValueByIndex(EnumIdx);
				if (V == INDEX_NONE || V == (int64)MP_MAX) continue;
				const EMaterialProperty P = (EMaterialProperty)V;
				FExpressionInput* In = Material->GetExpressionInputForProperty(P);
				if (!In || !In->Expression) continue;
				FOutEntry O;
				O.Property = PropertyNameFromEnum(P);
				O.SrcGuid = In->Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens);
				TArray<FExpressionOutput>& OutPins = In->Expression->GetOutputs();
				O.SrcOut = OutPins.IsValidIndex(In->OutputIndex)
					? NormalizePinName(OutPins[In->OutputIndex].OutputName.ToString())
					: FString();
				O.Key = FString::Printf(TEXT("%s|%s|%s"), *O.Property, *O.SrcGuid, *O.SrcOut);
				Outs.Add(MoveTemp(O));
			}
		}
		Outs.Sort([](const FOutEntry& A, const FOutEntry& B) { return A.Key < B.Key; });

		TArray<TSharedPtr<FJsonValue>> OutArr;
		for (const FOutEntry& O : Outs)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("property"), O.Property);
			Obj->SetStringField(TEXT("src"), O.SrcGuid);
			Obj->SetStringField(TEXT("src_out"), O.SrcOut);
			OutArr.Add(MakeShared<FJsonValueObject>(Obj));
		}
		Root->SetArrayField(TEXT("outputs"), OutArr);

		return Root;
	}
}

FString UUnrealBridgeMaterialLibrary::SnapshotMaterialGraphJson(const FString& MaterialPath)
{
	using namespace BridgeMaterialImpl;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return FString();

	TSharedPtr<FJsonObject> Root = BuildGraphJson(Material);
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

FString UUnrealBridgeMaterialLibrary::DiffMaterialGraphSnapshots(
	const FString& BeforeJson,
	const FString& AfterJson)
{
	auto Parse = [](const FString& Src, TSharedPtr<FJsonObject>& Out) -> bool
	{
		TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Src);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	};

	TSharedPtr<FJsonObject> A, B;
	if (!Parse(BeforeJson, A)) return TEXT("diff error: could not parse 'before' JSON");
	if (!Parse(AfterJson, B))  return TEXT("diff error: could not parse 'after' JSON");

	auto CollectNodes = [](const TSharedPtr<FJsonObject>& Root, TMap<FString, TSharedPtr<FJsonObject>>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Root->TryGetArrayField(TEXT("nodes"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) continue;
				Out.Add(Obj->GetStringField(TEXT("guid")), Obj);
			}
		}
	};
	auto CollectStringSet = [](const TSharedPtr<FJsonObject>& Root, const FString& Field, TSet<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Root->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) continue;
				FString Key;
				if (Field == TEXT("connections"))
				{
					Key = FString::Printf(TEXT("%s:%s -> %s:%s"),
						*Obj->GetStringField(TEXT("src")),
						*Obj->GetStringField(TEXT("src_out")),
						*Obj->GetStringField(TEXT("dst")),
						*Obj->GetStringField(TEXT("dst_in")));
				}
				else
				{
					Key = FString::Printf(TEXT("%s <- %s:%s"),
						*Obj->GetStringField(TEXT("property")),
						*Obj->GetStringField(TEXT("src")),
						*Obj->GetStringField(TEXT("src_out")));
				}
				Out.Add(Key);
			}
		}
	};

	TMap<FString, TSharedPtr<FJsonObject>> NodesA, NodesB;
	CollectNodes(A, NodesA);
	CollectNodes(B, NodesB);

	TSet<FString> ConnsA, ConnsB, OutsA, OutsB;
	CollectStringSet(A, TEXT("connections"), ConnsA);
	CollectStringSet(B, TEXT("connections"), ConnsB);
	CollectStringSet(A, TEXT("outputs"), OutsA);
	CollectStringSet(B, TEXT("outputs"), OutsB);

	TArray<FString> Lines;

	for (const TPair<FString, TSharedPtr<FJsonObject>>& KV : NodesB)
	{
		if (!NodesA.Contains(KV.Key))
		{
			Lines.Add(FString::Printf(TEXT("+ node %s  [%s]  (%d,%d)  %s"),
				*KV.Key,
				*KV.Value->GetStringField(TEXT("class")),
				(int32)KV.Value->GetNumberField(TEXT("x")),
				(int32)KV.Value->GetNumberField(TEXT("y")),
				*KV.Value->GetStringField(TEXT("key_props"))));
		}
	}
	for (const TPair<FString, TSharedPtr<FJsonObject>>& KV : NodesA)
	{
		if (!NodesB.Contains(KV.Key))
		{
			Lines.Add(FString::Printf(TEXT("- node %s  [%s]"),
				*KV.Key, *KV.Value->GetStringField(TEXT("class"))));
		}
		else
		{
			const TSharedPtr<FJsonObject>& OldN = KV.Value;
			const TSharedPtr<FJsonObject>& NewN = NodesB[KV.Key];
			const FString OldProps = OldN->GetStringField(TEXT("key_props"));
			const FString NewProps = NewN->GetStringField(TEXT("key_props"));
			if (OldProps != NewProps)
			{
				Lines.Add(FString::Printf(TEXT("* node %s  props: '%s' -> '%s'"),
					*KV.Key, *OldProps, *NewProps));
			}
			const int32 OldX = (int32)OldN->GetNumberField(TEXT("x"));
			const int32 OldY = (int32)OldN->GetNumberField(TEXT("y"));
			const int32 NewX = (int32)NewN->GetNumberField(TEXT("x"));
			const int32 NewY = (int32)NewN->GetNumberField(TEXT("y"));
			if (OldX != NewX || OldY != NewY)
			{
				Lines.Add(FString::Printf(TEXT("~ node %s  moved (%d,%d) -> (%d,%d)"),
					*KV.Key, OldX, OldY, NewX, NewY));
			}
		}
	}

	for (const FString& C : ConnsB)
	{
		if (!ConnsA.Contains(C)) Lines.Add(FString::Printf(TEXT("+ wire %s"), *C));
	}
	for (const FString& C : ConnsA)
	{
		if (!ConnsB.Contains(C)) Lines.Add(FString::Printf(TEXT("- wire %s"), *C));
	}
	for (const FString& O : OutsB)
	{
		if (!OutsA.Contains(O)) Lines.Add(FString::Printf(TEXT("+ out  %s"), *O));
	}
	for (const FString& O : OutsA)
	{
		if (!OutsB.Contains(O)) Lines.Add(FString::Printf(TEXT("- out  %s"), *O));
	}

	if (Lines.Num() == 0)
	{
		return TEXT("(no changes)");
	}
	return FString::Join(Lines, TEXT("\n"));
}

// ─── M2.5-2: Custom node factory ──────────────────────────────────

namespace BridgeMaterialImpl
{
	static ECustomMaterialOutputType ParseCustomOutputType(const FString& Str)
	{
		const FString S = Str.ToLower();
		if (S == TEXT("float1") || S.IsEmpty()) return CMOT_Float1;
		if (S == TEXT("float2")) return CMOT_Float2;
		if (S == TEXT("float3")) return CMOT_Float3;
		if (S == TEXT("float4")) return CMOT_Float4;
		if (S == TEXT("materialattributes") || S == TEXT("attrs")) return CMOT_MaterialAttributes;
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown custom output type '%s' — falling back to Float1"), *Str);
		return CMOT_Float1;
	}

	static FString FindBridgeSnippetsPath()
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealBridge"));
		if (!Plugin.IsValid()) return FString();
		// Must match the mapping in UnrealBridgeModule.cpp.
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"), TEXT("Private"), TEXT("BridgeSnippets.ush"));
	}
}

FBridgeAddExpressionResult UUnrealBridgeMaterialLibrary::AddCustomExpression(
	const FString& MaterialPath,
	int32 X, int32 Y,
	const TArray<FString>& InputNames,
	const FString& OutputType,
	const FString& Code,
	const TArray<FString>& IncludePaths,
	const FString& Description)
{
	using namespace BridgeMaterialImpl;

	FBridgeAddExpressionResult Result;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
	{
		Result.Error = FString::Printf(TEXT("could not load material '%s'"), *MaterialPath);
		return Result;
	}

	UMaterialExpression* Expr = UMaterialEditingLibrary::CreateMaterialExpression(
		Material, UMaterialExpressionCustom::StaticClass(), X, Y);
	UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
	if (!Custom)
	{
		Result.Error = TEXT("CreateMaterialExpression returned null for UMaterialExpressionCustom");
		return Result;
	}

	Custom->OutputType = ParseCustomOutputType(OutputType);
	Custom->Code = Code;
	Custom->Description = Description;
	Custom->IncludeFilePaths = IncludePaths;

	Custom->Inputs.Reset(InputNames.Num());
	for (const FString& Name : InputNames)
	{
		FCustomInput In;
		In.InputName = FName(*Name);
		Custom->Inputs.Add(In);
	}

	if (!Custom->MaterialExpressionGuid.IsValid())
	{
		Custom->MaterialExpressionGuid = FGuid::NewGuid();
	}

	Custom->PostEditChange();
	Material->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Guid = Custom->MaterialExpressionGuid;
	Result.ResolvedClass = Custom->GetClass()->GetName();
	return Result;
}

// ─── M2.5-4: snippet catalogue ────────────────────────────────────

namespace BridgeMaterialImpl
{
	struct FParsedSnippet
	{
		FString Name;
		FString Description;
		FString Signature;
		FString MinFeatureLevel;
		FString InstructionEstimate;
		int32 BodyStartLine = INDEX_NONE;
		int32 BodyEndLine = INDEX_NONE;  // line of the final closing brace
	};

	static TArray<FParsedSnippet> ParseBridgeSnippets(const TArray<FString>& Lines)
	{
		TArray<FParsedSnippet> Out;

		auto ExtractTag = [](const FString& L, const FString& Tag) -> FString
		{
			const FString Needle = FString::Printf(TEXT("// @%s"), *Tag);
			int32 Idx = L.Find(Needle);
			if (Idx == INDEX_NONE) return FString();
			FString Rest = L.Mid(Idx + Needle.Len());
			Rest.TrimStartAndEndInline();
			return Rest;
		};

		const int32 N = Lines.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const FString& L = Lines[i];
			int32 MarkerIdx = L.Find(TEXT("//@snippet "));
			if (MarkerIdx == INDEX_NONE)
			{
				continue;
			}

			FParsedSnippet S;
			S.Name = L.Mid(MarkerIdx + FString(TEXT("//@snippet ")).Len());
			S.Name.TrimStartAndEndInline();

			// Skip pseudo-placeholders from documentation examples: <name>, <...>, etc.
			if (S.Name.IsEmpty() || S.Name.StartsWith(TEXT("<")))
			{
				continue;
			}

			// Scan following comment-header lines for tag metadata.
			int32 j = i + 1;
			for (; j < N; ++j)
			{
				const FString& LL = Lines[j];
				const FString Trimmed = LL.TrimStart();
				if (!Trimmed.StartsWith(TEXT("//")))
				{
					break;
				}
				if (Trimmed.Contains(TEXT("@desc")))  S.Description = ExtractTag(LL, TEXT("desc"));
				else if (Trimmed.Contains(TEXT("@sig"))) S.Signature = ExtractTag(LL, TEXT("sig"));
				else if (Trimmed.Contains(TEXT("@fl")))  S.MinFeatureLevel = ExtractTag(LL, TEXT("fl"));
				else if (Trimmed.Contains(TEXT("@inst"))) S.InstructionEstimate = ExtractTag(LL, TEXT("inst"));
				// Handle multi-line @desc values — if the description line ends mid-sentence,
				// subsequent bare comment lines append to it with a space. Safe for a light-weight
				// parser: we only treat explicit @tag lines specially.
			}

			// Body: starts at j (first non-comment line, expect the function signature line),
			// ends at matching closing brace of the function.
			S.BodyStartLine = j;
			int32 BraceDepth = 0;
			bool bSawOpen = false;
			for (int32 k = j; k < N; ++k)
			{
				const FString& BodyLine = Lines[k];
				for (int32 c = 0; c < BodyLine.Len(); ++c)
				{
					const TCHAR Ch = BodyLine[c];
					if (Ch == TEXT('{')) { BraceDepth++; bSawOpen = true; }
					else if (Ch == TEXT('}')) { BraceDepth--; if (bSawOpen && BraceDepth == 0) { S.BodyEndLine = k; break; } }
				}
				if (S.BodyEndLine != INDEX_NONE) break;
			}

			Out.Add(MoveTemp(S));
			// Continue scan past the function body.
			if (S.BodyEndLine != INDEX_NONE)
			{
				i = S.BodyEndLine;
			}
		}
		return Out;
	}
}

TArray<FBridgeShaderSnippet> UUnrealBridgeMaterialLibrary::ListSharedSnippets()
{
	using namespace BridgeMaterialImpl;

	TArray<FBridgeShaderSnippet> Out;

	const FString SnippetsPath = FindBridgeSnippetsPath();
	if (SnippetsPath.IsEmpty())
	{
		return Out;
	}
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *SnippetsPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: could not read BridgeSnippets.ush at '%s'"), *SnippetsPath);
		return Out;
	}

	for (const FParsedSnippet& P : ParseBridgeSnippets(Lines))
	{
		FBridgeShaderSnippet S;
		S.Name = P.Name;
		S.Description = P.Description;
		S.Signature = P.Signature;
		S.MinFeatureLevel = P.MinFeatureLevel;
		S.InstructionEstimate = P.InstructionEstimate;
		Out.Add(MoveTemp(S));
	}
	return Out;
}

FBridgeShaderSnippet UUnrealBridgeMaterialLibrary::GetSharedSnippet(const FString& Name)
{
	using namespace BridgeMaterialImpl;

	FBridgeShaderSnippet Out;

	const FString SnippetsPath = FindBridgeSnippetsPath();
	if (SnippetsPath.IsEmpty())
	{
		return Out;
	}
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *SnippetsPath))
	{
		return Out;
	}

	for (const FParsedSnippet& P : ParseBridgeSnippets(Lines))
	{
		if (!P.Name.Equals(Name, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Out.Name = P.Name;
		Out.Description = P.Description;
		Out.Signature = P.Signature;
		Out.MinFeatureLevel = P.MinFeatureLevel;
		Out.InstructionEstimate = P.InstructionEstimate;

		// Assemble body from BodyStartLine..BodyEndLine inclusive.
		if (P.BodyStartLine != INDEX_NONE && P.BodyEndLine != INDEX_NONE)
		{
			TArray<FString> BodyLines;
			for (int32 k = P.BodyStartLine; k <= P.BodyEndLine && k < Lines.Num(); ++k)
			{
				BodyLines.Add(Lines[k]);
			}
			Out.Source = FString::Join(BodyLines, TEXT("\n"));
		}
		break;
	}
	return Out;
}

// ─── M6: MI parameter loop ────────────────────────────────────────

namespace BridgeMaterialImpl
{
	/**
	 * Apply one FBridgeMIParamSet to a UMaterialInstanceConstant. Returns true on success.
	 * Dispatches on Type — each handler parses Value with the format documented on the
	 * FBridgeMIParamSet struct.
	 */
	static bool ApplyMIParam(
		UMaterialInstanceConstant* MIC,
		const FBridgeMIParamSet& P,
		FString& OutError)
	{
		if (!MIC)
		{
			OutError = TEXT("null MI");
			return false;
		}

		const FName ParamName(*P.Name);
		const FString TypeLower = P.Type.ToLower();

		// NOTE: UE 5.7's UMaterialEditingLibrary::SetMaterialInstance*ParameterValue
		// functions have a stub `bool bResult = false` that is never updated — the return
		// value is effectively meaningless (always false). The setter *does* mutate the MI
		// via SetScalarParameterValueEditorOnly. We rely on that side effect and verify
		// parameter existence up-front via a GetScalarParameterValue probe.
		const FMaterialParameterInfo Info(ParamName);

		if (TypeLower == TEXT("scalar") || TypeLower.IsEmpty())
		{
			float Probe;
			if (!MIC->GetScalarParameterValue(Info, Probe))
			{
				OutError = FString::Printf(TEXT("scalar param '%s' not found on parent"), *P.Name);
				return false;
			}
			const float V = FCString::Atof(*P.Value);
			UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MIC, ParamName, V);
			return true;
		}
		if (TypeLower == TEXT("vector"))
		{
			FLinearColor Probe;
			if (!MIC->GetVectorParameterValue(Info, Probe))
			{
				OutError = FString::Printf(TEXT("vector param '%s' not found on parent"), *P.Name);
				return false;
			}
			FLinearColor V(FLinearColor::Black);
			if (!V.InitFromString(P.Value))
			{
				OutError = FString::Printf(TEXT("vector value '%s' not parseable — expected (R=,G=,B=,A=)"), *P.Value);
				return false;
			}
			UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MIC, ParamName, V);
			return true;
		}
		if (TypeLower == TEXT("texture"))
		{
			UTexture* ProbeTex = nullptr;
			if (!MIC->GetTextureParameterValue(Info, ProbeTex))
			{
				OutError = FString::Printf(TEXT("texture param '%s' not found on parent"), *P.Name);
				return false;
			}
			UTexture* Tex = nullptr;
			if (!P.Value.IsEmpty() && P.Value.ToLower() != TEXT("none"))
			{
				Tex = LoadObject<UTexture>(nullptr, *P.Value);
				if (!Tex)
				{
					OutError = FString::Printf(TEXT("texture '%s' failed to load"), *P.Value);
					return false;
				}
			}
			UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MIC, ParamName, Tex);
			return true;
		}
		if (TypeLower == TEXT("staticswitch") || TypeLower == TEXT("switch") || TypeLower == TEXT("bool"))
		{
			bool ProbeBool;
			FGuid ProbeGuid;
			if (!MIC->GetStaticSwitchParameterValue(Info, ProbeBool, ProbeGuid))
			{
				OutError = FString::Printf(TEXT("static-switch param '%s' not found on parent"), *P.Name);
				return false;
			}
			const FString Lower = P.Value.ToLower();
			const bool V = (Lower == TEXT("true") || Lower == TEXT("1") || Lower == TEXT("yes"));
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MIC, ParamName, V);
			return true;
		}

		OutError = FString::Printf(TEXT("unknown param type '%s' — expected Scalar/Vector/Texture/StaticSwitch"), *P.Type);
		return false;
	}
}

FBridgeMIParamResult UUnrealBridgeMaterialLibrary::SetMIParams(
	const FString& MaterialInstancePath,
	const TArray<FBridgeMIParamSet>& Params)
{
	using namespace BridgeMaterialImpl;

	FBridgeMIParamResult Result;

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *MaterialInstancePath);
	if (!MIC)
	{
		Result.Skipped.Add(FString::Printf(TEXT("could not load MI '%s'"), *MaterialInstancePath));
		return Result;
	}

	for (const FBridgeMIParamSet& P : Params)
	{
		FString Err;
		if (ApplyMIParam(MIC, P, Err))
		{
			++Result.Applied;
		}
		else
		{
			Result.Skipped.Add(FString::Printf(TEXT("%s = %s: %s"), *P.Name, *P.Value, *Err));
		}
	}

	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	Result.bSuccess = Result.Skipped.Num() == 0;
	return Result;
}

// ─── M6-2: set + preview atomic ───────────────────────────────────

bool UUnrealBridgeMaterialLibrary::SetMIAndPreview(
	const FString& MaterialInstancePath,
	const TArray<FBridgeMIParamSet>& Params,
	const FString& Mesh,
	const FString& Lighting,
	int32 Resolution,
	float CameraYawDeg,
	float CameraPitchDeg,
	float CameraDistance,
	const FString& OutPngPath)
{
	FBridgeMIParamResult PR = SetMIParams(MaterialInstancePath, Params);
	if (!PR.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SetMIAndPreview skipped %d params — rendering anyway"), PR.Skipped.Num());
	}

	return BridgeMaterialImpl::RenderMaterialPreview(
		MaterialInstancePath, Mesh, Lighting, Resolution,
		CameraYawDeg, CameraPitchDeg, CameraDistance,
		OutPngPath, /*bShaderComplexity=*/ false);
}

// ─── M6-3: sweep one param over a list of values ──────────────────

namespace BridgeMaterialImpl
{
	/**
	 * Capture a material preview into an FColor buffer (no PNG save), reusing the M1-6
	 * pipeline. Returns true on success with Pixels populated.
	 */
	static bool RenderMaterialPreviewToFColor(
		const FString& MaterialPath,
		const FString& MeshPreset,
		const FString& LightingPreset,
		int32 Res,
		float CameraYawDeg,
		float CameraPitchDeg,
		float CameraDistance,
		TArray<FColor>& OutPixels)
	{
		// Cheap path: render to a scratch PNG then decode. Simpler than duplicating the full
		// capture pipeline here. Tradeoff: one PNG encode per cell. For sweep sizes under 20
		// this is under 1s total encode time — negligible vs. shader compile/render.
		const FString ScratchPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("Bridge"),
			FString::Printf(TEXT("SweepCell_%s.png"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!RenderMaterialPreview(
			MaterialPath, MeshPreset, LightingPreset, Res,
			CameraYawDeg, CameraPitchDeg, CameraDistance,
			ScratchPath, /*bShaderComplexity=*/ false))
		{
			return false;
		}
		TArray<uint8> FileBytes;
		if (!FFileHelper::LoadFileToArray(FileBytes, *ScratchPath))
		{
			return false;
		}
		FImage Img;
		if (!FImageUtils::DecompressImage(FileBytes.GetData(), FileBytes.Num(), Img))
		{
			return false;
		}
		// Copy to FColor (the file was saved BGRA8 sRGB; decode gives us that back).
		OutPixels.SetNum(Res * Res);
		if (Img.Format == ERawImageFormat::BGRA8)
		{
			FMemory::Memcpy(OutPixels.GetData(), Img.RawData.GetData(), Res * Res * sizeof(FColor));
		}
		else
		{
			// Coerce to BGRA8 if it came out in another format.
			FImage BGRA;
			Img.CopyTo(BGRA, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			FMemory::Memcpy(OutPixels.GetData(), BGRA.RawData.GetData(), Res * Res * sizeof(FColor));
		}
		IFileManager::Get().Delete(*ScratchPath);
		return true;
	}
}

TArray<FString> UUnrealBridgeMaterialLibrary::SweepMIParams(
	const FString& MaterialInstancePath,
	const FString& ParamName,
	const TArray<FString>& Values,
	const FString& Mesh,
	const FString& Lighting,
	int32 Resolution,
	float CameraYawDeg,
	float CameraPitchDeg,
	float CameraDistance,
	int32 GridCols,
	const FString& OutGridPath)
{
	using namespace BridgeMaterialImpl;

	TArray<FString> Out;

	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *MaterialInstancePath);
	if (!MIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SweepMIParams could not load '%s'"), *MaterialInstancePath);
		return Out;
	}

	const int32 N = Values.Num();
	if (N == 0) return Out;

	const FName ParamNameN(*ParamName);
	const int32 Res = FMath::Clamp(Resolution > 0 ? Resolution : 256, 32, 2048);

	// Snapshot current override (so we can restore after sweep).
	// Try scalar first, then vector — that tells us the param type too.
	float OriginalScalar = 0.f;
	FLinearColor OriginalVector(FLinearColor::Black);
	bool bScalarParam = false;
	bool bVectorParam = false;
	const FMaterialParameterInfo Info(*ParamName);
	if (MIC->GetScalarParameterValue(Info, OriginalScalar))
	{
		bScalarParam = true;
	}
	else if (MIC->GetVectorParameterValue(Info, OriginalVector))
	{
		bVectorParam = true;
	}
	if (!bScalarParam && !bVectorParam)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SweepMIParams: param '%s' is not scalar or vector on '%s'"),
			*ParamName, *MaterialInstancePath);
		return Out;
	}

	// Render each value into an FColor buffer.
	TArray<TArray<FColor>> Cells;
	Cells.Reserve(N);
	TArray<FString> CellPaths;
	CellPaths.Reserve(N);

	FString GridBasePath = OutGridPath;
	if (FPaths::IsRelative(GridBasePath))
	{
		GridBasePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GridBasePath);
	}
	const FString GridBaseNoExt = FPaths::Combine(
		FPaths::GetPath(GridBasePath),
		FPaths::GetBaseFilename(GridBasePath));

	for (int32 i = 0; i < N; ++i)
	{
		const FString& V = Values[i];

		if (bScalarParam)
		{
			const float Val = FCString::Atof(*V);
			UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MIC, ParamNameN, Val);
		}
		else
		{
			FLinearColor Val(FLinearColor::Black);
			if (!Val.InitFromString(V))
			{
				UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SweepMIParams: value '%s' at index %d not parseable"), *V, i);
				continue;
			}
			UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MIC, ParamNameN, Val);
		}
		MIC->PostEditChange();

		// Save the cell PNG alongside, using the param value as suffix.
		FString SanitizedValue = V;
		SanitizedValue.ReplaceInline(TEXT("("), TEXT(""));
		SanitizedValue.ReplaceInline(TEXT(")"), TEXT(""));
		SanitizedValue.ReplaceInline(TEXT(","), TEXT("_"));
		SanitizedValue.ReplaceInline(TEXT("="), TEXT(""));
		SanitizedValue.ReplaceInline(TEXT(" "), TEXT(""));
		const FString CellPath = FString::Printf(TEXT("%s_%02d_%s.png"), *GridBaseNoExt, i, *SanitizedValue);

		TArray<FColor> Pixels;
		if (!RenderMaterialPreviewToFColor(
			MaterialInstancePath, Mesh, Lighting, Res,
			CameraYawDeg, CameraPitchDeg, CameraDistance, Pixels))
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SweepMIParams: render failed at index %d"), i);
			continue;
		}

		// Also save the individual cell PNG (user-visible).
		FImage CellImg;
		CellImg.Init(Res, Res, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		FMemory::Memcpy(CellImg.RawData.GetData(), Pixels.GetData(), Res * Res * sizeof(FColor));
		FImageUtils::SaveImageByExtension(*CellPath, CellImg, 0);

		Cells.Add(MoveTemp(Pixels));
		CellPaths.Add(CellPath);
	}

	// Restore original value.
	if (bScalarParam)
	{
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MIC, ParamNameN, OriginalScalar);
	}
	else
	{
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MIC, ParamNameN, OriginalVector);
	}
	MIC->PostEditChange();
	MIC->MarkPackageDirty();

	if (Cells.Num() == 0)
	{
		return Out;
	}

	// Compose grid image.
	const int32 Cols = GridCols > 0
		? GridCols
		: FMath::Max(1, (int32)FMath::CeilToInt(FMath::Sqrt((float)Cells.Num())));
	const int32 Rows = FMath::DivideAndRoundUp(Cells.Num(), Cols);
	const int32 GridW = Cols * Res;
	const int32 GridH = Rows * Res;

	FImage Grid;
	Grid.Init(GridW, GridH, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	FColor* GridDst = reinterpret_cast<FColor*>(Grid.RawData.GetData());
	// Fill background with a soft gray so empty cells in a partial last row read as intentional.
	for (int32 i = 0; i < GridW * GridH; ++i)
	{
		GridDst[i] = FColor(26, 26, 30, 255);
	}
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		const int32 Col = i % Cols;
		const int32 Row = i / Cols;
		const int32 OffX = Col * Res;
		const int32 OffY = Row * Res;
		for (int32 y = 0; y < Res; ++y)
		{
			FMemory::Memcpy(
				GridDst + (OffY + y) * GridW + OffX,
				Cells[i].GetData() + y * Res,
				Res * sizeof(FColor));
		}
	}

	if (FImageUtils::SaveImageByExtension(*GridBasePath, Grid, 0))
	{
		Out.Add(GridBasePath);
	}
	Out.Append(CellPaths);
	return Out;
}

// ─── M6-4: write to MaterialParameterCollection ───────────────────

FBridgeMIParamResult UUnrealBridgeMaterialLibrary::SetMaterialParameterCollection(
	const FString& CollectionPath,
	const TArray<FBridgeMIParamSet>& Params)
{
	FBridgeMIParamResult Result;

	UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, *CollectionPath);
	if (!MPC)
	{
		Result.Skipped.Add(FString::Printf(TEXT("could not load MPC '%s'"), *CollectionPath));
		return Result;
	}

	for (const FBridgeMIParamSet& P : Params)
	{
		const FName ParamName(*P.Name);
		const FString TypeLower = P.Type.ToLower();
		bool bDone = false;

		if (TypeLower == TEXT("scalar") || TypeLower.IsEmpty())
		{
			for (FCollectionScalarParameter& S : MPC->ScalarParameters)
			{
				if (S.ParameterName == ParamName)
				{
					S.DefaultValue = FCString::Atof(*P.Value);
					bDone = true;
					break;
				}
			}
		}
		else if (TypeLower == TEXT("vector"))
		{
			for (FCollectionVectorParameter& V : MPC->VectorParameters)
			{
				if (V.ParameterName == ParamName)
				{
					FLinearColor Val(FLinearColor::Black);
					if (!Val.InitFromString(P.Value))
					{
						Result.Skipped.Add(FString::Printf(TEXT("%s = %s: vector unparseable"), *P.Name, *P.Value));
						bDone = true;
						break;
					}
					V.DefaultValue = Val;
					bDone = true;
					break;
				}
			}
		}
		else
		{
			Result.Skipped.Add(FString::Printf(TEXT("%s: MPC only supports Scalar/Vector, got '%s'"), *P.Name, *P.Type));
			continue;
		}

		if (bDone)
		{
			++Result.Applied;
		}
		else
		{
			Result.Skipped.Add(FString::Printf(TEXT("%s: not found on MPC"), *P.Name));
		}
	}

	MPC->PostEditChange();
	MPC->MarkPackageDirty();

	Result.bSuccess = Result.Skipped.Num() == 0;
	return Result;
}

// ─── M6-5: diff two MI parameter sets ─────────────────────────────

FString UUnrealBridgeMaterialLibrary::DiffMIParams(
	const FString& PathA,
	const FString& PathB)
{
	UMaterialInstance* A = LoadObject<UMaterialInstance>(nullptr, *PathA);
	UMaterialInstance* B = LoadObject<UMaterialInstance>(nullptr, *PathB);
	if (!A) return FString::Printf(TEXT("diff error: could not load A '%s'"), *PathA);
	if (!B) return FString::Printf(TEXT("diff error: could not load B '%s'"), *PathB);

	struct FEntry { FString Type; FString Value; };

	auto Collect = [](UMaterialInstance* MI) -> TMap<FString, FEntry>
	{
		TMap<FString, FEntry> Out;
		for (const FScalarParameterValue& P : MI->ScalarParameterValues)
		{
			Out.Add(P.ParameterInfo.Name.ToString(), {TEXT("Scalar"), FString::SanitizeFloat(P.ParameterValue)});
		}
		for (const FVectorParameterValue& P : MI->VectorParameterValues)
		{
			Out.Add(P.ParameterInfo.Name.ToString(), {TEXT("Vector"),
				FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"),
					P.ParameterValue.R, P.ParameterValue.G, P.ParameterValue.B, P.ParameterValue.A)});
		}
		for (const FTextureParameterValue& P : MI->TextureParameterValues)
		{
			Out.Add(P.ParameterInfo.Name.ToString(), {TEXT("Texture"),
				P.ParameterValue ? P.ParameterValue->GetPathName() : TEXT("None")});
		}
		return Out;
	};

	TMap<FString, FEntry> MapA = Collect(A);
	TMap<FString, FEntry> MapB = Collect(B);

	TArray<FString> Lines;
	// Added in B
	for (const TPair<FString, FEntry>& KV : MapB)
	{
		if (!MapA.Contains(KV.Key))
		{
			Lines.Add(FString::Printf(TEXT("+ %s %s = %s"), *KV.Value.Type, *KV.Key, *KV.Value.Value));
		}
	}
	// Removed / changed
	for (const TPair<FString, FEntry>& KV : MapA)
	{
		if (!MapB.Contains(KV.Key))
		{
			Lines.Add(FString::Printf(TEXT("- %s %s = %s"), *KV.Value.Type, *KV.Key, *KV.Value.Value));
		}
		else if (MapB[KV.Key].Value != KV.Value.Value)
		{
			Lines.Add(FString::Printf(TEXT("~ %s %s: %s -> %s"),
				*KV.Value.Type, *KV.Key, *KV.Value.Value, *MapB[KV.Key].Value));
		}
	}

	Lines.Sort();
	if (Lines.Num() == 0)
	{
		return TEXT("(no override differences)");
	}
	return FString::Join(Lines, TEXT("\n"));
}

bool UUnrealBridgeMaterialLibrary::CompileMaterial(
	const FString& MaterialPath,
	bool bSaveAfter)
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material) return false;

	UMaterialEditingLibrary::RecompileMaterial(Material);
	FAssetCompilingManager::Get().FinishAllCompilation();

	if (bSaveAfter)
	{
		TArray<UPackage*> Pkgs;
		Pkgs.Add(Material->GetPackage());
		UEditorLoadingAndSavingUtils::SavePackages(Pkgs, /*bOnlyDirty=*/ false);
	}
	return true;
}

TArray<FString> UUnrealBridgeMaterialLibrary::GetMaterialCompileErrors(
	const FString& MaterialPath,
	const FString& FeatureLevelStr,
	const FString& QualityStr)
{
	using namespace BridgeMaterialImpl;

	TArray<FString> Errors;

	UMaterialInterface* MatInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!MatInterface)
	{
		Errors.Add(FString::Printf(TEXT("UnrealBridge: could not load material '%s'"), *MaterialPath));
		return Errors;
	}

	const ERHIFeatureLevel::Type FeatureLevel = ParseFeatureLevel(FeatureLevelStr);
	const EMaterialQualityLevel::Type Quality = ParseQuality(QualityStr);

	const FMaterialResource* Resource = ResolveMaterialResource(MatInterface, FeatureLevel, Quality);
	if (Resource)
	{
		Errors = Resource->GetCompileErrors();
	}
	return Errors;
}


// ─── M4: post-process material ops ──────────────────────────────

namespace BridgeMaterialImpl
{
	static bool ParseBlendableLocation(const FString& S, EBlendableLocation& Out)
	{
		// Canonical UE 5.7 enum tail names — accept bare names too for convenience.
		// UE 5.7 EBlendableLocation values: AfterTonemapping / BeforeBloom /
		// ReplacingTonemapper / TranslucencyAfterDOF / SSRInput. (There's no
		// "BeforeTonemapping" in UE 5.x — use BeforeBloom for pre-tonemap.)
		static const TMap<FString, EBlendableLocation> Map = {
			{ TEXT("BL_SceneColorAfterTonemapping"),  BL_SceneColorAfterTonemapping },
			{ TEXT("AfterTonemapping"),               BL_SceneColorAfterTonemapping },
			{ TEXT("BL_SceneColorBeforeBloom"),       BL_SceneColorBeforeBloom },
			{ TEXT("BeforeBloom"),                    BL_SceneColorBeforeBloom },
			{ TEXT("BeforeTonemapping"),              BL_SceneColorBeforeBloom },  // alias
			{ TEXT("BL_ReplacingTonemapper"),         BL_ReplacingTonemapper },
			{ TEXT("ReplacingTonemapper"),            BL_ReplacingTonemapper },
			{ TEXT("BL_SSRInput"),                    BL_SSRInput },
			{ TEXT("SSRInput"),                       BL_SSRInput },
			{ TEXT("BL_TranslucencyAfterDOF"),        BL_TranslucencyAfterDOF },
			{ TEXT("TranslucencyAfterDOF"),           BL_TranslucencyAfterDOF },
		};
		if (const EBlendableLocation* Found = Map.Find(S))
		{
			Out = *Found;
			return true;
		}
		return false;
	}

	static UWorld* GetEditorWorld()
	{
		if (!GEditor) return nullptr;
		return GEditor->GetEditorWorldContext().World();
	}

	/** Find a PPV by actor label. If Label is empty, return the first unbound one
	 *  (creating a new unbound PPV if none exists when bCreateIfMissing). */
	static APostProcessVolume* ResolveVolume(const FString& Label, bool bCreateIfMissing)
	{
		UWorld* World = GetEditorWorld();
		if (!World) return nullptr;

		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			APostProcessVolume* V = *It;
			if (!V) continue;
			if (Label.IsEmpty())
			{
				if (V->bUnbound) return V;
			}
			else if (V->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase)
				|| V->GetName().Equals(Label, ESearchCase::IgnoreCase))
			{
				return V;
			}
		}

		if (bCreateIfMissing && Label.IsEmpty())
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transactional;
			APostProcessVolume* NewVol = World->SpawnActor<APostProcessVolume>(
				APostProcessVolume::StaticClass(), FTransform::Identity, Params);
			if (NewVol)
			{
				NewVol->bUnbound = true;
				NewVol->SetActorLabel(TEXT("PPV_Bridge"));
				NewVol->MarkPackageDirty();
			}
			return NewVol;
		}
		return nullptr;
	}

	static FString BlendableToPath(const FWeightedBlendable& WB)
	{
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(WB.Object))
		{
			return Mat->GetPathName();
		}
		return WB.Object ? WB.Object->GetPathName() : FString();
	}
}  // namespace BridgeMaterialImpl


FBridgeCreateAssetResult UUnrealBridgeMaterialLibrary::CreatePostProcessMaterial(
	const FString& Path,
	const FString& BlendableLocation,
	bool bOutputAlpha)
{
	using namespace BridgeMaterialImpl;

	FBridgeCreateAssetResult Result;

	EBlendableLocation BL = BL_SceneColorAfterTonemapping;
	if (!BlendableLocation.IsEmpty() && !ParseBlendableLocation(BlendableLocation, BL))
	{
		Result.Error = FString::Printf(TEXT("unknown blendable_location '%s'"), *BlendableLocation);
		return Result;
	}

	FString PackagePath, AssetName;
	if (!SplitAssetPath(Path, PackagePath, AssetName))
	{
		Result.Error = FString::Printf(TEXT("invalid path '%s' — expected /Game/Folder/AssetName"), *Path);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);
	UMaterial* Material = Cast<UMaterial>(NewAsset);
	if (!Material)
	{
		Result.Error = FString::Printf(TEXT("AssetTools.CreateAsset returned null for %s/%s"),
			*PackagePath, *AssetName);
		return Result;
	}

	Material->MaterialDomain = MD_PostProcess;
	Material->BlendableLocation = BL;
	Material->BlendableOutputAlpha = bOutputAlpha;
	Material->BlendablePriority = 0;

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Path = Material->GetPathName();
	return Result;
}


TArray<FBridgePostProcessVolumeInfo> UUnrealBridgeMaterialLibrary::GetPostProcessState()
{
	using namespace BridgeMaterialImpl;

	TArray<FBridgePostProcessVolumeInfo> Out;
	UWorld* World = GetEditorWorld();
	if (!World) return Out;

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		APostProcessVolume* V = *It;
		if (!V) continue;

		FBridgePostProcessVolumeInfo Info;
		Info.ActorLabel = V->GetActorLabel();
		Info.ActorName = V->GetName();
		Info.bEnabled = V->bEnabled;
		Info.bUnbound = V->bUnbound;
		Info.BlendWeight = V->BlendWeight;
		Info.Priority = V->Priority;

		for (const FWeightedBlendable& WB : V->Settings.WeightedBlendables.Array)
		{
			FBridgePostProcessBlendable B;
			B.Weight = WB.Weight;
			B.MaterialPath = BlendableToPath(WB);
			Info.Blendables.Add(B);
		}
		Out.Add(Info);
	}
	return Out;
}


bool UUnrealBridgeMaterialLibrary::ApplyPostProcessMaterial(
	const FString& VolumeActor,
	const FString& MaterialPath,
	float Weight)
{
	using namespace BridgeMaterialImpl;

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyPostProcessMaterial: could not load material '%s'"), *MaterialPath);
		return false;
	}
	if (Mat->GetMaterial() && Mat->GetMaterial()->MaterialDomain != MD_PostProcess)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyPostProcessMaterial: '%s' is not a PostProcess material"),
			*MaterialPath);
		return false;
	}

	APostProcessVolume* Volume = ResolveVolume(VolumeActor, /*bCreateIfMissing=*/ VolumeActor.IsEmpty());
	if (!Volume)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyPostProcessMaterial: no PPV named '%s' (and not creating one)"),
			*VolumeActor);
		return false;
	}

	Volume->Modify();

	// Update if already present; otherwise append. Identity comparison — the
	// same asset loaded twice returns the same UObject*, so string path
	// normalization isn't our concern here.
	bool bUpdated = false;
	for (FWeightedBlendable& WB : Volume->Settings.WeightedBlendables.Array)
	{
		if (WB.Object == Mat)
		{
			WB.Weight = Weight;
			bUpdated = true;
			break;
		}
	}
	if (!bUpdated)
	{
		FWeightedBlendable NewWB;
		NewWB.Object = Mat;
		NewWB.Weight = Weight;
		Volume->Settings.WeightedBlendables.Array.Add(NewWB);
	}

	Volume->PostEditChange();
	Volume->MarkPackageDirty();
	return true;
}


bool UUnrealBridgeMaterialLibrary::RemovePostProcessMaterial(
	const FString& VolumeActor,
	const FString& MaterialPath)
{
	using namespace BridgeMaterialImpl;

	APostProcessVolume* Volume = ResolveVolume(VolumeActor, /*bCreateIfMissing=*/ false);
	if (!Volume) return false;

	// Resolve via LoadObject so "/Game/Foo/M" and "/Game/Foo/M.M" both match —
	// the WB.Object UObject identity is authoritative, not the string form.
	UMaterialInterface* Target = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Target) return false;

	Volume->Modify();

	const int32 Removed = Volume->Settings.WeightedBlendables.Array.RemoveAll(
		[Target](const FWeightedBlendable& WB)
		{
			return WB.Object == Target;
		});

	Volume->PostEditChange();
	Volume->MarkPackageDirty();
	return Removed > 0;
}


// ─── M5: analyze_material — lint rules ──────────────────────────────

namespace BridgeMaterialImpl
{
	using EProp = EMaterialProperty;

	static void EmitFinding(TArray<FBridgeMaterialFinding>& Out,
		const TCHAR* RuleId, const TCHAR* Severity,
		FString Message,
		const FGuid& Guid = FGuid(),
		const FString& ExprClass = FString(),
		const FString& Detail = FString())
	{
		FBridgeMaterialFinding F;
		F.RuleId = RuleId;
		F.Severity = Severity;
		F.Message = MoveTemp(Message);
		F.ExpressionGuid = Guid;
		F.ExpressionClass = ExprClass;
		F.Detail = Detail;
		Out.Add(MoveTemp(F));
	}

	/** Collect the set of expression guids reachable from any main material output. */
	static void CollectReachableFromMainOutputs(UMaterial* Material, TSet<FGuid>& OutReachable)
	{
		if (!Material) return;

		TArray<UMaterialExpression*> Stack;

		for (int32 i = 0; i < MP_MAX; ++i)
		{
			const EProp Prop = (EProp)i;
			FExpressionInput* In = Material->GetExpressionInputForProperty(Prop);
			if (In && In->Expression)
			{
				Stack.Add(In->Expression);
			}
		}

		while (Stack.Num() > 0)
		{
			UMaterialExpression* Expr = Stack.Pop(EAllowShrinking::No);
			if (!Expr) continue;
			const FGuid& Guid = Expr->MaterialExpressionGuid;
			if (!Guid.IsValid() || OutReachable.Contains(Guid)) continue;
			OutReachable.Add(Guid);
			for (FExpressionInputIterator It{Expr}; It; ++It)
			{
				if (It->Expression)
				{
					Stack.Add(It->Expression);
				}
			}
		}
	}

	/** Unique key for a TextureSample node — texture asset + UV source guid (or "default"). */
	static FString TextureSampleKey(UMaterialExpressionTextureSample* TS)
	{
		if (!TS) return FString();
		const UTexture* Tex = TS->Texture;
		const FString TexPath = Tex ? Tex->GetPathName() : TEXT("<null>");

		FString UvKey = TEXT("default_uv0");
		if (TS->Coordinates.Expression)
		{
			UvKey = FString::Printf(TEXT("expr=%s:%d"),
				*TS->Coordinates.Expression->MaterialExpressionGuid.ToString(EGuidFormats::Digits),
				TS->Coordinates.OutputIndex);
		}
		return FString::Printf(TEXT("%s|%s"), *TexPath, *UvKey);
	}

	static FString SamplerSourceToString(ESamplerSourceMode Mode)
	{
		switch (Mode)
		{
		case ESamplerSourceMode::SSM_FromTextureAsset:           return TEXT("SSM_FromTextureAsset");
		case ESamplerSourceMode::SSM_Wrap_WorldGroupSettings:    return TEXT("SSM_Wrap_WorldGroupSettings");
		case ESamplerSourceMode::SSM_Clamp_WorldGroupSettings:   return TEXT("SSM_Clamp_WorldGroupSettings");
		default:                                                  return TEXT("Unknown");
		}
	}

	static const TCHAR* MaterialPropertyName(EProp Prop)
	{
		switch (Prop)
		{
		case MP_BaseColor:            return TEXT("BaseColor");
		case MP_Metallic:             return TEXT("Metallic");
		case MP_Specular:             return TEXT("Specular");
		case MP_Roughness:            return TEXT("Roughness");
		case MP_Anisotropy:           return TEXT("Anisotropy");
		case MP_EmissiveColor:        return TEXT("EmissiveColor");
		case MP_Opacity:              return TEXT("Opacity");
		case MP_OpacityMask:          return TEXT("OpacityMask");
		case MP_Normal:               return TEXT("Normal");
		case MP_Tangent:              return TEXT("Tangent");
		case MP_WorldPositionOffset:  return TEXT("WorldPositionOffset");
		case MP_SubsurfaceColor:      return TEXT("SubsurfaceColor");
		case MP_CustomData0:          return TEXT("CustomData0");
		case MP_CustomData1:          return TEXT("CustomData1");
		case MP_AmbientOcclusion:     return TEXT("AmbientOcclusion");
		case MP_Refraction:           return TEXT("Refraction");
		case MP_PixelDepthOffset:     return TEXT("PixelDepthOffset");
		case MP_ShadingModel:         return TEXT("ShadingModel");
		default:                       return TEXT("Unknown");
		}
	}

	static bool IsPropertyWired(UMaterial* Material, EProp Prop)
	{
		if (!Material) return false;
		FExpressionInput* In = Material->GetExpressionInputForProperty(Prop);
		return In && In->Expression != nullptr;
	}
}  // namespace BridgeMaterialImpl


FBridgeMaterialAnalysis UUnrealBridgeMaterialLibrary::AnalyzeMaterial(
	const FString& MaterialPath,
	int32 InstructionBudget,
	int32 SamplerBudget)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialAnalysis Out;
	Out.InstructionBudget = FMath::Max(0, InstructionBudget);
	Out.SamplerBudget = FMath::Max(0, SamplerBudget);

	UMaterialInterface* MatInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!MatInterface)
	{
		return Out;
	}
	UMaterial* Material = MatInterface->GetMaterial();
	if (!Material)
	{
		return Out;
	}

	Out.bFound = true;
	Out.Path = MatInterface->GetPathName();
	Out.MaterialDomain = DomainToString(Material->MaterialDomain);

	// Shading models + compile errors + stats (M1-1/M1-3/M1-4 reuse).
	FMaterialShadingModelField Models = MatInterface->GetShadingModels();
	for (int32 i = 0; i < MSM_NUM; ++i)
	{
		const EMaterialShadingModel SM = (EMaterialShadingModel)i;
		if (Models.HasShadingModel(SM))
		{
			Out.ShadingModels.Add(ShadingModelToString(SM));
		}
	}

	// Collect stats (best effort — may report 0 if shader map still compiling).
	{
		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		const EMaterialQualityLevel::Type Quality = EMaterialQualityLevel::High;

		const FMaterialResource* Resource = ResolveMaterialResource(MatInterface, FeatureLevel, Quality);
		if (Resource)
		{
			Out.CompileErrors = Resource->GetCompileErrors();

			// Walk representative shader variants via the same helper M1-3 uses;
			// empty array = shader map still compiling.
			TArray<FBridgeMaterialShaderStat> Stats;
			CollectShaderInstructionStats(Resource, Stats);
			Out.bShaderStatsReady = Stats.Num() > 0;
			for (const FBridgeMaterialShaderStat& Stat : Stats)
			{
				Out.MaxInstructions = FMath::Max(Out.MaxInstructions, Stat.InstructionCount);
			}
		}
	}

	// Count distinct texture parameters (upper bound on sampler slots).
	{
		TArray<FMaterialParameterInfo> ParamInfos;
		TArray<FGuid> ParamGuids;
		MatInterface->GetAllTextureParameterInfo(ParamInfos, ParamGuids);
		Out.SamplerCount = ParamInfos.Num();
	}

	// Walk the graph — collect non-comment expressions for rule scans.
	TArray<UMaterialExpression*> Expressions;
	Expressions.Reserve(Material->GetExpressions().Num());
	for (const TObjectPtr<UMaterialExpression>& ExprPtr : Material->GetExpressions())
	{
		UMaterialExpression* Expr = ExprPtr.Get();
		if (Expr && !Expr->IsA<UMaterialExpressionComment>())
		{
			Expressions.Add(Expr);
		}
	}
	Out.ExpressionCount = Expressions.Num();

	// ── Rule M5-2: budget check ─────────────────────────────────────
	if (Out.InstructionBudget > 0 && Out.bShaderStatsReady && Out.MaxInstructions > Out.InstructionBudget)
	{
		EmitFinding(Out.Findings, TEXT("M5-2"), TEXT("warning"),
			FString::Printf(TEXT("Instruction count %d exceeds budget %d"),
				Out.MaxInstructions, Out.InstructionBudget));
	}
	if (Out.SamplerBudget > 0 && Out.SamplerCount > Out.SamplerBudget)
	{
		EmitFinding(Out.Findings, TEXT("M5-2"), TEXT("warning"),
			FString::Printf(TEXT("Sampler count %d exceeds budget %d"),
				Out.SamplerCount, Out.SamplerBudget));
	}

	// ── Rule M5-3: unreachable expressions (back-BFS from main outputs) ─────
	{
		TSet<FGuid> Reachable;
		CollectReachableFromMainOutputs(Material, Reachable);

		for (UMaterialExpression* Expr : Expressions)
		{
			if (!Expr->MaterialExpressionGuid.IsValid()) continue;
			// Ignore function-input / -output (never used in top-level materials
			// but cheap to guard).
			if (Expr->IsA<UMaterialExpressionFunctionInput>()) continue;
			if (Expr->IsA<UMaterialExpressionFunctionOutput>()) continue;

			if (!Reachable.Contains(Expr->MaterialExpressionGuid))
			{
				EmitFinding(Out.Findings, TEXT("M5-3"), TEXT("info"),
					FString::Printf(TEXT("Unused expression %s has no path to a main output"),
						*Expr->GetClass()->GetName()),
					Expr->MaterialExpressionGuid,
					Expr->GetClass()->GetName());
			}
		}
	}

	// ── Rule M5-4: duplicate TextureSample nodes ─────────────────────
	// Only flag plain `UMaterialExpressionTextureSample` — a
	// `UMaterialExpressionTextureSampleParameter*`'s default texture is MI-
	// overridable, so sibling parameters that happen to share a default
	// WhiteSquareTexture aren't actually redundant at runtime.
	{
		TMap<FString, TArray<UMaterialExpression*>> Groups;
		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr);
			if (!TS) continue;
			if (Expr->IsA<UMaterialExpressionTextureSampleParameter>()) continue;
			const FString Key = TextureSampleKey(TS);
			if (Key.IsEmpty()) continue;
			Groups.FindOrAdd(Key).Add(Expr);
		}
		for (const auto& Pair : Groups)
		{
			if (Pair.Value.Num() < 2) continue;
			for (UMaterialExpression* Dup : Pair.Value)
			{
				EmitFinding(Out.Findings, TEXT("M5-4"), TEXT("warning"),
					FString::Printf(TEXT("Duplicate texture lookup — %d samples share the same texture + UV"),
						Pair.Value.Num()),
					Dup->MaterialExpressionGuid,
					Dup->GetClass()->GetName(),
					Pair.Key);
			}
		}
	}

	// ── Rule M5-5: inconsistent SamplerSource across sibling samples ─────
	// SamplerSource lives on UMaterialExpressionTextureSample (not TextureBase)
	// — the base class exposes Texture + SamplerType only.
	{
		int32 FromAsset = 0;
		int32 Shared = 0;
		TArray<UMaterialExpression*> FromAssetNodes;
		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr);
			if (!TS) continue;
			if (TS->SamplerSource == SSM_FromTextureAsset)
			{
				++FromAsset;
				FromAssetNodes.Add(Expr);
			}
			else
			{
				++Shared;
			}
		}
		// Mixed: some use shared, some don't — each SSM_FromTextureAsset eats a slot.
		if (FromAsset > 0 && Shared > 0)
		{
			for (UMaterialExpression* Node : FromAssetNodes)
			{
				EmitFinding(Out.Findings, TEXT("M5-5"), TEXT("info"),
					TEXT("Mixing SSM_FromTextureAsset with SSM_Wrap_WorldGroupSettings — ")
					TEXT("switch this one to the shared wrap sampler to free a slot"),
					Node->MaterialExpressionGuid,
					Node->GetClass()->GetName(),
					TEXT("SamplerSource=SSM_FromTextureAsset"));
			}
		}
		// All from-asset + dense (>=4 textures): suggest sharing.
		else if (FromAsset >= 4 && Shared == 0)
		{
			EmitFinding(Out.Findings, TEXT("M5-5"), TEXT("info"),
				FString::Printf(TEXT("%d texture samples all use SSM_FromTextureAsset — ")
					TEXT("switch tileable textures to SSM_Wrap_WorldGroupSettings to ")
					TEXT("collapse them onto one shared sampler."), FromAsset));
		}
	}

	// ── Rule M5-8: shading-model ↔ main-output wiring consistency ─────
	// Skip when the material uses MaterialAttributes — the wiring comes through
	// MP_MaterialAttributes and per-property probing doesn't apply.
	if (!Material->bUseMaterialAttributes && Material->MaterialDomain == MD_Surface)
	{
		const bool bIsUnlit      = Models.HasShadingModel(MSM_Unlit);
		const bool bHasLit       = Models.HasShadingModel(MSM_DefaultLit)
									|| Models.HasShadingModel(MSM_Subsurface)
									|| Models.HasShadingModel(MSM_PreintegratedSkin)
									|| Models.HasShadingModel(MSM_ClearCoat)
									|| Models.HasShadingModel(MSM_SubsurfaceProfile)
									|| Models.HasShadingModel(MSM_TwoSidedFoliage)
									|| Models.HasShadingModel(MSM_Hair)
									|| Models.HasShadingModel(MSM_Cloth)
									|| Models.HasShadingModel(MSM_Eye);

		// Unlit: EmissiveColor is the only meaningful output; metallic / specular /
		// roughness / normal wires cost shader instructions for no visual effect.
		if (bIsUnlit && !bHasLit)
		{
			if (!IsPropertyWired(Material, MP_EmissiveColor))
			{
				EmitFinding(Out.Findings, TEXT("M5-8"), TEXT("warning"),
					TEXT("Unlit material has no EmissiveColor wired — it will render pitch-black."));
			}
			const EProp LitOnly[] = { MP_Metallic, MP_Specular, MP_Roughness, MP_Normal };
			for (EProp P : LitOnly)
			{
				if (IsPropertyWired(Material, P))
				{
					EmitFinding(Out.Findings, TEXT("M5-8"), TEXT("info"),
						FString::Printf(TEXT("Unlit material has %s wired — the connection is ignored at runtime."),
							MaterialPropertyName(P)));
				}
			}
		}
		// DefaultLit (and lit-variants): BaseColor + Normal strongly recommended.
		else if (bHasLit)
		{
			const EProp RequiredBase[] = { MP_BaseColor, MP_Normal };
			for (EProp P : RequiredBase)
			{
				if (!IsPropertyWired(Material, P))
				{
					EmitFinding(Out.Findings, TEXT("M5-8"), TEXT("warning"),
						FString::Printf(TEXT("Lit material is missing a %s wire — fall-back default will be used."),
							MaterialPropertyName(P)));
				}
			}
			// Subsurface family should wire SubsurfaceColor.
			if (Models.HasShadingModel(MSM_Subsurface) || Models.HasShadingModel(MSM_PreintegratedSkin)
				|| Models.HasShadingModel(MSM_TwoSidedFoliage))
			{
				if (!IsPropertyWired(Material, MP_SubsurfaceColor))
				{
					EmitFinding(Out.Findings, TEXT("M5-8"), TEXT("info"),
						TEXT("Subsurface shading model with no SubsurfaceColor wire — scatter tint will be white."));
				}
			}
			// ClearCoat uses CustomData0 = ClearCoat amount, CustomData1 = CC roughness.
			if (Models.HasShadingModel(MSM_ClearCoat))
			{
				if (!IsPropertyWired(Material, MP_CustomData0))
				{
					EmitFinding(Out.Findings, TEXT("M5-8"), TEXT("info"),
						TEXT("ClearCoat shading model with no ClearCoat amount (CustomData0) — varnish layer will be absent."));
				}
			}
		}
	}

	// ── Rule M5-7: expensive material missing Feature/Quality switch gate
	// Material with ≥1 Custom node OR ≥4 TextureSample nodes but no
	// FeatureLevelSwitch/QualitySwitch anywhere → info. Wrapping the
	// expensive subgraph with a QualitySwitch lets UE compile a cheaper
	// Low/Medium variant instead of forcing every hardware tier to run
	// the full-fat shader.
	//
	// ── Rule M5-13: Custom node uses SM5-only intrinsic without gate
	// Custom Code contains ddx_fine / ddy_fine / firstbithigh / firstbitlow
	// / reversebits / countbits → info. These need SM5; no gate means
	// Low/Mobile fallback has no way to compile a cheaper path.
	// PostProcess domain materials skip M5-7 entirely — they run every frame
	// regardless and the FeatureLevelSwitch / QualitySwitch gating doesn't
	// match the typical PP authoring idiom.
	if (Material->MaterialDomain != MD_PostProcess)
	{
		int32 TextureSampleCount = 0;
		int32 SceneTextureCount = 0;
		int32 CustomCount = 0;
		bool  bHasFLSwitch = false;
		bool  bHasQSwitch = false;
		for (UMaterialExpression* Expr : Expressions)
		{
			if (Expr->IsA<UMaterialExpressionTextureSample>()) ++TextureSampleCount;
			if (Expr->GetClass()->GetName() == TEXT("MaterialExpressionSceneTexture")) ++SceneTextureCount;
			if (Expr->IsA<UMaterialExpressionCustom>()) ++CustomCount;
			if (Expr->IsA<UMaterialExpressionFeatureLevelSwitch>()) bHasFLSwitch = true;
			if (Expr->IsA<UMaterialExpressionQualitySwitch>()) bHasQSwitch = true;
		}

		const int32 TexLookups = TextureSampleCount + SceneTextureCount;
		const bool bExpensive = (CustomCount >= 1 || TexLookups >= 4);
		if (bExpensive && !bHasFLSwitch && !bHasQSwitch)
		{
			EmitFinding(Out.Findings, TEXT("M5-7"), TEXT("info"),
				FString::Printf(TEXT("Material has %d TextureSample + %d SceneTexture + %d Custom nodes but no FeatureLevelSwitch / QualitySwitch — consider wrapping expensive work so UE can emit cheaper Low/Medium variants for weaker hardware."),
					TextureSampleCount, SceneTextureCount, CustomCount));
		}
	}
	{

		// M5-13: Custom SM-only intrinsics.
		static const TCHAR* const Sm5Intrinsics[] = {
			TEXT("ddx_fine"), TEXT("ddy_fine"),
			TEXT("firstbithigh"), TEXT("firstbitlow"),
			TEXT("reversebits"), TEXT("countbits"),
		};
		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
			if (!Custom) continue;
			const FString& Code = Custom->Code;
			for (const TCHAR* Intr : Sm5Intrinsics)
			{
				if (Code.Contains(Intr))
				{
					const FString IntrStr(Intr);
					EmitFinding(Out.Findings, TEXT("M5-13"), TEXT("info"),
						FString::Printf(TEXT("Custom node uses SM5-only intrinsic '%s' — make sure the material is gated by FeatureLevelSwitch so Mobile/ES3_1 has a fallback path."),
							*IntrStr),
						Expr->MaterialExpressionGuid,
						Expr->GetClass()->GetName(),
						IntrStr);
					break;  // one finding per Custom node is enough
				}
			}
		}
	}

	// ── Rule M5-6: static-switch misuse ─────────────────────────────
	// Two patterns flagged:
	//   1. LinearInterpolate whose Alpha is wired to a Constant (or Constant
	//      whose R is 0 / 1) — only one branch will ever be picked, so the
	//      Lerp is dead weight. Fix by inlining the chosen side OR converting
	//      to a StaticSwitchParameter if the decision is per-MI.
	//   2. LinearInterpolate whose Alpha is a ScalarParameter named like a
	//      boolean (`Use*` / `Enable*` / `Is*` / `Has*` / `Should*`) — info
	//      hint that a StaticSwitchParameter would compile the dead side out.
	{
		auto LooksBoolean = [](const FString& Name) -> bool
		{
			static const TCHAR* BoolPrefixes[] = {
				TEXT("Use"), TEXT("Enable"), TEXT("Is"), TEXT("Has"),
				TEXT("Should"), TEXT("Show"), TEXT("Bool"), TEXT("Toggle")
			};
			for (const TCHAR* P : BoolPrefixes)
			{
				if (Name.StartsWith(P, ESearchCase::IgnoreCase)) return true;
			}
			return false;
		};

		auto ConstantIsBinary = [](UMaterialExpressionConstant* C) -> int32
		{
			if (!C) return -1;
			if (FMath::IsNearlyEqual(C->R, 0.0f)) return 0;
			if (FMath::IsNearlyEqual(C->R, 1.0f)) return 1;
			return -1;
		};

		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionLinearInterpolate* Lerp =
				Cast<UMaterialExpressionLinearInterpolate>(Expr);
			if (!Lerp) continue;

			UMaterialExpression* AlphaSrc = Lerp->Alpha.Expression;
			if (!AlphaSrc) continue;

			// Pattern 1: Alpha is a literal Constant — dead branch.
			if (UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(AlphaSrc))
			{
				const int32 Binary = ConstantIsBinary(C);
				if (Binary == 0)
				{
					EmitFinding(Out.Findings, TEXT("M5-6"), TEXT("info"),
						TEXT("Lerp Alpha is Constant(0) — output is always the A input. "
							"Inline A and drop the Lerp, or convert to StaticSwitchParameter if the choice should be per-MI."),
						Expr->MaterialExpressionGuid,
						Expr->GetClass()->GetName(),
						TEXT("Dead branch: B never selected"));
				}
				else if (Binary == 1)
				{
					EmitFinding(Out.Findings, TEXT("M5-6"), TEXT("info"),
						TEXT("Lerp Alpha is Constant(1) — output is always the B input. "
							"Inline B and drop the Lerp, or convert to StaticSwitchParameter if the choice should be per-MI."),
						Expr->MaterialExpressionGuid,
						Expr->GetClass()->GetName(),
						TEXT("Dead branch: A never selected"));
				}
				else
				{
					// Non-binary constant (e.g. 0.5) — valid blend, no finding.
				}
			}
			// Pattern 2: Alpha is a ScalarParameter that names a boolean intent.
			else if (UMaterialExpressionScalarParameter* SP = Cast<UMaterialExpressionScalarParameter>(AlphaSrc))
			{
				const FString PName = SP->ParameterName.ToString();
				if (LooksBoolean(PName))
				{
					EmitFinding(Out.Findings, TEXT("M5-6"), TEXT("info"),
						FString::Printf(TEXT("Lerp Alpha driven by ScalarParameter '%s' (boolean-intent name) — consider StaticSwitchParameter so UE compiles the unused branch out instead of emitting a runtime lerp."),
							*PName),
						Expr->MaterialExpressionGuid,
						Expr->GetClass()->GetName(),
						TEXT("Candidate for StaticSwitch conversion"));
				}
			}
		}
	}

	// ── Rule M5-10: texture compression ↔ sampler-type consistency ─
	// Every TextureSample references a UTexture asset with both a
	// CompressionSettings enum and an SRGB bool. The SamplerType enum on
	// the expression has implicit expectations about both; a mismatch
	// means UE emits a shader-compile warning at best (wrong channel layout)
	// or a corrupt-looking render at worst. Flag per-expression so the agent
	// can pick which to fix.
	{
		auto CompressionName = [](TextureCompressionSettings S) -> const TCHAR*
		{
			switch (S)
			{
			case TC_Default:                  return TEXT("TC_Default");
			case TC_Normalmap:                return TEXT("TC_Normalmap");
			case TC_Masks:                    return TEXT("TC_Masks");
			case TC_Grayscale:                return TEXT("TC_Grayscale");
			case TC_Alpha:                    return TEXT("TC_Alpha");
			case TC_HDR:                      return TEXT("TC_HDR");
			case TC_BC7:                      return TEXT("TC_BC7");
			case TC_VectorDisplacementmap:    return TEXT("TC_VectorDisplacementmap");
			case TC_DistanceFieldFont:        return TEXT("TC_DistanceFieldFont");
			case TC_HalfFloat:                return TEXT("TC_HalfFloat");
			case TC_LQ:                       return TEXT("TC_LQ");
			case TC_EditorIcon:               return TEXT("TC_EditorIcon");
			case TC_HDR_Compressed:           return TEXT("TC_HDR_Compressed");
			case TC_SingleFloat:              return TEXT("TC_SingleFloat");
			default:                          return TEXT("TC_Unknown");
			}
		};
		auto SamplerTypeName = [](EMaterialSamplerType ST) -> const TCHAR*
		{
			switch (ST)
			{
			case SAMPLERTYPE_Color:            return TEXT("Color");
			case SAMPLERTYPE_Grayscale:        return TEXT("Grayscale");
			case SAMPLERTYPE_Alpha:            return TEXT("Alpha");
			case SAMPLERTYPE_Normal:           return TEXT("Normal");
			case SAMPLERTYPE_Masks:            return TEXT("Masks");
			case SAMPLERTYPE_DistanceFieldFont:return TEXT("DistanceFieldFont");
			case SAMPLERTYPE_LinearColor:      return TEXT("LinearColor");
			case SAMPLERTYPE_LinearGrayscale:  return TEXT("LinearGrayscale");
			case SAMPLERTYPE_Data:             return TEXT("Data");
			default:                           return TEXT("Unknown");
			}
		};
		auto IsEnginePlaceholder = [](const FString& Path) -> bool
		{
			// Engine 1×1 defaults used by template builders — MI override replaces
			// the real texture at runtime, so an agent shouldn't be forced to fix
			// the sampler mismatch on these defaults themselves.
			return Path.StartsWith(TEXT("/Engine/EngineResources/"))
				|| Path == TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal")
				|| Path == TEXT("/Engine/EngineMaterials/DefaultTexture.DefaultTexture");
		};

		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr);
			if (!TS || !TS->Texture) continue;

			const UTexture* Tex = TS->Texture;
			const TextureCompressionSettings Comp = Tex->CompressionSettings;
			const bool bSRGB = Tex->SRGB;
			const EMaterialSamplerType ST = TS->SamplerType;
			const FString TexPath = Tex->GetPathName();
			const bool bPlaceholder = IsEnginePlaceholder(TexPath);
			// Engine placeholders → demote warning to info so templates that
			// default-assign WhiteSquareTexture don't get yelled at.
			const TCHAR* Severity = bPlaceholder ? TEXT("info") : TEXT("warning");

			auto Emit = [&](const TCHAR* Msg)
			{
				EmitFinding(Out.Findings, TEXT("M5-10"), Severity,
					FString::Printf(TEXT("%s: sampler=%s, texture compression=%s, sRGB=%s"),
						Msg, SamplerTypeName(ST), CompressionName(Comp),
						bSRGB ? TEXT("true") : TEXT("false")),
					Expr->MaterialExpressionGuid,
					Expr->GetClass()->GetName(),
					TexPath);
			};

			switch (ST)
			{
			case SAMPLERTYPE_Color:
				// Color sampler: sRGB=true + BC1/BC3/BC7/Default expected.
				if (!bSRGB)
				{
					Emit(TEXT("Color sampler on a non-sRGB texture — did you mean LinearColor?"));
				}
				if (Comp == TC_Normalmap || Comp == TC_Masks || Comp == TC_Grayscale
					|| Comp == TC_HDR || Comp == TC_HDR_Compressed)
				{
					Emit(TEXT("Color sampler but texture compression is data-class (not BC1/BC3/BC7)"));
				}
				break;
			case SAMPLERTYPE_Normal:
				if (Comp != TC_Normalmap)
				{
					Emit(TEXT("Normal sampler but compression isn't TC_Normalmap"));
				}
				if (bSRGB)
				{
					Emit(TEXT("Normal sampler requires sRGB=false"));
				}
				break;
			case SAMPLERTYPE_Masks:
				if (Comp != TC_Masks)
				{
					Emit(TEXT("Masks sampler but compression isn't TC_Masks"));
				}
				if (bSRGB)
				{
					Emit(TEXT("Masks sampler requires sRGB=false"));
				}
				break;
			case SAMPLERTYPE_Grayscale:
				if (bSRGB)
				{
					// Grayscale is sRGB by default in UE, so don't flag sRGB=true.
				}
				if (Comp != TC_Grayscale)
				{
					Emit(TEXT("Grayscale sampler but compression isn't TC_Grayscale"));
				}
				break;
			case SAMPLERTYPE_LinearColor:
			case SAMPLERTYPE_LinearGrayscale:
				if (bSRGB)
				{
					Emit(TEXT("Linear* sampler requires sRGB=false"));
				}
				break;
			default:
				break;
			}
		}
	}

	// ── Rule M5-11: trivial Custom nodes ────────────────────────────
	// Custom blocks lose constant folding / CSE / DCE — tiny bodies are
	// almost always a net negative vs. equivalent node chains.
	// Exception: a one-liner that's just `return BridgeXxx(...);` through an
	// included .ush is the intended snippet-dispatch pattern; the real work
	// lives in the shared library, not the node body. Don't flag those.
	//
	// ── Rule M5-12: Custom nodes that grab textures themselves ─────
	// Texture2DSample / SceneTextureLookup inside a Custom bypass UE's
	// sampler-sharing + dependency tracking. Pass the sample result in
	// from a graph TextureSample node instead.
	{
		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
			if (!Custom) continue;

			// Strip comments + whitespace for the triviality heuristic.
			const FString& Code = Custom->Code;
			const int32 SigChars = Code.TrimStartAndEnd().Len();
			int32 Newlines = 0;
			for (int32 i = 0; i < Code.Len(); ++i)
			{
				if (Code[i] == TEXT('\n')) ++Newlines;
			}

			// Skip snippet-dispatch nodes: body is a call to a shared helper
			// via an included .ush. The helper itself may be hundreds of lines.
			const bool bIsSnippetTrampoline =
				Custom->IncludeFilePaths.Num() > 0 &&
				Code.Contains(TEXT("Bridge"));

			if (SigChars > 0 && SigChars < 64 && Newlines <= 1 && !bIsSnippetTrampoline)
			{
				EmitFinding(Out.Findings, TEXT("M5-11"), TEXT("info"),
					FString::Printf(TEXT("Custom node body is only %d chars / %d newlines — likely cheaper as native graph nodes (keeps const folding + CSE + DCE)."),
						SigChars, Newlines + 1),
					Expr->MaterialExpressionGuid,
					Expr->GetClass()->GetName());
			}

			if (Code.Contains(TEXT("Texture2DSample")) || Code.Contains(TEXT("SceneTextureLookup")))
			{
				EmitFinding(Out.Findings, TEXT("M5-12"), TEXT("warning"),
					TEXT("Custom node sources textures directly (Texture2DSample / SceneTextureLookup) — ")
					TEXT("use a graph TextureSample node and feed its result into the Custom input instead, ")
					TEXT("so UE can share samplers and track dependencies."),
					Expr->MaterialExpressionGuid,
					Expr->GetClass()->GetName());
			}
		}
	}

	// ── Rule M5-9: MI chain depth + StaticSwitch override risk ─────
	// Only relevant when analyze_material was called on a UMaterialInstance
	// (not a UMaterial master). Deep chains hurt cook-time iteration + make
	// it harder to predict final parameter state; a StaticSwitchParameter
	// override on an MI causes an extra shader permutation on top of whatever
	// the master already spawns.
	if (UMaterialInstance* MI = Cast<UMaterialInstance>(MatInterface))
	{
		// Walk the parent chain, counting MI layers before we hit the UMaterial.
		int32 ChainDepth = 0;
		UMaterialInterface* Cursor = MI;
		while (UMaterialInstance* CurMI = Cast<UMaterialInstance>(Cursor))
		{
			++ChainDepth;
			Cursor = CurMI->Parent;
			if (!Cursor) break;
		}
		if (ChainDepth > 3)
		{
			EmitFinding(Out.Findings, TEXT("M5-9"), TEXT("warning"),
				FString::Printf(TEXT("MI chain is %d layers deep — consider flattening; deep chains slow cooks and make final parameter state hard to predict."),
					ChainDepth));
		}
		else if (ChainDepth > 0)
		{
			// Informational — useful context without being alarming.
			EmitFinding(Out.Findings, TEXT("M5-9"), TEXT("info"),
				FString::Printf(TEXT("MI chain depth: %d (master at root)."), ChainDepth));
		}

		// Static-switch overrides on MI → each flip = a new shader permutation.
		// Walk every MI in the chain, collect unique override names.
		TSet<FString> SwitchOverrides;
		for (UMaterialInterface* Walk = MI; Walk;
		     Walk = Cast<UMaterialInstance>(Walk) ? Cast<UMaterialInstance>(Walk)->Parent : nullptr)
		{
			UMaterialInstance* WalkMI = Cast<UMaterialInstance>(Walk);
			if (!WalkMI) break;
			const FStaticParameterSet& StaticParams = WalkMI->GetStaticParameters();
			for (const FStaticSwitchParameter& SSP : StaticParams.StaticSwitchParameters)
			{
				if (SSP.bOverride)
				{
					SwitchOverrides.Add(SSP.ParameterInfo.Name.ToString());
				}
			}
		}
		if (SwitchOverrides.Num() >= 3)
		{
			FString Names = FString::Join(SwitchOverrides.Array(), TEXT(", "));
			EmitFinding(Out.Findings, TEXT("M5-9"), TEXT("warning"),
				FString::Printf(TEXT("%d StaticSwitch overrides in the MI chain (%s) — each flip spawns a fresh shader permutation; consider pushing these into the master as defaults instead."),
					SwitchOverrides.Num(), *Names));
		}
		else if (SwitchOverrides.Num() > 0)
		{
			FString Names = FString::Join(SwitchOverrides.Array(), TEXT(", "));
			EmitFinding(Out.Findings, TEXT("M5-9"), TEXT("info"),
				FString::Printf(TEXT("MI overrides StaticSwitch(es): %s — causes extra shader permutations beyond the master defaults."),
					*Names));
		}
	}

	// Sort findings: error → warning → info, then by RuleId.
	auto SevRank = [](const FString& S) -> int32
	{
		if (S == TEXT("error")) return 0;
		if (S == TEXT("warning")) return 1;
		return 2;
	};
	Out.Findings.Sort([&SevRank](const FBridgeMaterialFinding& A, const FBridgeMaterialFinding& B)
	{
		const int32 SA = SevRank(A.Severity);
		const int32 SB = SevRank(B.Severity);
		if (SA != SB) return SA < SB;
		return A.RuleId.Compare(B.RuleId) < 0;
	});

	return Out;
}


// ─── M5-14: auto_fix_material ──────────────────────────────────────

FBridgeMaterialAutoFixResult UUnrealBridgeMaterialLibrary::AutoFixMaterial(
	const FString& MaterialPath,
	const TArray<FString>& Fixes,
	bool bSaveAfter)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialAutoFixResult Out;

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
	{
		Out.Log.Add(FString::Printf(TEXT("could not load material '%s'"), *MaterialPath));
		return Out;
	}

	// Normalize + partition requested fixes.
	const TSet<FString> KnownFixes = {
		TEXT("drop_unused"),
		TEXT("samplersource_share"),
		TEXT("static_switch_conversion"),
		TEXT("inline_trivial_custom"),
	};
	TSet<FString> RequestedFixes;
	for (const FString& F : Fixes)
	{
		if (KnownFixes.Contains(F))
		{
			RequestedFixes.Add(F);
		}
		else
		{
			Out.SkippedFixes.Add(F);
			Out.Log.Add(FString::Printf(TEXT("SKIP: unknown fix id '%s'"), *F));
		}
	}

	if (RequestedFixes.Num() == 0)
	{
		Out.bSuccess = Out.SkippedFixes.Num() == 0;
		return Out;
	}

	// Re-run the analysis so we operate on the current snapshot.
	FBridgeMaterialAnalysis Analysis =
		UUnrealBridgeMaterialLibrary::AnalyzeMaterial(MaterialPath, 0, 0);
	if (!Analysis.bFound)
	{
		Out.Log.Add(TEXT("analyze_material failed — aborting"));
		return Out;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge",
		"AutoFixMaterial", "Auto-fix material"));
	Material->Modify();

	int32 FindingsHandled = 0;

	// --- drop_unused (M5-3) ----------------------------------------
	if (RequestedFixes.Contains(TEXT("drop_unused")))
	{
		// Collect guids so we don't mutate the expression list mid-iteration.
		TArray<FGuid> ToDelete;
		for (const FBridgeMaterialFinding& F : Analysis.Findings)
		{
			if (F.RuleId == TEXT("M5-3") && F.ExpressionGuid.IsValid())
			{
				ToDelete.AddUnique(F.ExpressionGuid);
			}
		}
		for (const FGuid& Guid : ToDelete)
		{
			UMaterialExpression* Expr = FindExpressionByGuid(Material, Guid);
			if (!Expr) continue;
			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);
			++Out.NodesRemoved;
			++FindingsHandled;
			Out.Log.Add(FString::Printf(TEXT("drop_unused: removed %s (%s)"),
				*Expr->GetClass()->GetName(), *Guid.ToString(EGuidFormats::Digits)));
		}
	}

	// --- samplersource_share (M5-5) --------------------------------
	if (RequestedFixes.Contains(TEXT("samplersource_share")))
	{
		TSet<FGuid> ToSwitch;
		for (const FBridgeMaterialFinding& F : Analysis.Findings)
		{
			if (F.RuleId == TEXT("M5-5") && F.ExpressionGuid.IsValid())
			{
				ToSwitch.Add(F.ExpressionGuid);
			}
		}
		for (const FGuid& Guid : ToSwitch)
		{
			UMaterialExpression* Expr = FindExpressionByGuid(Material, Guid);
			UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr);
			if (!TS) continue;
			TS->Modify();
			TS->PreEditChange(nullptr);
			TS->SamplerSource = SSM_Wrap_WorldGroupSettings;
			TS->PostEditChange();
			++Out.PropertiesChanged;
			++FindingsHandled;
			Out.Log.Add(FString::Printf(TEXT("samplersource_share: %s → SSM_Wrap_WorldGroupSettings (%s)"),
				*Expr->GetClass()->GetName(), *Guid.ToString(EGuidFormats::Digits)));
		}
	}

	// --- static_switch_conversion (M5-6 Pattern 2) ------------------
	//
	// The M5-6 finding fires on the Lerp. Pattern 2 specifically sets
	// Detail = "Candidate for StaticSwitch conversion" — we restrict the
	// fix to that variant so we don't accidentally process Pattern 1
	// (Lerp with a Constant alpha) which wants inlining, not a switch.
	if (RequestedFixes.Contains(TEXT("static_switch_conversion")))
	{
		TArray<FGuid> LerpGuids;
		for (const FBridgeMaterialFinding& F : Analysis.Findings)
		{
			if (F.RuleId == TEXT("M5-6")
				&& F.ExpressionGuid.IsValid()
				&& F.Detail == TEXT("Candidate for StaticSwitch conversion"))
			{
				LerpGuids.AddUnique(F.ExpressionGuid);
			}
		}

		for (const FGuid& LerpGuid : LerpGuids)
		{
			UMaterialExpression* Expr = FindExpressionByGuid(Material, LerpGuid);
			UMaterialExpressionLinearInterpolate* Lerp =
				Cast<UMaterialExpressionLinearInterpolate>(Expr);
			if (!Lerp) continue;

			UMaterialExpressionScalarParameter* SP =
				Cast<UMaterialExpressionScalarParameter>(Lerp->Alpha.Expression);
			if (!SP)
			{
				Out.Log.Add(FString::Printf(TEXT("static_switch_conversion: SKIP — %s alpha is no longer a ScalarParameter"),
					*LerpGuid.ToString(EGuidFormats::Digits)));
				continue;
			}

			const FName ParamName = SP->ParameterName;
			const bool DefaultBool = (SP->DefaultValue >= 0.5f);
			const FName Group = SP->Group;
			const int32 SortPriority = SP->SortPriority;

			UClass* SSClass = UMaterialExpressionStaticSwitchParameter::StaticClass();
			UMaterialExpression* NewExpr = UMaterialEditingLibrary::CreateMaterialExpression(
				Material, SSClass,
				Lerp->MaterialExpressionEditorX + 20,
				Lerp->MaterialExpressionEditorY);
			UMaterialExpressionStaticSwitchParameter* SS =
				Cast<UMaterialExpressionStaticSwitchParameter>(NewExpr);
			if (!SS)
			{
				Out.Log.Add(FString::Printf(TEXT("static_switch_conversion: FAIL — could not create StaticSwitchParameter for %s"),
					*ParamName.ToString()));
				continue;
			}

			SS->Modify();
			SS->PreEditChange(nullptr);
			SS->ParameterName = ParamName;
			SS->DefaultValue = DefaultBool;
			SS->Group = Group;
			SS->SortPriority = SortPriority;
			if (!SS->MaterialExpressionGuid.IsValid())
			{
				SS->MaterialExpressionGuid = FGuid::NewGuid();
			}
			// StaticSwitch.A = True branch (Lerp's B, picked when alpha=1)
			// StaticSwitch.B = False branch (Lerp's A, picked when alpha=0)
			SS->A = Lerp->B;
			SS->B = Lerp->A;
			SS->PostEditChange();

			const FRewireCounts Counts = RedirectUsageToNewSource(
				Material, Lerp, 0, SS, 0);

			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Lerp);
			++Out.NodesRemoved;

			// Drop the now-orphan ScalarParameter so the MI parameter list
			// doesn't end up with a dead entry. If another node still reads it
			// (unlikely but possible) we leave it in place.
			bool bSPDropped = false;
			if (!IsExpressionReferenced(Material, SP))
			{
				UMaterialEditingLibrary::DeleteMaterialExpression(Material, SP);
				++Out.NodesRemoved;
				bSPDropped = true;
			}

			++Out.NodesAdded;
			++FindingsHandled;
			Out.Log.Add(FString::Printf(TEXT("static_switch_conversion: '%s' Lerp → StaticSwitchParameter (default=%s, rewired %d inputs + %d main outputs, ScalarParameter %s)"),
				*ParamName.ToString(),
				DefaultBool ? TEXT("true") : TEXT("false"),
				Counts.ExpressionInputs, Counts.MainOutputs,
				bSPDropped ? TEXT("dropped") : TEXT("kept (still referenced)")));
		}
	}

	// --- inline_trivial_custom (M5-11) ------------------------------
	//
	// Match a small set of single-op HLSL bodies and replace the Custom
	// with the equivalent native graph node. Keeps the existing input
	// wiring (Custom.Inputs[i].Input → new node's i-th input), preserves
	// downstream consumers via RedirectUsageToNewSource.
	//
	// Patterns recognised:
	//   return A + B;       → Add
	//   return A - B;       → Subtract
	//   return A * B;       → Multiply
	//   return A / B;       → Divide
	//   return saturate(A); → Saturate
	//   return abs(A);      → Abs
	//   return frac(A);     → Frac
	//   return floor(A);    → Floor
	//   return ceil(A);     → Ceil
	//   return 1 - A;       → OneMinus
	//   return 1.0 - A;     → OneMinus
	//   return lerp(A,B,C); → LinearInterpolate
	//   return min(A, B);   → Min
	//   return max(A, B);   → Max
	//   return pow(A, B);   → Power
	//   return dot(A, B);   → DotProduct
	//   return normalize(A);→ Normalize
	if (RequestedFixes.Contains(TEXT("inline_trivial_custom")))
	{
		auto StripWs = [](FString In) -> FString
		{
			In.ReplaceInline(TEXT("\n"), TEXT(" "));
			In.ReplaceInline(TEXT("\r"), TEXT(" "));
			In.ReplaceInline(TEXT("\t"), TEXT(" "));
			while (In.ReplaceInline(TEXT("  "), TEXT(" ")) > 0) {}
			In.TrimStartAndEndInline();
			if (In.EndsWith(TEXT(";")))
			{
				In.LeftChopInline(1);
				In.TrimStartAndEndInline();
			}
			return In;
		};

		auto FindCustomInputByName = [](UMaterialExpressionCustom* Custom, const FString& Name) -> int32
		{
			for (int32 i = 0; i < Custom->Inputs.Num(); ++i)
			{
				if (Custom->Inputs[i].InputName.ToString().Equals(Name, ESearchCase::IgnoreCase))
				{
					return i;
				}
			}
			return INDEX_NONE;
		};

		// Resolve one token from the HLSL return — either a Custom input name
		// (wire to its source expression) or a literal constant (materialize
		// as a new Constant expression). Only literals "1", "1.0", and "0"
		// are accepted so we don't silently inline arbitrary numbers.
		auto ResolveToken = [&](UMaterial* Mat,
		                        UMaterialExpressionCustom* Custom,
		                        const FString& Token,
		                        int32 FallbackX, int32 FallbackY,
		                        FExpressionInput& OutInput) -> bool
		{
			const FString Tok = Token.TrimStartAndEnd();
			const int32 InIdx = FindCustomInputByName(Custom, Tok);
			if (InIdx != INDEX_NONE)
			{
				OutInput = Custom->Inputs[InIdx].Input;
				return true;
			}
			// Fall back to literal 0 / 1.
			float LitValue = 0.0f;
			if (Tok == TEXT("1") || Tok == TEXT("1.0") || Tok == TEXT("1.0f")) LitValue = 1.0f;
			else if (Tok == TEXT("0") || Tok == TEXT("0.0") || Tok == TEXT("0.0f")) LitValue = 0.0f;
			else return false;

			UMaterialExpression* Lit = UMaterialEditingLibrary::CreateMaterialExpression(
				Mat, UMaterialExpressionConstant::StaticClass(), FallbackX, FallbackY);
			if (UMaterialExpressionConstant* LC = Cast<UMaterialExpressionConstant>(Lit))
			{
				LC->R = LitValue;
				if (!LC->MaterialExpressionGuid.IsValid()) LC->MaterialExpressionGuid = FGuid::NewGuid();
				OutInput.Expression = LC;
				OutInput.OutputIndex = 0;
				return true;
			}
			return false;
		};

		struct FPattern
		{
			const TCHAR* Prefix;       // token that introduces the op, e.g. "saturate("
			int32 ArgCount;            // 1, 2, or 3
			const TCHAR* ShortClassName; // short name for ResolveExpressionClass
			const TCHAR* ArgPinNames[3]; // names to wire each arg to on the new node
		};

		static const FPattern Patterns[] =
		{
			{ TEXT("saturate("),  1, TEXT("Saturate"),   { TEXT(""),     TEXT(""),    TEXT("") } },
			{ TEXT("abs("),       1, TEXT("Abs"),        { TEXT(""),     TEXT(""),    TEXT("") } },
			{ TEXT("frac("),      1, TEXT("Frac"),       { TEXT(""),     TEXT(""),    TEXT("") } },
			{ TEXT("floor("),     1, TEXT("Floor"),      { TEXT(""),     TEXT(""),    TEXT("") } },
			{ TEXT("ceil("),      1, TEXT("Ceil"),       { TEXT(""),     TEXT(""),    TEXT("") } },
			{ TEXT("normalize("), 1, TEXT("Normalize"),  { TEXT(""),     TEXT(""),    TEXT("") } },
			{ TEXT("lerp("),      3, TEXT("LinearInterpolate"), { TEXT("A"), TEXT("B"), TEXT("Alpha") } },
			{ TEXT("min("),       2, TEXT("Min"),        { TEXT("A"),    TEXT("B"),   TEXT("") } },
			{ TEXT("max("),       2, TEXT("Max"),        { TEXT("A"),    TEXT("B"),   TEXT("") } },
			{ TEXT("pow("),       2, TEXT("Power"),      { TEXT("Base"), TEXT("Exp"), TEXT("") } },
			{ TEXT("dot("),       2, TEXT("DotProduct"), { TEXT("A"),    TEXT("B"),   TEXT("") } },
		};

		TArray<FGuid> CustomGuids;
		for (const FBridgeMaterialFinding& F : Analysis.Findings)
		{
			if (F.RuleId == TEXT("M5-11") && F.ExpressionGuid.IsValid())
			{
				CustomGuids.AddUnique(F.ExpressionGuid);
			}
		}

		for (const FGuid& CGuid : CustomGuids)
		{
			UMaterialExpression* Expr = FindExpressionByGuid(Material, CGuid);
			UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
			if (!Custom) continue;

			const FString Body = StripWs(Custom->Code);

			// Must start with "return " to qualify; single expression only.
			if (!Body.StartsWith(TEXT("return ")))
			{
				Out.Log.Add(FString::Printf(TEXT("inline_trivial_custom: SKIP — %s has no 'return' (body='%s')"),
					*CGuid.ToString(EGuidFormats::Digits), *Body));
				continue;
			}
			FString Expr1 = Body.RightChop(7).TrimStartAndEnd();

			UClass* ReplaceClass = nullptr;
			TArray<FString> ArgTokens;
			TArray<FString> PinNames;

			// Try binary operator patterns first (simpler parse).
			// Order-sensitive: check explicit "1 - X" / "1.0 - X" as OneMinus
			// before the generic Subtract.

			// Specialised OneMinus: body is `1 - <token>` / `1.0 - <token>`.
			{
				FString Lhs, Rhs;
				if (Expr1.Split(TEXT(" - "), &Lhs, &Rhs))
				{
					const FString LhsTrim = Lhs.TrimStartAndEnd();
					if (LhsTrim == TEXT("1") || LhsTrim == TEXT("1.0") || LhsTrim == TEXT("1.0f"))
					{
						ReplaceClass = ResolveExpressionClass(TEXT("OneMinus"));
						ArgTokens.Add(Rhs.TrimStartAndEnd());
						PinNames.Add(TEXT(""));
					}
				}
			}

			if (!ReplaceClass)
			{
				// Generic binary operators — search for " + " / " - " / " * " / " / " in Expr1.
				struct FBinOp { const TCHAR* Op; const TCHAR* ShortClass; const TCHAR* PinA; const TCHAR* PinB; };
				static const FBinOp Binops[] = {
					{ TEXT(" + "), TEXT("Add"),      TEXT("A"), TEXT("B") },
					{ TEXT(" - "), TEXT("Subtract"), TEXT("A"), TEXT("B") },
					{ TEXT(" * "), TEXT("Multiply"), TEXT("A"), TEXT("B") },
					{ TEXT(" / "), TEXT("Divide"),   TEXT("A"), TEXT("B") },
				};
				for (const FBinOp& B : Binops)
				{
					FString Lhs, Rhs;
					if (Expr1.Split(B.Op, &Lhs, &Rhs))
					{
						// Don't treat function-call-internal operators as binary.
						if (Lhs.Contains(TEXT("(")) || Rhs.Contains(TEXT("("))) continue;
						ReplaceClass = ResolveExpressionClass(B.ShortClass);
						ArgTokens.Add(Lhs.TrimStartAndEnd());
						ArgTokens.Add(Rhs.TrimStartAndEnd());
						PinNames.Add(B.PinA);
						PinNames.Add(B.PinB);
						break;
					}
				}
			}

			// Function-call patterns.
			if (!ReplaceClass)
			{
				for (const FPattern& P : Patterns)
				{
					if (!Expr1.StartsWith(P.Prefix)) continue;
					if (!Expr1.EndsWith(TEXT(")"))) continue;
					FString Inner = Expr1.Mid(FCString::Strlen(P.Prefix));
					Inner.LeftChopInline(1); // drop trailing ')'
					TArray<FString> Args;
					Inner.ParseIntoArray(Args, TEXT(","), /*CullEmpty=*/ false);
					if (Args.Num() != P.ArgCount) continue;
					ReplaceClass = ResolveExpressionClass(P.ShortClassName);
					for (int32 i = 0; i < P.ArgCount; ++i)
					{
						ArgTokens.Add(Args[i].TrimStartAndEnd());
						PinNames.Add(P.ArgPinNames[i]);
					}
					break;
				}
			}

			if (!ReplaceClass || ArgTokens.Num() == 0)
			{
				Out.Log.Add(FString::Printf(TEXT("inline_trivial_custom: SKIP — %s body does not match a known single-op pattern (expr='%s')"),
					*CGuid.ToString(EGuidFormats::Digits), *Expr1));
				continue;
			}

			// Build the replacement node.
			UMaterialExpression* NewNode = UMaterialEditingLibrary::CreateMaterialExpression(
				Material, ReplaceClass,
				Custom->MaterialExpressionEditorX,
				Custom->MaterialExpressionEditorY);
			if (!NewNode)
			{
				Out.Log.Add(FString::Printf(TEXT("inline_trivial_custom: FAIL — could not create %s"),
					*ReplaceClass->GetName()));
				continue;
			}
			if (!NewNode->MaterialExpressionGuid.IsValid())
			{
				NewNode->MaterialExpressionGuid = FGuid::NewGuid();
			}
			NewNode->Modify();
			NewNode->PreEditChange(nullptr);

			// Wire each argument.
			bool bWireOk = true;
			int32 LitStackY = Custom->MaterialExpressionEditorY + 80;
			for (int32 i = 0; i < ArgTokens.Num(); ++i)
			{
				FExpressionInput SrcInput;
				if (!ResolveToken(Material, Custom, ArgTokens[i],
						Custom->MaterialExpressionEditorX - 160,
						LitStackY, SrcInput))
				{
					Out.Log.Add(FString::Printf(TEXT("inline_trivial_custom: SKIP — '%s' token not a known Custom input or 0/1 literal"),
						*ArgTokens[i]));
					bWireOk = false;
					break;
				}
				LitStackY += 60;

				const int32 DstIdx = FindInputIndexByName(NewNode, PinNames[i]);
				if (DstIdx == INDEX_NONE)
				{
					Out.Log.Add(FString::Printf(TEXT("inline_trivial_custom: FAIL — '%s' pin missing on %s"),
						*PinNames[i], *ReplaceClass->GetName()));
					bWireOk = false;
					break;
				}
				FExpressionInput* DstInput = NewNode->GetInput(DstIdx);
				if (!DstInput)
				{
					bWireOk = false;
					break;
				}
				*DstInput = SrcInput;
			}
			NewNode->PostEditChange();

			if (!bWireOk)
			{
				// Partial wire — drop the stub and leave the Custom in place.
				UMaterialEditingLibrary::DeleteMaterialExpression(Material, NewNode);
				continue;
			}

			const FRewireCounts Counts = RedirectUsageToNewSource(
				Material, Custom, 0, NewNode, 0);

			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Custom);
			++Out.NodesRemoved;
			++Out.NodesAdded;
			++FindingsHandled;
			Out.Log.Add(FString::Printf(TEXT("inline_trivial_custom: Custom → %s (%d args, rewired %d inputs + %d main outputs)"),
				*ReplaceClass->GetName(),
				ArgTokens.Num(), Counts.ExpressionInputs, Counts.MainOutputs));
		}
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	if (bSaveAfter && (Out.NodesRemoved > 0 || Out.PropertiesChanged > 0 || Out.NodesAdded > 0))
	{
		TArray<UPackage*> Pkgs;
		Pkgs.Add(Material->GetPackage());
		UEditorLoadingAndSavingUtils::SavePackages(Pkgs, /*bOnlyDirty=*/ true);
	}

	Out.FindingsFixed = FindingsHandled;
	Out.bSuccess = true;
	return Out;
}


FBridgeMaterialGraphOpResult UUnrealBridgeMaterialLibrary::SetMaterialAttributeLayers(
	const FString& MaterialPath,
	FGuid ExpressionGuid,
	const TArray<UMaterialFunctionInterface*>& Layers,
	const TArray<UMaterialFunctionInterface*>& Blends,
	const TArray<FString>& LayerNames)
{
	using namespace BridgeMaterialImpl;

	FBridgeMaterialGraphOpResult Out;
	Out.bSuccess = false;
	Out.FailedAtIndex = 0;

	// Input validation.
	if (Layers.Num() < 1)
	{
		Out.Error = TEXT("Layers must contain at least one entry (the background).");
		return Out;
	}
	if (Blends.Num() != Layers.Num() - 1)
	{
		Out.Error = FString::Printf(
			TEXT("Blends count (%d) must equal Layers count - 1 (%d); Blends[i] is the blend MF for Layers[i+1]."),
			Blends.Num(), Layers.Num() - 1);
		return Out;
	}
	if (LayerNames.Num() != Layers.Num())
	{
		Out.Error = FString::Printf(
			TEXT("LayerNames count (%d) must equal Layers count (%d)."),
			LayerNames.Num(), Layers.Num());
		return Out;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
	{
		Out.Error = FString::Printf(TEXT("Material not loadable: %s"), *MaterialPath);
		return Out;
	}

	UMaterialExpression* Expr = FindExpressionByGuid(Material, ExpressionGuid);
	if (!Expr)
	{
		Out.Error = FString::Printf(TEXT("No expression with guid %s on %s."),
			*ExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens), *MaterialPath);
		return Out;
	}

	UMaterialExpressionMaterialAttributeLayers* MAL =
		Cast<UMaterialExpressionMaterialAttributeLayers>(Expr);
	if (!MAL)
	{
		Out.Error = FString::Printf(
			TEXT("Expression %s is not MaterialAttributeLayers (found %s)."),
			*ExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens),
			*Expr->GetClass()->GetName());
		return Out;
	}

	FScopedTransaction Tx(NSLOCTEXT("UnrealBridge", "SetMAL",
		"Configure MaterialAttributeLayers stack"));
	Material->Modify();
	MAL->Modify();

	// Build the stack via the authoritative engine APIs. Each Append* call
	// updates Layers + Blends + every EditorOnly parallel array atomically
	// (LayerStates / LayerNames / LayerGuids / LayerLinkStates / restrictions),
	// keeping the invariants that RebuildLayerGraph + PostLoad rely on.
	FMaterialLayersFunctions NewStack;
	NewStack.Empty();
	NewStack.AddDefaultBackgroundLayer();
	for (int32 i = 1; i < Layers.Num(); ++i)
	{
		NewStack.AppendBlendedLayer();
	}

	// Assign the MF pointers produced by ensure_layer_stack_assets().
	// Layers[0] = background; Blends[0] = blend for Layers[1]; etc.
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		NewStack.Layers[i] = Layers[i];
	}
	for (int32 i = 0; i < Blends.Num(); ++i)
	{
		NewStack.Blends[i] = Blends[i];
	}

	// Override display names so the editor's Layers panel shows something
	// meaningful instead of "Background" / "Layer 1" / "Layer 2".
	for (int32 i = 0; i < LayerNames.Num(); ++i)
	{
		NewStack.EditorOnly.LayerNames[i] = FText::FromString(LayerNames[i]);
	}

	MAL->DefaultLayers = NewStack;
	MAL->RebuildLayerGraph(/*bDisablePhysicalMaterials=*/ false);

	// PostEditChange so the material recompiles the shader map with the new
	// layer permutation. Callers that want to persist to disk should invoke
	// save_master after this returns — matches the other graph-edit ops.
	MAL->PostEditChange();
	Material->PostEditChange();
	Material->MarkPackageDirty();

	Out.bSuccess = true;
	Out.OpsApplied = 1;
	Out.FailedAtIndex = -1;
	return Out;
}

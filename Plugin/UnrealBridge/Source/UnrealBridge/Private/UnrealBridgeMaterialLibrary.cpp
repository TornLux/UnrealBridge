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
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
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

		// Input names — use the non-deprecated FExpressionInputIterator (UE 5.5+).
		for (FExpressionInputIterator It{Expr}; It; ++It)
		{
			Node.InputNames.Add(Expr->GetInputName(It.Index).ToString());
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
				Conn.DstInputName = Expr->GetInputName(It.Index).ToString();

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

	static FString NormalizePinName(const FString& Name)
	{
		// "" and "None" both mean the default anonymous output — CE treats empty as default.
		if (Name.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return FString();
		}
		return Name;
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

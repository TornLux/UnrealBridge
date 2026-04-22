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
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
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
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: unknown preview mesh '%s' — falling back to sphere"), *Preset);
			Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		}
		return LoadObject<UStaticMesh>(nullptr, *Path);
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
			// Sky + single directional — better for reflective PBR evaluation.
			USkyLightComponent* Sky = NewObject<USkyLightComponent>(
				GetTransientPackage(), USkyLightComponent::StaticClass());
			Sky->SetIntensity(3.0f);
			Sky->SourceType = ESkyLightSourceType::SLS_CapturedScene;
			Scene.AddComponent(Sky, FTransform::Identity);

			UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(
				GetTransientPackage(), UDirectionalLightComponent::StaticClass());
			Sun->SetIntensity(6.0f);
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
		FColor* Dst = reinterpret_cast<FColor*>(Img.RawData.GetData());
		// Flip vertically on copy — same reason as the anim pose capture: GPU origin is
		// bottom-left, PNG wants top-left.
		const FColor* Src = Pixels.GetData();
		for (int32 Y = 0; Y < Height; ++Y)
		{
			FMemory::Memcpy(
				Dst + Y * Width,
				Src + (Height - 1 - Y) * Width,
				Width * sizeof(FColor));
		}

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

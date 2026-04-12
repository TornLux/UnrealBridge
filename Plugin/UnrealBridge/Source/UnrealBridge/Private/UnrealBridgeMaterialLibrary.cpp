#include "UnrealBridgeMaterialLibrary.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "VT/RuntimeVirtualTexture.h"

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

	// Scalar parameters
	for (const FScalarParameterValue& P : MI->ScalarParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("Scalar");
		Param.Value = FString::SanitizeFloat(P.ParameterValue);
		Result.Parameters.Add(Param);
	}

	// Vector parameters
	for (const FVectorParameterValue& P : MI->VectorParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("Vector");
		Param.Value = FString::Printf(TEXT("(R=%.4f,G=%.4f,B=%.4f,A=%.4f)"),
			P.ParameterValue.R, P.ParameterValue.G, P.ParameterValue.B, P.ParameterValue.A);
		Result.Parameters.Add(Param);
	}

	// DoubleVector parameters
	for (const FDoubleVectorParameterValue& P : MI->DoubleVectorParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("DoubleVector");
		Param.Value = P.ParameterValue.ToString();
		Result.Parameters.Add(Param);
	}

	// Texture parameters
	for (const FTextureParameterValue& P : MI->TextureParameterValues)
	{
		FBridgeMaterialParam Param;
		Param.Name = P.ParameterInfo.Name.ToString();
		Param.ParamType = TEXT("Texture");
		Param.Value = P.ParameterValue ? P.ParameterValue->GetPathName() : TEXT("None");
		Result.Parameters.Add(Param);
	}

	// RuntimeVirtualTexture parameters
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

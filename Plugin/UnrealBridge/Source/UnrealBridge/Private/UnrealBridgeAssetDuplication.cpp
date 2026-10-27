#include "UnrealBridgeAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/World.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

FBridgeAssetDuplicateResult UUnrealBridgeAssetLibrary::DuplicateAsset(
	const FString& SourceAssetPath, const FString& DestinationAssetPath, const bool bSave)
{
	FBridgeAssetDuplicateResult Out;
	if (!GEditor || GEditor->PlayWorld)
	{
		Out.Error = TEXT("Duplication requires an editor world outside PIE");
		return Out;
	}
	const FString PackageName = FPackageName::ObjectPathToPackageName(
		FPackageName::ExportTextPathToObjectPath(DestinationAssetPath));
	FString Filename;
	if (!PackageName.StartsWith(TEXT("/Game/"))
		|| !FPackageName::IsValidLongPackageName(PackageName)
		|| !FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename)
		|| !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Filename), FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir())))
	{
		Out.Error = TEXT("Destination must be a valid package under this project's /Game content root");
		return Out;
	}
	if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName))
	{
		Out.Error = TEXT("Destination already exists; duplication never overwrites content");
		return Out;
	}
	UObject* Source = LoadObject<UObject>(nullptr, *FPackageName::ExportTextPathToObjectPath(SourceAssetPath));
	if (!Source || !Source->IsAsset() || Source->IsA<UWorld>())
	{
		Out.Error = TEXT("Source must be a loadable non-world asset; use level save-as for maps");
		return Out;
	}
	FScopedTransaction Transaction(NSLOCTEXT("UnrealBridgeAsset", "DuplicateAsset", "UnrealBridge: Duplicate Asset"));
	UObject* Copy = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(
		FPackageName::GetLongPackageAssetName(PackageName), FPackageName::GetLongPackagePath(PackageName), Source);
	if (!Copy)
	{
		Transaction.Cancel();
		Out.Error = TEXT("Native AssetTools duplication failed");
		return Out;
	}
	Out.bCreated = true;
	Out.AssetPath = Copy->GetPathName();
	if (bSave)
	{
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Copy->GetOutermost() }, false))
		{
			Out.Error = TEXT("Copy was created but saving failed; the new package remains in memory");
			return Out;
		}
		Out.bSaved = true;
	}
	Out.bSuccess = true;
	return Out;
}

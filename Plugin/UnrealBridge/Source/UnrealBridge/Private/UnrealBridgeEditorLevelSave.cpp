#include "UnrealBridgeEditorLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

bool UUnrealBridgeEditorLibrary::SaveCurrentLevelAs(const FString& AssetPath, bool bOverwrite)
{
	UWorld* World = GEditor && !GEditor->PlayWorld ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !World->PersistentLevel || !FPackageName::IsValidLongPackageName(AssetPath) || AssetPath.Contains(TEXT(".")) || AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/Temp/")) || AssetPath.StartsWith(TEXT("/Script/"))) return false;
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(AssetPath, Filename, FPackageName::GetMapPackageExtension())) return false;
	if (!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Filename), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()))) return false;
	if (!bOverwrite && (FPackageName::DoesPackageExist(AssetPath) || IFileManager::Get().FileExists(*Filename)))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SaveCurrentLevelAs refused existing target: %s"), *AssetPath);
		return false;
	}
	return FEditorFileUtils::SaveLevel(World->PersistentLevel, Filename);
}

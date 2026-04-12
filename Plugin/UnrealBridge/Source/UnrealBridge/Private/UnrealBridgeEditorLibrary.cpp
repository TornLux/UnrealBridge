#include "UnrealBridgeEditorLibrary.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Selection.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserItemPath.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "GameFramework/Actor.h"
#include "PlayInEditorDataTypes.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "Engine/ObjectLibrary.h"
#include "UObject/ObjectRedirector.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeEditor"

namespace
{
	UWorld* GetEditorWorld()
	{
		if (GEditor)
		{
			return GEditor->GetEditorWorldContext().World();
		}
		return nullptr;
	}

	FLevelEditorViewportClient* GetActiveViewportClient()
	{
		if (!FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			return nullptr;
		}
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<SLevelViewport> Viewport = LE.GetFirstActiveLevelViewport();
		if (!Viewport.IsValid())
		{
			return nullptr;
		}
		return &Viewport->GetLevelViewportClient();
	}
}

// ─── State ──────────────────────────────────────────────────

FString UUnrealBridgeEditorLibrary::GetEngineVersion()
{
	return FEngineVersion::Current().ToString();
}

bool UUnrealBridgeEditorLibrary::IsInPIE()
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

bool UUnrealBridgeEditorLibrary::IsPlayInEditorPaused()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return false;
	}
	return GEditor->PlayWorld->bDebugPauseExecution;
}

FBridgeEditorState UUnrealBridgeEditorLibrary::GetEditorState()
{
	FBridgeEditorState S;
	S.EngineVersion = FEngineVersion::Current().ToString();
	S.ProjectName = FApp::GetProjectName();
	S.bIsPIE = IsInPIE();
	S.bIsPaused = IsPlayInEditorPaused();

	if (UWorld* World = GetEditorWorld())
	{
		if (UPackage* Pkg = World->GetOutermost())
		{
			S.CurrentLevelPath = Pkg->GetName();
		}
	}

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			S.NumOpenedAssets = AES->GetAllEditedAssets().Num();
		}
		S.NumSelectedActors = GEditor->GetSelectedActorCount();
	}

	if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> Sel;
		CBM.Get().GetSelectedAssets(Sel);
		S.NumContentBrowserSelection = Sel.Num();
	}
	return S;
}

TArray<FBridgeOpenedAsset> UUnrealBridgeEditorLibrary::GetOpenedAssets()
{
	TArray<FBridgeOpenedAsset> Out;
	if (!GEditor)
	{
		return Out;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return Out;
	}
	for (UObject* O : AES->GetAllEditedAssets())
	{
		if (!O)
		{
			continue;
		}
		FBridgeOpenedAsset A;
		A.Path = O->GetPathName();
		A.ClassName = O->GetClass()->GetName();
		if (UPackage* Pkg = O->GetOutermost())
		{
			A.bIsDirty = Pkg->IsDirty();
		}
		Out.Add(A);
	}
	return Out;
}

TArray<FString> UUnrealBridgeEditorLibrary::GetContentBrowserSelection()
{
	TArray<FString> Out;
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return Out;
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> Sel;
	CBM.Get().GetSelectedAssets(Sel);
	for (const FAssetData& AD : Sel)
	{
		Out.Add(AD.GetObjectPathString());
	}
	return Out;
}

FString UUnrealBridgeEditorLibrary::GetContentBrowserPath()
{
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return FString();
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	return CBM.Get().GetCurrentPath().GetInternalPathString();
}

FBridgeViewportCamera UUnrealBridgeEditorLibrary::GetEditorViewportCamera()
{
	FBridgeViewportCamera Cam;
	if (FLevelEditorViewportClient* VC = GetActiveViewportClient())
	{
		Cam.Location = VC->GetViewLocation();
		Cam.Rotation = VC->GetViewRotation();
		Cam.FOV = VC->ViewFOV;
	}
	return Cam;
}

// ─── Asset control ──────────────────────────────────────────

namespace
{
	UObject* LoadAssetFromPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		if (UObject* Found = FindObject<UObject>(nullptr, *AssetPath))
		{
			return Found;
		}
		return LoadObject<UObject>(nullptr, *AssetPath);
	}
}

bool UUnrealBridgeEditorLibrary::OpenAsset(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* A = LoadAssetFromPath(AssetPath);
	if (!A)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: OpenAsset could not load '%s'"), *AssetPath);
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return AES && AES->OpenEditorForAsset(A);
}

bool UUnrealBridgeEditorLibrary::CloseAllAssetEditors()
{
	if (!GEditor)
	{
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return false;
	}
	AES->CloseAllAssetEditors();
	return true;
}

bool UUnrealBridgeEditorLibrary::SaveAsset(const FString& AssetPath)
{
	UObject* A = LoadAssetFromPath(AssetPath);
	if (!A)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: SaveAsset could not load '%s'"), *AssetPath);
		return false;
	}
	UPackage* Pkg = A->GetOutermost();
	if (!Pkg)
	{
		return false;
	}
	return UEditorLoadingAndSavingUtils::SavePackages({ Pkg }, false);
}

bool UUnrealBridgeEditorLibrary::SaveAllDirtyAssets(bool bIncludeMaps)
{
	return UEditorLoadingAndSavingUtils::SaveDirtyPackages(bIncludeMaps, true);
}

bool UUnrealBridgeEditorLibrary::SaveCurrentLevel()
{
	return FEditorFileUtils::SaveCurrentLevel();
}

bool UUnrealBridgeEditorLibrary::ReloadAsset(const FString& AssetPath)
{
	UObject* A = LoadAssetFromPath(AssetPath);
	if (!A)
	{
		return false;
	}
	UPackage* Pkg = A->GetOutermost();
	if (!Pkg)
	{
		return false;
	}
	return UPackageTools::ReloadPackages({ Pkg });
}

bool UUnrealBridgeEditorLibrary::SetContentBrowserSelection(const TArray<FString>& AssetPaths)
{
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return false;
	}
	TArray<FAssetData> ADs;
	for (const FString& P : AssetPaths)
	{
		UObject* O = LoadAssetFromPath(P);
		if (O)
		{
			ADs.Emplace(O);
		}
	}
	if (ADs.Num() == 0)
	{
		return false;
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	CBM.Get().SyncBrowserToAssets(ADs);
	return true;
}

bool UUnrealBridgeEditorLibrary::SyncContentBrowserToAsset(const FString& AssetPath)
{
	TArray<FString> One;
	One.Add(AssetPath);
	return SetContentBrowserSelection(One);
}

bool UUnrealBridgeEditorLibrary::FocusViewportOnSelection()
{
	if (!GEditor)
	{
		return false;
	}
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel || Sel->Num() == 0)
	{
		return false;
	}
	AActor* First = nullptr;
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AActor* A = Cast<AActor>(*It))
		{
			First = A;
			break;
		}
	}
	if (!First)
	{
		return false;
	}
	GEditor->MoveViewportCamerasToActor(*First, true);
	return true;
}

bool UUnrealBridgeEditorLibrary::SetEditorViewportCamera(FVector Location, FRotator Rotation, float FOV)
{
	FLevelEditorViewportClient* VC = GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	VC->SetViewLocation(Location);
	VC->SetViewRotation(Rotation);
	if (FOV > 0.f)
	{
		VC->ViewFOV = FOV;
	}
	VC->Invalidate();
	return true;
}

// ─── PIE ────────────────────────────────────────────────────

bool UUnrealBridgeEditorLibrary::StartPIE()
{
	if (!GEditor)
	{
		return false;
	}
	if (GEditor->PlayWorld)
	{
		// Already in PIE.
		return true;
	}
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;

	// Route the session into the currently-focused level viewport so the user's
	// "Selected Viewport" play-mode preference is honored (without this UE falls
	// back to a new window).
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<IAssetViewport> ActiveViewport = LE.GetFirstActiveViewport();
		if (ActiveViewport.IsValid())
		{
			Params.DestinationSlateViewport = ActiveViewport;
		}
	}

	GEditor->RequestPlaySession(Params);
	return true;
}

bool UUnrealBridgeEditorLibrary::StopPIE()
{
	if (!GEditor)
	{
		return false;
	}
	GEditor->RequestEndPlayMap();
	return true;
}

bool UUnrealBridgeEditorLibrary::PausePIE(bool bPaused)
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return false;
	}
	return GEditor->SetPIEWorldsPaused(bPaused);
}

// ─── Undo ───────────────────────────────────────────────────

bool UUnrealBridgeEditorLibrary::Undo()
{
	if (!GEditor)
	{
		return false;
	}
	return GEditor->UndoTransaction();
}

bool UUnrealBridgeEditorLibrary::Redo()
{
	if (!GEditor)
	{
		return false;
	}
	return GEditor->RedoTransaction();
}

// ─── Console / CVar ─────────────────────────────────────────

namespace
{
	class FCaptureDevice : public FOutputDevice
	{
	public:
		FString Output;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override
		{
			Output += V;
			Output += TEXT("\n");
		}
	};
}

FString UUnrealBridgeEditorLibrary::ExecuteConsoleCommand(const FString& Command)
{
	FCaptureDevice Dev;
	GLog->AddOutputDevice(&Dev);
	if (GEngine)
	{
		GEngine->Exec(GetEditorWorld(), *Command, Dev);
	}
	GLog->RemoveOutputDevice(&Dev);
	return Dev.Output;
}

FString UUnrealBridgeEditorLibrary::GetCVar(const FString& Name)
{
	IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CV)
	{
		return FString();
	}
	return CV->GetString();
}

bool UUnrealBridgeEditorLibrary::SetCVar(const FString& Name, const FString& Value)
{
	IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CV)
	{
		return false;
	}
	CV->Set(*Value, ECVF_SetByConsole);
	return true;
}

// ─── Utility ────────────────────────────────────────────────

int32 UUnrealBridgeEditorLibrary::FixupRedirectors(const TArray<FString>& Paths)
{
	if (Paths.Num() == 0)
	{
		return 0;
	}
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AR = ARM.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	for (const FString& P : Paths)
	{
		Filter.PackagePaths.Add(*P);
	}

	TArray<FAssetData> Found;
	AR.GetAssets(Filter, Found);

	TArray<UObjectRedirector*> Redirectors;
	Redirectors.Reserve(Found.Num());
	for (const FAssetData& AD : Found)
	{
		if (UObjectRedirector* R = Cast<UObjectRedirector>(AD.GetAsset()))
		{
			Redirectors.Add(R);
		}
	}
	if (Redirectors.Num() == 0)
	{
		return 0;
	}

	FAssetToolsModule& ATM = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	ATM.Get().FixupReferencers(Redirectors, /*bCheckoutDialogPrompt=*/false);
	return Redirectors.Num();
}

TArray<FBridgeCompileResult> UUnrealBridgeEditorLibrary::CompileBlueprints(const TArray<FString>& BlueprintPaths)
{
	TArray<FBridgeCompileResult> Out;
	for (const FString& P : BlueprintPaths)
	{
		FBridgeCompileResult R;
		R.Path = P;
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *P);
		if (!BP)
		{
			R.ErrorMessage = TEXT("Blueprint not found");
			Out.Add(R);
			continue;
		}
		FKismetEditorUtilities::CompileBlueprint(BP);
		switch (BP->Status)
		{
		case BS_UpToDate:
		case BS_UpToDateWithWarnings:
			R.bSuccess = true;
			break;
		case BS_Error:
			R.bSuccess = false;
			R.ErrorMessage = TEXT("Compile error");
			break;
		case BS_Unknown:
		case BS_Dirty:
		default:
			R.bSuccess = false;
			R.ErrorMessage = FString::Printf(TEXT("Status=%d"), (int32)BP->Status);
			break;
		}
		Out.Add(R);
	}
	return Out;
}

TArray<FString> UUnrealBridgeEditorLibrary::ListCVars(const FString& Keyword)
{
	TArray<FString> Out;
	IConsoleManager::Get().ForEachConsoleObjectThatContains(
		FConsoleObjectVisitor::CreateLambda([&Out](const TCHAR* CVName, IConsoleObject* Obj)
		{
			if (Obj)
			{
				if (IConsoleVariable* CV = Obj->AsVariable())
				{
					Out.Add(FString::Printf(TEXT("%s = %s"), CVName, *CV->GetString()));
				}
			}
		}),
		*Keyword);
	return Out;
}

#undef LOCTEXT_NAMESPACE

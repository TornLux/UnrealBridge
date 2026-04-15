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
#include "Misc/PackageName.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "HighResScreenshot.h"
#include "ShowFlags.h"
#include "Engine/EngineBaseTypes.h"
#include "Editor/UnrealEdTypes.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeEditor"

namespace BridgeEditorImpl
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

	if (UWorld* World = BridgeEditorImpl::GetEditorWorld())
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
	if (FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient())
	{
		Cam.Location = VC->GetViewLocation();
		Cam.Rotation = VC->GetViewRotation();
		Cam.FOV = VC->ViewFOV;
	}
	return Cam;
}

// ─── Asset control ──────────────────────────────────────────

namespace BridgeEditorImpl
{
	UObject* ResolveInnerAssetIfPackage(UObject* Obj)
	{
		UPackage* Pkg = Cast<UPackage>(Obj);
		if (!Pkg)
		{
			return Obj;
		}
		const FString LeafName = FPackageName::GetShortName(Pkg->GetFName());
		UObject* Match = nullptr;
		UObject* Fallback = nullptr;
		ForEachObjectWithOuter(Pkg, [&](UObject* Inner)
		{
			if (Match || !Inner || Inner->HasAnyFlags(RF_Transient))
			{
				return;
			}
			if (Inner->GetName() == LeafName)
			{
				Match = Inner;
			}
			else if (!Fallback && Inner->IsAsset())
			{
				Fallback = Inner;
			}
		}, false);
		if (Match)
		{
			return Match;
		}
		return Fallback ? Fallback : Obj;
	}

	UObject* LoadAssetFromPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		// Normalize "/Foo/Bar" → "/Foo/Bar.Bar" so we load the inner asset, not the UPackage wrapper.
		FString Normalized = AssetPath;
		int32 DotIdx;
		if (!Normalized.FindChar(TEXT('.'), DotIdx))
		{
			int32 SlashIdx;
			if (Normalized.FindLastChar(TEXT('/'), SlashIdx) && SlashIdx + 1 < Normalized.Len())
			{
				const FString Leaf = Normalized.Mid(SlashIdx + 1);
				Normalized = Normalized + TEXT(".") + Leaf;
			}
		}
		UObject* Result = FindObject<UObject>(nullptr, *Normalized);
		if (!Result)
		{
			Result = LoadObject<UObject>(nullptr, *Normalized);
		}
		return ResolveInnerAssetIfPackage(Result);
	}
}

bool UUnrealBridgeEditorLibrary::OpenAsset(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* A = BridgeEditorImpl::LoadAssetFromPath(AssetPath);
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
	UObject* A = BridgeEditorImpl::LoadAssetFromPath(AssetPath);
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
	UObject* A = BridgeEditorImpl::LoadAssetFromPath(AssetPath);
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
		UObject* O = BridgeEditorImpl::LoadAssetFromPath(P);
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
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
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

bool UUnrealBridgeEditorLibrary::StartSimulate()
{
	if (!GEditor)
	{
		return false;
	}
	if (GEditor->PlayWorld)
	{
		// Already playing — can't switch mode without stopping first.
		return true;
	}
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::SimulateInEditor;

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

bool UUnrealBridgeEditorLibrary::IsSimulating()
{
	return GEditor && GEditor->bIsSimulatingInEditor;
}

FString UUnrealBridgeEditorLibrary::GetPIENetMode()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return FString();
	}
	switch (GEditor->PlayWorld->GetNetMode())
	{
	case NM_Standalone:      return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer:    return TEXT("ListenServer");
	case NM_Client:          return TEXT("Client");
	default:                 return TEXT("Standalone");
	}
}

float UUnrealBridgeEditorLibrary::GetPIEWorldTime()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return -1.0f;
	}
	return GEditor->PlayWorld->GetTimeSeconds();
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

namespace BridgeEditorImpl
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
	BridgeEditorImpl::FCaptureDevice Dev;
	GLog->AddOutputDevice(&Dev);
	if (GEngine)
	{
		GEngine->Exec(BridgeEditorImpl::GetEditorWorld(), *Command, Dev);
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

// ─── Dirty-state tracking ──────────────────────────────────

namespace BridgeEditorImpl
{
	UPackage* FindPackageForAssetPath(const FString& AssetPath)
	{
		UObject* Asset = BridgeEditorImpl::LoadAssetFromPath(AssetPath);
		return Asset ? Asset->GetPackage() : nullptr;
	}
}

TArray<FString> UUnrealBridgeEditorLibrary::GetDirtyPackageNames()
{
	TArray<FString> Out;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Pkg = *It;
		if (Pkg && Pkg->IsDirty())
		{
			const FString Name = Pkg->GetName();
			// Skip script packages and engine transient buckets; keep /Game/ and plugin mounts.
			if (!Name.StartsWith(TEXT("/Script/")) && !Name.StartsWith(TEXT("/Temp/")) && Name != TEXT("/Engine/Transient"))
			{
				Out.Add(Name);
			}
		}
	}
	Out.Sort();
	return Out;
}

bool UUnrealBridgeEditorLibrary::IsAssetDirty(const FString& AssetPath)
{
	const UPackage* Pkg = BridgeEditorImpl::FindPackageForAssetPath(AssetPath);
	return Pkg && Pkg->IsDirty();
}

bool UUnrealBridgeEditorLibrary::MarkAssetDirty(const FString& AssetPath)
{
	UPackage* Pkg = BridgeEditorImpl::FindPackageForAssetPath(AssetPath);
	if (!Pkg)
	{
		return false;
	}
	Pkg->MarkPackageDirty();
	return true;
}

bool UUnrealBridgeEditorLibrary::IsAssetEditorOpen(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* Asset = BridgeEditorImpl::LoadAssetFromPath(AssetPath);
	if (!Asset)
	{
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return false;
	}
	return AES->FindEditorForAsset(Asset, /*bFocusIfOpen=*/false) != nullptr;
}

int32 UUnrealBridgeEditorLibrary::SaveAssets(const TArray<FString>& AssetPaths)
{
	TArray<UPackage*> Packages;
	Packages.Reserve(AssetPaths.Num());
	for (const FString& P : AssetPaths)
	{
		if (UPackage* Pkg = BridgeEditorImpl::FindPackageForAssetPath(P))
		{
			Packages.AddUnique(Pkg);
		}
	}
	if (Packages.Num() == 0)
	{
		return 0;
	}
	const FEditorFileUtils::EPromptReturnCode Ret = FEditorFileUtils::PromptForCheckoutAndSave(
		Packages,
		/*bCheckDirty=*/false,
		/*bPromptToSave=*/false);
	if (Ret != FEditorFileUtils::PR_Success && Ret != FEditorFileUtils::PR_Declined)
	{
		return 0;
	}
	int32 Saved = 0;
	for (UPackage* Pkg : Packages)
	{
		if (Pkg && !Pkg->IsDirty())
		{
			++Saved;
		}
	}
	return Saved;
}

// ─── Level / map control ───────────────────────────────────

bool UUnrealBridgeEditorLibrary::LoadLevel(const FString& LevelPath, bool bPromptSaveChanges)
{
	if (LevelPath.IsEmpty())
	{
		return false;
	}
	if (bPromptSaveChanges)
	{
		if (!FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave=*/true, /*bSaveMapPackages=*/true, /*bSaveContentPackages=*/false))
		{
			// User cancelled save prompt.
			return false;
		}
	}
	return UEditorLoadingAndSavingUtils::LoadMap(LevelPath) != nullptr;
}

bool UUnrealBridgeEditorLibrary::CreateNewLevel(bool bSaveExisting)
{
	return UEditorLoadingAndSavingUtils::NewBlankMap(bSaveExisting) != nullptr;
}

// ─── Asset editor / browser extras ─────────────────────────

bool UUnrealBridgeEditorLibrary::CloseAssetEditor(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* Asset = BridgeEditorImpl::LoadAssetFromPath(AssetPath);
	if (!Asset)
	{
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return false;
	}
	if (AES->FindEditorForAsset(Asset, /*bFocusIfOpen=*/false) == nullptr)
	{
		return false;
	}
	AES->CloseAllEditorsForAsset(Asset);
	return true;
}

bool UUnrealBridgeEditorLibrary::IsAssetLoaded(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}
	return FindObject<UObject>(nullptr, *AssetPath) != nullptr;
}

bool UUnrealBridgeEditorLibrary::SetContentBrowserPath(const FString& FolderPath)
{
	if (FolderPath.IsEmpty())
	{
		return false;
	}
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return false;
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FString> Folders;
	Folders.Add(FolderPath);
	CBM.Get().SyncBrowserToFolders(Folders);
	return true;
}

// ─── Viewport capture ──────────────────────────────────────

bool UUnrealBridgeEditorLibrary::TakeHighResScreenshot(float ResolutionMultiplier)
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		return false;
	}
	const float Mult = ResolutionMultiplier > 0.f ? ResolutionMultiplier : 1.f;
	GetHighResScreenshotConfig().SetResolution(
		FMath::Max(1, (int32)(VC->Viewport->GetSizeXY().X * Mult)),
		FMath::Max(1, (int32)(VC->Viewport->GetSizeXY().Y * Mult)),
		1.f);
	VC->Viewport->TakeHighResScreenShot();
	return true;
}

// ─── Viewport render / display ─────────────────────────────

namespace BridgeEditorImpl
{
	EViewModeIndex ParseViewMode(const FString& Mode)
	{
		const FString M = Mode.TrimStartAndEnd();
		if (M.Equals(TEXT("Lit"), ESearchCase::IgnoreCase)) return VMI_Lit;
		if (M.Equals(TEXT("Unlit"), ESearchCase::IgnoreCase)) return VMI_Unlit;
		if (M.Equals(TEXT("Wireframe"), ESearchCase::IgnoreCase)) return VMI_BrushWireframe;
		if (M.Equals(TEXT("BrushWireframe"), ESearchCase::IgnoreCase)) return VMI_BrushWireframe;
		if (M.Equals(TEXT("CSGWireframe"), ESearchCase::IgnoreCase)) return VMI_Wireframe;
		if (M.Equals(TEXT("DetailLighting"), ESearchCase::IgnoreCase)) return VMI_Lit_DetailLighting;
		if (M.Equals(TEXT("LightingOnly"), ESearchCase::IgnoreCase)) return VMI_LightingOnly;
		if (M.Equals(TEXT("LightComplexity"), ESearchCase::IgnoreCase)) return VMI_LightComplexity;
		if (M.Equals(TEXT("ShaderComplexity"), ESearchCase::IgnoreCase)) return VMI_ShaderComplexity;
		if (M.Equals(TEXT("LightmapDensity"), ESearchCase::IgnoreCase)) return VMI_LightmapDensity;
		if (M.Equals(TEXT("ReflectionOverride"), ESearchCase::IgnoreCase)) return VMI_ReflectionOverride;
		if (M.Equals(TEXT("CollisionPawn"), ESearchCase::IgnoreCase)) return VMI_CollisionPawn;
		if (M.Equals(TEXT("CollisionVisibility"), ESearchCase::IgnoreCase)) return VMI_CollisionVisibility;
		if (M.Equals(TEXT("LODColoration"), ESearchCase::IgnoreCase)) return VMI_LODColoration;
		if (M.Equals(TEXT("QuadOverdraw"), ESearchCase::IgnoreCase)) return VMI_QuadOverdraw;
		return VMI_Unknown;
	}

	FString ViewModeToString(EViewModeIndex Mode)
	{
		switch (Mode)
		{
		case VMI_Lit: return TEXT("Lit");
		case VMI_Unlit: return TEXT("Unlit");
		case VMI_BrushWireframe: return TEXT("Wireframe");
		case VMI_Wireframe: return TEXT("CSGWireframe");
		case VMI_Lit_DetailLighting: return TEXT("DetailLighting");
		case VMI_LightingOnly: return TEXT("LightingOnly");
		case VMI_LightComplexity: return TEXT("LightComplexity");
		case VMI_ShaderComplexity: return TEXT("ShaderComplexity");
		case VMI_LightmapDensity: return TEXT("LightmapDensity");
		case VMI_ReflectionOverride: return TEXT("ReflectionOverride");
		case VMI_CollisionPawn: return TEXT("CollisionPawn");
		case VMI_CollisionVisibility: return TEXT("CollisionVisibility");
		case VMI_LODColoration: return TEXT("LODColoration");
		case VMI_QuadOverdraw: return TEXT("QuadOverdraw");
		default: return FString::Printf(TEXT("VMI_%d"), (int32)Mode);
		}
	}

	bool ParseViewportType(const FString& Type, ELevelViewportType& Out)
	{
		const FString T = Type.TrimStartAndEnd();
		if (T.Equals(TEXT("Perspective"), ESearchCase::IgnoreCase)) { Out = LVT_Perspective; return true; }
		if (T.Equals(TEXT("Top"), ESearchCase::IgnoreCase) || T.Equals(TEXT("OrthoXY"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoXY; return true; }
		if (T.Equals(TEXT("Front"), ESearchCase::IgnoreCase) || T.Equals(TEXT("OrthoXZ"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoXZ; return true; }
		if (T.Equals(TEXT("Side"), ESearchCase::IgnoreCase) || T.Equals(TEXT("OrthoYZ"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoYZ; return true; }
		if (T.Equals(TEXT("OrthoFreelook"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoFreelook; return true; }
		return false;
	}

	FString ViewportTypeToString(ELevelViewportType Type)
	{
		switch (Type)
		{
		case LVT_Perspective: return TEXT("Perspective");
		case LVT_OrthoXY: return TEXT("Top");
		case LVT_OrthoXZ: return TEXT("Front");
		case LVT_OrthoYZ: return TEXT("Side");
		case LVT_OrthoFreelook: return TEXT("OrthoFreelook");
		default: return TEXT("Unknown");
		}
	}
}

bool UUnrealBridgeEditorLibrary::SetViewportRealtime(bool bRealtime)
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	VC->SetRealtime(bRealtime);
	VC->Invalidate();
	return true;
}

bool UUnrealBridgeEditorLibrary::IsViewportRealtime()
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	return VC && VC->IsRealtime();
}

FVector2D UUnrealBridgeEditorLibrary::GetViewportSize()
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		return FVector2D::ZeroVector;
	}
	const FIntPoint S = VC->Viewport->GetSizeXY();
	return FVector2D(S.X, S.Y);
}

bool UUnrealBridgeEditorLibrary::SetViewportViewMode(const FString& Mode)
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	const EViewModeIndex Parsed = BridgeEditorImpl::ParseViewMode(Mode);
	if (Parsed == VMI_Unknown)
	{
		return false;
	}
	VC->SetViewMode(Parsed);
	VC->Invalidate();
	return true;
}

FString UUnrealBridgeEditorLibrary::GetViewportViewMode()
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return FString();
	}
	return BridgeEditorImpl::ViewModeToString(VC->GetViewMode());
}

bool UUnrealBridgeEditorLibrary::SetViewportShowFlag(const FString& FlagName, bool bEnabled)
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	const int32 Index = FEngineShowFlags::FindIndexByName(*FlagName);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	VC->EngineShowFlags.SetSingleFlag((uint32)Index, bEnabled);
	VC->Invalidate();
	return true;
}

bool UUnrealBridgeEditorLibrary::GetViewportShowFlag(const FString& FlagName)
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	const int32 Index = FEngineShowFlags::FindIndexByName(*FlagName);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	return VC->EngineShowFlags.GetSingleFlag((uint32)Index);
}

bool UUnrealBridgeEditorLibrary::SetViewportType(const FString& ViewportType)
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	ELevelViewportType Parsed;
	if (!BridgeEditorImpl::ParseViewportType(ViewportType, Parsed))
	{
		return false;
	}
	VC->SetViewportType(Parsed);
	VC->Invalidate();
	return true;
}

FString UUnrealBridgeEditorLibrary::GetViewportType()
{
	FLevelEditorViewportClient* VC = BridgeEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return FString();
	}
	return BridgeEditorImpl::ViewportTypeToString(VC->GetViewportType());
}

#undef LOCTEXT_NAMESPACE

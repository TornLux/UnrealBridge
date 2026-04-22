// Ported from UnrealClientProtocol (MIT License - Italink)

#include "UnrealBridgeAssetLibrary.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "UObject/ObjectRedirector.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/TopLevelAssetPath.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PixelFormat.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

// ─── Internal helpers ───────────────────────────────────────

namespace BridgeAssetOps
{
	/** Strip surrounding single quotes from export-text paths. */
	void ParsePathToObjectPath(const FString& InPath, FString& OutObjectPath)
	{
		OutObjectPath = InPath.TrimStartAndEnd();
		int32 QuoteStart = INDEX_NONE;
		if (OutObjectPath.FindChar(TEXT('\''), QuoteStart))
		{
			int32 QuoteEnd = INDEX_NONE;
			if (OutObjectPath.FindLastChar(TEXT('\''), QuoteEnd) && QuoteEnd > QuoteStart)
			{
				OutObjectPath = OutObjectPath.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
			}
		}
	}

	/** Resolve a Blueprint asset path to its GeneratedClass. */
	UClass* ResolveBlueprintPathToClass(const FString& BlueprintClassPath)
	{
		FString ObjectPath;
		ParsePathToObjectPath(BlueprintClassPath, ObjectPath);

		UClass* BaseClass = nullptr;
		UBlueprint* LoadedBP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *ObjectPath));
		if (LoadedBP && LoadedBP->GeneratedClass)
		{
			BaseClass = LoadedBP->GeneratedClass;
		}
		if (!BaseClass)
		{
			BaseClass = FindObject<UClass>(nullptr, *ObjectPath);
		}
		if (!BaseClass && !ObjectPath.EndsWith(TEXT("_C")))
		{
			BaseClass = FindObject<UClass>(nullptr, *(ObjectPath + TEXT("_C")));
		}
		if (!BaseClass && !ObjectPath.EndsWith(TEXT("_C")))
		{
			BaseClass = LoadObject<UClass>(nullptr, *(ObjectPath + TEXT("_C")));
		}
		return BaseClass;
	}

	/** Convert an FAssetIdentifier to FSoftObjectPath. */
	FSoftObjectPath ConvertAssetIdentifierToSoftObjectPath(const FAssetIdentifier& AssetIdentifier)
	{
		if (!AssetIdentifier.IsValid())
		{
			return FSoftObjectPath();
		}
		if (AssetIdentifier.PrimaryAssetType.IsValid())
		{
			return FSoftObjectPath(AssetIdentifier.ToString());
		}
		if (AssetIdentifier.IsPackage())
		{
			const FString PackageStr = AssetIdentifier.PackageName.ToString();
			const FString ShortName = FPackageName::GetShortName(AssetIdentifier.PackageName);
			return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *PackageStr, *ShortName));
		}
		return FSoftObjectPath(AssetIdentifier.ToString());
	}

	/** Gather derived class paths from AssetRegistry, skipping SKEL_/REINST_. */
	void GatherDerivedClassPaths(
		const TArray<UClass*>& BaseClasses,
		const TSet<UClass*>& ExcludedClasses,
		TSet<FTopLevelAssetPath>& OutDerivedClassPaths)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

		TArray<FTopLevelAssetPath> BaseClassPaths;
		for (const UClass* C : BaseClasses)
		{
			if (C) BaseClassPaths.Emplace(C->GetClassPathName());
		}

		TSet<FTopLevelAssetPath> ExcludedClassPaths;
		for (const UClass* C : ExcludedClasses)
		{
			if (C) ExcludedClassPaths.Emplace(C->GetClassPathName());
		}

		TSet<FTopLevelAssetPath> DerivedClassPaths;
		AssetRegistry.GetDerivedClassNames(BaseClassPaths, ExcludedClassPaths, DerivedClassPaths);

		for (const FTopLevelAssetPath& Path : DerivedClassPaths)
		{
			FString AssetName = Path.GetAssetName().ToString();
			if (!AssetName.StartsWith(TEXT("SKEL_")) && !AssetName.StartsWith(TEXT("REINST_")))
			{
				OutDerivedClassPaths.Add(Path);
			}
		}
	}

	/** Normalize a content root path (trim, strip trailing slashes, ensure leading /). */
	void NormalizeContentRoot(FString& Path)
	{
		Path.TrimStartAndEndInline();
		while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
		{
			Path.LeftChopInline(1);
		}
		if (!Path.IsEmpty() && !Path.StartsWith(TEXT("/")))
		{
			Path = TEXT("/") + Path;
		}
	}
} // namespace BridgeAssetOps

// ─── Search query parser ────────────────────────────────────

namespace BridgeAssetSearch
{
	struct FParsedQuery
	{
		TArray<FString> IncludeTokens;
		TArray<FString> ExcludeTokens;
		FString TypeFilter;
	};

	static bool ParseQuery(const FString& Query, FParsedQuery& OutParsed)
	{
		OutParsed.IncludeTokens.Reset();
		OutParsed.ExcludeTokens.Reset();
		OutParsed.TypeFilter.Reset();

		FString Trimmed = Query.TrimStartAndEnd();
		TArray<FString> Tokens;
		Trimmed.ParseIntoArray(Tokens, TEXT(" "), true);

		for (FString& Token : Tokens)
		{
			Token.TrimStartAndEndInline();
			if (Token.IsEmpty()) continue;

			if (Token.StartsWith(TEXT("!")))
			{
				FString Exclude = Token.Mid(1).TrimStartAndEnd();
				if (!Exclude.IsEmpty())
					OutParsed.ExcludeTokens.Add(MoveTemp(Exclude));
			}
			else if (Token.StartsWith(TEXT("&Type="), ESearchCase::IgnoreCase))
			{
				OutParsed.TypeFilter = Token.Mid(6).TrimStartAndEnd();
			}
			else
			{
				OutParsed.IncludeTokens.Add(MoveTemp(Token));
			}
		}

		return OutParsed.IncludeTokens.Num() > 0 || !OutParsed.TypeFilter.IsEmpty();
	}

	static bool MatchesQuery(const FString& AssetName, const FString& Q, bool bCaseSensitive, bool bWholeWord)
	{
		FString Name = AssetName;
		FString Query = Q;
		if (!bCaseSensitive)
		{
			Name.ToLowerInline();
			Query.ToLowerInline();
		}
		if (bWholeWord)
		{
			int32 Idx = 0;
			while (Idx < Name.Len())
			{
				Idx = Name.Find(Query, ESearchCase::IgnoreCase, ESearchDir::FromStart, Idx);
				if (Idx == INDEX_NONE) return false;
				bool StartOK = (Idx == 0) || !FChar::IsAlnum(Name[Idx - 1]);
				bool EndOK = (Idx + Query.Len() >= Name.Len()) || !FChar::IsAlnum(Name[Idx + Query.Len()]);
				if (StartOK && EndOK) return true;
				Idx++;
			}
			return false;
		}
		return Name.Contains(Query);
	}

	static bool MatchesParsedQuery(const FString& AssetName, const FParsedQuery& Parsed, bool bCaseSensitive, bool bWholeWord)
	{
		for (const FString& Token : Parsed.IncludeTokens)
		{
			if (!Token.IsEmpty() && !MatchesQuery(AssetName, Token, bCaseSensitive, bWholeWord))
				return false;
		}
		for (const FString& Token : Parsed.ExcludeTokens)
		{
			if (!Token.IsEmpty() && MatchesQuery(AssetName, Token, bCaseSensitive, bWholeWord))
				return false;
		}
		return true;
	}

	static bool MatchesTypeFilter(const FAssetData& Data, const FString& TypeFilter)
	{
		if (TypeFilter.IsEmpty()) return true;
		FString ClassName = Data.AssetClassPath.GetAssetName().ToString();
		return ClassName.Contains(TypeFilter, ESearchCase::IgnoreCase);
	}

	static void SearchAssetsInternal(
		IAssetRegistry& Registry,
		const FParsedQuery& Parsed,
		EBridgeAssetSearchScope Scope,
		const FString& InCustomPackagePath,
		const FString& ClassFilter,
		bool bCaseSensitive, bool bWholeWord,
		int32 MaxResults,
		TArray<FAssetData>& OutResults)
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;

		switch (Scope)
		{
		case EBridgeAssetSearchScope::AllAssets:
		{
			TArray<FString> RootPaths;
			FPackageName::QueryRootContentPaths(RootPaths, false, false, true);
			for (const FString& Root : RootPaths)
			{
				FString Path = Root;
				if (!Path.StartsWith(TEXT("/"))) Path = TEXT("/") + Path;
				if (!Path.IsEmpty()) Filter.PackagePaths.Add(FName(*Path));
			}
			if (Filter.PackagePaths.Num() == 0)
			{
				Filter.PackagePaths.Add(FName("/Game"));
				Filter.PackagePaths.Add(FName("/Engine"));
			}
			break;
		}
		case EBridgeAssetSearchScope::Project:
			Filter.PackagePaths.Add(FName("/Game"));
			break;
		case EBridgeAssetSearchScope::CustomPackagePath:
		{
			FString Path = InCustomPackagePath;
			BridgeAssetOps::NormalizeContentRoot(Path);
			Filter.PackagePaths.Add(FName(Path.IsEmpty() ? TEXT("/Game") : *Path));
			break;
		}
		}

		if (!ClassFilter.IsEmpty() && ClassFilter != TEXT("*") && ClassFilter.StartsWith(TEXT("/Script/")))
		{
			Filter.ClassPaths.Add(FTopLevelAssetPath(ClassFilter));
			Filter.bRecursiveClasses = true;
		}

		TArray<FAssetData> AllCandidates;
		Registry.GetAssets(Filter, AllCandidates);

		for (const FAssetData& Data : AllCandidates)
		{
			if (OutResults.Num() >= MaxResults) break;
			FString AssetName = Data.AssetName.ToString();
			if (!MatchesParsedQuery(AssetName, Parsed, bCaseSensitive, bWholeWord)) continue;
			if (!MatchesTypeFilter(Data, Parsed.TypeFilter)) continue;
			OutResults.Add(Data);
		}
	}
} // namespace BridgeAssetSearch

// ═══════════════════════════════════════════════════════════
//  Asset Search
// ═══════════════════════════════════════════════════════════

void UUnrealBridgeAssetLibrary::SearchAssets(
	const FString& Query, EBridgeAssetSearchScope Scope, const FString& ClassFilter,
	bool bCaseSensitive, bool bWholeWord, int32 MaxResults, int32 MinCharacters,
	const FString& CustomPackagePath,
	TArray<FSoftObjectPath>& OutSoftPaths, TArray<FString>& OutIncludeTokensForHighlight)
{
	OutSoftPaths.Reset();
	OutIncludeTokensForHighlight.Reset();

	BridgeAssetSearch::FParsedQuery Parsed;
	if (!BridgeAssetSearch::ParseQuery(Query.TrimStartAndEnd(), Parsed)) return;
	if (Parsed.IncludeTokens.Num() == 0 && Parsed.TypeFilter.IsEmpty()) return;

	if (Parsed.IncludeTokens.Num() > 0)
	{
		int32 MinLen = Parsed.IncludeTokens[0].Len();
		for (const FString& T : Parsed.IncludeTokens)
			if (T.Len() < MinLen) MinLen = T.Len();
		if (MinLen < MinCharacters) return;
	}

	OutIncludeTokensForHighlight = Parsed.IncludeTokens;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Results;
	BridgeAssetSearch::SearchAssetsInternal(Registry, Parsed, Scope, CustomPackagePath, ClassFilter, bCaseSensitive, bWholeWord, MaxResults, Results);

	OutSoftPaths.Reserve(Results.Num());
	for (const FAssetData& Data : Results)
		OutSoftPaths.Add(Data.GetSoftObjectPath());
}

void UUnrealBridgeAssetLibrary::SearchAssetsInAllContent(
	const FString& Query, int32 MaxResults,
	TArray<FSoftObjectPath>& OutSoftPaths, TArray<FString>& OutIncludeTokensForHighlight)
{
	SearchAssets(Query, EBridgeAssetSearchScope::AllAssets, FString(),
		false, false, MaxResults, 1, FString(),
		OutSoftPaths, OutIncludeTokensForHighlight);
}

void UUnrealBridgeAssetLibrary::SearchAssetsUnderPath(
	const FString& ContentFolderPath, const FString& Query, int32 MaxResults,
	TArray<FSoftObjectPath>& OutSoftPaths, TArray<FString>& OutIncludeTokensForHighlight)
{
	OutSoftPaths.Reset();
	OutIncludeTokensForHighlight.Reset();
	if (ContentFolderPath.TrimStartAndEnd().IsEmpty()) return;

	SearchAssets(Query, EBridgeAssetSearchScope::CustomPackagePath, FString(),
		false, false, MaxResults, 1, ContentFolderPath,
		OutSoftPaths, OutIncludeTokensForHighlight);
}

// ═══════════════════════════════════════════════════════════
//  Derived Classes
// ═══════════════════════════════════════════════════════════

void UUnrealBridgeAssetLibrary::GetDerivedClasses(
	const TArray<UClass*>& BaseClasses, const TSet<UClass*>& ExcludedClasses,
	TSet<UClass*>& OutDerivedClasses)
{
	auto ShouldSkip = [](const UClass* InClass)
	{
		constexpr EClassFlags InvalidFlags = CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract | CLASS_NewerVersionExists;
		return InClass->HasAnyClassFlags(InvalidFlags)
			|| InClass->GetName().StartsWith(TEXT("SKEL_"))
			|| InClass->GetName().StartsWith(TEXT("REINST_"));
	};
	GetDerivedClassesWithFilter(BaseClasses, ExcludedClasses, OutDerivedClasses, ShouldSkip);
}

void UUnrealBridgeAssetLibrary::GetDerivedClassesWithFilter(
	const TArray<UClass*>& BaseClasses, const TSet<UClass*>& ExcludedClasses,
	TSet<UClass*>& OutDerivedClasses, TFunction<bool(const UClass*)> ShouldSkipClassFilter)
{
	TSet<FTopLevelAssetPath> DerivedClassPaths;
	BridgeAssetOps::GatherDerivedClassPaths(BaseClasses, ExcludedClasses, DerivedClassPaths);

	for (const FTopLevelAssetPath& Path : DerivedClassPaths)
	{
		UClass* Class = FindObject<UClass>(nullptr, *Path.ToString());
		if (!Class)
		{
			FString AssetPath = Path.ToString();
			if (AssetPath.EndsWith(TEXT("_C")))
			{
				AssetPath.LeftChopInline(2);
				UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
				if (Blueprint && Blueprint->GeneratedClass)
					Class = Blueprint->GeneratedClass;
			}
		}

		if (Class && !ShouldSkipClassFilter(Class))
		{
			OutDerivedClasses.Add(Class);
		}
	}
}

void UUnrealBridgeAssetLibrary::GetDerivedClassesByBlueprintPath(
	const FString& BlueprintClassPath, TArray<UClass*>& OutDerivedClasses)
{
	OutDerivedClasses.Reset();
	UClass* BaseClass = BridgeAssetOps::ResolveBlueprintPathToClass(BlueprintClassPath);
	if (!BaseClass) return;

	TArray<UClass*> BaseArr;
	BaseArr.Add(BaseClass);
	TSet<UClass*> DerivedSet;
	auto ShouldSkip = [](const UClass* InClass)
	{
		constexpr EClassFlags InvalidFlags = CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract | CLASS_NewerVersionExists;
		return InClass->HasAnyClassFlags(InvalidFlags)
			|| InClass->GetName().StartsWith(TEXT("SKEL_"))
			|| InClass->GetName().StartsWith(TEXT("REINST_"));
	};
	GetDerivedClassesWithFilter(BaseArr, TSet<UClass*>(), DerivedSet, ShouldSkip);

	for (UClass* C : DerivedSet)
		OutDerivedClasses.Add(C);
}

// ═══════════════════════════════════════════════════════════
//  Asset References
// ═══════════════════════════════════════════════════════════

void UUnrealBridgeAssetLibrary::GetAssetReferences(
	const FString& AssetPath,
	TArray<FSoftObjectPath>& OutDependencies, TArray<FSoftObjectPath>& OutReferencers)
{
	OutDependencies.Reset();
	OutReferencers.Reset();

	FString ObjectPath;
	BridgeAssetOps::ParsePathToObjectPath(AssetPath, ObjectPath);

	FSoftObjectPath SoftPath(ObjectPath);
	if (!SoftPath.IsValid()) return;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);
	if (!AssetData.IsValid()) return;

	FAssetIdentifier GraphId(AssetData.PackageName);

	TArray<FAssetIdentifier> Dependencies;
	TArray<FAssetIdentifier> Referencers;
	if (!AssetRegistry.GetDependencies(GraphId, Dependencies, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery()))
	{
		GraphId = FAssetIdentifier(AssetData.PackageName, AssetData.AssetName);
		AssetRegistry.GetDependencies(GraphId, Dependencies, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());
	}
	AssetRegistry.GetReferencers(GraphId, Referencers, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());

	TSet<FSoftObjectPath> SeenDeps, SeenRefs;
	for (const FAssetIdentifier& Id : Dependencies)
	{
		FSoftObjectPath SP = BridgeAssetOps::ConvertAssetIdentifierToSoftObjectPath(Id);
		if (SP.IsValid() && !SeenDeps.Contains(SP))
		{
			SeenDeps.Add(SP);
			OutDependencies.Add(SP);
		}
	}
	for (const FAssetIdentifier& Id : Referencers)
	{
		FSoftObjectPath SP = BridgeAssetOps::ConvertAssetIdentifierToSoftObjectPath(Id);
		if (SP.IsValid() && !SeenRefs.Contains(SP))
		{
			SeenRefs.Add(SP);
			OutReferencers.Add(SP);
		}
	}
}

// ═══════════════════════════════════════════════════════════
//  DataAsset Queries
// ═══════════════════════════════════════════════════════════

void UUnrealBridgeAssetLibrary::GetDataAssetsByBaseClass(
	TSubclassOf<UDataAsset> BaseDataAssetClass, TArray<FAssetData>& OutAssetDatas)
{
	OutAssetDatas.Reset();
	UClass* BaseClass = BaseDataAssetClass.Get();
	if (!BaseClass || !BaseClass->IsChildOf(UDataAsset::StaticClass())) return;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(BaseClass->GetClassPathName());
	Filter.bRecursiveClasses = true;
	AssetRegistry.GetAssets(Filter, OutAssetDatas);
}

void UUnrealBridgeAssetLibrary::GetDataAssetsByAssetPath(
	const FString& DataAssetPath, TArray<FAssetData>& OutAssetDatas)
{
	OutAssetDatas.Reset();

	FString ObjectPath;
	BridgeAssetOps::ParsePathToObjectPath(DataAssetPath, ObjectPath);

	UClass* BaseClass = nullptr;
	UBlueprint* LoadedBP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *ObjectPath));
	if (LoadedBP && LoadedBP->GeneratedClass && LoadedBP->GeneratedClass->IsChildOf(UDataAsset::StaticClass()))
	{
		BaseClass = LoadedBP->GeneratedClass;
	}
	if (!BaseClass)
	{
		UDataAsset* LoadedDA = Cast<UDataAsset>(StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ObjectPath));
		if (LoadedDA) BaseClass = LoadedDA->GetClass();
	}

	if (BaseClass) GetDataAssetsByBaseClass(BaseClass, OutAssetDatas);
}

void UUnrealBridgeAssetLibrary::GetDataAssetSoftPathsByBaseClass(
	TSubclassOf<UDataAsset> BaseDataAssetClass, TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();
	TArray<FAssetData> AssetDatas;
	GetDataAssetsByBaseClass(BaseDataAssetClass, AssetDatas);
	for (const FAssetData& Data : AssetDatas)
		OutSoftPaths.Add(Data.GetSoftObjectPath());
}

void UUnrealBridgeAssetLibrary::GetDataAssetSoftPathsByAssetPath(
	const FString& DataAssetPath, TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();

	FString ObjectPath;
	BridgeAssetOps::ParsePathToObjectPath(DataAssetPath, ObjectPath);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	if (!AssetData.IsValid()) return;

	FTopLevelAssetPath BaseClassPath;
	FString GeneratedClassPathStr;
	if (AssetData.GetTagValue(FName("GeneratedClass"), GeneratedClassPathStr) && !GeneratedClassPathStr.IsEmpty())
	{
		BaseClassPath = FTopLevelAssetPath(GeneratedClassPathStr);
	}
	else
	{
		BaseClassPath = AssetData.AssetClassPath;
	}
	if (!BaseClassPath.IsValid()) return;

	FARFilter Filter;
	Filter.ClassPaths.Add(BaseClassPath);
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> AssetDatas;
	AssetRegistry.GetAssets(Filter, AssetDatas);

	for (const FAssetData& Data : AssetDatas)
		OutSoftPaths.Add(Data.GetSoftObjectPath());
}

// ═══════════════════════════════════════════════════════════
//  Folder / Path Queries
// ═══════════════════════════════════════════════════════════

void UUnrealBridgeAssetLibrary::ListAssetsUnderPath(
	const FString& FolderPath, bool bIncludeSubfolders,
	TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();
	FString BasePath = FolderPath.TrimStartAndEnd();
	if (BasePath.IsEmpty()) return;

	BridgeAssetOps::NormalizeContentRoot(BasePath);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*BasePath));
	Filter.bRecursivePaths = bIncludeSubfolders;

	TArray<FAssetData> AssetDatas;
	AssetRegistry.GetAssets(Filter, AssetDatas);

	OutSoftPaths.Reserve(AssetDatas.Num());
	for (const FAssetData& Data : AssetDatas)
		OutSoftPaths.Add(Data.GetSoftObjectPath());
}

void UUnrealBridgeAssetLibrary::ListAssetsUnderPathSimple(
	const FString& ContentFolderPath, TArray<FSoftObjectPath>& OutSoftPaths)
{
	ListAssetsUnderPath(ContentFolderPath, true, OutSoftPaths);
}

void UUnrealBridgeAssetLibrary::GetSubFolderPaths(
	const FString& FolderPath, TArray<FString>& OutSubFolderPaths)
{
	OutSubFolderPaths.Reset();
	FString BasePath = FolderPath.TrimStartAndEnd();
	if (BasePath.IsEmpty()) return;

	BridgeAssetOps::NormalizeContentRoot(BasePath);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.GetSubPaths(BasePath, OutSubFolderPaths, false);
}

void UUnrealBridgeAssetLibrary::GetSubFolderNames(
	const FName& FolderPath, TArray<FName>& OutSubFolderNames)
{
	OutSubFolderNames.Reset();
	if (FolderPath.IsNone()) return;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.GetSubPaths(FolderPath, OutSubFolderNames, false);
}

// ─── Registry Metadata (no load) ────────────────────────────

namespace BridgeAssetOps
{
	/** Turn an input path (content path, object path, export-text) into a FSoftObjectPath usable by the registry. */
	static FSoftObjectPath MakeSoftPath(const FString& InPath)
	{
		FString ObjectPath;
		ParsePathToObjectPath(InPath, ObjectPath);
		if (ObjectPath.IsEmpty())
		{
			return FSoftObjectPath();
		}
		// If no '.' separator, assume content path like "/Game/Foo/Bar" → "/Game/Foo/Bar.Bar"
		if (!ObjectPath.Contains(TEXT(".")))
		{
			FString LeafName;
			int32 SlashIdx;
			if (ObjectPath.FindLastChar(TEXT('/'), SlashIdx))
			{
				LeafName = ObjectPath.Mid(SlashIdx + 1);
				ObjectPath = ObjectPath + TEXT(".") + LeafName;
			}
		}
		return FSoftObjectPath(ObjectPath);
	}

	/** Parse a TopLevelAssetPath from string, tolerating empty input. */
	static bool TryParseTopLevelPath(const FString& InClassPath, FTopLevelAssetPath& Out)
	{
		if (InClassPath.IsEmpty()) return false;
		FTopLevelAssetPath Parsed;
		Parsed.TrySetPath(InClassPath);
		if (Parsed.IsNull()) return false;
		Out = Parsed;
		return true;
	}
}

bool UUnrealBridgeAssetLibrary::DoesAssetExist(const FString& AssetPath)
{
	const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPath);
	if (Soft.IsNull()) return false;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	return AR.GetAssetByObjectPath(Soft).IsValid();
}

FBridgeAssetInfo UUnrealBridgeAssetLibrary::GetAssetInfo(const FString& AssetPath)
{
	FBridgeAssetInfo Result;

	const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPath);
	if (Soft.IsNull()) return Result;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	const FAssetData Data = AR.GetAssetByObjectPath(Soft);
	if (!Data.IsValid()) return Result;

	Result.bFound = true;
	Result.PackageName = Data.PackageName.ToString();
	Result.AssetName = Data.AssetName.ToString();
	Result.ClassPath = Data.AssetClassPath.ToString();
	Result.bIsRedirector = (Data.AssetClassPath == UObjectRedirector::StaticClass()->GetClassPathName());

	FString Filename;
	if (FPackageName::DoesPackageExist(Result.PackageName, &Filename))
	{
		const int64 Size = IFileManager::Get().FileSize(*Filename);
		if (Size > 0) Result.DiskSize = Size;
	}

	for (const TPair<FName, FAssetTagValueRef>& Pair : Data.TagsAndValues)
	{
		FBridgeAssetTag KV;
		KV.Key = Pair.Key.ToString();
		KV.Value = Pair.Value.AsString();
		Result.Tags.Add(KV);
	}

	return Result;
}

void UUnrealBridgeAssetLibrary::GetAssetsByClass(
	const FString& ClassPath,
	bool bSearchSubClasses,
	TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();

	FTopLevelAssetPath ClassTop;
	if (!BridgeAssetOps::TryParseTopLevelPath(ClassPath, ClassTop))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: GetAssetsByClass invalid ClassPath '%s'"), *ClassPath);
		return;
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssetsByClass(ClassTop, Datas, bSearchSubClasses);

	OutSoftPaths.Reserve(Datas.Num());
	for (const FAssetData& D : Datas)
	{
		OutSoftPaths.Add(D.ToSoftObjectPath());
	}
}

void UUnrealBridgeAssetLibrary::GetAssetsByTagValue(
	const FString& TagName,
	const FString& TagValue,
	const FString& OptionalClassPath,
	TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();
	if (TagName.IsEmpty()) return;

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.TagsAndValues.Add(FName(*TagName), TagValue);

	FTopLevelAssetPath ClassTop;
	if (BridgeAssetOps::TryParseTopLevelPath(OptionalClassPath, ClassTop))
	{
		Filter.ClassPaths.Add(ClassTop);
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssets(Filter, Datas);

	OutSoftPaths.Reserve(Datas.Num());
	for (const FAssetData& D : Datas)
	{
		OutSoftPaths.Add(D.ToSoftObjectPath());
	}
}

// ─── Cheap scalar / batch registry queries ──────────────────

FString UUnrealBridgeAssetLibrary::GetAssetClassPath(const FString& AssetPath)
{
	const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPath);
	if (Soft.IsNull()) return FString();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	const FAssetData Data = AR.GetAssetByObjectPath(Soft);
	if (!Data.IsValid()) return FString();

	return Data.AssetClassPath.ToString();
}

FString UUnrealBridgeAssetLibrary::GetAssetTagValue(const FString& AssetPath, const FString& TagName)
{
	if (TagName.IsEmpty()) return FString();

	const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPath);
	if (Soft.IsNull()) return FString();

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	const FAssetData Data = AR.GetAssetByObjectPath(Soft);
	if (!Data.IsValid()) return FString();

	FString Value;
	if (Data.GetTagValue(FName(*TagName), Value))
	{
		return Value;
	}
	return FString();
}

void UUnrealBridgeAssetLibrary::GetAssetsByPackagePaths(
	const TArray<FString>& FolderPaths,
	const FString& ClassFilter,
	bool bRecursive,
	TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();
	if (FolderPaths.Num() == 0) return;

	FARFilter Filter;
	Filter.bRecursivePaths = bRecursive;

	for (const FString& Raw : FolderPaths)
	{
		FString Path = Raw.TrimStartAndEnd();
		if (Path.IsEmpty()) continue;
		BridgeAssetOps::NormalizeContentRoot(Path);
		Filter.PackagePaths.Add(FName(*Path));
	}
	if (Filter.PackagePaths.Num() == 0) return;

	FTopLevelAssetPath ClassTop;
	if (BridgeAssetOps::TryParseTopLevelPath(ClassFilter, ClassTop))
	{
		Filter.ClassPaths.Add(ClassTop);
		Filter.bRecursiveClasses = bRecursive;
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssets(Filter, Datas);

	OutSoftPaths.Reserve(Datas.Num());
	for (const FAssetData& D : Datas)
	{
		OutSoftPaths.Add(D.GetSoftObjectPath());
	}
}

void UUnrealBridgeAssetLibrary::GetAssetsOfClasses(
	const TArray<FString>& ClassPaths,
	bool bSearchSubClasses,
	TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();
	if (ClassPaths.Num() == 0) return;

	FARFilter Filter;
	Filter.bRecursiveClasses = bSearchSubClasses;
	for (const FString& CP : ClassPaths)
	{
		FTopLevelAssetPath Top;
		if (BridgeAssetOps::TryParseTopLevelPath(CP, Top))
		{
			Filter.ClassPaths.Add(Top);
		}
	}
	if (Filter.ClassPaths.Num() == 0) return;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssets(Filter, Datas);

	OutSoftPaths.Reserve(Datas.Num());
	for (const FAssetData& D : Datas)
	{
		OutSoftPaths.Add(D.GetSoftObjectPath());
	}
}

void UUnrealBridgeAssetLibrary::FindRedirectorsUnderPath(
	const FString& FolderPath,
	bool bRecursive,
	TArray<FSoftObjectPath>& OutSoftPaths)
{
	OutSoftPaths.Reset();
	FString BasePath = FolderPath.TrimStartAndEnd();
	if (BasePath.IsEmpty()) return;
	BridgeAssetOps::NormalizeContentRoot(BasePath);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*BasePath));
	Filter.bRecursivePaths = bRecursive;
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssets(Filter, Datas);

	OutSoftPaths.Reserve(Datas.Num());
	for (const FAssetData& D : Datas)
	{
		OutSoftPaths.Add(D.GetSoftObjectPath());
	}
}

FString UUnrealBridgeAssetLibrary::ResolveRedirector(const FString& AssetPath)
{
	const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPath);
	if (Soft.IsNull()) return FString();

	UObjectRedirector* Redirector = LoadObject<UObjectRedirector>(nullptr, *Soft.ToString());
	if (!Redirector || !Redirector->DestinationObject)
	{
		return FString();
	}
	return Redirector->DestinationObject->GetPathName();
}

// ─── Cheap counts & batched per-asset queries ───────────────

int32 UUnrealBridgeAssetLibrary::GetAssetCountUnderPath(
	const FString& FolderPath,
	const FString& ClassFilter,
	bool bRecursive)
{
	FString BasePath = FolderPath.TrimStartAndEnd();
	if (BasePath.IsEmpty()) return 0;
	BridgeAssetOps::NormalizeContentRoot(BasePath);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*BasePath));
	Filter.bRecursivePaths = bRecursive;

	FTopLevelAssetPath ClassTop;
	if (BridgeAssetOps::TryParseTopLevelPath(ClassFilter, ClassTop))
	{
		Filter.ClassPaths.Add(ClassTop);
		Filter.bRecursiveClasses = bRecursive;
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssets(Filter, Datas);
	return Datas.Num();
}

namespace BridgeAssetOps
{
	static void GatherPackageLinks(
		const FString& PackageName,
		bool bHardOnly,
		bool bReferencers,
		TArray<FString>& OutPackageNames)
	{
		OutPackageNames.Reset();

		FString Trimmed = PackageName.TrimStartAndEnd();
		if (Trimmed.IsEmpty()) return;
		// If caller passed an object path "/Game/Foo/Bar.Bar", strip to package.
		int32 DotIdx;
		if (Trimmed.FindChar(TEXT('.'), DotIdx))
		{
			Trimmed = Trimmed.Left(DotIdx);
		}
		if (!Trimmed.StartsWith(TEXT("/"))) Trimmed = TEXT("/") + Trimmed;

		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		const FName PkgName(*Trimmed);

		UE::AssetRegistry::FDependencyQuery Query;
		if (bHardOnly)
		{
			Query.Required = UE::AssetRegistry::EDependencyProperty::Hard | UE::AssetRegistry::EDependencyProperty::Game;
		}

		TArray<FName> Links;
		if (bReferencers)
		{
			AR.GetReferencers(PkgName, Links, UE::AssetRegistry::EDependencyCategory::Package, Query);
		}
		else
		{
			AR.GetDependencies(PkgName, Links, UE::AssetRegistry::EDependencyCategory::Package, Query);
		}

		OutPackageNames.Reserve(Links.Num());
		for (const FName& L : Links)
		{
			OutPackageNames.Add(L.ToString());
		}
	}
}

void UUnrealBridgeAssetLibrary::GetPackageDependencies(
	const FString& PackageName,
	bool bHardOnly,
	TArray<FString>& OutDependencyPackageNames)
{
	BridgeAssetOps::GatherPackageLinks(PackageName, bHardOnly, /*bReferencers=*/false, OutDependencyPackageNames);
}

void UUnrealBridgeAssetLibrary::GetPackageReferencers(
	const FString& PackageName,
	bool bHardOnly,
	TArray<FString>& OutReferencerPackageNames)
{
	BridgeAssetOps::GatherPackageLinks(PackageName, bHardOnly, /*bReferencers=*/true, OutReferencerPackageNames);
}

void UUnrealBridgeAssetLibrary::GetAssetTagValuesBatch(
	const TArray<FString>& AssetPaths,
	const FString& TagName,
	TArray<FString>& OutValues)
{
	OutValues.Reset();
	OutValues.SetNum(AssetPaths.Num());
	if (AssetPaths.Num() == 0 || TagName.IsEmpty()) return;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	const FName TagFName(*TagName);

	for (int32 Idx = 0; Idx < AssetPaths.Num(); ++Idx)
	{
		const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPaths[Idx]);
		if (Soft.IsNull()) continue;

		const FAssetData Data = AR.GetAssetByObjectPath(Soft);
		if (!Data.IsValid()) continue;

		FString Value;
		if (Data.GetTagValue(TagFName, Value))
		{
			OutValues[Idx] = MoveTemp(Value);
		}
	}
}

void UUnrealBridgeAssetLibrary::GetAssetDiskSizesBatch(
	const TArray<FString>& AssetPaths,
	TArray<int64>& OutSizes)
{
	OutSizes.Reset();
	OutSizes.Init(-1, AssetPaths.Num());
	if (AssetPaths.Num() == 0) return;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	for (int32 Idx = 0; Idx < AssetPaths.Num(); ++Idx)
	{
		const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPaths[Idx]);
		if (Soft.IsNull()) continue;

		const FAssetData Data = AR.GetAssetByObjectPath(Soft);
		if (!Data.IsValid()) continue;

		FString Filename;
		if (FPackageName::DoesPackageExist(Data.PackageName.ToString(), &Filename))
		{
			const int64 Size = IFileManager::Get().FileSize(*Filename);
			if (Size > 0) OutSizes[Idx] = Size;
		}
	}
}

// ─── Structural / aggregate helpers ─────────────────────────

void UUnrealBridgeAssetLibrary::GetContentRoots(TArray<FString>& OutRoots)
{
	OutRoots.Reset();
	FPackageName::QueryRootContentPaths(OutRoots, /*bIncludeReadOnlyRoots=*/false, /*bWithoutLeadingSlashes=*/false, /*bWithoutTrailingSlashes=*/false);
}

bool UUnrealBridgeAssetLibrary::DoesFolderExist(const FString& FolderPath)
{
	FString BasePath = FolderPath.TrimStartAndEnd();
	if (BasePath.IsEmpty()) return false;
	BridgeAssetOps::NormalizeContentRoot(BasePath);

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	return AR.HasAssets(FName(*BasePath), /*bRecursive=*/true);
}

int64 UUnrealBridgeAssetLibrary::GetTotalDiskSizeUnderPath(
	const FString& FolderPath,
	const FString& ClassFilter,
	bool bRecursive,
	int32& OutAssetCount)
{
	OutAssetCount = 0;
	FString BasePath = FolderPath.TrimStartAndEnd();
	if (BasePath.IsEmpty()) return 0;
	BridgeAssetOps::NormalizeContentRoot(BasePath);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*BasePath));
	Filter.bRecursivePaths = bRecursive;

	FTopLevelAssetPath ClassTop;
	if (BridgeAssetOps::TryParseTopLevelPath(ClassFilter, ClassTop))
	{
		Filter.ClassPaths.Add(ClassTop);
		Filter.bRecursiveClasses = bRecursive;
	}

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Datas;
	AR.GetAssets(Filter, Datas);

	int64 Total = 0;
	IFileManager& FM = IFileManager::Get();
	for (const FAssetData& D : Datas)
	{
		FString Filename;
		if (FPackageName::DoesPackageExist(D.PackageName.ToString(), &Filename))
		{
			const int64 Size = FM.FileSize(*Filename);
			if (Size > 0)
			{
				Total += Size;
				++OutAssetCount;
			}
		}
	}
	return Total;
}

void UUnrealBridgeAssetLibrary::GetAssetClassPathsBatch(
	const TArray<FString>& AssetPaths,
	TArray<FString>& OutClassPaths)
{
	OutClassPaths.Reset();
	OutClassPaths.SetNum(AssetPaths.Num());
	if (AssetPaths.Num() == 0) return;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	for (int32 Idx = 0; Idx < AssetPaths.Num(); ++Idx)
	{
		const FSoftObjectPath Soft = BridgeAssetOps::MakeSoftPath(AssetPaths[Idx]);
		if (Soft.IsNull()) continue;

		const FAssetData Data = AR.GetAssetByObjectPath(Soft);
		if (!Data.IsValid()) continue;

		OutClassPaths[Idx] = Data.AssetClassPath.ToString();
	}
}

void UUnrealBridgeAssetLibrary::GetPackageDependenciesRecursive(
	const FString& PackageName,
	bool bHardOnly,
	int32 MaxDepth,
	TArray<FString>& OutDependencyPackageNames)
{
	OutDependencyPackageNames.Reset();

	FString Trimmed = PackageName.TrimStartAndEnd();
	if (Trimmed.IsEmpty()) return;
	int32 DotIdx;
	if (Trimmed.FindChar(TEXT('.'), DotIdx))
	{
		Trimmed = Trimmed.Left(DotIdx);
	}
	if (!Trimmed.StartsWith(TEXT("/"))) Trimmed = TEXT("/") + Trimmed;

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	UE::AssetRegistry::FDependencyQuery Query;
	if (bHardOnly)
	{
		Query.Required = UE::AssetRegistry::EDependencyProperty::Hard | UE::AssetRegistry::EDependencyProperty::Game;
	}

	TSet<FName> Visited;
	TArray<TPair<FName, int32>> Stack;
	Stack.Emplace(FName(*Trimmed), 0);
	Visited.Add(FName(*Trimmed));

	const bool bUnlimited = (MaxDepth <= 0);

	while (Stack.Num() > 0)
	{
		const TPair<FName, int32> Current = Stack.Pop();
		if (!bUnlimited && Current.Value >= MaxDepth) continue;

		TArray<FName> Links;
		AR.GetDependencies(Current.Key, Links, UE::AssetRegistry::EDependencyCategory::Package, Query);

		for (const FName& L : Links)
		{
			if (Visited.Contains(L)) continue;
			Visited.Add(L);
			OutDependencyPackageNames.Add(L.ToString());
			Stack.Emplace(L, Current.Value + 1);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════
//   Asset introspection — StaticMesh / SkeletalMesh / Texture / Sound
// ═══════════════════════════════════════════════════════════════════

namespace BridgeAssetIntrospection
{
	/** Collect declared material slot names + bound material asset paths
	 *  from a mesh that uses the shared FSkeletalMaterial / FStaticMaterial
	 *  shape. Captures "slot missing material" as an empty path so the array
	 *  stays 1:1 with MaterialSlotNames. */
	template<typename TMeshMaterial>
	static void CollectMaterials(const TArray<TMeshMaterial>& Mats,
		TArray<FString>& OutSlotNames, TArray<FString>& OutAssetPaths)
	{
		OutSlotNames.Reserve(Mats.Num());
		OutAssetPaths.Reserve(Mats.Num());
		for (const TMeshMaterial& M : Mats)
		{
			OutSlotNames.Add(M.MaterialSlotName.ToString());
			OutAssetPaths.Add(M.MaterialInterface ? M.MaterialInterface->GetPathName() : FString());
		}
	}
}

FBridgeStaticMeshInfo UUnrealBridgeAssetLibrary::GetStaticMeshInfo(const FString& AssetPath)
{
	FBridgeStaticMeshInfo Out;
	Out.AssetPath = AssetPath;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!Mesh) return Out;

	Out.bFound = true;

	const FBoxSphereBounds B = Mesh->GetBounds();
	Out.BoundsOrigin = B.Origin;
	Out.BoundsExtent = B.BoxExtent;
	Out.BoundsSphereRadius = B.SphereRadius;

	BridgeAssetIntrospection::CollectMaterials(Mesh->GetStaticMaterials(),
		Out.MaterialSlotNames, Out.MaterialAssetPaths);

	// LOD stats: walk render data if available; fall back to source data.
	if (FStaticMeshRenderData* RD = Mesh->GetRenderData())
	{
		Out.NumLODs = RD->LODResources.Num();
		for (int32 i = 0; i < RD->LODResources.Num(); ++i)
		{
			const FStaticMeshLODResources& LOD = RD->LODResources[i];
			FBridgeMeshLODStats S;
			S.LODIndex      = i;
			S.VertexCount   = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
			S.TriangleCount = LOD.GetNumTriangles();
			for (const FStaticMeshSection& Sec : LOD.Sections)
			{
				S.MaterialIndices.Add(Sec.MaterialIndex);
			}
			if (i == 0)
			{
				Out.NumUVChannels = LOD.GetNumTexCoords();
			}
			Out.LODStats.Add(MoveTemp(S));
		}
	}

	if (const UStaticMeshSocket* First = Mesh->Sockets.Num() > 0 ? Mesh->Sockets[0] : nullptr)
	{
		(void)First; // reach into socket array below
	}
	Out.NumSockets = Mesh->Sockets.Num();
	for (const UStaticMeshSocket* S : Mesh->Sockets)
	{
		if (S) Out.SocketNames.Add(S->SocketName.ToString());
	}

	Out.bHasCollision = (Mesh->GetBodySetup() != nullptr) &&
		(Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0);
	Out.bHasNaniteData = Mesh->IsNaniteEnabled();
	return Out;
}

FBridgeSkeletalMeshInfo UUnrealBridgeAssetLibrary::GetSkeletalMeshInfo(const FString& AssetPath)
{
	FBridgeSkeletalMeshInfo Out;
	Out.AssetPath = AssetPath;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
	if (!Mesh) return Out;

	Out.bFound = true;

	const FBoxSphereBounds B = Mesh->GetBounds();
	Out.BoundsOrigin = B.Origin;
	Out.BoundsExtent = B.BoxExtent;
	Out.BoundsSphereRadius = B.SphereRadius;

	BridgeAssetIntrospection::CollectMaterials(Mesh->GetMaterials(),
		Out.MaterialSlotNames, Out.MaterialAssetPaths);

	if (FSkeletalMeshRenderData* RD = Mesh->GetResourceForRendering())
	{
		Out.NumLODs = RD->LODRenderData.Num();
		for (int32 i = 0; i < RD->LODRenderData.Num(); ++i)
		{
			const FSkeletalMeshLODRenderData& LOD = RD->LODRenderData[i];
			FBridgeMeshLODStats S;
			S.LODIndex      = i;
			S.VertexCount   = LOD.GetNumVertices();
			// Triangle count = sum over sections of section NumTriangles.
			int32 Tris = 0;
			for (const FSkelMeshRenderSection& Sec : LOD.RenderSections)
			{
				Tris += Sec.NumTriangles;
				S.MaterialIndices.Add(Sec.MaterialIndex);
			}
			S.TriangleCount = Tris;
			Out.LODStats.Add(MoveTemp(S));
		}
	}

	if (USkeleton* Skel = Mesh->GetSkeleton())
	{
		Out.SkeletonPath = Skel->GetPathName();
		Out.NumBones = Skel->GetReferenceSkeleton().GetNum();
	}

	for (const USkeletalMeshSocket* S : Mesh->GetActiveSocketList())
	{
		if (S) Out.SocketNames.Add(S->SocketName.ToString());
	}
	Out.NumSockets = Out.SocketNames.Num();

	Out.NumMorphTargets = Mesh->GetMorphTargets().Num();

	if (UPhysicsAsset* Phys = Mesh->GetPhysicsAsset())
	{
		Out.PhysicsAssetPath = Phys->GetPathName();
	}
	return Out;
}

FBridgeTextureInfo UUnrealBridgeAssetLibrary::GetTextureInfo(const FString& AssetPath)
{
	FBridgeTextureInfo Out;
	Out.AssetPath = AssetPath;
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *AssetPath);
	if (!Tex) return Out;

	Out.bFound       = true;
	Out.Width        = Tex->GetSizeX();
	Out.Height       = Tex->GetSizeY();
	Out.NumMips      = Tex->GetNumMips();
	Out.PixelFormat  = GetPixelFormatString(Tex->GetPixelFormat());

	const UEnum* CompEnum = StaticEnum<TextureCompressionSettings>();
	Out.CompressionSettings = CompEnum ? CompEnum->GetNameStringByValue((int64)Tex->CompressionSettings) : FString();
	const UEnum* GroupEnum = StaticEnum<TextureGroup>();
	Out.LODGroup = GroupEnum ? GroupEnum->GetNameStringByValue((int64)Tex->LODGroup) : FString();

	Out.bSRGB         = Tex->SRGB != 0;
	Out.bNeverStream  = Tex->NeverStream != 0;
	Out.ResourceSizeBytes = Tex->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
	return Out;
}

FBridgeSoundInfo UUnrealBridgeAssetLibrary::GetSoundInfo(const FString& AssetPath)
{
	FBridgeSoundInfo Out;
	Out.AssetPath = AssetPath;
	UObject* Obj = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Obj) return Out;

	if (USoundWave* Wave = Cast<USoundWave>(Obj))
	{
		Out.bFound          = true;
		Out.SoundKind       = TEXT("SoundWave");
		Out.DurationSeconds = Wave->GetDuration();
		Out.SampleRate      = static_cast<int32>(Wave->GetSampleRateForCurrentPlatform());
		Out.NumChannels     = Wave->NumChannels;
		Out.bLooping        = Wave->bLooping;
		Out.CompressedDataBytes =
			Wave->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
	}
	else if (USoundCue* Cue = Cast<USoundCue>(Obj))
	{
		Out.bFound          = true;
		Out.SoundKind       = TEXT("SoundCue");
		Out.DurationSeconds = Cue->GetDuration();
	}
	else if (USoundBase* Base = Cast<USoundBase>(Obj))
	{
		Out.bFound          = true;
		Out.SoundKind       = Base->GetClass()->GetName();
		Out.DurationSeconds = Base->GetDuration();
	}
	return Out;
}

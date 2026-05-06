#include "UnrealBridgeGameplayTagLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameplayTagContainer.h"
#include "GameplayTagRedirectors.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"

namespace BridgeGameplayTagOps
{
	/** EGameplayTagSourceType → short string. Centralised so both
	 *  GetTagSourceInfo and any future reverse lookup share names. */
	const TCHAR* SourceTypeToString(EGameplayTagSourceType Type)
	{
		switch (Type)
		{
			case EGameplayTagSourceType::Native:             return TEXT("Native");
			case EGameplayTagSourceType::DefaultTagList:     return TEXT("DefaultTagList");
			case EGameplayTagSourceType::TagList:            return TEXT("TagList");
			case EGameplayTagSourceType::RestrictedTagList:  return TEXT("RestrictedTagList");
			case EGameplayTagSourceType::DataTable:          return TEXT("DataTable");
			default:                                          return TEXT("Invalid");
		}
	}

	/** Resolve a TagSource record into a human-readable file location.
	 *  For ini-backed sources we pull the actual config file path; for
	 *  Native and DataTable we fall back to the source's FName. */
	FString ResolveSourceLocation(const FGameplayTagSource* Source)
	{
		if (!Source) return FString();

		switch (Source->SourceType)
		{
			case EGameplayTagSourceType::DefaultTagList:
			case EGameplayTagSourceType::TagList:
			case EGameplayTagSourceType::RestrictedTagList:
			{
				const FString ConfigPath = Source->GetConfigFileName();
				return ConfigPath.IsEmpty() ? Source->SourceName.ToString() : ConfigPath;
			}
			default:
				return Source->SourceName.ToString();
		}
	}

	/**
	 * UE 5.7 has a quirk where some Add/Rename/Remove mutations re-serialise
	 * `UGameplayTagsSettings` and silently drop redirects whose `OldTagName`
	 * has no live in-memory tag node — which is exactly the shape every
	 * just-renamed redirect has (the OldTag is gone, only the redirect
	 * preserves lookups). The in-memory `UGameplayTagsList::GameplayTagRedirects`
	 * array still has the redirect for the rest of the session, so the bug
	 * only surfaces on the *next* editor restart, when on-disk state wins.
	 *
	 * This helper closes the gap by reading the on-disk ini and re-appending
	 * any redirects from the in-memory list that aren't textually present.
	 * Called from Add/Rename/Remove after the underlying editor module call.
	 *
	 * Returns the number of redirect lines that were re-appended (0 = ini
	 * already matches in-memory state).
	 */
	int32 EnsureSourceRedirectsPersisted(FName SourceName)
	{
		if (SourceName.IsNone()) return 0;

		UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
		const FGameplayTagSource* Source = TagsMgr.FindTagSource(SourceName);
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
		if (!Source || !Source->SourceTagList) return 0;

		const FString IniPath = Source->GetConfigFileName();
		if (IniPath.IsEmpty()) return 0;

		FString IniText;
		if (!FFileHelper::LoadFileToString(IniText, *IniPath)) return 0;

		// Build the set of redirect lines that should be present.
		TArray<FString> MissingLines;
		for (const FGameplayTagRedirect& R : Source->SourceTagList->GameplayTagRedirects)
		{
			if (R.OldTagName.IsNone() || R.NewTagName.IsNone()) continue;
			const FString Line = FString::Printf(
				TEXT("+GameplayTagRedirects=(OldTagName=\"%s\",NewTagName=\"%s\")"),
				*R.OldTagName.ToString(), *R.NewTagName.ToString());
			if (!IniText.Contains(Line, ESearchCase::CaseSensitive))
			{
				MissingLines.Add(Line);
			}
		}

		if (MissingLines.Num() == 0) return 0;

		// Insert before the first +GameplayTagList= line so the redirects stay
		// clustered with their existing siblings (UE convention). Fall back to
		// EOF append if no GameplayTagList line exists.
		const FString TagListMarker = TEXT("+GameplayTagList=");
		const FString JoinedNew = FString::Join(MissingLines, TEXT("\r\n")) + TEXT("\r\n");

		const int32 InsertIdx = IniText.Find(TagListMarker, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (InsertIdx != INDEX_NONE)
		{
			IniText.InsertAt(InsertIdx, JoinedNew);
		}
		else
		{
			if (!IniText.EndsWith(TEXT("\n"))) IniText += TEXT("\r\n");
			IniText += JoinedNew;
		}

		if (!FFileHelper::SaveStringToFile(IniText, *IniPath))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("EnsureSourceRedirectsPersisted: failed to write %s"), *IniPath);
			return 0;
		}

		UE_LOG(LogTemp, Log,
			TEXT("EnsureSourceRedirectsPersisted: re-appended %d redirect(s) to %s"),
			MissingLines.Num(), *IniPath);
		return MissingLines.Num();
#else
		return 0;  // The serialization bug is 5.7+ only
#endif
	}

	/** Look up a tag's primary source name (the FName the manager indexes
	 *  it by, e.g. "DefaultGameplayTags.ini"). Used by EnsureRedirectsPersisted
	 *  callers that only have a tag string, not a source. Returns NAME_None if
	 *  the tag isn't registered or has no source. */
	FName ResolveTagSourceName(const FString& TagString)
	{
#if WITH_EDITORONLY_DATA
		UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
		const TSharedPtr<FGameplayTagNode> Node = TagsMgr.FindTagNode(FName(*TagString));
		if (!Node.IsValid()) return NAME_None;
		const TArray<FName>& Sources = Node->GetAllSourceNames();
		return Sources.Num() > 0 ? Sources[0] : NAME_None;
#else
		return NAME_None;
#endif
	}
}

TArray<FString> UUnrealBridgeGameplayTagLibrary::FindAssetsReferencingTag(
	const FString& TagString, bool bIncludeChildren,
	const FString& PackagePathFilter, int32 MaxResults)
{
	TArray<FString> Result;
	if (TagString.IsEmpty()) return Result;

	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
	const FGameplayTag RootTag = TagsMgr.RequestGameplayTag(FName(*TagString), /*ErrorIfNotFound=*/false);
	if (!RootTag.IsValid())
	{
		// Fall through and try the literal name anyway — useful for
		// finding stale references to a tag that has been deleted but
		// is still indexed in old assets.
		UE_LOG(LogTemp, Verbose,
			TEXT("FindAssetsReferencingTag: tag '%s' not registered; querying SearchableName by raw name only."),
			*TagString);
	}

	TArray<FName> TagsToQuery;
	TagsToQuery.Add(FName(*TagString));

	if (bIncludeChildren && RootTag.IsValid())
	{
		const FGameplayTagContainer ChildContainer = TagsMgr.RequestGameplayTagChildren(RootTag);
		for (const FGameplayTag& Child : ChildContainer)
		{
			TagsToQuery.AddUnique(Child.GetTagName());
		}
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	UScriptStruct* TagStruct = FGameplayTag::StaticStruct();

	TSet<FString> Unique;
	for (const FName& TagName : TagsToQuery)
	{
		const FAssetIdentifier TagId(TagStruct, TagName);
		TArray<FAssetIdentifier> Refs;
		AssetRegistry.GetReferencers(TagId, Refs,
			UE::AssetRegistry::EDependencyCategory::SearchableName);

		for (const FAssetIdentifier& Ref : Refs)
		{
			if (Ref.PackageName.IsNone()) continue;
			const FString PackageName = Ref.PackageName.ToString();
			if (!PackagePathFilter.IsEmpty() && !PackageName.StartsWith(PackagePathFilter)) continue;
			Unique.Add(PackageName);
			if (MaxResults > 0 && Unique.Num() >= MaxResults) break;
		}
		if (MaxResults > 0 && Unique.Num() >= MaxResults) break;
	}

	Result = Unique.Array();
	Result.Sort();
	if (MaxResults > 0 && Result.Num() > MaxResults) Result.SetNum(MaxResults);
	return Result;
}

TArray<FString> UUnrealBridgeGameplayTagLibrary::ListAllRegisteredTags(
	const FString& FilterPrefix, int32 MaxResults)
{
	TArray<FString> Result;

	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
	FGameplayTagContainer AllTags;
	TagsMgr.RequestAllGameplayTags(AllTags, /*OnlyIncludeDictionaryTags=*/false);

	for (const FGameplayTag& Tag : AllTags)
	{
		FString TagStr = Tag.ToString();
		if (!FilterPrefix.IsEmpty() && !TagStr.StartsWith(FilterPrefix)) continue;
		Result.Add(MoveTemp(TagStr));
		if (MaxResults > 0 && Result.Num() >= MaxResults) break;
	}

	Result.Sort();
	return Result;
}

FBridgeTagSourceInfo UUnrealBridgeGameplayTagLibrary::GetTagSourceInfo(const FString& TagString)
{
	FBridgeTagSourceInfo Info;
	Info.TagString = TagString;
	Info.SourceType = TEXT("NotFound");

	if (TagString.IsEmpty()) return Info;

	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
	const TSharedPtr<FGameplayTagNode> Node = TagsMgr.FindTagNode(FName(*TagString));
	if (!Node.IsValid()) return Info;

	Info.bFound = true;
	Info.bIsRestricted = Node->IsRestrictedGameplayTag();

#if WITH_EDITORONLY_DATA
	Info.bIsExplicit = Node->IsExplicitTag();

	const TArray<FName>& SourceNames = Node->GetAllSourceNames();
	if (SourceNames.Num() == 0)
	{
		Info.SourceType = TEXT("Unknown");
		return Info;
	}

	// Primary = first source.
	const FName PrimarySourceName = SourceNames[0];
	if (const FGameplayTagSource* PrimarySource = TagsMgr.FindTagSource(PrimarySourceName))
	{
		Info.SourceType = BridgeGameplayTagOps::SourceTypeToString(PrimarySource->SourceType);
		Info.SourceLocation = BridgeGameplayTagOps::ResolveSourceLocation(PrimarySource);
	}
	else
	{
		Info.SourceType = TEXT("Unknown");
		Info.SourceLocation = PrimarySourceName.ToString();
	}

	// Additional sources (rare — when the same tag is registered from
	// multiple .ini / native locations).
	for (int32 i = 1; i < SourceNames.Num(); ++i)
	{
		Info.AdditionalSources.Add(SourceNames[i].ToString());
	}
#else
	// Non-editor build can't introspect source names. Should not happen
	// for this editor-only plugin, but guard for safety.
	Info.bIsExplicit = true;
	Info.SourceType = TEXT("Unknown");
#endif

	return Info;
}

// ── Tag source enumeration ──────────────────────────────────────

TArray<FBridgeTagSourceListing> UUnrealBridgeGameplayTagLibrary::ListTagSourceInis(
	const FString& FilterType)
{
	TArray<FBridgeTagSourceListing> Result;
	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();

	// FindTagSourcesWithType is the only public window onto the otherwise-private
	// TagSources map. Iterate every category and union the results.
	static const TArray<EGameplayTagSourceType> AllTypes = {
		EGameplayTagSourceType::Native,
		EGameplayTagSourceType::DefaultTagList,
		EGameplayTagSourceType::TagList,
		EGameplayTagSourceType::RestrictedTagList,
		EGameplayTagSourceType::DataTable,
	};

	const FName FilterFName = FilterType.IsEmpty() ? NAME_None : FName(*FilterType);

	for (EGameplayTagSourceType Type : AllTypes)
	{
		const FString TypeName = BridgeGameplayTagOps::SourceTypeToString(Type);
		if (!FilterFName.IsNone() && TypeName != FilterType) continue;

		TArray<const FGameplayTagSource*> Sources;
		TagsMgr.FindTagSourcesWithType(Type, Sources);

		for (const FGameplayTagSource* Source : Sources)
		{
			if (!Source) continue;
			FBridgeTagSourceListing Entry;
			Entry.SourceName = Source->SourceName.ToString();
			Entry.SourceType = TypeName;
			Entry.ConfigFilePath = BridgeGameplayTagOps::ResolveSourceLocation(Source);
			// Native sources require C++ edits — not writable from here.
			// DataTable would need DataTable-row mutation, also not yet wired.
			// Anything ini-backed (DefaultTagList / TagList / RestrictedTagList)
			// IS writable via IGameplayTagsEditorModule.
			switch (Type)
			{
				case EGameplayTagSourceType::DefaultTagList:
				case EGameplayTagSourceType::TagList:
				case EGameplayTagSourceType::RestrictedTagList:
					Entry.bIsWritable = true;
					break;
				default:
					Entry.bIsWritable = false;
					break;
			}
			Result.Add(MoveTemp(Entry));
		}
	}

	return Result;
}

// ── Mutations ───────────────────────────────────────────────────

bool UUnrealBridgeGameplayTagLibrary::AddGameplayTag(
	const FString& NewTag, const FString& SourceIni,
	const FString& Comment, bool bIsRestricted)
{
	if (NewTag.IsEmpty()) return false;
	if (!IGameplayTagsEditorModule::IsAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddGameplayTag: GameplayTagsEditor module unavailable"));
		return false;
	}

	const FName SourceFName = SourceIni.IsEmpty() ? NAME_None : FName(*SourceIni);
	const bool bOk = IGameplayTagsEditorModule::Get().AddNewGameplayTagToINI(
		NewTag, Comment, SourceFName, bIsRestricted, /*bAllowNonRestrictedChildren=*/true);

	// UE 5.7's add can re-serialise the source ini and drop redirects whose
	// OldTagName has no live in-memory node. Re-append any that vanished.
	if (bOk)
	{
		FName ResolvedSource = SourceFName;
		if (ResolvedSource.IsNone())
		{
			ResolvedSource = BridgeGameplayTagOps::ResolveTagSourceName(NewTag);
		}
		BridgeGameplayTagOps::EnsureSourceRedirectsPersisted(ResolvedSource);
	}

	return bOk;
}

bool UUnrealBridgeGameplayTagLibrary::RenameGameplayTag(
	const FString& OldTag, const FString& NewTag, bool bRenameChildren)
{
	// Case-sensitive: a case-only rename (e.g. "Foo.Bar" -> "foo.bar") is a legitimate
	// op that produces a redirect. FString::operator== is case-insensitive, so use Equals
	// with ESearchCase::CaseSensitive to let case-only renames through.
	if (OldTag.IsEmpty() || NewTag.IsEmpty() || OldTag.Equals(NewTag, ESearchCase::CaseSensitive)) return false;
	if (!IGameplayTagsEditorModule::IsAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("RenameGameplayTag: GameplayTagsEditor module unavailable"));
		return false;
	}

	// UE's RenameTagInINI does NOT validate that OldTag exists — it cheerfully
	// writes a redirect for any string pair, leaving useless +GameplayTagRedirects
	// lines in the ini. Guard up-front: only rename real tags.
	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
	if (!TagsMgr.FindTagNode(FName(*OldTag)).IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RenameGameplayTag: '%s' not registered"), *OldTag);
		return false;
	}

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
	const bool bOk = IGameplayTagsEditorModule::Get().RenameTagInINI(OldTag, NewTag, bRenameChildren);
#else
	const bool bOk = IGameplayTagsEditorModule::Get().RenameTagInINI(OldTag, NewTag);
#endif

	// Re-append the just-written redirect if a follow-up serialise dropped it,
	// and any other in-memory redirects that may have gone missing.
	if (bOk)
	{
		const FName SourceName = BridgeGameplayTagOps::ResolveTagSourceName(NewTag);
		BridgeGameplayTagOps::EnsureSourceRedirectsPersisted(SourceName);
	}

	return bOk;
}

bool UUnrealBridgeGameplayTagLibrary::RemoveGameplayTag(const FString& TagString)
{
	if (TagString.IsEmpty()) return false;
	if (!IGameplayTagsEditorModule::IsAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveGameplayTag: GameplayTagsEditor module unavailable"));
		return false;
	}

	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
	const TSharedPtr<FGameplayTagNode> Node = TagsMgr.FindTagNode(FName(*TagString));
	if (!Node.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveGameplayTag: '%s' not in tag manager"), *TagString);
		return false;
	}

	// Capture the source name BEFORE delete (after the call, the node is gone
	// and the lookup would fail).
	FName SourceName = NAME_None;
#if WITH_EDITORONLY_DATA
	{
		const TArray<FName>& Sources = Node->GetAllSourceNames();
		if (Sources.Num() > 0) SourceName = Sources[0];
	}
#endif

	const bool bOk = IGameplayTagsEditorModule::Get().DeleteTagFromINI(Node);
	if (bOk && !SourceName.IsNone())
	{
		BridgeGameplayTagOps::EnsureSourceRedirectsPersisted(SourceName);
	}
	return bOk;
}

bool UUnrealBridgeGameplayTagLibrary::RemoveGameplayTagRedirect(
	const FString& OldTag, const FString& NewTag)
{
	if (OldTag.IsEmpty() || NewTag.IsEmpty()) return false;

	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();
	const FName OldFName(*OldTag);
	const FName NewFName(*NewTag);

	// Walk every writable source looking for the exact (Old, New) pair.
	// Restricted is included even though current bridge mutations don't write
	// there — restricted tag redirects are still valid targets to clean up.
	static const TArray<EGameplayTagSourceType> WritableTypes = {
		EGameplayTagSourceType::DefaultTagList,
		EGameplayTagSourceType::TagList,
		EGameplayTagSourceType::RestrictedTagList,
	};

	const FGameplayTagSource* FoundSource = nullptr;
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
	UGameplayTagsList* FoundList = nullptr;
#else
	int32 FoundSettingsIdx = INDEX_NONE;
#endif
	int32 FoundIdx = INDEX_NONE;

	for (EGameplayTagSourceType Type : WritableTypes)
	{
		TArray<const FGameplayTagSource*> Sources;
		TagsMgr.FindTagSourcesWithType(Type, Sources);
		for (const FGameplayTagSource* Source : Sources)
		{
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
			if (!Source || !Source->SourceTagList) continue;
			UGameplayTagsList* List = Source->SourceTagList;
			for (int32 i = 0; i < List->GameplayTagRedirects.Num(); ++i)
			{
				const FGameplayTagRedirect& R = List->GameplayTagRedirects[i];
				if (R.OldTagName == OldFName && R.NewTagName == NewFName)
				{
					FoundSource = Source;
					FoundList = List;
					FoundIdx = i;
					break;
				}
			}
#else
			// Legacy: scan this source's ini for the matching redirect line
			if (!Source) continue;
			{
				const FString SIniPath = Source->GetConfigFileName();
				if (SIniPath.IsEmpty()) continue;
				FString SIniText;
				if (!FFileHelper::LoadFileToString(SIniText, *SIniPath)) continue;
				TArray<FString> SLines;
				SIniText.ParseIntoArrayLines(SLines);
				const FString TargetLine = FString::Printf(
					TEXT("+GameplayTagRedirects=(OldTagName=\"%s\",NewTagName=\"%s\")"),
					*OldTag, *NewTag);
				bool bFoundInSource = false;
				for (const FString& SL : SLines)
				{
					FString ST = SL.TrimStartAndEnd();
					if (ST == TargetLine) { bFoundInSource = true; break; }
				}
				if (!bFoundInSource) continue;
				FoundSource = Source;
				FoundSettingsIdx = 0;
				FoundIdx = 0;
			}
#endif
			if (FoundIdx != INDEX_NONE) break;
		}
		if (FoundIdx != INDEX_NONE) break;
	}

	if (FoundIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RemoveGameplayTagRedirect: redirect '%s' -> '%s' not found in any writable source"),
			*OldTag, *NewTag);
		return false;
	}

	// 1) drop from in-memory so EnsureSourceRedirectsPersisted won't re-add it
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
	FoundList->GameplayTagRedirects.RemoveAt(FoundIdx);
#else
	// Legacy: no in-memory singleton to update; ini strip handles persistence
#endif

	// 2) strip the matching line from the on-disk ini
	const FString IniPath = FoundSource->GetConfigFileName();
	if (!IniPath.IsEmpty())
	{
		FString IniText;
		if (FFileHelper::LoadFileToString(IniText, *IniPath))
		{
			const FString Line = FString::Printf(
				TEXT("+GameplayTagRedirects=(OldTagName=\"%s\",NewTagName=\"%s\")"),
				*OldTag, *NewTag);

			// Try CRLF, then LF, then no trailing newline (last line of file).
			bool bStripped = false;
			for (const TCHAR* Eol : { TEXT("\r\n"), TEXT("\n") })
			{
				const FString WithEol = Line + Eol;
				const int32 Idx = IniText.Find(WithEol, ESearchCase::CaseSensitive);
				if (Idx != INDEX_NONE)
				{
					IniText.RemoveAt(Idx, WithEol.Len());
					bStripped = true;
					break;
				}
			}
			if (!bStripped)
			{
				const int32 Idx = IniText.Find(Line, ESearchCase::CaseSensitive);
				if (Idx != INDEX_NONE)
				{
					IniText.RemoveAt(Idx, Line.Len());
					bStripped = true;
				}
			}
			if (bStripped)
			{
				FFileHelper::SaveStringToFile(IniText, *IniPath);
			}
		}
	}

	// 3) tell the manager to forget the redirect — without this, lookups
	//    for OldTag still resolve to NewTag in the running session.
	TagsMgr.EditorRefreshGameplayTagTree();

	UE_LOG(LogTemp, Log,
		TEXT("RemoveGameplayTagRedirect: removed '%s' -> '%s' from %s"),
		*OldTag, *NewTag, *FoundSource->SourceName.ToString());
	return true;
}

TArray<FBridgeTagRedirectEntry> UUnrealBridgeGameplayTagLibrary::ListGameplayTagRedirects(
	const FString& SourceIniFilter, const FString& OldTagPrefixFilter)
{
	TArray<FBridgeTagRedirectEntry> Result;
	UGameplayTagsManager& TagsMgr = UGameplayTagsManager::Get();

	const FName SourceFilterFName = SourceIniFilter.IsEmpty() ? NAME_None : FName(*SourceIniFilter);

	// RestrictedTagList uses URestrictedGameplayTagsList, which doesn't carry
	// a GameplayTagRedirects array — only DefaultTagList / TagList do.
	static const TArray<EGameplayTagSourceType> TypesWithRedirects = {
		EGameplayTagSourceType::DefaultTagList,
		EGameplayTagSourceType::TagList,
	};

	for (EGameplayTagSourceType Type : TypesWithRedirects)
	{
		TArray<const FGameplayTagSource*> Sources;
		TagsMgr.FindTagSourcesWithType(Type, Sources);
		for (const FGameplayTagSource* Source : Sources)
		{
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
			if (!Source || !Source->SourceTagList) continue;
#else
			if (!Source) continue;
#endif
			if (!SourceFilterFName.IsNone() && Source->SourceName != SourceFilterFName) continue;

			const FString SourceNameStr = Source->SourceName.ToString();
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
			for (const FGameplayTagRedirect& R : Source->SourceTagList->GameplayTagRedirects)
			{
				if (R.OldTagName.IsNone() || R.NewTagName.IsNone()) continue;

				FString OldStr = R.OldTagName.ToString();
				// Tags are case-sensitive (Statetree != StateTree); filter must match.
				if (!OldTagPrefixFilter.IsEmpty()
					&& !OldStr.StartsWith(OldTagPrefixFilter, ESearchCase::CaseSensitive)) continue;

				FBridgeTagRedirectEntry Entry;
				Entry.OldTag = MoveTemp(OldStr);
				Entry.NewTag = R.NewTagName.ToString();
				Entry.SourceName = SourceNameStr;
				Result.Add(MoveTemp(Entry));
			}
#else
			// Legacy: parse redirects from this source's ini file
			{
				const FString LIniPath = Source->GetConfigFileName();
				if (LIniPath.IsEmpty()) continue;
				FString LIniText;
				if (!FFileHelper::LoadFileToString(LIniText, *LIniPath)) continue;
				TArray<FString> LLines;
				LIniText.ParseIntoArrayLines(LLines);
				for (const FString& LL : LLines)
				{
					FString LT = LL.TrimStartAndEnd();
					if (!LT.StartsWith(TEXT("+GameplayTagRedirects="))) continue;
					FString OldStr, NewStr;
					{
						int32 OI = LT.Find(TEXT("OldTagName=\""));
						int32 NI = LT.Find(TEXT("NewTagName=\""));
						if (OI != INDEX_NONE)
						{
							int32 OStart = OI + 12; // len of "OldTagName=\""
							int32 OEnd = LT.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, OStart);
							if (OEnd != INDEX_NONE) OldStr = LT.Mid(OStart, OEnd - OStart);
						}
						if (NI != INDEX_NONE)
						{
							int32 NStart = NI + 12; // len of "NewTagName=\""
							int32 NEnd = LT.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, NStart);
							if (NEnd != INDEX_NONE) NewStr = LT.Mid(NStart, NEnd - NStart);
						}
					}
					if (OldStr.IsEmpty() || NewStr.IsEmpty()) continue;

					// Tags are case-sensitive (Statetree != StateTree); filter must match.
					if (!OldTagPrefixFilter.IsEmpty()
						&& !OldStr.StartsWith(OldTagPrefixFilter, ESearchCase::CaseSensitive)) continue;

					FBridgeTagRedirectEntry Entry;
					Entry.OldTag = MoveTemp(OldStr);
					Entry.NewTag = MoveTemp(NewStr);
					Entry.SourceName = SourceNameStr;
					Result.Add(MoveTemp(Entry));
				}
			}
#endif
		}
	}

	Result.Sort([](const FBridgeTagRedirectEntry& A, const FBridgeTagRedirectEntry& B)
	{
		return A.OldTag < B.OldTag;
	});
	return Result;
}

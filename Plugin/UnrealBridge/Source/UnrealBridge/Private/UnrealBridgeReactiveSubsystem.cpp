#include "UnrealBridgeReactiveSubsystem.h"
#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveLibrary.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactive, Log, All);

// Forward decls for adapter factories, defined in adapter translation units.
namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeGameplayEventAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeAttributeChangedAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeActorLifecycleAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeMovementModeAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeAnimNotifyAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeInputActionAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeTimerAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeAssetEventAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakePieStateAdapter();
	TUniquePtr<IBridgeReactiveAdapter> MakeBpCompiledAdapter();
}

namespace BridgeReactiveImpl
{
	/** Hard cap on synchronous handler re-entrancy (handler A fires event → handler B → …). */
	constexpr int32 MaxDispatchDepth = 16;
	/** Emit a warning at this depth before hitting the hard cap. */
	constexpr int32 WarnDispatchDepth = 8;

	FString TriggerTypeName(EBridgeTrigger T)
	{
		switch (T)
		{
		case EBridgeTrigger::GameplayEvent:        return TEXT("GameplayEvent");
		case EBridgeTrigger::AnimNotify:           return TEXT("AnimNotify");
		case EBridgeTrigger::AttributeChanged:     return TEXT("AttributeChanged");
		case EBridgeTrigger::MovementModeChanged:  return TEXT("MovementModeChanged");
		case EBridgeTrigger::InputAction:          return TEXT("InputAction");
		case EBridgeTrigger::ActorLifecycle:       return TEXT("ActorLifecycle");
		case EBridgeTrigger::Timer:                return TEXT("Timer");
		case EBridgeTrigger::AssetEvent:           return TEXT("AssetEvent");
		case EBridgeTrigger::PieEvent:             return TEXT("PieEvent");
		case EBridgeTrigger::BpCompiled:           return TEXT("BpCompiled");
		default:                                   return TEXT("None");
		}
	}

	FString LifetimeName(EBridgeHandlerLifetime L)
	{
		switch (L)
		{
		case EBridgeHandlerLifetime::Permanent:         return TEXT("Permanent");
		case EBridgeHandlerLifetime::Once:              return TEXT("Once");
		case EBridgeHandlerLifetime::Count:             return TEXT("Count");
		case EBridgeHandlerLifetime::WhilePIE:          return TEXT("WhilePIE");
		case EBridgeHandlerLifetime::WhileSubjectAlive: return TEXT("WhileSubjectAlive");
		default:                                        return TEXT("");
		}
	}

	FString ErrorPolicyName(EBridgeErrorPolicy E)
	{
		switch (E)
		{
		case EBridgeErrorPolicy::LogContinue:   return TEXT("LogContinue");
		case EBridgeErrorPolicy::LogUnregister: return TEXT("LogUnregister");
		case EBridgeErrorPolicy::Throw:         return TEXT("Throw");
		default:                                return TEXT("");
		}
	}

	FString EscapePythonStringLiteral(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\''))
			{
				Out.AppendChar(TEXT('\\'));
				Out.AppendChar(C);
			}
			else if (C == TEXT('\n'))
			{
				Out.Append(TEXT("\\n"));
			}
			else if (C == TEXT('\r'))
			{
				Out.Append(TEXT("\\r"));
			}
			else
			{
				Out.AppendChar(C);
			}
		}
		return Out;
	}

	FString BuildSummaryTrigger(EBridgeTrigger T, const FName& Selector)
	{
		if (Selector.IsNone())
		{
			return TriggerTypeName(T);
		}
		return FString::Printf(TEXT("%s:%s"), *TriggerTypeName(T), *Selector.ToString());
	}

	FString BuildSubjectPath(const TWeakObjectPtr<UObject>& Subject)
	{
		if (UObject* Obj = Subject.Get())
		{
			return Obj->GetPathName();
		}
		if (Subject.IsExplicitlyNull())
		{
			return FString();
		}
		return TEXT("<invalid>");
	}

	void ApplyLifetimeDecrement(FBridgeHandlerRecord& R, bool& bShouldRemove)
	{
		bShouldRemove = false;
		switch (R.Lifetime)
		{
		case EBridgeHandlerLifetime::Once:
			bShouldRemove = true;
			break;
		case EBridgeHandlerLifetime::Count:
			if (R.RemainingCalls > 0)
			{
				--R.RemainingCalls;
				if (R.RemainingCalls <= 0)
				{
					bShouldRemove = true;
				}
			}
			else
			{
				bShouldRemove = true;
			}
			break;
		default:
			break;
		}
	}
} // namespace BridgeReactiveImpl

// ─── Subsystem lifecycle ────────────────────────────────────────

UBridgeReactiveSubsystem* UBridgeReactiveSubsystem::Get()
{
	if (!GEditor)
	{
		return nullptr;
	}
	return GEditor->GetEditorSubsystem<UBridgeReactiveSubsystem>();
}

void UBridgeReactiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// World cleanup: purge handlers whose subject belonged to the
	// cleaned world. Fires on PIE end, map change, and editor shutdown.
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UBridgeReactiveSubsystem::HandleWorldCleanup);

	// PIE end: remove WhilePIE-scoped handlers.
	PieEndedHandle = FEditorDelegates::EndPIE.AddUObject(
		this, &UBridgeReactiveSubsystem::HandlePieEnded);

	// PIE start: retry any DeferredHandlers whose Subject needs a live PIE world.
	PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddUObject(
		this, &UBridgeReactiveSubsystem::HandlePostPIEStarted);

	// Persistence debounce ticker: saves ~100 ms after the last mutation.
	PersistenceDebounceHandle = FTSTicker::GetCoreTicker().AddTicker(
		TEXT("UnrealBridgeReactive.PersistenceDebounce"),
		0.1f,
		[this](float /*Dt*/) -> bool
		{
			if (bPersistenceDirty)
			{
				bPersistenceDirty = false;
				SaveAllHandlers();
			}
			return true;
		});

	// Deferred-exec ticker drains any snippets queued via DeferToNextTick.
	DeferTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		TEXT("UnrealBridgeReactive.DeferTicker"),
		0.0f,
		[this](float /*Dt*/) -> bool
		{
			TArray<FString> Drain;
			Swap(Drain, DeferredScripts);
			for (const FString& S : Drain)
			{
				FString Err;
				if (!ExecutePythonScript(S, Err))
				{
					UE_LOG(LogUnrealBridgeReactive, Warning,
						TEXT("deferred script failed: %s"), *Err);
				}
			}
			return true; // keep ticking
		});

	// Register built-in adapters (trigger types currently implemented).
	RegisterAdapter(BridgeReactiveAdapters::MakeGameplayEventAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeAttributeChangedAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeActorLifecycleAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeMovementModeAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeAnimNotifyAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeInputActionAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeTimerAdapter());
	// Editor-scope adapters (P5).
	RegisterAdapter(BridgeReactiveAdapters::MakeAssetEventAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakePieStateAdapter());
	RegisterAdapter(BridgeReactiveAdapters::MakeBpCompiledAdapter());

	UE_LOG(LogUnrealBridgeReactive, Log,
		TEXT("UBridgeReactiveSubsystem initialized (%d adapter(s))."), Adapters.Num());

	// Load persisted handlers AFTER adapters register so re-register's
	// OnHandlerAdded call lands on a live adapter. PIE-tied subjects that
	// don't resolve yet are parked in DeferredHandlers.
	LoadAllHandlers();
}

void UBridgeReactiveSubsystem::Deinitialize()
{
	// Flush any pending save so we don't lose the last mutation on quit.
	if (bPersistenceDirty)
	{
		bPersistenceDirty = false;
		SaveAllHandlers();
	}

	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FEditorDelegates::EndPIE.Remove(PieEndedHandle);
	FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(DeferTickerHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(PersistenceDebounceHandle);

	// Unregister all persistent tickers (sticky-input, Timer adapter's multiplexer, …).
	for (FTSTicker::FDelegateHandle& H : PersistentTickers)
	{
		if (H.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(H);
		}
	}
	PersistentTickers.Reset();

	// Tear down adapters first (they unbind UE delegates they still hold),
	// then drop handler records.
	for (TUniquePtr<IBridgeReactiveAdapter>& A : Adapters)
	{
		if (A.IsValid())
		{
			A->Shutdown();
		}
	}
	Adapters.Reset();
	Handlers.Reset();
	DeferredScripts.Reset();

	Super::Deinitialize();
}

void UBridgeReactiveSubsystem::RegisterAdapter(TUniquePtr<IBridgeReactiveAdapter> Adapter)
{
	if (Adapter.IsValid())
	{
		Adapters.Add(MoveTemp(Adapter));
	}
}

IBridgeReactiveAdapter* UBridgeReactiveSubsystem::FindAdapter(EBridgeTrigger TriggerType) const
{
	for (const TUniquePtr<IBridgeReactiveAdapter>& A : Adapters)
	{
		if (A.IsValid() && A->GetTriggerType() == TriggerType)
		{
			return A.Get();
		}
	}
	return nullptr;
}

TMap<FString, FString> UBridgeReactiveSubsystem::DescribeTriggerContext(const FString& TriggerTypeName) const
{
	for (const TUniquePtr<IBridgeReactiveAdapter>& A : Adapters)
	{
		if (A.IsValid() &&
			BridgeReactiveImpl::TriggerTypeName(A->GetTriggerType()) == TriggerTypeName)
		{
			return A->DescribeContext();
		}
	}
	return TMap<FString, FString>();
}

// ─── Handler id issuance ────────────────────────────────────────

FString UBridgeReactiveSubsystem::IssueHandlerId(const FString& Scope)
{
	int32& Seq = (Scope == TEXT("editor")) ? EditorSeq : RuntimeSeq;
	++Seq;
	// Short random tag reduces confusion if the agent retains a handler_id string
	// across an editor restart — the seq resets but rand4 won't collide.
	const uint32 Rand = FGuid::NewGuid().A ^ FGuid::NewGuid().B;
	const FString Prefix = (Scope == TEXT("editor")) ? TEXT("ed") : TEXT("rt");
	return FString::Printf(TEXT("%s_%04d_%04x"), *Prefix, Seq, Rand & 0xffff);
}

// ─── Registration ───────────────────────────────────────────────

FString UBridgeReactiveSubsystem::RegisterHandler(FBridgeHandlerRecord&& Record)
{
	if (Record.TaskName.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RegisterHandler refused: task_name is required."));
		return FString();
	}
	if (Record.Description.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RegisterHandler refused: description is required."));
		return FString();
	}
	if (Record.Script.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RegisterHandler refused: script is empty."));
		return FString();
	}
	if (Record.TriggerType == EBridgeTrigger::None)
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RegisterHandler refused: trigger type is None."));
		return FString();
	}

	IBridgeReactiveAdapter* Adapter = FindAdapter(Record.TriggerType);
	if (!Adapter)
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RegisterHandler refused: no adapter for trigger '%s' (not yet implemented?)."),
			*BridgeReactiveImpl::TriggerTypeName(Record.TriggerType));
		return FString();
	}

	if (Record.Scope.IsEmpty())
	{
		Record.Scope = TEXT("runtime");
	}
	Record.HandlerId = IssueHandlerId(Record.Scope);
	Record.CreatedAt = FDateTime::UtcNow();

	TSharedRef<FBridgeHandlerRecord> Shared = MakeShared<FBridgeHandlerRecord>(MoveTemp(Record));
	const FString Id = Shared->HandlerId;
	Handlers.Add(Id, Shared);

	Adapter->OnHandlerAdded(*Shared);

	UE_LOG(LogUnrealBridgeReactive, Log,
		TEXT("registered handler %s '%s' (%s)"),
		*Id, *Shared->TaskName,
		*BridgeReactiveImpl::BuildSummaryTrigger(Shared->TriggerType, Shared->Selector));
	MarkDirty();
	return Id;
}

bool UBridgeReactiveSubsystem::UnregisterHandler(const FString& HandlerId)
{
	TSharedRef<FBridgeHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found)
	{
		return false;
	}
	TSharedRef<FBridgeHandlerRecord> Ref = *Found;
	if (IBridgeReactiveAdapter* Adapter = FindAdapter(Ref->TriggerType))
	{
		Adapter->OnHandlerRemoved(*Ref);
	}
	Handlers.Remove(HandlerId);
	UE_LOG(LogUnrealBridgeReactive, Log, TEXT("unregistered handler %s"), *HandlerId);
	MarkDirty();
	return true;
}

void UBridgeReactiveSubsystem::RemoveByIdInternal(const FString& HandlerId)
{
	UnregisterHandler(HandlerId);
}

bool UBridgeReactiveSubsystem::PauseHandler(const FString& HandlerId)
{
	if (TSharedRef<FBridgeHandlerRecord>* R = Handlers.Find(HandlerId))
	{
		(*R)->bPaused = true;
		return true;
	}
	return false;
}

bool UBridgeReactiveSubsystem::ResumeHandler(const FString& HandlerId)
{
	if (TSharedRef<FBridgeHandlerRecord>* R = Handlers.Find(HandlerId))
	{
		(*R)->bPaused = false;
		return true;
	}
	return false;
}

int32 UBridgeReactiveSubsystem::ClearHandlers(const FString& Scope)
{
	const bool bAll = (Scope == TEXT("all") || Scope.IsEmpty());
	TArray<FString> ToRemove;
	for (const auto& Pair : Handlers)
	{
		if (bAll || Pair.Value->Scope == Scope)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToRemove)
	{
		UnregisterHandler(Id);
	}
	return ToRemove.Num();
}

// ─── Introspection ──────────────────────────────────────────────

TArray<FBridgeHandlerSummary> UBridgeReactiveSubsystem::ListAllHandlers(
	const FString& FilterScope,
	const FString& FilterTriggerTypeName,
	const FString& FilterTag) const
{
	TArray<FBridgeHandlerSummary> Results;
	Results.Reserve(Handlers.Num());

	for (const auto& Pair : Handlers)
	{
		const FBridgeHandlerRecord& R = *Pair.Value;

		if (!FilterScope.IsEmpty() && R.Scope != FilterScope)
		{
			continue;
		}
		if (!FilterTriggerTypeName.IsEmpty() &&
			BridgeReactiveImpl::TriggerTypeName(R.TriggerType) != FilterTriggerTypeName)
		{
			continue;
		}
		// Tag filter: literal text matches by exact equality (covers existing
		// callers); patterns containing '*' or '?' use FString::MatchesWildcard.
		// Detection is char-scan over the filter, not the per-handler tag list,
		// so it's O(filter_len) once per call.
		if (!FilterTag.IsEmpty())
		{
			const bool bWildcard = FilterTag.Contains(TEXT("*")) || FilterTag.Contains(TEXT("?"));
			const bool bMatch = bWildcard
				? R.Tags.ContainsByPredicate([&FilterTag](const FString& T)
				  {
					  return T.MatchesWildcard(FilterTag);
				  })
				: R.Tags.Contains(FilterTag);
			if (!bMatch)
			{
				continue;
			}
		}

		FBridgeHandlerSummary S;
		S.HandlerId = R.HandlerId;
		S.Scope = R.Scope;
		S.TaskName = R.TaskName;
		S.Description = R.Description;
		S.TriggerSummary = BridgeReactiveImpl::BuildSummaryTrigger(R.TriggerType, R.Selector);
		S.SubjectPath = BridgeReactiveImpl::BuildSubjectPath(R.Subject);
		S.ScriptPath = R.ScriptPath;
		S.Tags = R.Tags;
		S.Lifetime = BridgeReactiveImpl::LifetimeName(R.Lifetime);
		S.bPaused = R.bPaused;
		S.Stats = R.Stats;
		Results.Add(MoveTemp(S));
	}

	Results.Sort([](const FBridgeHandlerSummary& A, const FBridgeHandlerSummary& B)
	{
		return A.HandlerId < B.HandlerId;
	});
	return Results;
}

bool UBridgeReactiveSubsystem::GetHandler(const FString& HandlerId, FBridgeHandlerDetail& OutDetail) const
{
	const TSharedRef<FBridgeHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found)
	{
		return false;
	}
	const FBridgeHandlerRecord& R = **Found;

	FBridgeHandlerSummary S;
	S.HandlerId = R.HandlerId;
	S.Scope = R.Scope;
	S.TaskName = R.TaskName;
	S.Description = R.Description;
	S.TriggerSummary = BridgeReactiveImpl::BuildSummaryTrigger(R.TriggerType, R.Selector);
	S.SubjectPath = BridgeReactiveImpl::BuildSubjectPath(R.Subject);
	S.ScriptPath = R.ScriptPath;
	S.Tags = R.Tags;
	S.Lifetime = BridgeReactiveImpl::LifetimeName(R.Lifetime);
	S.bPaused = R.bPaused;
	S.Stats = R.Stats;

	OutDetail.Summary = MoveTemp(S);
	OutDetail.Script = R.Script;
	OutDetail.ErrorPolicy = BridgeReactiveImpl::ErrorPolicyName(R.ErrorPolicy);
	OutDetail.ThrottleMs = R.ThrottleMs;
	OutDetail.RemainingCalls =
		(R.Lifetime == EBridgeHandlerLifetime::Count) ? R.RemainingCalls : -1;
	OutDetail.CreatedAt = R.CreatedAt.ToIso8601();
	return true;
}

bool UBridgeReactiveSubsystem::GetStats(const FString& HandlerId, FBridgeHandlerStats& OutStats) const
{
	if (const TSharedRef<FBridgeHandlerRecord>* R = Handlers.Find(HandlerId))
	{
		OutStats = (*R)->Stats;
		return true;
	}
	return false;
}

// ─── Deferred execution ─────────────────────────────────────────

void UBridgeReactiveSubsystem::DeferToNextTick(const FString& Script)
{
	if (!Script.IsEmpty())
	{
		DeferredScripts.Add(Script);
	}
}

// ─── Dispatch ───────────────────────────────────────────────────

void UBridgeReactiveSubsystem::Dispatch(
	EBridgeTrigger TriggerType,
	TWeakObjectPtr<UObject> Subject,
	FName Selector,
	const TMap<FString, FString>& ContextLiterals)
{
	DispatchLocked(TriggerType, Subject, Selector, ContextLiterals);
}

void UBridgeReactiveSubsystem::DispatchLocked(
	EBridgeTrigger TriggerType,
	const TWeakObjectPtr<UObject>& Subject,
	const FName& Selector,
	const TMap<FString, FString>& ContextLiterals)
{
	if (DispatchDepth >= BridgeReactiveImpl::MaxDispatchDepth)
	{
		UE_LOG(LogUnrealBridgeReactive, Error,
			TEXT("dispatch depth %d exceeded cap (%d) — aborting this fire"),
			DispatchDepth, BridgeReactiveImpl::MaxDispatchDepth);
		return;
	}
	if (DispatchDepth >= BridgeReactiveImpl::WarnDispatchDepth)
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("dispatch depth %d (warn threshold); check for recursive event chains"),
			DispatchDepth);
	}

	// Snapshot matching handler ids. Handlers may register/unregister
	// during dispatch without invalidating iteration.
	TArray<FString> SnapshotIds;
	SnapshotIds.Reserve(Handlers.Num());
	for (const auto& Pair : Handlers)
	{
		const FBridgeHandlerRecord& R = *Pair.Value;
		if (R.TriggerType != TriggerType)
		{
			continue;
		}
		// Subject match: either both null (global), or pointer-equal.
		UObject* RecSubject = R.Subject.Get();
		UObject* EvtSubject = Subject.Get();
		if (RecSubject != EvtSubject)
		{
			continue;
		}
		if (!R.Selector.IsNone() && R.Selector != Selector)
		{
			continue;
		}
		SnapshotIds.Add(Pair.Key);
	}
	if (SnapshotIds.Num() == 0)
	{
		return;
	}

	++DispatchDepth;
	TArray<FString> ToRemoveAfter;

	for (const FString& Id : SnapshotIds)
	{
		TSharedRef<FBridgeHandlerRecord>* Found = Handlers.Find(Id);
		if (!Found)
		{
			continue; // removed mid-snapshot
		}
		ExecuteHandlerOnce(**Found, ContextLiterals, ToRemoveAfter);
	}

	--DispatchDepth;

	for (const FString& Id : ToRemoveAfter)
	{
		RemoveByIdInternal(Id);
	}
}

void UBridgeReactiveSubsystem::DispatchOne(
	const FString& HandlerId,
	const TMap<FString, FString>& ContextLiterals)
{
	if (DispatchDepth >= BridgeReactiveImpl::MaxDispatchDepth)
	{
		UE_LOG(LogUnrealBridgeReactive, Error,
			TEXT("DispatchOne: depth %d exceeded cap (%d) — aborting"),
			DispatchDepth, BridgeReactiveImpl::MaxDispatchDepth);
		return;
	}
	if (DispatchDepth >= BridgeReactiveImpl::WarnDispatchDepth)
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("DispatchOne: depth %d (warn threshold)"), DispatchDepth);
	}

	TSharedRef<FBridgeHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found) return;

	++DispatchDepth;
	TArray<FString> ToRemoveAfter;
	ExecuteHandlerOnce(**Found, ContextLiterals, ToRemoveAfter);
	--DispatchDepth;

	for (const FString& Id : ToRemoveAfter)
	{
		RemoveByIdInternal(Id);
	}
}

void UBridgeReactiveSubsystem::ExecuteHandlerOnce(
	FBridgeHandlerRecord& R,
	const TMap<FString, FString>& ContextLiterals,
	TArray<FString>& OutToRemove)
{
	if (R.bPaused)
	{
		return;
	}

	// Subject liveness check for WhileSubjectAlive lifetime.
	if (R.Lifetime == EBridgeHandlerLifetime::WhileSubjectAlive &&
		!R.Subject.IsExplicitlyNull() && !R.Subject.IsValid())
	{
		OutToRemove.Add(R.HandlerId);
		return;
	}

	// Throttle.
	if (R.ThrottleMs > 0)
	{
		const double Now = FPlatformTime::Seconds();
		const double Delta = (Now - R.LastFirePlatformSeconds) * 1000.0;
		if (Delta < static_cast<double>(R.ThrottleMs))
		{
			return;
		}
		R.LastFirePlatformSeconds = Now;
	}
	else
	{
		R.LastFirePlatformSeconds = FPlatformTime::Seconds();
	}

	// Build + execute.
	const FString WrappedScript = BuildWrappedScript(R, ContextLiterals);
	const double T0 = FPlatformTime::Seconds();
	FString Error;
	const bool bOk = ExecutePythonScript(WrappedScript, Error);
	const int64 ElapsedUs = static_cast<int64>((FPlatformTime::Seconds() - T0) * 1'000'000.0);

	// Stats.
	R.Stats.Calls += 1;
	R.Stats.TotalMicroseconds += ElapsedUs;
	R.Stats.MaxMicroseconds = FMath::Max(R.Stats.MaxMicroseconds, ElapsedUs);
	if (GEditor)
	{
		if (UWorld* W = GEditor->GetEditorWorldContext().World())
		{
			R.Stats.LastFireTimeSeconds = W->GetTimeSeconds();
		}
	}
	if (!bOk)
	{
		R.Stats.ErrorCount += 1;
		R.Stats.LastError = Error;
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("handler %s '%s' failed: %s"),
			*R.HandlerId, *R.TaskName, *Error);

		if (R.ErrorPolicy == EBridgeErrorPolicy::Throw)
		{
			UE_LOG(LogUnrealBridgeReactive, Error,
				TEXT("handler %s '%s' (Throw policy): %s"),
				*R.HandlerId, *R.TaskName, *Error);
		}
		else if (R.ErrorPolicy == EBridgeErrorPolicy::LogUnregister)
		{
			OutToRemove.Add(R.HandlerId);
			return;
		}
	}

	// Lifetime decrement after successful (or non-unregister-on-error) call.
	bool bShouldRemove = false;
	BridgeReactiveImpl::ApplyLifetimeDecrement(R, bShouldRemove);
	if (bShouldRemove)
	{
		OutToRemove.Add(R.HandlerId);
	}
}

// ─── Persistent ticker registry ─────────────────────────────────

FTSTicker::FDelegateHandle UBridgeReactiveSubsystem::RegisterPersistentTicker(
	TFunction<bool(float)> Callback,
	const FString& DebugName)
{
	if (!Callback)
	{
		return FTSTicker::FDelegateHandle();
	}
	FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
		*FString::Printf(TEXT("UnrealBridgeReactive.Persistent.%s"), *DebugName),
		0.0f,
		[CB = MoveTemp(Callback)](float Dt) -> bool { return CB(Dt); });
	PersistentTickers.Add(Handle);
	return Handle;
}

void UBridgeReactiveSubsystem::UnregisterPersistentTicker(FTSTicker::FDelegateHandle& Handle)
{
	if (!Handle.IsValid()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(Handle);
	PersistentTickers.RemoveAll([&Handle](const FTSTicker::FDelegateHandle& H){ return H == Handle; });
	Handle.Reset();
}

// ─── Script assembly + execution ────────────────────────────────

FString UBridgeReactiveSubsystem::BuildWrappedScript(
	const FBridgeHandlerRecord& Record,
	const TMap<FString, FString>& ContextLiterals)
{
	// Build the ctx dict body from the adapter-supplied literals. Values are
	// already Python source expressions (quoted strings, numeric literals,
	// unreal.load_object(...) calls, etc.) — the adapter is responsible for
	// quoting/escaping its string values.
	FString CtxBody;
	for (const auto& Pair : ContextLiterals)
	{
		CtxBody += FString::Printf(TEXT("    '%s': %s,\n"),
			*BridgeReactiveImpl::EscapePythonStringLiteral(Pair.Key),
			*Pair.Value);
	}

	const FString TaskNameEsc = BridgeReactiveImpl::EscapePythonStringLiteral(Record.TaskName);
	const FString HandlerIdEsc = BridgeReactiveImpl::EscapePythonStringLiteral(Record.HandlerId);

	// Preamble sets up ctx + convenience names + state dicts + log/defer helpers,
	// then runs the user script inside a try/except that prints the traceback on
	// failure and re-raises to make ExecPythonCommandEx return false.
	//
	// Note: we do NOT base64-encode the user script here (unlike the bridge
	// server's sync exec path) because reactive handlers don't need captured
	// stdout — their output just goes to the editor log via the user's own
	// unreal.log() calls. The simpler concat keeps per-fire overhead tiny.
	FString Script;
	Script.Append(TEXT("import sys as _sys\n"));
	Script.Append(TEXT("import unreal\n"));
	Script.Append(TEXT("_mod = _sys.modules.setdefault('_bridge_reactive_state', type(_sys)('_bridge_reactive_state'))\n"));
	Script.Append(TEXT("if not hasattr(_mod, 'shared'):  _mod.shared = {}\n"));
	Script.Append(TEXT("if not hasattr(_mod, 'private'): _mod.private = {}\n"));
	Script.Append(FString::Printf(TEXT("handler_id = '%s'\n"), *HandlerIdEsc));
	Script.Append(FString::Printf(TEXT("handler_task_name = '%s'\n"), *TaskNameEsc));
	Script.Append(TEXT("bridge_state = _mod.shared\n"));
	Script.Append(TEXT("state = _mod.private.setdefault(handler_id, {})\n"));
	Script.Append(TEXT("ctx = {\n"));
	Script.Append(CtxBody);
	Script.Append(TEXT("}\n"));
	// Hoist the most common ctx keys to local names so handler scripts stay terse.
	Script.Append(TEXT("for _k, _v in list(ctx.items()):\n"));
	Script.Append(TEXT("    globals()[_k] = _v\n"));
	// Helpers.
	Script.Append(TEXT("def log(_msg):\n"));
	Script.Append(TEXT("    unreal.log('[reactive:' + handler_id + '|' + handler_task_name + '] ' + str(_msg))\n"));
	Script.Append(TEXT("def defer_to_next_tick(_src):\n"));
	Script.Append(TEXT("    unreal.UnrealBridgeReactiveLibrary.defer_to_next_tick(_src)\n"));
	// User script inside a try/except.
	Script.Append(TEXT("try:\n"));
	// Indent the user script by 4 spaces. Cheap split + rejoin.
	{
		TArray<FString> Lines;
		Record.Script.ParseIntoArrayLines(Lines, /*InCullEmpty=*/false);
		for (const FString& Line : Lines)
		{
			Script.Append(TEXT("    "));
			Script.Append(Line);
			Script.Append(TEXT("\n"));
		}
		if (Lines.Num() == 0)
		{
			Script.Append(TEXT("    pass\n"));
		}
	}
	Script.Append(TEXT("except Exception:\n"));
	Script.Append(TEXT("    import traceback as _tb\n"));
	Script.Append(TEXT("    _err = _tb.format_exc()\n"));
	Script.Append(TEXT("    unreal.log_error('[reactive:' + handler_id + '|' + handler_task_name + '] ' + _err)\n"));
	Script.Append(TEXT("    raise\n"));
	return Script;
}

bool UBridgeReactiveSubsystem::ExecutePythonScript(const FString& FullScript, FString& OutError)
{
	OutError.Reset();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		OutError = TEXT("PythonScriptPlugin unavailable");
		return false;
	}

	FPythonCommandEx CommandEx;
	CommandEx.Command = FullScript;
	CommandEx.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	CommandEx.FileExecutionScope = EPythonFileExecutionScope::Public;

	const bool bOk = PythonPlugin->ExecPythonCommandEx(CommandEx);
	if (!bOk)
	{
		// Concatenate any Error-severity entries so callers can surface
		// the Python traceback to stats / agent.
		for (const FPythonLogOutputEntry& Entry : CommandEx.LogOutput)
		{
			if (Entry.Type == EPythonLogOutputType::Error)
			{
				if (!OutError.IsEmpty())
				{
					OutError += TEXT("\n");
				}
				OutError += Entry.Output;
			}
		}
		if (OutError.IsEmpty())
		{
			OutError = CommandEx.CommandResult.IsEmpty()
				? FString(TEXT("python exec failed (no error text)"))
				: CommandEx.CommandResult;
		}
	}
	return bOk;
}

// ─── Cleanup hooks ──────────────────────────────────────────────

void UBridgeReactiveSubsystem::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	if (!World)
	{
		return;
	}
	TArray<FString> ToDelete;     // WhilePIE: purge entirely (user declared ephemeral)
	TArray<FString> ToDefer;      // Others: move to DeferredHandlers so they survive the session gap
	for (const auto& Pair : Handlers)
	{
		const FBridgeHandlerRecord& R = *Pair.Value;
		UObject* Subj = R.Subject.Get();
		if (!Subj) continue;
		if (Subj->GetWorld() != World) continue;
		if (R.Lifetime == EBridgeHandlerLifetime::WhilePIE)
		{
			ToDelete.Add(Pair.Key);
		}
		else
		{
			ToDefer.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToDelete)
	{
		UnregisterHandler(Id);
	}
	for (const FString& Id : ToDefer)
	{
		MoveHandlerToDeferred(Id);
	}
	if (ToDelete.Num() > 0 || ToDefer.Num() > 0)
	{
		UE_LOG(LogUnrealBridgeReactive, Log,
			TEXT("world cleanup on %s: deleted %d ephemeral, deferred %d for next session"),
			*World->GetName(), ToDelete.Num(), ToDefer.Num());
	}
}

void UBridgeReactiveSubsystem::MoveHandlerToDeferred(const FString& HandlerId)
{
	TSharedRef<FBridgeHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found) return;
	TSharedRef<FBridgeHandlerRecord> Ref = *Found;
	if (IBridgeReactiveAdapter* Adapter = FindAdapter(Ref->TriggerType))
	{
		Adapter->OnHandlerRemoved(*Ref);
	}
	// Copy the record for the deferred queue. Stale Subject weak ptr is cleared
	// so ResolveForRestore rebinds fresh from RegistrationContext.
	FBridgeHandlerRecord Copy = *Ref;
	Copy.Subject.Reset();
	DeferredHandlers.Add(MoveTemp(Copy));
	Handlers.Remove(HandlerId);
	// Intentionally no MarkDirty: the record is still persisted via
	// DeferredHandlers (BuildPersistenceJson iterates both sets), so nothing
	// on disk needs to change.
}

void UBridgeReactiveSubsystem::HandlePieEnded(bool /*bIsSimulating*/)
{
	TArray<FString> ToRemove;
	for (const auto& Pair : Handlers)
	{
		if (Pair.Value->Lifetime == EBridgeHandlerLifetime::WhilePIE)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToRemove)
	{
		UnregisterHandler(Id);
	}
	if (ToRemove.Num() > 0)
	{
		UE_LOG(LogUnrealBridgeReactive, Log,
			TEXT("PIE ended: removed %d WhilePIE handler(s)"), ToRemove.Num());
	}
}

// ─── Persistence (P6.B3) ──────────────────────────────────────────

void UBridgeReactiveSubsystem::MarkDirty()
{
	bPersistenceDirty = true;
}

FString UBridgeReactiveSubsystem::GetPersistencePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("UnrealBridge"), TEXT("reactive-handlers.json"));
}

int32 UBridgeReactiveSubsystem::GetDeferredHandlerCount() const
{
	return DeferredHandlers.Num();
}

FString UBridgeReactiveSubsystem::RestoreSingleRecord(FBridgeHandlerRecord&& Record)
{
	if (Record.HandlerId.IsEmpty())
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RestoreSingleRecord refused: empty HandlerId"));
		return FString();
	}
	IBridgeReactiveAdapter* Adapter = FindAdapter(Record.TriggerType);
	if (!Adapter)
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RestoreSingleRecord refused: no adapter for trigger '%s'"),
			*BridgeReactiveImpl::TriggerTypeName(Record.TriggerType));
		return FString();
	}
	// Caller is expected to have preserved HandlerId + pre-resolved Subject/
	// Selector/AdapterPayload. CreatedAt is preserved from JSON; fall back
	// to now() if it wasn't serialised (older schemas).
	if (Record.CreatedAt.GetTicks() == 0)
	{
		Record.CreatedAt = FDateTime::UtcNow();
	}
	TSharedRef<FBridgeHandlerRecord> Shared = MakeShared<FBridgeHandlerRecord>(MoveTemp(Record));
	const FString Id = Shared->HandlerId;
	// Sanity: reject if the id is already live (double-restore).
	if (Handlers.Contains(Id))
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RestoreSingleRecord refused: HandlerId '%s' already present"), *Id);
		return FString();
	}
	Handlers.Add(Id, Shared);
	Adapter->OnHandlerAdded(*Shared);
	return Id;
}

void UBridgeReactiveSubsystem::HandlePostPIEStarted(bool /*bIsSimulating*/)
{
	if (DeferredHandlers.Num() > 0)
	{
		RetryDeferredHandlers();
	}
}

void UBridgeReactiveSubsystem::RetryDeferredHandlers()
{
	TArray<FBridgeHandlerRecord> StillDeferred;
	int32 Restored = 0;
	for (FBridgeHandlerRecord& R : DeferredHandlers)
	{
		if (UUnrealBridgeReactiveLibrary::ResolveForRestore(R))
		{
			if (!RestoreSingleRecord(MoveTemp(R)).IsEmpty())
			{
				++Restored;
				continue;
			}
		}
		StillDeferred.Add(MoveTemp(R));
	}
	DeferredHandlers = MoveTemp(StillDeferred);
	if (Restored > 0)
	{
		UE_LOG(LogUnrealBridgeReactive, Log,
			TEXT("PostPIEStarted: restored %d deferred handler(s); %d still waiting"),
			Restored, DeferredHandlers.Num());
	}
}

// JSON helpers — build a JSON handler array + seq counters.
namespace BridgeReactivePersistenceImpl
{
	TSharedPtr<FJsonObject> RecordToJson(const FBridgeHandlerRecord& R)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("handler_id"),     R.HandlerId);
		O->SetStringField(TEXT("scope"),          R.Scope);
		O->SetStringField(TEXT("task_name"),      R.TaskName);
		O->SetStringField(TEXT("description"),    R.Description);
		TArray<TSharedPtr<FJsonValue>> TagsJson;
		for (const FString& T : R.Tags) TagsJson.Add(MakeShared<FJsonValueString>(T));
		O->SetArrayField(TEXT("tags"), TagsJson);
		O->SetStringField(TEXT("script"),         R.Script);
		O->SetStringField(TEXT("script_path"),    R.ScriptPath);
		O->SetStringField(TEXT("trigger_type"),   BridgeReactiveImpl::TriggerTypeName(R.TriggerType));
		TSharedPtr<FJsonObject> Reg = MakeShared<FJsonObject>();
		for (const auto& Pair : R.RegistrationContext)
		{
			Reg->SetStringField(Pair.Key, Pair.Value);
		}
		O->SetObjectField(TEXT("registration_context"), Reg);
		O->SetStringField(TEXT("lifetime"),       BridgeReactiveImpl::LifetimeName(R.Lifetime));
		O->SetNumberField(TEXT("remaining_calls"), R.RemainingCalls);
		O->SetStringField(TEXT("error_policy"),   BridgeReactiveImpl::ErrorPolicyName(R.ErrorPolicy));
		O->SetNumberField(TEXT("throttle_ms"),    R.ThrottleMs);
		O->SetStringField(TEXT("created_at"),     R.CreatedAt.ToIso8601());
		return O;
	}

	EBridgeTrigger ParseTriggerType(const FString& S)
	{
		if (S == TEXT("GameplayEvent"))       return EBridgeTrigger::GameplayEvent;
		if (S == TEXT("AttributeChanged"))    return EBridgeTrigger::AttributeChanged;
		if (S == TEXT("ActorLifecycle"))      return EBridgeTrigger::ActorLifecycle;
		if (S == TEXT("MovementModeChanged")) return EBridgeTrigger::MovementModeChanged;
		if (S == TEXT("AnimNotify"))          return EBridgeTrigger::AnimNotify;
		if (S == TEXT("InputAction"))         return EBridgeTrigger::InputAction;
		if (S == TEXT("Timer"))               return EBridgeTrigger::Timer;
		if (S == TEXT("AssetEvent"))          return EBridgeTrigger::AssetEvent;
		if (S == TEXT("PieEvent"))            return EBridgeTrigger::PieEvent;
		if (S == TEXT("BpCompiled"))          return EBridgeTrigger::BpCompiled;
		return EBridgeTrigger::None;
	}

	EBridgeHandlerLifetime ParseLifetime(const FString& S)
	{
		if (S == TEXT("Permanent"))         return EBridgeHandlerLifetime::Permanent;
		if (S == TEXT("Once"))              return EBridgeHandlerLifetime::Once;
		if (S == TEXT("Count"))             return EBridgeHandlerLifetime::Count;
		if (S == TEXT("WhilePIE"))          return EBridgeHandlerLifetime::WhilePIE;
		if (S == TEXT("WhileSubjectAlive")) return EBridgeHandlerLifetime::WhileSubjectAlive;
		return EBridgeHandlerLifetime::Permanent;
	}

	EBridgeErrorPolicy ParseErrorPolicy(const FString& S)
	{
		if (S == TEXT("LogContinue"))   return EBridgeErrorPolicy::LogContinue;
		if (S == TEXT("LogUnregister")) return EBridgeErrorPolicy::LogUnregister;
		if (S == TEXT("Throw"))         return EBridgeErrorPolicy::Throw;
		return EBridgeErrorPolicy::LogContinue;
	}

	bool JsonToRecord(const TSharedPtr<FJsonObject>& O, FBridgeHandlerRecord& Out)
	{
		if (!O.IsValid()) return false;
		Out.HandlerId     = O->GetStringField(TEXT("handler_id"));
		Out.Scope         = O->GetStringField(TEXT("scope"));
		Out.TaskName      = O->GetStringField(TEXT("task_name"));
		Out.Description   = O->GetStringField(TEXT("description"));
		const TArray<TSharedPtr<FJsonValue>>* TagsArr = nullptr;
		if (O->TryGetArrayField(TEXT("tags"), TagsArr) && TagsArr)
		{
			for (const TSharedPtr<FJsonValue>& V : *TagsArr)
			{
				if (V.IsValid()) Out.Tags.Add(V->AsString());
			}
		}
		Out.Script        = O->GetStringField(TEXT("script"));
		Out.ScriptPath    = O->GetStringField(TEXT("script_path"));
		Out.TriggerType   = ParseTriggerType(O->GetStringField(TEXT("trigger_type")));
		const TSharedPtr<FJsonObject>* RegObj = nullptr;
		if (O->TryGetObjectField(TEXT("registration_context"), RegObj) && RegObj && RegObj->IsValid())
		{
			for (const auto& Pair : (*RegObj)->Values)
			{
				if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
				{
					Out.RegistrationContext.Add(Pair.Key, Pair.Value->AsString());
				}
			}
		}
		Out.Lifetime      = ParseLifetime(O->GetStringField(TEXT("lifetime")));
		Out.RemainingCalls = static_cast<int32>(O->GetNumberField(TEXT("remaining_calls")));
		Out.ErrorPolicy   = ParseErrorPolicy(O->GetStringField(TEXT("error_policy")));
		Out.ThrottleMs    = static_cast<int32>(O->GetNumberField(TEXT("throttle_ms")));
		FString Created;
		if (O->TryGetStringField(TEXT("created_at"), Created) && !Created.IsEmpty())
		{
			FDateTime::ParseIso8601(*Created, Out.CreatedAt);
		}
		return !Out.HandlerId.IsEmpty() && Out.TriggerType != EBridgeTrigger::None;
	}
}

FString UBridgeReactiveSubsystem::BuildPersistenceJson() const
{
	using namespace BridgeReactivePersistenceImpl;
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("runtime_seq"), RuntimeSeq);
	Root->SetNumberField(TEXT("editor_seq"),  EditorSeq);

	TArray<TSharedPtr<FJsonValue>> HandlersArr;
	// Live handlers: persist non-WhilePIE.
	for (const auto& Pair : Handlers)
	{
		const FBridgeHandlerRecord& R = *Pair.Value;
		if (R.Lifetime == EBridgeHandlerLifetime::WhilePIE) continue;
		HandlersArr.Add(MakeShared<FJsonValueObject>(RecordToJson(R)));
	}
	// Deferred handlers: keep them persisted too, so a session that never
	// starts PIE doesn't lose its pending restore records.
	for (const FBridgeHandlerRecord& R : DeferredHandlers)
	{
		if (R.Lifetime == EBridgeHandlerLifetime::WhilePIE) continue;
		HandlersArr.Add(MakeShared<FJsonValueObject>(RecordToJson(R)));
	}
	Root->SetArrayField(TEXT("handlers"), HandlersArr);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out, 0);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

bool UBridgeReactiveSubsystem::SaveAllHandlers()
{
	const FString Path = GetPersistencePath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*FPaths::GetPath(Path));
	const FString Content = BuildPersistenceJson();
	if (!FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("SaveAllHandlers: write failed (%s)"), *Path);
		return false;
	}
	UE_LOG(LogUnrealBridgeReactive, Verbose,
		TEXT("SaveAllHandlers: wrote %d handler(s) to %s"),
		Handlers.Num() + DeferredHandlers.Num(), *Path);
	return true;
}

int32 UBridgeReactiveSubsystem::RestoreFromJson(const FString& JsonText)
{
	using namespace BridgeReactivePersistenceImpl;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RestoreFromJson: invalid JSON — renaming file and starting fresh"));
		return 0;
	}

	const int32 Version = static_cast<int32>(Root->GetNumberField(TEXT("version")));
	if (Version != 1)
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("RestoreFromJson: unsupported schema version %d (expected 1); skipping"),
			Version);
		return 0;
	}

	// Seq counters: keep whichever is larger so new IDs never collide.
	const int32 SavedRuntimeSeq = static_cast<int32>(Root->GetNumberField(TEXT("runtime_seq")));
	const int32 SavedEditorSeq  = static_cast<int32>(Root->GetNumberField(TEXT("editor_seq")));
	RuntimeSeq = FMath::Max(RuntimeSeq, SavedRuntimeSeq);
	EditorSeq  = FMath::Max(EditorSeq,  SavedEditorSeq);

	const TArray<TSharedPtr<FJsonValue>>* HandlersArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("handlers"), HandlersArr) || !HandlersArr)
	{
		return 0;
	}

	int32 RestoredNow = 0;
	int32 Deferred = 0;
	for (const TSharedPtr<FJsonValue>& V : *HandlersArr)
	{
		if (!V.IsValid() || V->Type != EJson::Object) continue;
		FBridgeHandlerRecord R;
		if (!JsonToRecord(V->AsObject(), R)) continue;
		if (R.Lifetime == EBridgeHandlerLifetime::WhilePIE) continue;

		if (UUnrealBridgeReactiveLibrary::ResolveForRestore(R))
		{
			if (!RestoreSingleRecord(MoveTemp(R)).IsEmpty())
			{
				++RestoredNow;
				continue;
			}
		}
		DeferredHandlers.Add(MoveTemp(R));
		++Deferred;
	}

	if (RestoredNow > 0 || Deferred > 0)
	{
		UE_LOG(LogUnrealBridgeReactive, Log,
			TEXT("RestoreFromJson: restored %d handler(s); %d deferred (await PIE start)"),
			RestoredNow, Deferred);
	}
	return RestoredNow;
}

int32 UBridgeReactiveSubsystem::LoadAllHandlers()
{
	const FString Path = GetPersistencePath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.FileExists(*Path))
	{
		return 0;
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogUnrealBridgeReactive, Warning,
			TEXT("LoadAllHandlers: read failed (%s)"), *Path);
		return 0;
	}
	// Clear existing registry first so load is idempotent.
	if (Handlers.Num() > 0)
	{
		TArray<FString> Existing;
		Handlers.GetKeys(Existing);
		for (const FString& Id : Existing) { UnregisterHandler(Id); }
	}
	DeferredHandlers.Reset();

	const int32 Restored = RestoreFromJson(Text);

	// Don't let this pre-existing-file load trigger an immediate re-save.
	bPersistenceDirty = false;
	return Restored;
}

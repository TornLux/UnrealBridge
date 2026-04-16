#include "UnrealBridgeReactiveSubsystem.h"
#include "UnrealBridgeReactiveAdapter.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

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

	UE_LOG(LogUnrealBridgeReactive, Log,
		TEXT("UBridgeReactiveSubsystem initialized (%d adapter(s))."), Adapters.Num());
}

void UBridgeReactiveSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FEditorDelegates::EndPIE.Remove(PieEndedHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(DeferTickerHandle);

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
	TArray<FString> ToRemove;
	for (const auto& Pair : Handlers)
	{
		const FBridgeHandlerRecord& R = *Pair.Value;
		UObject* Subj = R.Subject.Get();
		if (!Subj)
		{
			continue;
		}
		if (Subj->GetWorld() == World)
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
			TEXT("world cleanup: removed %d handler(s) tied to %s"),
			ToRemove.Num(), *World->GetName());
	}
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

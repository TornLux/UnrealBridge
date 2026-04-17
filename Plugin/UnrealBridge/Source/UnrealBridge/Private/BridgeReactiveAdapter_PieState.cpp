#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactivePie, Log, All);

/**
 * Binds eight FEditorDelegates PIE-phase multicasts. All handlers share a
 * single binding per phase (refcount-by-handler-count). Subject is always
 * null; Selector is the phase name ("BeginPIE", "EndPIE", ...). The
 * subsystem's existing core EndPIE lambda (purges WhilePIE handlers) runs
 * BEFORE this adapter's EndPIE lambda because the subsystem's bind happens
 * during Initialize while the adapter's bind happens on first handler
 * registration — so WhilePIE PieEvent handlers listening for "EndPIE" get
 * purged before their own event fires. Document this in the library entry
 * point; agents who want to hear EndPIE must use Permanent (or PrePIEEnded,
 * which fires BEFORE the purge).
 */
class FBridgePieStateAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::PieEvent; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& /*Record*/) override
	{
		HandlerCount += 1;
		if (HandlerCount == 1)
		{
			BindDelegates();
		}
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& /*Record*/) override
	{
		HandlerCount = FMath::Max(0, HandlerCount - 1);
		if (HandlerCount == 0)
		{
			UnbindDelegates();
		}
	}

	virtual void Shutdown() override
	{
		HandlerCount = 0;
		UnbindDelegates();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),        TEXT("str — always 'pie_event'"));
		D.Add(TEXT("phase"),          TEXT("str — one of 'PreBeginPIE', 'BeginPIE', 'PostPIEStarted', 'PrePIEEnded', 'EndPIE', 'PausePIE', 'ResumePIE', 'SingleStepPIE'"));
		D.Add(TEXT("is_simulating"),  TEXT("bool — true if the PIE session is a Simulate (no possession), false for standard Play"));
		return D;
	}

private:
	void BindDelegates()
	{
		PreBeginPIE    = FEditorDelegates::PreBeginPIE.AddLambda([this](const bool S)    { FirePhase(TEXT("PreBeginPIE"), S); });
		BeginPIE       = FEditorDelegates::BeginPIE.AddLambda([this](const bool S)       { FirePhase(TEXT("BeginPIE"), S); });
		PostPIEStarted = FEditorDelegates::PostPIEStarted.AddLambda([this](const bool S) { FirePhase(TEXT("PostPIEStarted"), S); });
		PrePIEEnded    = FEditorDelegates::PrePIEEnded.AddLambda([this](const bool S)    { FirePhase(TEXT("PrePIEEnded"), S); });
		EndPIE         = FEditorDelegates::EndPIE.AddLambda([this](const bool S)         { FirePhase(TEXT("EndPIE"), S); });
		PausePIE       = FEditorDelegates::PausePIE.AddLambda([this](const bool S)       { FirePhase(TEXT("PausePIE"), S); });
		ResumePIE      = FEditorDelegates::ResumePIE.AddLambda([this](const bool S)      { FirePhase(TEXT("ResumePIE"), S); });
		SingleStepPIE  = FEditorDelegates::SingleStepPIE.AddLambda([this](const bool S)  { FirePhase(TEXT("SingleStepPIE"), S); });
	}

	void UnbindDelegates()
	{
		if (PreBeginPIE.IsValid())    { FEditorDelegates::PreBeginPIE.Remove(PreBeginPIE);       PreBeginPIE.Reset(); }
		if (BeginPIE.IsValid())       { FEditorDelegates::BeginPIE.Remove(BeginPIE);             BeginPIE.Reset(); }
		if (PostPIEStarted.IsValid()) { FEditorDelegates::PostPIEStarted.Remove(PostPIEStarted); PostPIEStarted.Reset(); }
		if (PrePIEEnded.IsValid())    { FEditorDelegates::PrePIEEnded.Remove(PrePIEEnded);       PrePIEEnded.Reset(); }
		if (EndPIE.IsValid())         { FEditorDelegates::EndPIE.Remove(EndPIE);                 EndPIE.Reset(); }
		if (PausePIE.IsValid())       { FEditorDelegates::PausePIE.Remove(PausePIE);             PausePIE.Reset(); }
		if (ResumePIE.IsValid())      { FEditorDelegates::ResumePIE.Remove(ResumePIE);           ResumePIE.Reset(); }
		if (SingleStepPIE.IsValid())  { FEditorDelegates::SingleStepPIE.Remove(SingleStepPIE);   SingleStepPIE.Reset(); }
	}

	void FirePhase(const TCHAR* PhaseName, bool bIsSimulating)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub) return;

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),       TEXT("'pie_event'"));
		Ctx.Add(TEXT("phase"),         FString::Printf(TEXT("'%s'"), PhaseName));
		Ctx.Add(TEXT("is_simulating"), bIsSimulating ? TEXT("True") : TEXT("False"));

		Sub->Dispatch(EBridgeTrigger::PieEvent,
			TWeakObjectPtr<UObject>(),
			FName(PhaseName),
			Ctx);
	}

	int32 HandlerCount = 0;
	FDelegateHandle PreBeginPIE;
	FDelegateHandle BeginPIE;
	FDelegateHandle PostPIEStarted;
	FDelegateHandle PrePIEEnded;
	FDelegateHandle EndPIE;
	FDelegateHandle PausePIE;
	FDelegateHandle ResumePIE;
	FDelegateHandle SingleStepPIE;
};

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakePieStateAdapter()
	{
		return MakeUnique<FBridgePieStateAdapter>();
	}
}

#include "UnrealBridgeEditorHealth.h"

#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"

namespace
{
	FString UnrealBridgeHealthUtcNow()
	{
		return FDateTime::UtcNow().ToIso8601();
	}
}

FUnrealBridgeEditorHealthCache::FUnrealBridgeEditorHealthCache()
	: FUnrealBridgeEditorHealthCache(
		[]() { return FPlatformTime::Seconds(); },
		[]() { return UnrealBridgeHealthUtcNow(); })
{
}

FUnrealBridgeEditorHealthCache::FUnrealBridgeEditorHealthCache(FClock InClock)
	: FUnrealBridgeEditorHealthCache(MoveTemp(InClock), []() { return UnrealBridgeHealthUtcNow(); })
{
}

FUnrealBridgeEditorHealthCache::FUnrealBridgeEditorHealthCache(FClock InClock, FUtcClock InUtcClock)
	: Clock(MoveTemp(InClock))
	, UtcClock(MoveTemp(InUtcClock))
{
	check(Clock);
	check(UtcClock);
}

void FUnrealBridgeEditorHealthCache::Reset()
{
	FScopeLock ScopeLock(&Lock);
	bReady = false;
	bEngineTickObserved = false;
	EngineTickSequence = 0;
	LastEngineTickSeconds = 0.0;
	LastEngineTickUtc.Reset();
	bSlateTickObserved = false;
	SlateTickSequence = 0;
	LastSlateTickSeconds = 0.0;
	LastSlateTickUtc.Reset();
	AttentionId = 0;
	ActiveModalFirstSeenUtc.Reset();
	Modal = FUnrealBridgeModalAttention();
}

void FUnrealBridgeEditorHealthCache::SetReady(bool bInReady)
{
	FScopeLock ScopeLock(&Lock);
	bReady = bInReady;
}

void FUnrealBridgeEditorHealthCache::RecordEngineTick()
{
	const double NowSeconds = Clock();
	const FString NowUtc = UtcClock();
	FScopeLock ScopeLock(&Lock);
	bEngineTickObserved = true;
	++EngineTickSequence;
	LastEngineTickSeconds = NowSeconds;
	LastEngineTickUtc = NowUtc;
}

void FUnrealBridgeEditorHealthCache::RecordSlateTick(const FUnrealBridgeModalAttention& InModal)
{
	const double NowSeconds = Clock();
	const FString NowUtc = UtcClock();
	FScopeLock ScopeLock(&Lock);
	bSlateTickObserved = true;
	++SlateTickSequence;
	LastSlateTickSeconds = NowSeconds;
	LastSlateTickUtc = NowUtc;

	const bool bModalChanged = InModal.bPresent != Modal.bPresent
		|| (InModal.bPresent && InModal.WindowGeneration != Modal.WindowGeneration);
	if (bModalChanged)
	{
		++AttentionId;
		ActiveModalFirstSeenUtc = InModal.bPresent ? NowUtc : FString();
	}
	Modal = InModal;
}

FUnrealBridgeEditorHealthReadout FUnrealBridgeEditorHealthCache::Read(double StaleAfterSeconds)
{
	const double NowSeconds = Clock();
	const double StaleAfterMs = FMath::Max(0.0, StaleAfterSeconds) * 1000.0;

	FScopeLock ScopeLock(&Lock);
	FUnrealBridgeEditorHealthReadout Out;
	Out.bReady = bReady;
	Out.bEngineTickObserved = bEngineTickObserved;
	Out.EngineTickSequence = EngineTickSequence;
	Out.LastEngineTickUtc = LastEngineTickUtc;
	if (bEngineTickObserved)
	{
		Out.EngineTickAgeMs = AgeMilliseconds(NowSeconds, LastEngineTickSeconds);
	}
	Out.bEngineStale = !bEngineTickObserved || Out.EngineTickAgeMs > StaleAfterMs;

	Out.bSlateTickObserved = bSlateTickObserved;
	Out.SlateTickSequence = SlateTickSequence;
	Out.LastSlateTickUtc = LastSlateTickUtc;
	if (bSlateTickObserved)
	{
		Out.SlateTickAgeMs = AgeMilliseconds(NowSeconds, LastSlateTickSeconds);
	}
	Out.bSlateStale = !bSlateTickObserved || Out.SlateTickAgeMs > StaleAfterMs;
	Out.bStale = Out.bEngineStale || Out.bSlateStale;
	Out.UiState = !bSlateTickObserved
		? TEXT("unavailable")
		: (Modal.bDebugging ? TEXT("debugging")
			: (Modal.bPresent ? TEXT("slate_modal") : (bReady ? TEXT("normal") : TEXT("initializing"))));
	Out.AttentionId = AttentionId;
	Out.ActiveModalFirstSeenUtc = ActiveModalFirstSeenUtc;
	Out.Modal = Modal;
	return Out;
}

double FUnrealBridgeEditorHealthCache::AgeMilliseconds(double NowSeconds, double ObservedSeconds)
{
	return FMath::Max(0.0, NowSeconds - ObservedSeconds) * 1000.0;
}

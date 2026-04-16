#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveTimer, Log, All);

/**
 * Multiplexed timer driver. Unlike other adapters, Timer handlers have no
 * underlying UE delegate to bind — the adapter owns one FTSTicker via the
 * subsystem's persistent-ticker registrar, iterates its own entries on each
 * tick, and fires each handler whose interval has elapsed via
 * UBridgeReactiveSubsystem::DispatchOne (bypasses Subject/Selector filter).
 *
 * AdapterPayload on the record stores the interval as a string like "0.5".
 * Subject is ignored (timers are global); Selector is ignored.
 */
class FBridgeTimerAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::Timer; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		const double Interval = FCString::Atod(*Record.AdapterPayload);
		if (Interval <= 0.0)
		{
			UE_LOG(LogUnrealBridgeReactiveTimer, Warning,
				TEXT("OnHandlerAdded %s: interval '%s' must be > 0 seconds"),
				*Record.HandlerId, *Record.AdapterPayload);
			return;
		}

		FTimerEntry E;
		E.HandlerId = Record.HandlerId;
		E.IntervalSeconds = Interval;
		E.LastFirePlatformTime = FPlatformTime::Seconds();
		E.FireCount = 0;
		Entries.Add(MoveTemp(E));

		EnsureTickerRunning();
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		Entries.RemoveAll([&Record](const FTimerEntry& E){ return E.HandlerId == Record.HandlerId; });
		StopTickerIfIdle();
	}

	virtual void Shutdown() override
	{
		Entries.Reset();
		StopTickerIfIdle();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),             TEXT("str — always 'timer'"));
		D.Add(TEXT("interval_seconds"),    TEXT("float — the configured interval"));
		D.Add(TEXT("elapsed_since_last"),  TEXT("float — wall-clock seconds since previous fire (≈ interval but jitters ±tick granularity)"));
		D.Add(TEXT("fire_count"),          TEXT("int — total fires so far (1 on first fire)"));
		D.Add(TEXT("world_time"),          TEXT("float — PIE world GetTimeSeconds(), 0.0 if no PIE world"));
		D.Add(TEXT("editor_time"),         TEXT("float — FPlatformTime::Seconds() when the fire was dispatched"));
		return D;
	}

private:
	struct FTimerEntry
	{
		FString HandlerId;
		double  IntervalSeconds = 1.0;
		double  LastFirePlatformTime = 0.0;
		int64   FireCount = 0;
	};

	void EnsureTickerRunning()
	{
		if (TickerHandle.IsValid()) return;
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub) return;
		TickerHandle = Sub->RegisterPersistentTicker(
			[this](float Dt) -> bool { return Tick(Dt); },
			TEXT("Timer"));
	}

	void StopTickerIfIdle()
	{
		if (Entries.Num() > 0) return;
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (Sub && TickerHandle.IsValid())
		{
			Sub->UnregisterPersistentTicker(TickerHandle);
		}
	}

	bool Tick(float /*DeltaSeconds*/)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub || Entries.Num() == 0)
		{
			return true;
		}

		const double NowPlat = FPlatformTime::Seconds();
		double WorldNow = 0.0;
		if (GEditor)
		{
			for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
			{
				if (Ctx.WorldType == EWorldType::PIE && Ctx.World() && Ctx.World()->HasBegunPlay())
				{
					WorldNow = Ctx.World()->GetTimeSeconds();
					break;
				}
			}
		}

		// Snapshot ids first — DispatchOne may add/remove handlers, mutating Entries.
		struct FPending { FString Id; double Elapsed; int64 FireCount; double Interval; };
		TArray<FPending> ToFire;
		ToFire.Reserve(Entries.Num());
		for (FTimerEntry& E : Entries)
		{
			const double Elapsed = NowPlat - E.LastFirePlatformTime;
			if (Elapsed + KINDA_SMALL_NUMBER >= E.IntervalSeconds)
			{
				E.LastFirePlatformTime = NowPlat;
				E.FireCount += 1;
				ToFire.Add({ E.HandlerId, Elapsed, E.FireCount, E.IntervalSeconds });
			}
		}

		for (const FPending& P : ToFire)
		{
			TMap<FString, FString> Ctx;
			Ctx.Add(TEXT("trigger"),            TEXT("'timer'"));
			Ctx.Add(TEXT("interval_seconds"),   FString::Printf(TEXT("%f"), P.Interval));
			Ctx.Add(TEXT("elapsed_since_last"), FString::Printf(TEXT("%f"), P.Elapsed));
			Ctx.Add(TEXT("fire_count"),         FString::Printf(TEXT("%lld"), P.FireCount));
			Ctx.Add(TEXT("world_time"),         FString::Printf(TEXT("%f"), WorldNow));
			Ctx.Add(TEXT("editor_time"),        FString::Printf(TEXT("%f"), NowPlat));
			Sub->DispatchOne(P.Id, Ctx);
		}

		return true; // keep ticking
	}

	TArray<FTimerEntry> Entries;
	FTSTicker::FDelegateHandle TickerHandle;
};

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeTimerAdapter()
	{
		return MakeUnique<FBridgeTimerAdapter>();
	}
}

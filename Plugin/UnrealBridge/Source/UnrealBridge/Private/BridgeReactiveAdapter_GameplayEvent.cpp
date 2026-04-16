#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveGE, Log, All);

namespace BridgeReactiveAdapterImpl_GE
{
	/** Escape a string for embedding inside a single-quoted Python literal. */
	FString EscapeSingleQuoted(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\''))
			{
				Out.AppendChar(TEXT('\\'));
			}
			Out.AppendChar(C);
		}
		return Out;
	}

	/** Render a UObject* as a Python expression ("unreal.load_object(None, '/path')" or "None"). */
	FString RenderObjectLiteral(const UObject* Obj)
	{
		if (!Obj)
		{
			return TEXT("None");
		}
		const FString Path = Obj->GetPathName();
		return FString::Printf(TEXT("unreal.load_object(None, '%s')"), *EscapeSingleQuoted(Path));
	}

	FString RenderTagContainerLiteral(const FGameplayTagContainer& Tags)
	{
		if (Tags.Num() == 0)
		{
			return TEXT("[]");
		}
		FString Out = TEXT("[");
		bool bFirst = true;
		for (const FGameplayTag& Tag : Tags)
		{
			if (!bFirst) Out += TEXT(", ");
			bFirst = false;
			Out += FString::Printf(TEXT("'%s'"), *EscapeSingleQuoted(Tag.ToString()));
		}
		Out += TEXT("]");
		return Out;
	}

	/**
	 * Resolve an ASC from an actor across the standard placement patterns.
	 * Duplicate of UnrealBridgeReactiveLibrary's ResolveActorASC — kept local
	 * so the adapter is self-contained and doesn't depend on the library cpp.
	 */
	UAbilitySystemComponent* ResolveActorASC(AActor* Actor)
	{
		if (!Actor) return nullptr;

		auto FromObject = [](UObject* Obj) -> UAbilitySystemComponent*
		{
			if (!Obj) return nullptr;
			if (IAbilitySystemInterface* I = Cast<IAbilitySystemInterface>(Obj))
			{
				if (UAbilitySystemComponent* ASC = I->GetAbilitySystemComponent()) return ASC;
			}
			if (AActor* A = Cast<AActor>(Obj))
			{
				return A->FindComponentByClass<UAbilitySystemComponent>();
			}
			return nullptr;
		};

		if (UAbilitySystemComponent* ASC = FromObject(Actor)) return ASC;
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (APlayerState* PS = Pawn->GetPlayerState())
			{
				if (UAbilitySystemComponent* ASC = FromObject(PS)) return ASC;
			}
			if (AController* Ctrl = Pawn->GetController())
			{
				if (UAbilitySystemComponent* ASC = FromObject(Ctrl)) return ASC;
				if (APlayerController* PC = Cast<APlayerController>(Ctrl))
				{
					if (UAbilitySystemComponent* ASC = FromObject(PC->PlayerState)) return ASC;
				}
			}
		}
		return nullptr;
	}

	/** True for PIE / PIE-server worlds. Editor-world ASCs (asset previews) excluded. */
	bool IsPieWorld(const UWorld* W)
	{
		if (!W) return false;
		const EWorldType::Type T = W->WorldType;
		return T == EWorldType::PIE || T == EWorldType::Game;
	}
}

/**
 * Binds to UAbilitySystemComponent::GenericGameplayEventCallbacks[Tag]. Two
 * registration shapes share one binding cache:
 *
 * - **Per-subject** handlers (Subject = ASC) bind a lambda when first added on
 *   that (ASC, Tag), refcounted by HandlerCount.
 * - **Global** handlers (Subject = nullptr, AdapterPayload = "global") fire on
 *   any live ASC for the matching Tag. The adapter walks live PIE-world ASCs
 *   at register time, ensures a (ASC, Tag) binding exists for each (refcounted
 *   by GlobalRefCount), and watches UWorld::OnActorSpawned to bind to
 *   late-spawning ASCs. PlayerState-attached ASCs are typically not present
 *   on the actor at OnActorSpawned, so the spawn callback also schedules a
 *   one-tick deferred re-resolve via FTSTicker.
 *
 * Lambda dispatches twice on fire: once with Subject = ASC (per-subject
 * handlers match), once with Subject = nullptr (global handlers match — the
 * subsystem's matcher already excludes per-subject records when the event
 * Subject is null because pointer equality fails).
 */
class FBridgeGameplayEventAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::GameplayEvent; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		if (Record.Selector.IsNone())
		{
			UE_LOG(LogUnrealBridgeReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: Selector (tag) required"), *Record.HandlerId);
			return;
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Record.Selector, /*bErrorIfNotFound=*/false);
		if (!Tag.IsValid())
		{
			UE_LOG(LogUnrealBridgeReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: tag '%s' is not registered"),
				*Record.HandlerId, *Record.Selector.ToString());
			return;
		}

		const bool bGlobal = Record.AdapterPayload == TEXT("global");
		if (bGlobal)
		{
			GlobalTagCounts.FindOrAdd(Tag) += 1;
			SnapshotGlobalBindingsForTag(Tag);
			EnsureWorldWatchers();
			return;
		}

		// Per-subject path.
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC)
		{
			UE_LOG(LogUnrealBridgeReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: Subject is not an ASC"), *Record.HandlerId);
			return;
		}
		EnsureBinding(ASC, Tag, /*bForGlobal=*/false);
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Record.Selector, false);
		if (!Tag.IsValid())
		{
			return;
		}

		const bool bGlobal = Record.AdapterPayload == TEXT("global");
		if (bGlobal)
		{
			int32* CountPtr = GlobalTagCounts.Find(Tag);
			if (!CountPtr) return;
			--(*CountPtr);
			if (*CountPtr <= 0)
			{
				GlobalTagCounts.Remove(Tag);
				// Drop the global refcount on every binding for this tag.
				for (int32 i = Bindings.Num() - 1; i >= 0; --i)
				{
					FBinding& B = Bindings[i];
					if (B.Tag != Tag) continue;
					if (B.GlobalRefCount > 0)
					{
						B.GlobalRefCount = 0;
					}
					if (B.HandlerCount + B.GlobalRefCount <= 0)
					{
						UnbindOne(B);
						Bindings.RemoveAtSwap(i);
					}
				}
			}
			TeardownWorldWatchersIfIdle();
			return;
		}

		// Per-subject path.
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC) return;
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.ASC.Get() == ASC && B.Tag == Tag)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount + B.GlobalRefCount <= 0)
				{
					UnbindOne(B);
					Bindings.RemoveAtSwap(i);
				}
				return;
			}
		}
	}

	virtual void Shutdown() override
	{
		for (FBinding& B : Bindings)
		{
			UnbindOne(B);
		}
		Bindings.Reset();
		GlobalTagCounts.Reset();
		for (auto& Pair : WorldSpawnHandles)
		{
			if (UWorld* W = Pair.Key.Get())
			{
				W->RemoveOnActorSpawnedHandler(Pair.Value);
			}
		}
		WorldSpawnHandles.Reset();
		if (PostPieStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPieStartedHandle);
			PostPieStartedHandle.Reset();
		}
		if (PieEndHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(PieEndHandle);
			PieEndHandle.Reset();
		}
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),                 TEXT("str — always 'gameplay_event'"));
		D.Add(TEXT("tag"),                     TEXT("str — the GameplayTag that fired"));
		D.Add(TEXT("source_asc"),              TEXT("unreal.AbilitySystemComponent | None — the ASC that received the event (useful for global handlers to know which ASC fired)"));
		D.Add(TEXT("event_instigator"),        TEXT("unreal.Object | None — FGameplayEventData::Instigator"));
		D.Add(TEXT("event_target"),            TEXT("unreal.Object | None — FGameplayEventData::Target"));
		D.Add(TEXT("event_optional_object"),   TEXT("unreal.Object | None"));
		D.Add(TEXT("event_optional_object2"),  TEXT("unreal.Object | None"));
		D.Add(TEXT("event_magnitude"),         TEXT("float — FGameplayEventData::EventMagnitude"));
		D.Add(TEXT("event_instigator_tags"),   TEXT("list[str]"));
		D.Add(TEXT("event_target_tags"),       TEXT("list[str]"));
		return D;
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayTag Tag;
		FDelegateHandle Handle;
		int32 HandlerCount = 0;       // per-subject handler refcount
		int32 GlobalRefCount = 0;     // 1 if any global handler covers this (ASC, Tag), else 0
	};
	TArray<FBinding> Bindings;

	/** Tag → number of global handlers registered for that tag. */
	TMap<FGameplayTag, int32> GlobalTagCounts;

	/** Worlds we've subscribed to OnActorSpawned for. */
	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> WorldSpawnHandles;

	/** PostPIEStarted subscription so we (re)snapshot ASCs when PIE comes up after registration. */
	FDelegateHandle PostPieStartedHandle;
	/** EndPIE subscription to clean up bindings whose ASC just got torn down. */
	FDelegateHandle PieEndHandle;

	/** Bind the ASC delegate if not already; bump the requested refcount. */
	void EnsureBinding(UAbilitySystemComponent* ASC, const FGameplayTag& Tag, bool bForGlobal)
	{
		if (!ASC) return;
		for (FBinding& B : Bindings)
		{
			if (B.ASC.Get() == ASC && B.Tag == Tag)
			{
				if (bForGlobal)
				{
					// Global contributes at most 1 refcount per (ASC, Tag) — multiple
					// global handlers on the same tag share the binding.
					if (B.GlobalRefCount == 0) B.GlobalRefCount = 1;
				}
				else
				{
					B.HandlerCount += 1;
				}
				return;
			}
		}

		// First handler for (ASC, Tag).
		FBinding NB;
		NB.ASC = ASC;
		NB.Tag = Tag;
		NB.HandlerCount = bForGlobal ? 0 : 1;
		NB.GlobalRefCount = bForGlobal ? 1 : 0;

		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
		const FGameplayTag CaptureTag = Tag;

		NB.Handle = ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddLambda(
			[WeakASC, CaptureTag](const FGameplayEventData* EventData)
			{
				UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
				if (!Sub) return;

				UAbilitySystemComponent* SourceASC = WeakASC.Get();

				TMap<FString, FString> Ctx;
				Ctx.Add(TEXT("trigger"),    TEXT("'gameplay_event'"));
				Ctx.Add(TEXT("tag"),        FString::Printf(TEXT("'%s'"),
					*BridgeReactiveAdapterImpl_GE::EscapeSingleQuoted(CaptureTag.ToString())));
				Ctx.Add(TEXT("source_asc"), BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(SourceASC));

				if (EventData)
				{
					Ctx.Add(TEXT("event_instigator"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->Instigator.Get()));
					Ctx.Add(TEXT("event_target"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->Target.Get()));
					Ctx.Add(TEXT("event_optional_object"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->OptionalObject.Get()));
					Ctx.Add(TEXT("event_optional_object2"),
						BridgeReactiveAdapterImpl_GE::RenderObjectLiteral(EventData->OptionalObject2.Get()));
					Ctx.Add(TEXT("event_magnitude"),
						FString::Printf(TEXT("%f"), EventData->EventMagnitude));
					Ctx.Add(TEXT("event_instigator_tags"),
						BridgeReactiveAdapterImpl_GE::RenderTagContainerLiteral(EventData->InstigatorTags));
					Ctx.Add(TEXT("event_target_tags"),
						BridgeReactiveAdapterImpl_GE::RenderTagContainerLiteral(EventData->TargetTags));
				}
				else
				{
					Ctx.Add(TEXT("event_instigator"),        TEXT("None"));
					Ctx.Add(TEXT("event_target"),            TEXT("None"));
					Ctx.Add(TEXT("event_optional_object"),   TEXT("None"));
					Ctx.Add(TEXT("event_optional_object2"),  TEXT("None"));
					Ctx.Add(TEXT("event_magnitude"),         TEXT("0.0"));
					Ctx.Add(TEXT("event_instigator_tags"),   TEXT("[]"));
					Ctx.Add(TEXT("event_target_tags"),       TEXT("[]"));
				}

				// Per-subject handlers match (Subject = ASC).
				Sub->Dispatch(EBridgeTrigger::GameplayEvent,
					TWeakObjectPtr<UObject>(SourceASC), CaptureTag.GetTagName(), Ctx);

				// Global handlers match (Subject = nullptr). The subsystem's
				// pointer-equality test excludes per-subject records on this
				// pass — no double-firing.
				Sub->Dispatch(EBridgeTrigger::GameplayEvent,
					TWeakObjectPtr<UObject>(), CaptureTag.GetTagName(), Ctx);
			});

		Bindings.Add(NB);

		UE_LOG(LogUnrealBridgeReactiveGE, Verbose,
			TEXT("bound GameplayEvent lambda for ASC=%s Tag=%s (HandlerCount=%d GlobalRef=%d)"),
			*ASC->GetPathName(), *Tag.ToString(), NB.HandlerCount, NB.GlobalRefCount);
	}

	void UnbindOne(FBinding& B)
	{
		if (B.ASC.IsValid())
		{
			B.ASC->GenericGameplayEventCallbacks.FindOrAdd(B.Tag).Remove(B.Handle);
		}
	}

	/** Walk live PIE-world actors, ensure a global binding exists for (ASC, Tag) on each. */
	void SnapshotGlobalBindingsForTag(const FGameplayTag& Tag)
	{
		if (!GEditor) return;
		for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
		{
			UWorld* W = Ctx.World();
			if (!BridgeReactiveAdapterImpl_GE::IsPieWorld(W)) continue;
			for (TActorIterator<AActor> It(W); It; ++It)
			{
				if (UAbilitySystemComponent* ASC = BridgeReactiveAdapterImpl_GE::ResolveActorASC(*It))
				{
					EnsureBinding(ASC, Tag, /*bForGlobal=*/true);
				}
			}
		}
	}

	/** Subscribe to spawn events for every PIE world; subscribe to PostPIEStarted / EndPIE. */
	void EnsureWorldWatchers()
	{
		if (!GEditor) return;

		// Per-world spawn hooks.
		for (const FWorldContext& WC : GEditor->GetWorldContexts())
		{
			UWorld* W = WC.World();
			if (!BridgeReactiveAdapterImpl_GE::IsPieWorld(W)) continue;
			TWeakObjectPtr<UWorld> Key(W);
			if (WorldSpawnHandles.Contains(Key)) continue;

			FDelegateHandle H = W->AddOnActorSpawnedHandler(
				FOnActorSpawned::FDelegate::CreateLambda(
					[this](AActor* SpawnedActor) { OnPieActorSpawned(SpawnedActor); }));
			WorldSpawnHandles.Add(Key, H);
		}

		if (!PostPieStartedHandle.IsValid())
		{
			PostPieStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda(
				[this](const bool /*bIsSimulating*/)
				{
					// New PIE world spun up — snapshot ASCs for every tracked global tag,
					// and add a spawn watcher for the new world.
					EnsureWorldWatchers();
					for (const auto& Pair : GlobalTagCounts)
					{
						SnapshotGlobalBindingsForTag(Pair.Key);
					}
				});
		}
		if (!PieEndHandle.IsValid())
		{
			PieEndHandle = FEditorDelegates::EndPIE.AddLambda(
				[this](const bool /*bIsSimulating*/)
				{
					// PIE world is going away. Drop bindings whose ASC is invalid OR
					// whose ASC's owning world is now invalid. The lambda capture's
					// WeakObjectPtr would also stop firing, but stale entries in
					// Bindings would leak across PIE sessions if we didn't sweep.
					for (int32 i = Bindings.Num() - 1; i >= 0; --i)
					{
						UAbilitySystemComponent* ASC = Bindings[i].ASC.Get();
						if (!ASC || !BridgeReactiveAdapterImpl_GE::IsPieWorld(ASC->GetWorld()))
						{
							// Best-effort unbind; if ASC is already gone the FindOrAdd
							// fallback in UnbindOne is a no-op on the stale handle.
							UnbindOne(Bindings[i]);
							Bindings.RemoveAtSwap(i);
						}
					}
					// Drop spawn handles for dead worlds.
					for (auto It = WorldSpawnHandles.CreateIterator(); It; ++It)
					{
						if (!It.Key().IsValid())
						{
							It.RemoveCurrent();
						}
					}
				});
		}
	}

	void TeardownWorldWatchersIfIdle()
	{
		if (GlobalTagCounts.Num() > 0) return;
		// No global handlers left — drop spawn watchers and PIE delegates.
		for (auto& Pair : WorldSpawnHandles)
		{
			if (UWorld* W = Pair.Key.Get())
			{
				W->RemoveOnActorSpawnedHandler(Pair.Value);
			}
		}
		WorldSpawnHandles.Reset();
		if (PostPieStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPieStartedHandle);
			PostPieStartedHandle.Reset();
		}
		if (PieEndHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(PieEndHandle);
			PieEndHandle.Reset();
		}
	}

	void OnPieActorSpawned(AActor* SpawnedActor)
	{
		if (!SpawnedActor || GlobalTagCounts.Num() == 0) return;
		if (UAbilitySystemComponent* ASC = BridgeReactiveAdapterImpl_GE::ResolveActorASC(SpawnedActor))
		{
			BindAllGlobalTagsToASC(ASC);
			return;
		}
		// PlayerState ASCs aren't on the actor at OnActorSpawned. Re-check next tick.
		TWeakObjectPtr<AActor> WeakActor(SpawnedActor);
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([this, WeakActor](float) -> bool
			{
				if (AActor* A = WeakActor.Get())
				{
					if (UAbilitySystemComponent* ASC = BridgeReactiveAdapterImpl_GE::ResolveActorASC(A))
					{
						BindAllGlobalTagsToASC(ASC);
					}
				}
				return false; // one-shot
			}),
			0.0f);
	}

	void BindAllGlobalTagsToASC(UAbilitySystemComponent* ASC)
	{
		if (!ASC) return;
		for (const auto& Pair : GlobalTagCounts)
		{
			EnsureBinding(ASC, Pair.Key, /*bForGlobal=*/true);
		}
	}
};

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeGameplayEventAdapter()
	{
		return MakeUnique<FBridgeGameplayEventAdapter>();
	}
}

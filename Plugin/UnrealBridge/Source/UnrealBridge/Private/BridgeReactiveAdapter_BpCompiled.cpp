#include "UnrealBridgeReactiveAdapter.h"
#include "UnrealBridgeReactiveSubsystem.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Blueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeReactiveBp, Log, All);

namespace BridgeReactiveAdapterImpl_Bp
{
	FString EscapeSingleQuoted(const FString& In)
	{
		FString Out; Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\'')) Out.AppendChar(TEXT('\\'));
			Out.AppendChar(C);
		}
		return Out;
	}

	/** Short class name of a BP's ParentClass ("ActorComponent", "PlayerController", …). */
	FString ParentClassShortName(UBlueprint* BP)
	{
		if (!BP || !BP->ParentClass) return FString();
		return BP->ParentClass->GetName();
	}
}

/**
 * Blueprint-compile adapter with two binding modes.
 *
 * Per-subject (Subject = UBlueprint): bind UBlueprint::OnCompiled() once per
 * distinct Subject; refcount via FBinding::HandlerCount. Dispatch ctx includes
 * blueprint_path + parent_class from the callback's UBlueprint*.
 *
 * Global (Subject = nullptr): bind GEditor->OnBlueprintCompiled() via HandlerCount
 * refcount, the same way AssetEvent does. The broadcast is no-param, so ctx
 * reports blueprint_path / parent_class as empty strings. Document this.
 *
 * Selector is always NAME_None for BpCompiled — there are no sub-event types.
 * Agents filter by specific BP at register time (via Subject) or in-script
 * (ctx['blueprint_path']).
 */
class FBridgeBpCompiledAdapter : public IBridgeReactiveAdapter
{
public:
	virtual EBridgeTrigger GetTriggerType() const override { return EBridgeTrigger::BpCompiled; }

	virtual void OnHandlerAdded(const FBridgeHandlerRecord& Record) override
	{
		UBlueprint* BP = Cast<UBlueprint>(Record.Subject.Get());
		if (BP)
		{
			AddPerSubjectBinding(BP);
		}
		else if (Record.Subject.IsExplicitlyNull())
		{
			AddGlobalBinding();
		}
		else
		{
			UE_LOG(LogUnrealBridgeReactiveBp, Warning,
				TEXT("OnHandlerAdded %s: Subject is non-null but not a UBlueprint; treating as global"),
				*Record.HandlerId);
			AddGlobalBinding();
		}
	}

	virtual void OnHandlerRemoved(const FBridgeHandlerRecord& Record) override
	{
		UBlueprint* BP = Cast<UBlueprint>(Record.Subject.Get());
		if (BP)
		{
			RemovePerSubjectBinding(BP);
		}
		else if (Record.Subject.IsExplicitlyNull())
		{
			RemoveGlobalBinding();
		}
		else
		{
			// Match the "treat as global" branch above.
			RemoveGlobalBinding();
		}
	}

	virtual void Shutdown() override
	{
		for (FBinding& B : Bindings)
		{
			if (B.Blueprint.IsValid() && B.Handle.IsValid())
			{
				B.Blueprint->OnCompiled().Remove(B.Handle);
			}
		}
		Bindings.Reset();

		if (GlobalHandle.IsValid() && GEditor)
		{
			GEditor->OnBlueprintCompiled().Remove(GlobalHandle);
		}
		GlobalHandle.Reset();
		GlobalHandlerCount = 0;
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),         TEXT("str — always 'bp_compiled'"));
		D.Add(TEXT("blueprint_path"),  TEXT("str — object path of the compiled blueprint; empty when handler is global (GEditor broadcast has no BP payload)"));
		D.Add(TEXT("parent_class"),    TEXT("str — short name of the BP's ParentClass; empty when handler is global"));
		return D;
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UBlueprint> Blueprint;
		FDelegateHandle            Handle;
		int32                      HandlerCount = 0;
	};

	void AddPerSubjectBinding(UBlueprint* BP)
	{
		for (FBinding& B : Bindings)
		{
			if (B.Blueprint.Get() == BP)
			{
				B.HandlerCount += 1;
				return;
			}
		}
		FBinding NB;
		NB.Blueprint = BP;
		NB.HandlerCount = 1;
		NB.Handle = BP->OnCompiled().AddLambda([this](UBlueprint* Compiled)
		{
			FirePerSubject(Compiled);
		});
		Bindings.Add(MoveTemp(NB));
	}

	void RemovePerSubjectBinding(UBlueprint* BP)
	{
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			if (Bindings[i].Blueprint.Get() == BP)
			{
				Bindings[i].HandlerCount = FMath::Max(0, Bindings[i].HandlerCount - 1);
				if (Bindings[i].HandlerCount == 0)
				{
					if (Bindings[i].Handle.IsValid() && BP)
					{
						BP->OnCompiled().Remove(Bindings[i].Handle);
					}
					Bindings.RemoveAt(i);
				}
				return;
			}
		}
	}

	void AddGlobalBinding()
	{
		GlobalHandlerCount += 1;
		if (GlobalHandlerCount == 1 && GEditor)
		{
			GlobalHandle = GEditor->OnBlueprintCompiled().AddLambda([this]()
			{
				FireGlobal();
			});
		}
	}

	void RemoveGlobalBinding()
	{
		GlobalHandlerCount = FMath::Max(0, GlobalHandlerCount - 1);
		if (GlobalHandlerCount == 0 && GlobalHandle.IsValid() && GEditor)
		{
			GEditor->OnBlueprintCompiled().Remove(GlobalHandle);
			GlobalHandle.Reset();
		}
	}

	void FirePerSubject(UBlueprint* BP)
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub || !BP) return;

		using namespace BridgeReactiveAdapterImpl_Bp;
		const FString Path   = BP->GetPathName();
		const FString Parent = ParentClassShortName(BP);

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),        TEXT("'bp_compiled'"));
		Ctx.Add(TEXT("blueprint_path"), FString::Printf(TEXT("'%s'"), *EscapeSingleQuoted(Path)));
		Ctx.Add(TEXT("parent_class"),   FString::Printf(TEXT("'%s'"), *EscapeSingleQuoted(Parent)));

		Sub->Dispatch(EBridgeTrigger::BpCompiled,
			TWeakObjectPtr<UObject>(BP),
			NAME_None,
			Ctx);
	}

	void FireGlobal()
	{
		UBridgeReactiveSubsystem* Sub = UBridgeReactiveSubsystem::Get();
		if (!Sub) return;

		// GEditor's broadcast is no-param — we can't identify the compiled BP.
		// Report empty strings so the handler script can still branch on trigger.
		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),        TEXT("'bp_compiled'"));
		Ctx.Add(TEXT("blueprint_path"), TEXT("''"));
		Ctx.Add(TEXT("parent_class"),   TEXT("''"));

		Sub->Dispatch(EBridgeTrigger::BpCompiled,
			TWeakObjectPtr<UObject>(),
			NAME_None,
			Ctx);
	}

	TArray<FBinding> Bindings;           // per-subject refcount
	FDelegateHandle  GlobalHandle;       // GEditor->OnBlueprintCompiled handle
	int32            GlobalHandlerCount = 0;
};

namespace BridgeReactiveAdapters
{
	TUniquePtr<IBridgeReactiveAdapter> MakeBpCompiledAdapter()
	{
		return MakeUnique<FBridgeBpCompiledAdapter>();
	}
}

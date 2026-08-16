#include "UnrealBridgeServer.h"
#include "IPythonScriptPlugin.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "SocketSubsystem.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "UnrealBridgeCallLog.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridge, Log, All);

namespace UnrealBridgeLimits
{
	// Max request JSON payload. 10 MB is generous for human-authored scripts;
	// the upper bound exists mainly to stop a malicious/buggy client from
	// triggering an OOM in the editor via blind SetNumUninitialized.
	constexpr int32 MaxRequestBytes = 10 * 1024 * 1024;
}

// Modal-dialog inspection and actions deliberately live outside the normal
// Python exec queue. A Slate modal loop does not tick FTSTicker, so an exec
// that opened a dialog cannot make forward progress until somebody resolves
// it. The nested Slate loop does keep pumping GameThread task-graph work,
// which makes AsyncTask(GameThread) a reliable side channel for observing and
// interacting with the active modal window.
namespace UnrealBridgeModal
{
	struct FModalButton
	{
		int32 Id = INDEX_NONE;
		FString Label;
		TSharedPtr<SButton> Widget;
	};

	struct FModalInput
	{
		enum class EKind : uint8
		{
			SingleLine,
			MultiLine,
		};

		int32 Id = INDEX_NONE;
		EKind Kind = EKind::SingleLine;
		TSharedPtr<SEditableText> SingleLineWidget;
		TSharedPtr<SMultiLineEditableText> MultiLineWidget;

		FString GetValue() const
		{
			return Kind == EKind::SingleLine
				? SingleLineWidget->GetText().ToString()
				: MultiLineWidget->GetText().ToString();
		}

		bool IsReadOnly() const
		{
			return Kind == EKind::SingleLine
				? SingleLineWidget->IsTextReadOnly()
				: MultiLineWidget->IsTextReadOnly();
		}

		bool IsPassword() const
		{
			return Kind == EKind::SingleLine && SingleLineWidget->IsTextPassword();
		}

		bool IsEnabled() const
		{
			return Kind == EKind::SingleLine
				? SingleLineWidget->IsEnabled()
				: MultiLineWidget->IsEnabled();
		}

		void SetValue(const FString& Value) const
		{
			if (Kind == EKind::SingleLine)
			{
				SingleLineWidget->SetText(FText::FromString(Value));
			}
			else
			{
				MultiLineWidget->SetText(FText::FromString(Value));
			}
		}
	};

	struct FModalCheckBox
	{
		int32 Id = INDEX_NONE;
		FString Label;
		TSharedPtr<SCheckBox> Widget;
	};

	struct FModalSnapshot
	{
		bool bPresent = false;
		uint64 WindowGeneration = 0;
		FString SnapshotId;
		FString Title;
		TArray<FString> BodyText;
		TArray<FModalButton> Buttons;
		TArray<FModalInput> Inputs;
		TArray<FModalCheckBox> CheckBoxes;
		TSharedPtr<SWindow> Window;
	};

	uint64 TrackWindowGeneration(const TSharedPtr<SWindow>& Window)
	{
		// Content alone is not enough for stale-action protection: two
		// consecutive dialogs can have identical title/body/buttons. Give each
		// observed SWindow instance a process-local generation and include it in
		// the snapshot hash. This state is touched on the GameThread only.
		static TWeakPtr<SWindow> LastWindow;
		static uint64 Generation = 0;
		if (!Window.IsValid())
		{
			LastWindow.Reset();
			return 0;
		}
		if (LastWindow.Pin().Get() != Window.Get())
		{
			++Generation;
			LastWindow = Window;
		}
		return Generation;
	}

	bool IsExactWidgetType(const TSharedRef<SWidget>& Widget, const FName ExpectedType)
	{
		// SWidget::GetType() is available across UnrealBridge's full UE
		// 5.3+ support matrix. The newer GetWidgetClass metadata API is not.
		return Widget->GetType() == ExpectedType;
	}

	void AppendNonEmptyText(const FText& Text, TArray<FString>& Out)
	{
		FString Value = Text.ToString();
		Value.TrimStartAndEndInline();
		if (!Value.IsEmpty())
		{
			Out.AddUnique(MoveTemp(Value));
		}
	}

	void CollectDisplayText(const TSharedRef<SWidget>& Widget, TArray<FString>& Out)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		if (IsExactWidgetType(Widget, TEXT("STextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<STextBlock>(Widget)->GetText(), Out);
		}
		else if (IsExactWidgetType(Widget, TEXT("SRichTextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<SRichTextBlock>(Widget)->GetText(), Out);
		}

		FChildren* Children = Widget->GetChildren();
		if (Children == nullptr)
		{
			return;
		}
		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			CollectDisplayText(Children->GetChildAt(ChildIndex), Out);
		}
	}

	FString GetWidgetLabel(const TSharedRef<SWidget>& Widget)
	{
		TArray<FString> Parts;
		CollectDisplayText(Widget, Parts);
		return FString::Join(Parts, TEXT(" "));
	}

	void VisitWidgetTree(const TSharedRef<SWidget>& Widget, FModalSnapshot& Out, bool bInsideButton)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		const bool bIsButton = IsExactWidgetType(Widget, TEXT("SButton"));

		if (bIsButton)
		{
			const FString Label = GetWidgetLabel(Widget);
			// SWindow's title bar is also composed from SButtons (close,
			// maximize, etc.) but those icon-only controls are not semantic
			// dialog choices. Excluding unlabelled buttons keeps agent actions
			// constrained to explicit choices such as OK / Cancel / Retry.
			if (!Label.IsEmpty())
			{
				FModalButton& Button = Out.Buttons.AddDefaulted_GetRef();
				Button.Id = Out.Buttons.Num() - 1;
				Button.Label = Label;
				Button.Widget = StaticCastSharedRef<SButton>(Widget);
			}
		}
		else if (!bInsideButton && IsExactWidgetType(Widget, TEXT("STextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<STextBlock>(Widget)->GetText(), Out.BodyText);
		}
		else if (!bInsideButton && IsExactWidgetType(Widget, TEXT("SRichTextBlock")))
		{
			AppendNonEmptyText(StaticCastSharedRef<SRichTextBlock>(Widget)->GetText(), Out.BodyText);
		}

		if (IsExactWidgetType(Widget, TEXT("SEditableText")))
		{
			FModalInput& Input = Out.Inputs.AddDefaulted_GetRef();
			Input.Id = Out.Inputs.Num() - 1;
			Input.Kind = FModalInput::EKind::SingleLine;
			Input.SingleLineWidget = StaticCastSharedRef<SEditableText>(Widget);
		}
		else if (IsExactWidgetType(Widget, TEXT("SMultiLineEditableText")))
		{
			FModalInput& Input = Out.Inputs.AddDefaulted_GetRef();
			Input.Id = Out.Inputs.Num() - 1;
			Input.Kind = FModalInput::EKind::MultiLine;
			Input.MultiLineWidget = StaticCastSharedRef<SMultiLineEditableText>(Widget);
		}

		if (IsExactWidgetType(Widget, TEXT("SCheckBox")))
		{
			FModalCheckBox& CheckBox = Out.CheckBoxes.AddDefaulted_GetRef();
			CheckBox.Id = Out.CheckBoxes.Num() - 1;
			CheckBox.Label = GetWidgetLabel(Widget);
			CheckBox.Widget = StaticCastSharedRef<SCheckBox>(Widget);
		}

		FChildren* Children = Widget->GetChildren();
		if (Children == nullptr)
		{
			return;
		}
		for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
		{
			VisitWidgetTree(Children->GetChildAt(ChildIndex), Out, bInsideButton || bIsButton);
		}
	}

	FString CheckStateToString(ECheckBoxState State)
	{
		switch (State)
		{
		case ECheckBoxState::Checked:
			return TEXT("checked");
		case ECheckBoxState::Undetermined:
			return TEXT("undetermined");
		default:
			return TEXT("unchecked");
		}
	}

	FModalSnapshot CaptureSnapshot()
	{
		FModalSnapshot Out;
		if (!FSlateApplication::IsInitialized())
		{
			return Out;
		}

		Out.Window = FSlateApplication::Get().GetActiveModalWindow();
		if (!Out.Window.IsValid())
		{
			TrackWindowGeneration(nullptr);
			return Out;
		}

		Out.bPresent = true;
		Out.WindowGeneration = TrackWindowGeneration(Out.Window);
		Out.Title = Out.Window->GetTitle().ToString();
		VisitWidgetTree(Out.Window.ToSharedRef(), Out, false);
		// Some message-dialog layouts repeat the window title in their child
		// tree. Keep title and body separate in the wire format.
		Out.BodyText.Remove(Out.Title);

		FString Fingerprint = FString::Printf(TEXT("W:%llu\x1e"), Out.WindowGeneration)
			+ Out.Title + TEXT("\x1e") + FString::Join(Out.BodyText, TEXT("\x1f"));
		for (const FModalButton& Button : Out.Buttons)
		{
			Fingerprint += FString::Printf(TEXT("\x1eB:%d:%s:%d"), Button.Id, *Button.Label,
				Button.Widget->IsEnabled() ? 1 : 0);
		}
		for (const FModalInput& Input : Out.Inputs)
		{
			Fingerprint += FString::Printf(TEXT("\x1eI:%d:%s:%s"), Input.Id,
				Input.IsPassword() ? TEXT("password") : TEXT("plain"),
				Input.IsPassword() ? TEXT("<redacted>") : *Input.GetValue());
		}
		for (const FModalCheckBox& CheckBox : Out.CheckBoxes)
		{
			Fingerprint += FString::Printf(TEXT("\x1eC:%d:%s:%s"), CheckBox.Id, *CheckBox.Label,
				*CheckStateToString(CheckBox.Widget->GetCheckedState()));
		}

		FTCHARToUTF8 Utf8(*Fingerprint);
		FSHAHash Hash;
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
		Out.SnapshotId = Hash.ToString().Left(16).ToLower();
		return Out;
	}

	TSharedPtr<FJsonObject> SnapshotToJson(const FModalSnapshot& Snapshot)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("present"), Snapshot.bPresent);
		Json->SetStringField(TEXT("snapshot_id"), Snapshot.SnapshotId);
		Json->SetStringField(TEXT("title"), Snapshot.Title);
		Json->SetStringField(TEXT("body"), FString::Join(Snapshot.BodyText, TEXT("\n")));

		TArray<TSharedPtr<FJsonValue>> TextValues;
		for (const FString& Text : Snapshot.BodyText)
		{
			TextValues.Add(MakeShared<FJsonValueString>(Text));
		}
		Json->SetArrayField(TEXT("text"), MoveTemp(TextValues));

		TArray<TSharedPtr<FJsonValue>> ButtonValues;
		for (const FModalButton& Button : Snapshot.Buttons)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), Button.Id);
			Item->SetStringField(TEXT("label"), Button.Label);
			Item->SetBoolField(TEXT("enabled"), Button.Widget->IsEnabled());
			Item->SetBoolField(TEXT("visible"), Button.Widget->GetVisibility().IsVisible());
			ButtonValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Item)));
		}
		Json->SetArrayField(TEXT("buttons"), MoveTemp(ButtonValues));

		TArray<TSharedPtr<FJsonValue>> InputValues;
		for (const FModalInput& Input : Snapshot.Inputs)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), Input.Id);
			Item->SetStringField(TEXT("kind"), Input.Kind == FModalInput::EKind::SingleLine
				? TEXT("single_line") : TEXT("multi_line"));
			Item->SetBoolField(TEXT("password"), Input.IsPassword());
			Item->SetBoolField(TEXT("read_only"), Input.IsReadOnly());
			Item->SetBoolField(TEXT("enabled"), Input.IsEnabled());
			if (Input.IsPassword())
			{
				Item->SetStringField(TEXT("value"), TEXT("<redacted>"));
			}
			else
			{
				Item->SetStringField(TEXT("value"), Input.GetValue());
			}
			InputValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Item)));
		}
		Json->SetArrayField(TEXT("inputs"), MoveTemp(InputValues));

		TArray<TSharedPtr<FJsonValue>> CheckBoxValues;
		for (const FModalCheckBox& CheckBox : Snapshot.CheckBoxes)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), CheckBox.Id);
			Item->SetStringField(TEXT("label"), CheckBox.Label);
			Item->SetStringField(TEXT("state"), CheckStateToString(CheckBox.Widget->GetCheckedState()));
			Item->SetBoolField(TEXT("enabled"), CheckBox.Widget->IsEnabled());
			Item->SetBoolField(TEXT("visible"), CheckBox.Widget->GetVisibility().IsVisible());
			CheckBoxValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Item)));
		}
		Json->SetArrayField(TEXT("checkboxes"), MoveTemp(CheckBoxValues));
		return Json;
	}

	TSharedPtr<FJsonObject> MakeResult(bool bSuccess, const FString& Output, const FString& Error,
		const FModalSnapshot& Snapshot)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("output"), Output);
		Result->SetStringField(TEXT("error"), Error);
		Result->SetBoolField(TEXT("ready"), true);
		Result->SetObjectField(TEXT("modal"), SnapshotToJson(Snapshot));
		return Result;
	}

	TSharedPtr<FJsonObject> GetStatus()
	{
		const FModalSnapshot Snapshot = CaptureSnapshot();
		return MakeResult(true, Snapshot.bPresent ? TEXT("modal present") : TEXT("no active modal"),
			TEXT(""), Snapshot);
	}

	TSharedPtr<FJsonObject> PerformAction(const FString& ExpectedSnapshot, const FString& Action,
		int32 ControlId, const FString& Value, bool bChecked, bool bHasChecked)
	{
		FModalSnapshot Snapshot = CaptureSnapshot();
		if (!Snapshot.bPresent)
		{
			return MakeResult(false, TEXT(""), TEXT("no active modal"), Snapshot);
		}
		if (ExpectedSnapshot.IsEmpty())
		{
			return MakeResult(false, TEXT(""), TEXT("missing 'snapshot' field; inspect the modal first"), Snapshot);
		}
		if (ExpectedSnapshot != Snapshot.SnapshotId)
		{
			return MakeResult(false, TEXT(""),
				FString::Printf(TEXT("stale modal snapshot: expected %s, current %s; inspect again before acting"),
					*ExpectedSnapshot, *Snapshot.SnapshotId), Snapshot);
		}

		if (Action == TEXT("click_button"))
		{
			if (!Snapshot.Buttons.IsValidIndex(ControlId))
			{
				return MakeResult(false, TEXT(""), TEXT("button id is out of range"), Snapshot);
			}
			const FModalButton& Button = Snapshot.Buttons[ControlId];
			if (!Button.Widget->IsEnabled() || !Button.Widget->GetVisibility().IsVisible())
			{
				return MakeResult(false, TEXT(""), TEXT("button is disabled or hidden"), Snapshot);
			}

			// Build the response before invoking the delegate: the click can close
			// the window and unwind the nested modal loop immediately.
			TSharedPtr<FJsonObject> Result = MakeResult(true,
				FString::Printf(TEXT("clicked button %d (%s)"), ControlId, *Button.Label), TEXT(""), Snapshot);
			Button.Widget->SimulateClick();
			return Result;
		}

		if (Action == TEXT("set_text"))
		{
			if (!Snapshot.Inputs.IsValidIndex(ControlId))
			{
				return MakeResult(false, TEXT(""), TEXT("input id is out of range"), Snapshot);
			}
			const FModalInput& Input = Snapshot.Inputs[ControlId];
			if (!Input.IsEnabled() || Input.IsReadOnly())
			{
				return MakeResult(false, TEXT(""), TEXT("input is disabled or read-only"), Snapshot);
			}
			const bool bPassword = Input.IsPassword();
			Input.SetValue(Value);
			Snapshot = CaptureSnapshot();
			if (!bPassword
				&& (!Snapshot.Inputs.IsValidIndex(ControlId)
					|| Snapshot.Inputs[ControlId].GetValue() != Value))
			{
				return MakeResult(false, TEXT(""), TEXT("input did not accept the requested value"), Snapshot);
			}
			return MakeResult(true, FString::Printf(TEXT("updated input %d"), ControlId), TEXT(""), Snapshot);
		}

		if (Action == TEXT("set_checkbox"))
		{
			if (!bHasChecked)
			{
				return MakeResult(false, TEXT(""), TEXT("missing boolean 'checked' field"), Snapshot);
			}
			if (!Snapshot.CheckBoxes.IsValidIndex(ControlId))
			{
				return MakeResult(false, TEXT(""), TEXT("checkbox id is out of range"), Snapshot);
			}
			const FModalCheckBox& CheckBox = Snapshot.CheckBoxes[ControlId];
			if (!CheckBox.Widget->IsEnabled() || !CheckBox.Widget->GetVisibility().IsVisible())
			{
				return MakeResult(false, TEXT(""), TEXT("checkbox is disabled or hidden"), Snapshot);
			}
			const bool bCurrentlyChecked = CheckBox.Widget->GetCheckedState() == ECheckBoxState::Checked;
			if (bCurrentlyChecked != bChecked)
			{
				CheckBox.Widget->ToggleCheckedState();
			}
			Snapshot = CaptureSnapshot();
			if (!Snapshot.CheckBoxes.IsValidIndex(ControlId)
				|| (Snapshot.CheckBoxes[ControlId].Widget->GetCheckedState() == ECheckBoxState::Checked) != bChecked)
			{
				return MakeResult(false, TEXT(""), TEXT("checkbox did not accept the requested state"), Snapshot);
			}
			return MakeResult(true, FString::Printf(TEXT("updated checkbox %d"), ControlId), TEXT(""), Snapshot);
		}

		return MakeResult(false, TEXT(""), TEXT("unsupported modal action"), Snapshot);
	}

	bool RunOnGameThread(TFunction<TSharedPtr<FJsonObject>()>&& Work,
		TSharedPtr<FJsonObject>& OutResult, float TimeoutSeconds = 3.0f)
	{
		auto Promise = MakeShared<TPromise<TSharedPtr<FJsonObject>>, ESPMode::ThreadSafe>();
		TFuture<TSharedPtr<FJsonObject>> Future = Promise->GetFuture();
		AsyncTask(ENamedThreads::GameThread, [Promise, Work = MoveTemp(Work)]() mutable
		{
			Promise->SetValue(Work());
		});

		if (!Future.WaitFor(FTimespan::FromSeconds(TimeoutSeconds)))
		{
			return false;
		}
		OutResult = Future.Get();
		return OutResult.IsValid();
	}
}

// ─────────────────────────────────────────────────────────────
// Server lifecycle
// ─────────────────────────────────────────────────────────────

FUnrealBridgeServer::FUnrealBridgeServer()
{
}

FUnrealBridgeServer::~FUnrealBridgeServer()
{
	Stop();
}

bool FUnrealBridgeServer::Start(int32 Port)
{
	FStartConfig Cfg;
	Cfg.Port = Port;
	return Start(Cfg);
}

bool FUnrealBridgeServer::Start(const FStartConfig& Config)
{
	if (bIsRunning)
	{
		return true;
	}

	// Safety gate: binding to a non-localhost interface exposes Python exec to
	// the LAN. Refuse if the caller didn't supply a token.
	const bool bIsLoopback =
		(Config.BindAddress == FIPv4Address(127, 0, 0, 1)) ||
		(Config.BindAddress == FIPv4Address::InternalLoopback);
	if (!bIsLoopback && Config.Token.IsEmpty())
	{
		UE_LOG(LogUnrealBridge, Error,
			TEXT("Refusing to bind %s:%d without a token — set -UnrealBridgeToken=... ")
			TEXT("or use -UnrealBridgeBind=127.0.0.1"),
			*Config.BindAddress.ToString(), Config.Port);
		return false;
	}

	BindAddressStr = Config.BindAddress.ToString();
	Token = Config.Token;

	const FIPv4Endpoint Endpoint(Config.BindAddress, Config.Port);

	// 100ms poll (vs default 1s) collapses the accept-race window that produced
	// intermittent WSAECONNABORTED 10053 on clients. bInReusable=true lets Start()
	// reclaim a TIME_WAIT socket after a crash/quick-restart instead of failing
	// with "address in use". See docs/server-stability-plan.md #7.
	Listener = MakeUnique<FTcpListener>(
		Endpoint,
		FTimespan::FromMilliseconds(100),
		true /* bInReusable */
	);

	if (!Listener.IsValid() || !Listener->IsActive())
	{
		UE_LOG(LogUnrealBridge, Error, TEXT("Failed to create TCP listener on %s:%d"),
			*BindAddressStr, Config.Port);
		Listener.Reset();
		return false;
	}

	// When Port=0 the kernel picks a free ephemeral port — read it back so
	// clients and the discovery responder know where to connect.
	ListenPort = Config.Port;
	if (Listener->GetSocket() != nullptr)
	{
		TSharedRef<FInternetAddr> LocalAddr =
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		Listener->GetSocket()->GetAddress(*LocalAddr);
		const int32 ResolvedPort = LocalAddr->GetPort();
		if (ResolvedPort > 0)
		{
			ListenPort = ResolvedPort;
		}
	}

	Listener->OnConnectionAccepted().BindRaw(this, &FUnrealBridgeServer::OnConnectionAccepted);

	// Register the GameThread ticker that drains the exec queue.
	// Using FTSTicker instead of AsyncTask(GameThread) prevents reentrancy:
	// ticker callbacks fire only from FEngineLoop::Tick, not from TaskGraph
	// pumps triggered inside user scripts (asset loads, blueprint compiles, etc.).
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FUnrealBridgeServer::TickConsumeQueue),
		0.0f /* tick every frame */
	);

	// PIE transition guard (item #11). The transition *window* is:
	//   BeginPIE  ─────────→  PostPIEStarted   (editor subsystems torn
	//       [unsafe to exec]                   down and rebuilt here)
	//   PrePIEEnded  ──────→  EndPIE           (shutdown sequence —
	//       [unsafe again]                     same teardown/rebuild)
	// Between PostPIEStarted and PrePIEEnded PIE is running stably and
	// execs are safe. An earlier version used only BeginPIE/EndPIE which
	// kept the flag True for the entire PIE session and blocked agent
	// observation calls.
	PieBeginHandle = FEditorDelegates::BeginPIE.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = true;
	});
	PiePostStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = false;
	});
	PiePreEndedHandle = FEditorDelegates::PrePIEEnded.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = true;
	});
	PieEndHandle = FEditorDelegates::EndPIE.AddLambda([this](const bool /*bIsSimulating*/)
	{
		bPieTransitionActive = false;
	});

	bIsRunning = true;
	UE_LOG(LogUnrealBridge, Log, TEXT("Listening on %s:%d%s"),
		*BindAddressStr, ListenPort,
		HasToken() ? TEXT(" (token auth enforced)") : TEXT(""));
	return true;
}

void FUnrealBridgeServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}
	bIsRunning = false;

	// 1. Stop accepting new connections.
	if (Listener.IsValid())
	{
		Listener.Reset();
	}

	// 2. Unregister GameThread ticker and editor delegates (items #11 #12).
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (PieBeginHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(PieBeginHandle);
		PieBeginHandle.Reset();
	}
	if (PiePostStartedHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(PiePostStartedHandle);
		PiePostStartedHandle.Reset();
	}
	if (PiePreEndedHandle.IsValid())
	{
		FEditorDelegates::PrePIEEnded.Remove(PiePreEndedHandle);
		PiePreEndedHandle.Reset();
	}
	if (PieEndHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(PieEndHandle);
		PieEndHandle.Reset();
	}

	// 3. Fulfill any queued execs with a shutdown error so worker threads
	// waiting on TFuture wake up immediately.
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
	while (ExecQueue.Dequeue(Pending) && Pending.IsValid())
	{
		FExecResult R;
		R.bSuccess = false;
		R.Error = TEXT("server shutting down");
		Pending->Promise.SetValue(MoveTemp(R));
	}

	// 4. Force-close active client sockets so HandleClient's RecvAll
	// unblocks immediately instead of waiting for its 5 s idle timeout.
	{
		FScopeLock Lock(&ActiveSocketsLock);
		for (FSocket* S : ActiveSockets)
		{
			if (S)
			{
				S->Close();
			}
		}
	}

	// 5. Bounded wait for AsyncTask workers to drain (item #12). Beyond
	// the deadline we log and proceed — the workers will still finish
	// their cleanup (DestroySocket, ActiveClients.Decrement) but
	// ShutdownModule isn't held hostage to a stuck Python exec.
	const double Deadline = FPlatformTime::Seconds() + 3.0;
	while (ActiveClients.GetValue() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(0.01f);
	}
	const int32 Stragglers = ActiveClients.GetValue();
	if (Stragglers > 0)
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("Stop(): %d client worker(s) still active after 3s drain timeout"),
			Stragglers);
	}
	else
	{
		UE_LOG(LogUnrealBridge, Log, TEXT("Stop(): all client workers drained cleanly"));
	}
}

bool FUnrealBridgeServer::IsRunning() const
{
	return bIsRunning;
}

void FUnrealBridgeServer::SetEditorReady(bool bReady)
{
	bEditorReady = bReady;
	if (bReady)
	{
		UE_LOG(LogUnrealBridge, Log, TEXT("Editor reported ready — Python exec now accepted"));
	}
}

bool FUnrealBridgeServer::IsEditorReady() const
{
	return bEditorReady;
}

// ─────────────────────────────────────────────────────────────
// Connection handling
// ─────────────────────────────────────────────────────────────

bool FUnrealBridgeServer::OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	const FString EndpointStr = ClientEndpoint.ToString();

	// Bound concurrent clients so a runaway caller can't saturate the
	// AsyncTask background pool and starve other editor work. When we're
	// over capacity we return false so FTcpListener destroys the accepted
	// socket itself — the client sees a clean connection reset rather than
	// a silent hang.
	const int32 Active = ActiveClients.Increment();
	if (Active > MaxConcurrentClients)
	{
		ActiveClients.Decrement();
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[conn] rejecting %s — at concurrency limit (%d/%d)"),
			*EndpointStr, Active - 1, MaxConcurrentClients);
		return false;
	}

	UE_LOG(LogUnrealBridge, Verbose, TEXT("[conn] accepted %s (active=%d)"), *EndpointStr, Active);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket, EndpointStr]()
	{
		// Register socket so Stop() can force-close us (item #6).
		{
			FScopeLock Lock(&ActiveSocketsLock);
			ActiveSockets.Add(ClientSocket);
		}

		HandleClient(ClientSocket, EndpointStr);

		{
			FScopeLock Lock(&ActiveSocketsLock);
			ActiveSockets.Remove(ClientSocket);
		}

		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(ClientSocket);
		}
		ActiveClients.Decrement();
	});

	return true;
}

void FUnrealBridgeServer::HandleClient(FSocket* ClientSocket, const FString& EndpointStr)
{
	// One request-response per connection. bridge.py opens a fresh socket
	// per call; keep-alive would tie worker threads up in idle waits and
	// saturate the AsyncTask pool under high request rates.
	if (!bIsRunning)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();

	// Per-request telemetry collected throughout this function and flushed
	// to the bridge-call ring buffer right before we return. Written-to in
	// several branches below — see each `else if` for where the fields
	// are populated. See UnrealBridgeCallLog.h.
	FBridgeCallRecord CallRecord;
	CallRecord.Endpoint = EndpointStr;
	{
		// ToUnixTimestamp truncates to whole seconds; compute fractional by
		// subtracting the epoch as FTimespan and using GetTotalSeconds().
		static const FDateTime UnixEpoch(1970, 1, 1);
		CallRecord.UnixSeconds = (FDateTime::UtcNow() - UnixEpoch).GetTotalSeconds();
	}
	ON_SCOPE_EXIT
	{
		CallRecord.TotalDurationMs = (FPlatformTime::Seconds() - T0) * 1000.0;
		FBridgeCallLog::Get().Append(MoveTemp(CallRecord));
	};

	// 1. Read 4-byte length prefix (big-endian)
	uint8 LenBuf[4];
	if (!RecvAll(ClientSocket, LenBuf, 4, 5.0f))
	{
		UE_LOG(LogUnrealBridge, Verbose,
			TEXT("[%s] recv header failed (client gave up or idle timeout)"),
			*EndpointStr);
		return;
	}

	const uint32 PayloadLen = (uint32(LenBuf[0]) << 24)
							| (uint32(LenBuf[1]) << 16)
							| (uint32(LenBuf[2]) << 8)
							| (uint32(LenBuf[3]));

	if (PayloadLen == 0 || PayloadLen > (uint32)UnrealBridgeLimits::MaxRequestBytes)
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[%s] invalid payload length %u (max %d) — closing"),
			*EndpointStr, PayloadLen, UnrealBridgeLimits::MaxRequestBytes);
		return;
	}

	// 2. Read JSON payload — Reserve first so an allocation failure is detected
	// before we commit to a SetNumUninitialized of PayloadLen bytes.
	TArray<uint8> PayloadBuf;
	PayloadBuf.Reserve((int32)PayloadLen);
	if (PayloadBuf.Max() < (int32)PayloadLen)
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[%s] failed to allocate %u bytes for payload"),
			*EndpointStr, PayloadLen);
		return;
	}
	PayloadBuf.SetNumUninitialized((int32)PayloadLen);
	if (!RecvAll(ClientSocket, PayloadBuf.GetData(), (int32)PayloadLen, 30.0f))
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[%s] recv payload failed (expected %u bytes)"),
			*EndpointStr, PayloadLen);
		return;
	}

	FUTF8ToTCHAR Converter((const ANSICHAR*)PayloadBuf.GetData(), PayloadLen);
	FString JsonStr(Converter.Length(), Converter.Get());

	// 3. Parse JSON
	TSharedPtr<FJsonObject> Request;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid())
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[%s] JSON parse failed (payload=%u bytes)"),
			*EndpointStr, PayloadLen);
		return;
	}

	FString RequestId;
	if (!Request->TryGetStringField(TEXT("id"), RequestId))
	{
		RequestId = TEXT("<missing>");
	}
	CallRecord.RequestId = RequestId;

	// 4. Build response
	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("id"), RequestId);

	// 4a. Token auth — only enforced when the server was started with a token
	// (i.e. binding to a non-localhost interface). Constant-time compare so a
	// timing oracle can't whittle the secret out.
	if (!Token.IsEmpty())
	{
		FString GivenToken;
		Request->TryGetStringField(TEXT("token"), GivenToken);

		const auto* A = (const TCHAR*)*Token;
		const auto* B = (const TCHAR*)*GivenToken;
		const int32 LenA = Token.Len();
		const int32 LenB = GivenToken.Len();
		uint32 Diff = (uint32)(LenA ^ LenB);
		const int32 Cmp = FMath::Min(LenA, LenB);
		for (int32 i = 0; i < Cmp; ++i)
		{
			Diff |= (uint32)(A[i] ^ B[i]);
		}

		if (Diff != 0)
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("output"), TEXT(""));
			Response->SetStringField(TEXT("error"), TEXT("unauthorized: missing or invalid token"));

			FString RespJson;
			TSharedRef<TJsonWriter<>> RespWriter = TJsonWriterFactory<>::Create(&RespJson);
			FJsonSerializer::Serialize(Response, RespWriter);

			const FTCHARToUTF8 RespUtf8(*RespJson);
			const int32 RespLen = RespUtf8.Length();
			uint8 AuthRespLenBuf[4] = {
				(uint8)((RespLen >> 24) & 0xFF),
				(uint8)((RespLen >> 16) & 0xFF),
				(uint8)((RespLen >> 8) & 0xFF),
				(uint8)(RespLen & 0xFF),
			};
			SendAll(ClientSocket, AuthRespLenBuf, 4);
			SendAll(ClientSocket, (const uint8*)RespUtf8.Get(), RespLen);

			UE_LOG(LogUnrealBridge, Warning,
				TEXT("[%s] unauthorized request id=%s (bad token)"),
				*EndpointStr, *RequestId);
			return;
		}
	}

	FString Command;
	Request->TryGetStringField(TEXT("command"), Command); // optional
	CallRecord.Command = Command.IsEmpty() ? TEXT("exec") : Command;

	UE_LOG(LogUnrealBridge, Verbose,
		TEXT("[%s] request id=%s cmd=%s payload=%u"),
		*EndpointStr, *RequestId, Command.IsEmpty() ? TEXT("(exec)") : *Command, PayloadLen);

	if (Command == TEXT("ping"))
	{
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("output"), TEXT("pong"));
		Response->SetStringField(TEXT("error"), TEXT(""));
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else if (Command == TEXT("debug_resume"))
	{
		// Recovery path for a stuck blueprint breakpoint.
		//
		// When a BP breakpoint fires, UE enters `FSlateApplication::EnterDebuggingMode`
		// — a nested Slate loop that keeps pumping the task graph on the
		// GameThread but does NOT pump the FTSTicker-based Python exec queue.
		// A prior `invoke_*` that triggered the break is still blocked inside
		// `ProcessEvent`, so the Python interpreter is occupied and new
		// `exec` commands can't land.
		//
		// Recovery requires TWO things, both dispatched via AsyncTask (task
		// graph is pumped during the nested Slate loop; FTSTicker is not):
		//
		//   1. `FSlateApplication::LeaveDebuggingMode()` — exits the nested
		//      Slate loop, unblocking `AttemptToBreakExecution` so the BP
		//      VM resumes.
		//   2. `FKismetDebugUtilities::RequestAbortingExecution()` — sets
		//      `bAbortingExecution` on the stack frame so when the VM
		//      resumes, it unwinds rather than continuing past the
		//      breakpoint (which would hit the same break again).
		//
		// Together these pop the debug-mode stack and let ProcessEvent
		// return, which unblocks the stuck Python exec, which unblocks the
		// TCP response to the original caller.
		AsyncTask(ENamedThreads::GameThread, []()
		{
			FKismetDebugUtilities::RequestAbortingExecution();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().LeaveDebuggingMode(/*bLeavingDebugForSingleStep*/ false);
			}
		});
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("output"), TEXT("resume requested"));
		Response->SetStringField(TEXT("error"), TEXT(""));
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else if (Command == TEXT("gamethread_ping"))
	{
		// Probe whether the GameThread is responsive without going through
		// the FTSTicker exec queue. Submits a no-op AsyncTask(GameThread)
		// and waits with a short bounded timeout (default 2s, max 10s).
		//
		// Diagnostic interpretation:
		//   - alive, low latency (~ms): GT idle, exec queue healthy
		//   - alive, high latency (~hundreds ms): GT mid-exec but TaskGraph
		//     is being pumped (asset load / BP compile inside Python) —
		//     editor is not deadlocked but the exec queue may be backed up
		//   - unresponsive: GT is fully stuck (native OS dialog, deadlock,
		//     pure-Python tight loop holding the GIL with no TG pump). Slate
		//     modals normally remain responsive here; use modal_status to
		//     distinguish them from ordinary long-running work. The
		//     FTSTicker exec queue cannot drain in this state.
		double ProbeTimeoutNum = 2.0;
		Request->TryGetNumberField(TEXT("timeout"), ProbeTimeoutNum);
		const float ProbeTimeout = FMath::Clamp((float)ProbeTimeoutNum, 0.1f, 10.0f);

		auto Probe = MakeShared<TPromise<bool>, ESPMode::ThreadSafe>();
		TFuture<bool> ProbeFuture = Probe->GetFuture();
		const double ProbeT0 = FPlatformTime::Seconds();

		AsyncTask(ENamedThreads::GameThread, [Probe]()
		{
			Probe->SetValue(true);
		});

		const bool bAlive = ProbeFuture.WaitFor(FTimespan::FromSeconds(ProbeTimeout));
		const double LatencyMs = (FPlatformTime::Seconds() - ProbeT0) * 1000.0;

		Response->SetBoolField(TEXT("success"), bAlive);
		Response->SetStringField(TEXT("output"), bAlive ? TEXT("alive") : TEXT("unresponsive"));
		Response->SetStringField(TEXT("error"), bAlive
			? TEXT("")
			: FString::Printf(TEXT("GameThread did not respond within %.1fs"), ProbeTimeout));
		Response->SetNumberField(TEXT("latency_ms"), LatencyMs);
		Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
	}
	else if (Command == TEXT("modal_status") || Command == TEXT("modal_action"))
	{
		// This path intentionally bypasses both Python and the FTSTicker exec
		// queue. It remains callable while an earlier exec is suspended inside
		// a nested Slate modal loop.
		FString ExpectedSnapshot;
		FString Action;
		FString Value;
		Request->TryGetStringField(TEXT("snapshot"), ExpectedSnapshot);
		Request->TryGetStringField(TEXT("action"), Action);
		Request->TryGetStringField(TEXT("value"), Value);

		double ControlIdNumber = -1.0;
		Request->TryGetNumberField(TEXT("control_id"), ControlIdNumber);
		const int32 ControlId = FMath::FloorToInt(ControlIdNumber);

		bool bChecked = false;
		const bool bHasChecked = Request->TryGetBoolField(TEXT("checked"), bChecked);

		TSharedPtr<FJsonObject> ModalResult;
		const bool bCompleted = UnrealBridgeModal::RunOnGameThread(
			[Command, ExpectedSnapshot, Action, ControlId, Value, bChecked, bHasChecked]()
			{
				if (Command == TEXT("modal_status"))
				{
					return UnrealBridgeModal::GetStatus();
				}
				return UnrealBridgeModal::PerformAction(
					ExpectedSnapshot, Action, ControlId, Value, bChecked, bHasChecked);
			},
			ModalResult);

		if (bCompleted)
		{
			Response = ModalResult.ToSharedRef();
			Response->SetStringField(TEXT("id"), RequestId);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("output"), TEXT(""));
			Response->SetStringField(TEXT("error"),
				TEXT("GameThread did not service the modal request within 3.0s"));
			Response->SetBoolField(TEXT("ready"), (bool)bEditorReady);
		}
	}
	else if (!bEditorReady)
	{
		// Reject Python exec while the editor is still initializing.
		// Dispatching to the GameThread during SlateRHIRenderer::CreateViewport's
		// render-fence can crash the editor, so fail fast with a clear signal.
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("output"), TEXT(""));
		Response->SetStringField(TEXT("error"), TEXT("editor not ready — main frame not yet created"));
		Response->SetBoolField(TEXT("ready"), false);
	}
	else if (bPieTransitionActive)
	{
		// Reject exec during Begin/EndPIE because editor subsystems (world,
		// GAS, anim) are torn down and rebuilt — Python running in that
		// window reliably crashes (item #11).
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("output"), TEXT(""));
		Response->SetStringField(TEXT("error"), TEXT("editor in PIE transition — retry in a moment"));
		Response->SetBoolField(TEXT("ready"), true);
	}
	else
	{
		// Execute Python script — serialized through the GameThread ticker queue.
		FString Script;
		if (!Request->TryGetStringField(TEXT("script"), Script))
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("output"), TEXT(""));
			Response->SetStringField(TEXT("error"), TEXT("missing 'script' field"));
			Response->SetBoolField(TEXT("ready"), true);
		}
		else
		{
			double TimeoutNum = 30.0;
			Request->TryGetNumberField(TEXT("timeout"), TimeoutNum);
			const float Timeout = FMath::Clamp((float)TimeoutNum, 0.1f, 300.0f);

			// Capture a preview of the script for the call-log ring. Cap at
			// ~80 chars; newlines collapse to spaces so the log stays
			// single-line-scannable.
			CallRecord.ScriptPreview = Script.Left(80).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(""));

			const double ExecT0 = FPlatformTime::Seconds();
			FExecResult Result = EnqueueAndWaitForExec(Script, Timeout, RequestId);
			const double ExecMs = (FPlatformTime::Seconds() - ExecT0) * 1000.0;
			CallRecord.ExecDurationMs = ExecMs;

			UE_LOG(LogUnrealBridge, Log,
				TEXT("[%s] exec id=%s ok=%s out=%dB err=%dB took=%.1fms"),
				*EndpointStr, *RequestId,
				Result.bSuccess ? TEXT("true") : TEXT("false"),
				Result.Output.Len(), Result.Error.Len(), ExecMs);

			Response->SetBoolField(TEXT("success"), Result.bSuccess);
			Response->SetStringField(TEXT("output"), Result.Output);
			Response->SetStringField(TEXT("error"), Result.Error);
			Response->SetBoolField(TEXT("ready"), true);
		}
	}

	// Mirror the authoritative Response fields into the call record so
	// every branch (ping / resume / exec / rejected-not-ready / etc.)
	// logs consistent success/output/error sizes without bespoke wiring.
	{
		bool bOk = false;
		Response->TryGetBoolField(TEXT("success"), bOk);
		CallRecord.bSuccess = bOk;
		FString OutStr, ErrStr;
		Response->TryGetStringField(TEXT("output"), OutStr);
		Response->TryGetStringField(TEXT("error"), ErrStr);
		CallRecord.OutputBytes = OutStr.Len();
		CallRecord.ErrorBytes = ErrStr.Len();
		if (!bOk)
		{
			CallRecord.ErrorPreview = ErrStr.Left(200);
		}
	}

	// 5. Serialize and send response
	FString ResponseStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Response, Writer);

	FTCHARToUTF8 Utf8Response(*ResponseStr);
	int32 ResponseLen = Utf8Response.Length();

	uint8 RespLenBuf[4];
	RespLenBuf[0] = (ResponseLen >> 24) & 0xFF;
	RespLenBuf[1] = (ResponseLen >> 16) & 0xFF;
	RespLenBuf[2] = (ResponseLen >> 8) & 0xFF;
	RespLenBuf[3] = ResponseLen & 0xFF;

	if (!SendAll(ClientSocket, RespLenBuf, 4))
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[%s] send response header failed (id=%s cmd=%s)"),
			*EndpointStr, *RequestId, *Command);
		return;
	}
	if (!SendAll(ClientSocket, (const uint8*)Utf8Response.Get(), ResponseLen))
	{
		UE_LOG(LogUnrealBridge, Warning,
			TEXT("[%s] send response body failed (id=%s cmd=%s len=%d)"),
			*EndpointStr, *RequestId, *Command, ResponseLen);
		return;
	}

	UE_LOG(LogUnrealBridge, Verbose,
		TEXT("[%s] done id=%s total=%.1fms"),
		*EndpointStr, *RequestId, (FPlatformTime::Seconds() - T0) * 1000.0);
}

// ─────────────────────────────────────────────────────────────
// Python execution pipeline
// ─────────────────────────────────────────────────────────────
//
// Worker threads enqueue heap-allocated FPendingExec and wait on the
// associated TFuture. A single FTSTicker consumer on the GameThread drains
// the queue one item per frame, guarded by bExecInFlight. This design:
//   - Eliminates the reentrancy crash caused by AsyncTask(GameThread) being
//     pulled off the task-graph queue during Python-triggered TaskGraph pumps.
//   - Removes the dangling-reference / event-pool-reuse bug from the old
//     per-request FEvent scheme: TSharedPtr<FPendingExec> keeps the promise
//     alive until the ticker fulfills it, regardless of whether the worker
//     has already returned a timeout to its client.
// ─────────────────────────────────────────────────────────────

FUnrealBridgeServer::FExecResult FUnrealBridgeServer::EnqueueAndWaitForExec(
	const FString& Script, float TimeoutSeconds, const FString& RequestId)
{
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending = MakeShared<FPendingExec, ESPMode::ThreadSafe>();
	Pending->Script = Script;
	Pending->TimeoutSeconds = TimeoutSeconds;
	Pending->RequestId = RequestId;

	TFuture<FExecResult> Future = Pending->Promise.GetFuture();
	ExecQueue.Enqueue(Pending);

	const bool bReady = Future.WaitFor(FTimespan::FromSeconds(TimeoutSeconds));
	if (!bReady)
	{
		FExecResult R;
		R.bSuccess = false;
		R.Error = FString::Printf(TEXT("exec timeout after %.1fs"), TimeoutSeconds);
		// Leave the promise alone — the ticker will still fulfill it later,
		// but Pending's shared-ptr means that's safe and leaks nothing.
		return R;
	}
	return Future.Get();
}

bool FUnrealBridgeServer::TickConsumeQueue(float /*DeltaTime*/)
{
	if (!bIsRunning)
	{
		return true; // still ticking; will be removed by Stop()
	}
	if (bExecInFlight)
	{
		return true; // belt-and-suspenders guard against ticker reentrancy
	}

	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
	if (!ExecQueue.Dequeue(Pending) || !Pending.IsValid())
	{
		return true;
	}

	bExecInFlight = true;
	FExecResult Result = DoPythonExec(Pending->Script);
	Pending->Promise.SetValue(MoveTemp(Result));
	bExecInFlight = false;
	return true;
}

FUnrealBridgeServer::FExecResult FUnrealBridgeServer::DoPythonExec(const FString& Script)
{
	FExecResult Result;

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		Result.bSuccess = false;
		Result.Error = TEXT("PythonScriptPlugin is not available");
		return Result;
	}

	// Wrap user script to capture stdout/stderr in Python-land,
	// then print the captured content so ExecPythonCommandEx can collect it via LogOutput.
	//
	// We base64-encode the user script instead of inlining it inside a
	// Python triple-quoted string. Triple-quoted strings in Python don't
	// honour backslash-escape for quotes, so any user script containing
	// `"""` (docstrings, embedded SQL/markdown) would break the old
	// escape scheme. Base64 sidesteps quoting entirely.
	const FTCHARToUTF8 ScriptUtf8(*Script);
	const FString ScriptB64 = FBase64::Encode(
		reinterpret_cast<const uint8*>(ScriptUtf8.Get()),
		ScriptUtf8.Length());

	// Output is base64-encoded before being printed because UE's Python stdout
	// shim mangles non-ASCII characters into U+FFFD on the way back to FString.
	// base64 is pure ASCII, so it survives the shim intact; the C++ side
	// decodes it back to UTF-8 bytes and rebuilds an FString via FUTF8ToTCHAR.
	FString WrappedScript = FString::Printf(TEXT(
		"import base64 as _b64, sys, io as _io, traceback as _tb\n"
		"_src = _b64.b64decode('%s').decode('utf-8')\n"
		"_ub_out, _ub_err = _io.StringIO(), _io.StringIO()\n"
		"_ub_old = sys.stdout, sys.stderr\n"
		"sys.stdout, sys.stderr = _ub_out, _ub_err\n"
		"try:\n"
		"    exec(compile(_src, '<unrealbridge>', 'exec'))\n"
		"except Exception:\n"
		"    sys.stderr.write(_tb.format_exc())\n"
		"finally:\n"
		"    sys.stdout, sys.stderr = _ub_old\n"
		"    _ub_o, _ub_e = _ub_out.getvalue(), _ub_err.getvalue()\n"
		"    _ub_out.close(); _ub_err.close()\n"
		"    _eo = _b64.b64encode(_ub_o.encode('utf-8')).decode('ascii') if _ub_o else ''\n"
		"    _ee = _b64.b64encode(_ub_e.encode('utf-8')).decode('ascii') if _ub_e else ''\n"
		"    print('__UB_B64__' + _eo + '|' + _ee + '__UB_END__')\n"
	), *ScriptB64);

	FPythonCommandEx CommandEx;
	CommandEx.Command = WrappedScript;
	CommandEx.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	CommandEx.FileExecutionScope = EPythonFileExecutionScope::Public;

	bool bExecSuccess = PythonPlugin->ExecPythonCommandEx(CommandEx);

	FString FullOutput;
	for (const FPythonLogOutputEntry& Entry : CommandEx.LogOutput)
	{
		FullOutput += Entry.Output + TEXT("\n");
	}
	if (FullOutput.IsEmpty() && !CommandEx.CommandResult.IsEmpty())
	{
		FullOutput = CommandEx.CommandResult;
	}

	// Wrapper emits `__UB_B64__<out_b64>|<err_b64>__UB_END__`. Decode each
	// half from base64 → UTF-8 bytes → FString via FUTF8ToTCHAR. Fallback to
	// raw FullOutput-as-Error when the envelope is missing (catastrophic
	// failure inside the wrapper itself, before the print could run).
	const FString EnvBegin = TEXT("__UB_B64__");
	const FString EnvEnd = TEXT("__UB_END__");
	const int32 BeginIdx = FullOutput.Find(EnvBegin);
	const int32 EndIdx = (BeginIdx != INDEX_NONE)
		? FullOutput.Find(EnvEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, BeginIdx + EnvBegin.Len())
		: INDEX_NONE;

	if (BeginIdx != INDEX_NONE && EndIdx != INDEX_NONE)
	{
		const int32 PayloadStart = BeginIdx + EnvBegin.Len();
		const FString Payload = FullOutput.Mid(PayloadStart, EndIdx - PayloadStart);

		int32 SepIdx = INDEX_NONE;
		Payload.FindChar(TEXT('|'), SepIdx);
		const FString OutB64 = (SepIdx != INDEX_NONE) ? Payload.Left(SepIdx) : Payload;
		const FString ErrB64 = (SepIdx != INDEX_NONE) ? Payload.Mid(SepIdx + 1) : FString();

		auto DecodeB64ToUtf8FString = [](const FString& B64) -> FString
		{
			if (B64.IsEmpty()) return FString();
			TArray<uint8> Bytes;
			if (!FBase64::Decode(B64, Bytes) || Bytes.Num() == 0) return FString();
			// Match the inbound-decode pattern used at line ~380 — FUTF8ToTCHAR
			// with explicit length, then construct FString from .Get() + .Length()
			// so we don't accidentally hit FString's ANSI-interpret constructor.
			FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			return FString(Conv.Length(), Conv.Get());
		};

		Result.Output = DecodeB64ToUtf8FString(OutB64);
		Result.Error = DecodeB64ToUtf8FString(ErrB64);
		Result.bSuccess = bExecSuccess && Result.Error.IsEmpty();
	}
	else
	{
		// Wrapper crashed before emitting the envelope — surface whatever UE captured.
		Result.Output = FString();
		Result.Error = FullOutput;
		Result.bSuccess = false;
	}

	Result.Output.TrimEndInline();
	Result.Error.TrimEndInline();
	return Result;
}

// ─────────────────────────────────────────────────────────────
// Socket helpers
// ─────────────────────────────────────────────────────────────

bool FUnrealBridgeServer::RecvAll(FSocket* Socket, uint8* Buffer, int32 NumBytes, float TimeoutSeconds)
{
	int32 BytesRead = 0;
	const double StartTime = FPlatformTime::Seconds();
	int32 ZeroReadTries = 0;

	while (BytesRead < NumBytes)
	{
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			return false;
		}

		// Select-level readiness probe. Without this, UE's FSocket::Recv on
		// a just-accepted FTcpListener socket can return Read=0 before the
		// kernel has delivered any data, which we'd mis-interpret as a FIN
		// and close the connection — producing WSAECONNABORTED 10053 on
		// the client mid-recv. Wait() reports readable only once real data
		// (or a real FIN) is present.
		const bool bReadable = Socket->Wait(
			ESocketWaitConditions::WaitForRead,
			FTimespan::FromMilliseconds(50));
		if (!bReadable)
		{
			continue; // Keep checking until TimeoutSeconds elapses.
		}

		uint32 PendingBytes = 0;
		const bool bHasPending = Socket->HasPendingData(PendingBytes);

		int32 Read = 0;
		const bool bRecvOk = Socket->Recv(Buffer + BytesRead, NumBytes - BytesRead, Read);
		if (bRecvOk)
		{
			if (Read == 0)
			{
				// Confirm genuine FIN: no pending kernel data AND a small retry budget
				// exhausted. Spurious zero-reads (UE socket edge case) are rare but
				// documented above.
				if (bHasPending && PendingBytes > 0)
				{
					++ZeroReadTries;
					FPlatformProcess::Sleep(0.001f);
					continue;
				}
				if (Socket->GetConnectionState() == SCS_Connected && ZeroReadTries < 5)
				{
					++ZeroReadTries;
					FPlatformProcess::Sleep(0.002f);
					continue;
				}
				return false;
			}
			BytesRead += Read;
			ZeroReadTries = 0;
		}
		else
		{
			if (Socket->GetConnectionState() != SCS_Connected)
			{
				return false;
			}
			FPlatformProcess::Sleep(0.001f);
		}
	}

	return true;
}

bool FUnrealBridgeServer::SendAll(FSocket* Socket, const uint8* Buffer, int32 NumBytes)
{
	int32 BytesSent = 0;

	while (BytesSent < NumBytes)
	{
		int32 Sent = 0;
		if (!Socket->Send(Buffer + BytesSent, NumBytes - BytesSent, Sent))
		{
			return false;
		}
		BytesSent += Sent;
	}

	return true;
}


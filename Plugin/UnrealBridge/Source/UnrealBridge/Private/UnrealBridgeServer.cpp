#include "UnrealBridgeServer.h"
#include "IPythonScriptPlugin.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "SocketSubsystem.h"
#include "Misc/Base64.h"
#include "Editor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridge, Log, All);

namespace UnrealBridgeLimits
{
	// Max request JSON payload. 10 MB is generous for human-authored scripts;
	// the upper bound exists mainly to stop a malicious/buggy client from
	// triggering an OOM in the editor via blind SetNumUninitialized.
	constexpr int32 MaxRequestBytes = 10 * 1024 * 1024;
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
	if (bIsRunning)
	{
		return true;
	}

	ListenPort = Port;

	FIPv4Endpoint Endpoint(FIPv4Address(127, 0, 0, 1), ListenPort);

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
		UE_LOG(LogUnrealBridge, Error, TEXT("Failed to create TCP listener on port %d"), ListenPort);
		Listener.Reset();
		return false;
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
	UE_LOG(LogUnrealBridge, Log, TEXT("Listening on 127.0.0.1:%d"), ListenPort);
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

	// 4. Build response
	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("id"), RequestId);

	FString Command;
	Request->TryGetStringField(TEXT("command"), Command); // optional

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

			const double ExecT0 = FPlatformTime::Seconds();
			FExecResult Result = EnqueueAndWaitForExec(Script, Timeout, RequestId);
			const double ExecMs = (FPlatformTime::Seconds() - ExecT0) * 1000.0;

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
		"    if _ub_o: print(_ub_o, end='')\n"
		"    if _ub_e: print('__UB_ERR__' + _ub_e, end='')\n"
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

	const FString ErrSentinel = TEXT("__UB_ERR__");
	int32 ErrIdx = FullOutput.Find(ErrSentinel);
	if (ErrIdx != INDEX_NONE)
	{
		Result.Output = FullOutput.Left(ErrIdx);
		Result.Error = FullOutput.Mid(ErrIdx + ErrSentinel.Len());
		Result.bSuccess = false;
	}
	else
	{
		Result.Output = FullOutput;
		Result.Error = FString();
		Result.bSuccess = bExecSuccess;
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


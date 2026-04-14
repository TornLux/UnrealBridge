#include "UnrealBridgeServer.h"
#include "IPythonScriptPlugin.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridge, Log, All);

// ─────────────────────────────────────────────────────────────
// Helper: Quote a Python string safely
// ─────────────────────────────────────────────────────────────
static FString QuotePythonString(const FString& Input)
{
	FString Escaped = Input;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("\"\"\""), TEXT("\\\"\\\"\\\""));
	return FString::Printf(TEXT("\"\"\"%s\"\"\""), *Escaped);
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

	bIsRunning = true;
	UE_LOG(LogUnrealBridge, Log, TEXT("Listening on 127.0.0.1:%d"), ListenPort);
	return true;
}

void FUnrealBridgeServer::Stop()
{
	bIsRunning = false;

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (Listener.IsValid())
	{
		Listener.Reset();
	}

	// Fulfill any queued execs with a shutdown error so worker threads unblock.
	TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
	while (ExecQueue.Dequeue(Pending) && Pending.IsValid())
	{
		FExecResult R;
		R.bSuccess = false;
		R.Error = TEXT("server shutting down");
		Pending->Promise.SetValue(MoveTemp(R));
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
	UE_LOG(LogUnrealBridge, Verbose, TEXT("Client connected from %s"), *ClientEndpoint.ToString());

	// Handle each client on a background thread
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket]()
	{
		HandleClient(ClientSocket);

		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(ClientSocket);
		}
	});

	return true;
}

void FUnrealBridgeServer::HandleClient(FSocket* ClientSocket)
{
	while (bIsRunning)
	{
		// 1. Read 4-byte length prefix (big-endian)
		uint8 LenBuf[4];
		if (!RecvAll(ClientSocket, LenBuf, 4, 5.0f))
		{
			break; // Client disconnected or timeout
		}

		uint32 PayloadLen = (uint32(LenBuf[0]) << 24)
						  | (uint32(LenBuf[1]) << 16)
						  | (uint32(LenBuf[2]) << 8)
						  | (uint32(LenBuf[3]));

		if (PayloadLen == 0 || PayloadLen > 10 * 1024 * 1024) // 10 MB max
		{
			UE_LOG(LogUnrealBridge, Warning, TEXT("Invalid payload length: %u"), PayloadLen);
			break;
		}

		// 2. Read JSON payload
		TArray<uint8> PayloadBuf;
		PayloadBuf.SetNumUninitialized(PayloadLen);
		if (!RecvAll(ClientSocket, PayloadBuf.GetData(), PayloadLen, 30.0f))
		{
			break;
		}

		FUTF8ToTCHAR Converter((const ANSICHAR*)PayloadBuf.GetData(), PayloadLen);
		FString JsonStr(Converter.Length(), Converter.Get());

		// 3. Parse JSON
		TSharedPtr<FJsonObject> Request;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid())
		{
			UE_LOG(LogUnrealBridge, Warning, TEXT("Failed to parse JSON request"));
			break;
		}

		FString RequestId = Request->GetStringField(TEXT("id"));

		// 4. Build response
		TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("id"), RequestId);

		// Check for ping command
		FString Command = Request->HasField(TEXT("command"))
			? Request->GetStringField(TEXT("command"))
			: FString();
		if (Command == TEXT("ping"))
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("output"), TEXT("pong"));
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
		else
		{
			// Execute Python script — serialized through the GameThread ticker queue.
			FString Script = Request->GetStringField(TEXT("script"));
			float Timeout = Request->HasField(TEXT("timeout"))
				? (float)Request->GetNumberField(TEXT("timeout"))
				: 30.0f;

			FExecResult Result = EnqueueAndWaitForExec(Script, Timeout, RequestId);

			Response->SetBoolField(TEXT("success"), Result.bSuccess);
			Response->SetStringField(TEXT("output"), Result.Output);
			Response->SetStringField(TEXT("error"), Result.Error);
			Response->SetBoolField(TEXT("ready"), true);
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
			break;
		}
		if (!SendAll(ClientSocket, (const uint8*)Utf8Response.Get(), ResponseLen))
		{
			break;
		}
	}
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
	FString WrappedScript = FString::Printf(TEXT(
		"import sys, io as _io, traceback as _tb\n"
		"_ub_out, _ub_err = _io.StringIO(), _io.StringIO()\n"
		"_ub_old = sys.stdout, sys.stderr\n"
		"sys.stdout, sys.stderr = _ub_out, _ub_err\n"
		"try:\n"
		"    exec(compile(%s, '<unrealbridge>', 'exec'))\n"
		"except Exception:\n"
		"    sys.stderr.write(_tb.format_exc())\n"
		"finally:\n"
		"    sys.stdout, sys.stderr = _ub_old\n"
		"    _ub_o, _ub_e = _ub_out.getvalue(), _ub_err.getvalue()\n"
		"    _ub_out.close(); _ub_err.close()\n"
		"    if _ub_o: print(_ub_o, end='')\n"
		"    if _ub_e: print('__UB_ERR__' + _ub_e, end='')\n"
	), *QuotePythonString(Script));

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
	double StartTime = FPlatformTime::Seconds();

	while (BytesRead < NumBytes)
	{
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			return false;
		}

		int32 Read = 0;
		if (Socket->Recv(Buffer + BytesRead, NumBytes - BytesRead, Read))
		{
			if (Read == 0)
			{
				// Connection closed
				return false;
			}
			BytesRead += Read;
		}
		else
		{
			// Check if socket is still connected
			ESocketConnectionState State = Socket->GetConnectionState();
			if (State != SCS_Connected)
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


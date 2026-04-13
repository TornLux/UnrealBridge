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

	Listener = MakeUnique<FTcpListener>(
		Endpoint,
		FTimespan::FromSeconds(1.0),
		false /* bInReusable */
	);

	if (!Listener.IsValid() || !Listener->IsActive())
	{
		UE_LOG(LogUnrealBridge, Error, TEXT("Failed to create TCP listener on port %d"), ListenPort);
		Listener.Reset();
		return false;
	}

	Listener->OnConnectionAccepted().BindRaw(this, &FUnrealBridgeServer::OnConnectionAccepted);

	bIsRunning = true;
	UE_LOG(LogUnrealBridge, Log, TEXT("Listening on 127.0.0.1:%d"), ListenPort);
	return true;
}

void FUnrealBridgeServer::Stop()
{
	bIsRunning = false;

	if (Listener.IsValid())
	{
		Listener.Reset();
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
			// Execute Python script
			FString Script = Request->GetStringField(TEXT("script"));
			float Timeout = Request->HasField(TEXT("timeout"))
				? (float)Request->GetNumberField(TEXT("timeout"))
				: 30.0f;

			FExecResult Result = ExecutePython(Script, Timeout);

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
// Python execution
// ─────────────────────────────────────────────────────────────

FUnrealBridgeServer::FExecResult FUnrealBridgeServer::ExecutePython(const FString& Script, float TimeoutSeconds)
{
	FExecResult Result;

	// Python must execute on the Game Thread
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

	AsyncTask(ENamedThreads::GameThread, [&Result, &Script, DoneEvent]()
	{
		IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
		if (!PythonPlugin)
		{
			Result.bSuccess = false;
			Result.Error = TEXT("PythonScriptPlugin is not available");
			DoneEvent->Trigger();
			return;
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

		// ExecPythonCommandEx captures all Python output (print / unreal.log)
		// into CommandEx.LogOutput and the combined text into CommandEx.CommandResult
		FPythonCommandEx CommandEx;
		CommandEx.Command = WrappedScript;
		CommandEx.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
		CommandEx.FileExecutionScope = EPythonFileExecutionScope::Public;

		bool bExecSuccess = PythonPlugin->ExecPythonCommandEx(CommandEx);

		// Separate stdout from stderr using our __UB_ERR__ sentinel
		FString FullOutput;
		for (const FPythonLogOutputEntry& Entry : CommandEx.LogOutput)
		{
			FullOutput += Entry.Output + TEXT("\n");
		}

		// Also check CommandResult as a fallback
		if (FullOutput.IsEmpty() && !CommandEx.CommandResult.IsEmpty())
		{
			FullOutput = CommandEx.CommandResult;
		}

		// Split on our sentinel
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

		// Trim trailing newline from output
		Result.Output.TrimEndInline();
		Result.Error.TrimEndInline();

		DoneEvent->Trigger();
	});

	DoneEvent->Wait(FTimespan::FromSeconds(TimeoutSeconds));
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

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


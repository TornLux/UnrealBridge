#pragma once

#include "CoreMinimal.h"
#include "Common/TcpListener.h"
#include "Sockets.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Async/Future.h"

/**
 * TCP server that listens for incoming connections and executes Python scripts
 * in the Unreal Editor's Python interpreter.
 *
 * Protocol: Length-prefixed JSON frames
 *   Request:  <4 bytes big-endian length><JSON: {"id":"...", "script":"...", "timeout":30}>
 *   Response: <4 bytes big-endian length><JSON: {"id":"...", "success":bool, "output":"...", "error":"..."}>
 *
 * Special commands:
 *   {"id":"...", "command":"ping"} -> {"id":"...", "success":true, "output":"pong", "error":""}
 */
class FUnrealBridgeServer : public TSharedFromThis<FUnrealBridgeServer>
{
public:
	FUnrealBridgeServer();
	~FUnrealBridgeServer();

	/** Start listening on the given port. Returns true on success. */
	bool Start(int32 Port = 9876);

	/** Stop the server and close all connections. */
	void Stop();

	/** Whether the server is currently listening. */
	bool IsRunning() const;

	/**
	 * Mark the editor as fully initialized (main frame created). Until this is
	 * set, Python exec requests are rejected with a "not ready" error to avoid
	 * racing the render thread during SlateRHIRenderer::CreateViewport.
	 */
	void SetEditorReady(bool bReady);
	bool IsEditorReady() const;

private:
	/** Called by FTcpListener when a new client connects. */
	bool OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);

	/** Process a single client connection (runs on a worker thread). */
	void HandleClient(FSocket* ClientSocket);

	/** Read exactly NumBytes from the socket. Returns false on failure. */
	bool RecvAll(FSocket* Socket, uint8* Buffer, int32 NumBytes, float TimeoutSeconds);

	/** Send all bytes to the socket. Returns false on failure. */
	bool SendAll(FSocket* Socket, const uint8* Buffer, int32 NumBytes);

	/** Result of a Python exec request. */
	struct FExecResult
	{
		bool bSuccess = false;
		FString Output;
		FString Error;
	};

	/**
	 * A queued exec request. Heap-allocated and shared between the worker
	 * thread (which waits on Promise's future) and the GameThread ticker
	 * consumer (which fulfills Promise). Shared ownership guarantees no
	 * dangling references if the worker times out before the ticker runs.
	 */
	struct FPendingExec
	{
		FString Script;
		float TimeoutSeconds = 30.0f;
		FString RequestId;
		TPromise<FExecResult> Promise;
	};

	/** Enqueue a script for GameThread execution and block on the future. */
	FExecResult EnqueueAndWaitForExec(const FString& Script, float TimeoutSeconds, const FString& RequestId);

	/** GameThread ticker callback: drains at most one pending exec per frame. */
	bool TickConsumeQueue(float DeltaTime);

	/** Actual Python exec (GameThread only, called by ticker). */
	FExecResult DoPythonExec(const FString& Script);

	TUniquePtr<FTcpListener> Listener;
	int32 ListenPort = 9876;
	FThreadSafeBool bIsRunning = false;
	FThreadSafeBool bEditorReady = false;

	// Exec pipeline (item #1 of server stability plan).
	TQueue<TSharedPtr<FPendingExec, ESPMode::ThreadSafe>, EQueueMode::Mpsc> ExecQueue;
	FTSTicker::FDelegateHandle TickHandle;
	bool bExecInFlight = false; // GameThread-only, no atomic needed
};

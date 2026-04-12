#pragma once

#include "CoreMinimal.h"
#include "Common/TcpListener.h"
#include "Sockets.h"

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

private:
	/** Called by FTcpListener when a new client connects. */
	bool OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);

	/** Process a single client connection (runs on a worker thread). */
	void HandleClient(FSocket* ClientSocket);

	/** Read exactly NumBytes from the socket. Returns false on failure. */
	bool RecvAll(FSocket* Socket, uint8* Buffer, int32 NumBytes, float TimeoutSeconds);

	/** Send all bytes to the socket. Returns false on failure. */
	bool SendAll(FSocket* Socket, const uint8* Buffer, int32 NumBytes);

	/** Execute a Python script on the Game Thread and capture output. */
	struct FExecResult
	{
		bool bSuccess = false;
		FString Output;
		FString Error;
	};
	FExecResult ExecutePython(const FString& Script, float TimeoutSeconds);

	TUniquePtr<FTcpListener> Listener;
	int32 ListenPort = 9876;
	FThreadSafeBool bIsRunning = false;
};

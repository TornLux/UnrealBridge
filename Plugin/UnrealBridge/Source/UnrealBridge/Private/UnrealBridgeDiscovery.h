#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/ThreadSafeCounter.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Misc/ScopeLock.h"

class FSocket;
class FRunnableThread;
class FInternetAddr;
class FJsonObject;

/**
 * UDP multicast discovery responder.
 *
 * One background thread per editor instance. Binds to the configured multicast
 * group with SO_REUSEADDR so multiple editors on the same host coexist without
 * fighting over the port. When a client broadcasts a `probe` message into the
 * group, every running editor receives it and replies — unicast to the
 * probe's source address — with its own TCP bind + port (and a token
 * fingerprint if auth is required).
 *
 * Protocol:
 *
 *   probe  (client → group):
 *     {"v":1, "type":"probe", "request_id":"<uuid>",
 *      "filter": {"project": "<name|path|*>"}}
 *
 *   response (server → probe source):
 *     {"v":1, "type":"response", "request_id":"<uuid>",
 *      "pid": 1234, "project": "MyGame", "project_path": "C:/.../MyGame.uproject",
 *      "engine_version": "5.7.0",
 *      "tcp_bind": "127.0.0.1", "tcp_port": 54321,
 *      "token_fingerprint": "a1b2c3d4e5f60718"}    // "" when no token
 *
 * The TCP port is reported live from the running server — when the listener
 * is still negotiating port 0 at startup, `SetTcpPort` is used to hand the
 * resolved port back in before any probes are answered.
 */
class FBridgeDiscoveryService : public FRunnable
{
public:
	struct FConfig
	{
		/** Multicast group address and port — same constant on every host. */
		FIPv4Address GroupAddress = FIPv4Address(239, 255, 42, 99);
		int32 GroupPort = 9876;

		/** TCP bind + initial port reported in responses. */
		FString TcpBindAddress = TEXT("127.0.0.1");
		int32 TcpPort = 0;

		/** Project display name + .uproject path that filters match against. */
		FString ProjectName;
		FString ProjectPath;

		/** Engine version string (e.g. "5.7.0"). */
		FString EngineVersion;

		/** Optional token for auth — only the fingerprint is leaked in responses. */
		FString TokenFingerprint;
	};

	explicit FBridgeDiscoveryService(const FConfig& InConfig);
	virtual ~FBridgeDiscoveryService() override;

	/** Bind the UDP socket to the multicast group and spawn the listen thread. */
	bool StartService();

	/** Stop the thread and close the socket. Idempotent. */
	void StopService();

	/** Update the TCP port advertised in responses (used after port=0 resolves). */
	void SetTcpPort(int32 NewPort);

	/** Whether the service thread is alive. */
	bool IsRunning() const { return bIsRunning; }

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	/** Parse an incoming probe datagram and, if it addresses us, send a response. */
	void HandleDatagram(const uint8* Bytes, int32 Length, const FInternetAddr& Source);

	/** Build + serialize the JSON response for a matched probe. */
	FString BuildResponseJson(const FString& RequestId) const;

	/** True when the given filter matches this editor's project. */
	bool FilterMatchesUs(const FString& Filter) const;

	FConfig Config;

	FSocket* Socket = nullptr;
	TUniquePtr<FRunnableThread> Thread;

	FThreadSafeBool bIsRunning = false;
	FThreadSafeBool bStopRequested = false;

	/** Currently-advertised TCP port. Atomic so SetTcpPort from any thread. */
	FThreadSafeCounter CurrentTcpPort;
};

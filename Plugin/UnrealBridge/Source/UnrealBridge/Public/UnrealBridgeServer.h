#pragma once

#include "CoreMinimal.h"
#include "Common/TcpListener.h"
#include "Sockets.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Async/Future.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Set.h"
#include "Misc/ScopeLock.h"
#include "Interfaces/IPv4/IPv4Address.h"

class FUnrealBridgeEditorHealthCache;
class FUnrealBridgeWorkAdmissionGate;

/**
 * TCP server that listens for incoming connections and executes Python scripts
 * in the Unreal Editor's Python interpreter.
 *
 * Protocol: Length-prefixed protocol-v2 JSON frames
 *   Request:  <length><JSON: {"id":"...", "command":"exact_exec", "expected":{...}, "request":{"script":"..."}}>
 *   Response: <length><JSON: {"id":"...", "success":bool, "output":"...", "error":"...", "instance_id":"..."}>
 *
 * 所有命令必须使用 exact_* wire form，并在任何副作用调度前通过启动实例身份验证。
 * Every command must use the exact_* wire form and pass Server-start identity validation before side-effect dispatch.
 * `exact_editor_status` 只读取缓存的 Engine/Slate 健康快照。
 * `exact_editor_status` only reads the cached Engine/Slate health snapshot.
 */
class FUnrealBridgeServer : public TSharedFromThis<FUnrealBridgeServer, ESPMode::ThreadSafe>
{
public:
	/** Configuration for Start — replaces the legacy `int32 Port` signature. */
	struct FStartConfig
	{
		/** Interface to bind. 127.0.0.1 is local-only; 0.0.0.0 is all interfaces. */
		FIPv4Address BindAddress = FIPv4Address(127, 0, 0, 1);

		/**
		 * TCP port. `0` = let the OS pick a free ephemeral port; the actual
		 * bound port is read back via GetBoundPort() after Start() succeeds.
		 */
		int32 Port = 0;

		/**
		 * Optional token. When non-empty, every inbound JSON request must carry
		 * a matching `"token": "..."` field or it is rejected with "unauthorized".
		 * Always required when BindAddress is not 127.0.0.1 (the server refuses
		 * to start otherwise to avoid accidental RCE exposure on a LAN).
		 */
		FString Token;
	};

	FUnrealBridgeServer();
	~FUnrealBridgeServer();

	/** Bring the server up using the given config. Returns true on success. */
	bool Start(const FStartConfig& Config);

	/** Legacy entry — bind to 127.0.0.1 on the given port. Kept for hot-reload callers. */
	bool Start(int32 Port);

	/** Stop the server and close all connections. */
	void Stop();

	/** Whether the server is currently listening. */
	bool IsRunning() const;

	/** The interface the server bound to (as a dotted string). */
	FString GetBoundAddress() const { return BindAddressStr; }

	/** The TCP port actually resolved at bind time (differs from FStartConfig::Port when 0). */
	int32 GetBoundPort() const { return ListenPort; }

	/** True if a token was set and request-time auth is enforced. */
	bool HasToken() const { return !Token.IsEmpty(); }

	/** 本次 Server 启动的不可复用 UUID。 / Non-reusable UUID for this Server start. */
	const FString& GetInstanceId() const { return InstanceId; }

	/** TCP/discovery 共同使用且客户端必须逐字复制的 wire-canonical 工程路径。 / Wire-canonical project path shared by TCP/discovery and copied verbatim by clients. */
	const FString& GetProjectPath() const { return ProjectPath; }

	/** exact identity 中冻结的当前进程 ID。 / Current process ID frozen into exact identity. */
	int32 GetProcessId() const { return ProcessId; }

	/** 当前 exact-wire 协议版本。 / Current exact-wire protocol version. */
	static int32 GetProtocolVersion();

	/**
	 * Mark the editor as fully initialized (main frame created). Until this is
	 * set, Python exec requests are rejected with a "not ready" error to avoid
	 * racing the render thread during SlateRHIRenderer::CreateViewport.
	 */
	void SetEditorReady(bool bReady);
	bool IsEditorReady() const;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FUnrealBridgeServerTestAccessor;

	/** 为确定性 Automation 注入不调用 Python 的 exec body。 / Inject a non-Python exec body for deterministic Automation. */
	bool EnqueueExecForTesting(TFunction<void()>&& Body, bool bCancelBeforeConsume, const FString& RequestId);
#endif

	/** Called by FTcpListener when a new client connects. */
	bool OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);

	/** Process a single client connection (runs on a worker thread). */
	void HandleClient(FSocket* ClientSocket, const FString& EndpointStr);

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
	 * queued exec 的私有 control block；定义留在 Server.cpp，避免把调度细节暴露为公共 API。
	 * Private queued-exec control block, defined in Server.cpp so scheduling details stay out of the public API.
	 */
	struct FPendingExec;

	/** Enqueue a script for GameThread execution and block on the future. */
	FExecResult EnqueueAndWaitForExec(const FString& Script, float TimeoutSeconds, const FString& RequestId);

	/** GameThread ticker callback: records health and drains at most one pending exec per frame. */
	bool TickConsumeQueue(float DeltaTime);

	/** Slate 所属线程回调：更新无控件指针的模态健康摘要。 / Slate-owner callback: update the widget-free modal health summary. */
	void TickUpdateSlateHealth(float DeltaTime);

	/** 仅在 Slate 所属线程捕获当前模态摘要。 / Capture the current modal summary on the Slate-owning thread only. */
	void RefreshCachedSlateHealth();

	/** Actual Python exec (GameThread only, called by ticker). */
	FExecResult DoPythonExec(const FString& Script);

	TUniquePtr<FTcpListener> Listener;
	int32 ListenPort = 0;
	FString BindAddressStr = TEXT("127.0.0.1");
	FString Token;
	FString InstanceId;
	FString ProjectPath;
	int32 ProcessId = 0;
	FThreadSafeBool bIsRunning = false;
	FThreadSafeBool bEditorReady = false;

	// exec admission 与 shutdown 由同一 gate 排序；Close 后 queue 不能再收到新 work。
	// One gate orders exec admission against shutdown; no work can enter the queue after Close.
	TUniquePtr<FUnrealBridgeWorkAdmissionGate> WorkAdmission;

	// 健康缓存隔离 Slate 所属线程与 TCP 工作线程；工作线程只复制普通值。
	// The health cache isolates the Slate-owning thread from TCP workers; workers only copy plain values.
	TUniquePtr<FUnrealBridgeEditorHealthCache> EditorHealthCache;
	FDelegateHandle SlatePreTickHandle;

	TQueue<TSharedPtr<FPendingExec, ESPMode::ThreadSafe>, EQueueMode::Mpsc> ExecQueue;
	FTSTicker::FDelegateHandle TickHandle;
	bool bExecInFlight = false; // GameThread-only, no atomic needed

	// client worker graph events 让 Stop 等待 closure 真正销毁，而不只是等待 raw counter 归零。
	// Client worker graph events let Stop wait for closure destruction, not merely a raw counter reaching zero.
	FGraphEventArray ClientWorkerTasks;

	// Connection limit (item #5). Atomic because we increment/decrement from
	// the listener thread (accept path) and the task-graph worker (completion).
	FThreadSafeCounter ActiveClients;
	static constexpr int32 MaxConcurrentClients = 16;

	// Tracks in-flight client sockets so Stop() can force-close them (item #6).
	// Without this, each active HandleClient worker would sit in the 5 s idle
	// RecvAll on shutdown, stretching ShutdownModule by up to that long per
	// client.
	FCriticalSection ActiveSocketsLock;
	TSet<FSocket*> ActiveSockets;

	// PIE transition guard (item #11). Flag is True during the unsafe
	// startup (BeginPIE → PostPIEStarted) and shutdown (PrePIEEnded →
	// EndPIE) windows; False while PIE is stably running. Exec requests
	// are rejected while True because the editor subsystem state is
	// being torn down and rebuilt.
	FThreadSafeBool bPieTransitionActive = false;
	FDelegateHandle PieBeginHandle;
	FDelegateHandle PiePostStartedHandle;
	FDelegateHandle PiePreEndedHandle;
	FDelegateHandle PieEndHandle;
};

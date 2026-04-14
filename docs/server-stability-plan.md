# UnrealBridge Server — Stability Refactor Plan

This document lists the concrete changes needed to make `FUnrealBridgeServer` robust under real-world use (concurrent clients, long-running scripts, editor lifecycle events). Items are ordered roughly by priority; #1 is the architectural core and must land first because several later items depend on its new structure.

Scope: `Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeServer.cpp` and its header. The Python client (`bridge.py`) is out of scope unless a fix requires a protocol bump.

Known bug coverage:
- **Concurrent-exec editor crash** (Python reentrancy inside `ExecPythonCommandEx`) — addressed by #1.
- **Dangling-reference / event-pool reuse in `ExecutePython`** — addressed by #1.
- **Intermittent `WinError 10053 (WSAECONNABORTED)` on client calls** — addressed by #7 (listener poll interval + `SO_REUSEADDR`).
- **Editor crash on malformed JSON / missing fields** — addressed by #3.
- **OOM on oversized payloads** — addressed by #4.
- **Background-task pool starvation from unbounded clients** — addressed by #5.
- **Hangs on plugin reload / editor shutdown** — addressed by #6 + #12.

---

## 1. Serialize Python exec through a single ticker-consumed queue (CORE REFACTOR)

### Problem
Current `ExecutePython` dispatches one `AsyncTask(ENamedThreads::GameThread, ...)` per request and blocks the worker thread on a pooled `FEvent`. Two compounding bugs:

1. **Reentrancy crash** — when script A is mid-exec on GameThread and its Python code indirectly pumps the TaskGraph (asset load, blueprint compile, `WaitUntilTasksComplete`, etc.), script B's queued `AsyncTask` fires inside A's call stack. `IPythonScriptPlugin::ExecPythonCommandEx` is not reentrant; the editor crashes in `PyObject` teardown. This is the "exec during a busy GameThread kills the editor" symptom.
2. **Dangling refs + event-pool reuse** — `Result` and `Script` are captured by reference; `DoneEvent` is returned to the pool after `Wait`. If the lambda fires after timeout it writes to freed stack memory and triggers someone else's pooled event.

### Solution

Replace per-request `AsyncTask` with an MPSC queue drained by a single `FTSTicker` callback.

```cpp
struct FPendingExec : TSharedFromThis<FPendingExec, ESPMode::ThreadSafe>
{
    FString        Script;
    float          TimeoutSeconds;
    FString        RequestId;              // for logging
    TPromise<FExecResult> Promise;
};

class FUnrealBridgeServer
{
    TQueue<TSharedPtr<FPendingExec, ESPMode::ThreadSafe>, EQueueMode::Mpsc> ExecQueue;
    FTSTicker::FDelegateHandle TickHandle;
    std::atomic<int32> QueuedCount { 0 };
    bool bExecInFlight = false;            // GameThread-only; no atomic needed
};
```

**Worker thread** (inside `HandleClient`):
```cpp
auto Pending = MakeShared<FPendingExec, ESPMode::ThreadSafe>();
Pending->Script = ...; Pending->TimeoutSeconds = ...; Pending->RequestId = ReqId;
TFuture<FExecResult> Future = Pending->Promise.GetFuture();
++QueuedCount;
ExecQueue.Enqueue(Pending);
const bool bReady = Future.WaitFor(FTimespan::FromSeconds(Pending->TimeoutSeconds));
--QueuedCount;
FExecResult Result = bReady ? Future.Get()
                            : FExecResult{ false, TEXT(""), TEXT("exec timeout") };
```

**GameThread ticker** (registered in `Start()`):
```cpp
TickHandle = FTSTicker::GetCoreTicker().AddTicker(
    FTickerDelegate::CreateRaw(this, &FUnrealBridgeServer::TickConsumeQueue),
    0.0f);

bool TickConsumeQueue(float)
{
    if (bExecInFlight) return true;                      // belt-and-suspenders
    TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
    if (!ExecQueue.Dequeue(Pending) || !Pending) return true;
    bExecInFlight = true;
    FExecResult R = DoPythonExec(Pending->Script);       // ONLY caller of ExecPythonCommandEx
    Pending->Promise.SetValue(MoveTemp(R));
    bExecInFlight = false;
    return true;
}
```

### Why ticker beats `AsyncTask(GameThread) + FCriticalSection`
- `FTSTicker` fires only from `FEngineLoop::Tick`'s explicit `FTSTicker::Tick(DeltaTime)` call. TaskGraph pumps (`WaitUntilTasksComplete`, etc.) do **not** re-enter the ticker. A `FCriticalSection` on the worker side cannot prevent the GameThread from pulling a queued `AsyncTask` mid-script.
- Ownership via `TSharedPtr<FPendingExec>` eliminates the dangling-ref and event-pool bugs with no extra synchronization.
- Idle cost ≈ one atomic load + empty dequeue ≈ < 100 ns per frame. Unmeasurable.

### Ordering constraint
Items #6, #8, #9, #10, #13, #14 all touch code that #1 rewrites. Land #1 first, then apply the rest on the new structure.

---

## 2. Eliminate triple-quote script wrapping — use base64

### Problem
`QuotePythonString` wraps the user script in `"""..."""` and tries to escape internal `"""` with `\"\"\"`. **Python triple-quoted strings do not honor backslash escapes for quotes.** Any user script containing `"""` (docstrings, multi-line SQL, markdown embedded in code) breaks `exec()` parsing. Future refactors inside this project risk stepping on it.

### Solution
Base64-encode the script on the server side; the injected Python shim decodes and execs:

```cpp
const FString B64 = FBase64::Encode(FTCHARToUTF8(*Script).Get(),
                                    FTCHARToUTF8(*Script).Length());
const FString Wrapped = FString::Printf(TEXT(
    "import base64 as _b64\n"
    "_src = _b64.b64decode('%s').decode('utf-8')\n"
    "import sys, io as _io, traceback as _tb\n"
    "_ub_out, _ub_err = _io.StringIO(), _io.StringIO()\n"
    "_ub_old = sys.stdout, sys.stderr\n"
    "sys.stdout, sys.stderr = _ub_out, _ub_err\n"
    "try:\n"
    "    exec(compile(_src, '<unrealbridge>', 'exec'))\n"
    "except Exception:\n"
    "    sys.stderr.write(_tb.format_exc())\n"
    "finally:\n"
    "    sys.stdout, sys.stderr = _ub_old\n"
    "    _o, _e = _ub_out.getvalue(), _ub_err.getvalue()\n"
    "    _ub_out.close(); _ub_err.close()\n"
    "    if _o: print(_o, end='')\n"
    "    if _e: print('__UB_ERR__' + _e, end='')\n"
), *B64);
```

No escaping. Can't be broken by any user-script content. Delete `QuotePythonString`.

---

## 3. Request JSON: use `TryGet*` and fail gracefully on missing fields

### Problem
`Request->GetStringField(TEXT("script"))` — UE 5.x logs a warning and returns an empty string on missing field; on some engine configurations a `check()` fires. Either way the client gets a confusing response. `GetStringField(TEXT("id"))` has the same issue.

### Solution
Validate all fields with `TryGetStringField` / `TryGetNumberField`, respond with a clear protocol error if missing:

```cpp
FString RequestId;
if (!Request->TryGetStringField(TEXT("id"), RequestId)) {
    // no id — we can still respond, but log it
    RequestId = TEXT("<missing>");
}

FString Command;
Request->TryGetStringField(TEXT("command"), Command);  // optional

if (Command == TEXT("ping")) { ... }
else {
    FString Script;
    if (!Request->TryGetStringField(TEXT("script"), Script)) {
        Respond(Response, false, TEXT(""), TEXT("missing 'script' field"));
        continue;
    }
    double TimeoutNum = 30.0;
    Request->TryGetNumberField(TEXT("timeout"), TimeoutNum);
    float Timeout = FMath::Clamp((float)TimeoutNum, 0.1f, 300.0f);
    ...
}
```

---

## 4. Bounded payload read — no blind `SetNumUninitialized`

### Problem
Server validates `PayloadLen <= 10 MB` but then calls `PayloadBuf.SetNumUninitialized(PayloadLen)`. Under low-memory or fragmented-heap conditions this can OOM the editor. A malicious client could also send a length right at the limit in many concurrent connections.

### Solution
Stream-read into a fixed-capacity reusable buffer, or `TryReserve` with rollback:

```cpp
TArray<uint8> PayloadBuf;
PayloadBuf.Reserve(PayloadLen);
if (PayloadBuf.GetAllocatedSize() < PayloadLen) {
    UE_LOG(LogUnrealBridge, Warning, TEXT("Failed to allocate %u bytes for payload"), PayloadLen);
    break;
}
PayloadBuf.SetNumUninitialized(PayloadLen);
```

Also add a symmetric `MaxResponseBytes` cap (e.g. 4 MB) in `HandleClient` response-write path — truncate+flag oversize Python output instead of sending multi-MB strings.

---

## 5. Concurrent connection limit

### Problem
`OnConnectionAccepted` spawns an `AnyBackgroundThreadNormalTask` per connection with no ceiling. A script doing `while True: subprocess.Popen(['bridge.py', 'exec', ...])` would saturate the editor's background task pool and starve other engine work.

### Solution
Atomic counter + hard limit; reject with a JSON-framed error response before dispatching the worker:

```cpp
static constexpr int32 MaxConcurrentClients = 8;
std::atomic<int32> ActiveClients { 0 };

bool OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint&)
{
    if (ActiveClients.load() >= MaxConcurrentClients) {
        // Send a one-shot error and close. Inline response so client sees a real error, not a reset.
        SendProtocolError(ClientSocket, TEXT("server busy — too many concurrent clients"));
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        return true;
    }
    ++ActiveClients;
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket]() {
        HandleClient(ClientSocket);
        --ActiveClients;
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
    });
    return true;
}
```

`MaxConcurrentClients` should be a `TAutoConsoleVariable` so ops can tune without rebuild.

---

## 6. Graceful `Stop()` — close active clients instead of letting them idle-timeout

### Problem
`Stop()` resets the listener but does nothing to in-flight `HandleClient` loops. Each active worker sits in its 5-second `RecvAll` until it times out, then exits. During editor shutdown this stretches hang time by up to 5 s per active client.

### Solution
Track active client sockets and force-close them on `Stop()`:

```cpp
FCriticalSection ClientsLock;
TSet<FSocket*> ActiveSockets;

void Stop() {
    bIsRunning.store(false);
    Listener.Reset();
    {
        FScopeLock L(&ClientsLock);
        for (FSocket* S : ActiveSockets) { S->Close(); }  // unblocks Recv immediately
    }
    // Wait (bounded) for in-flight HandleClient workers to drain — see #14.
}
```

`HandleClient` registers/unregisters itself in `ActiveSockets` at entry/exit under the lock.

---

## 7. Listener tuning — poll interval + reusable address (fixes WSAECONNABORTED 10053)

### Problem

`MakeUnique<FTcpListener>(Endpoint, FTimespan::FromSeconds(1.0), false /* bInReusable */)` has **two independent defects** that together produce the intermittent `WinError 10053 (WSAECONNABORTED)` clients observe:

**Poll interval = 1.0 s is the dominant cause.** `FTcpListener` is an `FRunnable` that wakes on the given `InSleepTime` to call `Accept()`. A new client's `connect()` + `sendall()` completes at kernel level in milliseconds, but the data then sits in the server's kernel backlog **for up to 1 second** until the listener thread happens to wake. During that window two races produce 10053:

- The previous worker thread is in `HandleClient`'s 5 s idle `RecvAll`; as it exits and calls `DestroySocket`, the new (not-yet-accepted) socket's accept is delayed by the same poll tick — the Windows TCP stack occasionally judges the pending connection as aborted.
- Under GameThread pressure (GC, shader compile spikes), the listener's `FRunnable` can be scheduled late, stretching 1 s to 2 s+ and giving the client's stack enough time to give up with 10053.

**`bInReusable = false` is the secondary cause.** Rapid connect/close cycles (e.g. CI scripts, the library-iteration loop) accumulate `TIME_WAIT`/`CLOSE_WAIT` entries on `(127.0.0.1, 9876)`. Some of the follow-up connects land on a 4-tuple Windows still considers reserved, and the stack aborts with 10053.

### Solution

Single two-line fix addresses both:

```cpp
Listener = MakeUnique<FTcpListener>(
    Endpoint,
    FTimespan::FromMilliseconds(100),   // 10× faster poll — shrinks both race windows
    true                                 // bInReusable — reclaim TIME_WAIT sockets
);
```

- **100 ms** is small enough that accept latency becomes unnoticeable (bridge call overhead is already dominated by subprocess start-up and JSON/socket round-trip), and large enough that the `FRunnable` doesn't burn meaningful CPU spinning.
- On Windows, `SO_REUSEADDR` (what UE 5.7's `FTcpListener` sets when `bInReusable=true`) permits binding to a port currently in `TIME_WAIT`, which is precisely what we need after a crash/restart and during quick reconnect bursts.

### Validation

After landing:
1. Fire the `concurrent_fire.py` stress test from commit `48bb8c6`'s verification session (10 parallel exec-file) five times in a row — expect 50/50 success rate (previously intermittent).
2. Kill + relaunch the editor within 10 s; confirm the bridge comes back up without an "address in use" error (previously could take up to 120 s for `TIME_WAIT` to expire).

### Why this belongs as its own commit

It's a single self-contained listener-config change, independent of the queue refactor (#1) and the payload/protocol work (#2–#5). Landing it alone makes the 10053 reduction measurable and gives us a clean bisect point if any regression shows up.

---

## 8. Atomic `bIsRunning` / `bEditorReady`

### Problem
Both flags are plain `bool` read/written across threads (listener thread, worker threads, GameThread). On x86 the naked read is unlikely to tear, but there are no memory barriers — a worker can loop in `HandleClient` long after `Stop()` flipped `bIsRunning` to false on another thread.

### Solution
```cpp
std::atomic<bool> bIsRunning { false };
std::atomic<bool> bEditorReady { false };
```

Use `.load(std::memory_order_acquire)` / `.store(..., std::memory_order_release)` or accept default `seq_cst` — the traffic is tiny. All existing accesses compile as-is with implicit conversion; add explicit `.load()`/`.store()` where clarity helps.

---

## 9. Log every error branch in `HandleClient`

### Problem
Almost every `break` in `HandleClient` is silent. When clients see 10053, there's no log trail on the server to say "oversize payload", "parse failed at JSON", "recv timeout after 5s", "sendall failed mid-response", etc.

### Solution
Convert each `break` path to a `UE_LOG` at `Log` or `Verbose` level, tagged with the request id when available:

```cpp
if (!RecvAll(..., 4, 5.0f)) {
    UE_LOG(LogUnrealBridge, Verbose, TEXT("[client=%s] recv length-prefix timeout — closing"), *ClientEndpointStr);
    break;
}
if (PayloadLen > MaxPayloadBytes) {
    UE_LOG(LogUnrealBridge, Warning, TEXT("[client=%s] oversize payload %u > %u — closing"),
           *ClientEndpointStr, PayloadLen, MaxPayloadBytes);
    SendProtocolError(ClientSocket, TEXT("payload too large"));
    break;
}
// ...parse failure, send failure, shutdown etc.
```

Store the parsed client endpoint string at the top of `HandleClient` so every log line has it.

---

## 10. Echo request id in logs + tie to exec lifecycle

### Problem
We parse `RequestId` and use it only in the response. When diagnosing a hang or crash there's no way to correlate "which request did this" from the log.

### Solution
Log at two canonical points:

```cpp
UE_LOG(LogUnrealBridge, Verbose, TEXT("[%s] received: command=%s script_len=%d timeout=%.1fs queue_depth=%d"),
       *RequestId, *Command, Script.Len(), Timeout, QueuedCount.load());

// after DoPythonExec
UE_LOG(LogUnrealBridge, Verbose, TEXT("[%s] done: success=%s output=%dB error=%dB elapsed=%.2fs"),
       *RequestId, Result.bSuccess?TEXT("true"):TEXT("false"),
       Result.Output.Len(), Result.Error.Len(), Elapsed);
```

Add a `Log`-level line whenever exec exceeds half its timeout — surfaces slow scripts before they hit the cliff.

---

## 11. PIE transition protection for exec

### Problem
Entering/exiting PIE temporarily reconstructs the world, reloads subsystems, and pumps TaskGraph heavily. Any exec arriving in that window has an elevated crash risk (asset loads fail partially, GameThread is in a transient state). Users hitting "Play" while a CLI-driven workflow is running can reliably crash the editor.

### Solution
Track PIE state via editor delegates and gate exec:

```cpp
FDelegateHandle PieBeginHandle, PieEndHandle;
std::atomic<bool> bPieTransitionActive { false };

void Start(...) {
    PieBeginHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FUnrealBridgeServer::OnBeginPie);
    PieEndHandle   = FEditorDelegates::EndPIE.AddRaw(this, &FUnrealBridgeServer::OnEndPie);
}
void OnBeginPie(const bool) { bPieTransitionActive = true; }
void OnEndPie(const bool)   { bPieTransitionActive = false; }
```

In `HandleClient` before dispatching exec:
```cpp
if (bPieTransitionActive.load()) {
    Respond(Response, false, TEXT(""), TEXT("editor is in PIE transition — retry in a moment"));
    continue;
}
```

Optional: callers can pass `{"allow_during_pie": true}` to bypass for read-only scripts; default is reject.

Remember to unbind the delegates in `Stop()`.

---

## 12. Clean plugin unload — join active work

### Problem
`ShutdownModule` in `UnrealBridgeModule` can run while:
- An exec is mid-flight on GameThread (ticker queue).
- A `HandleClient` worker is blocked in `RecvAll`.
- A `TPromise` is still waiting to be fulfilled.

Today the server is simply destroyed. Ticker delegate handle leaks, outstanding `TSharedPtr<FPendingExec>` keep promises alive with no consumer, worker threads scribble into torn-down state when they wake.

### Solution
Shutdown sequence in `Stop()`:

```cpp
void Stop()
{
    if (!bIsRunning.exchange(false)) return;

    // 1. Stop accepting new connections.
    Listener.Reset();

    // 2. Unregister editor/PIE delegates and ticker.
    if (TickHandle.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
    FEditorDelegates::BeginPIE.Remove(PieBeginHandle);
    FEditorDelegates::EndPIE.Remove(PieEndHandle);

    // 3. Fulfill any queued execs with a shutdown error so worker threads return.
    TSharedPtr<FPendingExec, ESPMode::ThreadSafe> Pending;
    while (ExecQueue.Dequeue(Pending) && Pending) {
        Pending->Promise.SetValue(FExecResult{ false, TEXT(""), TEXT("server shutting down") });
    }

    // 4. Force-close active client sockets so Recv unblocks (#6).
    { FScopeLock L(&ClientsLock); for (FSocket* S : ActiveSockets) { S->Close(); } }

    // 5. Bounded wait for worker threads to drain.
    const double Deadline = FPlatformTime::Seconds() + 3.0;
    while (ActiveClients.load() > 0 && FPlatformTime::Seconds() < Deadline) {
        FPlatformProcess::Sleep(0.01f);
    }
    if (ActiveClients.load() > 0) {
        UE_LOG(LogUnrealBridge, Warning, TEXT("Stop(): %d clients still active after 3s"), ActiveClients.load());
    }
}
```

This makes plugin reload (Live Coding, project switch, editor quit) hang-free and leak-free.

---

## Implementation order

1. **#1** — done in `48bb8c6`. Foundation for everything else.
2. **#7** next — smallest diff, biggest user-visible win (10053 intermittent failures disappear). Independent of all other items, no risk of regressing #1.
3. **#2, #3, #4** — small self-contained fixes, any order.
4. **#8** — trivial atomic conversion; batch with #3 or #4 commit.
5. **#5, #6, #9, #10** — medium, each is a short commit.
6. **#11, #12** — depend on the new structure from #1. Last.

Each change should land as its own commit with the `Server:` prefix in the message (distinct from the library-tick `Editor:`/`Level:`/etc. prefixes), so `git log --grep "^Server:"` surfaces the refactor trail.

## Out of scope (for now)

- Protocol versioning handshake
- Structured error responses (typed error codes instead of strings)
- HMAC / token auth for the TCP port
- Streaming Python stdout back to client mid-exec
- Multi-editor routing (one bridge for several editors on different ports)

These are reasonable follow-ups once the 12 items above are in.

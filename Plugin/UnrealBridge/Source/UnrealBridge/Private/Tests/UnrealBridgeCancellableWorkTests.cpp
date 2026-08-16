#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealBridgeCancellableWork.h"
#include "UnrealBridgeServer.h"
#include "SocketSubsystem.h"
#include "Misc/ScopeExit.h"

/**
 * Automation 只读访问 Server lifecycle 计数，不改变生产行为。
 * Automation-only read access to Server lifecycle counters; it does not change production behavior.
 */
class FUnrealBridgeServerTestAccessor
{
public:
	static int32 GetActiveClientCount(const FUnrealBridgeServer& Server)
	{
		return Server.ActiveClients.GetValue();
	}

	static int32 GetTrackedWorkerCount(const FUnrealBridgeServer& Server)
	{
		return Server.ClientWorkerTasks.Num();
	}

	static bool EnqueueExec(
		FUnrealBridgeServer& Server,
		TFunction<void()>&& Body,
		bool bCancelBeforeConsume,
		const FString& RequestId)
	{
		return Server.EnqueueExecForTesting(MoveTemp(Body), bCancelBeforeConsume, RequestId);
	}

	static void TickExecQueue(FUnrealBridgeServer& Server)
	{
		Server.TickConsumeQueue(0.0f);
	}
};

namespace UnrealBridgeCancellableWorkTests
{
	using FIntWork = TUnrealBridgeCancellableWork<int32>;

	int32 StateValue(EUnrealBridgeWorkState State)
	{
		return static_cast<int32>(State);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeCancelledWorkSkipsLateConsumerTest,
	"UnrealBridge.Server.CancellableWork.CancelledWorkSkipsLateConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeCancelledWorkSkipsLateConsumerTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeCancellableWorkTests;
	(void)Parameters;

	int32 SideEffectCount = 0;
	TFunction<int32()> Body = [&SideEffectCount]()
	{
		++SideEffectCount;
		return 42;
	};
	TSharedPtr<FIntWork, ESPMode::ThreadSafe> Work =
		MakeShared<FIntWork, ESPMode::ThreadSafe>(MoveTemp(Body));

	EUnrealBridgeWorkState ObservedState = EUnrealBridgeWorkState::Queued;
	TestTrue(TEXT("queued cancellation wins"), Work->TryCancel(-7, ObservedState));
	TestEqual(TEXT("cancel observes terminal state"), StateValue(ObservedState),
		StateValue(EUnrealBridgeWorkState::Cancelled));
	TestFalse(TEXT("late consumer cannot claim cancelled work"), Work->TryExecute());
	TestEqual(TEXT("cancelled work has no late side effect"), SideEffectCount, 0);
	TestEqual(TEXT("state remains cancelled"), StateValue(Work->GetState()),
		StateValue(EUnrealBridgeWorkState::Cancelled));

	TestTrue(TEXT("cancellation publishes one result"), Work->WaitFor(FTimespan::Zero()));
	if (Work->WaitFor(FTimespan::Zero()))
	{
		TestEqual(TEXT("published cancellation result is stable"), Work->GetResult(), -7);
	}

	EUnrealBridgeWorkState SecondObservedState = EUnrealBridgeWorkState::Queued;
	TestFalse(TEXT("terminal cancellation cannot publish twice"),
		Work->TryCancel(-9, SecondObservedState));
	TestEqual(TEXT("second cancellation observes first terminal state"),
		StateValue(SecondObservedState), StateValue(EUnrealBridgeWorkState::Cancelled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeRunningWorkCannotBeCancelledTest,
	"UnrealBridge.Server.CancellableWork.RunningWorkCannotBeCancelled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeRunningWorkCannotBeCancelledTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeCancellableWorkTests;
	(void)Parameters;

	TSharedPtr<FIntWork, ESPMode::ThreadSafe> Work;
	bool bCancellationWon = true;
	EUnrealBridgeWorkState StateSeenInsideBody = EUnrealBridgeWorkState::Queued;
	TFunction<int32()> Body = [&Work, &bCancellationWon, &StateSeenInsideBody]()
	{
		// body 内的显式尝试稳定模拟 deadline 在 consumer claim 后到达。
		// An explicit in-body attempt deterministically models a deadline arriving after consumer claim.
		bCancellationWon = Work->TryCancel(-1, StateSeenInsideBody);
		return 42;
	};
	Work = MakeShared<FIntWork, ESPMode::ThreadSafe>(MoveTemp(Body));

	TestTrue(TEXT("consumer claims and executes queued work"), Work->TryExecute());
	TestFalse(TEXT("running work cannot be relabelled cancelled"), bCancellationWon);
	TestEqual(TEXT("deadline observes running"), StateValue(StateSeenInsideBody),
		StateValue(EUnrealBridgeWorkState::Running));
	TestEqual(TEXT("successful body reaches completed"), StateValue(Work->GetState()),
		StateValue(EUnrealBridgeWorkState::Completed));
	TestTrue(TEXT("completed work publishes its result"), Work->WaitFor(FTimespan::Zero()));
	if (Work->WaitFor(FTimespan::Zero()))
	{
		TestEqual(TEXT("completed result remains authoritative"), Work->GetResult(), 42);
	}

	EUnrealBridgeWorkState ObservedState = EUnrealBridgeWorkState::Queued;
	TestFalse(TEXT("completed work cannot be cancelled"), Work->TryCancel(-2, ObservedState));
	TestEqual(TEXT("post-completion cancellation observes completed"), StateValue(ObservedState),
		StateValue(EUnrealBridgeWorkState::Completed));
	TestFalse(TEXT("completed work cannot execute twice"), Work->TryExecute());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeAdmissionGateRejectsAfterCloseTest,
	"UnrealBridge.Server.CancellableWork.AdmissionGateRejectsAfterClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeAdmissionGateRejectsAfterCloseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FUnrealBridgeWorkAdmissionGate Gate;
	int32 AdmissionCount = 0;
	TestFalse(TEXT("closed gate rejects initial work"), Gate.TryAdmit([&AdmissionCount]()
	{
		++AdmissionCount;
	}));

	Gate.Open();
	TestTrue(TEXT("open gate admits registration"), Gate.TryAdmit([&AdmissionCount]()
	{
		++AdmissionCount;
	}));
	Gate.Close();
	TestFalse(TEXT("closed shutdown gate rejects trailing work"), Gate.TryAdmit([&AdmissionCount]()
	{
		++AdmissionCount;
	}));
	TestEqual(TEXT("only pre-shutdown callback ran"), AdmissionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeCancelledBacklogDrainsBeforeLiveWorkTest,
	"UnrealBridge.Server.CancellableWork.CancelledBacklogDrainsBeforeLiveWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeCancelledBacklogDrainsBeforeLiveWorkTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedRef<FUnrealBridgeServer, ESPMode::ThreadSafe> Server =
		MakeShared<FUnrealBridgeServer, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("test server starts for production ticker coverage"), Server->Start(0)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (Server->IsRunning())
		{
			Server->Stop();
		}
	};

	int32 CancelledSideEffects = 0;
	int32 LiveSideEffects = 0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TFunction<void()> Body = [&CancelledSideEffects]()
		{
			++CancelledSideEffects;
		};
		TestTrue(TEXT("cancelled backlog item entered the production queue"),
			FUnrealBridgeServerTestAccessor::EnqueueExec(
				*Server, MoveTemp(Body), true, FString::Printf(TEXT("cancelled-%d"), Index)));
	}

	TFunction<void()> LiveBody = [&LiveSideEffects]()
	{
		++LiveSideEffects;
	};
	TestTrue(TEXT("live item entered behind cancelled tombstones"),
		FUnrealBridgeServerTestAccessor::EnqueueExec(
			*Server, MoveTemp(LiveBody), false, TEXT("live")));

	FUnrealBridgeServerTestAccessor::TickExecQueue(*Server);
	TestEqual(TEXT("cancelled production backlog produced no side effects"), CancelledSideEffects, 0);
	TestEqual(TEXT("production ticker reached one live body in the same drain"), LiveSideEffects, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeStopDrainsStragglingWorkerTest,
	"UnrealBridge.Server.CancellableWork.StopDrainsStragglingWorker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeStopDrainsStragglingWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedRef<FUnrealBridgeServer, ESPMode::ThreadSafe> Server =
		MakeShared<FUnrealBridgeServer, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("test server starts on an ephemeral port"), Server->Start(0)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		if (Server->IsRunning())
		{
			Server->Stop();
		}
	};

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!TestNotNull(TEXT("socket subsystem is available"), SocketSubsystem))
	{
		return false;
	}
	FSocket* ClientSocket = SocketSubsystem->CreateSocket(
		NAME_Stream, TEXT("UnrealBridge shutdown automation client"), false);
	if (!TestNotNull(TEXT("client socket was created"), ClientSocket))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		ClientSocket->Close();
		SocketSubsystem->DestroySocket(ClientSocket);
	};

	TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
	bool bValidAddress = false;
	Address->SetIp(TEXT("127.0.0.1"), bValidAddress);
	Address->SetPort(Server->GetBoundPort());
	TestTrue(TEXT("loopback address is valid"), bValidAddress);
	if (!TestTrue(TEXT("client connects to the test server"), ClientSocket->Connect(*Address)))
	{
		return false;
	}

	// 只发送 frame header，让真实 worker 阻塞在 payload recv；Stop 必须关 socket 并等待 graph event。
	// Send only a frame header so the real worker blocks in payload recv; Stop must close it and wait for its graph event.
	const uint8 Header[4] = { 0, 0, 0, 32 };
	int32 BytesSent = 0;
	TestTrue(TEXT("partial request header was sent"),
		ClientSocket->Send(Header, static_cast<int32>(UE_ARRAY_COUNT(Header)), BytesSent));
	TestEqual(TEXT("full frame header was sent"), BytesSent,
		static_cast<int32>(UE_ARRAY_COUNT(Header)));

	const double AcceptDeadline = FPlatformTime::Seconds() + 2.0;
	while (FUnrealBridgeServerTestAccessor::GetActiveClientCount(*Server) == 0
		&& FPlatformTime::Seconds() < AcceptDeadline)
	{
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("one straggling worker is tracked before shutdown"),
		FUnrealBridgeServerTestAccessor::GetActiveClientCount(*Server), 1);

	Server->Stop();
	TestEqual(TEXT("Stop waits for worker closure completion"),
		FUnrealBridgeServerTestAccessor::GetActiveClientCount(*Server), 0);
	TestEqual(TEXT("Stop releases all tracked graph events"),
		FUnrealBridgeServerTestAccessor::GetTrackedWorkerCount(*Server), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

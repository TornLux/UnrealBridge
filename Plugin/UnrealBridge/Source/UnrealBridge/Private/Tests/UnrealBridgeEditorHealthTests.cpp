#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UnrealBridgeEditorHealth.h"

namespace UnrealBridgeEditorHealthTests
{
	constexpr double StaleAfterSeconds = 2.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeEditorHealthInitialStateTest,
	"UnrealBridge.Server.EditorHealth.InitialStateIsStale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeEditorHealthInitialStateTest::RunTest(const FString& Parameters)
{
	double Now = 10.0;
	FUnrealBridgeEditorHealthCache Cache([&Now]() { return Now; });

	const FUnrealBridgeEditorHealthReadout Status = Cache.Read(
		UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestFalse(TEXT("未观察 Engine tick / Engine tick has not been observed"), Status.bEngineTickObserved);
	TestFalse(TEXT("未观察 Slate tick / Slate tick has not been observed"), Status.bSlateTickObserved);
	TestFalse(TEXT("主窗口尚未就绪 / Main frame is not ready"), Status.bReady);
	TestTrue(TEXT("无观测的状态为 stale / Missing observations are stale"), Status.bStale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeEditorHealthInjectedClockTest,
	"UnrealBridge.Server.EditorHealth.InjectedClockControlsAges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeEditorHealthInjectedClockTest::RunTest(const FString& Parameters)
{
	double Now = 100.0;
	FString NowUtc = TEXT("2026-08-08T00:00:00Z");
	FUnrealBridgeEditorHealthCache Cache(
		[&Now]() { return Now; },
		[&NowUtc]() { return NowUtc; });
	Cache.SetReady(true);
	Cache.RecordEngineTick();
	NowUtc = TEXT("2026-08-08T00:00:00.010Z");
	Cache.RecordSlateTick(FUnrealBridgeModalAttention());

	Now = 100.5;
	FUnrealBridgeEditorHealthReadout Status = Cache.Read(
		UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestEqual(TEXT("Engine tick 年龄 / Engine tick age"), Status.EngineTickAgeMs, 500.0);
	TestEqual(TEXT("Slate tick 年龄 / Slate tick age"), Status.SlateTickAgeMs, 500.0);
	TestEqual(TEXT("Engine tick 序号 / Engine tick sequence"), Status.EngineTickSequence, static_cast<uint64>(1));
	TestEqual(TEXT("Slate tick 序号 / Slate tick sequence"), Status.SlateTickSequence, static_cast<uint64>(1));
	TestEqual(TEXT("Engine UTC / Engine UTC"), Status.LastEngineTickUtc,
		FString(TEXT("2026-08-08T00:00:00Z")));
	TestEqual(TEXT("Slate UTC / Slate UTC"), Status.LastSlateTickUtc,
		FString(TEXT("2026-08-08T00:00:00.010Z")));
	TestEqual(TEXT("正常 UI 状态 / Normal UI state"), Status.UiState, FString(TEXT("normal")));
	TestFalse(TEXT("阈值内不 stale / Fresh inside threshold"), Status.bStale);

	Now = 102.001;
	Status = Cache.Read(UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestTrue(TEXT("超过阈值后 stale / Stale after threshold"), Status.bStale);
	TestTrue(TEXT("Engine 观测 stale / Engine observation is stale"), Status.bEngineStale);
	TestTrue(TEXT("Slate 观测 stale / Slate observation is stale"), Status.bSlateStale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeEditorHealthModalCopyTest,
	"UnrealBridge.Server.EditorHealth.ModalSummaryIsCopied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeEditorHealthModalCopyTest::RunTest(const FString& Parameters)
{
	double Now = 20.0;
	FString NowUtc = TEXT("2026-08-08T00:00:01Z");
	FUnrealBridgeEditorHealthCache Cache(
		[&Now]() { return Now; },
		[&NowUtc]() { return NowUtc; });
	FUnrealBridgeModalAttention Modal;
	Modal.bPresent = true;
	Modal.WindowGeneration = 9;
	Modal.SnapshotId = TEXT("0123456789abcdef");
	Modal.Title = TEXT("Message");
	Modal.ButtonCount = 2;
	Modal.InputCount = 1;
	Modal.CheckBoxCount = 3;
	Cache.RecordSlateTick(Modal);

	Modal.Title = TEXT("mutated after write");
	const FUnrealBridgeEditorHealthReadout Status = Cache.Read(
		UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestTrue(TEXT("需要注意 / Attention is required"), Status.Modal.bPresent);
	TestEqual(TEXT("快照 id / Snapshot id"), Status.Modal.SnapshotId, FString(TEXT("0123456789abcdef")));
	TestEqual(TEXT("标题按值复制 / Title copied by value"), Status.Modal.Title, FString(TEXT("Message")));
	TestEqual(TEXT("按钮数量 / Button count"), Status.Modal.ButtonCount, 2);
	TestEqual(TEXT("输入数量 / Input count"), Status.Modal.InputCount, 1);
	TestEqual(TEXT("复选框数量 / Checkbox count"), Status.Modal.CheckBoxCount, 3);
	TestEqual(TEXT("首次观测 UTC / First-seen UTC"), Status.ActiveModalFirstSeenUtc,
		FString(TEXT("2026-08-08T00:00:01Z")));
	TestEqual(TEXT("首次出现 attention id / First appearance attention id"), Status.AttentionId,
		static_cast<uint64>(1));
	TestEqual(TEXT("模态 UI 状态 / Modal UI state"), Status.UiState, FString(TEXT("slate_modal")));

	NowUtc = TEXT("2026-08-08T00:00:02Z");
	Modal.Title = TEXT("same window, updated title");
	Cache.RecordSlateTick(Modal);
	FUnrealBridgeEditorHealthReadout Updated = Cache.Read(
		UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestEqual(TEXT("同一窗口不递增 / Same window does not increment"), Updated.AttentionId,
		static_cast<uint64>(1));
	TestEqual(TEXT("同一窗口保留首次时间 / Same window preserves first seen"), Updated.ActiveModalFirstSeenUtc,
		FString(TEXT("2026-08-08T00:00:01Z")));

	NowUtc = TEXT("2026-08-08T00:00:03Z");
	Cache.RecordSlateTick(FUnrealBridgeModalAttention());
	Updated = Cache.Read(UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestEqual(TEXT("消失递增 / Disappearance increments"), Updated.AttentionId, static_cast<uint64>(2));
	TestTrue(TEXT("消失清空首次时间 / Disappearance clears first seen"), Updated.ActiveModalFirstSeenUtc.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeEditorHealthClockRollbackTest,
	"UnrealBridge.Server.EditorHealth.ClockRollbackClampsAge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeEditorHealthClockRollbackTest::RunTest(const FString& Parameters)
{
	double Now = 50.0;
	FUnrealBridgeEditorHealthCache Cache([&Now]() { return Now; });
	Cache.RecordEngineTick();
	Cache.RecordSlateTick(FUnrealBridgeModalAttention());

	Now = 49.0;
	const FUnrealBridgeEditorHealthReadout Status = Cache.Read(
		UnrealBridgeEditorHealthTests::StaleAfterSeconds);
	TestEqual(TEXT("Engine 年龄钳制为零 / Engine age clamps to zero"), Status.EngineTickAgeMs, 0.0);
	TestEqual(TEXT("Slate 年龄钳制为零 / Slate age clamps to zero"), Status.SlateTickAgeMs, 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

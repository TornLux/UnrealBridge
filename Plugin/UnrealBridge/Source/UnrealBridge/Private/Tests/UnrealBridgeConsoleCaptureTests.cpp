#include "UnrealBridgeConsoleCapture.h"
#include "Async/ParallelFor.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeConsoleCaptureConcurrencyTest,"UnrealBridge.Editor.ConsoleCaptureConcurrentWrites",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBridgeConsoleCaptureConcurrencyTest::RunTest(const FString&)
{
    BridgeEditorImpl::FCaptureDevice Capture;
    ParallelFor(32,[&](int32 Worker)
    {
        for(int32 Row=0;Row<64;++Row)
        {
            const FString Text=FString::Printf(TEXT("worker=%d row=%d"),Worker,Row);
            Capture.Serialize(*Text,ELogVerbosity::Display,NAME_None);
            Capture.Snapshot();
        }
    });
    TArray<FString> Rows;Capture.Snapshot().ParseIntoArrayLines(Rows);
    TSet<FString> Unique;for(const auto& Row:Rows)Unique.Add(Row);
    TestEqual(TEXT("Concurrent Exec/GLog-style writes retain every complete line"),Rows.Num(),32*64);
    TestEqual(TEXT("No duplicate/corrupt rows during concurrent append and snapshot"),Unique.Num(),32*64);
    return true;
}
#endif

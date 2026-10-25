#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"

namespace BridgeEditorImpl
{
    // Console output can arrive directly from Exec and from GLog's redirector thread at once.
    // 控制台输出可能同时来自 Exec 直写与 GLog 转发线程；完整字符串追加和读取必须互斥。
    class FCaptureDevice : public FOutputDevice
    {
        mutable FCriticalSection Mutex;
        FString Output;
    public:
        virtual void Serialize(const TCHAR* Text,ELogVerbosity::Type,const FName&) override
        {
            FScopeLock Lock(&Mutex);
            Output+=Text;Output+=TEXT("\n");
        }
        FString Snapshot() const { FScopeLock Lock(&Mutex);return Output; }
    };
}

#pragma once

#include "Containers/IndirectArray.h"
#include "CoreTypes.h"

class UWorld;
struct FWorldContext;

namespace BridgeAgentImpl
{
	/**
	 * 按 Editor 提供的上下文顺序返回首个已开始运行的 PIE 世界，供通用及权威操作使用。
	 * Returns the first begun-play PIE world in editor context order for general and authority-sensitive operations.
	 */
	UWorld* SelectFirstValidPIEWorld(const TIndirectArray<FWorldContext>& WorldContexts);

	/**
	 * 返回首个其首个 PlayerController 由 LocalPlayer 拥有的已运行 PIE 世界；本地客户端未就绪时显式返回空。
	 * Returns the first begun-play PIE world whose first PlayerController is owned by a LocalPlayer; returns null while the local client is not ready.
	 */
	UWorld* SelectFirstLocalPlayerPIEWorld(const TIndirectArray<FWorldContext>& WorldContexts);
}

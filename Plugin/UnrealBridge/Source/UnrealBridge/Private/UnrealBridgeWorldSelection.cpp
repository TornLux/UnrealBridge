#include "UnrealBridgeWorldSelection.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace BridgeAgentImpl
{
	namespace
	{
		/** 只接受已进入 BeginPlay 的 PIE 世界，避免 Editor/Preview 及启动中的半初始化世界。 */
		UWorld* GetBegunPlayPIEWorld(const FWorldContext& WorldContext)
		{
			UWorld* World = WorldContext.World();
			return WorldContext.WorldType == EWorldType::PIE
				&& World
				&& World->HasBegunPlay()
				? World
				: nullptr;
		}
	}

	UWorld* SelectFirstValidPIEWorld(const TIndirectArray<FWorldContext>& WorldContexts)
	{
		for (const FWorldContext& WorldContext : WorldContexts)
		{
			if (UWorld* World = GetBegunPlayPIEWorld(WorldContext))
			{
				return World;
			}
		}
		return nullptr;
	}

	UWorld* SelectFirstLocalPlayerPIEWorld(const TIndirectArray<FWorldContext>& WorldContexts)
	{
		for (const FWorldContext& WorldContext : WorldContexts)
		{
			UWorld* World = GetBegunPlayPIEWorld(WorldContext);
			if (!World)
			{
				continue;
			}

			// 本库的玩家/相机 API 全部使用 FirstPlayerController，因此选择条件必须精确匹配消费者。
			// Player/camera APIs in this library consume FirstPlayerController, so selection must match that exact predicate.
			const APlayerController* PlayerController = World->GetFirstPlayerController();
			if (PlayerController && PlayerController->GetLocalPlayer())
			{
				return World;
			}
		}
		return nullptr;
	}
}

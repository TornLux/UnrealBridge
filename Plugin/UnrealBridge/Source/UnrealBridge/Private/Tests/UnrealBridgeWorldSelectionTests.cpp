#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealBridgeWorldSelection.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Package.h"

namespace UnrealBridgeWorldSelectionTests
{
	/**
	 * 为选择器测试创建真实 UWorld、PlayerController 与 LocalPlayer，不把生产判定降级成布尔测试替身。
	 * Creates real UWorld, PlayerController, and LocalPlayer objects so selector tests do not replace production predicates with booleans.
	 */
	class FScopedPIEWorld
	{
	public:
		explicit FScopedPIEWorld(bool bHasBegunPlay, bool bAddPlayerController = false, bool bAttachLocalPlayer = false)
		{
			static int32 NextWorldIndex = 0;
			const FName WorldName(*FString::Printf(TEXT("UnrealBridgeWorldSelection_%d"), NextWorldIndex++));
			World = UWorld::CreateWorld(
				EWorldType::PIE,
				/*bInformEngineOfWorld=*/ false,
				WorldName,
				GetTransientPackage(),
				/*bAddToRoot=*/ false);
			if (!World)
			{
				return;
			}

			if (bAddPlayerController)
			{
				PlayerController = World->SpawnActor<APlayerController>();
				if (PlayerController)
				{
					// 未初始化的瞬态世界不会替测试 Actor 建立 controller list；显式注册以复现生产 GetFirstPlayerController 路径。
					// An uninitialized transient world does not build the controller list for test actors, so register explicitly to exercise the production GetFirstPlayerController path.
					World->AddController(PlayerController);
				}
				if (PlayerController && bAttachLocalPlayer && GEngine)
				{
					// ULocalPlayer 声明 Within=Engine；使用实际引擎作为 Outer，保持与生产构造约束一致。
					// ULocalPlayer declares Within=Engine, so use the live engine as its Outer to match production construction constraints.
					LocalPlayer = NewObject<ULocalPlayer>(GEngine, NAME_None, RF_Transient);
					PlayerController->SetPlayer(LocalPlayer);
				}
			}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
			World->bBegunPlay = bHasBegunPlay;
#else
			World->SetBegunPlay(bHasBegunPlay);
#endif
		}

		~FScopedPIEWorld()
		{
			if (PlayerController)
			{
				// SetPlayer 不接受空指针；先清除控制器侧测试引用，再销毁世界并回收 Engine-owned LocalPlayer。
				// SetPlayer rejects null, so clear the controller-side test reference before destroying the world and collecting the engine-owned LocalPlayer.
				PlayerController->Player = nullptr;
			}
			if (ReplacementPlayerController)
			{
				ReplacementPlayerController->Player = nullptr;
			}
			if (World)
			{
				if (PlayerController)
				{
					World->RemoveController(PlayerController);
				}
				if (ReplacementPlayerController)
				{
					World->RemoveController(ReplacementPlayerController);
				}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
				World->bBegunPlay = false;
#else
				World->SetBegunPlay(false);
#endif
				World->DestroyWorld(/*bInformEngineOfWorld=*/ false);
				World->MarkAsGarbage();
			}
			if (LocalPlayer)
			{
				LocalPlayer->MarkAsGarbage();
			}
			if (ReplacementLocalPlayer)
			{
				ReplacementLocalPlayer->MarkAsGarbage();
			}
		}

		UWorld* GetWorld() const { return World; }
		APlayerController* GetPlayerController() const { return PlayerController; }
		ULocalPlayer* GetLocalPlayer() const { return LocalPlayer; }

		ULocalPlayer* ReplaceLocalPlayer()
		{
			if (!PlayerController || !GEngine)
			{
				return nullptr;
			}
			ReplacementLocalPlayer = NewObject<ULocalPlayer>(GEngine, NAME_None, RF_Transient);
			PlayerController->SetPlayer(ReplacementLocalPlayer);
			return ReplacementLocalPlayer;
		}

		APlayerController* ReplacePlayerController()
		{
			if (!World || !GEngine)
			{
				return nullptr;
			}
			World->RemoveController(PlayerController);
			ReplacementPlayerController = World->SpawnActor<APlayerController>();
			if (!ReplacementPlayerController)
			{
				return nullptr;
			}
			World->AddController(ReplacementPlayerController);
			ReplacementLocalPlayer = NewObject<ULocalPlayer>(GEngine, NAME_None, RF_Transient);
			ReplacementPlayerController->SetPlayer(ReplacementLocalPlayer);
			return ReplacementPlayerController;
		}

	private:
		UWorld* World = nullptr;
		APlayerController* PlayerController = nullptr;
		ULocalPlayer* LocalPlayer = nullptr;
		APlayerController* ReplacementPlayerController = nullptr;
		ULocalPlayer* ReplacementLocalPlayer = nullptr;
	};

	void AddPIEContext(TIndirectArray<FWorldContext>& Contexts, UWorld* World)
	{
		FWorldContext* Context = new FWorldContext();
		Context->WorldType = EWorldType::PIE;
		Context->SetCurrentWorld(World);
		Contexts.Add(Context);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeGeneralWorldKeepsServerOrderTest,
	"UnrealBridge.Gameplay.WorldSelection.ProductionWiring.GeneralKeepsFirstValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeGeneralWorldKeepsServerOrderTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeWorldSelectionTests;
	(void)Parameters;

	FScopedPIEWorld ServerWorld(/*bHasBegunPlay=*/ true);
	FScopedPIEWorld LocalClientWorld(/*bHasBegunPlay=*/ true, /*bAddPlayerController=*/ true, /*bAttachLocalPlayer=*/ true);
	TestNotNull(TEXT("server world was created"), ServerWorld.GetWorld());
	TestNotNull(TEXT("client first controller has an actual LocalPlayer"),
		LocalClientWorld.GetPlayerController() ? LocalClientWorld.GetPlayerController()->GetLocalPlayer() : nullptr);

	TIndirectArray<FWorldContext> Contexts;
	AddPIEContext(Contexts, ServerWorld.GetWorld());
	AddPIEContext(Contexts, LocalClientWorld.GetWorld());

	TestEqual(TEXT("general and authority-sensitive calls preserve first-valid/server ordering"),
		BridgeAgentImpl::SelectFirstValidPIEWorld(Contexts), ServerWorld.GetWorld());
	TestEqual(TEXT("local player calls select the real local-client context"),
		BridgeAgentImpl::SelectFirstLocalPlayerPIEWorld(Contexts), LocalClientWorld.GetWorld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeLocalWorldStartupFailsClosedTest,
	"UnrealBridge.Gameplay.WorldSelection.ProductionWiring.ClientStartupFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeLocalWorldStartupFailsClosedTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeWorldSelectionTests;
	(void)Parameters;

	FScopedPIEWorld ServerWorld(/*bHasBegunPlay=*/ true);
	FScopedPIEWorld ClientWithoutLocalPlayer(/*bHasBegunPlay=*/ true, /*bAddPlayerController=*/ true);
	TestNotNull(TEXT("client has an actual first controller"), ClientWithoutLocalPlayer.GetPlayerController());
	TestNull(TEXT("client first controller is not locally owned during startup"),
		ClientWithoutLocalPlayer.GetPlayerController()
			? ClientWithoutLocalPlayer.GetPlayerController()->GetLocalPlayer()
			: nullptr);

	TIndirectArray<FWorldContext> Contexts;
	AddPIEContext(Contexts, ServerWorld.GetWorld());
	AddPIEContext(Contexts, ClientWithoutLocalPlayer.GetWorld());

	TestEqual(TEXT("general calls remain on the server during client startup"),
		BridgeAgentImpl::SelectFirstValidPIEWorld(Contexts), ServerWorld.GetWorld());
	TestNull(TEXT("local-only calls do not fall back to the server while the client is unready"),
		BridgeAgentImpl::SelectFirstLocalPlayerPIEWorld(Contexts));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeLocalWorldRequiresBegunPlayTest,
	"UnrealBridge.Gameplay.WorldSelection.ProductionWiring.RequiresBegunPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeLocalWorldRequiresBegunPlayTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeWorldSelectionTests;
	(void)Parameters;

	FScopedPIEWorld UnreadyLocalWorld(/*bHasBegunPlay=*/ false, /*bAddPlayerController=*/ true, /*bAttachLocalPlayer=*/ true);
	TestNotNull(TEXT("unready world still has a real local controller fixture"),
		UnreadyLocalWorld.GetPlayerController()
			? UnreadyLocalWorld.GetPlayerController()->GetLocalPlayer()
			: nullptr);

	TIndirectArray<FWorldContext> Contexts;
	AddPIEContext(Contexts, UnreadyLocalWorld.GetWorld());
	TestNull(TEXT("general selector rejects a PIE world before BeginPlay"),
		BridgeAgentImpl::SelectFirstValidPIEWorld(Contexts));
	TestNull(TEXT("local selector rejects a local controller before BeginPlay"),
		BridgeAgentImpl::SelectFirstLocalPlayerPIEWorld(Contexts));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeStandaloneLocalWorldTest,
	"UnrealBridge.Gameplay.WorldSelection.ProductionWiring.StandaloneLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeStandaloneLocalWorldTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeWorldSelectionTests;
	(void)Parameters;

	FScopedPIEWorld StandaloneWorld(/*bHasBegunPlay=*/ true, /*bAddPlayerController=*/ true, /*bAttachLocalPlayer=*/ true);
	TIndirectArray<FWorldContext> Contexts;
	AddPIEContext(Contexts, StandaloneWorld.GetWorld());

	TestEqual(TEXT("standalone with a real LocalPlayer is a valid local-player world"),
		BridgeAgentImpl::SelectFirstLocalPlayerPIEWorld(Contexts), StandaloneWorld.GetWorld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeServerOnlyHasNoLocalWorldTest,
	"UnrealBridge.Gameplay.WorldSelection.ProductionWiring.ServerOnlyHasNoLocalWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeServerOnlyHasNoLocalWorldTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeWorldSelectionTests;
	(void)Parameters;

	FScopedPIEWorld ServerWorld(/*bHasBegunPlay=*/ true, /*bAddPlayerController=*/ true);
	TIndirectArray<FWorldContext> Contexts;
	AddPIEContext(Contexts, ServerWorld.GetWorld());

	TestEqual(TEXT("server-only general operations retain the server world"),
		BridgeAgentImpl::SelectFirstValidPIEWorld(Contexts), ServerWorld.GetWorld());
	TestNull(TEXT("server-only sessions intentionally expose no local-player world"),
		BridgeAgentImpl::SelectFirstLocalPlayerPIEWorld(Contexts));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeStickyInputReplacementFailsClosedTest,
	"UnrealBridge.Gameplay.StickyInput.ProductionWiring.ReplacementFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeStickyInputReplacementFailsClosedTest::RunTest(const FString& Parameters)
{
	using namespace UnrealBridgeWorldSelectionTests;
	(void)Parameters;

	// 分别替换同一 PIE 世界中的 LocalPlayer 与 FirstPlayerController，确定旧粘滞身份不会被新对象继承。
	// Replace the LocalPlayer and FirstPlayerController in separate same-world fixtures so a stale sticky identity cannot transfer to either new object.
	FScopedPIEWorld LocalPlayerReplacementWorld(
		/*bHasBegunPlay=*/ true, /*bAddPlayerController=*/ true, /*bAttachLocalPlayer=*/ true);
	UWorld* FirstWorld = LocalPlayerReplacementWorld.GetWorld();
	APlayerController* FirstController = LocalPlayerReplacementWorld.GetPlayerController();
	ULocalPlayer* FirstLocalPlayer = LocalPlayerReplacementWorld.GetLocalPlayer();
	TestTrue(TEXT("captured local-player identity starts valid"),
		BridgeAgentImpl::IsSameLocalPlayerPIEIdentity(FirstWorld, FirstController, FirstLocalPlayer));
	TestNotNull(TEXT("replacement LocalPlayer was created"), LocalPlayerReplacementWorld.ReplaceLocalPlayer());
	TestFalse(TEXT("replacing LocalPlayer in the same world invalidates the captured sticky identity"),
		BridgeAgentImpl::IsSameLocalPlayerPIEIdentity(FirstWorld, FirstController, FirstLocalPlayer));

	FScopedPIEWorld ControllerReplacementWorld(
		/*bHasBegunPlay=*/ true, /*bAddPlayerController=*/ true, /*bAttachLocalPlayer=*/ true);
	UWorld* SecondWorld = ControllerReplacementWorld.GetWorld();
	APlayerController* SecondController = ControllerReplacementWorld.GetPlayerController();
	ULocalPlayer* SecondLocalPlayer = ControllerReplacementWorld.GetLocalPlayer();
	TestTrue(TEXT("second captured local-player identity starts valid"),
		BridgeAgentImpl::IsSameLocalPlayerPIEIdentity(SecondWorld, SecondController, SecondLocalPlayer));
	TestNotNull(TEXT("replacement FirstPlayerController was created"),
		ControllerReplacementWorld.ReplacePlayerController());
	TestFalse(TEXT("replacing FirstPlayerController in the same world invalidates the captured sticky identity"),
		BridgeAgentImpl::IsSameLocalPlayerPIEIdentity(SecondWorld, SecondController, SecondLocalPlayer));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

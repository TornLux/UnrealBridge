#include "UnrealBridgeModule.h"
#include "UnrealBridgeServer.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"

// Forward decls for the debug-hook registration that lives in the blueprint
// library's .cpp (keeps the delegate handle private while still letting the
// module control init / teardown lifetime).
namespace BridgeDebugState
{
	void Register();
	void Unregister();
}

#define LOCTEXT_NAMESPACE "FUnrealBridgeModule"

void FUnrealBridgeModule::StartupModule()
{
	// Subscribe to FBlueprintCoreDelegates::OnScriptException so breakpoint hits
	// are captured into the LastBreakpointHit snapshot table.
	BridgeDebugState::Register();

	Server = MakeShared<FUnrealBridgeServer>();

	// TODO: Read port from project settings / developer settings once UUnrealBridgeSettings is added
	constexpr int32 DefaultPort = 9876;

	if (Server->Start(DefaultPort))
	{
		UE_LOG(LogTemp, Log, TEXT("UnrealBridge: Server started on port %d"), DefaultPort);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealBridge: Failed to start server on port %d"), DefaultPort);
	}

	// Gate Python exec on main-frame creation to avoid racing SlateRHIRenderer::CreateViewport
	// during editor startup, which previously caused EXCEPTION_ACCESS_VIOLATION crashes when
	// clients hammered the bridge with ping/exec during boot.
	TWeakPtr<FUnrealBridgeServer> WeakServer = Server;
	auto OnMainFrameReady = [WeakServer](TSharedPtr<SWindow>, bool)
	{
		if (TSharedPtr<FUnrealBridgeServer> Pinned = WeakServer.Pin())
		{
			Pinned->SetEditorReady(true);
		}
	};

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (MainFrame.IsWindowInitialized())
	{
		// Already created (e.g. hot-reload) — flip immediately.
		Server->SetEditorReady(true);
	}
	else
	{
		MainFrame.OnMainFrameCreationFinished().AddLambda(OnMainFrameReady);
	}
}

void FUnrealBridgeModule::ShutdownModule()
{
	BridgeDebugState::Unregister();

	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
		UE_LOG(LogTemp, Log, TEXT("UnrealBridge: Server stopped."));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealBridgeModule, UnrealBridge)

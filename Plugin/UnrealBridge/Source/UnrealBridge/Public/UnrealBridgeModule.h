#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUnrealBridgeServer;
class FBridgeDiscoveryService;

class FUnrealBridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FUnrealBridgeServer, ESPMode::ThreadSafe> Server;
	TUniquePtr<FBridgeDiscoveryService> Discovery;
	FDelegateHandle MainFrameReadyHandle;
};

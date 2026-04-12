#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUnrealBridgeServer;

class FUnrealBridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FUnrealBridgeServer> Server;
};

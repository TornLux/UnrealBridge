#include "UnrealBridgeModule.h"
#include "UnrealBridgeServer.h"

#define LOCTEXT_NAMESPACE "FUnrealBridgeModule"

void FUnrealBridgeModule::StartupModule()
{
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
}

void FUnrealBridgeModule::ShutdownModule()
{
	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
		UE_LOG(LogTemp, Log, TEXT("UnrealBridge: Server stopped."));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealBridgeModule, UnrealBridge)

#include "UnrealBridgeModule.h"
#include "UnrealBridgeDiscovery.h"
#include "UnrealBridgeServer.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

// Forward decls for the debug-hook registration that lives in the blueprint
// library's .cpp (keeps the delegate handle private while still letting the
// module control init / teardown lifetime).
namespace BridgeDebugState
{
	void Register();
	void Unregister();
}

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeModule, Log, All);

namespace
{
	/**
	 * Layered config resolution: CLI > env > EditorPerProjectUserSettings.ini > hardcoded default.
	 * Returns `Out` on success, leaves it untouched on miss.
	 */
	void ResolveStringConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, FString& Out)
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), CliKey, Value) && !Value.IsEmpty())
		{
			Out = Value;
			return;
		}
		Value = FPlatformMisc::GetEnvironmentVariable(EnvKey);
		if (!Value.IsEmpty())
		{
			Out = Value;
			return;
		}
		if (GConfig)
		{
			FString Tmp;
			if (GConfig->GetString(TEXT("UnrealBridge"), IniKey, Tmp, GEditorPerProjectIni) && !Tmp.IsEmpty())
			{
				Out = Tmp;
				return;
			}
		}
	}

	void ResolveIntConfig(const TCHAR* CliKey, const TCHAR* EnvKey, const TCHAR* IniKey, int32& Out)
	{
		int32 Value = 0;
		if (FParse::Value(FCommandLine::Get(), CliKey, Value))
		{
			Out = Value;
			return;
		}
		const FString EnvStr = FPlatformMisc::GetEnvironmentVariable(EnvKey);
		if (!EnvStr.IsEmpty() && EnvStr.IsNumeric())
		{
			Out = FCString::Atoi(*EnvStr);
			return;
		}
		if (GConfig)
		{
			int32 Tmp = 0;
			if (GConfig->GetInt(TEXT("UnrealBridge"), IniKey, Tmp, GEditorPerProjectIni))
			{
				Out = Tmp;
				return;
			}
		}
	}

	/** sha1(token) first 8 bytes, lowercase hex. Empty input → empty output.
	 *  Not cryptographic — only used so clients can verify "I'm talking to the
	 *  editor that holds this token" without leaking the token itself. */
	FString TokenFingerprint(const FString& Token)
	{
		if (Token.IsEmpty())
		{
			return FString();
		}
		const FTCHARToUTF8 Utf8(*Token);
		FSHA1 Hasher;
		Hasher.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Hasher.Final();
		uint8 Digest[20];
		Hasher.GetHash(Digest);
		return BytesToHex(Digest, 8).ToLower();
	}

	/**
	 * Find the editor's .uproject path for the running session. Used by the
	 * discovery responder so clients can match by path as well as by name.
	 */
	FString ResolveProjectPath()
	{
		FString Path = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("%s.uproject"), FApp::GetProjectName())));
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Path;
	}
}

void FUnrealBridgeModule::StartupModule()
{
	BridgeDebugState::Register();

	// Map /Plugin/UnrealBridge/ -> this plugin's Shaders/ dir so UMaterialExpressionCustom
	// nodes can #include "/Plugin/UnrealBridge/BridgeSnippets.ush" and friends.
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealBridge"));
		if (Plugin.IsValid())
		{
			const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"), TEXT("Private"));
			if (FPaths::DirectoryExists(ShaderDir))
			{
				AddShaderSourceDirectoryMapping(TEXT("/Plugin/UnrealBridge"), ShaderDir);
				UE_LOG(LogUnrealBridgeModule, Log,
					TEXT("registered shader dir '%s' under /Plugin/UnrealBridge"), *ShaderDir);
			}
		}
	}

	// ---- resolve config (CLI > env > ini > default) ---------------------
	FString BindStr = TEXT("127.0.0.1");
	int32 Port = 0;                   // 0 = OS-assigned ephemeral (discovery tells clients)
	FString Token;
	FString DiscoveryGroup = TEXT("239.255.42.99:9876");
	int32 DiscoveryEnabled = 1;

	ResolveStringConfig(TEXT("UnrealBridgeBind="), TEXT("UNREAL_BRIDGE_BIND"), TEXT("Bind"), BindStr);
	ResolveIntConfig(TEXT("UnrealBridgePort="), TEXT("UNREAL_BRIDGE_PORT"), TEXT("Port"), Port);
	ResolveStringConfig(TEXT("UnrealBridgeToken="), TEXT("UNREAL_BRIDGE_TOKEN"), TEXT("Token"), Token);
	ResolveStringConfig(TEXT("UnrealBridgeDiscoveryGroup="), TEXT("UNREAL_BRIDGE_DISCOVERY_GROUP"),
		TEXT("DiscoveryGroup"), DiscoveryGroup);
	ResolveIntConfig(TEXT("UnrealBridgeDiscoveryEnabled="), TEXT("UNREAL_BRIDGE_DISCOVERY"),
		TEXT("DiscoveryEnabled"), DiscoveryEnabled);

	// Accept `-UnrealBridgeNoDiscovery` as a convenient shorthand toggle.
	if (FParse::Param(FCommandLine::Get(), TEXT("UnrealBridgeNoDiscovery")))
	{
		DiscoveryEnabled = 0;
	}

	// ---- parse bind + discovery group ---------------------------------
	FIPv4Address BindAddress = FIPv4Address(127, 0, 0, 1);
	if (!FIPv4Address::Parse(BindStr, BindAddress))
	{
		UE_LOG(LogUnrealBridgeModule, Warning,
			TEXT("invalid -UnrealBridgeBind='%s' — falling back to 127.0.0.1"), *BindStr);
		BindAddress = FIPv4Address(127, 0, 0, 1);
		BindStr = TEXT("127.0.0.1");
	}

	FIPv4Endpoint DiscoveryEndpoint(FIPv4Address(239, 255, 42, 99), 9876);
	if (!FIPv4Endpoint::Parse(DiscoveryGroup, DiscoveryEndpoint))
	{
		UE_LOG(LogUnrealBridgeModule, Warning,
			TEXT("invalid -UnrealBridgeDiscoveryGroup='%s' — using 239.255.42.99:9876"), *DiscoveryGroup);
		DiscoveryEndpoint = FIPv4Endpoint(FIPv4Address(239, 255, 42, 99), 9876);
	}

	// ---- start the TCP server -----------------------------------------
	Server = MakeShared<FUnrealBridgeServer>();
	FUnrealBridgeServer::FStartConfig StartCfg;
	StartCfg.BindAddress = BindAddress;
	StartCfg.Port = Port;
	StartCfg.Token = Token;

	if (!Server->Start(StartCfg))
	{
		UE_LOG(LogUnrealBridgeModule, Error,
			TEXT("server failed to start (bind=%s port=%d) — bridge disabled this session"),
			*BindStr, Port);
		Server.Reset();
		return;
	}

	UE_LOG(LogUnrealBridgeModule, Log, TEXT("Server up on %s:%d%s"),
		*Server->GetBoundAddress(), Server->GetBoundPort(),
		Server->HasToken() ? TEXT(" (token enforced)") : TEXT(""));

	if (Server->HasToken())
	{
		// Write the token to a predictable place so the client can consume it
		// without copy-pasting from the log. 0 on the mode because UE's
		// FFileHelper doesn't expose ACL control — the Saved/ folder is
		// already user-scoped.
		const FString TokenPath = FPaths::Combine(FPaths::ProjectSavedDir(),
			TEXT("UnrealBridge"), TEXT("token.txt"));
		FFileHelper::SaveStringToFile(Token, *TokenPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("token written to %s"), *TokenPath);
	}

	// ---- start the discovery responder --------------------------------
	if (DiscoveryEnabled != 0)
	{
		FBridgeDiscoveryService::FConfig DiscCfg;
		DiscCfg.GroupAddress = DiscoveryEndpoint.Address;
		DiscCfg.GroupPort = DiscoveryEndpoint.Port;
		DiscCfg.TcpBindAddress = Server->GetBoundAddress();
		DiscCfg.TcpPort = Server->GetBoundPort();
		DiscCfg.ProjectName = FApp::GetProjectName();
		DiscCfg.ProjectPath = ResolveProjectPath();
		DiscCfg.EngineVersion = FEngineVersion::Current().ToString();
		DiscCfg.TokenFingerprint = TokenFingerprint(Token);

		Discovery = MakeUnique<FBridgeDiscoveryService>(DiscCfg);
		if (!Discovery->StartService())
		{
			UE_LOG(LogUnrealBridgeModule, Warning,
				TEXT("discovery failed to start — clients will need an explicit --endpoint=%s:%d"),
				*Server->GetBoundAddress(), Server->GetBoundPort());
			Discovery.Reset();
		}
	}
	else
	{
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("discovery disabled (opt-out) — clients need --endpoint="));
	}

	// ---- editor-ready gate --------------------------------------------
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

	if (Discovery.IsValid())
	{
		Discovery->StopService();
		Discovery.Reset();
	}

	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
		UE_LOG(LogUnrealBridgeModule, Log, TEXT("Server stopped."));
	}
}

IMPLEMENT_MODULE(FUnrealBridgeModule, UnrealBridge)

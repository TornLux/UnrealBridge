#include "UnrealBridgeDiscovery.h"

#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealBridgeDiscovery, Log, All);

namespace
{
	constexpr int32 MaxDatagramBytes = 64 * 1024;

	bool MatchProjectFilter(const FString& Filter, const FString& ProjectName, const FString& ProjectPath)
	{
		if (Filter.IsEmpty() || Filter == TEXT("*"))
		{
			return true;
		}

		if (ProjectName.Equals(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (!ProjectPath.IsEmpty() && ProjectPath.Equals(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		// Allow path-suffix matches so users can give the directory containing
		// the .uproject instead of the full asset path.
		if (!ProjectPath.IsEmpty())
		{
			FString NormalizedProject = ProjectPath;
			FString NormalizedFilter = Filter;
			NormalizedProject.ReplaceInline(TEXT("\\"), TEXT("/"));
			NormalizedFilter.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (NormalizedProject.EndsWith(NormalizedFilter, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		// Allow project-name substring for casual use ("my" matches "MyGame").
		if (ProjectName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}

		return false;
	}
}


FBridgeDiscoveryService::FBridgeDiscoveryService(const FConfig& InConfig)
	: Config(InConfig)
{
	CurrentTcpPort.Set(InConfig.TcpPort);
}

FBridgeDiscoveryService::~FBridgeDiscoveryService()
{
	StopService();
}

bool FBridgeDiscoveryService::StartService()
{
	if (bIsRunning)
	{
		return true;
	}

	// Bind to 0.0.0.0:GroupPort, join the multicast group, enable loopback so
	// localhost probes work on this same machine.
	Socket = FUdpSocketBuilder(TEXT("UnrealBridgeDiscovery"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(Config.GroupPort)
		.JoinedToGroup(Config.GroupAddress)
		.WithMulticastLoopback()
		.WithMulticastTtl(1)
		.Build();

	if (Socket == nullptr)
	{
		UE_LOG(LogUnrealBridgeDiscovery, Warning,
			TEXT("Failed to bind UDP discovery socket on %s:%d — discovery disabled for this instance"),
			*Config.GroupAddress.ToString(), Config.GroupPort);
		return false;
	}

	bStopRequested = false;
	Thread.Reset(FRunnableThread::Create(this, TEXT("UnrealBridgeDiscovery"), 0,
		TPri_BelowNormal));

	if (!Thread.IsValid())
	{
		UE_LOG(LogUnrealBridgeDiscovery, Warning, TEXT("Failed to create discovery thread"));
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
		return false;
	}

	bIsRunning = true;
	UE_LOG(LogUnrealBridgeDiscovery, Log,
		TEXT("Discovery listening on multicast %s:%d (tcp advertised: %s:%d, project='%s')"),
		*Config.GroupAddress.ToString(), Config.GroupPort,
		*Config.TcpBindAddress, CurrentTcpPort.GetValue(), *Config.ProjectName);
	return true;
}

void FBridgeDiscoveryService::StopService()
{
	if (!bIsRunning && !Thread.IsValid() && Socket == nullptr)
	{
		return;
	}

	bStopRequested = true;

	if (Thread.IsValid())
	{
		Thread->Kill(/*bShouldWait=*/true);
		Thread.Reset();
	}

	if (Socket != nullptr)
	{
		ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (Subsystem != nullptr)
		{
			Subsystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}

	bIsRunning = false;
}

void FBridgeDiscoveryService::SetTcpPort(int32 NewPort)
{
	CurrentTcpPort.Set(NewPort);
}

bool FBridgeDiscoveryService::Init()
{
	return Socket != nullptr;
}

uint32 FBridgeDiscoveryService::Run()
{
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (Subsystem == nullptr || Socket == nullptr)
	{
		return 1;
	}

	TSharedRef<FInternetAddr> SourceAddr = Subsystem->CreateInternetAddr();
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(MaxDatagramBytes);

	while (!bStopRequested)
	{
		uint32 PendingSize = 0;
		bool bHasData = Socket->HasPendingData(PendingSize);
		if (bHasData && PendingSize > 0)
		{
			const int32 ToRead = FMath::Min<int32>(PendingSize, Buffer.Num());
			int32 BytesRead = 0;
			if (Socket->RecvFrom(Buffer.GetData(), ToRead, BytesRead, *SourceAddr) && BytesRead > 0)
			{
				HandleDatagram(Buffer.GetData(), BytesRead, *SourceAddr);
			}
		}
		else
		{
			// 100ms poll — imperceptible discovery latency, negligible CPU.
			FPlatformProcess::Sleep(0.1f);
		}
	}

	return 0;
}

void FBridgeDiscoveryService::Stop()
{
	bStopRequested = true;
}

void FBridgeDiscoveryService::Exit()
{
}

void FBridgeDiscoveryService::HandleDatagram(const uint8* Bytes, int32 Length, const FInternetAddr& Source)
{
	if (Length <= 0)
	{
		return;
	}

	// UE's TCHAR JSON parser wants TCHAR-encoded text; UDP payloads are UTF-8
	// on the wire so we convert here.
	const FString Payload = FString(Length, UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes)));

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	double Version = 0.0;
	if (!Root->TryGetNumberField(TEXT("v"), Version) || FMath::RoundToInt(Version) != 1)
	{
		return;
	}

	FString Type;
	if (!Root->TryGetStringField(TEXT("type"), Type) || Type != TEXT("probe"))
	{
		return;
	}

	FString RequestId;
	if (!Root->TryGetStringField(TEXT("request_id"), RequestId) || RequestId.IsEmpty())
	{
		return;
	}

	FString Filter = TEXT("*");
	const TSharedPtr<FJsonObject>* FilterObj = nullptr;
	if (Root->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj && FilterObj->IsValid())
	{
		FString ProjectFilter;
		if ((*FilterObj)->TryGetStringField(TEXT("project"), ProjectFilter) && !ProjectFilter.IsEmpty())
		{
			Filter = ProjectFilter;
		}
	}

	if (!FilterMatchesUs(Filter))
	{
		return;
	}

	const FString ResponseJson = BuildResponseJson(RequestId);
	const FTCHARToUTF8 Utf8(*ResponseJson);
	const int32 NumBytes = Utf8.Length();

	int32 BytesSent = 0;
	if (!Socket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), NumBytes, BytesSent, Source))
	{
		UE_LOG(LogUnrealBridgeDiscovery, Verbose, TEXT("SendTo(response) failed"));
	}
}

bool FBridgeDiscoveryService::FilterMatchesUs(const FString& Filter) const
{
	return MatchProjectFilter(Filter, Config.ProjectName, Config.ProjectPath);
}

FString FBridgeDiscoveryService::BuildResponseJson(const FString& RequestId) const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("v"), 1);
	Root->SetStringField(TEXT("type"), TEXT("response"));
	Root->SetStringField(TEXT("request_id"), RequestId);
	Root->SetNumberField(TEXT("pid"), (int32)FPlatformProcess::GetCurrentProcessId());
	Root->SetStringField(TEXT("project"), Config.ProjectName);
	Root->SetStringField(TEXT("project_path"), Config.ProjectPath);
	Root->SetStringField(TEXT("engine_version"), Config.EngineVersion);
	Root->SetStringField(TEXT("tcp_bind"), Config.TcpBindAddress);
	Root->SetNumberField(TEXT("tcp_port"), CurrentTcpPort.GetValue());
	Root->SetStringField(TEXT("token_fingerprint"), Config.TokenFingerprint);

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

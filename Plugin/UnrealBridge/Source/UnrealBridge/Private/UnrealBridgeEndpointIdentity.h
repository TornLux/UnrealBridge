#pragma once

#include "CoreMinimal.h"
#include "UnrealBridgeProtocol.h"

class FJsonObject;

/**
 * 端点身份是一次 Server 启动的不可复用事实源；discovery、TCP 前置条件与响应必须共享它。
 * Endpoint identity is the non-reusable source of truth for one Server start; discovery, TCP preconditions, and responses share it.
 */
struct FUnrealBridgeEndpointIdentity
{
	static constexpr int32 ProtocolVersion = UnrealBridgeProtocol::Version;

	FString InstanceId;
	FString ProjectPath;
	int32 ProcessId = 0;

	/** 为当前进程的一次 Server 启动创建新身份。 / Create a fresh identity for one Server start in this process. */
	static FUnrealBridgeEndpointIdentity Create();

	/** 将权威身份写入所有 TCP 响应。 / Append the authoritative identity to every TCP response. */
	void AppendToResponse(const TSharedRef<FJsonObject>& Response) const;

	/**
	 * 验证 expected identity；生产 dispatcher 保证在任何 command body 或 work admission 前调用。
	 * Validate expected identity; the production dispatcher calls it before any command body or work admission.
	 */
	bool ValidateRequest(const TSharedPtr<FJsonObject>& Request, FString& OutErrorCode, FString& OutError) const;

	/** 只生成 Server 的 wire-canonical 启动路径；客户端必须逐字复制结果。 / Generate the Server's wire-canonical startup path; clients must copy the result verbatim. */
	static FString NormalizeProjectPath(FString Path);
};

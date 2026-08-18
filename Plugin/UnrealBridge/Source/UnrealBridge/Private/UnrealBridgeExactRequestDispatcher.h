#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class FJsonObject;
struct FUnrealBridgeEndpointIdentity;

/**
 * exact wire 可进入的六种命令；未知别名不映射为 exec。
 * Six commands admitted by the exact wire; unknown aliases never map to exec.
 */
enum class EUnrealBridgeExactCommand : uint8
{
	Exec,
	Ping,
	GameThreadPing,
	DebugResume,
	ModalStatus,
	ModalAction,
};

/**
 * 已通过 wire、identity、payload 与命令映射验证的请求。
 * Request that passed wire, identity, payload, and command mapping validation.
 */
struct FUnrealBridgeAcceptedRequest
{
	EUnrealBridgeExactCommand Command = EUnrealBridgeExactCommand::Exec;
	TSharedPtr<FJsonObject> Payload;
};

/**
 * 生产请求的唯一前置调度门；只有全部前置条件通过才调用 admission callback。
 * Sole pre-dispatch gate for production requests; invokes the admission callback only after every precondition passes.
 */
class FUnrealBridgeExactRequestDispatcher final
{
public:
	/**
	 * 验证并准入一个 exact request；拒绝时 callback 保证不执行。
	 * Validate and admit one exact request; the callback is guaranteed not to run on rejection.
	 */
	static bool TryDispatch(
		const TSharedPtr<FJsonObject>& WireRequest,
		const FUnrealBridgeEndpointIdentity& Identity,
		TFunctionRef<void(const FUnrealBridgeAcceptedRequest&)> OnAccepted,
		FString& OutErrorCode,
		FString& OutError);
};

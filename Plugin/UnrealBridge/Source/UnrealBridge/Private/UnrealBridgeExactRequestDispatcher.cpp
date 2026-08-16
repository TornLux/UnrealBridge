#include "UnrealBridgeExactRequestDispatcher.h"

#include "Dom/JsonObject.h"
#include "UnrealBridgeEndpointIdentity.h"
#include "UnrealBridgeProtocol.h"

namespace
{
	bool TryMapCommand(const FString& WireCommand, EUnrealBridgeExactCommand& OutCommand)
	{
		if (WireCommand == UnrealBridgeProtocol::ExactExec)
		{
			OutCommand = EUnrealBridgeExactCommand::Exec;
		}
		else if (WireCommand == UnrealBridgeProtocol::ExactPing)
		{
			OutCommand = EUnrealBridgeExactCommand::Ping;
		}
		else if (WireCommand == UnrealBridgeProtocol::ExactGameThreadPing)
		{
			OutCommand = EUnrealBridgeExactCommand::GameThreadPing;
		}
		else if (WireCommand == UnrealBridgeProtocol::ExactDebugResume)
		{
			OutCommand = EUnrealBridgeExactCommand::DebugResume;
		}
		else if (WireCommand == UnrealBridgeProtocol::ExactModalStatus)
		{
			OutCommand = EUnrealBridgeExactCommand::ModalStatus;
		}
		else if (WireCommand == UnrealBridgeProtocol::ExactModalAction)
		{
			OutCommand = EUnrealBridgeExactCommand::ModalAction;
		}
		else
		{
			return false;
		}
		return true;
	}
}

bool FUnrealBridgeExactRequestDispatcher::TryDispatch(
	const TSharedPtr<FJsonObject>& WireRequest,
	const FUnrealBridgeEndpointIdentity& Identity,
	TFunctionRef<void(const FUnrealBridgeAcceptedRequest&)> OnAccepted,
	FString& OutErrorCode,
	FString& OutError)
{
	OutErrorCode.Reset();
	OutError.Reset();

	FString WireCommand;
	if (!WireRequest.IsValid()
		|| !WireRequest->TryGetStringField(UnrealBridgeProtocol::CommandField, WireCommand)
		|| !WireCommand.StartsWith(UnrealBridgeProtocol::ExactPrefix, ESearchCase::CaseSensitive))
	{
		OutErrorCode = TEXT("exact_command_required");
		OutError = TEXT("legacy or unknown wire form rejected; use an exact_* command with expected identity");
		return false;
	}

	if (!Identity.ValidateRequest(WireRequest, OutErrorCode, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	if (!WireRequest->TryGetObjectField(UnrealBridgeProtocol::RequestField, PayloadPtr)
		|| PayloadPtr == nullptr
		|| !PayloadPtr->IsValid())
	{
		OutErrorCode = TEXT("invalid_request");
		OutError = TEXT("missing or non-object field 'request'");
		return false;
	}

	EUnrealBridgeExactCommand Command = EUnrealBridgeExactCommand::Exec;
	if (!TryMapCommand(WireCommand, Command))
	{
		OutErrorCode = TEXT("unsupported_command");
		OutError = FString::Printf(TEXT("unsupported exact command '%s'"), *WireCommand);
		return false;
	}

	FUnrealBridgeAcceptedRequest Accepted;
	Accepted.Command = Command;
	Accepted.Payload = *PayloadPtr;
	OnAccepted(Accepted);
	return true;
}

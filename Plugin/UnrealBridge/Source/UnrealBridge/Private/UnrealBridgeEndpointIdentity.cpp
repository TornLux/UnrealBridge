#include "UnrealBridgeEndpointIdentity.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
	/** JSON number 必须是有限、正数、int32 范围内的整数，并精确等于权威值。 / JSON numbers must be finite positive int32 integers exactly equal to the authority. */
	bool IsExactPositiveInt32(double Value, int32 Authority)
	{
		return Authority > 0
			&& FMath::IsFinite(Value)
			&& Value >= 1.0
			&& Value <= static_cast<double>(TNumericLimits<int32>::Max())
			&& Value == static_cast<double>(static_cast<int32>(Value))
			&& static_cast<int32>(Value) == Authority;
	}
}

FUnrealBridgeEndpointIdentity FUnrealBridgeEndpointIdentity::Create()
{
	FUnrealBridgeEndpointIdentity Identity;
	Identity.InstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Identity.ProcessId = static_cast<int32>(FPlatformProcess::GetCurrentProcessId());
	Identity.ProjectPath = NormalizeProjectPath(FPaths::Combine(
		FPaths::ProjectDir(), FString::Printf(TEXT("%s.uproject"), FApp::GetProjectName())));
	return Identity;
}

FString FUnrealBridgeEndpointIdentity::NormalizeProjectPath(FString Path)
{
	Path = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Path);
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

void FUnrealBridgeEndpointIdentity::AppendToResponse(const TSharedRef<FJsonObject>& Response) const
{
	Response->SetNumberField(UnrealBridgeProtocol::ProtocolVersionField, ProtocolVersion);
	Response->SetStringField(UnrealBridgeProtocol::InstanceIdField, InstanceId);
	Response->SetNumberField(UnrealBridgeProtocol::ProcessIdField, ProcessId);
	Response->SetStringField(UnrealBridgeProtocol::ProjectPathField, ProjectPath);
}

bool FUnrealBridgeEndpointIdentity::ValidateRequest(
	const TSharedPtr<FJsonObject>& Request,
	FString& OutErrorCode,
	FString& OutError) const
{
	const TSharedPtr<FJsonObject>* ExpectedPtr = nullptr;
	if (!Request.IsValid()
		|| !Request->TryGetObjectField(UnrealBridgeProtocol::ExpectedField, ExpectedPtr)
		|| ExpectedPtr == nullptr
		|| !ExpectedPtr->IsValid())
	{
		OutErrorCode = TEXT("identity_required");
		OutError = TEXT("missing object field 'expected'; exact endpoint identity is required");
		return false;
	}

	const TSharedPtr<FJsonObject>& Expected = *ExpectedPtr;

	// UE 的 typed getter 会转换某些 JSON 类型；identity precondition 必须先验证原始 wire 类型再读取值。
	// UE typed getters coerce some JSON types; identity preconditions must validate the raw wire type before reading values.
	const TSharedPtr<FJsonValue>* ProtocolValue = Expected->Values.Find(UnrealBridgeProtocol::ProtocolVersionField);
	if (ProtocolValue == nullptr
		|| !ProtocolValue->IsValid()
		|| (*ProtocolValue)->Type != EJson::Number
		|| !IsExactPositiveInt32((*ProtocolValue)->AsNumber(), ProtocolVersion))
	{
		OutErrorCode = TEXT("protocol_mismatch");
		OutError = FString::Printf(TEXT("expected protocol_version %d as a JSON number"), ProtocolVersion);
		return false;
	}

	const TSharedPtr<FJsonValue>* InstanceValue = Expected->Values.Find(UnrealBridgeProtocol::InstanceIdField);
	if (InstanceValue == nullptr
		|| !InstanceValue->IsValid()
		|| (*InstanceValue)->Type != EJson::String
		|| (*InstanceValue)->AsString().IsEmpty())
	{
		OutErrorCode = TEXT("identity_required");
		OutError = TEXT("missing or non-string expected.instance_id");
		return false;
	}
	const FString ExpectedInstance = (*InstanceValue)->AsString();
	if (!ExpectedInstance.Equals(InstanceId, ESearchCase::CaseSensitive))
	{
		OutErrorCode = TEXT("instance_mismatch");
		OutError = TEXT("expected instance_id does not exactly match this Server start");
		return false;
	}

	const TSharedPtr<FJsonValue>* PidValue = Expected->Values.Find(UnrealBridgeProtocol::ProcessIdField);
	if (PidValue == nullptr
		|| !PidValue->IsValid()
		|| (*PidValue)->Type != EJson::Number
		|| !IsExactPositiveInt32((*PidValue)->AsNumber(), ProcessId))
	{
		OutErrorCode = TEXT("pid_mismatch");
		OutError = TEXT("expected pid must be an exact JSON integer matching this Server process");
		return false;
	}

	const TSharedPtr<FJsonValue>* ProjectPathValue = Expected->Values.Find(UnrealBridgeProtocol::ProjectPathField);
	if (ProjectPathValue == nullptr
		|| !ProjectPathValue->IsValid()
		|| (*ProjectPathValue)->Type != EJson::String
		|| (*ProjectPathValue)->AsString().IsEmpty())
	{
		OutErrorCode = TEXT("identity_required");
		OutError = TEXT("missing or non-string expected.project_path");
		return false;
	}
	const FString ExpectedProjectPath = (*ProjectPathValue)->AsString();
	// project_path 是 discovery/startup 输出的 wire identity；所有平台都必须逐字符、区分大小写匹配。
	// project_path is the discovery/startup wire identity; every platform requires character-exact, case-sensitive equality.
	if (!ExpectedProjectPath.Equals(ProjectPath, ESearchCase::CaseSensitive))
	{
		OutErrorCode = TEXT("project_mismatch");
		OutError = TEXT("expected project_path must exactly match the discovery or startup value");
		return false;
	}

	return true;
}

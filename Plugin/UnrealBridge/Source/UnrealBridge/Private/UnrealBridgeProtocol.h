#pragma once

#include "CoreMinimal.h"

/**
 * protocol v2 的字段值与命令名事实源；Server、discovery 与契约测试共同引用。
 * Source of truth for protocol-v2 field values and command names shared by Server, discovery, and contract tests.
 */
namespace UnrealBridgeProtocol
{
	constexpr int32 Version = 2;
	constexpr const TCHAR* CommandField = TEXT("command");
	constexpr const TCHAR* ExpectedField = TEXT("expected");
	constexpr const TCHAR* RequestField = TEXT("request");
	constexpr const TCHAR* ProtocolVersionField = TEXT("protocol_version");
	constexpr const TCHAR* InstanceIdField = TEXT("instance_id");
	constexpr const TCHAR* ProcessIdField = TEXT("pid");
	constexpr const TCHAR* ProjectPathField = TEXT("project_path");

	constexpr const TCHAR* ExactPrefix = TEXT("exact_");
	constexpr const TCHAR* ExactExec = TEXT("exact_exec");
	constexpr const TCHAR* ExactPing = TEXT("exact_ping");
	constexpr const TCHAR* ExactGameThreadPing = TEXT("exact_gamethread_ping");
	constexpr const TCHAR* ExactDebugResume = TEXT("exact_debug_resume");
	constexpr const TCHAR* ExactModalStatus = TEXT("exact_modal_status");
	constexpr const TCHAR* ExactModalAction = TEXT("exact_modal_action");

	constexpr const TCHAR* ExactCapabilities[] = {
		ExactExec,
		ExactPing,
		ExactGameThreadPing,
		ExactDebugResume,
		ExactModalStatus,
		ExactModalAction,
	};
}

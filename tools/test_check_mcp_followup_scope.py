#!/usr/bin/env python3
"""Unit-style checks for check_mcp_followup_scope.py."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / "tools" / "check_mcp_followup_scope.py"


def _load_checker():
    spec = importlib.util.spec_from_file_location("check_mcp_followup_scope_for_tests", CHECKER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _assert_failure_contains(failures: list[str], expected: str) -> None:
    joined = "\n".join(failures)
    if expected not in joined:
        raise AssertionError(f"expected {expected!r} in failures, got {joined!r}")


def main() -> int:
    checker = _load_checker()

    allowed = [
        ".claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py",
        "docs/plans/mcp-pagination-followups.md",
        "docs/plans/upstream-pr-description.md",
        "docs/reports/2026-05-29_1156_pagination-followup-branch-status.md",
        "tools/check_mcp_followup_scope.py",
        "tools/test_smoke_mcp_all.py",
        "tools/test_smoke_mcp_pagination.py",
        "tools/test_smoke_mcp_stdio.py",
    ]
    assert checker.check_scope(allowed) == []

    combined_allowed = [
        ".claude/skills/unreal-bridge/scripts/bridge.py",
        "Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeServer.cpp",
        "Plugin/UnrealBridge/UnrealBridge.uplugin",
        "tools/smoke_output_spill.py",
    ]
    assert checker.check_scope(combined_allowed, mode="combined") == []

    plugin_failures = checker.check_scope(
        ["Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeServer.cpp"]
    )
    _assert_failure_contains(plugin_failures, "forbidden Unreal plugin/source paths changed")

    unknown_failures = checker.check_scope(["tools/unrelated_helper.py"])
    _assert_failure_contains(unknown_failures, "unexpected files outside MCP follow-up scope")

    combined_unknown_failures = checker.check_scope(["Plugin/UnrealBridge/Content/Unexpected.uasset"], mode="combined")
    _assert_failure_contains(combined_unknown_failures, "unexpected files outside MCP follow-up scope")

    mixed_failures = checker.check_scope(
        [
            "docs/reports/followup.md",
            "Plugin/UnrealBridge/UnrealBridge.uplugin",
            "scripts/random.py",
        ]
    )
    _assert_failure_contains(mixed_failures, "forbidden Unreal plugin/source paths changed")
    _assert_failure_contains(mixed_failures, "unexpected files outside MCP follow-up scope")

    print("MCP follow-up scope checker tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

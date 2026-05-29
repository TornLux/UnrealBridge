#!/usr/bin/env python3
"""Unit-style checks for the UnrealBridge MCP server protocol layer."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "unrealbridge_mcp_server.py"


def _load_server():
    spec = importlib.util.spec_from_file_location("unrealbridge_mcp_server_for_tests", SERVER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _boom(_args):
    raise RuntimeError("boom")


def _assert_error(reply, request_id, code, text):
    if reply.get("id") != request_id:
        raise AssertionError(f"expected id {request_id!r}, got {reply!r}")
    error = reply.get("error")
    if not isinstance(error, dict):
        raise AssertionError(f"expected error response, got {reply!r}")
    if error.get("code") != code:
        raise AssertionError(f"expected error code {code}, got {reply!r}")
    if text not in str(error.get("message", "")):
        raise AssertionError(f"expected {text!r} in error message, got {reply!r}")


def main() -> int:
    mcp = _load_server()
    test_tool = mcp.ToolSpec(
        "bridge_test_boom",
        "Test-only tool that raises.",
        {},
        _boom,
    )
    original = mcp.TOOL_BY_NAME.get(test_tool.name)
    mcp.TOOL_BY_NAME[test_tool.name] = test_tool

    try:
        reply = mcp._handle_message(
            {
                "jsonrpc": "2.0",
                "id": 42,
                "method": "tools/call",
                "params": {"name": test_tool.name, "arguments": {}},
            }
        )
        _assert_error(reply, 42, -32603, "bridge_test_boom")
        _assert_error(reply, 42, -32603, "boom")

        batch = mcp._handle_payload(
            [
                {
                    "jsonrpc": "2.0",
                    "id": 43,
                    "method": "tools/call",
                    "params": {"name": test_tool.name, "arguments": {}},
                },
                {"jsonrpc": "2.0", "id": 44, "method": "ping"},
            ]
        )
        if not isinstance(batch, list) or len(batch) != 2:
            raise AssertionError(f"expected two batch replies, got {batch!r}")
        _assert_error(batch[0], 43, -32603, "bridge_test_boom")
        if batch[1] != {"jsonrpc": "2.0", "id": 44, "result": {}}:
            raise AssertionError(f"batch did not continue after handler failure: {batch!r}")
    finally:
        if original is None:
            mcp.TOOL_BY_NAME.pop(test_tool.name, None)
        else:
            mcp.TOOL_BY_NAME[test_tool.name] = original

    print("MCP server protocol tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

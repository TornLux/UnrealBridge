#!/usr/bin/env python3
"""No-editor smoke test for the UnrealBridge MCP stdio wrapper."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "unrealbridge_mcp_server.py"


def _request(proc: subprocess.Popen[str], payload: Any) -> Any:
    assert proc.stdin is not None
    assert proc.stdout is not None
    proc.stdin.write(json.dumps(payload, separators=(",", ":")) + "\n")
    proc.stdin.flush()
    line = proc.stdout.readline()
    if not line:
        stderr = proc.stderr.read() if proc.stderr is not None else ""
        raise RuntimeError(f"MCP server exited before replying. stderr={stderr!r}")
    return json.loads(line)


def _notify(proc: subprocess.Popen[str], payload: Any) -> None:
    assert proc.stdin is not None
    proc.stdin.write(json.dumps(payload, separators=(",", ":")) + "\n")
    proc.stdin.flush()


def _result(reply: dict[str, Any]) -> Any:
    if "error" in reply:
        raise AssertionError(reply["error"])
    return reply["result"]


def _assert_tool_schemas(tools: list[dict[str, Any]]) -> None:
    names = [tool["name"] for tool in tools]
    if len(names) != len(set(names)):
        raise AssertionError(f"duplicate tool names: {names}")

    for tool in tools:
        name = tool["name"]
        if not tool.get("description"):
            raise AssertionError(f"{name} missing description")
        schema = tool.get("inputSchema")
        if not isinstance(schema, dict):
            raise AssertionError(f"{name} missing inputSchema")
        if schema.get("type") != "object":
            raise AssertionError(f"{name} inputSchema must be object: {schema}")
        if schema.get("additionalProperties") is not False:
            raise AssertionError(f"{name} must reject additionalProperties")
        properties = schema.get("properties")
        if not isinstance(properties, dict):
            raise AssertionError(f"{name} properties must be an object")
        required = schema.get("required")
        if not isinstance(required, list):
            raise AssertionError(f"{name} required must be a list")
        missing = [field for field in required if field not in properties]
        if missing:
            raise AssertionError(f"{name} required fields missing from properties: {missing}")


def main() -> int:
    env = os.environ.copy()
    env.setdefault("PYTHONIOENCODING", "utf-8")
    proc = subprocess.Popen(
        [sys.executable, str(SERVER)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=env,
    )

    try:
        init = _result(
            _request(
                proc,
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-03-26",
                        "capabilities": {},
                        "clientInfo": {"name": "smoke_mcp_stdio", "version": "0.1.0"},
                    },
                },
            )
        )
        assert init["serverInfo"]["name"] == "unrealbridge-mcp"
        assert "tools" in init["capabilities"]

        _notify(proc, {"jsonrpc": "2.0", "method": "notifications/initialized"})

        ping = _result(_request(proc, {"jsonrpc": "2.0", "id": 2, "method": "ping"}))
        assert ping == {}

        tools = _result(_request(proc, {"jsonrpc": "2.0", "id": 3, "method": "tools/list"}))
        _assert_tool_schemas(tools["tools"])
        names = {tool["name"] for tool in tools["tools"]}
        assert {
            "bridge_ping",
            "bridge_preflight",
            "bridge_exec",
            "bridge_search_assets_page",
            "bridge_list_actors_page",
            "bridge_searchable_name_values_page",
            "bridge_assets_referencing_searchable_name_page",
            "bridge_datatable_row_names_page",
            "bridge_datatable_search_rows_page",
            "bridge_blueprint_call_sites_page",
            "bridge_blueprint_debug_prints_page",
        }.issubset(names)

        preflight = _result(
            _request(
                proc,
                {
                    "jsonrpc": "2.0",
                    "id": 4,
                    "method": "tools/call",
                    "params": {
                        "name": "bridge_preflight",
                        "arguments": {"code": "print('mcp smoke')"},
                    },
                },
            )
        )
        assert not preflight.get("isError"), preflight
        payload = json.loads(preflight["content"][0]["text"])
        assert payload["ok"] is True
        assert payload["result"]["ok"] is True

        batch = _request(
            proc,
            [
                {"jsonrpc": "2.0", "id": 5, "method": "resources/list"},
                {"jsonrpc": "2.0", "id": 6, "method": "prompts/list"},
                {"jsonrpc": "2.0", "method": "notifications/cancelled"},
            ],
        )
        assert isinstance(batch, list)
        assert len(batch) == 2
        assert _result(batch[0]) == {"resources": []}
        assert _result(batch[1]) == {"prompts": []}
    finally:
        proc.stdin.close() if proc.stdin is not None else None
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)

    print("MCP stdio smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

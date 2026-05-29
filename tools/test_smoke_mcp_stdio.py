#!/usr/bin/env python3
"""Unit-style checks for smoke_mcp_stdio.py helpers."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SMOKE = ROOT / "tools" / "smoke_mcp_stdio.py"


def _load_smoke():
    spec = importlib.util.spec_from_file_location("smoke_mcp_stdio_for_tests", SMOKE)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _tool_schema(**overrides):
    tool = {
        "name": "bridge_ping",
        "description": "Ping UnrealBridge.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "project": {"type": "string"},
            },
            "required": [],
            "additionalProperties": False,
        },
    }
    tool.update(overrides)
    return tool


def _assert_fails(smoke, tools, expected: str) -> None:
    try:
        smoke._assert_tool_schemas(tools)
    except AssertionError as exc:
        if expected not in str(exc):
            raise AssertionError(f"expected {expected!r} in {exc!r}") from exc
        return
    raise AssertionError("expected schema check failure")


def main() -> int:
    smoke = _load_smoke()

    valid = _tool_schema()
    smoke._assert_tool_schemas([valid])

    _assert_fails(smoke, [valid, valid.copy()], "duplicate tool names")
    _assert_fails(smoke, [_tool_schema(description="")], "missing description")
    _assert_fails(smoke, [_tool_schema(inputSchema={"type": "array"})], "inputSchema must be object")
    _assert_fails(
        smoke,
        [
            _tool_schema(
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": [],
                    "additionalProperties": True,
                }
            )
        ],
        "must reject additionalProperties",
    )
    _assert_fails(
        smoke,
        [
            _tool_schema(
                inputSchema={
                    "type": "object",
                    "properties": {},
                    "required": ["project"],
                    "additionalProperties": False,
                }
            )
        ],
        "required fields missing from properties",
    )

    print("MCP stdio smoke helper tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

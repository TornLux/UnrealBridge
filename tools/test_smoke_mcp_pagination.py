#!/usr/bin/env python3
"""Unit-style checks for MCP pagination helper failure paths."""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "unrealbridge_mcp_server.py"


def _load_server():
    spec = importlib.util.spec_from_file_location("unrealbridge_mcp_for_pagination_tests", SERVER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _assert_raises(expected: str, func) -> None:
    try:
        func()
    except Exception as exc:
        if expected not in str(exc):
            raise AssertionError(f"expected {expected!r} in {exc!r}") from exc
        return
    raise AssertionError("expected failure")


def _content_json(response: dict) -> dict:
    return json.loads(response["content"][0]["text"])


def main() -> int:
    mcp = _load_server()

    parsed = mcp._parse_bridge_json_output(
        {"result": {"output": 'log line\n{"items":["A"],"has_more":false}'}}
    )
    assert parsed == {"items": ["A"], "has_more": False}

    _assert_raises("empty bridge output", lambda: mcp._parse_bridge_json_output({"result": {"output": ""}}))
    _assert_raises(
        "bridge output JSON must be an object",
        lambda: mcp._parse_bridge_json_output({"result": {"output": "[1,2,3]"}}),
    )
    _assert_raises(
        "Expecting value",
        lambda: mcp._parse_bridge_json_output({"result": {"output": "log line\nnot json"}}),
    )

    stale = mcp._handle_search_assets_page({"query": "hero", "cursor": "not-a-cursor"})
    assert stale.get("isError") is True
    stale_payload = _content_json(stale)
    assert stale_payload["ok"] is False
    assert stale_payload["code"] == "INVALID_CURSOR"

    original_run_bridge = mcp._run_bridge

    def bad_bridge_json(args, command, stdin=None):
        return {"exit_code": 0, "ok": True, "result": {"output": "log line\nnot json"}}

    def failed_bridge(args, command, stdin=None):
        return {"exit_code": 7, "ok": False, "error": "bridge failed"}

    try:
        mcp._run_bridge = bad_bridge_json
        bad_page = mcp._handle_datatable_row_names_page(
            {"data_table_path": "/Game/Data/DT_Items.DT_Items"}
        )
        assert bad_page.get("isError") is True
        bad_payload = _content_json(bad_page)
        assert bad_payload["ok"] is False
        assert "bad DataTable row-names payload" in bad_payload["error"]
        assert bad_payload["raw"]["exit_code"] == 0

        mcp._run_bridge = failed_bridge
        failed_page = mcp._handle_datatable_row_names_page(
            {"data_table_path": "/Game/Data/DT_Items.DT_Items"}
        )
        assert failed_page.get("isError") is True
        failed_payload = _content_json(failed_page)
        assert failed_payload["ok"] is False
        assert failed_payload["exit_code"] == 7
    finally:
        mcp._run_bridge = original_run_bridge

    print("MCP pagination helper tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

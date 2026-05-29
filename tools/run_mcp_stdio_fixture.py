#!/usr/bin/env python3
"""Run a data-driven no-editor MCP stdio compatibility fixture."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "unrealbridge_mcp_server.py"
DEFAULT_FIXTURE = ROOT / "tools" / "fixtures" / "mcp_stdio_common_probes.json"


def _request(proc: subprocess.Popen[str], payload: Any) -> Any:
    assert proc.stdin is not None
    assert proc.stdout is not None
    proc.stdin.write(json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n")
    proc.stdin.flush()
    line = proc.stdout.readline()
    if not line:
        stderr = proc.stderr.read() if proc.stderr is not None else ""
        raise AssertionError(f"MCP server exited before replying. stderr={stderr!r}")
    return json.loads(line)


def _notify(proc: subprocess.Popen[str], payload: Any) -> None:
    assert proc.stdin is not None
    proc.stdin.write(json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n")
    proc.stdin.flush()


def _result(reply: dict[str, Any]) -> Any:
    if "error" in reply:
        raise AssertionError(reply["error"])
    return reply["result"]


def _assert_equal(actual: Any, expected: Any, label: str) -> None:
    if actual != expected:
        raise AssertionError(f"{label}: expected {expected!r}, got {actual!r}")


def _assert_response_envelope(reply: dict[str, Any], request: dict[str, Any]) -> None:
    _assert_equal(reply.get("jsonrpc"), "2.0", "jsonrpc")
    if "id" in request:
        _assert_equal(reply.get("id"), request["id"], "response id")


def _assert_initialize(reply: dict[str, Any], expect: dict[str, Any]) -> None:
    result = _result(reply)
    _assert_equal(result["serverInfo"]["name"], expect["server_name"], "server name")
    _assert_equal(result["protocolVersion"], expect["protocol_version"], "protocol version")
    for capability in expect.get("capabilities", []):
        if capability not in result.get("capabilities", {}):
            raise AssertionError(f"missing capability: {capability}")


def _assert_request(reply: dict[str, Any], request: dict[str, Any], expect: dict[str, Any]) -> None:
    _assert_response_envelope(reply, request)

    if "error_code" in expect:
        error = reply.get("error")
        if not isinstance(error, dict):
            raise AssertionError(f"expected JSON-RPC error, got: {reply}")
        _assert_equal(error.get("code"), expect["error_code"], "error code")
        if "message_contains" in expect and expect["message_contains"] not in str(error.get("message", "")):
            raise AssertionError(f"error message missing {expect['message_contains']!r}: {error}")
        return

    result = _result(reply)
    if "result" in expect:
        _assert_equal(result, expect["result"], "result")
    if "tools_include" in expect:
        names = {tool["name"] for tool in result["tools"]}
        missing = sorted(set(expect["tools_include"]) - names)
        if missing:
            raise AssertionError(f"missing tools: {missing}")
    if expect.get("tool_ok") is True:
        if result.get("isError"):
            raise AssertionError(f"tool returned isError: {result}")
        payload = json.loads(result["content"][0]["text"])
        if expect.get("inner_ok") is True and not payload.get("ok"):
            raise AssertionError(f"inner payload not ok: {payload}")
        inner = payload.get("result")
        if expect.get("inner_ok") is True and isinstance(inner, dict) and inner.get("ok") is not True:
            raise AssertionError(f"inner result not ok: {payload}")
    if expect.get("tool_is_error") is True:
        if result.get("isError") is not True:
            raise AssertionError(f"tool did not return isError: {result}")
        if "content_contains" in expect:
            text = result["content"][0]["text"]
            if expect["content_contains"] not in text:
                raise AssertionError(f"tool content missing {expect['content_contains']!r}: {text!r}")


def _assert_batch(reply: list[dict[str, Any]], request: list[Any], expect: dict[str, Any]) -> None:
    _assert_equal(len(reply), expect["reply_count"], "batch reply count")
    expected_ids = expect.get("ids")
    if expected_ids is None:
        expected_ids = [item["id"] for item in request if isinstance(item, dict) and "id" in item]
    ids = [item.get("id") for item in reply]
    _assert_equal(ids, expected_ids, "batch response ids")
    for item in reply:
        _assert_equal(item.get("jsonrpc"), "2.0", "batch jsonrpc")
    results = [_result(item) for item in reply]
    _assert_equal(results, expect["results"], "batch results")


def _shutdown_server(proc: subprocess.Popen[str]) -> None:
    if proc.stdin is not None:
        proc.stdin.close()
    try:
        return_code = proc.wait(timeout=5)
    except subprocess.TimeoutExpired as exc:
        proc.kill()
        proc.wait(timeout=5)
        raise AssertionError("MCP server did not exit after stdin closed") from exc
    stderr = proc.stderr.read() if proc.stderr is not None else ""
    if return_code != 0:
        raise AssertionError(f"MCP server exited with code {return_code}. stderr={stderr!r}")
    if stderr.strip():
        raise AssertionError(f"MCP server wrote to stderr: {stderr!r}")


def run_fixture(fixture_path: Path) -> None:
    fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
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
        init_request = fixture["initialize"]["request"]
        init_reply = _request(proc, init_request)
        _assert_response_envelope(init_reply, init_request)
        _assert_initialize(init_reply, fixture["initialize"]["expect"])
        for notification in fixture.get("notifications", []):
            _notify(proc, notification)
        for item in fixture.get("requests", []):
            _assert_request(_request(proc, item["request"]), item["request"], item["expect"])
        if "batch" in fixture:
            batch_reply = _request(proc, fixture["batch"]["request"])
            if not isinstance(batch_reply, list):
                raise AssertionError(f"batch reply is not a list: {batch_reply!r}")
            _assert_batch(batch_reply, fixture["batch"]["request"], fixture["batch"]["expect"])
    finally:
        _shutdown_server(proc)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", default=str(DEFAULT_FIXTURE), help="Path to a JSON fixture.")
    args = parser.parse_args()
    run_fixture(Path(args.fixture))
    print(f"MCP stdio fixture passed: {args.fixture}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

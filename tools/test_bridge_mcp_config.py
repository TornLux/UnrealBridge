#!/usr/bin/env python3
"""No-editor checks for bridge.py mcp-config output."""

from __future__ import annotations

import json
import subprocess
import sys
import tomllib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "bridge.py"
SCRIPT = "C:/UnrealBridge/.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py"


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(BRIDGE), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def _require_ok(proc: subprocess.CompletedProcess[str]) -> str:
    if proc.returncode != 0:
        raise AssertionError(
            f"command failed with {proc.returncode}\nstdout={proc.stdout}\nstderr={proc.stderr}"
        )
    if proc.stderr:
        raise AssertionError(f"unexpected stderr: {proc.stderr}")
    return proc.stdout


def main() -> int:
    generic = json.loads(_require_ok(_run(
        "mcp-config",
        "--client", "generic",
        "--script-path", SCRIPT,
        "--project", "TEST_57",
    )))
    server = generic["mcpServers"]["unrealbridge"]
    assert server["command"] == "python"
    assert server["args"] == [SCRIPT]
    assert server["env"] == {"UNREAL_BRIDGE_PROJECT": "TEST_57"}

    claude = json.loads(_require_ok(_run(
        "mcp-config",
        "--client", "claude-desktop",
        "--script-path", SCRIPT,
        "--server-name", "ue",
        "--discovery-group", "239.1.2.3:9876",
    )))
    claude_server = claude["mcpServers"]["ue"]
    assert claude_server["command"] == "python"
    assert claude_server["args"] == [SCRIPT]
    assert claude_server["env"] == {
        "UNREAL_BRIDGE_DISCOVERY_GROUP": "239.1.2.3:9876",
    }

    cursor = json.loads(_require_ok(_run(
        "mcp-config",
        "--client", "cursor",
        "--script-path", SCRIPT,
        "--server-name", "unrealbridge-cursor",
        "--project", "TEST_57",
        "--endpoint", "127.0.0.1:6904",
    )))
    cursor_server = cursor["mcpServers"]["unrealbridge-cursor"]
    assert cursor_server["command"] == "python"
    assert cursor_server["args"] == [SCRIPT]
    assert cursor_server["env"] == {
        "UNREAL_BRIDGE_PROJECT": "TEST_57",
        "UNREAL_BRIDGE_ENDPOINT": "127.0.0.1:6904",
    }

    openclaw = json.loads(_require_ok(_run(
        "mcp-config",
        "--client", "openclaw",
        "--server-name", "ue",
        "--python", "py",
        "--script-path", SCRIPT,
    )))
    assert openclaw["mcp"]["servers"]["ue"]["command"] == "py"

    codex_text = _require_ok(_run(
        "mcp-config",
        "--client", "codex",
        "--script-path", SCRIPT,
        "--endpoint", "127.0.0.1:6904",
    ))
    codex = tomllib.loads(codex_text)
    codex_server = codex["mcp_servers"]["unrealbridge"]
    assert codex_server["command"] == "python"
    assert codex_server["args"] == [SCRIPT]
    assert codex_server["env"]["UNREAL_BRIDGE_ENDPOINT"] == "127.0.0.1:6904"

    codex_dotted_text = _require_ok(_run(
        "mcp-config",
        "--client", "codex",
        "--server-name", "unreal.bridge",
        "--script-path", SCRIPT,
        "--project", "TEST_57",
    ))
    codex_dotted = tomllib.loads(codex_dotted_text)
    assert "unreal.bridge" in codex_dotted["mcp_servers"]
    assert "unreal" not in codex_dotted["mcp_servers"]
    assert codex_dotted["mcp_servers"]["unreal.bridge"]["env"] == {
        "UNREAL_BRIDGE_PROJECT": "TEST_57",
    }

    codex_spaced_text = _require_ok(_run(
        "mcp-config",
        "--client", "codex",
        "--server-name", "unreal bridge",
        "--script-path", SCRIPT,
    ))
    codex_spaced = tomllib.loads(codex_spaced_text)
    assert codex_spaced["mcp_servers"]["unreal bridge"]["args"] == [SCRIPT]

    hermes = _require_ok(_run(
        "mcp-config",
        "--client", "hermes",
        "--script-path", SCRIPT,
        "--project", "TEST_57",
    ))
    assert "mcp_servers:" in hermes
    assert '  "unrealbridge":' in hermes
    assert f'      - "{SCRIPT}"' in hermes
    assert '      UNREAL_BRIDGE_PROJECT: "TEST_57"' in hermes
    assert "      resources: false" in hermes

    hermes_named = _require_ok(_run(
        "mcp-config",
        "--client", "hermes",
        "--server-name", "unreal:bridge # local",
        "--script-path", SCRIPT,
    ))
    assert '  "unreal:bridge # local":' in hermes_named
    assert "  unreal:bridge # local:" not in hermes_named

    token_proc = _run("--token", "secret-value", "mcp-config")
    assert token_proc.returncode == 2
    assert "refuses to print --token" in token_proc.stderr
    assert "secret-value" not in token_proc.stdout
    assert "secret-value" not in token_proc.stderr

    token_after_proc = _run("mcp-config", "--token", "secret-value")
    assert token_after_proc.returncode == 2
    assert "refuses to print --token" in token_after_proc.stderr
    assert "secret-value" not in token_after_proc.stdout
    assert "secret-value" not in token_after_proc.stderr

    token_equals_proc = _run("--token=secret-value", "mcp-config")
    assert token_equals_proc.returncode == 2
    assert "refuses to print --token" in token_equals_proc.stderr
    assert "secret-value" not in token_equals_proc.stdout
    assert "secret-value" not in token_equals_proc.stderr

    token_after_equals_proc = _run("mcp-config", "--token=secret-value")
    assert token_after_equals_proc.returncode == 2
    assert "refuses to print --token" in token_after_equals_proc.stderr
    assert "secret-value" not in token_after_equals_proc.stdout
    assert "secret-value" not in token_after_equals_proc.stderr

    print("bridge.py mcp-config tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

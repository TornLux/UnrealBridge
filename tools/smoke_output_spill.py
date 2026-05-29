#!/usr/bin/env python3
"""No-editor smoke tests for UnrealBridge output spill helpers."""

from __future__ import annotations

import importlib.util
import re
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
SCRIPT_DIR = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts"


def _load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    bridge = _load_module("unrealbridge_cli_for_spill_smoke", SCRIPT_DIR / "bridge.py")
    mcp = _load_module("unrealbridge_mcp_for_spill_smoke", SCRIPT_DIR / "unrealbridge_mcp_server.py")

    with tempfile.TemporaryDirectory(prefix="unrealbridge_spill_smoke_") as tmp:
        tmp_path = Path(tmp)
        args = SimpleNamespace(max_output_bytes=32, spill_dir=str(tmp_path / "bridge"))
        full_output = "0123456789" * 20
        result = bridge._apply_output_spills(
            {"success": True, "output": full_output, "error": ""},
            None,
            args,
            "spill-smoke",
        )

        assert "spills" in result, result
        assert "output" in result["spills"], result
        spill_path = Path(result["spills"]["output"]["path"])
        assert spill_path.exists(), spill_path
        assert spill_path.read_text(encoding="utf-8") == full_output
        assert "UnrealBridge truncated output" in result["output"]

        mcp_text = mcp._content(
            {"payload": "abcdef" * 80},
            args={"max_mcp_content_bytes": 64, "spill_dir": str(tmp_path / "mcp")},
        )["content"][0]["text"]
        assert "UnrealBridge MCP content truncated" in mcp_text, mcp_text
        match = re.search(r"wrote full text to (.+?) \(", mcp_text)
        assert match, mcp_text
        assert Path(match.group(1)).exists(), match.group(1)

    print("Output spill smoke passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run all no-editor MCP wrapper checks."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

CHECKS = [
    (
        "py_compile",
        [
            "-m",
            "py_compile",
            ".claude/skills/unreal-bridge/scripts/bridge.py",
            ".claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py",
            "tools/smoke_mcp_stdio.py",
            "tools/run_mcp_stdio_fixture.py",
            "tools/smoke_output_spill.py",
            "tools/smoke_mcp_pagination.py",
            "tools/check_mcp_followup_scope.py",
            "tools/check_mcp_tool_docs.py",
            "tools/check_mcp_workflow.py",
            "tools/test_check_mcp_followup_scope.py",
            "tools/test_check_mcp_tool_docs.py",
            "tools/test_check_mcp_workflow.py",
            "tools/test_smoke_mcp_all.py",
            "tools/test_smoke_mcp_pagination.py",
            "tools/test_smoke_mcp_stdio.py",
            "tools/smoke_mcp_all.py",
        ],
    ),
    ("stdio smoke", ["tools/smoke_mcp_stdio.py"]),
    ("pagination smoke", ["tools/smoke_mcp_pagination.py"]),
    ("output spill smoke", ["tools/smoke_output_spill.py"]),
    ("client probe fixture", ["tools/run_mcp_stdio_fixture.py"]),
    ("tool docs check", ["tools/check_mcp_tool_docs.py"]),
    ("tool docs checker tests", ["tools/test_check_mcp_tool_docs.py"]),
    ("workflow check", ["tools/check_mcp_workflow.py"]),
    ("workflow checker tests", ["tools/test_check_mcp_workflow.py"]),
    ("follow-up scope checker tests", ["tools/test_check_mcp_followup_scope.py"]),
    ("all-smoke runner tests", ["tools/test_smoke_mcp_all.py"]),
    ("pagination helper tests", ["tools/test_smoke_mcp_pagination.py"]),
    ("stdio smoke helper tests", ["tools/test_smoke_mcp_stdio.py"]),
]


def main() -> int:
    for name, args in CHECKS:
        print(f"== {name} ==", flush=True)
        proc = subprocess.run([sys.executable, *args], cwd=ROOT)
        if proc.returncode != 0:
            print(f"{name} failed with exit code {proc.returncode}", file=sys.stderr)
            return proc.returncode
    print("All no-editor MCP checks passed", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

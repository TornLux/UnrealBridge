#!/usr/bin/env python3
"""Check that MCP tool documentation matches the server tool registry."""

from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "unrealbridge_mcp_server.py"
DOCS = {
    "docs/mcp-stdio-wrapper.md": (
        "## Exposed tools",
        "## Output limits",
    ),
    ".claude/skills/unreal-bridge/SKILL.md": (
        "Exposed MCP tools:",
        "## Workflow",
    ),
}


def _load_server():
    spec = importlib.util.spec_from_file_location("unrealbridge_mcp_for_doc_check", SERVER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _section(path: Path, start: str, end: str) -> str:
    text = path.read_text(encoding="utf-8")
    start_index = text.find(start)
    if start_index < 0:
        raise AssertionError(f"{path.relative_to(ROOT)} is missing section start {start!r}")
    end_index = text.find(end, start_index + len(start))
    if end_index < 0:
        raise AssertionError(f"{path.relative_to(ROOT)} is missing section end {end!r}")
    return text[start_index:end_index]


def _listed_tools(section: str) -> list[str]:
    tools: list[str] = []
    for line in section.splitlines():
        match = re.match(r"^\s*-\s+`(bridge_[A-Za-z0-9_]+)`", line)
        if match:
            tools.append(match.group(1))
    return tools


def compare_tool_lists(expected: list[str], actual: list[str]) -> list[str]:
    if actual == expected:
        return []

    missing = [name for name in expected if name not in actual]
    extra = [name for name in actual if name not in expected]
    ordering = not missing and not extra
    details = []
    if missing:
        details.append(f"missing={missing}")
    if extra:
        details.append(f"extra={extra}")
    if ordering:
        details.append("same names but different order")
    return details


def main() -> int:
    mcp = _load_server()
    expected = [tool.name for tool in mcp.TOOLS]

    failures: list[str] = []
    for relative, bounds in DOCS.items():
        path = ROOT / relative
        tools = _listed_tools(_section(path, *bounds))
        details = compare_tool_lists(expected, tools)
        if details:
            failures.append(f"{relative}: {', '.join(details)}")

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print(f"MCP tool docs match server registry ({len(expected)} tools)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

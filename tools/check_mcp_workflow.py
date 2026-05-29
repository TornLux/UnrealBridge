#!/usr/bin/env python3
"""Check that the no-editor MCP workflow stays scoped and runnable."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "mcp-no-editor.yml"

REQUIRED_SNIPPETS = [
    "name: MCP no-editor checks",
    "pull_request:",
    "push:",
    "workflow_dispatch:",
    "ubuntu-latest",
    "windows-latest",
    "python tools/smoke_mcp_all.py",
]

EXPECTED_PATH_FILTERS = [
    ".claude/skills/unreal-bridge/**",
    "tools/check_mcp_followup_scope.py",
    "tools/check_mcp_tool_docs.py",
    "tools/check_mcp_workflow.py",
    "tools/fixtures/mcp_stdio_common_probes.json",
    "tools/run_mcp_stdio_fixture.py",
    "tools/smoke_mcp_all.py",
    "tools/smoke_mcp_pagination.py",
    "tools/smoke_mcp_stdio.py",
    "tools/smoke_output_spill.py",
    "tools/test_check_mcp_followup_scope.py",
    "tools/test_check_mcp_tool_docs.py",
    "tools/test_check_mcp_workflow.py",
    "tools/test_smoke_mcp_all.py",
    "tools/test_smoke_mcp_pagination.py",
    "tools/test_smoke_mcp_stdio.py",
    "docs/mcp-stdio-wrapper.md",
    "docs/plans/mcp-pagination-followups.md",
    "docs/plans/mcp-stdio-wrapper-roadmap.md",
    "docs/plans/pagination-followup-pr-description.md",
    "docs/plans/pagination-followup-submit-checklist.md",
    ".github/workflows/mcp-no-editor.yml",
]

FORBIDDEN_SNIPPETS = [
    '"tools/**"',
    "RunUAT",
    "UnrealEditor",
    "UE_",
]


def _line_indent(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def _event_path_filters(text: str, event_name: str) -> list[str]:
    lines = text.splitlines()
    event_line = f"  {event_name}:"
    try:
        event_index = lines.index(event_line)
    except ValueError as exc:
        raise AssertionError(f"missing workflow event {event_name!r}") from exc

    paths_index = None
    for index in range(event_index + 1, len(lines)):
        line = lines[index]
        if _line_indent(line) <= 2 and line.strip().endswith(":"):
            break
        if line == "    paths:":
            paths_index = index
            break
    if paths_index is None:
        raise AssertionError(f"missing paths list for workflow event {event_name!r}")

    paths: list[str] = []
    for line in lines[paths_index + 1:]:
        if _line_indent(line) <= 4 and line.strip():
            break
        stripped = line.strip()
        if stripped.startswith("- "):
            paths.append(stripped[2:].strip('"'))
    return paths


def check_workflow_text(text: str) -> list[str]:
    failures: list[str] = []
    missing = [snippet for snippet in REQUIRED_SNIPPETS if snippet not in text]
    forbidden = [snippet for snippet in FORBIDDEN_SNIPPETS if snippet in text]
    if missing:
        failures.append(f"Missing workflow snippets: {missing}")
    if forbidden:
        failures.append(f"Forbidden workflow snippets present: {forbidden}")

    for event_name in ("pull_request", "push"):
        try:
            paths = _event_path_filters(text, event_name)
        except AssertionError as exc:
            failures.append(str(exc))
            continue
        if paths != EXPECTED_PATH_FILTERS:
            failures.append(
                f"{event_name} paths differ: expected={EXPECTED_PATH_FILTERS!r} actual={paths!r}"
            )
    return failures


def main() -> int:
    failures = check_workflow_text(WORKFLOW.read_text(encoding="utf-8"))
    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print("MCP workflow check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

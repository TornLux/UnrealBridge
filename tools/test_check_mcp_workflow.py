#!/usr/bin/env python3
"""Unit-style checks for check_mcp_workflow.py."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / "tools" / "check_mcp_workflow.py"
WORKFLOW = ROOT / ".github" / "workflows" / "mcp-no-editor.yml"


def _load_checker():
    spec = importlib.util.spec_from_file_location("check_mcp_workflow_for_tests", CHECKER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _replace_once(text: str, old: str, new: str) -> str:
    if old not in text:
        raise AssertionError(f"fixture text missing {old!r}")
    return text.replace(old, new, 1)


def _assert_fails(checker, text: str, expected_fragment: str) -> None:
    failures = checker.check_workflow_text(text)
    if not failures:
        raise AssertionError("expected workflow check failure")
    joined = "\n".join(failures)
    if expected_fragment not in joined:
        raise AssertionError(f"expected {expected_fragment!r} in failures, got {joined!r}")


def main() -> int:
    checker = _load_checker()
    workflow = WORKFLOW.read_text(encoding="utf-8")

    assert checker.check_workflow_text(workflow) == []

    wide_tools = _replace_once(
        workflow,
        '      - "tools/check_mcp_tool_docs.py"',
        '      - "tools/**"',
    )
    _assert_fails(checker, wide_tools, "Forbidden workflow snippets present")

    missing_path = _replace_once(
        workflow,
        '      - "tools/check_mcp_workflow.py"\n',
        "",
    )
    _assert_fails(checker, missing_path, "paths differ")

    extra_path = _replace_once(
        workflow,
        '      - "tools/smoke_output_spill.py"\n',
        '      - "tools/smoke_output_spill.py"\n      - "tools/unrelated.py"\n',
    )
    _assert_fails(checker, extra_path, "paths differ")

    missing_event = _replace_once(workflow, "  workflow_dispatch:\n", "")
    _assert_fails(checker, missing_event, "workflow_dispatch")

    print("MCP workflow checker tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

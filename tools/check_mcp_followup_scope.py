#!/usr/bin/env python3
"""Check that the MCP pagination follow-up branch stays in scope."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

ALLOWED_FILES = {
    ".claude/skills/unreal-bridge/SKILL.md",
    ".claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py",
    ".github/workflows/mcp-no-editor.yml",
    "README.md",
    "README.zh-CN.md",
    "docs/mcp-stdio-wrapper.md",
    "docs/plans/mcp-pagination-followups.md",
    "docs/plans/mcp-stdio-wrapper-roadmap.md",
    "docs/plans/pagination-followup-pr-description.md",
    "docs/plans/pagination-followup-submit-checklist.md",
    "docs/plans/upstream-pr-description.md",
    "tools/check_mcp_followup_scope.py",
    "tools/check_mcp_tool_docs.py",
    "tools/check_mcp_workflow.py",
    "tools/fixtures/mcp_stdio_common_probes.json",
    "tools/run_mcp_stdio_fixture.py",
    "tools/smoke_mcp_all.py",
    "tools/smoke_mcp_pagination.py",
    "tools/smoke_mcp_stdio.py",
    "tools/test_check_mcp_followup_scope.py",
    "tools/test_check_mcp_tool_docs.py",
    "tools/test_check_mcp_workflow.py",
    "tools/test_smoke_mcp_all.py",
    "tools/test_smoke_mcp_pagination.py",
    "tools/test_smoke_mcp_stdio.py",
}

ALLOWED_PREFIXES = (
    "docs/reports/",
)

FORBIDDEN_PREFIXES = (
    "Plugin/",
)


def _changed_files(base_ref: str) -> list[str]:
    proc = subprocess.run(
        ["git", "diff", "--name-only", f"{base_ref}..HEAD"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or f"git diff failed for base {base_ref!r}")
    return [line.strip().replace("\\", "/") for line in proc.stdout.splitlines() if line.strip()]


def _is_allowed(path: str) -> bool:
    if path in ALLOWED_FILES:
        return True
    return any(path.startswith(prefix) for prefix in ALLOWED_PREFIXES)


def check_scope(changed: list[str]) -> list[str]:
    failures: list[str] = []
    forbidden = [path for path in changed if any(path.startswith(prefix) for prefix in FORBIDDEN_PREFIXES)]
    unexpected = [path for path in changed if not _is_allowed(path)]

    if forbidden:
        failures.append(f"forbidden Unreal plugin/source paths changed: {forbidden}")
    if unexpected:
        failures.append(f"unexpected files outside MCP follow-up scope: {unexpected}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--base",
        default="codex/mcp-stdio-wrapper-pr",
        help="Base ref to compare against. Use origin/main after PR #2 is merged and this branch is rebased.",
    )
    args = parser.parse_args()

    try:
        changed = _changed_files(args.base)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    failures = check_scope(changed)
    if failures:
        print(f"Changed files from {args.base}..HEAD:", file=sys.stderr)
        for path in changed:
            print(f"  {path}", file=sys.stderr)
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print(f"MCP follow-up scope check passed ({len(changed)} changed files from {args.base}..HEAD)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

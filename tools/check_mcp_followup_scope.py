#!/usr/bin/env python3
"""Check that MCP wrapper / pagination branches stay in scope."""

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
    "docs/plans/eda-integration-roadmap.md",
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
    "tools/test_bridge_mcp_config.py",
    "tools/test_mcp_server_protocol.py",
    "tools/test_smoke_mcp_all.py",
    "tools/test_smoke_mcp_pagination.py",
    "tools/test_smoke_mcp_stdio.py",
}

COMBINED_PR_EXTRA_FILES = {
    ".claude/skills/unreal-bridge/scripts/bridge.py",
    "docs/plans/upstream-pr-split-plan.md",
    "tools/smoke_output_spill.py",
}

ALLOWED_PREFIXES = (
    "docs/reports/",
)

COMBINED_PR_EXTRA_PREFIXES = (
    "Plugin/UnrealBridge/Source/UnrealBridge/",
)

COMBINED_PR_EXTRA_EXACT = {
    "Plugin/UnrealBridge/UnrealBridge.uplugin",
}

FOLLOWUP_FORBIDDEN_PREFIXES = (
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


def _is_allowed(path: str, mode: str) -> bool:
    allowed_files = set(ALLOWED_FILES)
    allowed_prefixes = tuple(ALLOWED_PREFIXES)
    if mode == "combined":
        allowed_files |= COMBINED_PR_EXTRA_FILES | COMBINED_PR_EXTRA_EXACT
        allowed_prefixes = (*allowed_prefixes, *COMBINED_PR_EXTRA_PREFIXES)

    if path in allowed_files:
        return True
    return any(path.startswith(prefix) for prefix in allowed_prefixes)


def check_scope(changed: list[str], mode: str = "followup") -> list[str]:
    if mode not in {"followup", "combined"}:
        raise ValueError(f"unsupported scope mode: {mode}")

    failures: list[str] = []
    forbidden: list[str] = []
    if mode == "followup":
        forbidden = [path for path in changed if any(path.startswith(prefix) for prefix in FOLLOWUP_FORBIDDEN_PREFIXES)]
    unexpected = [path for path in changed if not _is_allowed(path, mode)]

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
    parser.add_argument(
        "--mode",
        choices=("followup", "combined"),
        default="followup",
        help="Scope mode. Use 'combined' for PR #2; use 'followup' for later pagination-only branches.",
    )
    args = parser.parse_args()

    try:
        changed = _changed_files(args.base)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    failures = check_scope(changed, mode=args.mode)
    if failures:
        print(f"Changed files from {args.base}..HEAD:", file=sys.stderr)
        for path in changed:
            print(f"  {path}", file=sys.stderr)
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print(f"MCP {args.mode} scope check passed ({len(changed)} changed files from {args.base}..HEAD)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

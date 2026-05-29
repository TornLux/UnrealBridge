#!/usr/bin/env python3
"""Unit-style checks for smoke_mcp_all.py."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SMOKE_ALL = ROOT / "tools" / "smoke_mcp_all.py"


def _load_smoke_all():
    spec = importlib.util.spec_from_file_location("smoke_mcp_all_for_tests", SMOKE_ALL)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    smoke_all = _load_smoke_all()
    checks = smoke_all.CHECKS
    names = [name for name, _args in checks]
    if len(names) != len(set(names)):
        raise AssertionError(f"duplicate check names: {names}")

    py_compile_args = checks[0][1]
    if py_compile_args[:2] != ["-m", "py_compile"]:
        raise AssertionError(f"first check must be py_compile, got: {checks[0]}")
    py_compile_scripts = set(py_compile_args[2:])

    for script in py_compile_scripts:
        if not (ROOT / script).is_file():
            raise AssertionError(f"py_compile target does not exist: {script}")

    executed_scripts = []
    for name, args in checks[1:]:
        if len(args) != 1:
            raise AssertionError(f"{name} should execute exactly one script: {args}")
        script = args[0]
        executed_scripts.append(script)
        if script not in py_compile_scripts:
            raise AssertionError(f"{name} script is not covered by py_compile: {script}")

    for script in ("tools/smoke_mcp_all.py", "tools/test_smoke_mcp_all.py"):
        if script not in py_compile_scripts:
            raise AssertionError(f"{script} is missing from py_compile")

    if len(executed_scripts) != len(set(executed_scripts)):
        raise AssertionError(f"duplicate executed scripts: {executed_scripts}")

    print("MCP all-smoke runner tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Unit-style checks for check_mcp_tool_docs.py."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / "tools" / "check_mcp_tool_docs.py"


def _load_checker():
    spec = importlib.util.spec_from_file_location("check_mcp_tool_docs_for_tests", CHECKER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _assert_contains(items: list[str], expected: str) -> None:
    joined = "\n".join(items)
    if expected not in joined:
        raise AssertionError(f"expected {expected!r} in {joined!r}")


def main() -> int:
    checker = _load_checker()
    expected = ["bridge_ping", "bridge_exec", "bridge_search_assets_page"]

    assert checker.compare_tool_lists(expected, expected.copy()) == []

    missing = checker.compare_tool_lists(expected, ["bridge_ping", "bridge_exec"])
    _assert_contains(missing, "missing=['bridge_search_assets_page']")

    extra = checker.compare_tool_lists(expected, [*expected, "bridge_extra"])
    _assert_contains(extra, "extra=['bridge_extra']")

    reordered = checker.compare_tool_lists(expected, ["bridge_exec", "bridge_ping", "bridge_search_assets_page"])
    _assert_contains(reordered, "same names but different order")

    section = """
## Exposed tools

- `bridge_ping`
- `bridge_exec`
- not a tool
- `bridge_search_assets_page`: paginated wrapper
"""
    assert checker._listed_tools(section) == expected

    print("MCP tool docs checker tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

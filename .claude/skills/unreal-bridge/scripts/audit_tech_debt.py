#!/usr/bin/env python3
"""Audit tech debt before a release.

Scans for:
  1. Blueprint `PrintString` / `PrintText` / `PrintWarning` call sites
     (via `UnrealBridgeBlueprintLibrary.FindBlueprintDebugPrints`).
  2. `// TODO`, `// HACK`, `// FIXME`, `// XXX` comments across .cpp / .h /
     .hpp / .inl / .py / .cs source files.
  3. `UE_LOG(LogTemp, ...)` calls — the "I'll pick a proper log category later"
     placeholder that often ships by accident.

Exit codes:
    0  scan completed (regardless of finding count)
    1  bridge unreachable / exec failed and --skip-bp was not set
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
from collections import Counter

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
BRIDGE_PY = SCRIPT_DIR / "bridge.py"
# scripts → unreal-bridge → skills → .claude → <repo-root>
REPO_ROOT = SCRIPT_DIR.parents[3]

# Match "TODO:" / "TODO " / "TODO\n" etc. after a // or # comment start.
# Non-comment contexts (e.g. a log string containing "TODO") are intentionally
# included — false positives are cheap to ignore, false negatives hide debt.
TAG_RE = re.compile(r"\b(TODO|HACK|FIXME|XXX)\b[:\s-]", re.IGNORECASE)
UELOG_LOGTEMP_RE = re.compile(r"\bUE_LOG\s*\(\s*LogTemp\b")

SOURCE_EXTS = {".cpp", ".h", ".hpp", ".inl", ".py", ".cs"}
CPP_EXTS = {".cpp", ".h", ".hpp", ".inl"}
SKIP_DIRS = {
    "Binaries",
    "Intermediate",
    "Build",
    "DerivedDataCache",
    "Saved",
    ".git",
    ".vs",
    "node_modules",
    "__pycache__",
}


def scan_file(path: pathlib.Path, findings: list[dict]) -> None:
    ext = path.suffix.lower()
    try:
        with path.open("r", encoding="utf-8", errors="replace") as fh:
            for lineno, line in enumerate(fh, start=1):
                stripped = line.rstrip("\n")
                # Ignore this very audit source — it mentions TODO/HACK/etc.
                # in regex literals and would flag itself.
                if path.resolve() == pathlib.Path(__file__).resolve():
                    continue
                m = TAG_RE.search(stripped)
                if m:
                    findings.append(
                        {
                            "kind": m.group(1).upper(),
                            "source": f"{path}:{lineno}",
                            "details": stripped.strip(),
                        }
                    )
                if ext in CPP_EXTS and UELOG_LOGTEMP_RE.search(stripped):
                    findings.append(
                        {
                            "kind": "LOG_TEMP",
                            "source": f"{path}:{lineno}",
                            "details": stripped.strip(),
                        }
                    )
    except OSError:
        pass


def scan_filesystem(roots: list[pathlib.Path]) -> list[dict]:
    findings: list[dict] = []
    for root in roots:
        if not root.exists():
            print(f"[audit] skipping missing root: {root}", file=sys.stderr)
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if any(part in SKIP_DIRS for part in path.parts):
                continue
            if path.suffix.lower() not in SOURCE_EXTS:
                continue
            scan_file(path, findings)
    return findings


BP_EXEC_SCRIPT = """
import unreal, json
sites = unreal.UnrealBridgeBlueprintLibrary.find_blueprint_debug_prints({pkg!r}, {cap})
out = []
for s in sites:
    out.append({{
        'bp': str(s.blueprint_path),
        'graph_type': str(s.graph_type),
        'graph_name': str(s.graph_name),
        'guid': str(s.node_guid),
        'fn': str(s.function_name),
        'literal': str(s.string_literal),
        'connected': bool(s.has_connected_input),
    }})
print('BRIDGE_AUDIT_JSON_START' + json.dumps(out) + 'BRIDGE_AUDIT_JSON_END')
"""


def scan_blueprints(package_path: str, max_results: int) -> list[dict] | None:
    script = BP_EXEC_SCRIPT.format(pkg=package_path, cap=max_results)
    # bridge.py puts --json before the subcommand (argparse parses it at the
    # top-level parser, not the subparser).
    result = subprocess.run(
        [sys.executable, str(BRIDGE_PY), "--json", "exec", script],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"[audit] BP scan bridge call failed:\n{result.stderr.strip()}", file=sys.stderr)
        return None
    try:
        response = json.loads(result.stdout)
    except json.JSONDecodeError:
        print(f"[audit] bridge --json response wasn't JSON:\n{result.stdout}", file=sys.stderr)
        return None
    if not response.get("success"):
        err = response.get("error") or response.get("output") or response
        print(f"[audit] bridge exec reported failure:\n{err}", file=sys.stderr)
        return None
    output = response.get("output", "")
    match = re.search(
        r"BRIDGE_AUDIT_JSON_START(.*?)BRIDGE_AUDIT_JSON_END",
        output,
        re.DOTALL,
    )
    if not match:
        print(
            f"[audit] bridge output missing sentinel markers:\n{output}",
            file=sys.stderr,
        )
        return None
    sites = json.loads(match.group(1))
    findings: list[dict] = []
    for s in sites:
        if s["connected"]:
            arg_repr = "<connected>"
        elif s["literal"]:
            arg_repr = f'"{s["literal"]}"'
        else:
            arg_repr = "<empty>"
        details = f"{s['fn']}({arg_repr})"
        findings.append(
            {
                "kind": f"BP_{s['fn'].upper()}",
                "source": f"{s['bp']} @ {s['graph_type']}:{s['graph_name']} [{s['guid']}]",
                "details": details,
            }
        )
    return findings


def emit_text(findings: list[dict]) -> None:
    counts = Counter(f["kind"] for f in findings)
    print(f"# Tech-debt audit — {len(findings)} findings\n")
    if counts:
        for kind in sorted(counts):
            print(f"  {kind:16} {counts[kind]}")
        print()
    for f in findings:
        print(f"[{f['kind']}] {f['source']}")
        print(f"    {f['details']}")


def emit_json(findings: list[dict]) -> None:
    payload = {
        "total": len(findings),
        "counts": dict(Counter(f["kind"] for f in findings)),
        "items": findings,
    }
    print(json.dumps(payload, indent=2, ensure_ascii=False))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Audit BP PrintString + source-file TODO/HACK/FIXME/UE_LOG(LogTemp). "
            "Default scans the repo's Plugin/UnrealBridge/Source + all Blueprints "
            "under /Game via the running UE editor."
        ),
    )
    parser.add_argument("--skip-bp", action="store_true", help="Skip Blueprint scan (no bridge call).")
    parser.add_argument("--skip-cpp", action="store_true", help="Skip filesystem scan.")
    parser.add_argument("--package-path", default="/Game", help="BP content path (default /Game).")
    parser.add_argument("--max-bp-results", type=int, default=1000, help="Cap for BP scan (default 1000).")
    parser.add_argument(
        "--root",
        action="append",
        default=[],
        help="Additional filesystem root to scan (repeatable). Defaults to the repo root.",
    )
    parser.add_argument("--json", action="store_true", help="Emit JSON instead of text.")
    args = parser.parse_args()

    findings: list[dict] = []

    if not args.skip_cpp:
        roots = [REPO_ROOT, *(pathlib.Path(r) for r in args.root)]
        findings += scan_filesystem(roots)

    if not args.skip_bp:
        bp_findings = scan_blueprints(args.package_path, args.max_bp_results)
        if bp_findings is None:
            return 1
        findings += bp_findings

    if args.json:
        emit_json(findings)
    else:
        emit_text(findings)

    return 0


if __name__ == "__main__":
    sys.exit(main())

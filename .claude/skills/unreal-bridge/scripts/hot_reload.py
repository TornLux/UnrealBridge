#!/usr/bin/env python3
"""Hot-reload the UnrealBridge plugin via Live Coding (no editor restart).

Use this whenever your C++ edits only changed function bodies — no new
UFUNCTION / UCLASS / UPROPERTY / USTRUCT members. Live Coding patches the
running editor in place; the TCP bridge, open assets, and PIE state all
survive.

Flow:
  1. ping bridge (bail out if the editor isn't up)
  2. run sync_plugin.bat (unless --no-sync)
  3. call UnrealBridgeEditorLibrary.TriggerLiveCodingCompile via bridge
  4. report Status (Success / NoChanges / Failure / ...)

Exit codes:
    0  Success or NoChanges
    1  bridge ping / exec failed
    2  Live Coding compile reported failure / not started
    3  sync_plugin.bat failed
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
BRIDGE_PY  = SCRIPT_DIR / "bridge.py"
# scripts -> unreal-bridge -> skills -> .claude -> <repo-root>
REPO_ROOT  = SCRIPT_DIR.parents[3]
SYNC_BAT   = REPO_ROOT / "sync_plugin.bat"


def run_bridge(argv: list[str], capture: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(BRIDGE_PY), *argv],
        capture_output=capture,
        text=True,
    )


def ping() -> bool:
    p = run_bridge(["ping"])
    if p.returncode != 0:
        sys.stderr.write(p.stderr or "bridge ping failed\n")
        return False
    return True


def trigger_live_coding(wait: bool, timeout: int) -> dict:
    wait_lit = "True" if wait else "False"
    script = (
        "import unreal, json\n"
        f"r = unreal.UnrealBridgeEditorLibrary.trigger_live_coding_compile({wait_lit})\n"
        "print(json.dumps({"
        "'triggered': bool(r.triggered),"
        "'completed': bool(r.completed),"
        "'status': str(r.status),"
        "'error': str(r.error)"
        "}))\n"
    )
    p = run_bridge(["--json", "--timeout", str(timeout), "exec", script])
    if p.returncode != 0:
        sys.stderr.write(p.stderr or "bridge exec failed\n")
        return {}
    try:
        envelope = json.loads(p.stdout)
    except json.JSONDecodeError:
        sys.stderr.write(f"Bad bridge envelope: {p.stdout!r}\n")
        return {}
    payload_text = envelope.get("output") or ""
    line = payload_text.strip().splitlines()[-1] if payload_text.strip() else ""
    try:
        return json.loads(line)
    except json.JSONDecodeError:
        sys.stderr.write(
            f"Failed to parse LC result. stdout={p.stdout!r}, line={line!r}\n"
        )
        return {}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--no-sync", action="store_true",
                    help="Skip sync_plugin.bat (assume sources already copied).")
    ap.add_argument("--no-wait", action="store_true",
                    help="Fire LC compile async and return immediately.")
    ap.add_argument("--timeout", type=int, default=300,
                    help="Bridge exec timeout in seconds (default: 300).")
    args = ap.parse_args()

    if not ping():
        return 1

    if not args.no_sync:
        if not SYNC_BAT.exists():
            sys.stderr.write(f"sync_plugin.bat not found at {SYNC_BAT}\n")
            return 3
        print(f"[hot-reload] syncing plugin via {SYNC_BAT.name} ...")
        sync = subprocess.run(["cmd.exe", "/c", str(SYNC_BAT)])
        if sync.returncode != 0:
            sys.stderr.write(f"sync_plugin.bat failed (rc={sync.returncode})\n")
            return 3

    print("[hot-reload] triggering Live Coding compile ...")
    result = trigger_live_coding(wait=not args.no_wait, timeout=args.timeout)
    if not result:
        return 1

    print(json.dumps(result, indent=2))

    if not result.get("triggered"):
        return 2

    status = result.get("status", "")
    if args.no_wait:
        return 0 if status in ("Success", "NoChanges", "InProgress") else 2
    return 0 if status in ("Success", "NoChanges") else 2


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Full C++ rebuild + editor relaunch for the UnrealBridge plugin.

Use this whenever a hot-reload via Live Coding won't work:
    - You added / removed / renamed a UFUNCTION / UCLASS / UPROPERTY
    - You changed a struct layout or reflection metadata
    - Live Coding reports Failure on `hot_reload.py`
    - The editor is already down

Flow:
  1. (if editor is up) ask it to quit_editor via the bridge
  2. wait for UnrealEditor.exe to exit (kill as last resort)
  3. run sync_plugin.bat (unless --no-sync)
  4. run the target project's Build.bat (compiles the editor module)
  5. launch UnrealEditor.exe <uproject> detached
  6. poll `bridge.py ping` until ready (or --no-wait)

Paths are read from sync_plugin.bat's DST line by default; override with
--project-dir / --uproject / --editor-exe. The target project is the one
sync_plugin.bat pushes to — usually NOT this repo.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import time

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
BRIDGE_PY  = SCRIPT_DIR / "bridge.py"
REPO_ROOT  = SCRIPT_DIR.parents[3]
SYNC_BAT   = REPO_ROOT / "sync_plugin.bat"

DEFAULT_EDITOR_EXE = r"G:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"


def parse_project_dir_from_sync_bat() -> pathlib.Path | None:
    """Pull the target project's root directory from sync_plugin.bat's DST line.

    The bat writes `set "DST=...\\Plugins\\UnrealBridge"`, so the project
    root is two parents up.
    """
    if not SYNC_BAT.exists():
        return None
    try:
        content = SYNC_BAT.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return None
    m = re.search(r'set\s+"DST=([^"]+)"', content, flags=re.IGNORECASE)
    if not m:
        return None
    dst = pathlib.Path(m.group(1))
    # DST ends with \Plugins\<PluginName>; project root is two levels up.
    if dst.parent.name.lower() == "plugins":
        return dst.parent.parent
    return None


def find_uproject(project_dir: pathlib.Path) -> pathlib.Path | None:
    for p in project_dir.glob("*.uproject"):
        return p
    return None


def is_editor_running() -> bool:
    try:
        out = subprocess.check_output(
            ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/NH"],
            text=True, stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False
    return "UnrealEditor.exe" in out


def quit_editor_gracefully(timeout_s: float) -> bool:
    """Try bridge.exec('quit_editor()'); fall back to TerminateProcess."""
    script = (
        "import unreal\n"
        "try:\n"
        "    unreal.SystemLibrary.quit_editor()\n"
        "except Exception as e:\n"
        "    print('quit_editor failed:', e)\n"
    )
    subprocess.run(
        [sys.executable, str(BRIDGE_PY), "--timeout", "10", "exec", script],
        capture_output=True, text=True,
    )
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if not is_editor_running():
            return True
        time.sleep(0.5)
    return False


def force_kill_editor() -> None:
    subprocess.run(
        ["taskkill", "/F", "/IM", "UnrealEditor.exe"],
        capture_output=True, text=True,
    )
    for _ in range(20):
        if not is_editor_running():
            return
        time.sleep(0.25)


def run_sync(verbose: bool) -> int:
    if not SYNC_BAT.exists():
        sys.stderr.write(f"sync_plugin.bat not found at {SYNC_BAT}\n")
        return 3
    print(f"[rebuild] syncing plugin via {SYNC_BAT.name} ...")
    p = subprocess.run(
        ["cmd.exe", "/c", str(SYNC_BAT)],
        capture_output=not verbose, text=True,
    )
    if p.returncode != 0:
        if not verbose and p.stdout:
            sys.stderr.write(p.stdout)
        if not verbose and p.stderr:
            sys.stderr.write(p.stderr)
        sys.stderr.write(f"sync_plugin.bat failed (rc={p.returncode})\n")
        return 3
    return 0


def run_build(project_dir: pathlib.Path, verbose: bool) -> int:
    build_bat = project_dir / "Build.bat"
    if not build_bat.exists():
        sys.stderr.write(f"Build.bat not found at {build_bat}\n")
        return 4
    print(f"[rebuild] compiling editor target: {build_bat}")
    # Stream stdout so long compiles don't look hung.
    proc = subprocess.Popen(
        ["cmd.exe", "/c", str(build_bat)],
        cwd=str(project_dir),
        stdout=None if verbose else subprocess.PIPE,
        stderr=None if verbose else subprocess.STDOUT,
        text=True,
    )
    if not verbose and proc.stdout:
        for line in proc.stdout:
            # Only surface lines that look informative; drop heavy UBT chatter.
            lower = line.lower()
            if any(tag in lower for tag in ("error", "warning", "total time", "compiling", "link:")):
                sys.stdout.write(line)
    rc = proc.wait()
    if rc != 0:
        sys.stderr.write(f"Build.bat failed (rc={rc})\n")
        return 4
    return 0


def launch_editor(editor_exe: pathlib.Path, uproject: pathlib.Path) -> None:
    print(f"[rebuild] launching: {editor_exe} {uproject.name}")
    # Detach so this script returns as soon as the editor starts.
    # Windows: CREATE_NEW_PROCESS_GROUP + DETACHED_PROCESS.
    flags = 0
    if os.name == "nt":
        flags = 0x00000200 | 0x00000008  # CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS
    subprocess.Popen(
        [str(editor_exe), str(uproject)],
        creationflags=flags,
        close_fds=True,
    )


def wait_for_bridge(timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    last_reported = 0.0
    while time.time() < deadline:
        p = subprocess.run(
            [sys.executable, str(BRIDGE_PY), "--json", "--timeout", "3", "ping"],
            capture_output=True, text=True,
        )
        if p.returncode == 0:
            try:
                data = json.loads(p.stdout)
                if data.get("success") and data.get("ready") is True:
                    return True
            except json.JSONDecodeError:
                pass
        now = time.time()
        if now - last_reported > 15:
            remaining = int(deadline - now)
            print(f"[rebuild] waiting for bridge ... ({remaining}s remaining)")
            last_reported = now
        time.sleep(2.0)
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--project-dir",
                    help="UE project root (contains Build.bat + .uproject). "
                         "Default: parsed from sync_plugin.bat DST line.")
    ap.add_argument("--uproject",
                    help="Explicit path to .uproject (default: first *.uproject "
                         "under --project-dir).")
    ap.add_argument("--editor-exe", default=DEFAULT_EDITOR_EXE,
                    help=f"UnrealEditor.exe path (default: {DEFAULT_EDITOR_EXE}).")
    ap.add_argument("--no-sync", action="store_true")
    ap.add_argument("--no-build", action="store_true",
                    help="Skip Build.bat. Useful if you've already compiled.")
    ap.add_argument("--no-launch", action="store_true",
                    help="Don't relaunch the editor after build.")
    ap.add_argument("--no-wait", action="store_true",
                    help="Don't poll the bridge after launching.")
    ap.add_argument("--wait-timeout", type=float, default=300,
                    help="Seconds to wait for bridge to come up (default: 300).")
    ap.add_argument("--quit-timeout", type=float, default=30,
                    help="Seconds to wait for graceful editor quit (default: 30).")
    ap.add_argument("--verbose", action="store_true",
                    help="Stream sync/build output to stdout.")
    args = ap.parse_args()

    # Resolve project dir
    if args.project_dir:
        project_dir = pathlib.Path(args.project_dir)
    else:
        project_dir = parse_project_dir_from_sync_bat()
        if project_dir is None:
            sys.stderr.write(
                "Could not parse project dir from sync_plugin.bat. "
                "Pass --project-dir.\n"
            )
            return 5
    if not project_dir.is_dir():
        sys.stderr.write(f"project-dir does not exist: {project_dir}\n")
        return 5

    # Resolve uproject
    if args.uproject:
        uproject = pathlib.Path(args.uproject)
    else:
        uproject = find_uproject(project_dir)
        if uproject is None:
            sys.stderr.write(f"No .uproject found under {project_dir}\n")
            return 5
    if not uproject.is_file():
        sys.stderr.write(f"uproject does not exist: {uproject}\n")
        return 5

    editor_exe = pathlib.Path(args.editor_exe)
    if not args.no_launch and not editor_exe.is_file():
        sys.stderr.write(f"UnrealEditor.exe not found at {editor_exe}\n")
        return 5

    # 1+2: shut the editor down
    if is_editor_running():
        print(f"[rebuild] editor is running — asking it to quit ...")
        if not quit_editor_gracefully(args.quit_timeout):
            print("[rebuild] graceful quit timed out — force killing.")
            force_kill_editor()
        if is_editor_running():
            sys.stderr.write("Editor still running after taskkill — aborting.\n")
            return 6

    # 3: sync
    if not args.no_sync:
        rc = run_sync(args.verbose)
        if rc != 0:
            return rc

    # 4: build
    if not args.no_build:
        rc = run_build(project_dir, args.verbose)
        if rc != 0:
            return rc

    # 5: launch
    if not args.no_launch:
        launch_editor(editor_exe, uproject)

    # 6: wait for bridge
    if not args.no_launch and not args.no_wait:
        if wait_for_bridge(args.wait_timeout):
            print("[rebuild] bridge is ready.")
            return 0
        sys.stderr.write(
            f"Bridge did not come up within {args.wait_timeout:.0f}s.\n"
        )
        return 7

    return 0


if __name__ == "__main__":
    sys.exit(main())

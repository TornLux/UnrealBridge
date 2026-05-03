#!/usr/bin/env python3
"""Build the UnrealBridge plugin against multiple Unreal Engine versions.

Reads engine paths from tools/engines.local.json (preferred) or tools/engines.json,
runs RunUAT.bat BuildPlugin per engine, captures logs, and writes an aggregate
markdown report at <repo>/build_matrix_report.md.

Usage:
    python tools/build_matrix.py                  # all enabled engines
    python tools/build_matrix.py --only 5.7       # one version
    python tools/build_matrix.py --only 5.4,5.7   # explicit set
    python tools/build_matrix.py --skip 5.4       # skip one
    python tools/build_matrix.py --verbose        # mirror UAT output to stdout
    python tools/build_matrix.py --keep-package   # don't wipe package dir before run
    python tools/build_matrix.py --list           # list configured engines and exit
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR = REPO_ROOT / "tools"
PLUGIN_FILE = REPO_ROOT / "Plugin" / "UnrealBridge" / "UnrealBridge.uplugin"
OUT_ROOT = TOOLS_DIR / "build_engines"
REPORT_FILE = REPO_ROOT / "build_matrix_report.md"

# Windows consoles in non-en locales (e.g. cp936) can't encode every Unicode
# code point that ends up in MSVC localized error text — replace instead of crash.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(errors="replace")

ERROR_PATTERNS = [
    re.compile(r"error C\d+:"),
    re.compile(r"error LNK\d+:"),
    re.compile(r"\): error :"),
    re.compile(r"^\s*ERROR: ", re.IGNORECASE | re.MULTILINE),
    re.compile(r"BUILD FAILED"),
    re.compile(r"AutomationTool exiting with ExitCode=\d+\s*\(Error_"),
]

@dataclass
class EngineConfig:
    version: str
    path: Path
    enabled: bool = True

@dataclass
class EngineResult:
    version: str
    engine_path: str
    started_at: str
    duration_s: float
    exit_code: int
    success: bool
    status: str  # PASS | FAIL | SKIP_NO_PATH | SKIP_DISABLED | SKIP_FILTERED
    first_error: Optional[str] = None
    error_context: list = field(default_factory=list)
    log_file: str = ""

def load_config() -> tuple[list[EngineConfig], list[str], Path]:
    local = TOOLS_DIR / "engines.local.json"
    fallback = TOOLS_DIR / "engines.json"
    src = local if local.exists() else fallback
    if not src.exists():
        sys.exit(
            f"error: no engine config found.\n"
            f"  expected one of:\n"
            f"    {local}\n"
            f"    {fallback}\n"
            f"  copy tools/engines.example.json to tools/engines.local.json and edit paths."
        )
    data = json.loads(src.read_text(encoding="utf-8"))
    engines = [
        EngineConfig(
            version=str(e["version"]),
            path=Path(e["path"]),
            enabled=bool(e.get("enabled", True)),
        )
        for e in data.get("engines", [])
    ]
    build_args = list(data.get("build_args", ["-TargetPlatforms=Win64", "-Rocket"]))
    return engines, build_args, src

def runuat_path(engine_path: Path) -> Path:
    return engine_path / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"

def find_first_error(log_path: Path) -> tuple[Optional[str], list[str]]:
    if not log_path.exists():
        return None, []
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    for idx, line in enumerate(lines):
        for pat in ERROR_PATTERNS:
            if pat.search(line):
                start = max(0, idx - 2)
                end = min(len(lines), idx + 8)
                return line.strip(), lines[start:end]
    return None, []

def run_engine(eng: EngineConfig, build_args: list[str], verbose: bool, keep_package: bool) -> EngineResult:
    out_dir = OUT_ROOT / eng.version
    package_dir = out_dir / "package"
    log_file = out_dir / "uat.log"
    status_file = out_dir / "status.json"
    out_dir.mkdir(parents=True, exist_ok=True)

    runuat = runuat_path(eng.path)
    if not runuat.exists():
        result = EngineResult(
            version=eng.version,
            engine_path=str(eng.path),
            started_at=datetime.now().isoformat(timespec="seconds"),
            duration_s=0.0,
            exit_code=-1,
            success=False,
            status="SKIP_NO_PATH",
            first_error=f"RunUAT.bat not found at {runuat}",
            log_file=str(log_file.relative_to(REPO_ROOT)) if log_file.exists() else "",
        )
        status_file.write_text(json.dumps(asdict(result), indent=2, default=str), encoding="utf-8")
        print(f"[{eng.version}] SKIP — {result.first_error}")
        return result

    if not keep_package and package_dir.exists():
        shutil.rmtree(package_dir, ignore_errors=True)
    package_dir.mkdir(parents=True, exist_ok=True)

    comspec = os.environ.get("COMSPEC", "cmd.exe")
    cmd = [
        comspec, "/c",
        str(runuat),
        "BuildPlugin",
        f"-Plugin={PLUGIN_FILE}",
        f"-Package={package_dir}",
        *build_args,
    ]
    # Force MSVC to emit English diagnostics so logs stay UTF-8 compatible.
    env = {**os.environ, "VSLANG": "1033"}

    started = datetime.now()
    started_iso = started.isoformat(timespec="seconds")
    print(f"[{eng.version}] BuildPlugin against {eng.path}")
    print(f"[{eng.version}] log: {log_file.relative_to(REPO_ROOT)}")
    if verbose:
        print(f"[{eng.version}] cmd: {' '.join(cmd)}")

    t0 = time.monotonic()
    with open(log_file, "w", encoding="utf-8", errors="replace") as logf:
        logf.write(f"# build_matrix.py invocation\n")
        logf.write(f"# started: {started_iso}\n")
        logf.write(f"# engine_version: {eng.version}\n")
        logf.write(f"# engine_path: {eng.path}\n")
        logf.write(f"# cmd: {' '.join(cmd)}\n\n")
        logf.flush()

        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
            env=env,
        )
        assert proc.stdout is not None
        try:
            for line in proc.stdout:
                logf.write(line)
                logf.flush()
                if verbose:
                    print(line, end="")
        except KeyboardInterrupt:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
            raise
        proc.wait()

    duration = time.monotonic() - t0
    exit_code = proc.returncode
    success = exit_code == 0
    first_error, context = (None, []) if success else find_first_error(log_file)

    result = EngineResult(
        version=eng.version,
        engine_path=str(eng.path),
        started_at=started_iso,
        duration_s=round(duration, 1),
        exit_code=exit_code,
        success=success,
        status="PASS" if success else "FAIL",
        first_error=first_error,
        error_context=context,
        log_file=str(log_file.relative_to(REPO_ROOT)),
    )
    status_file.write_text(json.dumps(asdict(result), indent=2, default=str), encoding="utf-8")

    tag = result.status
    print(f"[{eng.version}] {tag} in {fmt_duration(duration)} (exit={exit_code})")
    if not success and first_error:
        print(f"[{eng.version}] first error: {first_error}")
    return result

def fmt_duration(s: float) -> str:
    if s < 60:
        return f"{s:.0f}s"
    m, sec = divmod(int(s), 60)
    return f"{m}m {sec:02d}s"

def write_report(results: list[EngineResult], config_src: Path, total_duration: float) -> None:
    out: list[str] = []
    out.append("# UnrealBridge Build Matrix Report")
    out.append("")
    out.append(f"- Run: {datetime.now().isoformat(timespec='seconds')}")
    out.append(f"- Total duration: {fmt_duration(total_duration)}")
    out.append(f"- Config: `{config_src.relative_to(REPO_ROOT)}`")
    out.append(f"- Plugin: `{PLUGIN_FILE.relative_to(REPO_ROOT)}`")
    out.append("")
    out.append("## Summary")
    out.append("")
    out.append("| Engine | Status | Duration | First Error |")
    out.append("|---|---|---|---|")
    for r in results:
        err = r.first_error or "—"
        if len(err) > 110:
            err = err[:107] + "..."
        err_md = err.replace("|", "\\|")
        out.append(f"| {r.version} | {r.status} | {fmt_duration(r.duration_s)} | {err_md} |")
    out.append("")

    failures = [r for r in results if r.status == "FAIL"]
    if failures:
        out.append("## Failures")
        out.append("")
        for r in failures:
            out.append(f"### {r.version}  →  exit {r.exit_code}")
            out.append("")
            out.append(f"Log: `{r.log_file}`")
            out.append("")
            if r.error_context:
                out.append("```")
                out.extend(r.error_context)
                out.append("```")
            out.append("")

    skipped = [r for r in results if r.status.startswith("SKIP")]
    if skipped:
        out.append("## Skipped")
        out.append("")
        for r in skipped:
            note = r.first_error or ""
            out.append(f"- **{r.version}** ({r.status}) — {note}")
        out.append("")

    out.append("## Re-run")
    out.append("")
    out.append("```")
    out.append("python tools/build_matrix.py                  # all enabled")
    out.append("python tools/build_matrix.py --only 5.7       # one version")
    out.append("python tools/build_matrix.py --verbose        # stream UAT output")
    out.append("```")
    out.append("")
    REPORT_FILE.write_text("\n".join(out), encoding="utf-8")
    print(f"\nReport: {REPORT_FILE.relative_to(REPO_ROOT)}")

def parse_versions_arg(s: str) -> set[str]:
    return {v.strip() for v in s.split(",") if v.strip()}

def main() -> int:
    ap = argparse.ArgumentParser(description="Build UnrealBridge plugin against multiple UE versions.")
    ap.add_argument("--only", type=parse_versions_arg, help="Comma-separated versions to build (e.g. 5.4,5.7)")
    ap.add_argument("--skip", type=parse_versions_arg, default=set(), help="Comma-separated versions to skip")
    ap.add_argument("--verbose", action="store_true", help="Mirror UAT output to stdout")
    ap.add_argument("--keep-package", action="store_true", help="Don't wipe package dir before each run")
    ap.add_argument("--list", action="store_true", help="List configured engines and exit")
    args = ap.parse_args()

    engines, build_args, config_src = load_config()
    print(f"Config: {config_src.relative_to(REPO_ROOT)}")
    print(f"Plugin: {PLUGIN_FILE.relative_to(REPO_ROOT)}")

    if args.list:
        for e in engines:
            ok = runuat_path(e.path).exists()
            print(f"  {e.version:6s} enabled={e.enabled} runuat_ok={ok} path={e.path}")
        return 0

    selected: list[EngineConfig] = []
    skipped_results: list[EngineResult] = []
    for e in engines:
        reason = None
        status = None
        if not e.enabled:
            reason, status = "disabled in config", "SKIP_DISABLED"
        elif args.only and e.version not in args.only:
            reason, status = "filtered out by --only", "SKIP_FILTERED"
        elif e.version in args.skip:
            reason, status = "filtered out by --skip", "SKIP_FILTERED"

        if reason:
            skipped_results.append(EngineResult(
                version=e.version, engine_path=str(e.path),
                started_at="", duration_s=0.0, exit_code=0, success=False,
                status=status, first_error=reason,
            ))
        else:
            selected.append(e)

    if not selected:
        print("error: no engines selected to build")
        return 2

    print(f"Building against: {', '.join(e.version for e in selected)}")
    OUT_ROOT.mkdir(parents=True, exist_ok=True)

    results: list[EngineResult] = []
    t0 = time.monotonic()
    interrupted = False
    try:
        for eng in selected:
            results.append(run_engine(eng, build_args, args.verbose, args.keep_package))
    except KeyboardInterrupt:
        interrupted = True
        print("\ninterrupted; writing partial report")
    total = time.monotonic() - t0

    write_report(results + skipped_results, config_src, total)

    if interrupted:
        return 130
    failed = sum(1 for r in results if not r.success)
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())

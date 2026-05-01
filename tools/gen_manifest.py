#!/usr/bin/env python3
"""
Generate bridge_manifest.json — single source of truth for AST preflight and
the kwargs-only wrapper module.

Two run modes (auto-detected by whether `import unreal` succeeds):

  CLI driver (outside UE):
      python tools/gen_manifest.py [--out PATH] [--bridge PATH]
      Drives a running UE editor via bridge.py to execute the in-UE half,
      captures the JSON output, and writes it to
      .claude/skills/unreal-bridge/scripts/bridge_manifest.json by default.

  In-UE reflection:
      bridge.py exec-file tools/gen_manifest.py
      Walks every unreal.UnrealBridge*Library class plus every
      unreal.Bridge* / unreal.EBridge* enum, and prints the manifest as
      JSON to stdout.

Manifest schema:
    {
      "generated_at": "<ISO 8601 UTC>",
      "ue_version": "5.7.x",
      "libraries": {
        "UnrealBridgeAssetLibrary": {
          "functions": {
            "search_assets": {
              "params": [
                {"name": "query", "type": "str", "default": null, "has_default": false},
                ...
              ],
              "returns": "tuple[list[SoftObjectPath], list[str]]",
              "doc": "Full-featured keyword search..."
            }
          }
        }
      },
      "enums": {
        "BridgeAssetSearchScope": ["ALL_ASSETS", "PROJECT", "CUSTOM_PACKAGE_PATH"]
      }
    }
"""

import json
import keyword as _keyword
import os
import sys

try:
    import unreal  # noqa: F401
    _IN_UE = True
except ImportError:
    _IN_UE = False


# ── In-UE half: reflect the live UnrealBridge* surface ─────────────────────

def _build_manifest_in_ue() -> dict:
    """Walk unreal.UnrealBridge*Library classes + Bridge* enums; return a manifest dict."""
    # All UnrealBridge*Library classes inherit from BlueprintFunctionLibrary →
    # UObject → _ObjectBase, which contributes ~50 generic helpers (cast,
    # get_class, call_method, get_editor_property, …). Those are NOT bridge
    # functions; subtract them so the manifest only carries our UFUNCTIONs.
    inherited_names = _collect_inherited_method_names()

    libraries = {}
    for name in sorted(dir(unreal)):
        if not name.startswith("UnrealBridge") or not name.endswith("Library"):
            continue
        cls = getattr(unreal, name, None)
        if cls is None or not isinstance(cls, type):
            continue
        funcs = {}
        for fn_name in sorted(dir(cls)):
            if fn_name.startswith("_"):
                continue
            if fn_name in inherited_names:
                continue
            fn = getattr(cls, fn_name, None)
            if fn is None or not callable(fn):
                continue
            entry = _introspect_function(fn, fn_name)
            if entry is not None:
                funcs[fn_name] = entry
        if funcs:
            libraries[name] = {"functions": funcs}

    enums = {}
    for name in sorted(dir(unreal)):
        # UE Python strips the `E` prefix on enums but the user's reference docs
        # also use `BridgeXxx` form — keep both names if both surface.
        if not (name.startswith("Bridge") or name.startswith("EBridge")):
            continue
        cls = getattr(unreal, name, None)
        if cls is None or not isinstance(cls, type):
            continue
        members = _enum_members(cls)
        if members:
            enums[name] = members

    return {
        "generated_at": _utc_now(),
        "ue_version": _ue_version_string(),
        "project_path": _project_path(),
        "libraries": libraries,
        "enums": enums,
    }


def _project_path() -> str:
    """Absolute path to the loaded .uproject file (for wrapper mirroring)."""
    try:
        proj_dir = unreal.SystemLibrary.get_project_directory()
        proj_name = unreal.SystemLibrary.get_game_name()
        return f"{proj_dir.rstrip('/')}/{proj_name}.uproject"
    except Exception:
        return ""


def _collect_inherited_method_names() -> set:
    """Return the set of method names contributed by UE's generic Python
    base classes (BlueprintFunctionLibrary, Object, _ObjectBase, …).

    Strategy: take the dir() of BlueprintFunctionLibrary itself — every
    UnrealBridge*Library inherits from it. Names present on the bare base
    class are NOT bridge functions and should not appear in the manifest.
    """
    base_names = set()
    base = getattr(unreal, "BlueprintFunctionLibrary", None)
    if base is not None and isinstance(base, type):
        base_names.update(n for n in dir(base) if not n.startswith("_"))
    # Also subtract anything on Object / _WrapperBase / _ObjectBase if reachable.
    for cand in ("Object",):
        c = getattr(unreal, cand, None)
        if c is not None and isinstance(c, type):
            base_names.update(n for n in dir(c) if not n.startswith("_"))
    return base_names


def _introspect_function(fn, name: str):
    """Extract param names + types + defaults + return type from a UE-bound callable.

    Strategy: try `inspect.signature` first (works for some bindings), fall
    back to parsing __doc__ which UE generates as
        `X.foo(arg1, arg2, ...) -> RetType -- summary`
    Returns None when the callable isn't shaped like a UFUNCTION binding (e.g.
    inherited Python builtins like __init_subclass__).
    """
    import inspect
    import re

    doc = (getattr(fn, "__doc__", "") or "").strip()
    summary = doc.split("\n", 1)[0] if doc else ""

    # --- Path 1: inspect.signature (rare for native UE bindings, but try) ---
    try:
        sig = inspect.signature(fn)
        params = []
        for pname, p in sig.parameters.items():
            if pname in ("self", "cls"):
                continue
            params.append({
                "name": pname,
                "type": _type_repr(p.annotation, p.empty),
                "default": _default_repr(p.default) if p.default is not p.empty else None,
                "has_default": p.default is not p.empty,
            })
        returns = _type_repr(sig.return_annotation, sig.empty)
        if params or returns or summary:
            return {"params": params, "returns": returns, "doc": summary}
    except (ValueError, TypeError):
        pass  # Fall through to docstring parser

    # --- Path 2: parse the UE-generated docstring ---
    if not doc:
        return None

    # Match a leading signature line. UE forms:
    #   X.foo(arg1, arg2=default) -> RetType -- summary
    #   foo(arg1) -> RetType -- summary
    first_line = doc.split("\n", 1)[0].strip()
    m = re.match(
        r"(?:[A-Za-z_]\w*\.)?(\w+)\s*\(([^)]*)\)\s*(?:->\s*([^-\n]+?))?\s*(?:--\s*(.*))?$",
        first_line,
    )
    if not m:
        return None
    _matched_name, arg_str, ret_str, summary_str = m.groups()
    ret_str = (ret_str or "").strip()
    summary_str = (summary_str or summary or "").strip()

    params = []
    for raw in _split_top_level(arg_str):
        raw = raw.strip()
        if not raw:
            continue
        if "=" in raw:
            n, d = raw.split("=", 1)
            params.append({
                "name": n.strip(),
                "type": "",
                "default": d.strip(),
                "has_default": True,
            })
        else:
            params.append({"name": raw, "type": "", "default": None, "has_default": False})

    return {"params": params, "returns": ret_str, "doc": summary_str}


def _split_top_level(s: str):
    """Split a comma-separated arg list, respecting balanced (), [], {}."""
    parts, buf, depth = [], [], 0
    for ch in s:
        if ch in "([{":
            depth += 1
            buf.append(ch)
        elif ch in ")]}":
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(ch)
    if buf:
        parts.append("".join(buf))
    return parts


def _enum_members(cls) -> list:
    """Heuristically detect enum members on a UE-bound enum class.

    UE Python convention: enum members are UPPER_CASE attributes whose value
    has an integer-like `.value` or is itself an int. Filter out non-members
    aggressively to avoid mistaking metaclass attrs for enum entries.
    """
    members = []
    for m in dir(cls):
        if m.startswith("_"):
            continue
        if not (m.isupper() or "_" in m and m.replace("_", "").isupper()):
            continue
        try:
            val = getattr(cls, m)
        except Exception:
            continue
        # Members are typically Enum instances or ints.
        if isinstance(val, int):
            members.append(m)
            continue
        if hasattr(val, "value"):
            try:
                int(val.value)
                members.append(m)
                continue
            except (TypeError, ValueError):
                pass
        # Exact-name comparison — Enum instance repr equals its name in UE Python
        if hasattr(val, "name") and getattr(val, "name", None) == m:
            members.append(m)
    return members


def _type_repr(annotation, empty_sentinel) -> str:
    """Stringify a parameter annotation; empty when unannotated."""
    if annotation is empty_sentinel:
        return ""
    try:
        if hasattr(annotation, "__name__"):
            return annotation.__name__
        return str(annotation)
    except Exception:
        return ""


def _default_repr(d) -> str:
    try:
        return repr(d)
    except Exception:
        return "<unrepr>"


def _utc_now() -> str:
    import datetime
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def _ue_version_string() -> str:
    try:
        return str(unreal.SystemLibrary.get_engine_version())
    except Exception:
        return "unknown"


# ── CLI driver: only fires when not running inside UE ──────────────────────

def _cli() -> int:
    import argparse
    import subprocess

    parser = argparse.ArgumentParser(
        description="Generate bridge_manifest.json by introspecting a running UE editor."
    )
    parser.add_argument("--out", help="Output path (default: <repo>/.claude/skills/unreal-bridge/scripts/bridge_manifest.json)")
    parser.add_argument("--wrapper-out", help="Wrapper module output path (default: <repo>/Plugin/UnrealBridge/Content/Python/unreal_bridge.py)")
    parser.add_argument("--no-wrapper", action="store_true", help="Skip generating the kwargs-only wrapper module")
    parser.add_argument("--bridge", help="Path to bridge.py (default: auto-detect relative to this script)")
    parser.add_argument("--timeout", type=int, default=60, help="Bridge call timeout in seconds (default: 60)")
    args = parser.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)  # tools/ → repo root
    bridge = args.bridge or os.path.join(
        repo, ".claude", "skills", "unreal-bridge", "scripts", "bridge.py"
    )
    out = args.out or os.path.join(
        repo, ".claude", "skills", "unreal-bridge", "scripts", "bridge_manifest.json"
    )

    if not os.path.isfile(bridge):
        print(f"ERROR: bridge.py not found at {bridge}", file=sys.stderr)
        return 1

    cmd = [sys.executable, bridge, "--json", "exec-file", os.path.abspath(__file__)]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        print(f"ERROR: bridge call timed out after {args.timeout}s", file=sys.stderr)
        return 1

    if res.returncode != 0:
        print(f"ERROR: bridge call failed (exit {res.returncode})", file=sys.stderr)
        if res.stderr:
            print(res.stderr, file=sys.stderr)
        if res.stdout:
            print(res.stdout, file=sys.stderr)
        return 1

    try:
        outer = json.loads(res.stdout)
    except json.JSONDecodeError:
        print(f"ERROR: bridge returned non-JSON:\n{res.stdout[:500]}", file=sys.stderr)
        return 1

    if not outer.get("success"):
        print(f"ERROR: in-UE script failed:\n{outer.get('error', '?')}", file=sys.stderr)
        return 1

    manifest_text = (outer.get("output") or "").strip()
    if not manifest_text:
        print("ERROR: in-UE script printed no output", file=sys.stderr)
        return 1

    # The script may print log lines BEFORE the JSON. Take the last line that
    # parses as JSON (the manifest is a single-line dump).
    last_json = None
    for line in reversed(manifest_text.splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            last_json = json.loads(line)
            break
        except json.JSONDecodeError:
            continue
    if last_json is None:
        print(f"ERROR: no JSON line in script output:\n{manifest_text[:500]}", file=sys.stderr)
        return 1

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(last_json, f, indent=2, ensure_ascii=False, sort_keys=True)
        f.write("\n")

    n_libs = len(last_json.get("libraries", {}))
    n_funcs = sum(len(L.get("functions", {})) for L in last_json.get("libraries", {}).values())
    n_enums = len(last_json.get("enums", {}))
    print(f"Wrote {out}")
    print(f"  {n_libs} libraries, {n_funcs} functions, {n_enums} enums")
    print(f"  UE: {last_json.get('ue_version', '?')}, generated: {last_json.get('generated_at', '?')}")

    if not args.no_wrapper:
        wrapper_out = args.wrapper_out or os.path.join(
            repo, "Plugin", "UnrealBridge", "Content", "Python", "unreal_bridge.py"
        )
        wrapper_src, stats = _generate_wrapper(last_json)
        os.makedirs(os.path.dirname(wrapper_out), exist_ok=True)
        with open(wrapper_out, "w", encoding="utf-8") as f:
            f.write(wrapper_src)
        print(f"Wrote {wrapper_out}")
        print(f"  {stats['classes']} classes, {stats['methods']} methods, "
              f"{stats['skipped']} skipped (Python keyword in param name)")

        # UE auto-loads Python from the target project's Plugins/UnrealBridge/
        # Content/Python/, not the source repo. Mirror the wrapper there so a
        # plain `import unreal_bridge` inside UE just works after regen.
        proj_uproject = (last_json.get("project_path") or "").strip()
        if proj_uproject:
            mirror = os.path.join(
                os.path.dirname(proj_uproject), "Plugins", "UnrealBridge",
                "Content", "Python", "unreal_bridge.py",
            )
            try:
                os.makedirs(os.path.dirname(mirror), exist_ok=True)
                with open(mirror, "w", encoding="utf-8") as f:
                    f.write(wrapper_src)
                print(f"Mirrored to {mirror}")
            except OSError as e:
                print(f"WARN: could not mirror wrapper to project ({e})", file=sys.stderr)
        else:
            print("WARN: project_path missing from manifest — wrapper not mirrored to live editor",
                  file=sys.stderr)

    return 0


# ── Wrapper module generation (offline; reads manifest, emits Python) ──────

def _generate_wrapper(manifest: dict) -> "tuple[str, dict]":
    """Emit the kwargs-only wrapper module source from the manifest.

    Each unreal.UnrealBridgeXxxLibrary becomes a class `Xxx` whose
    @staticmethods mirror the bridge functions with kwargs-only signatures.
    """
    out = []
    out.append('"""')
    out.append("Auto-generated kwargs-only wrapper for UnrealBridge*Library functions.")
    out.append("")
    out.append("Regenerate after C++ header changes:")
    out.append("    python tools/gen_manifest.py")
    out.append("")
    out.append("Usage from a script sent via the bridge:")
    out.append("    from unreal_bridge import Asset, Level")
    out.append("    paths, _ = Asset.search_assets_in_all_content(query='Hero', max_results=20)")
    out.append("    info = Level.get_actor_info(actor_path='/Persistent/Player')")
    out.append("")
    out.append("Why kwargs-only? Positional-arg-order is the #1 source of model")
    out.append("hallucinations against bridge APIs — kwargs make the contract")
    out.append("structural rather than mnemonic.")
    out.append('"""')
    out.append("")
    out.append("import unreal")
    out.append("")
    out.append(f"_GENERATED_AT = {manifest.get('generated_at', '?')!r}")
    out.append(f"_UE_VERSION = {manifest.get('ue_version', '?')!r}")
    out.append("")

    n_classes, n_methods, n_skipped = 0, 0, 0

    for lib_name in sorted(manifest.get("libraries", {}).keys()):
        lib = manifest["libraries"][lib_name]
        short = _short_name(lib_name)  # UnrealBridgeAssetLibrary → Asset
        out.append(f"class {short}:")
        out.append(f'    """Wraps unreal.{lib_name} (kwargs-only)."""')
        out.append("")
        n_classes += 1

        funcs = lib.get("functions", {})
        if not funcs:
            out.append("    pass")
            out.append("")
            continue

        for fn_name in sorted(funcs.keys()):
            fn = funcs[fn_name]
            params = fn.get("params", [])

            # Skip if any param name is a Python keyword — wrapping it would
            # produce invalid syntax (def foo(*, class=...) is a SyntaxError).
            bad = [p["name"] for p in params if _keyword.iskeyword(p["name"])]
            if bad:
                out.append(f"    # SKIPPED {fn_name}: param name(s) are Python keywords: {bad}")
                out.append(f"    # Call directly: unreal.{lib_name}.{fn_name}(...)")
                out.append("")
                n_skipped += 1
                continue

            sig_parts, call_parts = [], []
            for p in params:
                pname = p["name"]
                if p.get("has_default") and p.get("default") is not None:
                    sig_parts.append(f"{pname}={p['default']}")
                else:
                    sig_parts.append(pname)
                call_parts.append(pname)

            doc = (fn.get("doc") or "").replace('"""', "'''")
            out.append("    @staticmethod")
            if params:
                out.append(f"    def {fn_name}(*, {', '.join(sig_parts)}):")
            else:
                out.append(f"    def {fn_name}():")
            if doc:
                out.append(f'        """{doc}"""')
            out.append(f"        return unreal.{lib_name}.{fn_name}({', '.join(call_parts)})")
            out.append("")
            n_methods += 1

        out.append("")

    return "\n".join(out), {
        "classes": n_classes,
        "methods": n_methods,
        "skipped": n_skipped,
    }


def _short_name(lib_name: str) -> str:
    """UnrealBridgeAssetLibrary → Asset; UnrealBridgeUMGLibrary → UMG."""
    s = lib_name
    if s.startswith("UnrealBridge"):
        s = s[len("UnrealBridge"):]
    if s.endswith("Library"):
        s = s[: -len("Library")]
    return s or lib_name


# ── Entry point ────────────────────────────────────────────────────────────

if _IN_UE:
    print(json.dumps(_build_manifest_in_ue(), ensure_ascii=False))
else:
    sys.exit(_cli())

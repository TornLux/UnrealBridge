#!/usr/bin/env python3
"""
UnrealBridge CLI — execute Python in a running Unreal Editor over a TCP bridge.

Usage:
    python bridge.py ping                          # Check connection
    python bridge.py exec "print('hello')"         # Execute inline code (single statement)
    python bridge.py exec --stdin <<'EOF'          # Multi-step, one-shot, no disk file
    import unreal
    print(unreal.SystemLibrary.get_project_directory())
    EOF
    python bridge.py exec -                        # `-` is shorthand for --stdin
    python bridge.py exec-file script.py           # Multi-step, will iterate / keep on disk
    python bridge.py exec "code" --json            # Machine-readable output

The client auto-discovers the editor via UDP multicast (239.255.42.99:9876)
— zero config for the common case of one editor running on the local host.

Every exec / exec-file invocation is recorded as one JSONL line in the project's
Saved/UnrealBridge/exec.log (ring-buffered, 5 MB × 3 backups = 20 MB hard cap).
Useful for post-hoc audit and failure-mode analysis.

Discovery overrides (rarely needed):
    --project=<name|path>      Pick one of multiple running editors
    --endpoint=host:port       Skip discovery entirely, connect direct
    --token=<secret>           Required when the server binds non-loopback
    --discovery-timeout=<ms>   Probe wait window (default: 800)
    --discovery-group=a.b.c.d:p  Override the multicast group

Environment variable fallbacks:
    UNREAL_BRIDGE_PROJECT, UNREAL_BRIDGE_ENDPOINT, UNREAL_BRIDGE_TOKEN,
    UNREAL_BRIDGE_DISCOVERY_GROUP

Protocol: length-prefixed JSON over TCP
    Request:  [4 bytes big-endian length][JSON payload]
    Response: [4 bytes big-endian length][JSON payload]
"""

import argparse
import datetime
import json
import logging
import os
import socket
import struct
import sys
import uuid
from logging.handlers import RotatingFileHandler

try:
    from bridge_discovery import (
        DEFAULT_DISCOVERY_GROUP,
        DEFAULT_DISCOVERY_PORT,
        DEFAULT_DISCOVERY_TIMEOUT_MS,
        DiscoveryError,
        Endpoint,
        discover,
        load_token,
        select,
    )
except ImportError:
    # Support being launched as `python /abs/path/bridge.py` — add scripts/
    # dir to sys.path and retry.
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from bridge_discovery import (  # noqa: E402
        DEFAULT_DISCOVERY_GROUP,
        DEFAULT_DISCOVERY_PORT,
        DEFAULT_DISCOVERY_TIMEOUT_MS,
        DiscoveryError,
        Endpoint,
        discover,
        load_token,
        select,
    )


DEFAULT_TIMEOUT = 30

AUDIT_LOG_MAX_BYTES = 5 * 1024 * 1024   # 5 MB per file
AUDIT_LOG_BACKUPS = 3                    # 4 files total (1 active + 3 backups) = 20 MB hard cap
AUDIT_LOG_NAME = "exec.log"


# ── Resolution: turn CLI args into a (host, port, token, project_path) tuple ─

def resolve_target(args) -> "tuple[str, int, str | None, str | None]":
    """Figure out which editor to talk to.

    Returns (host, port, token, project_path). project_path is the .uproject
    file path when discovery succeeded — used to locate the per-project audit
    log under Saved/UnrealBridge/. None when --endpoint skipped discovery.

    Precedence:
      1. --endpoint=host:port (or UNREAL_BRIDGE_ENDPOINT)
      2. UDP multicast discovery, filtered by --project (or UNREAL_BRIDGE_PROJECT)
    """
    # 1. Explicit endpoint — no discovery, no project_path.
    endpoint_str = getattr(args, "endpoint", None) or os.environ.get("UNREAL_BRIDGE_ENDPOINT")
    if endpoint_str:
        if ":" not in endpoint_str:
            raise SystemExit(f"--endpoint must be host:port (got {endpoint_str!r})")
        host, port_s = endpoint_str.rsplit(":", 1)
        token = getattr(args, "token", None) or os.environ.get("UNREAL_BRIDGE_TOKEN")
        return host, int(port_s), token, None

    # 2. Discovery.
    project = getattr(args, "project", None) or os.environ.get("UNREAL_BRIDGE_PROJECT") or "*"
    group_str = (getattr(args, "discovery_group", None)
                 or os.environ.get("UNREAL_BRIDGE_DISCOVERY_GROUP")
                 or f"{DEFAULT_DISCOVERY_GROUP}:{DEFAULT_DISCOVERY_PORT}")
    if ":" in group_str:
        group_addr, group_port_s = group_str.rsplit(":", 1)
        group_port = int(group_port_s)
    else:
        group_addr, group_port = group_str, DEFAULT_DISCOVERY_PORT

    timeout_ms = getattr(args, "discovery_timeout", None) or DEFAULT_DISCOVERY_TIMEOUT_MS

    try:
        eps = discover(project_filter=project, group=group_addr,
                       group_port=group_port, timeout_ms=timeout_ms)
        ep = select(eps, project_filter=project if project != "*" else None)
        token = load_token(ep, explicit_token=getattr(args, "token", None)
                           or os.environ.get("UNREAL_BRIDGE_TOKEN"))
        return ep.host, ep.port, token, ep.project_path or None
    except DiscoveryError as e:
        raise SystemExit(f"discovery: {e}")


# ── Audit log: ring-buffered JSONL of every exec attempt ────────────────────

_AUDIT_LOGGER_CACHE: "dict[str, logging.Logger]" = {}


def _audit_log_path(project_path: "str | None") -> "str | None":
    """Resolve the audit-log target path.

    Priority: UNREAL_BRIDGE_AUDIT_LOG env override → <project>/Saved/UnrealBridge/exec.log
    → None (disabled) when neither is available (e.g. --endpoint without env override).
    """
    override = os.environ.get("UNREAL_BRIDGE_AUDIT_LOG")
    if override:
        return override
    if project_path:
        saved_dir = os.path.join(os.path.dirname(project_path), "Saved", "UnrealBridge")
        return os.path.join(saved_dir, AUDIT_LOG_NAME)
    return None


def _get_audit_logger(log_path: str) -> logging.Logger:
    """Lazily build a RotatingFileHandler-backed logger for the given path.

    Multiple bridge.py invocations can race on rotation (RotatingFileHandler
    is not process-safe). For a single-user/single-machine workflow the worst
    case is one log entry lost during the rotation window — acceptable.
    """
    cached = _AUDIT_LOGGER_CACHE.get(log_path)
    if cached is not None:
        return cached
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    logger = logging.getLogger(f"unreal_bridge.audit.{log_path}")
    logger.setLevel(logging.INFO)
    logger.propagate = False
    if not logger.handlers:
        handler = RotatingFileHandler(
            log_path,
            maxBytes=AUDIT_LOG_MAX_BYTES,
            backupCount=AUDIT_LOG_BACKUPS,
            encoding="utf-8",
        )
        handler.setFormatter(logging.Formatter("%(message)s"))
        logger.addHandler(handler)
    _AUDIT_LOGGER_CACHE[log_path] = logger
    return logger


_JSON_COMPACT_PREAMBLE = '''import json as __ub_json, os as __ub_os_for_json
# UE Python interpreter is persistent across exec calls. If we re-patched on
# every exec, second-and-later runs would chain wrappers (each capturing the
# previous patch as "orig") → infinite recursion. The marker makes patching
# idempotent: capture the TRUE original once, patch once, no-op forever after.
if not __ub_os_for_json.environ.get("UNREAL_BRIDGE_NO_JSON_COMPACT") and not getattr(__ub_json, "__ub_patched_marker__", False):
    __ub_json_orig_dumps = __ub_json.dumps
    __ub_json_orig_dump = __ub_json.dump

    def __ub_json_dumps(__ub_obj, *__ub_a, **__ub_kw):
        """Force-compact json.dumps. Strips indent (token waste), defaults to
        (',', ':') separators. Behaviorally identical for downstream parsers;
        saves 30-50% tokens on agent print(json.dumps(...)) calls."""
        __ub_kw.pop("indent", None)
        if "separators" not in __ub_kw:
            __ub_kw["separators"] = (",", ":")
        return __ub_json_orig_dumps(__ub_obj, *__ub_a, **__ub_kw)

    def __ub_json_dump(__ub_obj, __ub_fp, *__ub_a, **__ub_kw):
        __ub_kw.pop("indent", None)
        if "separators" not in __ub_kw:
            __ub_kw["separators"] = (",", ":")
        return __ub_json_orig_dump(__ub_obj, __ub_fp, *__ub_a, **__ub_kw)

    __ub_json.dumps = __ub_json_dumps
    __ub_json.dump = __ub_json_dump
    __ub_json.__ub_patched_marker__ = True
'''

_ATTR_ENRICH_PREAMBLE = '''import sys as __ub_sys, re as __ub_re, difflib as __ub_difflib

def __ub_to_snake(__ub_n):
    """PascalCase / 'Display Name' / 'Slash/Sep' -> snake_case. Handles all 3 forms
    of UPROPERTY names UE exposes: native PascalCase ('UserFriendlyName'), BP-variable
    display names with spaces ('User Friendly Name'), and slash-separated ('Thunder/Lightning').
    Order matters: PascalCase boundaries FIRST (or splits won't trigger after we've already
    inserted underscores from spaces), THEN collapse all separators to single underscore."""
    __ub_s1 = __ub_re.sub(r"(.)([A-Z][a-z]+)", r"\\1_\\2", __ub_n)
    __ub_s2 = __ub_re.sub(r"([a-z0-9])([A-Z])", r"\\1_\\2", __ub_s1)
    __ub_s3 = __ub_re.sub(r"[\\s/\\-_]+", "_", __ub_s2)
    return __ub_s3.strip("_").lower()

def __ub_resolve_for_enrich(__ub_exc):
    """Resolve (cls_name, cls_path_or_empty, methods_list, uprops_list) from the failing
    AttributeError. Prefers exc.obj (Python 3.10+, gives the ACTUAL class via obj.get_class()
    — critical for BP child classes whose parent is the only class on `unreal` module);
    falls back to parsing the exception message."""
    __ub_obj = getattr(__ub_exc, "obj", None)
    __ub_bad = getattr(__ub_exc, "name", None)
    try:
        import unreal as __ub_unreal
    except Exception:
        return None, None, "", "", [], []

    __ub_cls_name = ""
    __ub_cls_path = ""
    __ub_dir_target = None

    # Path A: Python 3.10+ — exc.obj is the live object/class/module
    if __ub_obj is not None and __ub_bad is not None:
        # Resolve the *actual* UClass via obj.get_class() — handles BP child classes
        # where type(obj) only sees the closest Python-bound ancestor.
        try:
            if hasattr(__ub_obj, "get_class") and not isinstance(__ub_obj, type):
                __ub_ue_cls = __ub_obj.get_class()
                __ub_cls_path = str(__ub_ue_cls.get_path_name())
                # Use the class name from the actual class (e.g. "UDS_Weather_Settings_C"),
                # falling back to type() name when get_name unavailable.
                __ub_cls_name = str(__ub_ue_cls.get_name()) if hasattr(__ub_ue_cls, "get_name") else type(__ub_obj).__name__
            elif hasattr(__ub_obj, "static_class"):
                # Class-level access (`unreal.X.Y`) where obj IS the class
                __ub_ue_cls = __ub_obj.static_class()
                __ub_cls_path = str(__ub_ue_cls.get_path_name())
                __ub_cls_name = __ub_obj.__name__ if hasattr(__ub_obj, "__name__") else type(__ub_obj).__name__
            else:
                __ub_cls_name = type(__ub_obj).__name__ if not isinstance(__ub_obj, type) else __ub_obj.__name__
        except Exception:
            __ub_cls_name = type(__ub_obj).__name__ if not isinstance(__ub_obj, type) else __ub_obj.__name__

        # dir() target rules:
        #   instance:   type(obj)   — gives bound methods
        #   class:      obj         — gives static methods
        #   module:     obj         — gives the module's top-level names (the 10k+ unreal classes)
        import types as __ub_types
        if isinstance(__ub_obj, __ub_types.ModuleType):
            __ub_dir_target = __ub_obj
        elif isinstance(__ub_obj, type):
            __ub_dir_target = __ub_obj
        else:
            __ub_dir_target = type(__ub_obj)
        if __ub_obj is __ub_unreal:
            __ub_cls_name = "unreal"

    # Path B: parse the message (Python <3.10 or non-standard exception)
    if not __ub_cls_name:
        __ub_msg = str(__ub_exc)
        __ub_m = (__ub_re.match(r"\\\'(\\w+)\\\' object has no attribute \\\'(\\w+)\\\'", __ub_msg)
                  or __ub_re.match(r"type object \\\'(\\w+)\\\' has no attribute \\\'(\\w+)\\\'", __ub_msg)
                  or __ub_re.match(r"module \\\'(\\w+)\\\' has no attribute \\\'(\\w+)\\\'", __ub_msg))
        if not __ub_m:
            return None, None, "", "", [], []
        __ub_cls_name, __ub_bad = __ub_m.group(1), __ub_m.group(2)
        if __ub_cls_name == "unreal":
            __ub_dir_target = __ub_unreal
        else:
            __ub_dir_target = getattr(__ub_unreal, __ub_cls_name, None)
            if __ub_dir_target is not None and hasattr(__ub_dir_target, "static_class"):
                try:
                    __ub_cls_path = str(__ub_dir_target.static_class().get_path_name())
                except Exception:
                    pass
        if __ub_dir_target is None:
            return None, None, "", "", [], []

    # Stage: Python-bound methods (snake_case from dir())
    try:
        __ub_methods = sorted([__ub_a for __ub_a in dir(__ub_dir_target) if not __ub_a.startswith("_")])
    except Exception:
        __ub_methods = []

    # Stage: UPROPERTYs via reflection (PascalCase) — uses bridge function we already shipped
    __ub_uprops = []
    if __ub_cls_path and __ub_cls_name != "unreal":
        try:
            __ub_pinfos = __ub_unreal.UnrealBridgeLevelLibrary.list_class_properties(__ub_cls_path)
            __ub_uprops = [str(__ub_p.name) for __ub_p in __ub_pinfos]
        except Exception:
            pass

    return __ub_bad, __ub_cls_name, __ub_cls_path, __ub_methods, __ub_uprops

def __ub_enrich_attr(__ub_exc):
    """Build the enriched AttributeError message. Returns enriched string, or None."""
    __ub_resolved = __ub_resolve_for_enrich(__ub_exc)
    if __ub_resolved is None or len(__ub_resolved) != 5:
        return None
    __ub_bad, __ub_cls_name, __ub_cls_path, __ub_methods, __ub_uprops = __ub_resolved
    if not (__ub_methods or __ub_uprops):
        return None

    __ub_msg = str(__ub_exc)
    __ub_lines = [__ub_msg]

    # Snake/Pascal mismatch — the high-value detection (UE UPROPERTY name vs Python attr guess)
    __ub_snake_to_pascal = {__ub_to_snake(__ub_p): __ub_p for __ub_p in __ub_uprops}
    __ub_pascal_match = __ub_snake_to_pascal.get(__ub_bad) if __ub_bad else None

    __ub_combined = sorted(set(__ub_methods + __ub_uprops))
    __ub_sugg = __ub_difflib.get_close_matches(__ub_bad, __ub_combined, n=3, cutoff=0.4) if __ub_bad else []

    if __ub_pascal_match:
        __ub_lines.append(f"  >> '{__ub_bad}' looks like snake_case. The actual UPROPERTY name is '{__ub_pascal_match}'.")
        __ub_lines.append(f"     For a UE asset:  obj.get_editor_property('{__ub_pascal_match}')")
        __ub_lines.append(f"     For a placed actor: Level.get_actor_property(actor_name='...', property_path='{__ub_pascal_match}')")
        if __ub_cls_path:
            __ub_lines.append(f"     All UPROPERTYs: Level.list_class_properties(class_path='{__ub_cls_path}')")
    elif __ub_sugg:
        __ub_lines.append(f"  did you mean: {__ub_sugg}")

    if __ub_methods:
        if len(__ub_methods) > 20:
            __ub_lines.append(f"  Python-bound methods on {__ub_cls_name} ({len(__ub_methods)} total, first 20): {__ub_methods[:20]}")
        else:
            __ub_lines.append(f"  Python-bound methods on {__ub_cls_name}: {__ub_methods}")
    if __ub_uprops:
        if len(__ub_uprops) > 30:
            __ub_lines.append(f"  UPROPERTYs on {__ub_cls_name} (PascalCase, {len(__ub_uprops)} total, first 30): {__ub_uprops[:30]}")
        else:
            __ub_lines.append(f"  UPROPERTYs on {__ub_cls_name} (PascalCase): {__ub_uprops}")
    return "\\n".join(__ub_lines)
'''

_ATTR_ENRICH_HANDLER = '''
except AttributeError as __ub_e:
    __ub_enriched = __ub_enrich_attr(__ub_e)
    if __ub_enriched:
        raise AttributeError(__ub_enriched).with_traceback(__ub_e.__traceback__) from None
    raise
'''


def _wrap_for_attr_enrichment(user_code: str) -> str:
    """Wrap user code in try/except AttributeError to enrich 'has no attribute' errors
    with valid-attrs + did-you-mean. Best-effort: if wrapping fails (or env opt-out),
    return user_code unchanged.

    User code runs at module level (inside try), so top-level vars persist into UE
    Python's globals across exec calls — same semantics as before.
    """
    if os.environ.get("UNREAL_BRIDGE_NO_ATTR_ENRICH"):
        return user_code
    # Indent every non-blank line by 4 spaces so it sits cleanly inside `try:`.
    indented = []
    for line in user_code.splitlines():
        if line.strip():
            indented.append("    " + line)
        else:
            indented.append(line)
    body = "\n".join(indented)
    # Note: we do NOT wrap if user code contains unindented future imports (they
    # must be at module top before any other statement). Detect minimally.
    if user_code.lstrip().startswith("from __future__"):
        return user_code
    return f"{_JSON_COMPACT_PREAMBLE}\n{_ATTR_ENRICH_PREAMBLE}\ntry:\n{body}\n{_ATTR_ENRICH_HANDLER}"


def _preflight_or_skip(code: str) -> "tuple[list[str], list[str]]":
    """Run AST preflight on `code`. Returns (errors, warnings).

    Best-effort: if bridge_preflight or its manifest is missing, returns
    ([], []) so the call proceeds normally — preflight is additive, not
    load-bearing.
    """
    try:
        from bridge_preflight import lint  # local import keeps cold-start cheap
    except ImportError:
        return [], []
    try:
        return lint(code)
    except Exception:
        return [], []


def _audit(project_path: "str | None", mode: str, src: "str | None",
           code: str, ok: bool, err: "str | None") -> None:
    """Best-effort audit log of one exec attempt. Never raises."""
    log_path = _audit_log_path(project_path)
    if not log_path:
        return
    try:
        entry = {
            "ts": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="milliseconds"),
            "pid": os.getpid(),
            "mode": mode,
            "src": src,
            "size": len(code.encode("utf-8")),
            "ok": ok,
            "err": err,
            "script": code,
        }
        line = json.dumps(entry, ensure_ascii=False)
        _get_audit_logger(log_path).info(line)
    except OSError:
        # Log directory unwritable / disk full / etc — don't break the call.
        pass
    except Exception:
        # Any other audit-side failure is non-fatal.
        pass


# ── Wire protocol helpers ─────────────────────────────────────────────────

def send_request(host: str, port: int, payload: dict, timeout: float,
                 token: "str | None" = None) -> dict:
    """Send a JSON request; return the parsed response."""
    if token:
        payload = dict(payload)
        payload.setdefault("token", token)
    data = json.dumps(payload).encode("utf-8")
    header = struct.pack(">I", len(data))

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect((host, port))
        sock.sendall(header + data)

        resp_header = _recv_all(sock, 4)
        resp_len = struct.unpack(">I", resp_header)[0]
        resp_data = _recv_all(sock, resp_len)
        return json.loads(resp_data.decode("utf-8"))


def _recv_all(sock: socket.socket, num_bytes: int) -> bytes:
    chunks = []
    received = 0
    while received < num_bytes:
        chunk = sock.recv(num_bytes - received)
        if not chunk:
            raise ConnectionError("Connection closed by server")
        chunks.append(chunk)
        received += len(chunk)
    return b"".join(chunks)


# ── Commands ────────────────────────────────────────────────────────────

def cmd_ping(args):
    host, port, token, _project_path = resolve_target(args)
    try:
        payload = {"id": str(uuid.uuid4()), "command": "ping"}
        resp = send_request(host, port, payload, args.timeout, token=token)
    except ConnectionRefusedError:
        if args.json:
            print(json.dumps({"success": False, "error": "Connection refused"}))
        else:
            print(
                f"ERROR: Cannot connect to {host}:{port}. "
                "Is Unreal Editor running with UnrealBridge plugin enabled?",
                file=sys.stderr,
            )
        return 1
    except Exception as e:
        if args.json:
            print(json.dumps({"success": False, "error": str(e)}))
        else:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(resp, ensure_ascii=False))
    else:
        if resp.get("success"):
            ready = resp.get("ready")
            suffix = ""
            if ready is False:
                suffix = " (editor still initializing — exec will be rejected)"
            elif ready is True:
                suffix = " (ready)"
            print(f"Connected to UnrealBridge at {host}:{port}{suffix}")
        else:
            print(f"ERROR: {resp.get('error', 'unexpected')}", file=sys.stderr)
            return 1
    return 0


def cmd_gt_ping(args):
    """Probe whether the UE GameThread is responsive."""
    host, port, token, _project_path = resolve_target(args)
    payload = {
        "id": str(uuid.uuid4()),
        "command": "gamethread_ping",
        "timeout": args.probe_timeout,
    }
    try:
        resp = send_request(host, port, payload, args.probe_timeout + 3.0, token=token)
    except Exception as e:
        if args.json:
            print(json.dumps({"success": False, "error": str(e)}))
        else:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(resp, ensure_ascii=False))
    else:
        state = resp.get("output") or "unknown"
        latency = resp.get("latency_ms")
        if latency is not None:
            print(f"{state} ({latency:.1f} ms)")
        else:
            print(state)
        err = resp.get("error")
        if err:
            print(err, file=sys.stderr)
    return 0 if resp.get("success") else 1


def cmd_resume(args):
    host, port, token, _project_path = resolve_target(args)
    try:
        payload = {"id": str(uuid.uuid4()), "command": "debug_resume"}
        resp = send_request(host, port, payload, args.timeout, token=token)
    except Exception as e:
        if args.json:
            print(json.dumps({"success": False, "error": str(e)}))
        else:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(resp, ensure_ascii=False))
    else:
        msg = resp.get("output") or resp.get("error") or "unknown"
        print(msg)
    return 0 if resp.get("success") else 1


def cmd_wait_compile(args):
    """Poll the editor for shader-map readiness on a given material / MI.

    Each poll is one fast bridge.exec (GetMaterialShaderCompileStatus), the
    GT is released between polls. This architecturally avoids the GT-waits-for-GT
    deadlock that happens when in-exec FinishAllCompilation tries to drain
    async completion callbacks while bridge is holding GT itself. See
    feedback_preview_material_serial_compile memory for the history.

    Exit codes:
      0  ready (shader map compiled + live on GT)
      1  timeout
      2  material not loadable
      3  bridge / transport error
    """
    import time as _time

    deadline = _time.time() + args.wait_timeout
    poll = max(0.1, float(args.poll_interval))
    code = (
        "import unreal, json\n"
        f"r = unreal.UnrealBridgeMaterialLibrary.get_material_shader_compile_status("
        f"'{args.material_path}', '{args.feature_level}', '{args.quality}')\n"
        "print(json.dumps({'found': bool(r.found), 'ready': bool(r.shader_map_ready),"
        " 'pending': int(r.pending_assets_global), 'fl': str(r.feature_level),"
        " 'ql': str(r.quality_level), 'err': str(r.error)}))\n"
    )

    host, port, token, _project_path = resolve_target(args)
    last = None
    while _time.time() < deadline:
        payload = {"id": str(uuid.uuid4()), "script": code, "timeout": 5}
        try:
            resp = send_request(host, port, payload, 10.0, token=token)
        except Exception as e:
            if args.json:
                print(json.dumps({"success": False, "error": f"transport: {e}"}))
            else:
                print(f"ERROR: {e}", file=sys.stderr)
            return 3

        if not resp.get("success"):
            err = resp.get("error") or "unknown"
            if args.json:
                print(json.dumps({"success": False, "error": err}))
            else:
                print(f"ERROR: {err}", file=sys.stderr)
            return 3

        line = (resp.get("output") or "").strip().splitlines()[-1] if resp.get("output") else ""
        try:
            last = json.loads(line)
        except Exception:
            if args.json:
                print(json.dumps({"success": False, "error": f"bad status payload: {line!r}"}))
            else:
                print(f"ERROR: bad payload: {line!r}", file=sys.stderr)
            return 3

        if not last.get("found"):
            # Empty material path, missing FL/QL resource — not a poll condition,
            # error out so the caller sees the config issue immediately.
            if args.json:
                print(json.dumps({"success": False, "status": last}))
            else:
                print(f"material lookup failed: {last.get('err')}", file=sys.stderr)
            return 2

        if last.get("ready"):
            if args.json:
                print(json.dumps({"success": True, "status": last}))
            else:
                print(f"ready ({last.get('fl')}/{last.get('ql')}, "
                      f"pending_global={last.get('pending')})")
            return 0

        _time.sleep(poll)

    if args.json:
        print(json.dumps({"success": False, "error": "timeout", "status": last}))
    else:
        pending = (last or {}).get("pending", "?")
        print(f"TIMEOUT after {args.wait_timeout}s  (last pending_global={pending})",
              file=sys.stderr)
    return 1


def cmd_exec(args):
    # Three input modes: positional code arg, --stdin flag, or `-` shorthand.
    use_stdin = args.stdin or args.code == "-"
    if use_stdin:
        if args.code is not None and args.code != "-":
            print("ERROR: cannot pass both a code argument and --stdin", file=sys.stderr)
            return 2
        code = sys.stdin.read()
        if not code.strip():
            print("ERROR: --stdin given but stdin was empty", file=sys.stderr)
            return 2
        return _execute(args, code, mode="stdin", src=None)
    if args.code is None:
        print("ERROR: provide a code argument or use --stdin", file=sys.stderr)
        return 2
    return _execute(args, args.code, mode="exec", src=None)


def cmd_exec_file(args):
    try:
        with open(args.file, "r", encoding="utf-8") as f:
            code = f.read()
    except FileNotFoundError:
        print(f"ERROR: File not found: {args.file}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"ERROR: Cannot read file: {e}", file=sys.stderr)
        return 1

    return _execute(args, code, mode="exec-file", src=args.file)


def cmd_suggest(args):
    """Look up bridge equivalents for raw unreal.* fallback patterns."""
    try:
        from bridge_preflight import load_redirects
    except ImportError:
        print("ERROR: bridge_preflight unavailable", file=sys.stderr)
        return 2
    redirects = load_redirects()
    if not redirects:
        print("ERROR: bridge_redirects.json not found", file=sys.stderr)
        return 2
    entries = redirects.get("redirects", [])

    needle = (args.pattern or "").lower().strip()
    matches = []
    for e in entries:
        # A direct_call entry has raw_pattern; an asset_registry_method entry
        # has method. Match either against the needle (or list everything if no needle).
        haystacks = []
        if e.get("raw_pattern"):
            haystacks.append(e["raw_pattern"])
        if e.get("method"):
            haystacks.append(f"AssetRegistryHelpers.{e['method']}")
        haystack_str = " ".join(haystacks).lower()
        if not needle or needle in haystack_str:
            matches.append((haystacks, e))

    if args.json:
        print(json.dumps({"matches": [e for _, e in matches]}, ensure_ascii=False, indent=2))
        return 0

    if not matches:
        print(f"No redirect matches '{args.pattern}'.\n"
              f"List everything with: bridge.py suggest")
        return 1

    for haystacks, e in matches:
        primary = haystacks[0] if haystacks else "(unknown)"
        print(f"-- {primary} --")
        bridge = e.get("bridge_replacement", "")
        for line in bridge.splitlines():
            print(f"  {line}")
        reason = e.get("reason", "")
        if reason:
            print(f"  why: {reason}")
        print()
    return 0


def cmd_preflight(args):
    """Lint a script standalone — never touches the network."""
    if args.file == "-":
        code = sys.stdin.read()
    else:
        try:
            with open(args.file, "r", encoding="utf-8") as f:
                code = f.read()
        except OSError as e:
            print(f"ERROR: cannot read {args.file}: {e}", file=sys.stderr)
            return 2
    errs = _preflight_or_skip(code)
    if not errs:
        if args.json:
            print(json.dumps({"ok": True, "errors": []}))
        else:
            print("preflight: clean")
        return 0
    if args.json:
        print(json.dumps({"ok": False, "errors": errs}, ensure_ascii=False))
    else:
        for e in errs:
            print(e, file=sys.stderr)
    return 1


def cmd_list_editors(args):
    """List every editor reachable via discovery — a diagnostic shortcut."""
    group_str = (getattr(args, "discovery_group", None)
                 or os.environ.get("UNREAL_BRIDGE_DISCOVERY_GROUP")
                 or f"{DEFAULT_DISCOVERY_GROUP}:{DEFAULT_DISCOVERY_PORT}")
    if ":" in group_str:
        group_addr, group_port_s = group_str.rsplit(":", 1)
        group_port = int(group_port_s)
    else:
        group_addr, group_port = group_str, DEFAULT_DISCOVERY_PORT

    timeout_ms = getattr(args, "discovery_timeout", None) or DEFAULT_DISCOVERY_TIMEOUT_MS

    eps = discover(project_filter="*", group=group_addr, group_port=group_port,
                   timeout_ms=timeout_ms)

    if args.json:
        print(json.dumps([ep.__dict__ for ep in eps], indent=2))
        return 0

    if not eps:
        print("(no editors found)")
        return 1

    for ep in eps:
        print(ep)
    return 0


def _execute(args, code: str, mode: str = "exec", src: "str | None" = None) -> int:
    # AST preflight: catch bridge-call errors locally before any UE round-trip.
    # Warnings are printed but don't block. Errors short-circuit with exit 3.
    if not getattr(args, "no_preflight", False):
        errs, warns = _preflight_or_skip(code)
        for w in warns:
            print(w, file=sys.stderr)
        if errs:
            for e in errs:
                print(e, file=sys.stderr)
            try:
                _, _, _, proj = resolve_target(args)
            except SystemExit:
                proj = None
            _audit(proj, mode, src, code, ok=False,
                   err=f"preflight: {len(errs)} error(s); first: {errs[0].splitlines()[0]}")
            return 3  # 3 = preflight rejection (distinct from 1 = transport, 2 = arg)

    host, port, token, project_path = resolve_target(args)

    # Wrap user code so AttributeError messages get enriched with valid-attrs
    # + did-you-mean before reaching the agent's stderr. UE engine API has
    # 1000s of types; agent attribute hallucinations are unavoidable, but the
    # cost can be one round-trip → instant fix instead of N guesses.
    wrapped = _wrap_for_attr_enrichment(code)

    payload = {
        "id": str(uuid.uuid4()),
        "script": wrapped,
        "timeout": args.timeout,
    }

    try:
        resp = send_request(host, port, payload, args.timeout + 5, token=token)
    except ConnectionRefusedError:
        msg = (
            f"Cannot connect to {host}:{port}. "
            "Is Unreal Editor running with UnrealBridge plugin enabled?"
        )
        if args.json:
            print(json.dumps({"success": False, "error": msg}))
        else:
            print(f"ERROR: {msg}", file=sys.stderr)
        _audit(project_path, mode, src, code, ok=False, err=f"transport: {msg}")
        return 1
    except Exception as e:
        if args.json:
            print(json.dumps({"success": False, "error": str(e)}))
        else:
            print(f"ERROR: {e}", file=sys.stderr)
        _audit(project_path, mode, src, code, ok=False, err=f"transport: {e}")
        return 1

    if args.json:
        print(json.dumps(resp, ensure_ascii=False))
    else:
        output = resp.get("output", "")
        error = resp.get("error", "")

        if output:
            print(output, end="" if output.endswith("\n") else "\n")
        if error:
            print(error, file=sys.stderr, end="" if error.endswith("\n") else "\n")

    ok = bool(resp.get("success"))
    _audit(project_path, mode, src, code,
           ok=ok, err=(resp.get("error") or None) if not ok else None)
    return 0 if ok else 1


# ── CLI plumbing ────────────────────────────────────────────────────────

def _add_common_args(parser: argparse.ArgumentParser) -> None:
    """Shared discovery / override flags that every subcommand accepts."""
    parser.add_argument(
        "--endpoint",
        help="Skip discovery and connect directly to host:port "
             "(or set UNREAL_BRIDGE_ENDPOINT).",
    )
    parser.add_argument(
        "--project",
        help="Select an editor by project name / path when multiple are running "
             "(or set UNREAL_BRIDGE_PROJECT).",
    )
    parser.add_argument(
        "--token",
        help="Auth token (required when the server binds non-loopback; "
             "or set UNREAL_BRIDGE_TOKEN).",
    )
    parser.add_argument(
        "--discovery-timeout",
        type=int,
        help=f"Probe collection window in ms (default: {DEFAULT_DISCOVERY_TIMEOUT_MS}).",
    )
    parser.add_argument(
        "--discovery-group",
        help=f"Multicast group host:port "
             f"(default: {DEFAULT_DISCOVERY_GROUP}:{DEFAULT_DISCOVERY_PORT}).",
    )


def main():
    parser = argparse.ArgumentParser(
        prog="bridge",
        description="UnrealBridge — execute Python in Unreal Engine from the command line",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"Per-request timeout in seconds (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output in JSON format (machine-readable)",
    )
    parser.add_argument(
        "--no-preflight",
        action="store_true",
        help="Skip the AST preflight that validates unreal.UnrealBridge*Library "
             "calls against the manifest before sending. Use only when you "
             "intentionally want to bypass the contract (rare).",
    )
    _add_common_args(parser)

    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("ping", help="Check if UE is connected")

    exec_parser = subparsers.add_parser(
        "exec",
        help="Execute Python code (positional arg, --stdin, or `-` for stdin)",
    )
    exec_parser.add_argument(
        "code",
        nargs="?",
        help="Python code to execute. Pass `-` (or use --stdin) to read from stdin instead.",
    )
    exec_parser.add_argument(
        "--stdin",
        action="store_true",
        help="Read the script body from stdin (use for multi-line one-shot scripts; "
             "avoids creating a temp .py file).",
    )

    execfile_parser = subparsers.add_parser(
        "exec-file", help="Execute a Python script file"
    )
    execfile_parser.add_argument("file", help="Path to the Python script")

    subparsers.add_parser(
        "resume",
        help="Resume a paused BP breakpoint (bypasses the exec queue)",
    )

    gtp_parser = subparsers.add_parser(
        "gamethread-ping",
        help="Probe GameThread liveness (bypasses the exec queue)",
    )
    gtp_parser.add_argument(
        "--probe-timeout",
        type=float,
        default=2.0,
        help="Server-side wait for the GT to ack the probe (default: 2.0s, max 10.0s)",
    )

    subparsers.add_parser(
        "list-editors",
        help="Send a discovery probe and list every editor that answered",
    )

    pf_parser = subparsers.add_parser(
        "preflight",
        help="Lint a script for bridge-call errors WITHOUT sending it to UE",
    )
    pf_parser.add_argument(
        "file",
        help="Path to .py file (or `-` to read script from stdin).",
    )

    sg_parser = subparsers.add_parser(
        "suggest",
        help="Look up the bridge equivalent for a raw unreal.* fallback pattern",
    )
    sg_parser.add_argument(
        "pattern",
        nargs="?",
        help="Substring of a raw pattern (e.g. 'AssetRegistry', 'GameplayStatics'). "
             "Omit to list every redirect.",
    )

    wc_parser = subparsers.add_parser(
        "wait-compile",
        help="Poll shader-map readiness on a material / MI (client-side, "
             "safe for post-permutation-change sequencing)",
    )
    wc_parser.add_argument("material_path",
        help="Material or MI asset path, e.g. /Game/BridgeTemplates/MI_Foo")
    wc_parser.add_argument("--wait-timeout", type=float, default=120.0,
        help="Max total seconds to poll before giving up (default: 120)")
    wc_parser.add_argument("--poll-interval", type=float, default=0.5,
        help="Seconds between polls (default: 0.5). Between polls GT runs "
             "normal ticks so async compile completions drain.")
    wc_parser.add_argument("--feature-level", default="",
        help='Feature level: "SM5" / "SM6" / "ES3_1". Empty → editor\'s '
             'current max (SM6 on UE 5.7 DX12 by default).')
    wc_parser.add_argument("--quality", default="",
        help='Quality level: "Low" / "Medium" / "High" / "Epic". Empty → "High".')

    args = parser.parse_args()

    if args.command == "ping":
        sys.exit(cmd_ping(args))
    elif args.command == "exec":
        sys.exit(cmd_exec(args))
    elif args.command == "exec-file":
        sys.exit(cmd_exec_file(args))
    elif args.command == "resume":
        sys.exit(cmd_resume(args))
    elif args.command == "gamethread-ping":
        sys.exit(cmd_gt_ping(args))
    elif args.command == "wait-compile":
        sys.exit(cmd_wait_compile(args))
    elif args.command == "list-editors":
        sys.exit(cmd_list_editors(args))
    elif args.command == "preflight":
        sys.exit(cmd_preflight(args))
    elif args.command == "suggest":
        sys.exit(cmd_suggest(args))


if __name__ == "__main__":
    main()

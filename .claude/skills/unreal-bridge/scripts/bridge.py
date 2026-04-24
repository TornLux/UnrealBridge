#!/usr/bin/env python3
"""
UnrealBridge CLI — execute Python in a running Unreal Editor over a TCP bridge.

Usage:
    python bridge.py ping                          # Check connection
    python bridge.py exec "print('hello')"         # Execute inline code
    python bridge.py exec-file script.py           # Execute a file
    python bridge.py exec "code" --json            # Machine-readable output

The client auto-discovers the editor via UDP multicast (239.255.42.99:9876)
— zero config for the common case of one editor running on the local host.

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
import json
import os
import socket
import struct
import sys
import uuid

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


# ── Resolution: turn CLI args into a (host, port, token) triple ─────────────

def resolve_target(args) -> "tuple[str, int, str | None]":
    """Figure out which editor to talk to.

    Precedence:
      1. --endpoint=host:port (or UNREAL_BRIDGE_ENDPOINT)
      2. UDP multicast discovery, filtered by --project (or UNREAL_BRIDGE_PROJECT)
    """
    # 1. Explicit endpoint — no discovery.
    endpoint_str = getattr(args, "endpoint", None) or os.environ.get("UNREAL_BRIDGE_ENDPOINT")
    if endpoint_str:
        if ":" not in endpoint_str:
            raise SystemExit(f"--endpoint must be host:port (got {endpoint_str!r})")
        host, port_s = endpoint_str.rsplit(":", 1)
        token = getattr(args, "token", None) or os.environ.get("UNREAL_BRIDGE_TOKEN")
        return host, int(port_s), token

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
        return ep.host, ep.port, token
    except DiscoveryError as e:
        raise SystemExit(f"discovery: {e}")


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
    host, port, token = resolve_target(args)
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
    host, port, token = resolve_target(args)
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
    host, port, token = resolve_target(args)
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

    host, port, token = resolve_target(args)
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
    return _execute(args, args.code)


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

    return _execute(args, code)


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


def _execute(args, code: str) -> int:
    host, port, token = resolve_target(args)

    payload = {
        "id": str(uuid.uuid4()),
        "script": code,
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
        output = resp.get("output", "")
        error = resp.get("error", "")

        if output:
            print(output, end="" if output.endswith("\n") else "\n")
        if error:
            print(error, file=sys.stderr, end="" if error.endswith("\n") else "\n")

    return 0 if resp.get("success") else 1


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
    _add_common_args(parser)

    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("ping", help="Check if UE is connected")

    exec_parser = subparsers.add_parser("exec", help="Execute inline Python code")
    exec_parser.add_argument("code", help="Python code to execute")

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


if __name__ == "__main__":
    main()

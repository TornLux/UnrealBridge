#!/usr/bin/env python3
"""
UnrealBridge CLI - Send Python scripts to a running Unreal Engine instance.

Usage:
    python bridge.py ping                          # Check connection
    python bridge.py exec "print('hello')"         # Execute inline code
    python bridge.py exec-file script.py           # Execute a file
    python bridge.py exec "code" --json            # Machine-readable output

Protocol: Length-prefixed JSON over TCP.
    Request:  [4 bytes big-endian length][JSON payload]
    Response: [4 bytes big-endian length][JSON payload]
"""

import argparse
import json
import socket
import struct
import sys
import uuid


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9876
DEFAULT_TIMEOUT = 30


def send_request(host: str, port: int, payload: dict, timeout: float) -> dict:
    """Send a JSON request to the UE bridge server and return the response."""
    data = json.dumps(payload).encode("utf-8")
    header = struct.pack(">I", len(data))

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect((host, port))
        sock.sendall(header + data)

        # Read 4-byte length prefix
        resp_header = _recv_all(sock, 4)
        resp_len = struct.unpack(">I", resp_header)[0]

        # Read JSON payload
        resp_data = _recv_all(sock, resp_len)
        return json.loads(resp_data.decode("utf-8"))


def _recv_all(sock: socket.socket, num_bytes: int) -> bytes:
    """Receive exactly num_bytes from the socket."""
    chunks = []
    received = 0
    while received < num_bytes:
        chunk = sock.recv(num_bytes - received)
        if not chunk:
            raise ConnectionError("Connection closed by server")
        chunks.append(chunk)
        received += len(chunk)
    return b"".join(chunks)


def cmd_ping(args):
    """Check if the UE bridge server is running."""
    try:
        payload = {"id": str(uuid.uuid4()), "command": "ping"}
        resp = send_request(args.host, args.port, payload, args.timeout)

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
                print(f"Connected to UnrealBridge at {args.host}:{args.port}{suffix}")
            else:
                print(f"ERROR: Unexpected response: {resp}", file=sys.stderr)
                return 1
    except ConnectionRefusedError:
        if args.json:
            print(json.dumps({"success": False, "error": "Connection refused"}))
        else:
            print(
                f"ERROR: Cannot connect to {args.host}:{args.port}. "
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

    return 0


def cmd_gt_ping(args):
    """Probe whether the UE GameThread is responsive.

    Bypasses the FTSTicker-based exec queue: dispatches a no-op
    AsyncTask(GameThread) on the server side and waits up to
    --probe-timeout seconds. Use this from a *separate* terminal
    while a long `exec` is in flight to distinguish three states:

      - alive,  low latency : GT idle, queue healthy
      - alive,  high latency: GT mid-exec but pumping TaskGraph
      - unresponsive        : GT fully stuck (modal, deadlock, GIL loop)
    """
    payload = {
        "id": str(uuid.uuid4()),
        "command": "gamethread_ping",
        "timeout": args.probe_timeout,
    }
    try:
        # Connection-level timeout sits a bit above the server-side probe
        # timeout so we always get a structured "unresponsive" response
        # rather than a client-side socket timeout.
        resp = send_request(args.host, args.port, payload, args.probe_timeout + 3.0)
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
    """Unstick a paused Blueprint breakpoint.

    Goes through the bridge's server-level `debug_resume` command, which
    dispatches `FKismetDebugUtilities::RequestAbortingExecution` via AsyncTask.
    Unlike `exec`, this bypasses the FTSTicker-based Python exec queue and
    reaches the GameThread even while it's inside UE's nested Slate debug
    loop.
    """
    try:
        payload = {"id": str(uuid.uuid4()), "command": "debug_resume"}
        resp = send_request(args.host, args.port, payload, args.timeout)
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


def cmd_exec(args):
    """Execute inline Python code in UE."""
    return _execute(args, args.code)


def cmd_exec_file(args):
    """Execute a Python file in UE."""
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


def _execute(args, code: str) -> int:
    """Send code for execution and handle the response."""
    payload = {
        "id": str(uuid.uuid4()),
        "script": code,
        "timeout": args.timeout,
    }

    try:
        resp = send_request(args.host, args.port, payload, args.timeout + 5)
    except ConnectionRefusedError:
        msg = (
            f"Cannot connect to {args.host}:{args.port}. "
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


def main():
    parser = argparse.ArgumentParser(
        prog="bridge",
        description="UnrealBridge - Execute Python in Unreal Engine from the command line",
    )
    parser.add_argument(
        "--host", default=DEFAULT_HOST, help=f"Server host (default: {DEFAULT_HOST})"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"Server port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"Timeout in seconds (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output in JSON format (machine-readable)",
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    # ping
    subparsers.add_parser("ping", help="Check if UE is connected")

    # exec
    exec_parser = subparsers.add_parser("exec", help="Execute inline Python code")
    exec_parser.add_argument("code", help="Python code to execute")

    # exec-file
    execfile_parser = subparsers.add_parser(
        "exec-file", help="Execute a Python script file"
    )
    execfile_parser.add_argument("file", help="Path to the Python script")

    # resume — unstick a paused Blueprint breakpoint
    subparsers.add_parser(
        "resume",
        help="Resume a paused BP breakpoint (bypasses the exec queue)",
    )

    # gamethread-ping — probe GT liveness independently of the exec queue
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


if __name__ == "__main__":
    main()

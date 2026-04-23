"""UDP-multicast discovery for UnrealBridge.

Replaces the old "assume 127.0.0.1:9876" wiring: the client sends a single
probe to the multicast group 239.255.42.99:9876, every running editor on
the same host or subnet that has the UnrealBridge plugin loaded answers
with its project name + TCP bind + TCP port. The client picks one (single
match → auto, multiple → by --project filter or error).

Wire format:

    probe (client → group):
        {"v":1, "type":"probe",
         "request_id": "<uuid>",
         "filter": {"project": "<name|path|*>"}}

    response (server → probe source):
        {"v":1, "type":"response",
         "request_id": "<uuid>",
         "pid": 1234, "project": "MyGame",
         "project_path": "C:/.../MyGame.uproject",
         "engine_version": "5.7.0",
         "tcp_bind": "127.0.0.1", "tcp_port": 54321,
         "token_fingerprint": "a1b2c3d4e5f60718"}    # "" when no token
"""

from __future__ import annotations

import hashlib
import json
import os
import socket
import struct
import sys
import time
import uuid
from dataclasses import dataclass
from typing import Iterable, List, Optional, Tuple


DEFAULT_DISCOVERY_GROUP = "239.255.42.99"
DEFAULT_DISCOVERY_PORT = 9876
DEFAULT_DISCOVERY_TIMEOUT_MS = 800


@dataclass
class Endpoint:
    """One running UnrealBridge editor, as seen via discovery."""
    pid: int
    project: str
    project_path: str
    engine_version: str
    tcp_bind: str
    tcp_port: int
    token_fingerprint: str

    @property
    def host(self) -> str:
        """Best host to connect to — loopback if the server reported 0.0.0.0."""
        if self.tcp_bind in ("0.0.0.0", "::"):
            return "127.0.0.1"
        return self.tcp_bind

    @property
    def port(self) -> int:
        return self.tcp_port

    def __str__(self) -> str:
        token = " [token]" if self.token_fingerprint else ""
        return f"{self.project} @ {self.host}:{self.port} (pid {self.pid}){token}"


def _parse_group(group: str) -> Tuple[str, int]:
    """Parse 'addr:port' or 'addr' (uses default port) into a tuple."""
    if ":" in group:
        addr, port = group.rsplit(":", 1)
        return addr, int(port)
    return group, DEFAULT_DISCOVERY_PORT


def discover(project_filter: str = "*",
             group: str = DEFAULT_DISCOVERY_GROUP,
             group_port: int = DEFAULT_DISCOVERY_PORT,
             timeout_ms: int = DEFAULT_DISCOVERY_TIMEOUT_MS) -> List[Endpoint]:
    """Broadcast a probe to the discovery group; collect every response.

    Returns a list of Endpoint objects — empty if no editors responded.
    Never raises on "not found"; only on socket-level failures.
    """
    request_id = str(uuid.uuid4())
    probe_payload = json.dumps({
        "v": 1,
        "type": "probe",
        "request_id": request_id,
        "filter": {"project": project_filter or "*"},
    }).encode("utf-8")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
        # Bind ephemeral — responses arrive as unicast to this port.
        sock.bind(("0.0.0.0", 0))

        sock.sendto(probe_payload, (group, group_port))

        deadline = time.monotonic() + (timeout_ms / 1000.0)
        results: List[Endpoint] = []
        seen_pids: set = set()

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(remaining)
            try:
                data, _addr = sock.recvfrom(64 * 1024)
            except socket.timeout:
                break

            try:
                resp = json.loads(data.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                continue

            if resp.get("type") != "response":
                continue
            if resp.get("request_id") != request_id:
                continue
            pid = int(resp.get("pid", 0))
            if pid and pid in seen_pids:
                continue  # same editor answering on two interfaces — dedup
            seen_pids.add(pid)

            results.append(Endpoint(
                pid=pid,
                project=str(resp.get("project", "")),
                project_path=str(resp.get("project_path", "")),
                engine_version=str(resp.get("engine_version", "")),
                tcp_bind=str(resp.get("tcp_bind", "127.0.0.1")),
                tcp_port=int(resp.get("tcp_port", 0)),
                token_fingerprint=str(resp.get("token_fingerprint", "")),
            ))

        return results
    finally:
        sock.close()


def select(endpoints: List[Endpoint],
           project_filter: Optional[str] = None) -> Endpoint:
    """Choose one endpoint from a discovery result.

    - 0 endpoints → DiscoveryError("no editors found")
    - 1 endpoint  → that one (filter applied or not)
    - >1 endpoints with matching filter → ambiguity error listing candidates
    - >1 endpoints, filter narrows to one → that one
    """
    if not endpoints:
        raise DiscoveryError(
            "no UnrealBridge editors found on the LAN (multicast probe timed out). "
            "Check — in this order:\n"
            "  1. UE editor is running (and loaded past the splash screen).\n"
            "  2. The UnrealBridge plugin is installed in that project's "
            "Plugins/ folder AND enabled in the .uproject.\n"
            "  3. Multicast isn't being dropped by a VPN / virtual NIC — if "
            "everything else is fine, pass --endpoint=127.0.0.1:<port>, "
            "reading <port> from the editor log line "
            "`LogUnrealBridge: Listening on 127.0.0.1:<port>`."
        )

    if project_filter and project_filter != "*":
        matches = [
            e for e in endpoints
            if _matches_project(e, project_filter)
        ]
        if not matches:
            raise DiscoveryError(
                f"no editors matched --project={project_filter!r}. "
                f"Seen:\n  " + "\n  ".join(str(e) for e in endpoints))
        endpoints = matches

    if len(endpoints) == 1:
        return endpoints[0]

    raise DiscoveryError(
        f"{len(endpoints)} editors found — specify one with --project=<name|path>:\n  "
        + "\n  ".join(str(e) for e in endpoints)
    )


def _matches_project(ep: Endpoint, filter_str: str) -> bool:
    """Same matching rules as the C++ responder — case-insensitive, with
    support for name equality, full-path equality, path-suffix, and
    name-substring."""
    f = filter_str.lower()
    if not f or f == "*":
        return True
    if ep.project.lower() == f:
        return True
    path = ep.project_path.replace("\\", "/").lower()
    if path == f or path.endswith(f.replace("\\", "/")):
        return True
    if f in ep.project.lower():
        return True
    return False


def load_token(ep: Endpoint, explicit_token: Optional[str] = None) -> Optional[str]:
    """Resolve the token for the given endpoint, if one is needed.

    Priority: explicit CLI token → env UNREAL_BRIDGE_TOKEN →
    <Project>/Saved/UnrealBridge/token.txt (the path the server writes it to).
    Returns None if the server doesn't require a token
    (empty token_fingerprint).
    """
    if not ep.token_fingerprint:
        # Server didn't set a token — no auth needed.
        return None

    def _verify(token: str) -> Optional[str]:
        fp = hashlib.sha1(token.encode("utf-8")).hexdigest()[:16]
        if fp.lower() != ep.token_fingerprint.lower():
            return None
        return token

    if explicit_token:
        verified = _verify(explicit_token)
        if not verified:
            raise DiscoveryError(
                "--token doesn't match the server's token fingerprint. "
                "Check <Project>/Saved/UnrealBridge/token.txt for the current value."
            )
        return verified

    env_token = os.environ.get("UNREAL_BRIDGE_TOKEN")
    if env_token:
        verified = _verify(env_token)
        if verified:
            return verified

    # Fall back to the file the server writes.
    if ep.project_path:
        saved_dir = os.path.dirname(ep.project_path)
        token_file = os.path.join(saved_dir, "Saved", "UnrealBridge", "token.txt")
        if os.path.isfile(token_file):
            try:
                with open(token_file, "r", encoding="utf-8") as f:
                    file_token = f.read().strip()
                verified = _verify(file_token)
                if verified:
                    return verified
            except OSError:
                pass

    raise DiscoveryError(
        f"token required for {ep} but none found. "
        "Pass --token=<secret>, set UNREAL_BRIDGE_TOKEN, "
        "or ensure <Project>/Saved/UnrealBridge/token.txt is readable."
    )


class DiscoveryError(Exception):
    """Raised when discovery or endpoint selection fails."""


def _cli():
    """Small command-line driver: `python bridge_discovery.py` to list editors."""
    import argparse
    parser = argparse.ArgumentParser(
        description="List every UnrealBridge editor reachable via multicast discovery.")
    parser.add_argument("--project", default="*",
                        help="Filter by project name/path (default: all)")
    parser.add_argument("--group", default=DEFAULT_DISCOVERY_GROUP,
                        help="Multicast group address")
    parser.add_argument("--group-port", type=int, default=DEFAULT_DISCOVERY_PORT,
                        help="Multicast group port")
    parser.add_argument("--timeout-ms", type=int,
                        default=DEFAULT_DISCOVERY_TIMEOUT_MS,
                        help="Probe collection window (ms)")
    parser.add_argument("--json", action="store_true",
                        help="Emit endpoints as a JSON list")
    args = parser.parse_args()

    try:
        eps = discover(project_filter=args.project,
                       group=args.group, group_port=args.group_port,
                       timeout_ms=args.timeout_ms)
    except OSError as e:
        print(f"discovery failed: {e}", file=sys.stderr)
        sys.exit(2)

    if args.json:
        print(json.dumps([ep.__dict__ for ep in eps], indent=2))
        return

    if not eps:
        print("(no editors found)")
        sys.exit(1)

    for ep in eps:
        print(ep)


if __name__ == "__main__":
    _cli()

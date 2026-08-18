"""UDP discovery for UnrealBridge.

Replaces the old "assume 127.0.0.1:9876" wiring: the client sends the same
probe to the multicast group 239.255.42.99:9876 and to the local loopback
responder. Editors on the same host or subnet that have the UnrealBridge
plugin loaded answer with their project name + TCP bind + TCP port. The
client de-duplicates responses by Server-start instance UUID, then picks one (single match
→ auto, multiple → by --project filter or error).

Wire format:

    probe (client → group):
        {"v":2, "type":"probe",
         "request_id": "<uuid>",
         "filter": {"project": "<name|path|*>"}}

    response (server → probe source):
        {"v":2, "protocol_version":2, "type":"response",
         "request_id":"<uuid>", "instance_id":"<uuid>",
         "pid":1234, "project":"MyGame",
         "project_path": "C:/.../MyGame.uproject",
         "engine_version": "5.7.0",
         "tcp_bind": "127.0.0.1", "tcp_port": 54321,
         "token_fingerprint": "a1b2c3d4e5f60718",
         "capabilities": ["exact_exec", ...]}    # base exact set required; unique optional commands advertised

Malformed datagrams are discarded independently. For wildcard tcp_bind values,
the response source IP becomes the TCP host instead of client loopback.
"""

from __future__ import annotations

import hashlib
import ipaddress
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
LOCAL_DISCOVERY_HOST = "127.0.0.1"
PROTOCOL_VERSION = 2
EXACT_EDITOR_STATUS_CAPABILITY = "exact_editor_status"
EXACT_CAPABILITIES = (
    "exact_exec", "exact_ping", EXACT_EDITOR_STATUS_CAPABILITY,
    "exact_gamethread_ping", "exact_debug_resume", "exact_modal_status",
    "exact_modal_action",
)
# protocol-v2 的既有基础命令仍是 discovery 最低门槛；新增只读命令按 capability 单独协商，
# 避免新版客户端把仍可安全执行基础命令的旧 v2 endpoint 整体丢弃。
# Existing protocol-v2 base commands remain the discovery floor; new read-only
# commands are negotiated per capability so newer clients retain safe base access.
REQUIRED_EXACT_CAPABILITIES = tuple(
    capability for capability in EXACT_CAPABILITIES
    if capability != EXACT_EDITOR_STATUS_CAPABILITY
)


@dataclass
class Endpoint:
    """discovery 冻结的一次精确 Server 启动。 / One exact Server start frozen from discovery."""
    protocol_version: int
    instance_id: str
    pid: int
    project: str
    project_path: str
    engine_version: str
    tcp_bind: str
    tcp_port: int
    token_fingerprint: str
    capabilities: Tuple[str, ...]
    response_host: str

    @property
    def host(self) -> str:
        """通配 bind 使用响应源 IP；否则使用广告地址。 / Use response source IP for wildcard binds, otherwise the advertised address."""
        if self.tcp_bind in ("0.0.0.0", "::"):
            return self.response_host
        return self.tcp_bind

    @property
    def port(self) -> int:
        return self.tcp_port

    def __str__(self) -> str:
        token = " [token]" if self.token_fingerprint else ""
        return (f"{self.project} @ {self.host}:{self.port} "
                f"(pid {self.pid}, instance {self.instance_id}){token}")


def _parse_group(group: str) -> Tuple[str, int]:
    """Parse 'addr:port' or 'addr' (uses default port) into a tuple."""
    if ":" in group:
        addr, port = group.rsplit(":", 1)
        return addr, int(port)
    return group, DEFAULT_DISCOVERY_PORT


def _parse_endpoint_response(resp, request_id: str, response_host: str) -> "Endpoint | None":
    """严格解析单个响应；畸形数据只丢弃当前 datagram。 / Strictly parse one response; malformed data drops only that datagram."""
    if not isinstance(resp, dict):
        return None
    if type(resp.get("v")) is not int or resp["v"] != PROTOCOL_VERSION:
        return None
    if (type(resp.get("protocol_version")) is not int
            or resp["protocol_version"] != PROTOCOL_VERSION):
        return None
    if resp.get("type") != "response" or resp.get("request_id") != request_id:
        return None

    instance_id = resp.get("instance_id")
    if not isinstance(instance_id, str) or not instance_id:
        return None
    try:
        parsed_uuid = uuid.UUID(instance_id)
    except (ValueError, AttributeError, TypeError):
        return None
    if str(parsed_uuid) != instance_id.lower() or instance_id != instance_id.lower():
        return None

    pid = resp.get("pid")
    tcp_port = resp.get("tcp_port")
    if type(pid) is not int or not (1 <= pid <= 2_147_483_647):
        return None
    if type(tcp_port) is not int or not (1 <= tcp_port <= 65_535):
        return None

    required_strings = ("project", "project_path", "engine_version", "tcp_bind", "token_fingerprint")
    if any(not isinstance(resp.get(field), str) for field in required_strings):
        return None
    project = resp["project"]
    project_path = resp["project_path"]
    engine_version = resp["engine_version"]
    tcp_bind = resp["tcp_bind"]
    token_fingerprint = resp["token_fingerprint"]
    if not project or not project_path or not engine_version or not tcp_bind:
        return None
    try:
        ipaddress.ip_address(tcp_bind)
        ipaddress.ip_address(response_host)
    except ValueError:
        return None
    if token_fingerprint:
        if (len(token_fingerprint) != 16
                or token_fingerprint != token_fingerprint.lower()
                or any(ch not in "0123456789abcdef" for ch in token_fingerprint)):
            return None

    raw_capabilities = resp.get("capabilities")
    if not isinstance(raw_capabilities, list):
        return None
    if any(not isinstance(item, str) or not item for item in raw_capabilities):
        return None
    if len(set(raw_capabilities)) != len(raw_capabilities):
        return None
    capabilities = tuple(raw_capabilities)
    if not set(REQUIRED_EXACT_CAPABILITIES).issubset(capabilities):
        return None

    return Endpoint(
        protocol_version=PROTOCOL_VERSION,
        instance_id=instance_id,
        pid=pid,
        project=project,
        project_path=project_path,
        engine_version=engine_version,
        tcp_bind=tcp_bind,
        tcp_port=tcp_port,
        token_fingerprint=token_fingerprint,
        capabilities=capabilities,
        response_host=response_host,
    )


def discover(project_filter: str = "*",
             group: str = DEFAULT_DISCOVERY_GROUP,
             group_port: int = DEFAULT_DISCOVERY_PORT,
             timeout_ms: int = DEFAULT_DISCOVERY_TIMEOUT_MS) -> List[Endpoint]:
    """Probe the multicast group and local loopback; collect every response.

    Returns a list of Endpoint objects — empty if no editors responded.
    A failure on one send path does not suppress the other path. Never raises
    on "not found"; only when every probe send fails at the socket level.
    """
    request_id = str(uuid.uuid4())
    probe_payload = json.dumps({
        "v": PROTOCOL_VERSION,
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

        # Windows multicast loopback can be silently dropped by VPNs, virtual
        # NICs, or Public firewall policy even while LAN multicast remains
        # useful. Send the identical request from the same ephemeral socket to
        # loopback as a local-only fallback. Both responders reply to this
        # socket and the collection loop de-duplicates them by Server-start UUID.
        probe_targets = [(group, group_port)]
        loopback_target = (LOCAL_DISCOVERY_HOST, group_port)
        if loopback_target not in probe_targets:
            probe_targets.append(loopback_target)

        send_errors: List[OSError] = []
        for target in probe_targets:
            try:
                sock.sendto(probe_payload, target)
            except OSError as error:
                send_errors.append(error)

        if len(send_errors) == len(probe_targets):
            raise send_errors[-1]

        deadline = time.monotonic() + (timeout_ms / 1000.0)
        results: List[Endpoint] = []
        seen_instances: set = set()

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(remaining)
            try:
                data, source_addr = sock.recvfrom(64 * 1024)
            except socket.timeout:
                break

            try:
                resp = json.loads(data.decode("utf-8"))
                response_host = source_addr[0]
                endpoint = _parse_endpoint_response(resp, request_id, response_host)
            except (UnicodeDecodeError, json.JSONDecodeError, TypeError, ValueError, OverflowError, IndexError):
                endpoint = None
            if endpoint is None:
                continue
            if endpoint.instance_id in seen_instances:
                # 同一次 Server 启动可能同时经 multicast 与 loopback 响应。
                # One Server start may answer through both multicast and loopback.
                continue
            seen_instances.add(endpoint.instance_id)
            results.append(endpoint)

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
            "no UnrealBridge editors found (multicast and local probes timed out). "
            "Check — in this order:\n"
            "  1. UE editor is running (and loaded past the splash screen).\n"
            "  2. The UnrealBridge plugin is installed in that project's "
            "Plugins/ folder AND enabled in the .uproject.\n"
            "  3. UDP discovery isn't being blocked locally — if everything "
            "else is fine, direct mode requires the complete --endpoint, "
            "--instance-id, --expected-pid, and --expected-project-path tuple "
            "printed by the Server startup log."
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
        description="List every UnrealBridge editor reachable via UDP discovery.")
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

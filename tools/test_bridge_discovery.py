"""Focused stdlib tests for UnrealBridge UDP discovery."""

from __future__ import annotations

import importlib.util
import json
import socket
import sys
import unittest
from pathlib import Path
from unittest import mock


MODULE_PATH = (
    Path(__file__).resolve().parents[1]
    / ".claude" / "skills" / "unreal-bridge" / "scripts" / "bridge_discovery.py"
)
SPEC = importlib.util.spec_from_file_location("bridge_discovery", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot load discovery module from {MODULE_PATH}")
bridge_discovery = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge_discovery
SPEC.loader.exec_module(bridge_discovery)


class FakeDiscoverySocket:
    def __init__(self, *, fail_hosts=(), response_count=1, response_overrides=None,
                 response_sequence=None, source_host="127.0.0.1"):
        self.fail_hosts = set(fail_hosts)
        self.response_overrides = dict(response_overrides or {})
        self.response_sequence = (list(response_sequence)
                                  if response_sequence is not None
                                  else [dict(self.response_overrides) for _ in range(response_count)])
        self.source_host = source_host
        self.request_id = ""
        self.targets = []
        self.closed = False

    def setsockopt(self, *_args):
        pass

    def bind(self, _address):
        pass

    def sendto(self, payload, target):
        self.targets.append(target)
        self.request_id = json.loads(payload.decode("utf-8"))["request_id"]
        if target[0] in self.fail_hosts:
            raise OSError(f"simulated send failure for {target[0]}")

    def settimeout(self, _timeout):
        pass

    def _valid_response(self):
        return {
            "v": bridge_discovery.PROTOCOL_VERSION,
            "protocol_version": bridge_discovery.PROTOCOL_VERSION,
            "type": "response",
            "request_id": self.request_id,
            "instance_id": "11111111-2222-4333-8444-555555555555",
            "pid": 4242,
            "project": "TestProject",
            "project_path": "C:/Projects/TestProject/TestProject.uproject",
            "engine_version": "5.7.0",
            "tcp_bind": "127.0.0.1",
            "tcp_port": 32123,
            "token_fingerprint": "",
            "capabilities": list(bridge_discovery.EXACT_CAPABILITIES),
        }

    def recvfrom(self, _max_size):
        if not self.response_sequence:
            raise socket.timeout()
        item = self.response_sequence.pop(0)
        if isinstance(item, bytes):
            payload = item
        elif isinstance(item, tuple) and item[0] == "root":
            payload = json.dumps(item[1]).encode("utf-8")
        else:
            response = self._valid_response()
            response.update(item)
            payload = json.dumps(response).encode("utf-8")
        return payload, (self.source_host, 9876)

    def close(self):
        self.closed = True


class DiscoveryTests(unittest.TestCase):
    def run_discovery(self, fake_socket):
        with mock.patch.object(bridge_discovery.socket, "socket", return_value=fake_socket):
            return bridge_discovery.discover(timeout_ms=10)

    def test_multicast_and_loopback_responses_are_deduplicated_by_instance(self):
        fake_socket = FakeDiscoverySocket(response_count=2)
        endpoints = self.run_discovery(fake_socket)
        self.assertEqual(fake_socket.targets, [
            (bridge_discovery.DEFAULT_DISCOVERY_GROUP, 9876),
            (bridge_discovery.LOCAL_DISCOVERY_HOST, 9876),
        ])
        self.assertEqual([endpoint.pid for endpoint in endpoints], [4242])
        self.assertTrue(fake_socket.closed)

    def test_wildcard_bind_uses_remote_response_source(self):
        fake_socket = FakeDiscoverySocket(
            response_overrides={"tcp_bind": "0.0.0.0"},
            source_host="192.0.2.10",
        )
        endpoints = self.run_discovery(fake_socket)
        self.assertEqual(len(endpoints), 1)
        self.assertEqual(endpoints[0].response_host, "192.0.2.10")
        self.assertEqual(endpoints[0].host, "192.0.2.10")

    def test_loopback_still_discovers_when_multicast_send_fails(self):
        fake_socket = FakeDiscoverySocket(fail_hosts={bridge_discovery.DEFAULT_DISCOVERY_GROUP})
        self.assertEqual([endpoint.tcp_port for endpoint in self.run_discovery(fake_socket)], [32123])

    def test_pid_reuse_with_two_instance_ids_stays_ambiguous(self):
        fields = dict(
            protocol_version=bridge_discovery.PROTOCOL_VERSION,
            pid=4242,
            project="TestProject",
            project_path="C:/Projects/TestProject/TestProject.uproject",
            engine_version="5.7.0",
            tcp_bind="127.0.0.1",
            tcp_port=32123,
            token_fingerprint="",
            capabilities=bridge_discovery.EXACT_CAPABILITIES,
            response_host="127.0.0.1",
        )
        endpoints = [
            bridge_discovery.Endpoint(instance_id="old-instance", **fields),
            bridge_discovery.Endpoint(instance_id="new-instance", **fields),
        ]
        with self.assertRaises(bridge_discovery.DiscoveryError):
            bridge_discovery.select(endpoints, project_filter="TestProject")

    def test_malformed_datagrams_do_not_suppress_later_valid_response(self):
        malformed = [
            ("root", []),
            b"\xff",
            {"v": True},
            {"protocol_version": 2.0},
            {"pid": "bad"},
            {"pid": 0},
            {"pid": 2_147_483_648},
            {"pid": float("nan")},
            {"tcp_port": []},
            {"tcp_port": 32123.5},
            {"tcp_port": 0},
            {"instance_id": "not-a-uuid"},
            {"instance_id": "11111111-2222-4333-8444-55555555555Z"},
            {"project_path": []},
            {"tcp_bind": "not-an-ip"},
            {"capabilities": "exact_exec"},
            {"capabilities": list(bridge_discovery.EXACT_CAPABILITIES) + [7]},
            {"capabilities": list(bridge_discovery.EXACT_CAPABILITIES) + ["exact_exec"]},
            {"token_fingerprint": "NOT-HEX"},
            {},
        ]
        endpoints = self.run_discovery(FakeDiscoverySocket(response_sequence=malformed))
        self.assertEqual(len(endpoints), 1)
        self.assertEqual(endpoints[0].instance_id, "11111111-2222-4333-8444-555555555555")

    def test_minimum_capabilities_allow_unique_forward_compatible_extras(self):
        capabilities = list(bridge_discovery.EXACT_CAPABILITIES) + ["exact_future_read"]
        endpoints = self.run_discovery(FakeDiscoverySocket(
            response_overrides={"capabilities": capabilities}
        ))
        self.assertEqual(endpoints[0].capabilities, tuple(capabilities))

    def test_legacy_and_incomplete_v2_responses_are_ignored(self):
        fake_socket = FakeDiscoverySocket(response_sequence=[
            {"v": 1, "protocol_version": 1},
            {"instance_id": ""},
        ])
        self.assertEqual(self.run_discovery(fake_socket), [])

    def test_discovery_raises_only_when_every_send_path_fails(self):
        fake_socket = FakeDiscoverySocket(
            fail_hosts={bridge_discovery.DEFAULT_DISCOVERY_GROUP,
                        bridge_discovery.LOCAL_DISCOVERY_HOST},
            response_count=0,
        )
        with self.assertRaises(OSError):
            self.run_discovery(fake_socket)
        self.assertTrue(fake_socket.closed)


if __name__ == "__main__":
    unittest.main()

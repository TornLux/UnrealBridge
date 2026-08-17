"""Deterministic client and source-contract tests for exact endpoint identity."""

from __future__ import annotations

import importlib.util
import json
import struct
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

MODULE_PATH = (Path(__file__).resolve().parents[1] / ".claude" / "skills" /
               "unreal-bridge" / "scripts" / "bridge.py")
SPEC = importlib.util.spec_from_file_location("unreal_bridge_cli_identity_tests", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot load bridge CLI from {MODULE_PATH}")
bridge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


def identity(instance_id="11111111-2222-4333-8444-555555555555", project_path=None):
    return bridge.Endpoint(
        protocol_version=bridge.PROTOCOL_VERSION,
        instance_id=instance_id,
        pid=4242,
        project="TestProject",
        project_path=(project_path
                      if project_path is not None
                      else "C:/Projects/TestProject/TestProject.uproject"),
        engine_version="5.7.0",
        tcp_bind="127.0.0.1",
        tcp_port=32123,
        token_fingerprint="",
        capabilities=bridge.EXACT_CAPABILITIES,
        response_host="127.0.0.1",
    )


def valid_response(ep=None):
    ep = ep or identity()
    return {
        "success": True, "output": "pong", "error": "", "ready": True,
        "protocol_version": ep.protocol_version,
        "instance_id": ep.instance_id,
        "pid": ep.pid,
        "project_path": ep.project_path,
    }


class FakeTcpSocket:
    def __init__(self, response=None, *, body=None, declared_length=None):
        if body is None:
            body = json.dumps(response).encode("utf-8")
        length = len(body) if declared_length is None else declared_length
        self.incoming = bytearray(struct.pack(">I", length) + body)
        self.sent = bytearray()

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def settimeout(self, _timeout):
        pass

    def connect(self, _endpoint):
        pass

    def sendall(self, data):
        self.sent.extend(data)

    def recv(self, size):
        chunk = self.incoming[:size]
        del self.incoming[:size]
        return bytes(chunk)


class EndpointIdentityTests(unittest.TestCase):
    def send_with_socket(self, fake_socket, ep=None):
        ep = ep or identity()
        with mock.patch.object(bridge.socket, "socket", return_value=fake_socket):
            return bridge.send_request("127.0.0.1", 32123,
                                       {"command": "ping"}, 1, identity=ep)

    def test_cpp_and_python_protocol_constants_match(self):
        header = (Path(__file__).resolve().parents[1] / "Plugin" / "UnrealBridge" /
                  "Source" / "UnrealBridge" / "Private" /
                  "UnrealBridgeProtocol.h").read_text(encoding="utf-8")
        self.assertIn(f"constexpr int32 Version = {bridge.PROTOCOL_VERSION};", header)
        for capability in bridge.EXACT_CAPABILITIES:
            self.assertIn(f'TEXT("{capability}")', header)

    def test_production_handler_uses_exact_dispatcher_as_secondary_source_check(self):
        source = (Path(__file__).resolve().parents[1] / "Plugin" / "UnrealBridge" /
                  "Source" / "UnrealBridge" / "Private" /
                  "UnrealBridgeServer.cpp").read_text(encoding="utf-8")
        handler = source[source.index("void FUnrealBridgeServer::HandleClient"):
                         source.index("// Python execution pipeline")]
        dispatcher = handler.index("FUnrealBridgeExactRequestDispatcher::TryDispatch")
        first_command_body = handler.index("EUnrealBridgeExactCommand::Ping")
        self.assertLess(dispatcher, first_command_body)
        self.assertIn("if (!bExactRequestValid)", handler[dispatcher:first_command_body])
        self.assertIn("ResponseIdentity.AppendToResponse(Response)", handler)

    def test_exec_wire_is_nested_and_preserves_exact_project_identity(self):
        ep = identity(project_path="C:\\Projects\\Exact\\TestProject.uproject")
        wire = bridge._build_exact_payload({
            "id": "request-1", "script": "SIDE_EFFECT()", "timeout": 3,
        }, ep)
        self.assertEqual(wire["command"], "exact_exec")
        self.assertNotIn("script", wire)
        self.assertEqual(wire["request"]["script"], "SIDE_EFFECT()")
        self.assertEqual(wire["expected"]["project_path"], ep.project_path)

    def test_every_command_uses_exact_wire_form(self):
        for command in ("ping", "editor_status", "gamethread_ping", "debug_resume",
                        "modal_status", "modal_action"):
            with self.subTest(command=command):
                wire = bridge._build_exact_payload({"command": command}, identity())
                self.assertEqual(wire["command"], f"exact_{command}")
                self.assertEqual(wire["request"], {})

    def test_correct_identity_round_trip_succeeds(self):
        ep = identity()
        sock = FakeTcpSocket(valid_response(ep))
        result = self.send_with_socket(sock, ep)
        self.assertTrue(result["success"])
        sent_length = struct.unpack(">I", sock.sent[:4])[0]
        sent = json.loads(sock.sent[4:4 + sent_length].decode("utf-8"))
        self.assertEqual(sent["command"], "exact_ping")

    def test_response_identity_requires_exact_string_and_integer_types(self):
        ep = identity()
        cases = [
            {**valid_response(ep), "project_path": ep.project_path.lower()},
            {**valid_response(ep), "project_path": ep.project_path.replace("/", "\\")},
            {**valid_response(ep), "pid": float(ep.pid)},
            {**valid_response(ep), "protocol_version": float(ep.protocol_version)},
            {**valid_response(ep), "instance_id": None},
        ]
        for response in cases:
            with self.subTest(response=response):
                with self.assertRaises(bridge.EndpointIdentityError):
                    bridge._verify_response_identity(response, ep)

    def test_stale_instance_and_wrong_project_are_rejected(self):
        stale = {**valid_response(), "instance_id": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"}
        wrong_project = {**valid_response(), "project_path": "C:/Other/Other.uproject"}
        for response in (stale, wrong_project):
            with self.assertRaises(bridge.EndpointIdentityError):
                bridge._verify_response_identity(response, identity())

    def test_legacy_response_and_missing_identity_fail_without_fallback(self):
        with self.assertRaises(bridge.EndpointIdentityError):
            bridge._verify_response_identity({"success": True, "output": "pong"}, identity())
        with self.assertRaises(bridge.EndpointIdentityError):
            bridge.send_request("127.0.0.1", 32123, {"command": "ping"}, 1)

    def test_direct_endpoint_requires_and_preserves_inseparable_identity_tuple(self):
        missing = SimpleNamespace(endpoint="127.0.0.1:32123", token=None,
                                  instance_id=None, expected_project_path=None,
                                  expected_pid=None)
        with mock.patch.dict(bridge.os.environ, {}, clear=True):
            with self.assertRaises(SystemExit) as raised:
                bridge.resolve_target(missing)
        self.assertIn("direct legacy writes are not allowed", str(raised.exception))

        exact_path = "C:\\Projects\\ExactCase\\TestProject.uproject"
        args = SimpleNamespace(
            endpoint="192.0.2.10:32123", token="secret",
            instance_id=identity().instance_id,
            expected_project_path=exact_path,
            expected_pid=4242,
        )
        host, port, token, project_path, ep = bridge.resolve_target(args)
        self.assertEqual((host, port, token), ("192.0.2.10", 32123, "secret"))
        self.assertEqual(project_path, exact_path)
        self.assertEqual(ep.project_path, exact_path)

    def test_response_frame_length_boundaries(self):
        for accepted in (1, bridge.MAX_RESPONSE_FRAME_BYTES):
            bridge._validate_response_frame_length(accepted)
        for rejected in (0, bridge.MAX_RESPONSE_FRAME_BYTES + 1, 0xFFFFFFFF):
            with self.assertRaises(bridge.BridgeProtocolError):
                bridge._validate_response_frame_length(rejected)

    def test_oversized_frame_is_rejected_before_body_read(self):
        sock = FakeTcpSocket(body=b"", declared_length=bridge.MAX_RESPONSE_FRAME_BYTES + 1)
        with self.assertRaises(bridge.BridgeProtocolError):
            self.send_with_socket(sock)
        self.assertEqual(len(sock.incoming), 0)

    def test_truncated_invalid_utf8_invalid_json_and_non_object_frames_fail(self):
        cases = [
            (FakeTcpSocket(body=b"{}", declared_length=20), ConnectionError),
            (FakeTcpSocket(body=b"\xff"), bridge.BridgeProtocolError),
            (FakeTcpSocket(body=b"{"), bridge.BridgeProtocolError),
            (FakeTcpSocket(body=b"[]"), bridge.BridgeProtocolError),
        ]
        for sock, error_type in cases:
            with self.subTest(error_type=error_type):
                with self.assertRaises(error_type):
                    self.send_with_socket(sock)


if __name__ == "__main__":
    unittest.main()

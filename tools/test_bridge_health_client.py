"""Focused stdlib tests for the cached editor-status CLI."""

from __future__ import annotations

import copy
import importlib.util
import io
import json
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


MODULE_PATH = (
    Path(__file__).resolve().parents[1]
    / ".claude"
    / "skills"
    / "unreal-bridge"
    / "scripts"
    / "bridge.py"
)
SPEC = importlib.util.spec_from_file_location("unreal_bridge_cli_for_health_tests", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot load bridge CLI from {MODULE_PATH}")
bridge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


def sample_health() -> dict:
    return {
        "success": True,
        "output": "slate_modal",
        "error": "",
        "ready": True,
        "ui_state": "slate_modal",
        "editor_status": {
            "schema_version": 1,
            "editor_ready": True,
            "stale": True,
            "stale_after_ms": 2000.0,
            "attention_required": True,
            "engine_tick_sequence": 12,
            "last_engine_tick_utc": "2026-08-08T00:00:00Z",
            "engine_tick_age_ms": 2300.0,
            "engine_stale": True,
            "slate_tick_sequence": 42,
            "last_slate_tick_utc": "2026-08-08T00:00:02Z",
            "slate_tick_age_ms": 4.5,
            "slate_stale": False,
            "ui_state": "slate_modal",
            "attention_id": 7,
            "active_modal": {
                "present": True,
                "snapshot_id": "0123456789abcdef",
                "title": "Message",
                "first_seen_utc": "2026-08-08T00:00:01Z",
                "button_count": 1,
                "input_count": 0,
                "checkbox_count": 1,
            },
        },
    }


def endpoint_identity(capabilities=None):
    return bridge.Endpoint(
        protocol_version=bridge.PROTOCOL_VERSION,
        instance_id="11111111-2222-4333-8444-555555555555",
        pid=4242,
        project="TestProject",
        project_path="C:/Projects/TestProject/TestProject.uproject",
        engine_version="5.7.0",
        tcp_bind="127.0.0.1",
        tcp_port=12345,
        token_fingerprint="",
        capabilities=(bridge.EXACT_CAPABILITIES
                      if capabilities is None else tuple(capabilities)),
        response_host="127.0.0.1",
    )


class HealthClientTests(unittest.TestCase):
    def test_human_render_preserves_v1_status_fields(self):
        rendered = "\n".join(bridge._status_lines(sample_health()))

        self.assertIn("ui=slate_modal", rendered)
        self.assertIn("slate_age_ms=4.5", rendered)
        self.assertIn("engine_age_ms=2300.0", rendered)
        self.assertIn("attention=7", rendered)
        self.assertIn("modal_title='Message'", rendered)

    def test_never_observed_age_preserves_v1_minus_one_sentinel(self):
        response = sample_health()
        response["editor_status"]["engine_tick_age_ms"] = -1.0

        rendered = "\n".join(bridge._status_lines(response))

        self.assertIn("engine_age_ms=-1.0", rendered)

    def test_command_selects_exact_cached_status_with_frozen_identity(self):
        args = SimpleNamespace(timeout=3.0, json=True)
        identity = endpoint_identity()
        stdout = io.StringIO()
        with mock.patch.object(
            bridge,
            "resolve_target",
            return_value=("127.0.0.1", 12345, None, None, identity),
        ), mock.patch.object(
            bridge, "send_request", return_value=sample_health()
        ) as send, redirect_stdout(stdout):
            exit_code = bridge.cmd_status(args)

        payload = send.call_args.args[2]
        self.assertEqual(exit_code, 0)
        self.assertEqual(payload["command"], "editor_status")
        self.assertNotIn("script", payload)
        self.assertIs(send.call_args.kwargs["identity"], identity)
        wire = bridge._build_exact_payload(payload, identity)
        self.assertEqual(wire["command"], "exact_editor_status")
        self.assertEqual(wire["request"], {})
        self.assertEqual(wire["expected"]["instance_id"], identity.instance_id)
        self.assertTrue(json.loads(stdout.getvalue())["success"])

    def test_status_requires_its_capability_without_hiding_base_endpoint(self):
        args = SimpleNamespace(timeout=3.0, json=True)
        base_capabilities = tuple(
            capability for capability in bridge.EXACT_CAPABILITIES
            if capability != bridge.EXACT_EDITOR_STATUS_CAPABILITY
        )
        identity = endpoint_identity(base_capabilities)
        stdout = io.StringIO()
        with mock.patch.object(
            bridge,
            "resolve_target",
            return_value=("127.0.0.1", 12345, None, None, identity),
        ), mock.patch.object(bridge, "send_request") as send, redirect_stdout(stdout):
            exit_code = bridge.cmd_status(args)

        send.assert_not_called()
        rendered = json.loads(stdout.getvalue())
        self.assertEqual(exit_code, 1)
        self.assertFalse(rendered["success"])
        self.assertIn("does not advertise exact_editor_status", rendered["error"])

    def test_successful_status_rejects_wrong_schema_and_field_types(self):
        args = SimpleNamespace(timeout=3.0, json=True)
        identity = endpoint_identity()
        cases = []

        missing = sample_health()
        missing.pop("editor_status")
        cases.append((missing, "missing editor_status"))

        wrong_version = sample_health()
        wrong_version["editor_status"]["schema_version"] = 2
        cases.append((wrong_version, "schema_version"))

        wrong_age = sample_health()
        wrong_age["editor_status"]["engine_tick_age_ms"] = "fresh"
        cases.append((wrong_age, "engine_tick_age_ms"))

        fractional_negative_age = sample_health()
        fractional_negative_age["editor_status"]["engine_tick_age_ms"] = -0.5
        cases.append((fractional_negative_age, "engine_tick_age_ms"))

        wrong_count = sample_health()
        wrong_count["editor_status"]["active_modal"]["button_count"] = True
        cases.append((wrong_count, "button_count"))

        for malformed, expected_error in cases:
            with self.subTest(expected_error=expected_error):
                stdout = io.StringIO()
                with mock.patch.object(
                    bridge,
                    "resolve_target",
                    return_value=("127.0.0.1", 12345, None, None, identity),
                ), mock.patch.object(
                    bridge, "send_request", return_value=copy.deepcopy(malformed)
                ), redirect_stdout(stdout):
                    exit_code = bridge.cmd_status(args)

                rendered = json.loads(stdout.getvalue())
                self.assertEqual(exit_code, 1)
                self.assertFalse(rendered["success"])
                self.assertIn(expected_error, rendered["error"])


if __name__ == "__main__":
    unittest.main()

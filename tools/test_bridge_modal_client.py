"""Focused stdlib tests for UnrealBridge modal diagnostics and redaction."""

from __future__ import annotations

import importlib.util
import io
import json
import sys
import unittest
from contextlib import ExitStack, redirect_stdout
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
SPEC = importlib.util.spec_from_file_location("unreal_bridge_cli_for_modal_tests", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot load bridge CLI from {MODULE_PATH}")
bridge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


def sample_modal() -> dict:
    return {
        "present": True,
        "snapshot_id": "0123456789abcdef",
        "title": "Message",
        "body": "The asset path has no valid root.",
        "buttons": [{"id": 0, "label": "OK", "enabled": True, "visible": True}],
        "inputs": [
            {
                "id": 0,
                "kind": "single_line",
                "password": True,
                "value": "must-never-be-rendered",
                "read_only": False,
                "enabled": True,
            }
        ],
        "checkboxes": [{"id": 0, "label": "Remember", "state": "unchecked"}],
    }


class ModalClientTests(unittest.TestCase):
    def test_human_render_redacts_password_even_if_server_value_is_wrong(self):
        rendered = "\n".join(bridge._modal_lines(sample_modal()))

        self.assertIn("Message", rendered)
        self.assertIn("[0] OK", rendered)
        self.assertIn("<redacted>", rendered)
        self.assertNotIn("must-never-be-rendered", rendered)

    def test_timeout_diagnostic_is_structured_and_action_neutral(self):
        response = {"success": False, "error": "exec timeout after 2.0s"}

        bridge._attach_modal_diagnostic(response, sample_modal())

        self.assertTrue(response["blocked_by_modal"])
        self.assertEqual(response["modal"]["snapshot_id"], "0123456789abcdef")
        self.assertIn("Message", response["error"])
        self.assertIn("Inspect/refresh", response["error"])
        self.assertNotIn("modal-click", response["error"])
        self.assertNotIn("must-never-be-rendered", response["error"])

    def test_probe_uses_modal_bypass_command(self):
        response = {"success": True, "modal": sample_modal()}
        with mock.patch.object(bridge, "send_request", return_value=response) as send:
            modal = bridge._probe_modal("127.0.0.1", 12345, None)

        self.assertEqual(modal["title"], "Message")
        payload = send.call_args.args[2]
        self.assertEqual(payload["command"], "modal_status")
        self.assertNotIn("script", payload)

    def test_absent_modal_does_not_relabel_timeout(self):
        response = {"success": False, "error": "exec timeout after 2.0s"}

        bridge._attach_modal_diagnostic(response, {"present": False})

        self.assertNotIn("blocked_by_modal", response)
        self.assertNotIn("modal", response)

    def test_exec_timeout_automatically_attaches_modal_snapshot(self):
        args = SimpleNamespace(no_preflight=True, timeout=2, json=True)
        server_timeout = {
            "success": False,
            "error": "exec timeout after 2.0s",
            "output": "",
        }
        stdout = io.StringIO()

        with ExitStack() as stack:
            stack.enter_context(mock.patch.object(
                bridge, "resolve_target", return_value=("127.0.0.1", 12345, None, None)
            ))
            stack.enter_context(mock.patch.object(
                bridge, "send_request", return_value=server_timeout
            ))
            probe = stack.enter_context(mock.patch.object(
                bridge, "_probe_modal", return_value=sample_modal()
            ))
            stack.enter_context(mock.patch.object(bridge, "_audit"))
            stack.enter_context(redirect_stdout(stdout))
            exit_code = bridge._execute(args, "print('test')")

        rendered = json.loads(stdout.getvalue())
        self.assertEqual(exit_code, 1)
        self.assertTrue(rendered["blocked_by_modal"])
        self.assertEqual(rendered["modal"]["title"], "Message")
        probe.assert_called_once_with("127.0.0.1", 12345, None)


if __name__ == "__main__":
    unittest.main()

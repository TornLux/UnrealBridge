"""验证 exec/modal 共用可取消 work 契约。 / Verify the shared cancellable-work contract for exec/modal."""

from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SERVER_CPP = (
    REPO_ROOT
    / "Plugin"
    / "UnrealBridge"
    / "Source"
    / "UnrealBridge"
    / "Private"
    / "UnrealBridgeServer.cpp"
)
WORK_HEADER = SERVER_CPP.with_name("UnrealBridgeCancellableWork.h")
GUIDE = REPO_ROOT / "CLAUDE.md"


class CancellableWorkSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.server = SERVER_CPP.read_text(encoding="utf-8")
        cls.work = WORK_HEADER.read_text(encoding="utf-8")
        cls.guide = GUIDE.read_text(encoding="utf-8")

    def test_state_machine_has_only_the_four_authoritative_states(self):
        enum_body = self.work.split(
            "enum class EUnrealBridgeWorkState", 1
        )[1].split("};", 1)[0]
        for state in ("Queued", "Running", "Cancelled", "Completed"):
            self.assertEqual(enum_body.count(state), 1)

    def test_exec_and_modal_use_the_same_work_primitive(self):
        self.assertIn(
            "TUnrealBridgeCancellableWork<FExecResult> Work;", self.server
        )
        self.assertIn(
            "using FModalWork = "
            "TUnrealBridgeCancellableWork<TSharedPtr<FJsonObject>>;",
            self.server,
        )
        self.assertGreaterEqual(self.server.count("Pending->Work.TryCancel"), 2)
        self.assertIn("Pending->TryCancel", self.server)
        self.assertIn("Pending->Work.TryExecute", self.server)
        self.assertIn("Pending->TryExecute", self.server)

    def test_timeout_results_distinguish_cancelled_from_running(self):
        self.assertIn("queued work cancelled before execution", self.server)
        self.assertIn("work already started and outcome is unknown", self.server)
        self.assertNotIn(
            "Leave the promise alone — the ticker will still fulfill it later",
            self.server,
        )

    def test_timeout_contract_is_documented_for_callers(self):
        self.assertIn("cancelled before execution", self.guide)
        self.assertIn("already started and outcome is unknown", self.guide)

    def test_shutdown_closes_admission_and_drains_plugin_closures(self):
        self.assertGreaterEqual(self.server.count("WorkAdmission->TryAdmit"), 2)
        self.assertIn("WorkAdmission->Close();", self.server)
        self.assertIn("WaitUntilTasksComplete", self.server)
        self.assertIn("ProcessThreadUntilIdle", self.server)
        self.assertNotIn("still active after 3s drain timeout", self.server)

    def test_background_worker_owns_thread_safe_server_lifetime(self):
        self.assertIn(
            "TSharedRef<FUnrealBridgeServer, ESPMode::ThreadSafe> Self = AsShared();",
            self.server,
        )
        self.assertIn("FFunctionGraphTask::CreateAndDispatchWhenReady", self.server)
        self.assertNotIn("[this, ClientSocket, EndpointStr]", self.server)

    def test_exec_ticker_skips_backlog_before_running_one_body(self):
        self.assertIn("while (ExecQueue.Dequeue(Pending))", self.server)
        self.assertIn("if (bExecuted)", self.server)


if __name__ == "__main__":
    unittest.main()

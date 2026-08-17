"""Static ownership/dispatch contract checks for cached editor health."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "Plugin/UnrealBridge/Source/UnrealBridge/Private"
SERVER = PRIVATE / "UnrealBridgeServer.cpp"
PROTOCOL = PRIVATE / "UnrealBridgeProtocol.h"
DISPATCHER = PRIVATE / "UnrealBridgeExactRequestDispatcher.cpp"
DISCOVERY_CLIENT = (
    ROOT / ".claude/skills/unreal-bridge/scripts/bridge_discovery.py"
)
TESTS = (
    ROOT
    / "Plugin/UnrealBridge/Source/UnrealBridge/Private/Tests"
    / "UnrealBridgeEditorHealthTests.cpp"
)


class EditorHealthContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = SERVER.read_text(encoding="utf-8")
        cls.protocol = PROTOCOL.read_text(encoding="utf-8")
        cls.dispatcher = DISPATCHER.read_text(encoding="utf-8")
        cls.discovery_client = DISCOVERY_CLIENT.read_text(encoding="utf-8")
        cls.tests = TESTS.read_text(encoding="utf-8")

    def test_health_branch_reads_cache_without_fresh_dispatch_or_slate(self):
        match = re.search(
            r"else if \(Command == EUnrealBridgeExactCommand::EditorStatus\)(.*?)"
            r"else if \(Command == EUnrealBridgeExactCommand::DebugResume\)",
            self.server,
            re.DOTALL,
        )
        self.assertIsNotNone(match)
        branch = match.group(1)
        self.assertIn("EditorHealthCache->Read", branch)
        self.assertIn('TEXT("schema_version")', branch)
        self.assertIn('TEXT("editor_status")', branch)
        self.assertIn('TEXT("active_modal")', branch)
        self.assertNotIn("AsyncTask", branch)
        self.assertNotIn("RunOnGameThread", branch)
        self.assertNotIn("FSlateApplication", branch)
        self.assertNotIn("CaptureSnapshot", branch)
        dispatcher = self.server.index("FUnrealBridgeExactRequestDispatcher::TryDispatch")
        self.assertLess(dispatcher, match.start())

    def test_exact_status_is_canonical_dispatched_and_discoverable(self):
        self.assertIn(
            'ExactEditorStatus = TEXT("exact_editor_status")', self.protocol
        )
        capabilities = self.protocol[
            self.protocol.index("ExactCapabilities[]"):
        ]
        self.assertIn("ExactEditorStatus", capabilities)
        self.assertIn(
            "WireCommand == UnrealBridgeProtocol::ExactEditorStatus",
            self.dispatcher,
        )
        self.assertIn(
            "OutCommand = EUnrealBridgeExactCommand::EditorStatus",
            self.dispatcher,
        )
        self.assertIn('"exact_editor_status"', self.discovery_client)

    def test_modal_summary_is_refreshed_by_slate_owner_delegate(self):
        self.assertIn("OnPreTick().AddRaw", self.server)
        self.assertIn("RefreshCachedSlateHealth", self.server)
        self.assertIn("UnrealBridgeModal::CaptureSnapshot", self.server)
        self.assertIn("OnPreTick().Remove", self.server)

    def test_deterministic_tests_inject_clock(self):
        self.assertGreaterEqual(self.tests.count("[&Now]() { return Now; }"), 4)
        self.assertGreaterEqual(self.tests.count("[&NowUtc]() { return NowUtc; }"), 2)
        self.assertIn("InjectedClockControlsAges", self.tests)
        self.assertIn("ClockRollbackClampsAge", self.tests)


if __name__ == "__main__":
    unittest.main()

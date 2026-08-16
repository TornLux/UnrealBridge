"""Contract tests for the UnrealBridge plugin descriptor."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


DESCRIPTOR_PATH = (
    Path(__file__).resolve().parents[1]
    / "Plugin"
    / "UnrealBridge"
    / "UnrealBridge.uplugin"
)


class PluginDescriptorTests(unittest.TestCase):
    def test_bridge_module_is_excluded_from_commandlets(self):
        descriptor = json.loads(DESCRIPTOR_PATH.read_text(encoding="utf-8"))
        bridge_modules = [
            module
            for module in descriptor["Modules"]
            if module.get("Name") == "UnrealBridge"
        ]

        self.assertEqual(len(bridge_modules), 1)
        self.assertEqual(bridge_modules[0].get("Type"), "EditorNoCommandlet")


if __name__ == "__main__":
    unittest.main()

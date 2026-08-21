"""Offline contract tests for the safe asset-duplication bridge surface."""

from __future__ import annotations

import ast
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "Plugin" / "UnrealBridge"
SKILL = ROOT / ".claude" / "skills" / "unreal-bridge"


class DuplicateAssetContractTests(unittest.TestCase):
    def test_native_api_wraps_editor_asset_library_with_guards(self):
        header = (
            PLUGIN
            / "Source"
            / "UnrealBridge"
            / "Public"
            / "UnrealBridgeAssetLibrary.h"
        ).read_text(encoding="utf-8")
        implementation = (
            PLUGIN
            / "Source"
            / "UnrealBridge"
            / "Private"
            / "UnrealBridgeAssetLibrary.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn("struct FBridgeAssetDuplicateResult", header)
        self.assertIn("static FBridgeAssetDuplicateResult DuplicateAsset(", header)
        self.assertIn('#include "EditorAssetLibrary.h"', implementation)
        self.assertIn("UEditorAssetLibrary::DuplicateAsset(", implementation)
        self.assertIn("UEditorAssetLibrary::SaveLoadedAsset(", implementation)
        self.assertIn("destination must be a writable /Game asset path", implementation)
        self.assertIn("UnrealBridge never overwrites assets", implementation)

    def test_editor_scripting_utilities_dependency_is_explicit(self):
        build_rules = (
            PLUGIN / "Source" / "UnrealBridge" / "UnrealBridge.Build.cs"
        ).read_text(encoding="utf-8")
        descriptor = json.loads(
            (PLUGIN / "UnrealBridge.uplugin").read_text(encoding="utf-8")
        )

        self.assertIn('"EditorScriptingUtilities"', build_rules)
        enabled_plugins = {
            entry["Name"]
            for entry in descriptor.get("Plugins", [])
            if entry.get("Enabled") is True
        }
        self.assertIn("EditorScriptingUtilities", enabled_plugins)

    def test_manifest_and_kwargs_wrapper_expose_the_same_signature(self):
        manifest = json.loads(
            (SKILL / "scripts" / "bridge_manifest.json").read_text(encoding="utf-8")
        )
        function = manifest["libraries"]["UnrealBridgeAssetLibrary"]["functions"][
            "duplicate_asset"
        ]
        self.assertEqual(
            [param["name"] for param in function["params"]],
            ["source_asset_path", "destination_asset_path", "save"],
        )
        self.assertEqual(function["params"][-1]["default"], "True")
        self.assertEqual(function["returns"], "BridgeAssetDuplicateResult")
        self.assertEqual(
            manifest["structs"]["BridgeAssetDuplicateResult"],
            [
                "asset_class_path",
                "destination_asset_path",
                "error",
                "package_dirty",
                "saved",
                "source_asset_path",
                "success",
            ],
        )

        wrapper_path = PLUGIN / "Content" / "Python" / "unreal_bridge.py"
        module = ast.parse(wrapper_path.read_text(encoding="utf-8"))
        asset_class = next(
            node
            for node in module.body
            if isinstance(node, ast.ClassDef) and node.name == "Asset"
        )
        duplicate = next(
            node
            for node in asset_class.body
            if isinstance(node, ast.FunctionDef) and node.name == "duplicate_asset"
        )
        self.assertEqual(
            [arg.arg for arg in duplicate.args.kwonlyargs],
            ["source_asset_path", "destination_asset_path", "save"],
        )

    def test_raw_editor_call_redirects_to_the_guarded_bridge_api(self):
        redirects = json.loads(
            (SKILL / "scripts" / "bridge_redirects.json").read_text(encoding="utf-8")
        )["redirects"]
        entry = next(
            item
            for item in redirects
            if item["id"] == "editor_asset_library_duplicate_asset"
        )
        self.assertEqual(entry["raw_pattern"], "unreal.EditorAssetLibrary.duplicate_asset")
        self.assertIn("Asset.duplicate_asset", entry["bridge_replacement"])

    def test_reference_documents_the_ellie_destination(self):
        reference = (SKILL / "references" / "bridge-asset-api.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("/Game/Locomotion/P3P4_Ellie/PSS_Ellie", reference)
        self.assertIn("non-recursive", reference)
        self.assertIn("there is no overwrite flag", reference)


if __name__ == "__main__":
    unittest.main()

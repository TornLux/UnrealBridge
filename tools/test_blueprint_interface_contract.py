#!/usr/bin/env python3
"""Offline contract checks for Blueprint Interface authoring and wiring.

These checks deliberately require neither an Unreal Editor nor a C++ build.
They guard the reflected surface, the pin-name compatibility fix, the K2 node
configuration order, and the generated Python/manifest/documentation contract.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeBlueprintLibrary.h"
CPP = ROOT / "Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeBlueprintLibrary.cpp"
WRAPPER = ROOT / "Plugin/UnrealBridge/Content/Python/unreal_bridge.py"
DOC = ROOT / ".claude/skills/unreal-bridge/references/bridge-blueprint-api.md"
MANIFEST = ROOT / ".claude/skills/unreal-bridge/scripts/bridge_manifest.json"


class BlueprintInterfaceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.cpp = CPP.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.doc = DOC.read_text(encoding="utf-8")
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def _body(self, function_name: str, next_marker: str) -> str:
        start = self.cpp.index(f"UUnrealBridgeBlueprintLibrary::{function_name}(")
        end = self.cpp.index(next_marker, start)
        return self.cpp[start:end]

    def test_reflected_surface_is_declared_and_implemented(self) -> None:
        names = (
            "CreateBlueprintInterfaceAsset",
            "AddBlueprintInterfaceFunction",
            "AddInterfaceCallNode",
            "AddInterfaceEventNode",
        )
        for name in names:
            with self.subTest(name=name):
                self.assertEqual(self.header.count(f"static " + ("FString " if name != "AddBlueprintInterfaceFunction" else "bool ") + name + "("), 1)
                self.assertEqual(self.cpp.count(f"UUnrealBridgeBlueprintLibrary::{name}("), 1)

    def test_interface_asset_creation_is_safe_and_typed(self) -> None:
        body = self._body("CreateBlueprintInterfaceAsset", "// ─── AddBlueprintInterfaceFunction")
        self.assertIn('StartsWith(TEXT("/Game/"))', body)
        self.assertIn("destination already exists; overwrite is not supported", body)
        self.assertIn("UInterface::StaticClass()", body)
        self.assertIn("BPTYPE_Interface", body)
        self.assertIn("FAssetRegistryModule::AssetCreated", body)
        self.assertIn("UEditorLoadingAndSavingUtils::SavePackages", body)

    def test_interface_signature_creation_is_restricted_and_idempotent(self) -> None:
        body = self._body("AddBlueprintInterfaceFunction", "// ─── AddBlueprintInterface")
        self.assertIn("BlueprintType != BPTYPE_Interface", body)
        self.assertIn("Graph->GetFName() == RequestedName", body)
        self.assertIn("AddFunctionGraph<UClass>", body)

    def test_interface_nodes_are_configured_before_pin_allocation(self) -> None:
        call = self._body("AddInterfaceCallNode", "FString UUnrealBridgeBlueprintLibrary::AddInterfaceEventNode")
        message = self._body("AddInterfaceMessageNode", "// ─── Variable metadata")
        for body in (call, message):
            self.assertLess(body.index("SetFromFunction(Fn)"), body.index("FinalizeNewNode"))

    def test_interface_event_uses_owner_aware_external_reference(self) -> None:
        helper_start = self.cpp.index("static FString EnsureInterfaceEventNode")
        helper_end = self.cpp.index("static UFunction* ResolveCallableFunction", helper_start)
        helper = self.cpp[helper_start:helper_end]
        self.assertIn("FunctionCanBePlacedAsEvent", helper)
        self.assertIn("IsImplementedByBlueprint", helper)
        self.assertIn("GetMemberParentClass(ContextClass) != InterfaceClass", helper)
        self.assertIn("EventReference.SetExternalMember(FunctionName, InterfaceClass)", helper)

    def test_visible_target_alias_resolves_to_internal_self(self) -> None:
        resolver_start = self.cpp.index("UEdGraphPin* ResolvePinReference")
        resolver_end = self.cpp.index("UClass* ResolveTargetClass", resolver_start)
        resolver = self.cpp[resolver_start:resolver_end]
        self.assertIn('Requested.Equals(TEXT("Target")', resolver)
        self.assertIn("UEdGraphSchema_K2::PN_Self", resolver)
        connect = self._body("ConnectGraphPins", "bool UUnrealBridgeBlueprintLibrary::RemoveGraphNode")
        self.assertEqual(connect.count("ResolvePinReference"), 2)

    def test_interface_implementation_graphs_are_addressable(self) -> None:
        read_start = self.cpp.index("static TArray<UEdGraph*> FindGraphs")
        read_end = self.cpp.index("/** Classify a node", read_start)
        read_finder = self.cpp[read_start:read_end]
        self.assertIn("BP->ImplementedInterfaces", read_finder)
        self.assertIn("Interface.Graphs", read_finder)

        start = self.cpp.index("UEdGraph* FindGraphByName")
        end = self.cpp.index("UEdGraphNode* FindNodeByGuid", start)
        finder = self.cpp[start:end]
        self.assertIn("BP->ImplementedInterfaces", finder)
        self.assertIn("Interface.Graphs", finder)
        self.assertIn("Stack.Append(Interface.Graphs)", finder)

    def test_manifest_wrapper_and_docs_match_surface(self) -> None:
        functions = self.manifest["libraries"]["UnrealBridgeBlueprintLibrary"]["functions"]
        expected = {
            "create_blueprint_interface_asset",
            "add_blueprint_interface_function",
            "add_interface_call_node",
            "add_interface_event_node",
            "add_interface_message_node",
            "connect_graph_pins",
        }
        self.assertTrue(expected.issubset(functions))
        create_params = functions["create_blueprint_interface_asset"]["params"]
        self.assertEqual(create_params[1]["name"], "save")
        self.assertTrue(create_params[1]["has_default"])
        self.assertEqual(create_params[1]["default"], "True")
        for name in expected:
            self.assertIn(f"def {name}(", self.wrapper)
        self.assertIn("Target", self.doc)
        self.assertIn("internally named `self`", self.doc)


if __name__ == "__main__":
    unittest.main()

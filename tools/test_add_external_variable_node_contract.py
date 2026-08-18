"""Deterministic source-contract tests for AddExternalVariableNode."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeBlueprintLibrary.h"
SOURCE = ROOT / "Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeBlueprintLibrary.cpp"
REFERENCE = ROOT / ".claude/skills/unreal-bridge/references/bridge-blueprint-api.md"
MANIFEST = ROOT / ".claude/skills/unreal-bridge/scripts/bridge_manifest.json"
WRAPPER = ROOT / "Plugin/UnrealBridge/Content/Python/unreal_bridge.py"
AUTOMATION = (
    ROOT
    / "Plugin/UnrealBridge/Source/UnrealBridge/Private/Tests"
    / "UnrealBridgeAddExternalVariableNodeTests.cpp"
)


def extract_function(source: str, signature: str) -> str:
    """Return one complete C++ function using balanced braces."""
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"Unbalanced function body for {signature}")


class AddExternalVariableNodeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.reference = REFERENCE.read_text(encoding="utf-8")
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.automation = AUTOMATION.read_text(encoding="utf-8")
        cls.function = extract_function(
            cls.source,
            "FString UUnrealBridgeBlueprintLibrary::AddExternalVariableNode(",
        )

    def test_reflected_api_is_adjacent_to_self_variable_api(self) -> None:
        declaration = re.compile(
            r"AddVariableNode\([^;]+;\s*/\*\*.*?"
            r"UFUNCTION\(BlueprintCallable, Category = \"UnrealBridge\|Blueprint\"\)\s*"
            r"static FString AddExternalVariableNode\(",
            re.DOTALL,
        )
        self.assertRegex(self.header, declaration)
        self.assertIn("const FString& OwnerClassPath", self.header)

    def test_all_failures_are_pre_mutation_and_diagnostic(self) -> None:
        transaction = self.function.index("FScopedTransaction")
        for required_validation in (
            "OwnerClassPath.TrimStartAndEnd().IsEmpty()",
            "Cast<UEdGraphSchema_K2>(Graph->GetSchema())",
            "FindFProperty<FProperty>(RequestedOwnerClass, VarFName)",
            "FBlueprintEditorUtils::IsPropertyWritableInBlueprint(BP, Property)",
            "FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, Property)",
            "Property->GetOwner<UClass>()",
            'FindPin(TEXT("self"))',
            "CandidateNode->FindPin(VarFName)",
        ):
            self.assertLess(self.function.index(required_validation), transaction)
        self.assertEqual(1, self.function.count("FScopedTransaction"))
        self.assertNotIn("return FString();", self.function.replace("return FString();", "", 1))
        self.assertIn("UE_LOG(LogUnrealBridgeBlueprintGraph, Warning", self.function)
        for diagnostic_field in (
            "Blueprint='%s'",
            "Graph='%s'",
            "OwnerClass='%s'",
            "Property='%s'",
            "Operation='%s'",
            "Reason='%s'",
        ):
            self.assertIn(diagnostic_field, self.function)

    def test_mutation_uses_declaring_owner_and_one_transaction(self) -> None:
        transaction = self.function.index("FScopedTransaction")
        mutation = self.function[transaction:]
        self.assertIn(
            "VariableReference.SetExternalMember(VarFName, DeclaringOwnerClass)",
            mutation,
        )
        self.assertNotIn(
            "VariableReference.SetExternalMember(VarFName, RequestedOwnerClass)",
            self.function,
        )
        self.assertIn("BP->Modify();", mutation)
        self.assertIn("Graph->Modify();", mutation)
        self.assertIn("Node->SetFlags(RF_Transactional);", mutation)
        self.assertIn("Node->Modify();", mutation)
        self.assertIn("FBlueprintEditorUtils::MarkBlueprintAsModified(BP);", mutation)
        self.assertIn("Node->NodeGuid.ToString(EGuidFormats::Digits)", mutation)

    def test_validation_candidate_is_transient_and_scope_cleaned(self) -> None:
        transaction = self.function.index("FScopedTransaction")
        preflight = self.function[:transaction]
        self.assertIn("RF_Transient", preflight)
        self.assertIn("ON_SCOPE_EXIT", preflight)
        self.assertIn("CandidateNode->MarkAsGarbage();", preflight)

    def test_automation_covers_private_access_and_behavioral_undo_redo(self) -> None:
        for required in (
            "FBlueprintMetadata::MD_Private",
            "PrivateGetterGuid",
            "PrivateSetterGuid",
            "GEditor->UndoTransaction()",
            "GEditor->RedoTransaction()",
            "HasAnyFlags(RF_Transactional)",
            "CountLiveGraphOwnedVariableNodes",
            "EInternalObjectFlags::Garbage",
            "Getter GUID is absent after Undo",
            "Redo restores the getter with the same GUID",
            "Redo restores getter target and value pins",
            "Setter GUID is absent after Undo",
            "Redo restores the setter with the same GUID",
            "Redo restores setter target and value pins",
        ):
            self.assertIn(required, self.automation)

    def test_generated_surfaces_match_the_reflected_signature(self) -> None:
        function = self.manifest["libraries"]["UnrealBridgeBlueprintLibrary"]["functions"][
            "add_external_variable_node"
        ]
        self.assertEqual(
            [parameter["name"] for parameter in function["params"]],
            [
                "blueprint_path",
                "graph_name",
                "owner_class_path",
                "variable_name",
                "is_set",
                "node_pos_x",
                "node_pos_y",
            ],
        )
        self.assertEqual(function["returns"], "str")
        self.assertIn(
            "def add_external_variable_node(*, blueprint_path, graph_name, "
            "owner_class_path, variable_name, is_set, node_pos_x, node_pos_y):",
            self.wrapper,
        )

    def test_reference_documents_runtime_contract(self) -> None:
        section_start = self.reference.index("### add_external_variable_node(")
        section_end = self.reference.index("\n### ", section_start + 5)
        section = self.reference[section_start:section_end]
        self.assertIn(
            "add_external_variable_node(blueprint_path, graph_name, owner_class_path, "
            "variable_name, is_set, node_pos_x, node_pos_y)",
            section,
        )
        for expected in (
            "cannot be empty",
            "canonical Blueprint access rules",
            "Blueprint-private",
            "Blueprint-read-only",
            "actually declares it",
            "single undo transaction",
            "does not compile automatically",
            "`self`",
            "`Output_Get`",
        ):
            self.assertIn(expected, section)


if __name__ == "__main__":
    unittest.main()

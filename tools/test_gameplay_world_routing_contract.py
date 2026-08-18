"""Source contract for typed Gameplay Library PIE-world routing."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = (
    ROOT
    / "Plugin"
    / "UnrealBridge"
    / "Source"
    / "UnrealBridge"
    / "Private"
    / "UnrealBridgeGameplayLibrary.cpp"
)
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")

GENERAL_CALLERS = {
    "FindNavPath",
    "TeleportPawn",
    "IsInPIE",
    "GetNavWorld",
    "SetPawnMaxWalkSpeed",
    "SetPawnGravityScale",
    "SimulateJumpArc",
    "SpawnActorInPIE",
    "DestroyActorInPIE",
    "GetPIEActorLocation",
    "GetGlobalTimeDilation",
    "SetGlobalTimeDilation",
    "GetActorTimeDilation",
    "SetActorTimeDilation",
    "PlaySound2D",
    "PlaySoundAtLocation",
    "ApplyDamageToActor",
    "GetAllPawns",
    "GetAIPawns",
    "GetActorController",
    "DrawDebugLine",
    "DrawDebugSphereAt",
    "DrawDebugBoxAt",
    "DrawDebugArrow",
    "DrawDebugString",
    "FlushPersistentDebugDraws",
    "GetPIEDeltaSeconds",
    "GetPIENumPlayers",
    "GetPIENumAIControllers",
    "GetPIEPrimitive",
    "PlayWorldCameraShake",
    "FindPlayerStart",
    "RespawnPlayerPawn",
    "PauseGame",
    "IsGamePaused",
    "GetGameModeClassName",
    "GetGameStateClassName",
    "ApplyRadialDamage",
    "FindPIEActorsByClass",
}

LOCAL_PLAYER_CALLERS = {
    "GetAgentObservation",
    "ApplyMovementInput",
    "ApplyLookInput",
    "SetControlRotation",
    "Jump",
    "StopJumping",
    "InjectEnhancedInputAxis",
    "SetStickyInput",
    "TriggerInputAction",
    "GetControlRotation",
    "GetCameraHitActor",
    "GetCameraHitLocation",
    "IsActorVisibleFromCamera",
    "GetPawnGroundHeight",
    "GetPIEViewportSize",
    "DeprojectScreenToWorld",
    "ProjectWorldToScreen",
    "GetPIECameraManager",
    "GetPawnMaxWalkSpeed",
    "GetPawnSpeed",
    "GetPawnCapabilities",
    "GetEILocalPlayerSub",
    "GetActiveSubsystem",
    "GetPawnForwardVector",
    "GetPawnRightVector",
    "GetPawnUpVector",
    "GetDistanceToPawn",
    "GetPlayerPawnActorName",
    "GetPlayerSkeletalMeshComponent",
    "GetCameraViewPoint",
    "GetActorAtScreenPosition",
}


def _function_body(name: str) -> str:
    public_pattern = re.compile(
        rf"UUnrealBridgeGameplayLibrary::{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{",
        re.MULTILINE,
    )
    helper_pattern = re.compile(
        rf"^[\t ]*(?:static\s+)?[^\n;{{}}]+\s+{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{",
        re.MULTILINE,
    )
    match = public_pattern.search(SOURCE) or helper_pattern.search(SOURCE)
    if match is None:
        raise AssertionError(f"production function not found: {name}")

    opening_brace = SOURCE.find("{", match.start())
    depth = 0
    for index in range(opening_brace, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[opening_brace : index + 1]
    raise AssertionError(f"unterminated production function: {name}")


class GameplayWorldRoutingContractTests(unittest.TestCase):
    def test_every_direct_world_source_caller_is_typed_and_classified(self) -> None:
        for name in sorted(GENERAL_CALLERS):
            body = _function_body(name)
            self.assertIn("GetPIEWorld()", body, name)
            self.assertNotIn("GetLocalPlayerPIEWorld()", body, name)

        for name in sorted(LOCAL_PLAYER_CALLERS):
            body = _function_body(name)
            self.assertIn("GetLocalPlayerPIEWorld()", body, name)
            self.assertNotIn("BridgeAgentImpl::GetPIEWorld()", body, name)

        # The extra occurrence in each count is the source function definition.
        self.assertEqual(SOURCE.count("GetPIEWorld()"), len(GENERAL_CALLERS) + 1)
        self.assertEqual(
            SOURCE.count("GetLocalPlayerPIEWorld()"),
            len(LOCAL_PLAYER_CALLERS) + 1,
        )
        self.assertEqual(len(GENERAL_CALLERS), 39)
        self.assertEqual(len(LOCAL_PLAYER_CALLERS), 31)
        self.assertEqual(len(GENERAL_CALLERS | LOCAL_PLAYER_CALLERS), 70)
        self.assertTrue(GENERAL_CALLERS.isdisjoint(LOCAL_PLAYER_CALLERS))

    def test_no_independent_pie_context_scans_bypass_typed_sources(self) -> None:
        general_source = _function_body("GetPIEWorld")
        local_source = _function_body("GetLocalPlayerPIEWorld")
        self.assertIn("GetWorldContexts()", general_source)
        self.assertIn("GetWorldContexts()", local_source)
        self.assertEqual(SOURCE.count("GetWorldContexts()"), 2)
        self.assertNotIn("EWorldType::PIE", SOURCE)

    def test_sticky_input_is_pinned_to_its_originating_local_player_identity(self) -> None:
        sticky_tick = _function_body("StickyTick")
        self.assertIn("E.World.Get()", sticky_tick)
        self.assertIn("E.PlayerController.Get()", sticky_tick)
        self.assertIn("E.LocalPlayer.Get()", sticky_tick)
        self.assertIn("IsSameLocalPlayerPIEIdentity(World, PlayerController, LocalPlayer)", sticky_tick)
        self.assertIn("const double WorldNow = World->GetTimeSeconds()", sticky_tick)
        self.assertNotIn("GetFirstPlayerController()", sticky_tick)
        self.assertNotIn("GetPIEWorld()", sticky_tick)
        self.assertNotIn("GetLocalPlayerPIEWorld()", sticky_tick)
        self.assertIn("TWeakObjectPtr<UWorld> World;", SOURCE)
        self.assertIn("TWeakObjectPtr<APlayerController> PlayerController;", SOURCE)
        self.assertIn("TWeakObjectPtr<ULocalPlayer> LocalPlayer;", SOURCE)
        self.assertEqual(SOURCE.count("Entry.World = World;"), 2)
        self.assertEqual(SOURCE.count("Entry.PlayerController = PlayerController;"), 2)
        self.assertEqual(SOURCE.count("Entry.LocalPlayer = LocalPlayer;"), 2)


if __name__ == "__main__":
    unittest.main()

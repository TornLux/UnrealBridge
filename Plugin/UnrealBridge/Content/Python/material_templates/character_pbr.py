"""M3-1: M_Character_PBR — generic PBR master for skin / cloth / metal.

This is the restricted subset of M3-2 ``character_armor``: every feature
flag left at its default (all static switches = false), so the compiled
permutation is the lean core pipeline — ORM unpack → BaseColor × Tint →
Roughness remap → Metallic scale → Normal intensity → main outputs. No
wear, no wetness, no detail normal, no anisotropy, no emissive.

The graph is structurally identical to ``M_Character_Armor``; we just
publish it under a distinct asset name so designers can tell the
"hero armor" master from the "generic PBR" master at a glance. If a
project only needs one master, prefer M3-2 directly and flip switches
per-MI — this file exists for clarity, not to add graph capability.

Parameters exposed (all MI-overridable):

  BaseColorTex / BaseColorTint
  ORMTex                        (R=AO, G=Roughness, B=Metallic)
  NormalTex / NormalIntensity
  RoughnessMin / RoughnessMax
  MetallicScale

Budget targets (SM5 / High): ≤ 250 instructions, ≤ 10 samplers — same
as M3-2 since the master graph is the same.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

from . import character_armor as _armor
from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/M_Character_PBR",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Character_PBR_Test",
          preview_png: Optional[str] = None,
          preview_mesh: str = C.DEFAULT_PREVIEW_MESH,
          preview_lighting: str = C.DEFAULT_PREVIEW_LIGHTING,
          preview_resolution: int = 512,
          preview_camera_yaw: float = 35.0,
          preview_camera_pitch: float = -15.0,
          preview_camera_distance: float = 0.0,
          instr_budget: int = 250,
          sampler_budget: int = 10,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Generate the Character_PBR master by delegating to character_armor.

    All outputs match character_armor.build — including the ``custom_node_guid``
    from the RNM-blend Custom node. The Custom sits in the UseDetailNormal
    permutation and is dead-coded when that switch is off (default), so the
    PS shader map doesn't include it.
    """
    return _armor.build(
        master_path=master_path,
        mi_path=mi_path,
        preview_png=preview_png,
        preview_mesh=preview_mesh,
        preview_lighting=preview_lighting,
        preview_resolution=preview_resolution,
        preview_camera_yaw=preview_camera_yaw,
        preview_camera_pitch=preview_camera_pitch,
        preview_camera_distance=preview_camera_distance,
        instr_budget=instr_budget,
        sampler_budget=sampler_budget,
        compile=compile,
        rebuild=rebuild,
    )

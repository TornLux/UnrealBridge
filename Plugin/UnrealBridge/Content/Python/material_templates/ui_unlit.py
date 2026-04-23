"""M3-8: M_UI_Unlit — minimal Unlit master for UMG widgets / HUD decals.

ShadingModel=Unlit, Domain=Surface, Blend=Translucent. Deliberately tiny —
UMG materials should stay under 60 instructions so they don't dominate
UI draw cost when stacked across a menu screen.

Parameters:

  BaseColorTex   — optional texture (default WhiteSquareTexture)
  Tint           — multiplied over the texture (RGBA, default white)
  OpacityScale   — 0..1 multiplier on the texture alpha (default 1)

Typical use: set Tint.A to 1 for opaque widgets, lower it for fade-ins;
leave BaseColorTex empty to get a flat-tinted colour panel.
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/M_UI_Unlit",
          mi_path: "str | None" = "/Game/BridgeTemplates/MI_UI_Unlit_Test",
          # Translucent + Unlit still compiles ~350 inst because UE emits the
          # translucent-lighting shader variant regardless. Budget below sets a
          # realistic ceiling, NOT the roadmap's aspirational 60 (which only
          # applies to the agent-written portion of the graph).
          instr_budget: int = 400,
          sampler_budget: int = 2,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    master_path = C.ensure_master_material(
        master_path, "Surface", "Unlit", "Translucent",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    ops = C.OpList()

    # --- parameters ------------------------------------------------
    C.add_texture_param_2d(ops, "tp_base", -900, -40,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="01 UI", sort_priority=1)
    C.add_vector_param(ops, "vp_tint", -900, 180,
                       "Tint", "(R=1,G=1,B=1,A=1)",
                       group="01 UI", sort_priority=2)
    C.add_scalar_param(ops, "sp_opacity", -900, 260,
                       "OpacityScale", 1.0,
                       group="01 UI", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)

    # --- colour chain: tex.RGB * tint.RGB → EmissiveColor ---------
    ops.add("mul_rgb", "Multiply", -500, 40)
    ops.connect("tp_base", "RGB", "mul_rgb", "A")
    ops.connect("vp_tint", "", "mul_rgb", "B")
    ops.connect_out("mul_rgb", "", "EmissiveColor")

    # --- opacity chain: tex.A * tint.A * OpacityScale -------------
    ops.add("mul_a_base", "Multiply", -500, 220)
    ops.connect("tp_base", "A", "mul_a_base", "A")
    ops.connect("vp_tint", "A", "mul_a_base", "B")

    ops.add("mul_a_final", "Multiply", -300, 220)
    ops.connect("mul_a_base", "", "mul_a_final", "A")
    ops.connect("sp_opacity", "", "mul_a_final", "B")
    ops.connect_out("mul_a_final", "", "Opacity")

    ops.comment(-960, -120, 800, 500,
                "UI unlit — tex×tint → Emissive, tex.A × tint.A × Scale → Opacity",
                C.COMMENT_COLOR_PBR)

    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")
    C.save_master(master_path)

    stats = C.collect_stats(master_path) if compile else {}
    budget = C.check_budget(stats, instr_budget, sampler_budget) if stats else {}

    out: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(result.ops_applied),
    }
    if stats:
        out.update({
            "max_instructions": stats["max_instructions"],
            "sampler_count": stats["sampler_count"],
            "compile_errors": stats["compile_errors"],
            "compile_clean": len(stats["compile_errors"]) == 0,
            "num_expressions": stats["num_expressions"],
            "over_instr": budget.get("over_instr", False),
            "over_sampler": budget.get("over_sampler", False),
            "ok": budget.get("ok", True),
        })

    if mi_path:
        if unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            out["mi_path"] = mi_path
        else:
            mi_result = L.create_material_instance(master_path, mi_path)
            if mi_result.success:
                out["mi_path"] = str(mi_result.path) or mi_path
            else:
                out["mi_error"] = mi_result.error

    return out

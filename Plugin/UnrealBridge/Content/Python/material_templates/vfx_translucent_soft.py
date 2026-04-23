"""M3-9b: M_VFX_Translucent_Soft — Niagara-ready translucent with soft depth fade.

Smoke / cloud / dust VFX material:

  Domain=Surface, ShadingModel=Unlit, Blend=Translucent

Soft-particle depth fade attenuates opacity near intersecting geometry
so the sprite doesn't exhibit the classic "cardboard edge" look where it
pokes through a wall. Uses UE's built-in UMaterialExpressionDepthFade
node (no HLSL needed — pure graph).

Graph:

    EmissiveColor = BaseColorTex.RGB * ColorTint.RGB * ParticleColor.RGB
    soft_opacity  = DepthFade(FadeDistance, input_opacity)
    Opacity       = soft_opacity * OpacityScale

Parameters:

  BaseColorTex    — sprite / flipbook (default WhiteSquareTexture)
  ColorTint       — flat tint + alpha gate (RGBA, default white)
  OpacityScale    — 0..1 master opacity multiplier (default 1)
  DepthFadeDist   — world-space distance over which the sprite fades in
                    near intersecting geometry (default 100 cm)
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/M_VFX_Translucent_Soft",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_VFX_Translucent_Soft_Test",
          instr_budget: int = 200,
          sampler_budget: int = 2,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    master_path = C.ensure_master_material(
        master_path, "Surface", "Unlit", "Translucent",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    ops = C.OpList()

    # --- UV + texture --------------------------------------------------
    ops.add("uv0", "TextureCoordinate", -1200, 180)
    C.add_texture_param_2d(ops, "tp_base", -900, 40,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=1)
    ops.connect("uv0", "", "tp_base", "UVs")

    # --- parameters ----------------------------------------------------
    C.add_vector_param(ops, "vp_tint", -1200, -40,
                       "ColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Color", sort_priority=1)
    C.add_scalar_param(ops, "sp_opacity_scale", -1200, 40,
                       "OpacityScale", 1.0,
                       group="01 Color", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_depth_fade", -1200, 120,
                       "DepthFadeDist", 100.0,
                       group="02 Soft", sort_priority=1,
                       slider_min=1.0, slider_max=1000.0)

    ops.add("particle_col", "ParticleColor", -1200, 340)

    # --- EmissiveColor = tex.RGB × tint.RGB × particle_col.RGB ------
    ops.add("mul_tex_tint", "Multiply", -600, 40)
    ops.connect("tp_base", "RGB", "mul_tex_tint", "A")
    ops.connect("vp_tint", "", "mul_tex_tint", "B")

    ops.add("emissive_final", "Multiply", -400, 40)
    ops.connect("mul_tex_tint", "", "emissive_final", "A")
    ops.connect("particle_col", "RGB", "emissive_final", "B")
    ops.connect_out("emissive_final", "", "EmissiveColor")

    # --- base opacity before soft fade: tex.A × tint.A × particle.A ---
    ops.add("mul_a_tex_tint", "Multiply", -600, 280)
    ops.connect("tp_base", "A", "mul_a_tex_tint", "A")
    ops.connect("vp_tint", "A", "mul_a_tex_tint", "B")

    ops.add("base_opacity", "Multiply", -400, 280)
    ops.connect("mul_a_tex_tint", "", "base_opacity", "A")
    ops.connect("particle_col", "A", "base_opacity", "B")

    # --- DepthFade soft-particle attenuation -------------------------
    # UMaterialExpressionDepthFade inputs: InOpacity, FadeDistance.
    # The shortened pin names are "Opacity" and "FadeDistance" (Opacity
    # pin is named "Opacity" in the UI even though the field is "InOpacity").
    ops.add("depth_fade", "DepthFade", -150, 280)
    ops.connect("base_opacity", "", "depth_fade", "Opacity")
    ops.connect("sp_depth_fade", "", "depth_fade", "FadeDistance")

    # --- final opacity = depth_fade × OpacityScale -------------------
    ops.add("opacity_final", "Multiply", 100, 280)
    ops.connect("depth_fade", "", "opacity_final", "A")
    ops.connect("sp_opacity_scale", "", "opacity_final", "B")
    ops.connect_out("opacity_final", "", "Opacity")

    ops.comment(-1260, -120, 1500, 560,
                "VFX Translucent Soft — tex × Tint × ParticleColor → Emissive, "
                "DepthFade × Scale → Opacity",
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

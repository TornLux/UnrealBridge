"""M3-9a: M_VFX_Unlit_Additive — Niagara-ready additive-blend base.

Classic glow / spark / flash VFX material:

  Domain=Surface, ShadingModel=Unlit, Blend=Additive

Graph:

    EmissiveColor = BaseColorTex.RGB
                    * ColorTint.RGB
                    * ParticleColor.RGB        (Niagara per-particle tint)
                    * EmissiveIntensity
    Opacity       = BaseColorTex.A * ColorTint.A * ParticleColor.A

For Additive blend, UE multiplies the Emissive output by Opacity at
composite time, so the alpha channel acts as a fade multiplier without
ever writing to the depth buffer — exactly the behaviour glow-style
effects expect.

Parameters:

  BaseColorTex         — sprite / flipbook texture (default WhiteSquareTexture)
  ColorTint            — flat-multiplier tint + alpha gate (RGBA, default white)
  EmissiveIntensity    — HDR punch (default 3)
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/M_VFX_Unlit_Additive",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_VFX_Unlit_Additive_Test",
          instr_budget: int = 120,
          sampler_budget: int = 2,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    master_path = C.ensure_master_material(
        master_path, "Surface", "Unlit", "Additive",
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
    C.add_scalar_param(ops, "sp_intensity", -1200, 40,
                       "EmissiveIntensity", 3.0,
                       group="01 Color", sort_priority=2,
                       slider_min=0.0, slider_max=50.0)

    # --- particle color (Niagara per-particle tint) -------------------
    ops.add("particle_col", "ParticleColor", -1200, 340)

    # --- tint.RGB × tex.RGB --------------------------------------------
    ops.add("mul_tex_tint", "Multiply", -600, 40)
    ops.connect("tp_base", "RGB", "mul_tex_tint", "A")
    ops.connect("vp_tint", "", "mul_tex_tint", "B")

    # --- × particle_col.RGB --------------------------------------------
    ops.add("mul_particle_rgb", "Multiply", -400, 40)
    ops.connect("mul_tex_tint", "", "mul_particle_rgb", "A")
    ops.connect("particle_col", "RGB", "mul_particle_rgb", "B")

    # --- × intensity ---------------------------------------------------
    ops.add("emissive_final", "Multiply", -200, 40)
    ops.connect("mul_particle_rgb", "", "emissive_final", "A")
    ops.connect("sp_intensity", "", "emissive_final", "B")
    ops.connect_out("emissive_final", "", "EmissiveColor")

    # --- opacity = tex.A × tint.A × particle_col.A ---------------------
    ops.add("mul_a_tex_tint", "Multiply", -600, 280)
    ops.connect("tp_base", "A", "mul_a_tex_tint", "A")
    ops.connect("vp_tint", "A", "mul_a_tex_tint", "B")

    ops.add("opacity_final", "Multiply", -400, 280)
    ops.connect("mul_a_tex_tint", "", "opacity_final", "A")
    ops.connect("particle_col", "A", "opacity_final", "B")
    ops.connect_out("opacity_final", "", "Opacity")

    ops.comment(-1260, -120, 1300, 540,
                "VFX Additive — tex × Tint × ParticleColor × Intensity → Emissive",
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

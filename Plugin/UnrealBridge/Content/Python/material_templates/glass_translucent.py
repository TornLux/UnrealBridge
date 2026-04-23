"""M3-6: M_Glass_Translucent — ThinTranslucent glass / clear-acrylic master.

ShadingModel=ThinTranslucent, Blend=Translucent. Uses Fresnel to make edges
opaque (hard-to-see-through at glancing angles, clear dead-on). Transmittance
tint routes through SubsurfaceColor per UE's ThinTranslucent convention.

Parameters:

  BaseColorTint       — surface tint for reflection (RGB, default slight grey)
  Roughness           — 0..1 (default 0.05, very smooth for sharp highlights)
  Metallic            — 0..1 (default 0.0, dielectric by default)
  Opacity             — dead-on opacity (default 0.15, mostly-clear)
  EdgeFresnelPower    — how sharply opacity ramps at grazing angles (default 3)
  RefractionIndex     — IoR for the Refraction output (default 1.05, thin glass)
  TransmittanceTint   — what colour the light picks up passing through (default white)
  NormalIntensity     — 0..2 blend between flat and sampled normal (default 1)
  NormalTex           — optional normal texture (default DefaultNormal = flat)

Caveat: UE ThinTranslucent needs ``r.SeparateTranslucency`` + project-level
ThinTranslucent material quality enabled. Agent tasks should verify the
project is set up for translucent rendering before reporting visual results.
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/M_Glass_Translucent",
          mi_path: "str | None" = "/Game/BridgeTemplates/MI_Glass_Test",
          instr_budget: int = 150,
          sampler_budget: int = 4,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    master_path = C.ensure_master_material(
        master_path, "Surface", "ThinTranslucent", "Translucent",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    ops = C.OpList()

    # --- UV + normal sampling ------------------------------------------
    ops.add("uv0", "TextureCoordinate", -1500, 260)

    C.add_texture_param_2d(ops, "tp_normal", -1200, 40,
                           "NormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="01 Surface", sort_priority=10)
    ops.connect("uv0", "", "tp_normal", "UVs")

    ops.add("c3v_flat", "Constant3Vector", -1200, 220)
    ops.setp("c3v_flat", "Constant", "(R=0,G=0,B=1,A=0)")

    # --- parameters ----------------------------------------------------
    C.add_vector_param(ops, "vp_base", -1500, -60,
                       "BaseColorTint", "(R=0.85,G=0.88,B=0.92,A=1)",
                       group="01 Surface", sort_priority=1)
    C.add_scalar_param(ops, "sp_rough", -1500, 20,
                       "Roughness", 0.05,
                       group="01 Surface", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_metal", -1500, 100,
                       "Metallic", 0.0,
                       group="01 Surface", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_normal_int", -1500, 180,
                       "NormalIntensity", 1.0,
                       group="01 Surface", sort_priority=4,
                       slider_min=0.0, slider_max=2.0)
    C.add_scalar_param(ops, "sp_opacity", -1500, 360,
                       "Opacity", 0.15,
                       group="02 Translucency", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_fresnel_pow", -1500, 440,
                       "EdgeFresnelPower", 3.0,
                       group="02 Translucency", sort_priority=2,
                       slider_min=0.5, slider_max=10.0)
    C.add_scalar_param(ops, "sp_ior", -1500, 520,
                       "RefractionIndex", 1.05,
                       group="02 Translucency", sort_priority=3,
                       slider_min=1.0, slider_max=2.0)
    C.add_vector_param(ops, "vp_transmit", -1500, 600,
                       "TransmittanceTint", "(R=1,G=1,B=1,A=1)",
                       group="02 Translucency", sort_priority=4)

    ops.add("c_one", "Constant", -1500, 700)
    ops.setp("c_one", "R", "1.0")

    # --- normal chain: lerp(flat, sampled, intensity) ------------------
    ops.add("normal_final", "LinearInterpolate", -900, 100)
    ops.connect("c3v_flat", "", "normal_final", "A")
    ops.connect("tp_normal", "RGB", "normal_final", "B")
    ops.connect("sp_normal_int", "", "normal_final", "Alpha")
    ops.connect_out("normal_final", "", "Normal")

    # --- PBR routing ---------------------------------------------------
    ops.connect_out("vp_base", "", "BaseColor")
    ops.connect_out("sp_rough", "", "Roughness")
    ops.connect_out("sp_metal", "", "Metallic")

    # --- opacity = lerp(base_opacity, 1.0, fresnel) ---------------
    # Fresnel's pin is named "ExponentIn"; the scalar fallback UPROPERTY is
    # "Exponent" (used only when ExponentIn is unwired). We wire the pin, so
    # no need to set the fallback.
    ops.add("fresnel", "Fresnel", -900, 360)
    ops.connect("sp_fresnel_pow", "", "fresnel", "ExponentIn")

    ops.add("opacity_final", "LinearInterpolate", -600, 340)
    ops.connect("sp_opacity", "", "opacity_final", "A")
    ops.connect("c_one", "", "opacity_final", "B")
    ops.connect("fresnel", "", "opacity_final", "Alpha")
    ops.connect_out("opacity_final", "", "Opacity")

    # --- refraction: scalar IoR on the Refraction output --------------
    ops.connect_out("sp_ior", "", "Refraction")

    # --- transmittance: ThinTranslucent routes the tint through SubsurfaceColor
    ops.connect_out("vp_transmit", "", "SubsurfaceColor")

    ops.comment(-1560, -140, 2000, 900,
                "Glass (ThinTranslucent) — Fresnel-gated opacity, IoR refraction, tint → SubsurfaceColor",
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

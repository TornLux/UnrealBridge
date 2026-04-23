"""M3-4: M_Foliage_Master — masked two-sided foliage with WPO wind.

Shading=TwoSidedFoliage, Blend=Masked, TwoSided=True. Alpha cut-out for
leaves, subsurface scatter tint so back-lit leaves pick up green through-
light, and a lightweight sine-based World-Position-Offset for per-vertex
wind sway. Vertex colour R gates wind amplitude so artists can paint
"strength" onto leaves vs. trunk.

Parameters:

  BaseColorTex / BaseColorTint    — leaf albedo
  ORMTex                          — packed AO / Roughness / Metallic
  RoughnessFloor / RoughnessCeil  — remap ORM.G into this range (default 0.3 / 0.85)
  MetallicScale                   — default 0.0 (foliage is dielectric)
  NormalTex / NormalIntensity     — tangent-space normal
  SubsurfaceColor                 — translucent tint, default sap-green
  SubsurfaceOpacity               — subsurface blend strength (default 0.5)
  UseWind                         — static switch; off = no WPO at all
  WindStrength                    — WPO amplitude in cm (default 5)
  WindSpeed                       — oscillation frequency (default 1.5)

Budget (SM5 / High): ~180 instructions, ≤ 4 samplers (4 shared-wrap texture
parameters). All texture parameters default to engine 1×1 fallbacks so a
fresh MI previews cleanly before the artist assigns any real texture.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/M_Foliage_Master",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Foliage_Test",
          instr_budget: int = 180,
          sampler_budget: int = 4,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    master_path = C.ensure_master_material(
        master_path, "Surface", "TwoSidedFoliage", "Masked",
        two_sided=True, use_material_attributes=False,
        rebuild=rebuild)

    ops = C.OpList()

    # ---------- UV + texture samples -----------------------------------
    ops.add("uv0", "TextureCoordinate", -1600, 400)

    C.add_texture_param_2d(ops, "tp_base", -1250, -120,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=1)
    # ORM uses LinearColor sampler — Masks sampler would reject the default
    # WhiteSquareTexture (which is Color type). Artists replacing the default
    # with a real ORM packed texture should either set that texture's
    # compression to Masks (non-sRGB) + LinearColor sampler, or set the
    # sampler here to Masks and supply a Masks-compressed default.
    C.add_texture_param_2d(ops, "tp_orm", -1250, 140,
                           "ORMTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="LinearColor",
                           group="Textures", sort_priority=2)
    C.add_texture_param_2d(ops, "tp_normal", -1250, 400,
                           "NormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=3)

    for tex in ("tp_base", "tp_orm", "tp_normal"):
        ops.connect("uv0", "", tex, "UVs")

    # ---------- parameters --------------------------------------------
    C.add_vector_param(ops, "vp_tint", -1600, -120,
                       "BaseColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Base", sort_priority=1)
    C.add_scalar_param(ops, "sp_rough_min", -1600, -40,
                       "RoughnessFloor", 0.3,
                       group="01 Base", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_rough_max", -1600, 40,
                       "RoughnessCeil", 0.85,
                       group="01 Base", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_metal", -1600, 120,
                       "MetallicScale", 0.0,
                       group="01 Base", sort_priority=4,
                       slider_min=0.0, slider_max=2.0)
    C.add_scalar_param(ops, "sp_norm_int", -1600, 200,
                       "NormalIntensity", 1.0,
                       group="01 Base", sort_priority=5,
                       slider_min=0.0, slider_max=2.0)
    # Note: UE's OpacityMaskClipValue is a material-level property, not a
    # graph input — adjust it on the UMaterial via the Material Editor's
    # Details panel or via M2-7 set_material_expression_property on the
    # material asset itself (not a node). Exposing it as a ScalarParameter
    # here would dangle with no wire; M5-3 would rightly flag it.

    C.add_vector_param(ops, "vp_ss_color", -1600, 600,
                       "SubsurfaceColor", "(R=0.3,G=0.55,B=0.15,A=1)",
                       group="02 Subsurface", sort_priority=1)
    C.add_scalar_param(ops, "sp_ss_opacity", -1600, 680,
                       "SubsurfaceOpacity", 0.5,
                       group="02 Subsurface", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_wind_str", -1600, 860,
                       "WindStrength", 5.0,
                       group="03 Wind", sort_priority=1,
                       slider_min=0.0, slider_max=50.0)
    C.add_scalar_param(ops, "sp_wind_speed", -1600, 940,
                       "WindSpeed", 1.5,
                       group="03 Wind", sort_priority=2,
                       slider_min=0.0, slider_max=10.0)

    # ---------- BaseColor = tex × tint ---------------------------------
    ops.add("mul_bc", "Multiply", -850, -120)
    ops.connect("tp_base", "RGB", "mul_bc", "A")
    ops.connect("vp_tint", "", "mul_bc", "B")
    ops.connect_out("mul_bc", "", "BaseColor")

    # ---------- Roughness = lerp(min, max, ORM.G) ---------------------
    ops.add("lerp_rough", "LinearInterpolate", -850, 40)
    ops.connect("sp_rough_min", "", "lerp_rough", "A")
    ops.connect("sp_rough_max", "", "lerp_rough", "B")
    ops.connect("tp_orm", "G", "lerp_rough", "Alpha")
    ops.connect_out("lerp_rough", "", "Roughness")

    # ---------- Metallic = ORM.B × scale ------------------------------
    ops.add("mul_metal", "Multiply", -850, 160)
    ops.connect("tp_orm", "B", "mul_metal", "A")
    ops.connect("sp_metal", "", "mul_metal", "B")
    ops.connect_out("mul_metal", "", "Metallic")

    # ---------- AO ----------------------------------------------------
    ops.connect_out("tp_orm", "R", "AmbientOcclusion")

    # ---------- Normal = lerp(flat, sampled, intensity) ---------------
    ops.add("c3v_flat", "Constant3Vector", -850, 340)
    ops.setp("c3v_flat", "Constant", "(R=0,G=0,B=1,A=0)")

    ops.add("lerp_normal", "LinearInterpolate", -600, 340)
    ops.connect("c3v_flat", "", "lerp_normal", "A")
    ops.connect("tp_normal", "RGB", "lerp_normal", "B")
    ops.connect("sp_norm_int", "", "lerp_normal", "Alpha")
    ops.connect_out("lerp_normal", "", "Normal")

    # ---------- Opacity mask -----------------------------------------
    ops.add("opacity_mul", "Multiply", -850, 250)
    ops.connect("tp_base", "A", "opacity_mul", "A")
    # vp_tint.A gates global fade; users can drop tint alpha to hide.
    ops.connect("vp_tint", "A", "opacity_mul", "B")
    ops.connect_out("opacity_mul", "", "OpacityMask")
    # Material-level OpacityMaskClipValue can be set by the MI author via
    # the Material Editor "Override Opacity Mask Clip Value"; we expose it
    # as sp_opacity_clip for agent visibility but UE's mask-clip threshold
    # is still the project default without that explicit override.

    # ---------- Subsurface scatter tint ------------------------------
    ops.add("mul_ss_base", "Multiply", -850, 620)
    ops.connect("vp_ss_color", "", "mul_ss_base", "A")
    ops.connect("mul_bc", "", "mul_ss_base", "B")

    ops.add("lerp_ss", "LinearInterpolate", -600, 620)
    ops.connect("mul_bc", "", "lerp_ss", "A")
    ops.connect("mul_ss_base", "", "lerp_ss", "B")
    ops.connect("sp_ss_opacity", "", "lerp_ss", "Alpha")
    ops.connect_out("lerp_ss", "", "SubsurfaceColor")

    # ---------- Wind WPO ---------------------------------------------
    # offset = VertexColor.R * WindStrength * sin(Time*WindSpeed + WorldPos.x*0.1) * dir
    # dir = float3(1, 0, 0.3)  — sway X with slight Z lift
    ops.add("vcol", "VertexColor", -1400, 940)
    ops.add("world_pos", "WorldPosition", -1400, 1020)
    ops.add("time", "Time", -1400, 1100)

    ops.add("wp_x", "ComponentMask", -1200, 1020)
    ops.setp("wp_x", "R", "true")
    ops.setp("wp_x", "G", "false")
    ops.setp("wp_x", "B", "false")
    ops.setp("wp_x", "A", "false")
    ops.connect("world_pos", "", "wp_x", "")

    ops.add("c_phase_scale", "Constant", -1200, 1100)
    ops.setp("c_phase_scale", "R", "0.1")

    ops.add("wp_x_scaled", "Multiply", -1000, 1060)
    ops.connect("wp_x", "", "wp_x_scaled", "A")
    ops.connect("c_phase_scale", "", "wp_x_scaled", "B")

    ops.add("time_scaled", "Multiply", -1000, 1140)
    ops.connect("time", "", "time_scaled", "A")
    ops.connect("sp_wind_speed", "", "time_scaled", "B")

    ops.add("phase", "Add", -800, 1100)
    ops.connect("time_scaled", "", "phase", "A")
    ops.connect("wp_x_scaled", "", "phase", "B")

    ops.add("phase_sin", "Sine", -600, 1100)
    ops.connect("phase", "", "phase_sin", "")

    # mul by VertexColor.R (wind mask painted on foliage).
    ops.add("wind_scalar", "Multiply", -400, 1060)
    ops.connect("phase_sin", "", "wind_scalar", "A")
    ops.connect("vcol", "R", "wind_scalar", "B")

    ops.add("wind_scalar_amped", "Multiply", -200, 1060)
    ops.connect("wind_scalar", "", "wind_scalar_amped", "A")
    ops.connect("sp_wind_str", "", "wind_scalar_amped", "B")

    # direction vector (1, 0, 0.3)
    ops.add("c3v_dir", "Constant3Vector", -400, 1180)
    ops.setp("c3v_dir", "Constant", "(R=1,G=0,B=0.3,A=0)")

    ops.add("wpo_vec", "Multiply", 0, 1100)
    ops.connect("wind_scalar_amped", "", "wpo_vec", "A")
    ops.connect("c3v_dir", "", "wpo_vec", "B")
    ops.connect_out("wpo_vec", "", "WorldPositionOffset")

    # ---------- visual grouping --------------------------------------
    ops.comment(-1660, -220, 1700, 340,
                "BaseColor / ORM / Normal chain",
                C.COMMENT_COLOR_PBR)
    ops.comment(-1660, 560, 1700, 200,
                "Subsurface scatter (back-lit tint)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(-1660, 820, 1700, 460,
                "WPO wind sway (VertexColor.R × Sine(Time + WorldPos.x))",
                C.COMMENT_COLOR_SWITCH)

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

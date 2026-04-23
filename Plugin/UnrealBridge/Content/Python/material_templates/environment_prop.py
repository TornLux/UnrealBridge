"""M3-3: M_Environment_Prop — AAA environment / world-prop master material.

Feature set:

  Base layer          — BaseColor + ORM (O/R/M packed) + Normal
  Overlay layer       — second BaseColor + ORM + Normal, blended per-pixel
                        by VertexColor.R (painter-driven mask) shaped by
                        OverlayBlendHardness / OverlayBlendBias
  Vertex-color mask   — a single call that drives the layer mix, so VertexPaint
                        lets artists place dirt / moss / wear per-vertex
  Wetness / Puddle    — horizontal surfaces (VertexNormal.Z > threshold)
                        receive darkened base color + roughness → 0.05

Everything is gated behind StaticSwitchParameters (UseOverlay / UseWetness) so
an MI with both switches off compiles to a tight PBR core (~80 instr).

Budget targets (SM5 / High): <= 200 instructions, <= 8 samplers, <= 4 lookups.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


# Canvas columns
X_PARAM = -1700
X_UV = -1700
X_TEX = -1350
X_DERIVE1 = -1000
X_DERIVE2 = -700
X_DERIVE3 = -420
X_SWITCH = -140
X_OUTPUT = 180

Y_BASE = -700
Y_OVERLAY = -100
Y_MASK = 400
Y_WETNESS = 800


def _row(base: int, i: int, spacing: int = 80) -> int:
    return base + i * spacing


def build(master_path: str = "/Game/BridgeTemplates/M_Environment_Prop",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Environment_Prop_Test",
          preview_png: Optional[str] = None,
          preview_mesh: str = C.DEFAULT_PREVIEW_MESH,
          preview_lighting: str = C.DEFAULT_PREVIEW_LIGHTING,
          preview_resolution: int = 512,
          preview_camera_yaw: float = 35.0,
          preview_camera_pitch: float = -15.0,
          preview_camera_distance: float = 0.0,
          instr_budget: int = 200,
          sampler_budget: int = 8,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Generate the Environment_Prop master + optional test MI + preview."""
    L = unreal.UnrealBridgeMaterialLibrary

    master_path = C.ensure_master_material(
        master_path, "Surface", "DefaultLit", "Opaque",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    ops = C.OpList()

    # ---------- parameters ---------------------------------------------
    C.add_vector_param(ops, "vp_bc_tint", X_PARAM, _row(Y_BASE, 0),
                       "BaseColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Base", sort_priority=1)
    C.add_scalar_param(ops, "sp_rough_min", X_PARAM, _row(Y_BASE, 1),
                       "RoughnessMin", 0.15, group="01 Base", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_rough_max", X_PARAM, _row(Y_BASE, 2),
                       "RoughnessMax", 0.9, group="01 Base", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_metal_mult", X_PARAM, _row(Y_BASE, 3),
                       "MetallicScale", 0.0, group="01 Base", sort_priority=4,
                       slider_min=0.0, slider_max=2.0)
    C.add_scalar_param(ops, "sp_normal_int", X_PARAM, _row(Y_BASE, 4),
                       "NormalIntensity", 1.0, group="01 Base", sort_priority=5,
                       slider_min=0.0, slider_max=2.0)

    C.add_vector_param(ops, "vp_ov_tint", X_PARAM, _row(Y_OVERLAY, 0),
                       "OverlayColorTint", "(R=1,G=1,B=1,A=1)",
                       group="02 Overlay", sort_priority=1)
    C.add_scalar_param(ops, "sp_ov_blend_bias", X_PARAM, _row(Y_OVERLAY, 1),
                       "OverlayBlendBias", 0.0,
                       group="02 Overlay", sort_priority=2,
                       slider_min=-1.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_ov_blend_hard", X_PARAM, _row(Y_OVERLAY, 2),
                       "OverlayBlendHardness", 2.0,
                       group="02 Overlay", sort_priority=3,
                       slider_min=1.0, slider_max=10.0)

    C.add_scalar_param(ops, "sp_wet_amount", X_PARAM, _row(Y_WETNESS, 0),
                       "WetnessAmount", 0.8,
                       group="03 Wetness", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_wet_threshold", X_PARAM, _row(Y_WETNESS, 1),
                       "WetnessNormalThreshold", 0.4,
                       group="03 Wetness", sort_priority=2,
                       slider_min=-1.0, slider_max=1.0)

    # ---------- textures + UV ------------------------------------------
    ops.add("uv0", "TextureCoordinate", X_UV, Y_MASK + 200)

    C.add_texture_param_2d(ops, "tp_bc_base", X_TEX, Y_BASE,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=1)
    C.add_texture_param_2d(ops, "tp_orm_base", X_TEX, Y_BASE + 220,
                           "ORMTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Masks",
                           group="Textures", sort_priority=2)
    C.add_texture_param_2d(ops, "tp_normal_base", X_TEX, Y_BASE + 440,
                           "NormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=3)

    C.add_texture_param_2d(ops, "tp_bc_overlay", X_TEX, Y_OVERLAY,
                           "OverlayColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=4)
    C.add_texture_param_2d(ops, "tp_orm_overlay", X_TEX, Y_OVERLAY + 220,
                           "OverlayORMTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Masks",
                           group="Textures", sort_priority=5)
    C.add_texture_param_2d(ops, "tp_normal_overlay", X_TEX, Y_OVERLAY + 440,
                           "OverlayNormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=6)

    for tex in ("tp_bc_base", "tp_orm_base", "tp_normal_base",
                "tp_bc_overlay", "tp_orm_overlay", "tp_normal_overlay"):
        ops.connect("uv0", "", tex, "UVs")

    # ---------- static switches ----------------------------------------
    C.add_static_switch_param(ops, "ss_use_overlay",
                              X_SWITCH, _row(Y_OVERLAY, 0),
                              "UseOverlay", default=False,
                              group="02 Overlay", sort_priority=0)
    C.add_static_switch_param(ops, "ss_bc_wet",
                              X_SWITCH, _row(Y_WETNESS - 140, 0),
                              "UseWetness", default=False,
                              group="03 Wetness", sort_priority=0)
    C.add_static_switch_param(ops, "ss_rough_wet",
                              X_SWITCH, _row(Y_WETNESS, 0),
                              "UseWetness", default=False,
                              group="03 Wetness", sort_priority=0)
    C.add_static_switch_param(ops, "ss_norm_wet",
                              X_SWITCH, _row(Y_WETNESS + 140, 0),
                              "UseWetness", default=False,
                              group="03 Wetness", sort_priority=0)

    # ---------- base PBR chain -----------------------------------------
    ops.add("mul_bc_tint", "Multiply", X_DERIVE1, Y_BASE)
    ops.connect("tp_bc_base", "RGB", "mul_bc_tint", "A")
    ops.connect("vp_bc_tint", "", "mul_bc_tint", "B")

    ops.add("lerp_rough_base", "LinearInterpolate", X_DERIVE1, Y_BASE + 220)
    ops.connect("sp_rough_min", "", "lerp_rough_base", "A")
    ops.connect("sp_rough_max", "", "lerp_rough_base", "B")
    ops.connect("tp_orm_base", "G", "lerp_rough_base", "Alpha")

    ops.add("mul_metal_base", "Multiply", X_DERIVE1, Y_BASE + 340)
    ops.connect("tp_orm_base", "B", "mul_metal_base", "A")
    ops.connect("sp_metal_mult", "", "mul_metal_base", "B")

    # Base normal — unpacked sample × intensity (Lerp(flatN, N, intensity)).
    ops.add("c3v_flat_normal", "Constant3Vector", X_DERIVE1, Y_BASE + 540)
    ops.setp("c3v_flat_normal", "Constant", "(R=0,G=0,B=1,A=0)")
    ops.add("lerp_normal_base", "LinearInterpolate", X_DERIVE2, Y_BASE + 440)
    ops.connect("c3v_flat_normal", "", "lerp_normal_base", "A")
    ops.connect("tp_normal_base", "RGB", "lerp_normal_base", "B")
    ops.connect("sp_normal_int", "", "lerp_normal_base", "Alpha")

    # ---------- overlay PBR chain --------------------------------------
    ops.add("mul_bc_overlay_tint", "Multiply", X_DERIVE1, Y_OVERLAY)
    ops.connect("tp_bc_overlay", "RGB", "mul_bc_overlay_tint", "A")
    ops.connect("vp_ov_tint", "", "mul_bc_overlay_tint", "B")

    ops.add("lerp_rough_overlay", "LinearInterpolate", X_DERIVE1, Y_OVERLAY + 220)
    ops.connect("sp_rough_min", "", "lerp_rough_overlay", "A")
    ops.connect("sp_rough_max", "", "lerp_rough_overlay", "B")
    ops.connect("tp_orm_overlay", "G", "lerp_rough_overlay", "Alpha")

    ops.add("mul_metal_overlay", "Multiply", X_DERIVE1, Y_OVERLAY + 340)
    ops.connect("tp_orm_overlay", "B", "mul_metal_overlay", "A")
    ops.connect("sp_metal_mult", "", "mul_metal_overlay", "B")

    ops.add("lerp_normal_overlay", "LinearInterpolate", X_DERIVE2, Y_OVERLAY + 440)
    ops.connect("c3v_flat_normal", "", "lerp_normal_overlay", "A")
    ops.connect("tp_normal_overlay", "RGB", "lerp_normal_overlay", "B")
    ops.connect("sp_normal_int", "", "lerp_normal_overlay", "Alpha")

    # ---------- vertex-color blend mask --------------------------------
    # blend_alpha = saturate( (VertexColor.R + OverlayBlendBias) * OverlayBlendHardness )
    ops.add("vc", "VertexColor", X_PARAM, _row(Y_MASK, 0))

    ops.add("add_mask_bias", "Add", X_DERIVE1, Y_MASK)
    ops.connect("vc", "R", "add_mask_bias", "A")
    ops.connect("sp_ov_blend_bias", "", "add_mask_bias", "B")

    ops.add("mul_mask_hard", "Multiply", X_DERIVE2, Y_MASK)
    ops.connect("add_mask_bias", "", "mul_mask_hard", "A")
    ops.connect("sp_ov_blend_hard", "", "mul_mask_hard", "B")

    ops.add("sat_mask", "Saturate", X_DERIVE3, Y_MASK)
    ops.connect("mul_mask_hard", "", "sat_mask", "")

    # Overlay-layer blend — always built; the UseOverlay switch picks whether
    # the base or the blended result feeds the main outputs.
    ops.add("lerp_bc_ov", "LinearInterpolate", X_DERIVE3, Y_OVERLAY)
    ops.connect("mul_bc_tint", "", "lerp_bc_ov", "A")
    ops.connect("mul_bc_overlay_tint", "", "lerp_bc_ov", "B")
    ops.connect("sat_mask", "", "lerp_bc_ov", "Alpha")

    ops.add("lerp_rough_ov", "LinearInterpolate", X_DERIVE3, Y_OVERLAY + 220)
    ops.connect("lerp_rough_base", "", "lerp_rough_ov", "A")
    ops.connect("lerp_rough_overlay", "", "lerp_rough_ov", "B")
    ops.connect("sat_mask", "", "lerp_rough_ov", "Alpha")

    ops.add("lerp_metal_ov", "LinearInterpolate", X_DERIVE3, Y_OVERLAY + 340)
    ops.connect("mul_metal_base", "", "lerp_metal_ov", "A")
    ops.connect("mul_metal_overlay", "", "lerp_metal_ov", "B")
    ops.connect("sat_mask", "", "lerp_metal_ov", "Alpha")

    ops.add("lerp_normal_ov", "LinearInterpolate", X_DERIVE3, Y_OVERLAY + 440)
    ops.connect("lerp_normal_base", "", "lerp_normal_ov", "A")
    ops.connect("lerp_normal_overlay", "", "lerp_normal_ov", "B")
    ops.connect("sat_mask", "", "lerp_normal_ov", "Alpha")

    # Overlay static switches (one per output): True = overlay-blended, False = base.
    C.add_static_switch_param(ops, "ss_bc_ov", X_SWITCH, Y_OVERLAY,
                              "UseOverlay", default=False,
                              group="02 Overlay", sort_priority=0)
    ops.connect("lerp_bc_ov", "", "ss_bc_ov", "True")
    ops.connect("mul_bc_tint", "", "ss_bc_ov", "False")

    C.add_static_switch_param(ops, "ss_rough_ov", X_SWITCH, Y_OVERLAY + 220,
                              "UseOverlay", default=False,
                              group="02 Overlay", sort_priority=0)
    ops.connect("lerp_rough_ov", "", "ss_rough_ov", "True")
    ops.connect("lerp_rough_base", "", "ss_rough_ov", "False")

    C.add_static_switch_param(ops, "ss_metal_ov", X_SWITCH, Y_OVERLAY + 340,
                              "UseOverlay", default=False,
                              group="02 Overlay", sort_priority=0)
    ops.connect("lerp_metal_ov", "", "ss_metal_ov", "True")
    ops.connect("mul_metal_base", "", "ss_metal_ov", "False")

    C.add_static_switch_param(ops, "ss_normal_ov", X_SWITCH, Y_OVERLAY + 440,
                              "UseOverlay", default=False,
                              group="02 Overlay", sort_priority=0)
    ops.connect("lerp_normal_ov", "", "ss_normal_ov", "True")
    ops.connect("lerp_normal_base", "", "ss_normal_ov", "False")

    # ---------- wetness / puddle chain --------------------------------
    # puddle_mask = saturate((VertexNormalWS.z - threshold) / (1 - threshold)) * WetnessAmount
    # We use VertexNormalWS because wetness pools on up-facing surfaces; a small
    # bias keeps slanted surfaces partially wet.
    ops.add("vn_ws", "VertexNormalWS", X_PARAM, _row(Y_WETNESS, 3))

    ops.add("mask_vn_z", "ComponentMask", X_DERIVE1, _row(Y_WETNESS, 3))
    # ComponentMask property bool fields are R/G/B/A.
    ops.setp("mask_vn_z", "R", "false")
    ops.setp("mask_vn_z", "G", "false")
    ops.setp("mask_vn_z", "B", "true")
    ops.setp("mask_vn_z", "A", "false")
    ops.connect("vn_ws", "", "mask_vn_z", "")

    ops.add("sub_vn_threshold", "Subtract", X_DERIVE2, _row(Y_WETNESS, 3))
    ops.connect("mask_vn_z", "", "sub_vn_threshold", "A")
    ops.connect("sp_wet_threshold", "", "sub_vn_threshold", "B")

    ops.add("const_one_b", "Constant", X_DERIVE1, _row(Y_WETNESS, 4))
    ops.setp("const_one_b", "R", "1.0")
    ops.add("sub_one_threshold", "Subtract", X_DERIVE2, _row(Y_WETNESS, 4))
    ops.connect("const_one_b", "", "sub_one_threshold", "A")
    ops.connect("sp_wet_threshold", "", "sub_one_threshold", "B")

    ops.add("div_vn_norm", "Divide", X_DERIVE3, _row(Y_WETNESS, 3))
    ops.connect("sub_vn_threshold", "", "div_vn_norm", "A")
    # Add tiny epsilon to the denominator by not caring — user must keep
    # threshold < 1 (enforced by slider 0..1 minus a hair).
    ops.connect("sub_one_threshold", "", "div_vn_norm", "B")

    ops.add("sat_puddle_raw", "Saturate", X_DERIVE3, _row(Y_WETNESS, 4))
    ops.connect("div_vn_norm", "", "sat_puddle_raw", "")

    ops.add("mul_puddle_amount", "Multiply", X_DERIVE3, _row(Y_WETNESS, 5))
    ops.connect("sat_puddle_raw", "", "mul_puddle_amount", "A")
    ops.connect("sp_wet_amount", "", "mul_puddle_amount", "B")

    # Apply wetness: darken base-color + flatten roughness + flatten normal toward flat
    ops.add("const_wet_color_floor", "Constant", X_DERIVE1, _row(Y_WETNESS, 6))
    ops.setp("const_wet_color_floor", "R", "0.55")
    ops.add("lerp_wet_color_coef", "LinearInterpolate", X_DERIVE2, _row(Y_WETNESS, 6))
    ops.connect("const_one_b", "", "lerp_wet_color_coef", "A")
    ops.connect("const_wet_color_floor", "", "lerp_wet_color_coef", "B")
    ops.connect("mul_puddle_amount", "", "lerp_wet_color_coef", "Alpha")

    ops.add("mul_bc_wet", "Multiply", X_DERIVE3, _row(Y_WETNESS - 150, 0))
    ops.connect("ss_bc_ov", "", "mul_bc_wet", "A")
    ops.connect("lerp_wet_color_coef", "", "mul_bc_wet", "B")

    ops.add("const_rough_wet_floor", "Constant", X_DERIVE2, _row(Y_WETNESS, 7))
    ops.setp("const_rough_wet_floor", "R", "0.05")
    ops.add("lerp_rough_wet", "LinearInterpolate", X_DERIVE3, _row(Y_WETNESS, 0))
    ops.connect("ss_rough_ov", "", "lerp_rough_wet", "A")
    ops.connect("const_rough_wet_floor", "", "lerp_rough_wet", "B")
    ops.connect("mul_puddle_amount", "", "lerp_rough_wet", "Alpha")

    # Normal flatten under puddles — puddles look smoother than sub-strate.
    ops.add("lerp_norm_wet", "LinearInterpolate", X_DERIVE3, _row(Y_WETNESS + 140, 0))
    ops.connect("ss_normal_ov", "", "lerp_norm_wet", "A")
    ops.connect("c3v_flat_normal", "", "lerp_norm_wet", "B")
    ops.connect("mul_puddle_amount", "", "lerp_norm_wet", "Alpha")

    # Static-switch gate (UseWetness): True=wet variant, False=dry overlay result.
    ops.connect("mul_bc_wet", "", "ss_bc_wet", "True")
    ops.connect("ss_bc_ov", "", "ss_bc_wet", "False")

    ops.connect("lerp_rough_wet", "", "ss_rough_wet", "True")
    ops.connect("ss_rough_ov", "", "ss_rough_wet", "False")

    ops.connect("lerp_norm_wet", "", "ss_norm_wet", "True")
    ops.connect("ss_normal_ov", "", "ss_norm_wet", "False")

    # ---------- main outputs --------------------------------------------
    ops.connect_out("ss_bc_wet", "", "BaseColor")
    ops.connect_out("ss_metal_ov", "", "Metallic")
    ops.connect_out("ss_rough_wet", "", "Roughness")
    ops.connect_out("tp_orm_base", "R", "AmbientOcclusion")
    ops.connect_out("ss_norm_wet", "", "Normal")

    # ---------- visual grouping -----------------------------------------
    ops.comment(X_TEX - 60, Y_BASE - 110, 1700, 620,
                "Base layer (BaseColor/ORM/Normal)",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 60, Y_OVERLAY - 110, 1700, 640,
                "Overlay layer (VertexColor.R-driven blend)",
                C.COMMENT_COLOR_WEAR)
    ops.comment(X_PARAM - 60, Y_MASK - 60, 1000, 200,
                "Blend mask — saturate((VC.R + bias) * hardness)",
                C.COMMENT_COLOR_SWITCH)
    ops.comment(X_TEX - 60, Y_WETNESS - 60, 1700, 760,
                "Wetness — VertexNormalWS.Z-driven puddle mask",
                C.COMMENT_COLOR_WETNESS)

    # 4. Flush + compile
    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")

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
            "vt_stack_count": stats["vt_stack_count"],
            "compile_errors": stats["compile_errors"],
            "compile_clean": len(stats["compile_errors"]) == 0,
            "num_expressions": stats["num_expressions"],
            "over_instr": budget["over_instr"],
            "over_sampler": budget["over_sampler"],
            "ok": budget["ok"],
        })

    if mi_path:
        if unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            out["mi_path"] = mi_path
        else:
            mi_result = unreal.UnrealBridgeMaterialLibrary.create_material_instance(
                master_path, mi_path)
            if mi_result.success:
                out["mi_path"] = str(mi_result.path) or mi_path
            else:
                out["mi_error"] = mi_result.error

    if preview_png:
        preview_target = out.get("mi_path") or master_path
        ok = unreal.UnrealBridgeMaterialLibrary.preview_material(
            preview_target, preview_mesh, preview_lighting,
            preview_resolution,
            preview_camera_yaw, preview_camera_pitch, preview_camera_distance,
            preview_png)
        out["preview_ok"] = bool(ok)
        out["preview_png"] = preview_png

    return out

"""M4-5: PP_ColorGradeLUT_Extended — 3D-style LUT + 3-zone grading + saturation.

Combines three AAA-standard grading primitives in one PP material:

  1. Optional 2D-unwrapped LUT (256x16 neutral-LUT layout). Sampled with
     dual-tap + frac-blue lerp so banding stays subtle even with 16-slice
     depth. Gated by ``UseLUT`` so the default-off state dead-codes the
     LUT subgraph — no runtime cost, no compile dependency on an MI-bound
     texture. When the switch flips on, the MI must bind a real 256x16
     LUT via ``LUT_Texture``; leaving it at the engine white placeholder
     produces a white screen.
  2. 3-zone tone control via per-channel gain (``ShadowGain`` /
     ``MidtoneGain`` / ``HighlightGain``). Masks are luminance-driven
     piecewise linears with fixed pivots at 0.33 / 0.66 that sum to 1.
  3. Saturation via ``lerp(gray, color, Saturation)`` — 0 = fully
     desaturated, 1 = pass-through, 2 = boosted.

Final:

    base        = UseLUT ? SampleLUT(saturate(scene)) : saturate(scene)
    zone_tint   = ShadowGain*Smask + MidtoneGain*Mmask + HighlightGain*Hmask
    toned       = base * zone_tint
    graded      = lerp(dot(toned, Rec709), toned, Saturation)
    output      = lerp(scene, graded, Strength)

Parameters:

  Strength           — overall blend 0..1 (default 1)
  UseLUT             — static switch for LUT path (default false)
  LUT_Texture        — 256x16 neutral LUT (default white placeholder)
  ShadowGain         — RGB gain on luminance < 0.33 (default 1,1,1)
  MidtoneGain        — RGB gain on luminance in [0.33, 0.66] (default 1,1,1)
  HighlightGain      — RGB gain on luminance > 0.66 (default 1,1,1)
  Saturation         — 0..2 (default 1)
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/PP_ColorGradeLUT_Extended",
          apply_to_volume: str = "",
          apply_weight: float = 1.0,
          blendable_location: str = "AfterTonemapping",
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    exists = unreal.EditorAssetLibrary.does_asset_exist(master_path)
    if exists:
        if not rebuild:
            raise RuntimeError(
                f"post-process material already exists at '{master_path}' — "
                f"pass rebuild=True to wipe its graph and regenerate")
        C.clear_material_graph(master_path)
    else:
        r = L.create_post_process_material(master_path, blendable_location, False)
        if not r.success:
            raise RuntimeError(f"create_post_process_material failed: {r.error}")
        master_path = str(r.path) or master_path

    ops = C.OpList()

    # ── scene input ─────────────────────────────────────────────────
    ops.add("scene", "SceneTexture", -1800, 0)
    ops.setp("scene", "SceneTextureId", "PPI_PostProcessInput0")

    ops.add("scene_rgb", "ComponentMask", -1550, 0)
    ops.setp("scene_rgb", "R", "true")
    ops.setp("scene_rgb", "G", "true")
    ops.setp("scene_rgb", "B", "true")
    ops.setp("scene_rgb", "A", "false")
    ops.connect("scene", "Color", "scene_rgb", "")

    ops.add("scene_clamped", "Saturate", -1350, 0)
    ops.connect("scene_rgb", "", "scene_clamped", "")

    # ── parameters ──────────────────────────────────────────────────
    C.add_scalar_param(ops, "sp_strength", -1800, 220, "Strength", 1.0,
                       group="01 Grade", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)

    C.add_static_switch_param(ops, "ss_use_lut", -1800, 300,
                              "UseLUT", default=False,
                              group="02 LUT", sort_priority=1)

    # LUT texture object — sampled twice below with different UVs for blue-axis
    # interpolation. TextureObjectParameter exposes the asset reference without
    # a dedicated sampler, so the two TextureSample nodes share the binding.
    # Note: TextureObjectParameter inherits from UMaterialExpressionTextureSample,
    # so M5-5 counts its SamplerSource — keep it aligned with the shared-wrap
    # setting used on the downstream samples.
    ops.add("tp_lut_obj", "TextureObjectParameter", -1800, 380)
    ops.setp("tp_lut_obj", "ParameterName", "LUT_Texture")
    ops.setp("tp_lut_obj", "Texture",
             "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    ops.setp("tp_lut_obj", "SamplerType", "SAMPLERTYPE_LinearColor")
    ops.setp("tp_lut_obj", "SamplerSource", "SSM_Wrap_WorldGroupSettings")
    ops.setp("tp_lut_obj", "Group", "02 LUT")
    ops.setp("tp_lut_obj", "SortPriority", "2")

    C.add_vector_param(ops, "vp_shadow_gain", -1800, 500,
                       "ShadowGain", "(R=1,G=1,B=1,A=1)",
                       group="03 Zones", sort_priority=1)
    C.add_vector_param(ops, "vp_midtone_gain", -1800, 580,
                       "MidtoneGain", "(R=1,G=1,B=1,A=1)",
                       group="03 Zones", sort_priority=2)
    C.add_vector_param(ops, "vp_highlight_gain", -1800, 660,
                       "HighlightGain", "(R=1,G=1,B=1,A=1)",
                       group="03 Zones", sort_priority=3)

    C.add_scalar_param(ops, "sp_saturation", -1800, 760,
                       "Saturation", 1.0,
                       group="04 Saturation", sort_priority=1,
                       slider_min=0.0, slider_max=2.0)

    # ── LUT sampling: 2D unwrapped 256x16 layout ────────────────────
    # c_rg = saturate(scene).rg  (becomes UV within the 16x16 tile)
    # c_b  = saturate(scene).b   (picks which 16x16 tile = slice)
    ops.add("c_rg", "ComponentMask", -1150, -60)
    ops.setp("c_rg", "R", "true")
    ops.setp("c_rg", "G", "true")
    ops.setp("c_rg", "B", "false")
    ops.setp("c_rg", "A", "false")
    ops.connect("scene_clamped", "", "c_rg", "")

    ops.add("c_b", "ComponentMask", -1150, 20)
    ops.setp("c_b", "R", "false")
    ops.setp("c_b", "G", "false")
    ops.setp("c_b", "B", "true")
    ops.setp("c_b", "A", "false")
    ops.connect("scene_clamped", "", "c_b", "")

    # slice_f in [0, 15]; slice0 = floor(slice_f); slice1 = min(slice0+1, 15)
    ops.add("c_15", "Constant", -1350, 100)
    ops.setp("c_15", "R", "15.0")

    ops.add("slice_f", "Multiply", -950, 20)
    ops.connect("c_b", "", "slice_f", "A")
    ops.connect("c_15", "", "slice_f", "B")

    ops.add("slice0", "Floor", -750, 20)
    ops.connect("slice_f", "", "slice0", "")

    ops.add("c_1", "Constant", -950, 100)
    ops.setp("c_1", "R", "1.0")

    ops.add("slice0_plus_1", "Add", -550, 60)
    ops.connect("slice0", "", "slice0_plus_1", "A")
    ops.connect("c_1", "", "slice0_plus_1", "B")

    ops.add("slice1", "Min", -350, 60)
    ops.connect("slice0_plus_1", "", "slice1", "A")
    ops.connect("c_15", "", "slice1", "B")

    ops.add("frac_b", "Subtract", -550, 140)
    ops.connect("slice_f", "", "frac_b", "A")
    ops.connect("slice0", "", "frac_b", "B")

    # Inside-tile UV: rg * (15/16) + (0.5/16) — avoids bleeding into the
    # neighbouring tile on bilinear filtering.
    ops.add("c_tile_scale", "Constant", -950, -140)
    ops.setp("c_tile_scale", "R", "0.9375")  # = 15/16

    ops.add("c_tile_bias", "Constant", -950, -80)
    ops.setp("c_tile_bias", "R", "0.03125")  # = 0.5/16

    ops.add("uv_tile_scaled", "Multiply", -750, -100)
    ops.connect("c_rg", "", "uv_tile_scaled", "A")
    ops.connect("c_tile_scale", "", "uv_tile_scaled", "B")

    ops.add("uv_in_tile", "Add", -550, -100)
    ops.connect("uv_tile_scaled", "", "uv_in_tile", "A")
    ops.connect("c_tile_bias", "", "uv_in_tile", "B")

    ops.add("uv_x", "ComponentMask", -350, -140)
    ops.setp("uv_x", "R", "true")
    ops.setp("uv_x", "G", "false")
    ops.setp("uv_x", "B", "false")
    ops.setp("uv_x", "A", "false")
    ops.connect("uv_in_tile", "", "uv_x", "")

    ops.add("uv_y", "ComponentMask", -350, -60)
    ops.setp("uv_y", "R", "false")
    ops.setp("uv_y", "G", "true")
    ops.setp("uv_y", "B", "false")
    ops.setp("uv_y", "A", "false")
    ops.connect("uv_in_tile", "", "uv_y", "")

    # uvN.x = (sliceN + uv_x) / 16
    ops.add("c_16", "Constant", -150, 240)
    ops.setp("c_16", "R", "16.0")

    ops.add("s0_plus_x", "Add", -150, 60)
    ops.connect("slice0", "", "s0_plus_x", "A")
    ops.connect("uv_x", "", "s0_plus_x", "B")

    ops.add("s1_plus_x", "Add", -150, 140)
    ops.connect("slice1", "", "s1_plus_x", "A")
    ops.connect("uv_x", "", "s1_plus_x", "B")

    ops.add("uv0_x", "Divide", 50, 60)
    ops.connect("s0_plus_x", "", "uv0_x", "A")
    ops.connect("c_16", "", "uv0_x", "B")

    ops.add("uv1_x", "Divide", 50, 140)
    ops.connect("s1_plus_x", "", "uv1_x", "A")
    ops.connect("c_16", "", "uv1_x", "B")

    ops.add("uv0", "AppendVector", 250, 30)
    ops.connect("uv0_x", "", "uv0", "A")
    ops.connect("uv_y", "", "uv0", "B")

    ops.add("uv1", "AppendVector", 250, 110)
    ops.connect("uv1_x", "", "uv1", "A")
    ops.connect("uv_y", "", "uv1", "B")

    # Two samples off the shared LUT_Texture TextureObject. SamplerType must
    # match the object-param declaration (LinearColor) or the compiler screams.
    ops.add("lut_s0", "TextureSample", 450, 30)
    ops.setp("lut_s0", "SamplerType", "SAMPLERTYPE_LinearColor")
    ops.setp("lut_s0", "SamplerSource", "SSM_Wrap_WorldGroupSettings")
    ops.connect("tp_lut_obj", "", "lut_s0", "TextureObject")
    ops.connect("uv0", "", "lut_s0", "UVs")

    ops.add("lut_s1", "TextureSample", 450, 200)
    ops.setp("lut_s1", "SamplerType", "SAMPLERTYPE_LinearColor")
    ops.setp("lut_s1", "SamplerSource", "SSM_Wrap_WorldGroupSettings")
    ops.connect("tp_lut_obj", "", "lut_s1", "TextureObject")
    ops.connect("uv1", "", "lut_s1", "UVs")

    ops.add("lut_rgb", "LinearInterpolate", 700, 110)
    ops.connect("lut_s0", "RGB", "lut_rgb", "A")
    ops.connect("lut_s1", "RGB", "lut_rgb", "B")
    ops.connect("frac_b", "", "lut_rgb", "Alpha")

    # ── LUT-enable switch ───────────────────────────────────────────
    # True  = graded LUT sample; False = clamped scene color. Dead-codes the
    # LUT sampling subgraph when UseLUT is false (default).
    ops.connect("lut_rgb", "", "ss_use_lut", "True")
    ops.connect("scene_clamped", "", "ss_use_lut", "False")

    # ── 3-zone luminance masks ──────────────────────────────────────
    ops.add("lum_coefs", "Constant3Vector", -1150, 880)
    ops.setp("lum_coefs", "Constant", "(R=0.299,G=0.587,B=0.114,A=0)")

    ops.add("lum", "DotProduct", 950, 300)
    ops.connect("ss_use_lut", "", "lum", "A")
    ops.connect("lum_coefs", "", "lum", "B")

    # shadow_mask = saturate(1 - L*3)   (pivot at L=1/3)
    ops.add("c_3_shadow", "Constant", 950, 400)
    ops.setp("c_3_shadow", "R", "3.0")

    ops.add("lum_x3_s", "Multiply", 1100, 380)
    ops.connect("lum", "", "lum_x3_s", "A")
    ops.connect("c_3_shadow", "", "lum_x3_s", "B")

    ops.add("one_minus_lum3", "OneMinus", 1250, 380)
    ops.connect("lum_x3_s", "", "one_minus_lum3", "")

    ops.add("shadow_mask", "Saturate", 1400, 380)
    ops.connect("one_minus_lum3", "", "shadow_mask", "")

    # highlight_mask = saturate((L - 0.66) * 3)   (pivot at L=2/3)
    ops.add("c_066", "Constant", 950, 500)
    ops.setp("c_066", "R", "0.666667")

    ops.add("lum_minus_066", "Subtract", 1100, 480)
    ops.connect("lum", "", "lum_minus_066", "A")
    ops.connect("c_066", "", "lum_minus_066", "B")

    ops.add("c_3_hi", "Constant", 950, 580)
    ops.setp("c_3_hi", "R", "3.0")

    ops.add("hi_scaled", "Multiply", 1250, 480)
    ops.connect("lum_minus_066", "", "hi_scaled", "A")
    ops.connect("c_3_hi", "", "hi_scaled", "B")

    ops.add("highlight_mask", "Saturate", 1400, 480)
    ops.connect("hi_scaled", "", "highlight_mask", "")

    # midtone_mask = saturate(1 - shadow_mask - highlight_mask)
    ops.add("masks_sum", "Add", 1550, 420)
    ops.connect("shadow_mask", "", "masks_sum", "A")
    ops.connect("highlight_mask", "", "masks_sum", "B")

    ops.add("one_minus_masks", "OneMinus", 1700, 420)
    ops.connect("masks_sum", "", "one_minus_masks", "")

    ops.add("midtone_mask", "Saturate", 1850, 420)
    ops.connect("one_minus_masks", "", "midtone_mask", "")

    # ── zone-weighted gain sum ──────────────────────────────────────
    ops.add("shadow_gain_w", "Multiply", 2000, 500)
    ops.connect("vp_shadow_gain", "", "shadow_gain_w", "A")
    ops.connect("shadow_mask", "", "shadow_gain_w", "B")

    ops.add("mid_gain_w", "Multiply", 2000, 580)
    ops.connect("vp_midtone_gain", "", "mid_gain_w", "A")
    ops.connect("midtone_mask", "", "mid_gain_w", "B")

    ops.add("hi_gain_w", "Multiply", 2000, 660)
    ops.connect("vp_highlight_gain", "", "hi_gain_w", "A")
    ops.connect("highlight_mask", "", "hi_gain_w", "B")

    ops.add("gain_sm", "Add", 2200, 540)
    ops.connect("shadow_gain_w", "", "gain_sm", "A")
    ops.connect("mid_gain_w", "", "gain_sm", "B")

    ops.add("zone_tint", "Add", 2400, 580)
    ops.connect("gain_sm", "", "zone_tint", "A")
    ops.connect("hi_gain_w", "", "zone_tint", "B")

    ops.add("toned", "Multiply", 2600, 300)
    ops.connect("ss_use_lut", "", "toned", "A")
    ops.connect("zone_tint", "", "toned", "B")

    # ── saturation ──────────────────────────────────────────────────
    # gray_s = dot(toned, luminance_coefs); saturated = lerp(gray_s, toned, sat)
    ops.add("gray_s", "DotProduct", 2800, 300)
    ops.connect("toned", "", "gray_s", "A")
    ops.connect("lum_coefs", "", "gray_s", "B")

    ops.add("saturated", "LinearInterpolate", 3000, 300)
    ops.connect("gray_s", "", "saturated", "A")
    ops.connect("toned", "", "saturated", "B")
    ops.connect("sp_saturation", "", "saturated", "Alpha")

    # ── final strength blend ────────────────────────────────────────
    ops.add("final", "LinearInterpolate", 3250, 200)
    ops.connect("scene_rgb", "", "final", "A")
    ops.connect("saturated", "", "final", "B")
    ops.connect("sp_strength", "", "final", "Alpha")

    ops.connect_out("final", "", "EmissiveColor")

    ops.comment(-1860, -220, 5200, 1120,
                "PP_ColorGradeLUT_Extended — optional 2D-LUT + 3-zone gain + saturation",
                C.COMMENT_COLOR_PBR)

    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")
    C.save_master(master_path)

    out: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(result.ops_applied),
    }
    if apply_weight > 0.0:
        ok = L.apply_post_process_material(apply_to_volume, master_path, apply_weight)
        out["applied"] = bool(ok)
        out["volume"] = apply_to_volume or "<first-unbound-or-spawned>"
    return out

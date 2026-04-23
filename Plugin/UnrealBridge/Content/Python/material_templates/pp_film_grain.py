"""M4-6: PP_Film_Grain_AA — animated film-grain + sub-LSB anti-band dither.

Two noise contributions added additively to the post-tonemap image:

  1. **Grain** — ``BridgeHash21(UV * GrainScale + t * TimeSpeed * (1, 0.37))``
     remapped to ``[-GrainIntensity, +GrainIntensity]``. The 0.37 y-axis
     offset decorrelates horizontal/vertical drift so the grain pattern
     doesn't shimmer in one direction. Optional luminance modulation
     (``LuminanceWeight``) peaks grain at midtones (classic cinematic
     look — grain is least visible in blacks and whites).
  2. **Dither** — second ``BridgeHash21`` tap at high frequency (pixel-
     sized cells), scaled to roughly ±``DitherStrength`` * 1 LSB at
     8-bit. Purpose: break up banding introduced by tone-mapping or
     LUT quantization. Effectively invisible but fixes gradient artefacts.

Both hashes trampoline through the shared ``BridgeSnippets.ush`` helper
so M5-11 (trivial-Custom) skips them.

Final:

    grain    = (BridgeHash21(uv_grain) - 0.5) * 2 * GrainIntensity
    midmask  = 4 * L * (1 - L)                       # parabola peak at L=0.5
    grain_m  = grain * lerp(1, midmask, LuminanceWeight)
    dither   = (BridgeHash21(uv_dither) - 0.5) * DitherStrength / 127.5
    output   = lerp(scene, scene + grain_m + dither, Strength)

Parameters:

  Strength         — overall blend 0..1 (default 1)
  GrainIntensity   — amplitude of signed grain, 0..0.2 (default 0.04)
  GrainScale       — hash-space UV scale, higher = finer grain (default 1200)
  TimeSpeed        — animation drift rate (default 15)
  LuminanceWeight  — 0 = constant grain, 1 = grain peaks at midtones (default 1)
  DitherStrength   — anti-band dither in LSBs, 0..4 (default 1)
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/PP_Film_Grain_AA",
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

    # ── Custom HLSL trampolines (out-of-band, then registered into OpList) ──
    grain_custom = L.add_custom_expression(
        master_path, 700, 340,
        ["P"],
        "Float1",
        "return BridgeHash21(P);",
        ["/Plugin/UnrealBridge/BridgeSnippets.ush"],
        "Film-grain hash tap")
    if not grain_custom.success:
        raise RuntimeError(f"grain custom node failed: {grain_custom.error}")

    dither_custom = L.add_custom_expression(
        master_path, 700, 860,
        ["P"],
        "Float1",
        "return BridgeHash21(P);",
        ["/Plugin/UnrealBridge/BridgeSnippets.ush"],
        "Dither hash tap")
    if not dither_custom.success:
        raise RuntimeError(f"dither custom node failed: {dither_custom.error}")

    ops = C.OpList()
    ops.add_literal(C.guid_to_str(grain_custom.guid), "grain_hash")
    ops.add_literal(C.guid_to_str(dither_custom.guid), "dither_hash")

    # ── scene + screen UV + time ────────────────────────────────────
    ops.add("scene", "SceneTexture", -1600, 0)
    ops.setp("scene", "SceneTextureId", "PPI_PostProcessInput0")

    ops.add("scene_rgb", "ComponentMask", -1350, 0)
    ops.setp("scene_rgb", "R", "true")
    ops.setp("scene_rgb", "G", "true")
    ops.setp("scene_rgb", "B", "true")
    ops.setp("scene_rgb", "A", "false")
    ops.connect("scene", "Color", "scene_rgb", "")

    ops.add("screen_uv", "ScreenPosition", -1600, 240)

    ops.add("time", "Time", -1600, 400)

    # ── parameters ──────────────────────────────────────────────────
    C.add_scalar_param(ops, "sp_strength", -1600, 540, "Strength", 1.0,
                       group="01 Grain", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_intensity", -1600, 620, "GrainIntensity", 0.04,
                       group="01 Grain", sort_priority=2,
                       slider_min=0.0, slider_max=0.2)
    C.add_scalar_param(ops, "sp_scale", -1600, 700, "GrainScale", 1200.0,
                       group="01 Grain", sort_priority=3,
                       slider_min=100.0, slider_max=4000.0)
    C.add_scalar_param(ops, "sp_speed", -1600, 780, "TimeSpeed", 15.0,
                       group="01 Grain", sort_priority=4,
                       slider_min=0.0, slider_max=60.0)
    C.add_scalar_param(ops, "sp_lum_w", -1600, 860, "LuminanceWeight", 1.0,
                       group="01 Grain", sort_priority=5,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_dither", -1600, 940, "DitherStrength", 1.0,
                       group="02 Dither", sort_priority=1,
                       slider_min=0.0, slider_max=4.0)

    # ── constants ───────────────────────────────────────────────────
    ops.add("c_half", "Constant", -1600, 1040)
    ops.setp("c_half", "R", "0.5")
    ops.add("c_two", "Constant", -1600, 1100)
    ops.setp("c_two", "R", "2.0")
    ops.add("c_one", "Constant", -1600, 1160)
    ops.setp("c_one", "R", "1.0")
    ops.add("c_four", "Constant", -1600, 1220)
    ops.setp("c_four", "R", "4.0")
    # 1 / 127.5 ≈ 0.00784 — signed noise scale so ±DitherStrength LSBs at 8-bit.
    ops.add("c_inv_lsb", "Constant", -1600, 1280)
    ops.setp("c_inv_lsb", "R", "0.00784314")

    ops.add("c_lum_coefs", "Constant3Vector", -1350, 80)
    ops.setp("c_lum_coefs", "Constant", "(R=0.299,G=0.587,B=0.114,A=0)")

    # Constant2Vector used to decorrelate x vs y time drift.
    ops.add("c_drift_dir", "Constant2Vector", -1350, 460)
    ops.setp("c_drift_dir", "R", "1.0")
    ops.setp("c_drift_dir", "G", "0.37")

    # Dither UV time seed — large prime-ish values, different from grain.
    ops.add("c_dither_drift_dir", "Constant2Vector", -1350, 940)
    ops.setp("c_dither_drift_dir", "R", "41.0")
    ops.setp("c_dither_drift_dir", "G", "53.0")
    ops.add("c_dither_scale", "Constant", -1350, 1020)
    ops.setp("c_dither_scale", "R", "4096.0")

    # ── grain-hash UV ───────────────────────────────────────────────
    # uv_scaled = screen_uv.ViewportUV * GrainScale
    ops.add("grain_uv_scaled", "Multiply", -1100, 260)
    ops.connect("screen_uv", "ViewportUV", "grain_uv_scaled", "A")
    ops.connect("sp_scale", "", "grain_uv_scaled", "B")

    # time_x_speed = time * TimeSpeed
    ops.add("time_x_speed", "Multiply", -1100, 420)
    ops.connect("time", "", "time_x_speed", "A")
    ops.connect("sp_speed", "", "time_x_speed", "B")

    # time_drift_v2 = (time*speed) * (1, 0.37)
    ops.add("time_drift_v2", "Multiply", -900, 440)
    ops.connect("time_x_speed", "", "time_drift_v2", "A")
    ops.connect("c_drift_dir", "", "time_drift_v2", "B")

    # grain_hash_input = uv_scaled + time_drift_v2
    ops.add("grain_hash_input", "Add", -700, 340)
    ops.connect("grain_uv_scaled", "", "grain_hash_input", "A")
    ops.connect("time_drift_v2", "", "grain_hash_input", "B")

    ops.connect("grain_hash_input", "", "grain_hash", "P")

    # ── grain signed ───────────────────────────────────────────────
    ops.add("grain_minus_half", "Subtract", 900, 340)
    ops.connect("grain_hash", "", "grain_minus_half", "A")
    ops.connect("c_half", "", "grain_minus_half", "B")

    ops.add("grain_x2", "Multiply", 1080, 340)
    ops.connect("grain_minus_half", "", "grain_x2", "A")
    ops.connect("c_two", "", "grain_x2", "B")

    ops.add("grain_signed", "Multiply", 1260, 340)
    ops.connect("grain_x2", "", "grain_signed", "A")
    ops.connect("sp_intensity", "", "grain_signed", "B")

    # ── luminance modulation ───────────────────────────────────────
    ops.add("lum", "DotProduct", -1100, 30)
    ops.connect("scene_rgb", "", "lum", "A")
    ops.connect("c_lum_coefs", "", "lum", "B")

    ops.add("one_minus_lum", "OneMinus", -900, 30)
    ops.connect("lum", "", "one_minus_lum", "")

    ops.add("l_x_1ml", "Multiply", -700, 30)
    ops.connect("lum", "", "l_x_1ml", "A")
    ops.connect("one_minus_lum", "", "l_x_1ml", "B")

    ops.add("mid_weight", "Multiply", -500, 30)
    ops.connect("l_x_1ml", "", "mid_weight", "A")
    ops.connect("c_four", "", "mid_weight", "B")

    ops.add("lum_factor", "LinearInterpolate", -300, 30)
    ops.connect("c_one", "", "lum_factor", "A")
    ops.connect("mid_weight", "", "lum_factor", "B")
    ops.connect("sp_lum_w", "", "lum_factor", "Alpha")

    ops.add("grain_mod", "Multiply", 1440, 340)
    ops.connect("grain_signed", "", "grain_mod", "A")
    ops.connect("lum_factor", "", "grain_mod", "B")

    # ── dither-hash UV ─────────────────────────────────────────────
    ops.add("dither_uv_scaled", "Multiply", -1100, 960)
    ops.connect("screen_uv", "ViewportUV", "dither_uv_scaled", "A")
    ops.connect("c_dither_scale", "", "dither_uv_scaled", "B")

    ops.add("dither_time_v2", "Multiply", -900, 920)
    ops.connect("time", "", "dither_time_v2", "A")
    ops.connect("c_dither_drift_dir", "", "dither_time_v2", "B")

    ops.add("dither_hash_input", "Add", -700, 880)
    ops.connect("dither_uv_scaled", "", "dither_hash_input", "A")
    ops.connect("dither_time_v2", "", "dither_hash_input", "B")

    ops.connect("dither_hash_input", "", "dither_hash", "P")

    # ── dither signed (sub-LSB) ────────────────────────────────────
    ops.add("dither_minus_half", "Subtract", 900, 860)
    ops.connect("dither_hash", "", "dither_minus_half", "A")
    ops.connect("c_half", "", "dither_minus_half", "B")

    ops.add("dither_x_strength", "Multiply", 1080, 860)
    ops.connect("dither_minus_half", "", "dither_x_strength", "A")
    ops.connect("sp_dither", "", "dither_x_strength", "B")

    ops.add("dither_signed", "Multiply", 1260, 860)
    ops.connect("dither_x_strength", "", "dither_signed", "A")
    ops.connect("c_inv_lsb", "", "dither_signed", "B")

    # ── combine and blend ──────────────────────────────────────────
    ops.add("total_noise", "Add", 1620, 600)
    ops.connect("grain_mod", "", "total_noise", "A")
    ops.connect("dither_signed", "", "total_noise", "B")

    ops.add("effect_rgb", "Add", 1800, 200)
    ops.connect("scene_rgb", "", "effect_rgb", "A")
    ops.connect("total_noise", "", "effect_rgb", "B")

    ops.add("final", "LinearInterpolate", 2000, 100)
    ops.connect("scene_rgb", "", "final", "A")
    ops.connect("effect_rgb", "", "final", "B")
    ops.connect("sp_strength", "", "final", "Alpha")

    ops.connect_out("final", "", "EmissiveColor")

    ops.comment(-1660, -80, 3900, 1480,
                "PP_Film_Grain_AA — animated hash-based grain + sub-LSB dither",
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

"""M4-4: PP_Halftone — screen-space dot-pattern post-process.

Classic halftone:

    pattern = 0.5 * (1 + sin(UV.x * Frequency) * sin(UV.y * Frequency))
    lum     = dot(SceneColor, Rec.709 coefficients)
    mask    = step(pattern, lum)                  # 1 = bright pixel, 0 = dark
    output  = lerp(InkColor, PaperColor, mask)
    output  = lerp(SceneColor, output, Strength)

Parameters:

  Frequency    — dot density (pixels per full sin period, default 200)
  Strength     — 0..1 blend against the untouched scene (default 1)
  InkColor     — dark-ink tint (RGB, default black)
  PaperColor   — paper tint (RGB, default white)
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/PP_Halftone",
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

    # Scene color + RGB mask.
    ops.add("scene", "SceneTexture", -1400, 0)
    ops.setp("scene", "SceneTextureId", "PPI_PostProcessInput0")

    ops.add("scene_rgb", "ComponentMask", -1150, 0)
    ops.setp("scene_rgb", "R", "true")
    ops.setp("scene_rgb", "G", "true")
    ops.setp("scene_rgb", "B", "true")
    ops.setp("scene_rgb", "A", "false")
    ops.connect("scene", "Color", "scene_rgb", "")

    # Screen-space UV (0..1 across the viewport).
    ops.add("screen_uv", "ScreenPosition", -1400, 280)

    # Parameters.
    C.add_scalar_param(ops, "sp_freq", -1400, 420, "Frequency", 200.0,
                       group="01 Halftone", sort_priority=1,
                       slider_min=20.0, slider_max=800.0)
    C.add_scalar_param(ops, "sp_strength", -1400, 500, "Strength", 1.0,
                       group="01 Halftone", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_vector_param(ops, "vp_ink", -1400, 580, "InkColor",
                       "(R=0,G=0,B=0,A=1)",
                       group="01 Halftone", sort_priority=3)
    C.add_vector_param(ops, "vp_paper", -1400, 660, "PaperColor",
                       "(R=1,G=1,B=1,A=1)",
                       group="01 Halftone", sort_priority=4)

    # ViewportUV * Frequency  → float2
    ops.add("uv_times_freq", "Multiply", -1100, 340)
    ops.connect("screen_uv", "ViewportUV", "uv_times_freq", "A")
    ops.connect("sp_freq", "", "uv_times_freq", "B")

    # Split into x + y components.
    ops.add("uv_x", "ComponentMask", -900, 310)
    ops.setp("uv_x", "R", "true")
    ops.setp("uv_x", "G", "false")
    ops.setp("uv_x", "B", "false")
    ops.setp("uv_x", "A", "false")
    ops.connect("uv_times_freq", "", "uv_x", "")

    ops.add("uv_y", "ComponentMask", -900, 390)
    ops.setp("uv_y", "R", "false")
    ops.setp("uv_y", "G", "true")
    ops.setp("uv_y", "B", "false")
    ops.setp("uv_y", "A", "false")
    ops.connect("uv_times_freq", "", "uv_y", "")

    # sin(x) * sin(y) gives a +/-1 grid pattern.
    ops.add("sin_x", "Sine", -700, 310)
    ops.connect("uv_x", "", "sin_x", "")
    ops.add("sin_y", "Sine", -700, 390)
    ops.connect("uv_y", "", "sin_y", "")

    ops.add("sin_mul", "Multiply", -500, 350)
    ops.connect("sin_x", "", "sin_mul", "A")
    ops.connect("sin_y", "", "sin_mul", "B")

    # Remap [-1,1] → [0,1]: pattern = 0.5 * (1 + sin_mul)
    ops.add("const_half", "Constant", -700, 470)
    ops.setp("const_half", "R", "0.5")

    ops.add("const_one", "Constant", -700, 540)
    ops.setp("const_one", "R", "1.0")

    ops.add("pattern_plus1", "Add", -350, 420)
    ops.connect("sin_mul", "", "pattern_plus1", "A")
    ops.connect("const_one", "", "pattern_plus1", "B")

    ops.add("pattern", "Multiply", -150, 420)
    ops.connect("pattern_plus1", "", "pattern", "A")
    ops.connect("const_half", "", "pattern", "B")

    # Luminance.
    ops.add("lum_coefs", "Constant3Vector", -900, 60)
    ops.setp("lum_coefs", "Constant", "(R=0.299,G=0.587,B=0.114,A=0)")

    ops.add("dot_lum", "DotProduct", -650, 30)
    ops.connect("scene_rgb", "", "dot_lum", "A")
    ops.connect("lum_coefs", "", "dot_lum", "B")

    # mask = step(pattern, lum)  implemented as saturate((lum - pattern) * k)
    # with a large k so the transition is nearly binary. Avoids UE's If
    # expression whose branch-inputs are awkward to wire from a batched build.
    ops.add("lum_minus_pat", "Subtract", -150, 220)
    ops.connect("dot_lum", "", "lum_minus_pat", "A")
    ops.connect("pattern", "", "lum_minus_pat", "B")

    ops.add("c_step_gain", "Constant", -150, 290)
    ops.setp("c_step_gain", "R", "1000.0")

    ops.add("lum_minus_pat_scaled", "Multiply", 0, 240)
    ops.connect("lum_minus_pat", "", "lum_minus_pat_scaled", "A")
    ops.connect("c_step_gain", "", "lum_minus_pat_scaled", "B")

    ops.add("mask", "Saturate", 180, 240)
    ops.connect("lum_minus_pat_scaled", "", "mask", "")

    # halftone = lerp(InkColor, PaperColor, mask)
    ops.add("halftone", "LinearInterpolate", 350, 220)
    ops.connect("vp_ink", "", "halftone", "A")
    ops.connect("vp_paper", "", "halftone", "B")
    ops.connect("mask", "", "halftone", "Alpha")

    # final = lerp(SceneColor, halftone, Strength)
    ops.add("final", "LinearInterpolate", 580, 110)
    ops.connect("scene_rgb", "", "final", "A")
    ops.connect("halftone", "", "final", "B")
    ops.connect("sp_strength", "", "final", "Alpha")

    ops.connect_out("final", "", "EmissiveColor")

    ops.comment(-1460, -80, 2100, 800,
                "PP_Halftone — sin×sin dot pattern thresholded by luminance",
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

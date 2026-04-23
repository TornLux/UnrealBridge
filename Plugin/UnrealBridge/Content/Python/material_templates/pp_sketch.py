"""M4-2: PP_Sketch — pencil-sketch / cel-shade post-process.

Composites three classic sketch layers over posterized scene colour:

  1. Depth-edge outline (4-neighbour depth gradient via BridgeSobelEdge)
  2. Luminance-driven cross-hatching (4 direction layers via BridgeCrossHatch)
  3. Luminance posterize (N-band chroma-preserving gain)

Final pixel:

    ink_strength = saturate(edge_mask + (1 - hatch_mask) * HatchStrength)
    posterized   = scene * (floor(lum * Bands) / Bands) / max(lum, eps) * PaperTint
    sketched     = lerp(posterized, InkColor, ink_strength)
    output       = lerp(scene, sketched, Strength)

Parameters:

  Strength            — 0..1 overall blend (default 1)
  InkColor            — RGB ink for edges + hatch (default near-black)
  PaperTint           — RGB multiplier on posterized scene (default warm paper)
  EdgeThickness       — pixel offset for depth gradient (default 1.5)
  EdgeThreshold       — min depth jump to count as an edge (default 0.002)
  EdgeSensitivity     — gain on the thresholded gradient (default 500)
  HatchFrequency      — strokes per viewport unit (default 120)
  HatchStrength       — 0..1 hatch contribution to ink (default 0.5)
  PosterizeBands      — luminance quantization steps (default 4)
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/PP_Sketch",
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

    # ── Custom HLSL nodes (out-of-band add; guid registered into ops) ──
    sobel = L.add_custom_expression(
        master_path, -80, 520,
        ["dL", "dR", "dU", "dD", "Threshold", "Gain"],
        "Float1",
        "return BridgeSobelEdge(dL, dR, dU, dD, Threshold, Gain);",
        ["/Plugin/UnrealBridge/BridgeSnippets.ush"],
        "Depth Sobel edge")
    if not sobel.success:
        raise RuntimeError(f"sobel custom node failed: {sobel.error}")

    hatch = L.add_custom_expression(
        master_path, 300, 760,
        ["UV", "Luminance", "Frequency"],
        "Float1",
        "return BridgeCrossHatch(UV, Luminance, Frequency);",
        ["/Plugin/UnrealBridge/BridgeSnippets.ush"],
        "Cross-hatch shading")
    if not hatch.success:
        raise RuntimeError(f"hatch custom node failed: {hatch.error}")

    ops = C.OpList()
    ops.add_literal(C.guid_to_str(sobel.guid), "sobel")
    ops.add_literal(C.guid_to_str(hatch.guid), "hatch")

    # ── scene color + screen UV ─────────────────────────────────────
    ops.add("scene", "SceneTexture", -1800, 0)
    ops.setp("scene", "SceneTextureId", "PPI_PostProcessInput0")

    ops.add("scene_rgb", "ComponentMask", -1550, 0)
    ops.setp("scene_rgb", "R", "true")
    ops.setp("scene_rgb", "G", "true")
    ops.setp("scene_rgb", "B", "true")
    ops.setp("scene_rgb", "A", "false")
    ops.connect("scene", "Color", "scene_rgb", "")

    ops.add("screen_uv", "ScreenPosition", -1800, 240)

    # ── parameters ──────────────────────────────────────────────────
    C.add_scalar_param(ops, "sp_strength", -1800, 420, "Strength", 1.0,
                       group="01 Sketch", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)
    C.add_vector_param(ops, "vp_ink", -1800, 500, "InkColor",
                       "(R=0.05,G=0.05,B=0.06,A=1)",
                       group="01 Sketch", sort_priority=2)
    C.add_vector_param(ops, "vp_paper", -1800, 580, "PaperTint",
                       "(R=1.04,G=1.00,B=0.94,A=1)",
                       group="01 Sketch", sort_priority=3)

    C.add_scalar_param(ops, "sp_thickness", -1800, 700, "EdgeThickness", 1.5,
                       group="02 Edge", sort_priority=1,
                       slider_min=0.5, slider_max=6.0)
    C.add_scalar_param(ops, "sp_threshold", -1800, 780, "EdgeThreshold", 0.002,
                       group="02 Edge", sort_priority=2,
                       slider_min=0.0, slider_max=0.1)
    C.add_scalar_param(ops, "sp_sensitivity", -1800, 860, "EdgeSensitivity", 500.0,
                       group="02 Edge", sort_priority=3,
                       slider_min=1.0, slider_max=5000.0)

    C.add_scalar_param(ops, "sp_hatch_freq", -1800, 980, "HatchFrequency", 120.0,
                       group="03 Hatch", sort_priority=1,
                       slider_min=30.0, slider_max=400.0)
    C.add_scalar_param(ops, "sp_hatch_strength", -1800, 1060, "HatchStrength", 0.5,
                       group="03 Hatch", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_bands", -1800, 1160, "PosterizeBands", 4.0,
                       group="04 Posterize", sort_priority=1,
                       slider_min=2.0, slider_max=12.0)

    ops.add("c_zero", "Constant", -1800, 1260)
    ops.setp("c_zero", "R", "0.0")
    ops.add("c_one", "Constant", -1800, 1320)
    ops.setp("c_one", "R", "1.0")
    ops.add("c_eps", "Constant", -1800, 1380)
    ops.setp("c_eps", "R", "0.0001")

    # ── pixel offset vectors ────────────────────────────────────────
    ops.add("pixel_offset_v2", "Multiply", -1550, 700)
    ops.connect("scene", "InvSize", "pixel_offset_v2", "A")
    ops.connect("sp_thickness", "", "pixel_offset_v2", "B")

    ops.add("off_x_scalar", "ComponentMask", -1350, 680)
    ops.setp("off_x_scalar", "R", "true")
    ops.setp("off_x_scalar", "G", "false")
    ops.setp("off_x_scalar", "B", "false")
    ops.setp("off_x_scalar", "A", "false")
    ops.connect("pixel_offset_v2", "", "off_x_scalar", "")

    ops.add("off_y_scalar", "ComponentMask", -1350, 740)
    ops.setp("off_y_scalar", "R", "false")
    ops.setp("off_y_scalar", "G", "true")
    ops.setp("off_y_scalar", "B", "false")
    ops.setp("off_y_scalar", "A", "false")
    ops.connect("pixel_offset_v2", "", "off_y_scalar", "")

    ops.add("off_x_v2", "AppendVector", -1150, 680)
    ops.connect("off_x_scalar", "", "off_x_v2", "A")
    ops.connect("c_zero", "", "off_x_v2", "B")

    ops.add("off_y_v2", "AppendVector", -1150, 740)
    ops.connect("c_zero", "", "off_y_v2", "A")
    ops.connect("off_y_scalar", "", "off_y_v2", "B")

    # ── 4 neighbour UVs ─────────────────────────────────────────────
    ops.add("uv_l", "Subtract", -950, 620)
    ops.connect("screen_uv", "ViewportUV", "uv_l", "A")
    ops.connect("off_x_v2", "", "uv_l", "B")

    ops.add("uv_r", "Add", -950, 680)
    ops.connect("screen_uv", "ViewportUV", "uv_r", "A")
    ops.connect("off_x_v2", "", "uv_r", "B")

    ops.add("uv_u", "Subtract", -950, 740)
    ops.connect("screen_uv", "ViewportUV", "uv_u", "A")
    ops.connect("off_y_v2", "", "uv_u", "B")

    ops.add("uv_d", "Add", -950, 800)
    ops.connect("screen_uv", "ViewportUV", "uv_d", "A")
    ops.connect("off_y_v2", "", "uv_d", "B")

    # ── 4 SceneDepth samples ────────────────────────────────────────
    for name, uv_src, y in (("d_l", "uv_l", 620), ("d_r", "uv_r", 680),
                             ("d_u", "uv_u", 740), ("d_d", "uv_d", 800)):
        ops.add(name, "SceneTexture", -750, y)
        ops.setp(name, "SceneTextureId", "PPI_SceneDepth")
        ops.connect(uv_src, "", name, "UVs")
        mask_name = name + "_r"
        ops.add(mask_name, "ComponentMask", -550, y)
        ops.setp(mask_name, "R", "true")
        ops.setp(mask_name, "G", "false")
        ops.setp(mask_name, "B", "false")
        ops.setp(mask_name, "A", "false")
        ops.connect(name, "Color", mask_name, "")

    # ── Sobel edge via Custom ───────────────────────────────────────
    ops.connect("d_l_r", "", "sobel", "dL")
    ops.connect("d_r_r", "", "sobel", "dR")
    ops.connect("d_u_r", "", "sobel", "dU")
    ops.connect("d_d_r", "", "sobel", "dD")
    ops.connect("sp_threshold", "", "sobel", "Threshold")
    ops.connect("sp_sensitivity", "", "sobel", "Gain")
    # sobel output = edge mask 0..1

    # ── Luminance from scene ────────────────────────────────────────
    ops.add("lum_coefs", "Constant3Vector", -1350, 60)
    ops.setp("lum_coefs", "Constant", "(R=0.299,G=0.587,B=0.114,A=0)")

    ops.add("dot_lum", "DotProduct", -1100, 30)
    ops.connect("scene_rgb", "", "dot_lum", "A")
    ops.connect("lum_coefs", "", "dot_lum", "B")

    # ── Hatch via Custom ───────────────────────────────────────────
    ops.connect("screen_uv", "ViewportUV", "hatch", "UV")
    ops.connect("dot_lum", "", "hatch", "Luminance")
    ops.connect("sp_hatch_freq", "", "hatch", "Frequency")

    # hatch_ink = (1 - hatch_mask) * HatchStrength
    ops.add("hatch_inverted", "Subtract", 500, 760)
    ops.connect("c_one", "", "hatch_inverted", "A")
    ops.connect("hatch", "", "hatch_inverted", "B")

    ops.add("hatch_ink", "Multiply", 680, 760)
    ops.connect("hatch_inverted", "", "hatch_ink", "A")
    ops.connect("sp_hatch_strength", "", "hatch_ink", "B")

    # ── combined_ink = saturate(edge + hatch_ink) ──────────────────
    ops.add("ink_sum", "Add", 200, 600)
    ops.connect("sobel", "", "ink_sum", "A")
    ops.connect("hatch_ink", "", "ink_sum", "B")

    ops.add("combined_ink", "Saturate", 400, 600)
    ops.connect("ink_sum", "", "combined_ink", "")

    # ── Posterize luminance: floor(lum * bands) / bands ────────────
    ops.add("lum_times_bands", "Multiply", -900, 120)
    ops.connect("dot_lum", "", "lum_times_bands", "A")
    ops.connect("sp_bands", "", "lum_times_bands", "B")

    ops.add("floor_lb", "Floor", -700, 120)
    ops.connect("lum_times_bands", "", "floor_lb", "")

    ops.add("lum_quant", "Divide", -500, 120)
    ops.connect("floor_lb", "", "lum_quant", "A")
    ops.connect("sp_bands", "", "lum_quant", "B")

    # gain = lum_quant / max(lum, eps)
    ops.add("max_lum_eps", "Max", -900, 240)
    ops.connect("dot_lum", "", "max_lum_eps", "A")
    ops.connect("c_eps", "", "max_lum_eps", "B")

    ops.add("gain", "Divide", -500, 240)
    ops.connect("lum_quant", "", "gain", "A")
    ops.connect("max_lum_eps", "", "gain", "B")

    # posterized = scene_rgb × gain × paper_tint
    ops.add("post_scene", "Multiply", -300, 120)
    ops.connect("scene_rgb", "", "post_scene", "A")
    ops.connect("gain", "", "post_scene", "B")

    ops.add("post_tinted", "Multiply", -100, 120)
    ops.connect("post_scene", "", "post_tinted", "A")
    ops.connect("vp_paper", "", "post_tinted", "B")

    # sketched = lerp(post_tinted, ink_color, combined_ink)
    ops.add("sketched", "LinearInterpolate", 600, 200)
    ops.connect("post_tinted", "", "sketched", "A")
    ops.connect("vp_ink", "", "sketched", "B")
    ops.connect("combined_ink", "", "sketched", "Alpha")

    # final = lerp(scene_rgb, sketched, Strength)
    ops.add("final", "LinearInterpolate", 900, 100)
    ops.connect("scene_rgb", "", "final", "A")
    ops.connect("sketched", "", "final", "B")
    ops.connect("sp_strength", "", "final", "Alpha")
    ops.connect_out("final", "", "EmissiveColor")

    ops.comment(-1860, -100, 3000, 1450,
                "PP_Sketch — depth-Sobel edge + cross-hatch + posterize",
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

"""M4-3: PP_Outline — depth-gradient outline (cartoon / cel-shade edges).

Samples SceneDepth at 4 pixel-sized offsets (left / right / up / down) and
approximates ``|grad(depth)| = |dR-dL| + |dD-dU|``. Thresholded + scaled
into a 0..1 edge mask; the mask lerps the scene toward an ink colour.

Parameters:

  EdgeThickness    — offset distance in pixels (1..10, default 1.5)
  EdgeThreshold    — min depth-gradient magnitude to count as an edge (default 0.001)
  EdgeSensitivity  — gain applied after the threshold subtract (default 500)
  EdgeColor        — ink tint (RGB, default pure black)
  Strength         — 0..1 blend against the untouched scene (default 1)
"""

from __future__ import annotations

from typing import Any, Dict

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/PP_Outline",
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

    # --- scene inputs ---------------------------------------------------
    ops.add("scene", "SceneTexture", -1700, 0)
    ops.setp("scene", "SceneTextureId", "PPI_PostProcessInput0")

    ops.add("scene_rgb", "ComponentMask", -1450, 0)
    ops.setp("scene_rgb", "R", "true")
    ops.setp("scene_rgb", "G", "true")
    ops.setp("scene_rgb", "B", "true")
    ops.setp("scene_rgb", "A", "false")
    ops.connect("scene", "Color", "scene_rgb", "")

    ops.add("screen_uv", "ScreenPosition", -1700, 200)

    # --- parameters -----------------------------------------------------
    C.add_scalar_param(ops, "sp_thickness", -1700, 380, "EdgeThickness", 1.5,
                       group="01 Outline", sort_priority=1,
                       slider_min=0.5, slider_max=10.0)
    C.add_scalar_param(ops, "sp_threshold", -1700, 460, "EdgeThreshold", 0.001,
                       group="01 Outline", sort_priority=2,
                       slider_min=0.0, slider_max=0.1)
    C.add_scalar_param(ops, "sp_sensitivity", -1700, 540, "EdgeSensitivity", 500.0,
                       group="01 Outline", sort_priority=3,
                       slider_min=1.0, slider_max=10000.0)
    C.add_vector_param(ops, "vp_edge_color", -1700, 620, "EdgeColor",
                       "(R=0,G=0,B=0,A=1)",
                       group="01 Outline", sort_priority=4)
    C.add_scalar_param(ops, "sp_strength", -1700, 700, "Strength", 1.0,
                       group="01 Outline", sort_priority=5,
                       slider_min=0.0, slider_max=1.0)

    ops.add("c_zero", "Constant", -1700, 800)
    ops.setp("c_zero", "R", "0.0")

    # --- compute UV offsets: thickness_pixels * inv_size ---------------
    ops.add("offset_pixels", "Multiply", -1450, 300)
    ops.connect("scene", "InvSize", "offset_pixels", "A")
    ops.connect("sp_thickness", "", "offset_pixels", "B")

    ops.add("offx_scalar", "ComponentMask", -1250, 280)
    ops.setp("offx_scalar", "R", "true")
    ops.setp("offx_scalar", "G", "false")
    ops.setp("offx_scalar", "B", "false")
    ops.setp("offx_scalar", "A", "false")
    ops.connect("offset_pixels", "", "offx_scalar", "")

    ops.add("offy_scalar", "ComponentMask", -1250, 340)
    ops.setp("offy_scalar", "R", "false")
    ops.setp("offy_scalar", "G", "true")
    ops.setp("offy_scalar", "B", "false")
    ops.setp("offy_scalar", "A", "false")
    ops.connect("offset_pixels", "", "offy_scalar", "")

    # offset_x_v2 = (offx_scalar, 0)
    ops.add("offset_x_v2", "AppendVector", -1050, 280)
    ops.connect("offx_scalar", "", "offset_x_v2", "A")
    ops.connect("c_zero", "", "offset_x_v2", "B")

    # offset_y_v2 = (0, offy_scalar)
    ops.add("offset_y_v2", "AppendVector", -1050, 340)
    ops.connect("c_zero", "", "offset_y_v2", "A")
    ops.connect("offy_scalar", "", "offset_y_v2", "B")

    # 4 neighbour UVs.
    ops.add("uv_l", "Subtract", -850, 220)
    ops.connect("screen_uv", "ViewportUV", "uv_l", "A")
    ops.connect("offset_x_v2", "", "uv_l", "B")

    ops.add("uv_r", "Add", -850, 290)
    ops.connect("screen_uv", "ViewportUV", "uv_r", "A")
    ops.connect("offset_x_v2", "", "uv_r", "B")

    ops.add("uv_u", "Subtract", -850, 360)
    ops.connect("screen_uv", "ViewportUV", "uv_u", "A")
    ops.connect("offset_y_v2", "", "uv_u", "B")

    ops.add("uv_d", "Add", -850, 430)
    ops.connect("screen_uv", "ViewportUV", "uv_d", "A")
    ops.connect("offset_y_v2", "", "uv_d", "B")

    # 4 depth lookups.
    for name, uv_ref, y in (
        ("d_l", "uv_l", 220),
        ("d_r", "uv_r", 290),
        ("d_u", "uv_u", 360),
        ("d_d", "uv_d", 430),
    ):
        ops.add(name, "SceneTexture", -650, y)
        ops.setp(name, "SceneTextureId", "PPI_SceneDepth")
        ops.connect(uv_ref, "", name, "Coordinates")

    # Depth value is in .Color.R — mask to get a float.
    for name in ("d_l", "d_r", "d_u", "d_d"):
        mask_name = f"{name}_r"
        ops.add(mask_name, "ComponentMask", -450, 220 + ("lrud".index(name[-1]) * 70))
        ops.setp(mask_name, "R", "true")
        ops.setp(mask_name, "G", "false")
        ops.setp(mask_name, "B", "false")
        ops.setp(mask_name, "A", "false")
        ops.connect(name, "Color", mask_name, "")

    # Gradient magnitudes.
    ops.add("grad_x", "Subtract", -250, 240)
    ops.connect("d_r_r", "", "grad_x", "A")
    ops.connect("d_l_r", "", "grad_x", "B")

    ops.add("grad_y", "Subtract", -250, 380)
    ops.connect("d_d_r", "", "grad_y", "A")
    ops.connect("d_u_r", "", "grad_y", "B")

    ops.add("abs_gx", "Abs", -80, 240)
    ops.connect("grad_x", "", "abs_gx", "")

    ops.add("abs_gy", "Abs", -80, 380)
    ops.connect("grad_y", "", "abs_gy", "")

    ops.add("edge_raw", "Add", 100, 310)
    ops.connect("abs_gx", "", "edge_raw", "A")
    ops.connect("abs_gy", "", "edge_raw", "B")

    # Thresholded mask: saturate((edge_raw - threshold) * sensitivity)
    ops.add("edge_minus_t", "Subtract", 280, 310)
    ops.connect("edge_raw", "", "edge_minus_t", "A")
    ops.connect("sp_threshold", "", "edge_minus_t", "B")

    ops.add("edge_scaled", "Multiply", 450, 310)
    ops.connect("edge_minus_t", "", "edge_scaled", "A")
    ops.connect("sp_sensitivity", "", "edge_scaled", "B")

    ops.add("edge_mask", "Saturate", 620, 310)
    ops.connect("edge_scaled", "", "edge_mask", "")

    # Mask × Strength.
    ops.add("mask_strength", "Multiply", 800, 340)
    ops.connect("edge_mask", "", "mask_strength", "A")
    ops.connect("sp_strength", "", "mask_strength", "B")

    # Lerp(scene_rgb, edge_color, mask*strength)
    ops.add("final", "LinearInterpolate", 1000, 160)
    ops.connect("scene_rgb", "", "final", "A")
    ops.connect("vp_edge_color", "", "final", "B")
    ops.connect("mask_strength", "", "final", "Alpha")

    ops.connect_out("final", "", "EmissiveColor")

    ops.comment(-1760, -80, 2900, 900,
                "PP_Outline — 4-neighbour depth-gradient edge",
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

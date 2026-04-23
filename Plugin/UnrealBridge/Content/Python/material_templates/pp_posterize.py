"""M4-ish: PP_Posterize — quantized-luminance post-process material.

Minimal proof of the M4 pipeline (create_post_process_material → wire →
apply_post_process_material). No HLSL snippets — pure graph nodes.

The effect:

    C' = SceneColor * (floor(L * Bands) / Bands) / max(L, eps)

where L = dot(SceneColor, (0.299, 0.587, 0.114)) is Rec. 709 luminance.
Scaling by (quantized_L / L) preserves chroma so a red wall stays red but
the luminance collapses onto ``Bands`` discrete levels.

Parameters exposed for MI tweaking:

  PosterizeBands  — number of tonal steps (default 4)
  Strength        — 0..1 blend between original and posterized (default 1)
  EdgeTint        — RGB tint multiplied on the posterized output (default 1,1,1)
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


def build(master_path: str = "/Game/BridgeTemplates/PP_Posterize",
          apply_to_volume: str = "",
          apply_weight: float = 1.0,
          blendable_location: str = "AfterTonemapping",
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Build the posterize PP material and optionally install it on a volume.

    Pass ``apply_to_volume`` = ``""`` to target / create an unbound PPV.
    Pass ``apply_weight = 0`` to skip the apply step.
    """
    L = unreal.UnrealBridgeMaterialLibrary

    # 1. Create (or wipe + reuse).
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

    # 2. Build the graph.
    ops = C.OpList()

    # SceneTexture: PostProcessInput0 (the incoming scene color).
    # ImportText expects the full enum value — ESceneTextureId uses the
    # PPI_ prefix ("PPI_PostProcessInput0", not bare "PostProcessInput0").
    ops.add("scene", "SceneTexture", -1400, 0)
    ops.setp("scene", "SceneTextureId", "PPI_PostProcessInput0")

    # SceneTexture's "Color" output is float4 — mask to RGB so the rest of the
    # chain can use float3 constants / parameters without type mismatches.
    ops.add("scene_rgb", "ComponentMask", -1150, 0)
    ops.setp("scene_rgb", "R", "true")
    ops.setp("scene_rgb", "G", "true")
    ops.setp("scene_rgb", "B", "true")
    ops.setp("scene_rgb", "A", "false")
    ops.connect("scene", "Color", "scene_rgb", "")

    # Luminance coefficients.
    ops.add("lum_coefs", "Constant3Vector", -1000, 180)
    ops.setp("lum_coefs", "Constant", "(R=0.299,G=0.587,B=0.114,A=0)")

    # Parameters.
    C.add_scalar_param(ops, "sp_bands", -1000, 340, "PosterizeBands", 4.0,
                       group="01 Posterize", sort_priority=1,
                       slider_min=2.0, slider_max=16.0)
    C.add_scalar_param(ops, "sp_strength", -1000, 420, "Strength", 1.0,
                       group="01 Posterize", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_vector_param(ops, "vp_tint", -1000, 500, "EdgeTint",
                       "(R=1,G=1,B=1,A=1)",
                       group="01 Posterize", sort_priority=3)
    ops.add("eps", "Constant", -1000, 580)
    ops.setp("eps", "R", "1e-4")

    # Luminance = dot(SceneColor, coefs).
    ops.add("dot_lum", "DotProduct", -700, 90)
    ops.connect("scene_rgb", "", "dot_lum", "A")
    ops.connect("lum_coefs", "", "dot_lum", "B")

    # Quantize: floor(L * Bands) / Bands.
    ops.add("lum_times_bands", "Multiply", -500, 90)
    ops.connect("dot_lum", "", "lum_times_bands", "A")
    ops.connect("sp_bands", "", "lum_times_bands", "B")

    ops.add("floor_lb", "Floor", -350, 90)
    ops.connect("lum_times_bands", "", "floor_lb", "")

    ops.add("lum_quant", "Divide", -200, 90)
    ops.connect("floor_lb", "", "lum_quant", "A")
    ops.connect("sp_bands", "", "lum_quant", "B")

    # Gain = lum_quant / max(L, eps) — preserves chroma.
    ops.add("max_lum_eps", "Max", -500, 250)
    ops.connect("dot_lum", "", "max_lum_eps", "A")
    ops.connect("eps", "", "max_lum_eps", "B")

    ops.add("gain", "Divide", -200, 250)
    ops.connect("lum_quant", "", "gain", "A")
    ops.connect("max_lum_eps", "", "gain", "B")

    # Posterized = SceneColor * gain * tint.
    ops.add("post_rgb", "Multiply", 0, 90)
    ops.connect("scene_rgb", "", "post_rgb", "A")
    ops.connect("gain", "", "post_rgb", "B")

    ops.add("post_rgb_tinted", "Multiply", 180, 90)
    ops.connect("post_rgb", "", "post_rgb_tinted", "A")
    ops.connect("vp_tint", "", "post_rgb_tinted", "B")

    # Mix: Lerp(SceneColor, Posterized, Strength).
    ops.add("final", "LinearInterpolate", 380, 90)
    ops.connect("scene_rgb", "", "final", "A")
    ops.connect("post_rgb_tinted", "", "final", "B")
    ops.connect("sp_strength", "", "final", "Alpha")

    ops.connect_out("final", "", "EmissiveColor")

    ops.comment(-1060, -80, 1550, 710,
                "PP_Posterize — quantize luminance then re-chroma by gain",
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

    # 3. Optionally apply to a PPV.
    if apply_weight > 0.0:
        ok = L.apply_post_process_material(apply_to_volume, master_path, apply_weight)
        out["applied"] = bool(ok)
        out["volume"] = apply_to_volume or "<first-unbound-or-spawned>"

    return out

"""M3-10: M_Fabric_PBR — AAA-aligned fabric / clothing master material.

Mirrors the input surface of the TLoU2 Ellie shipped masters
(`M_Fabric_Master` + `M_Matel_Master` rolled into one): takes **per-pixel**
Diffuse / Normal / AO / Roughness / Metallic and does a minimal PBR routing.

The key difference from ``M_Character_Armor`` (M3-2) is: that master assumes
an ORM-packed single-texture workflow + global scalar roughness/metallic.
This master assumes **separate per-channel textures** and thus supports the
common Naughty-Dog / Sony-first-party asset format directly.

Parameters (all MI-overridable):

  Textures:  BaseColorTex / NormalTex / AOTex / RoughnessTex / MetallicTex
  Vectors:   BaseColorTint
  Scalars:   RoughnessMin / RoughnessMax (linear remap of RoughnessTex.R)
             MetallicScale (multiplied with MetallicTex.R)
             AOStrength (0 = ignore AO, 1 = full AO)

Default textures (when an MI doesn't override):

  BaseColorTex  → /Engine/.../WhiteSquareTexture  (sRGB color white)
  NormalTex     → /Engine/.../DefaultNormal       (flat Z-up)
  AOTex         → T_White_Linear                  (linear white = no occlusion)
  RoughnessTex  → T_White_Linear                  (R=1 → lerps to RoughnessMax)
  MetallicTex   → T_White_Linear                  (R=1 → multiplied by MetallicScale)

All texture params share ``SSM_Wrap_WorldGroupSettings`` → zero dedicated
sampler-slot cost (Masks vs Color sampler-type mismatch avoided by using
SamplerType=LinearColor for non-color textures and routing through the
project-local ``T_White_Linear`` default).

Budget targets (SM5 / High): <= 180 instructions, <= 5 sampler slots.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


# --- node positions (editor canvas x/y) -----------------------------------------

X_TEX = -1600          # texture parameter column
X_SPLIT = -1200        # channel mask / multiply column
X_MATH = -800          # lerp / scale column
X_OUTPUT = -200        # connect-to-main-output column

# Y bands per channel
Y_BASECOLOR = -700
Y_NORMAL = -300
Y_AO = 100
Y_ROUGH = 500
Y_METAL = 900


def build(master_path: str = "/Game/BridgeTemplates/M_Fabric_PBR",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Fabric_PBR_Test",
          preview_png: Optional[str] = None,
          preview_mesh: str = C.DEFAULT_PREVIEW_MESH,
          preview_lighting: str = C.DEFAULT_PREVIEW_LIGHTING,
          preview_resolution: int = 512,
          preview_camera_yaw: float = 35.0,
          preview_camera_pitch: float = -15.0,
          preview_camera_distance: float = 0.0,
          instr_budget: int = 180,
          sampler_budget: int = 5,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Create M_Fabric_PBR master + optional test MI; return stats dict."""

    L = unreal.UnrealBridgeMaterialLibrary

    # Project-local defaults for LinearColor-sampled slots. Must already exist
    # on disk — bundling the Texture2DFactoryNew path with a master build + MI
    # save deadlocks the GameThread (asset-reference-completing modal). Run
    # ``C.ensure_default_linear_texture()`` in a separate exec first.
    linear_white = C.DEFAULT_LINEAR_WHITE_TEX
    if not unreal.EditorAssetLibrary.does_asset_exist(linear_white):
        raise RuntimeError(
            f"Required system texture missing: {linear_white}. "
            "Call material_templates._common.ensure_default_linear_texture() "
            "in a separate bridge.exec before building M_Fabric_PBR."
        )

    C.ensure_master_material(master_path, rebuild=rebuild)

    ops = C.OpList()

    # --- BaseColor ---------------------------------------------------------
    C.add_texture_param_2d(ops, "BaseColorTex", X_TEX, Y_BASECOLOR,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="01 Base", sort_priority=1)
    C.add_vector_param(ops, "BaseColorTint", X_TEX - 400, Y_BASECOLOR + 180,
                       "BaseColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Base", sort_priority=2)

    ops.add("BaseColorMul", "Multiply", X_MATH, Y_BASECOLOR + 80)
    ops.connect("BaseColorTex", "RGB", "BaseColorMul", "A")
    ops.connect("BaseColorTint", "", "BaseColorMul", "B")
    ops.connect_out("BaseColorMul", "", "BaseColor")

    # --- Normal ------------------------------------------------------------
    C.add_texture_param_2d(ops, "NormalTex", X_TEX, Y_NORMAL,
                           "NormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="02 Normal", sort_priority=1)
    ops.connect_out("NormalTex", "RGB", "Normal")

    # --- AO ----------------------------------------------------------------
    # SamplerType=LinearColor + T_White_Linear default (sRGB=False).
    C.add_texture_param_2d(ops, "AOTex", X_TEX, Y_AO,
                           "AOTex", linear_white,
                           sampler_type="LinearColor",
                           group="03 AO", sort_priority=1)
    C.add_scalar_param(ops, "AOStrength", X_TEX - 400, Y_AO + 180,
                       "AOStrength", 1.0,
                       group="03 AO", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    # AO = lerp(1, AOTex.R, AOStrength) — lets authors blend between
    # full-effect (1) and disabled (0) without touching the texture.
    ops.add("AOOne", "Constant", X_SPLIT - 200, Y_AO - 40)
    ops.setp("AOOne", "R", "1.0")
    ops.add("AOLerp", "LinearInterpolate", X_MATH, Y_AO + 80)
    ops.connect("AOOne",      "",  "AOLerp", "A")
    ops.connect("AOTex",      "R", "AOLerp", "B")
    ops.connect("AOStrength", "",  "AOLerp", "Alpha")
    ops.connect_out("AOLerp", "", "AmbientOcclusion")

    # --- Roughness ---------------------------------------------------------
    C.add_texture_param_2d(ops, "RoughnessTex", X_TEX, Y_ROUGH,
                           "RoughnessTex", linear_white,
                           sampler_type="LinearColor",
                           group="04 Roughness", sort_priority=1)
    C.add_scalar_param(ops, "RoughnessMin", X_TEX - 400, Y_ROUGH + 140,
                       "RoughnessMin", 0.35,
                       group="04 Roughness", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "RoughnessMax", X_TEX - 400, Y_ROUGH + 220,
                       "RoughnessMax", 0.85,
                       group="04 Roughness", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)
    # Roughness = lerp(RoughnessMin, RoughnessMax, RoughnessTex.R)
    ops.add("RoughLerp", "LinearInterpolate", X_MATH, Y_ROUGH + 80)
    ops.connect("RoughnessMin", "",  "RoughLerp", "A")
    ops.connect("RoughnessMax", "",  "RoughLerp", "B")
    ops.connect("RoughnessTex", "R", "RoughLerp", "Alpha")
    ops.connect_out("RoughLerp", "", "Roughness")

    # --- Metallic ----------------------------------------------------------
    C.add_texture_param_2d(ops, "MetallicTex", X_TEX, Y_METAL,
                           "MetallicTex", linear_white,
                           sampler_type="LinearColor",
                           group="05 Metallic", sort_priority=1)
    C.add_scalar_param(ops, "MetallicScale", X_TEX - 400, Y_METAL + 180,
                       "MetallicScale", 0.0,
                       group="05 Metallic", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    # Metallic = MetallicTex.R * MetallicScale
    ops.add("MetalMul", "Multiply", X_MATH, Y_METAL + 80)
    ops.connect("MetallicTex",   "R", "MetalMul", "A")
    ops.connect("MetallicScale", "",  "MetalMul", "B")
    ops.connect_out("MetalMul", "", "Metallic")

    # --- Section comments --------------------------------------------------
    ops.comment(X_TEX - 450, Y_BASECOLOR - 120, 1700, 300,
                "01 Base Color", C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 20,  Y_NORMAL - 120,    1500, 260,
                "02 Normal",     C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 450, Y_AO - 120,        1700, 300,
                "03 Ambient Occlusion", C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 450, Y_ROUGH - 120,     1700, 360,
                "04 Roughness",  C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 450, Y_METAL - 120,     1700, 300,
                "05 Metallic",   C.COMMENT_COLOR_PBR)

    # --- Flush + compile ---------------------------------------------------
    flush = ops.flush(master_path, compile=compile)
    C.save_master(master_path)

    stats = C.collect_stats(master_path)
    budget = C.check_budget(stats, instr_budget, sampler_budget)

    result: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(flush.ops_applied),
        "num_expressions": stats["num_expressions"],
        "max_instructions": stats["max_instructions"],
        "sampler_count": stats["sampler_count"],
        "compile_errors": stats["compile_errors"],
        "compile_clean": len(stats["compile_errors"]) == 0,
        **budget,
    }

    # --- Optional test MI --------------------------------------------------
    if mi_path is not None:
        if not unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            mr = L.create_material_instance(master_path, mi_path)
            if not mr.success:
                result["mi_error"] = str(mr.error)
        unreal.EditorAssetLibrary.save_asset(mi_path, only_if_is_dirty=False)
        result["mi_path"] = mi_path

        if preview_png:
            out = preview_png
            import os
            if not os.path.isabs(out):
                out = os.path.join(str(unreal.Paths.project_dir()), out).replace("\\", "/")
            ok = L.preview_material(mi_path, preview_mesh, preview_lighting,
                                    preview_resolution,
                                    preview_camera_yaw, preview_camera_pitch,
                                    preview_camera_distance, out)
            result["preview_ok"] = bool(ok)
            result["preview_png"] = out

    return result

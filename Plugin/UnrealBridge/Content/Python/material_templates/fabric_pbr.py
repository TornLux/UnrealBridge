"""M3-10: M_Fabric_PBR — AAA-aligned fabric / clothing master material (v2 — Matrix-influenced).

Mirrors the input surface of the TLoU2 Ellie shipped masters: takes per-pixel
Diffuse / Normal / AO / Roughness / Metallic and does a PBR routing. v2 absorbs
two Matrix-demo patterns relevant to cloth authoring:

  * **4-point roughness remap** (Low / SculptLow / SculptHigh / High) driven by
    ``BridgeRoughnessRemap4Pt`` — replaces the v1 2-point Min/Max lerp. Matches
    how Matrix's `M_Clothing` tunes per-layer roughness envelopes.
  * **Charlie sheen grazing rim** gated by ``DoCharlieSheen`` — adds
    ``BridgeCharlieSheenGrazing(N, V, SheenRoughness) × SheenColor × SheenIntensity``
    onto EmissiveColor. Matches the Matrix clothing rim-light approach: the
    term lives alongside UE's deferred DefaultLit shading rather than fighting
    it. Cloth's soft grazing highlight is the single most identifiable AAA
    fabric indicator and a 2-Custom-node add cost vs the visual payoff is
    a clear win.

The key difference from ``M_Character_Armor`` (M3-2) is still: that master
assumes an ORM-packed single-texture workflow + global scalar roughness /
metallic. This master assumes per-channel separate textures (Naughty-Dog /
Sony-first-party asset format).

Parameters (all MI-overridable):

  Textures:  BaseColorTex / NormalTex / AOTex / RoughnessTex / MetallicTex
  Vectors:   BaseColorTint / Fabric_SheenColor
  Scalars:   Fabric_Rough_Low / _High / _SculptLow / _SculptHigh  (4-pt remap)
             MetallicScale (multiplied with MetallicTex.R)
             AOStrength (0 = ignore AO, 1 = full AO)
             Fabric_SheenIntensity (default 0 = sheen off even if switch is on)
             Fabric_SheenRoughness

Static switches:
  DoCharlieSheen — gate the sheen contribution to EmissiveColor

Default textures (when an MI doesn't override):

  BaseColorTex  → /Engine/.../WhiteSquareTexture  (sRGB color white)
  NormalTex     → /Engine/.../DefaultNormal       (flat Z-up)
  AOTex         → T_White_Linear                  (linear white = no occlusion)
  RoughnessTex  → T_White_Linear                  (R=1 → remap's 'value' arg)
  MetallicTex   → T_White_Linear                  (R=1 → MetallicScale gate)

All texture params share ``SSM_Wrap_WorldGroupSettings`` → zero dedicated
sampler-slot cost.

Budget targets (SM5 / High): ≤ 220 instructions (up from v1 180 — 2 Custom
nodes + sheen chain add ~40 inst), ≤ 5 sampler slots.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


# --- node positions (editor canvas x/y) -----------------------------------------

X_PARAM = -2000
X_TEX = -1600
X_SPLIT = -1200
X_MATH = -800
X_SWITCH = -400
X_OUTPUT = -200

# Y bands per channel / section
Y_BASECOLOR = -700
Y_NORMAL = -300
Y_AO = 100
Y_ROUGH = 500
Y_METAL = 900
Y_SHEEN = 1280

SNIPPETS_PATH = "/Plugin/UnrealBridge/BridgeSnippets.ush"


def build(master_path: str = "/Game/BridgeTemplates/M_Fabric_PBR",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Fabric_PBR_Test",
          preview_png: Optional[str] = None,
          preview_mesh: str = C.DEFAULT_PREVIEW_MESH,
          preview_lighting: str = C.DEFAULT_PREVIEW_LIGHTING,
          preview_resolution: int = 512,
          preview_camera_yaw: float = 35.0,
          preview_camera_pitch: float = -15.0,
          preview_camera_distance: float = 0.0,
          instr_budget: int = 220,
          sampler_budget: int = 5,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Create M_Fabric_PBR master + optional test MI; return stats dict."""

    L = unreal.UnrealBridgeMaterialLibrary

    linear_white = C.DEFAULT_LINEAR_WHITE_TEX
    if not unreal.EditorAssetLibrary.does_asset_exist(linear_white):
        raise RuntimeError(
            f"Required system texture missing: {linear_white}. "
            "Call material_templates._common.ensure_default_linear_texture() "
            "in a separate bridge.exec before building M_Fabric_PBR."
        )

    C.ensure_master_material(master_path, rebuild=rebuild)

    # Custom HLSL nodes — added outside ops because apply_material_graph_ops
    # can't configure Custom's typed inputs.
    custom_remap = L.add_custom_expression(
        master_path,
        X_MATH - 200, Y_ROUGH + 80,
        ["value", "low", "high", "sculptLow", "sculptHigh"],
        "Float1",
        "return BridgeRoughnessRemap4Pt(value, low, high, sculptLow, sculptHigh);",
        [SNIPPETS_PATH],
        "4-point roughness remap")
    if not custom_remap.success:
        raise RuntimeError(f"custom_remap: {custom_remap.error}")

    custom_sheen = L.add_custom_expression(
        master_path,
        X_MATH - 200, Y_SHEEN + 80,
        ["N", "V", "roughness"],
        "Float1",
        "return BridgeCharlieSheenGrazing(N, V, roughness);",
        [SNIPPETS_PATH],
        "Charlie sheen grazing rim (scalar envelope)")
    if not custom_sheen.success:
        raise RuntimeError(f"custom_sheen: {custom_sheen.error}")

    ops = C.OpList()
    ops.add_literal(C.guid_to_str(custom_remap.guid), "custom_rough_remap")
    ops.add_literal(C.guid_to_str(custom_sheen.guid), "custom_sheen_grazing")

    # --- BaseColor ---------------------------------------------------------
    C.add_texture_param_2d(ops, "BaseColorTex", X_TEX, Y_BASECOLOR,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="01 Base", sort_priority=1)
    C.add_vector_param(ops, "BaseColorTint", X_PARAM, Y_BASECOLOR + 180,
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
    # The normal goes to Normal slot AND to sheen's N input.
    # Note: "RGB" on TextureSample is float3 already.
    ops.connect_out("NormalTex", "RGB", "Normal")

    # --- AO ----------------------------------------------------------------
    C.add_texture_param_2d(ops, "AOTex", X_TEX, Y_AO,
                           "AOTex", linear_white,
                           sampler_type="LinearColor",
                           group="03 AO", sort_priority=1)
    C.add_scalar_param(ops, "AOStrength", X_PARAM, Y_AO + 180,
                       "AOStrength", 1.0,
                       group="03 AO", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)

    ops.add("AOOne", "Constant", X_SPLIT - 200, Y_AO - 40)
    ops.setp("AOOne", "R", "1.0")
    ops.add("AOLerp", "LinearInterpolate", X_MATH, Y_AO + 80)
    ops.connect("AOOne",      "",  "AOLerp", "A")
    ops.connect("AOTex",      "R", "AOLerp", "B")
    ops.connect("AOStrength", "",  "AOLerp", "Alpha")
    ops.connect_out("AOLerp", "", "AmbientOcclusion")

    # --- Roughness (4-point remap) -----------------------------------------
    C.add_texture_param_2d(ops, "RoughnessTex", X_TEX, Y_ROUGH,
                           "RoughnessTex", linear_white,
                           sampler_type="LinearColor",
                           group="04 Roughness", sort_priority=1)

    rough_syms = C.add_roughness_remap_4pt(
        ops, "Fabric", X_PARAM, Y_ROUGH + 180,
        group="04 Roughness",
        low=0.5, high=0.85, sculpt_low=0.0, sculpt_high=1.0,
        base_priority=2)

    # Wire RoughnessTex.R → remap.value, remap params → remap.*
    ops.connect("RoughnessTex",             "R", "custom_rough_remap", "value")
    ops.connect(rough_syms["low"],          "",  "custom_rough_remap", "low")
    ops.connect(rough_syms["high"],         "",  "custom_rough_remap", "high")
    ops.connect(rough_syms["sculpt_low"],   "",  "custom_rough_remap", "sculptLow")
    ops.connect(rough_syms["sculpt_high"],  "",  "custom_rough_remap", "sculptHigh")
    ops.connect_out("custom_rough_remap", "", "Roughness")

    # --- Metallic ----------------------------------------------------------
    C.add_texture_param_2d(ops, "MetallicTex", X_TEX, Y_METAL,
                           "MetallicTex", linear_white,
                           sampler_type="LinearColor",
                           group="05 Metallic", sort_priority=1)
    C.add_scalar_param(ops, "MetallicScale", X_PARAM, Y_METAL + 180,
                       "MetallicScale", 0.0,
                       group="05 Metallic", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    ops.add("MetalMul", "Multiply", X_MATH, Y_METAL + 80)
    ops.connect("MetallicTex",   "R", "MetalMul", "A")
    ops.connect("MetallicScale", "",  "MetalMul", "B")
    ops.connect_out("MetalMul", "", "Metallic")

    # --- Charlie Sheen (gated; outputs to EmissiveColor) -------------------
    sheen_syms = C.add_charlie_sheen_scalars(
        ops, "Fabric", X_PARAM, Y_SHEEN + 180,
        group="06 Sheen",
        sheen_intensity_default=0.0,
        sheen_roughness_default=0.5,
        base_priority=1)

    # Wire the grazing rim: N ← NormalTex.RGB, V ← CameraVectorWS, roughness ← sheenRoughness
    ops.add("cam_vec_sheen", "CameraVectorWS", X_SPLIT - 200, Y_SHEEN + 240)
    ops.connect("NormalTex",               "RGB", "custom_sheen_grazing", "N")
    ops.connect("cam_vec_sheen",           "",    "custom_sheen_grazing", "V")
    ops.connect(sheen_syms["roughness"],   "",    "custom_sheen_grazing", "roughness")

    ops.add("SheenColorMul", "Multiply", X_MATH, Y_SHEEN + 120)
    ops.connect("custom_sheen_grazing", "", "SheenColorMul", "A")
    ops.connect(sheen_syms["color"],    "", "SheenColorMul", "B")

    ops.add("SheenIntensityMul", "Multiply", X_MATH + 220, Y_SHEEN + 120)
    ops.connect("SheenColorMul",         "", "SheenIntensityMul", "A")
    ops.connect(sheen_syms["intensity"], "", "SheenIntensityMul", "B")

    # DoCharlieSheen switch — True path emits sheen, False emits zero.
    C.add_static_switch_param(ops, "ss_do_sheen", X_SWITCH, Y_SHEEN,
                              "DoCharlieSheen", default=False,
                              group="06 Sheen", sort_priority=0)

    ops.add("c3v_sheen_zero", "Constant3Vector", X_SWITCH - 60, Y_SHEEN + 200)
    ops.setp("c3v_sheen_zero", "Constant", "(R=0,G=0,B=0,A=0)")

    ops.connect("SheenIntensityMul", "", "ss_do_sheen", "True")
    ops.connect("c3v_sheen_zero",    "", "ss_do_sheen", "False")
    ops.connect_out("ss_do_sheen", "", "EmissiveColor")

    # --- Section comments --------------------------------------------------
    ops.comment(X_PARAM - 50, Y_BASECOLOR - 120, 2000, 300,
                "01 Base Color — tex × tint", C.COMMENT_COLOR_PBR)
    ops.comment(X_PARAM - 50, Y_NORMAL - 120,   2000, 260,
                "02 Normal — direct passthrough (feeds sheen N too)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_PARAM - 50, Y_AO - 120,       2000, 300,
                "03 Ambient Occlusion — lerp(1, AO.R, AOStrength)",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_PARAM - 50, Y_ROUGH - 120,    2000, 420,
                "04 Roughness — 4-point remap (Low / SculptLow / SculptHigh / High) on R.channel",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_PARAM - 50, Y_METAL - 120,    2000, 300,
                "05 Metallic — Metal.R × MetallicScale",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_PARAM - 50, Y_SHEEN - 120,    2000, 440,
                "06 Charlie Sheen (rim-light × SheenColor × Intensity → EmissiveColor, gated by DoCharlieSheen)",
                C.COMMENT_COLOR_SWITCH)

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
        "custom_node_guids": {
            "rough_remap":    C.guid_to_str(custom_remap.guid),
            "sheen_grazing":  C.guid_to_str(custom_sheen.guid),
        },
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

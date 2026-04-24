"""M3-2: M_Character_Armor — AAA character / armor master material (v2 — Matrix-influenced).

This revision absorbs the Matrix-demo clothing master's authoring patterns:

  * **A_ prefix** on layer params, so adding a B-layer later doesn't force a
    rename pass. Graph-symbol names (python ``OpList`` keys) use the prefix
    consistently — MI-facing names also use ``A_`` for the armor pass.
  * **4-point roughness remap** — Low / SculptLow / SculptHigh / High
    (driven by ``BridgeRoughnessRemap4Pt`` HLSL snippet). Matrix's clothing
    master uses the same 4 slots per layer; matches AAA tuning expectations
    where the low-end and high-end segments can be pinned flat via the
    sculpt endpoints.
  * **Bent normals** (optional, gated by ``A_DoBentNormal``) — extra normal
    texture decoded through ``BridgeBentNormalAO`` and multiplied into AO.
    Pass sculpt/cavity AO that the flat ORM.R occlusion can't resolve.
  * **Charlie sheen grazing** (optional, gated by ``A_DoCharlieSheen``) —
    the Matrix approach: compute a rim-light-style grazing envelope via
    ``BridgeCharlieSheenGrazing`` and ADD it (× sheen color × intensity) to
    EmissiveColor. Doesn't fight UE's deferred DefaultLit — it rides on top.
  * **Matrix static-switch naming**: ``Do*`` for boolean feature gates.
    Existing switches renamed from ``Use*`` to ``Do*`` (breaking for any MI
    that referenced the old names — the Test MI is rebuilt fresh by this
    template so no real MI breaks).

Feature flags (all driven by StaticSwitchParameter — runtime-free when off):

  A_Normal_DoDetail    — RNM-blend a second detail-normal texture onto the base
  A_DoBentNormal       — multiply bent-normal-based AO into AmbientOcclusion
  A_DoCharlieSheen     — add grazing sheen rim-light to EmissiveColor
  DoEmissive           — multiply emissive texture × tint × intensity, else zero
  DoWear               — lerp base-color / roughness toward a wear tint + rough
  DoWetness            — lerp roughness toward 0.08 + darken base-color
  DoAnisotropy         — expose Anisotropy scalar, else 0

Parameters (all MI-overridable):

  Base:       A_BaseColorTex / A_BaseColorTint / A_ORMTex / A_NormalTex
              A_Metallic_Scale / A_Normal_Intensity
  Roughness:  A_Rough_Low / A_Rough_High / A_Rough_SculptLow / A_Rough_SculptHigh
  Detail:     A_DetailNormalTex / A_DetailNormalScale / A_DetailNormalIntensity
  BentNormal: A_BentNormalTex / A_BentNormal_Strength
  Sheen:      A_SheenColor / A_SheenIntensity / A_SheenRoughness
  Emissive:   EmissiveTex / EmissiveTint / EmissiveIntensity
  Wear:       WearColor / WearRoughness / WearAmount
  Wetness:    WetnessAmount
  Anisotropy: AnisotropyAmount

Budget targets (SM5 / High): ≤ 400 instructions (raised from v1's 250 —
4 Custom nodes + sheen + bent-normal + 4-pt remap all expand inline),
≤ 10 samplers (6 textures sharing the global wrap sampler + 1 for the
bent normal = 7 unique sampler slots worst case; stays in budget).

Typical call shape::

    from material_templates import character_armor
    r = character_armor.build(
        master_path="/Game/BridgeTemplates/M_Character_Armor",
        mi_path="/Game/BridgeTemplates/MI_Character_Armor_Test",
        preview_png="material_previews/character_armor_default.png",
    )
    assert r["compile_clean"], r["compile_errors"]
    assert not r["over_instr"], r["max_instructions"]
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


# --- node positions (editor canvas x/y) -----------------------------------------

X_PARAM   = -1800
X_UV      = -1800
X_TEX     = -1400
X_DERIVE1 = -1050
X_DERIVE2 = -720
X_DERIVE3 = -420
X_SWITCH  = -140
X_OUTPUT  = 180

# y-band per logical section — leave generous gaps, graph now ~100 exprs.
Y_BASE    = -1100
Y_ROUGH   = -820
Y_NORMAL  = -520
Y_DETAIL  = -220
Y_BENT    = 80
Y_SHEEN   = 400
Y_EMISSIVE = 760
Y_WEAR    = 1080
Y_WETNESS = 1360
Y_ANISO   = 1600

SNIPPETS_PATH = "/Plugin/UnrealBridge/BridgeSnippets.ush"


def _row(base: int, i: int, spacing: int = 80) -> int:
    return base + i * spacing


# --- build ----------------------------------------------------------------------

def build(master_path: str = "/Game/BridgeTemplates/M_Character_Armor",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Character_Armor_Test",
          preview_png: Optional[str] = None,
          preview_mesh: str = C.DEFAULT_PREVIEW_MESH,
          preview_lighting: str = C.DEFAULT_PREVIEW_LIGHTING,
          preview_resolution: int = 512,
          preview_camera_yaw: float = 35.0,
          preview_camera_pitch: float = -15.0,
          preview_camera_distance: float = 0.0,
          instr_budget: int = 400,
          sampler_budget: int = 10,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Generate the Character_Armor master (+ optional MI + preview)."""
    L = unreal.UnrealBridgeMaterialLibrary

    # 1. Resolve the master material (create fresh or wipe-and-rebuild).
    master_path = C.ensure_master_material(
        master_path, "Surface", "DefaultLit", "Opaque",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    # 2. Add Custom HLSL nodes — each sits outside the ops batch because
    # apply_material_graph_ops can't configure Custom's typed inputs. Same
    # pattern used on Weapon_Hero / PP templates.

    custom_blend = L.add_custom_expression(
        master_path,
        X_DERIVE3, Y_NORMAL - 80,
        ["BaseN", "DetailN", "Strength"],
        "Float3",
        "return BridgeBlendAngleCorrectedNormals(BaseN, DetailN, Strength);",
        [SNIPPETS_PATH],
        "Detail normal RNM blend")
    if not custom_blend.success:
        raise RuntimeError(f"custom_blend: {custom_blend.error}")

    custom_remap = L.add_custom_expression(
        master_path,
        X_DERIVE2, Y_ROUGH + 80,
        ["value", "low", "high", "sculptLow", "sculptHigh"],
        "Float1",
        "return BridgeRoughnessRemap4Pt(value, low, high, sculptLow, sculptHigh);",
        [SNIPPETS_PATH],
        "4-point roughness remap")
    if not custom_remap.success:
        raise RuntimeError(f"custom_remap: {custom_remap.error}")

    custom_sheen = L.add_custom_expression(
        master_path,
        X_DERIVE3, Y_SHEEN + 80,
        ["N", "V", "roughness"],
        "Float1",
        "return BridgeCharlieSheenGrazing(N, V, roughness);",
        [SNIPPETS_PATH],
        "Charlie sheen grazing rim (scalar envelope)")
    if not custom_sheen.success:
        raise RuntimeError(f"custom_sheen: {custom_sheen.error}")

    custom_bent = L.add_custom_expression(
        master_path,
        X_DERIVE3, Y_BENT + 80,
        ["BentN", "V"],
        "Float1",
        "return BridgeBentNormalAO(BentN, V);",
        [SNIPPETS_PATH],
        "Bent normal micro-AO")
    if not custom_bent.success:
        raise RuntimeError(f"custom_bent: {custom_bent.error}")

    # 3. Build the ops batch.
    ops = C.OpList()
    ops.add_literal(C.guid_to_str(custom_blend.guid), "custom_blend_normals")
    ops.add_literal(C.guid_to_str(custom_remap.guid), "custom_rough_remap")
    ops.add_literal(C.guid_to_str(custom_sheen.guid), "custom_sheen_grazing")
    ops.add_literal(C.guid_to_str(custom_bent.guid),  "custom_bent_ao")

    # ---------- 01 Base params ------------------------------------------
    C.add_vector_param(ops, "vp_bc_tint", X_PARAM, _row(Y_BASE, 0),
                       "A_BaseColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Base", sort_priority=1)
    C.add_scalar_param(ops, "sp_metal_scale", X_PARAM, _row(Y_BASE, 1),
                       "A_Metallic_Scale", 0.0,
                       group="01 Base", sort_priority=2,
                       slider_min=0.0, slider_max=2.0)
    C.add_scalar_param(ops, "sp_normal_int", X_PARAM, _row(Y_BASE, 2),
                       "A_Normal_Intensity", 1.0,
                       group="01 Base", sort_priority=3,
                       slider_min=0.0, slider_max=2.0)

    # ---------- 02 Roughness (4-point remap) ----------------------------
    # Permissive slider ranges — Matrix sets *_Low to negative + *_High > 1
    # for extreme remap curves. helper emits all 4 params at the right
    # slider bounds.
    rough_syms = C.add_roughness_remap_4pt(
        ops, "A", X_PARAM, _row(Y_ROUGH, 0),
        group="02 Roughness",
        low=0.5, high=0.85, sculpt_low=0.0, sculpt_high=1.0,
        base_priority=1)

    # ---------- 03 Detail normal ----------------------------------------
    C.add_scalar_param(ops, "sp_detail_scale", X_PARAM, _row(Y_DETAIL, 0),
                       "A_DetailNormalScale", 4.0,
                       group="03 Detail", sort_priority=1,
                       slider_min=0.25, slider_max=16.0)
    C.add_scalar_param(ops, "sp_detail_int", X_PARAM, _row(Y_DETAIL, 1),
                       "A_DetailNormalIntensity", 0.75,
                       group="03 Detail", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)

    # ---------- 04 Bent normal ------------------------------------------
    C.add_scalar_param(ops, "sp_bent_strength", X_PARAM, _row(Y_BENT, 0),
                       "A_BentNormal_Strength", 0.6,
                       group="04 BentNormal", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)

    # ---------- 05 Sheen (Charlie-grazing) ------------------------------
    sheen_syms = C.add_charlie_sheen_scalars(
        ops, "A", X_PARAM, _row(Y_SHEEN, 0),
        group="05 Sheen",
        sheen_intensity_default=0.0,
        sheen_roughness_default=0.5,
        base_priority=1)

    # ---------- 06 Emissive ---------------------------------------------
    C.add_vector_param(ops, "vp_emis_tint", X_PARAM, _row(Y_EMISSIVE, 0),
                       "EmissiveTint", "(R=1,G=1,B=1,A=1)",
                       group="06 Emissive", sort_priority=1)
    C.add_scalar_param(ops, "sp_emis_int", X_PARAM, _row(Y_EMISSIVE, 1),
                       "EmissiveIntensity", 3.0,
                       group="06 Emissive", sort_priority=2,
                       slider_min=0.0, slider_max=50.0)

    # ---------- 07 Wear / 08 Wetness / 09 Anisotropy --------------------
    C.add_vector_param(ops, "vp_wear_color", X_PARAM, _row(Y_WEAR, 0),
                       "WearColor", "(R=0.08,G=0.06,B=0.05,A=1)",
                       group="07 Wear", sort_priority=1)
    C.add_scalar_param(ops, "sp_wear_rough", X_PARAM, _row(Y_WEAR, 1),
                       "WearRoughness", 0.70,
                       group="07 Wear", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_wear_amount", X_PARAM, _row(Y_WEAR, 2),
                       "WearAmount", 0.35,
                       group="07 Wear", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_wet_amount", X_PARAM, _row(Y_WETNESS, 0),
                       "WetnessAmount", 0.0,
                       group="08 Wetness", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_anis_amount", X_PARAM, _row(Y_ANISO, 0),
                       "AnisotropyAmount", 0.0,
                       group="09 Anisotropy", sort_priority=1,
                       slider_min=-1.0, slider_max=1.0)

    # ---------- UV0 + textures (col X_TEX) -----------------------------
    ops.add("uv0", "TextureCoordinate", X_UV, Y_DETAIL + 400)

    C.add_texture_param_2d(ops, "tp_basecolor", X_TEX, Y_BASE,
                           "A_BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=1)
    C.add_texture_param_2d(ops, "tp_orm", X_TEX, Y_ROUGH,
                           "A_ORMTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Masks",
                           group="Textures", sort_priority=2)
    C.add_texture_param_2d(ops, "tp_normal", X_TEX, Y_NORMAL,
                           "A_NormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=3)
    C.add_texture_param_2d(ops, "tp_detail_normal", X_TEX, Y_DETAIL,
                           "A_DetailNormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=4)
    C.add_texture_param_2d(ops, "tp_bent_normal", X_TEX, Y_BENT,
                           "A_BentNormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=5)
    C.add_texture_param_2d(ops, "tp_emissive", X_TEX, Y_EMISSIVE,
                           "EmissiveTex", C.DEFAULT_BLACK_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=6)

    # Regular textures share uv0.
    for tex in ("tp_basecolor", "tp_orm", "tp_normal",
                "tp_bent_normal", "tp_emissive"):
        ops.connect("uv0", "", tex, "UVs")

    # ---------- Static switches (Matrix Do* convention) ------------------
    def _switch(name: str, param_name: str, x: int, y: int,
                group: str) -> None:
        C.add_static_switch_param(ops, name, x, y,
                                  param_name, default=False,
                                  group=group, sort_priority=0)

    _switch("ss_detail",    "A_Normal_DoDetail",
            X_SWITCH, _row(Y_NORMAL, 0), "02 Normal")
    _switch("ss_bent",      "A_DoBentNormal",
            X_SWITCH, _row(Y_BENT, 0), "04 BentNormal")
    _switch("ss_sheen",     "A_DoCharlieSheen",
            X_SWITCH, _row(Y_SHEEN, 0), "05 Sheen")
    _switch("ss_emissive",  "DoEmissive",
            X_SWITCH, _row(Y_EMISSIVE, 0), "06 Emissive")
    _switch("ss_bc_wear",   "DoWear",
            X_SWITCH, _row(Y_BASE, 0), "07 Wear")
    _switch("ss_rough_wear","DoWear",
            X_SWITCH, _row(Y_WEAR, 0), "07 Wear")
    _switch("ss_bc_wet",    "DoWetness",
            X_SWITCH, _row(Y_WETNESS - 200, 0), "08 Wetness")
    _switch("ss_rough_wet", "DoWetness",
            X_SWITCH, _row(Y_WETNESS, 0), "08 Wetness")
    _switch("ss_anis",      "DoAnisotropy",
            X_SWITCH, _row(Y_ANISO, 0), "09 Anisotropy")

    # ---------- BaseColor chain ----------------------------------------
    ops.add("mul_bc_tint", "Multiply", X_DERIVE1, Y_BASE)
    ops.connect("tp_basecolor", "RGB", "mul_bc_tint", "A")
    ops.connect("vp_bc_tint", "", "mul_bc_tint", "B")

    ops.add("lerp_bc_wear", "LinearInterpolate", X_DERIVE2, Y_WEAR - 200)
    ops.connect("mul_bc_tint", "", "lerp_bc_wear", "A")
    ops.connect("vp_wear_color", "", "lerp_bc_wear", "B")
    ops.connect("sp_wear_amount", "", "lerp_bc_wear", "Alpha")

    ops.connect("lerp_bc_wear", "", "ss_bc_wear", "True")
    ops.connect("mul_bc_tint",  "", "ss_bc_wear", "False")

    ops.add("const_one_bc_wet", "Constant", X_DERIVE2, Y_WETNESS - 260)
    ops.setp("const_one_bc_wet", "R", "1.0")
    ops.add("const_wet_floor", "Constant", X_DERIVE2, Y_WETNESS - 180)
    ops.setp("const_wet_floor", "R", "0.55")

    ops.add("lerp_wet_coef", "LinearInterpolate", X_DERIVE3, Y_WETNESS - 240)
    ops.connect("const_one_bc_wet", "", "lerp_wet_coef", "A")
    ops.connect("const_wet_floor",  "", "lerp_wet_coef", "B")
    ops.connect("sp_wet_amount",    "", "lerp_wet_coef", "Alpha")

    ops.add("mul_bc_wet", "Multiply", X_DERIVE3, Y_BASE + 80)
    ops.connect("ss_bc_wear",    "", "mul_bc_wet", "A")
    ops.connect("lerp_wet_coef", "", "mul_bc_wet", "B")

    ops.connect("mul_bc_wet",   "", "ss_bc_wet", "True")
    ops.connect("ss_bc_wear",   "", "ss_bc_wet", "False")

    # ---------- Roughness chain (4-point remap → wear → wetness) -------
    # The remap takes ORM.G as 'value' and the 4 param scalars as the
    # Low/High/SculptLow/SculptHigh endpoints. Output: Float1 in [0,1].
    ops.connect("tp_orm",                  "G",  "custom_rough_remap", "value")
    ops.connect(rough_syms["low"],         "",   "custom_rough_remap", "low")
    ops.connect(rough_syms["high"],        "",   "custom_rough_remap", "high")
    ops.connect(rough_syms["sculpt_low"],  "",   "custom_rough_remap", "sculptLow")
    ops.connect(rough_syms["sculpt_high"], "",   "custom_rough_remap", "sculptHigh")

    ops.add("lerp_rough_wear", "LinearInterpolate", X_DERIVE2, Y_WEAR - 60)
    ops.connect("custom_rough_remap", "", "lerp_rough_wear", "A")
    ops.connect("sp_wear_rough",      "", "lerp_rough_wear", "B")
    ops.connect("sp_wear_amount",     "", "lerp_rough_wear", "Alpha")

    ops.connect("lerp_rough_wear",    "", "ss_rough_wear", "True")
    ops.connect("custom_rough_remap", "", "ss_rough_wear", "False")

    ops.add("const_rough_wet_floor", "Constant", X_DERIVE2, Y_WETNESS + 20)
    ops.setp("const_rough_wet_floor", "R", "0.08")

    ops.add("lerp_rough_wet", "LinearInterpolate", X_DERIVE3, Y_WETNESS)
    ops.connect("ss_rough_wear",         "", "lerp_rough_wet", "A")
    ops.connect("const_rough_wet_floor", "", "lerp_rough_wet", "B")
    ops.connect("sp_wet_amount",         "", "lerp_rough_wet", "Alpha")

    ops.connect("lerp_rough_wet",  "", "ss_rough_wet", "True")
    ops.connect("ss_rough_wear",   "", "ss_rough_wet", "False")

    # ---------- Metallic -----------------------------------------------
    ops.add("mul_metal", "Multiply", X_DERIVE2, Y_WEAR + 140)
    ops.connect("tp_orm",         "B", "mul_metal", "A")
    ops.connect("sp_metal_scale", "",  "mul_metal", "B")

    # ---------- Normal chain -------------------------------------------
    ops.add("c3v_flat_normal", "Constant3Vector", X_DERIVE1, Y_NORMAL - 180)
    ops.setp("c3v_flat_normal", "Constant", "(R=0,G=0,B=1,A=0)")

    ops.add("lerp_base_normal", "LinearInterpolate", X_DERIVE2, Y_NORMAL - 80)
    ops.connect("c3v_flat_normal", "",    "lerp_base_normal", "A")
    ops.connect("tp_normal",       "RGB", "lerp_base_normal", "B")
    ops.connect("sp_normal_int",   "",    "lerp_base_normal", "Alpha")

    ops.add("mul_uv_detail", "Multiply", X_DERIVE1, Y_DETAIL + 120)
    ops.connect("uv0",              "", "mul_uv_detail", "A")
    ops.connect("sp_detail_scale",  "", "mul_uv_detail", "B")
    ops.connect("mul_uv_detail",    "", "tp_detail_normal", "UVs")

    ops.add("lerp_detail_normal", "LinearInterpolate", X_DERIVE2, Y_DETAIL + 220)
    ops.connect("c3v_flat_normal",    "",    "lerp_detail_normal", "A")
    ops.connect("tp_detail_normal",   "RGB", "lerp_detail_normal", "B")
    ops.connect("sp_detail_int",      "",    "lerp_detail_normal", "Alpha")

    ops.add("const_normal_strength", "Constant", X_DERIVE2, Y_NORMAL + 60)
    ops.setp("const_normal_strength", "R", "1.0")

    ops.connect("lerp_base_normal",    "", "custom_blend_normals", "BaseN")
    ops.connect("lerp_detail_normal",  "", "custom_blend_normals", "DetailN")
    ops.connect("const_normal_strength","","custom_blend_normals", "Strength")

    ops.connect("custom_blend_normals", "", "ss_detail", "True")
    ops.connect("lerp_base_normal",     "", "ss_detail", "False")

    # ---------- Bent-normal-driven AO -----------------------------------
    # AO = saturate(ORM.R * (1 - Strength) + ORM.R * BridgeBentNormalAO(BentN,V) * Strength)
    # = Lerp(ORM.R, ORM.R * bentAO, BentNormal_Strength)
    ops.add("cam_vec", "CameraVectorWS", X_DERIVE1, Y_BENT + 160)
    ops.connect("tp_bent_normal", "RGB", "custom_bent_ao", "BentN")
    ops.connect("cam_vec",         "",    "custom_bent_ao", "V")

    ops.add("mul_ao_bent", "Multiply", X_DERIVE2 - 40, Y_BENT)
    ops.connect("tp_orm",          "R", "mul_ao_bent", "A")
    ops.connect("custom_bent_ao",  "",  "mul_ao_bent", "B")

    ops.add("lerp_ao_bent", "LinearInterpolate", X_DERIVE3 - 80, Y_BENT)
    ops.connect("tp_orm",          "R", "lerp_ao_bent", "A")
    ops.connect("mul_ao_bent",     "",  "lerp_ao_bent", "B")
    ops.connect("sp_bent_strength","",  "lerp_ao_bent", "Alpha")

    ops.connect("lerp_ao_bent", "", "ss_bent", "True")
    ops.connect("tp_orm",       "R","ss_bent", "False")

    # ---------- Charlie sheen (rim-light envelope → Emissive) ----------
    # sheen_contrib = BridgeCharlieSheenGrazing(N, V, sheenRoughness) × sheenColor × sheenIntensity
    # Input N comes from the already-blended normal (ss_detail output).
    ops.add("cam_vec_sheen", "CameraVectorWS", X_DERIVE1, Y_SHEEN + 260)

    ops.connect("ss_detail",               "",  "custom_sheen_grazing", "N")
    ops.connect("cam_vec_sheen",           "",  "custom_sheen_grazing", "V")
    ops.connect(sheen_syms["roughness"],   "",  "custom_sheen_grazing", "roughness")

    ops.add("mul_sheen_color", "Multiply", X_DERIVE3, Y_SHEEN + 160)
    ops.connect("custom_sheen_grazing",    "", "mul_sheen_color", "A")
    ops.connect(sheen_syms["color"],       "", "mul_sheen_color", "B")

    ops.add("mul_sheen_intensity", "Multiply", X_DERIVE3 + 220, Y_SHEEN + 160)
    ops.connect("mul_sheen_color",          "", "mul_sheen_intensity", "A")
    ops.connect(sheen_syms["intensity"],    "", "mul_sheen_intensity", "B")

    ops.add("c3v_sheen_off", "Constant3Vector", X_DERIVE3, Y_SHEEN + 280)
    ops.setp("c3v_sheen_off", "Constant", "(R=0,G=0,B=0,A=0)")

    ops.connect("mul_sheen_intensity", "", "ss_sheen", "True")
    ops.connect("c3v_sheen_off",       "", "ss_sheen", "False")

    # ---------- Emissive chain (base + sheen add) ----------------------
    ops.add("mul_emis_tex_tint", "Multiply", X_DERIVE1, Y_EMISSIVE + 80)
    ops.connect("tp_emissive",  "RGB", "mul_emis_tex_tint", "A")
    ops.connect("vp_emis_tint", "",    "mul_emis_tex_tint", "B")

    ops.add("mul_emis_final", "Multiply", X_DERIVE2, Y_EMISSIVE + 80)
    ops.connect("mul_emis_tex_tint", "", "mul_emis_final", "A")
    ops.connect("sp_emis_int",       "", "mul_emis_final", "B")

    ops.add("c3v_emis_zero", "Constant3Vector", X_DERIVE2, Y_EMISSIVE + 200)
    ops.setp("c3v_emis_zero", "Constant", "(R=0,G=0,B=0,A=0)")

    ops.connect("mul_emis_final", "", "ss_emissive", "True")
    ops.connect("c3v_emis_zero",  "", "ss_emissive", "False")

    # final_emissive = ss_emissive + ss_sheen
    ops.add("add_emis_sheen", "Add", X_SWITCH + 120, Y_EMISSIVE + 40)
    ops.connect("ss_emissive", "", "add_emis_sheen", "A")
    ops.connect("ss_sheen",    "", "add_emis_sheen", "B")

    # ---------- Anisotropy ---------------------------------------------
    ops.add("const_aniso_zero", "Constant", X_DERIVE3, Y_ANISO + 80)
    ops.setp("const_aniso_zero", "R", "0.0")
    ops.connect("sp_anis_amount",   "", "ss_anis", "True")
    ops.connect("const_aniso_zero", "", "ss_anis", "False")

    # ---------- Main output connections --------------------------------
    ops.connect_out("ss_bc_wet",       "", "BaseColor")
    ops.connect_out("mul_metal",       "", "Metallic")
    ops.connect_out("ss_rough_wet",    "", "Roughness")
    ops.connect_out("ss_bent",         "", "AmbientOcclusion")
    ops.connect_out("ss_detail",       "", "Normal")
    ops.connect_out("add_emis_sheen",  "", "EmissiveColor")
    ops.connect_out("ss_anis",         "", "Anisotropy")

    # ---------- Visual grouping (comment blocks per section) -----------
    ops.comment(X_TEX - 60, Y_BASE - 140, 1900, 200,
                "01 Base — BaseColorTex × Tint → wear/wet lerps",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 60, Y_ROUGH - 140, 1900, 220,
                "02 Roughness — ORM.G → 4-point remap (Low/SculptLow/SculptHigh/High) → wear/wet",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 60, Y_NORMAL - 220, 1900, 220,
                "03 Base Normal (tex × intensity, default flat when disabled)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 60, Y_DETAIL - 40, 1900, 260,
                "04 Detail Normal (UV × DetailScale → RNM blend gated by A_Normal_DoDetail)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 60, Y_BENT - 40, 1900, 260,
                "05 Bent Normal AO (BentN · V refined AO, lerp by A_BentNormal_Strength)",
                C.COMMENT_COLOR_WETNESS)
    ops.comment(X_TEX - 60, Y_SHEEN - 40, 1900, 320,
                "06 Charlie Sheen Grazing (rim-light × sheenColor × intensity → added to Emissive)",
                C.COMMENT_COLOR_SWITCH)
    ops.comment(X_TEX - 60, Y_EMISSIVE - 60, 1900, 300,
                "07 Emissive (tex × tint × intensity) + Sheen add-on",
                C.COMMENT_COLOR_EMISSIVE)
    ops.comment(X_TEX - 60, Y_WEAR - 320, 1900, 560,
                "08 Wear — BaseColor + Roughness lerp toward wear values",
                C.COMMENT_COLOR_WEAR)
    ops.comment(X_TEX - 60, Y_WETNESS - 320, 1900, 300,
                "09 Wetness (base-color darken + roughness → ~0.08)",
                C.COMMENT_COLOR_WETNESS)
    ops.comment(X_TEX - 60, Y_ANISO - 60, 1900, 180,
                "10 Anisotropy (scalar override when DoAnisotropy = true)",
                C.COMMENT_COLOR_SWITCH)

    # 4. Flush ops + compile.
    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")
    C.save_master(master_path)

    # 5. Collect stats + budget check.
    stats = C.collect_stats(master_path) if compile else {}
    budget = C.check_budget(stats, instr_budget, sampler_budget) if stats else {}

    out: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(result.ops_applied),
        "custom_node_guids": {
            "blend_normals":   C.guid_to_str(custom_blend.guid),
            "rough_remap":     C.guid_to_str(custom_remap.guid),
            "sheen_grazing":   C.guid_to_str(custom_sheen.guid),
            "bent_ao":         C.guid_to_str(custom_bent.guid),
        },
    }
    if stats:
        out.update({
            "max_instructions": stats["max_instructions"],
            "sampler_count": stats["sampler_count"],
            "vt_stack_count": stats["vt_stack_count"],
            "compile_errors": stats["compile_errors"],
            "compile_clean": len(stats["compile_errors"]) == 0,
            "num_expressions": stats["num_expressions"],
        })
        out.update({k: budget[k] for k in
                    ("over_instr", "over_sampler", "ok")})

    # 6. Optional: create test MI + preview render.
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
            preview_target,
            preview_mesh,
            preview_lighting,
            preview_resolution,
            preview_camera_yaw,
            preview_camera_pitch,
            preview_camera_distance,
            preview_png)
        out["preview_ok"] = bool(ok)
        out["preview_png"] = preview_png

    return out

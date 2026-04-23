"""M3-5: M_Weapon_Hero — hero-weapon master, dual-UV + animated emissive pulse.

Built on top of the M3-2 Character_Armor pipeline (same PBR core + detail
normal + wear + wetness + anisotropy toggles) with three additions
specific to hero weapons:

  1. **Dual UV set** — BaseColor / ORM / Normal / Detail sample UV0
     (object layout), while the emissive path samples UV1. This lets
     designers tile the emissive mask independently of the main texture
     atlas so glowing runes / circuits can repeat without dragging the
     whole albedo with them.
  2. **Emissive mask** — a second TextureSampleParameter2D (``EmissiveMaskTex``)
     tiled via UV1 × ``EmissiveMaskScale`` + optional scalar boost. Multiplied
     into the emissive chain so designers can keep the base emissive colour
     texture continuous and paint the glowing regions as a greyscale mask.
  3. **Pulse animation** — ``0.5 + 0.5 * sin(Time * PulseFrequency)``
     remapped to ``[PulseMin, 1]`` and multiplied into the final emissive
     intensity. Gated by ``UsePulse`` static switch so the sine math is
     dead-coded when a designer wants a constant glow.

Not included (deferred — handoff notes them as optional):

  * Parallax Occlusion Mapping — requires either a ~40-line HLSL snippet
    with internal texture samples (triggers M5-12) or ~16 graph-side
    pre-samples fed into a Custom (ugly). Revisit when graph-side POM
    wiring becomes ergonomic enough.
  * Curve Atlas driven pulse — a DataAsset-driven curve would replace the
    parameter-driven sine. The parameter-driven sine covers 90% of pulse
    authoring with zero extra asset surface area.

Parameters (all MI-overridable):

  Base:       BaseColorTex / BaseColorTint / ORMTex / NormalTex
              RoughnessMin / RoughnessMax / MetallicScale / NormalIntensity
  Detail:     DetailNormalTex / DetailNormalScale / DetailNormalIntensity
  Emissive:   EmissiveTex / EmissiveTint / EmissiveIntensity
              EmissiveMaskTex / EmissiveMaskScale / EmissiveMaskBoost
  Pulse:      PulseFrequency / PulseMin
  Wear:       WearColor / WearRoughness / WearAmount
  Wetness:    WetnessAmount
  Anisotropy: AnisotropyAmount

Static switches (default all false — stock permutation is lean core PBR):

  UseDetailNormal / UseEmissive / UsePulse / UseWear / UseWetness / UseAnisotropy

Budget targets (SM5 / High): ≤ 260 instructions, ≤ 10 samplers.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import unreal

from . import _common as C


# --- node positions (editor canvas x/y) -----------------------------------------

X_PARAM = -1800
X_UV = -1800
X_TEX = -1400
X_DERIVE1 = -1050
X_DERIVE2 = -720
X_DERIVE3 = -420
X_SWITCH = -140
X_OUTPUT = 180

Y_BASE = -900
Y_NORMAL = -480
Y_DETAIL = -220
Y_EMISSIVE = 220
Y_WEAR = 620
Y_WETNESS = 920
Y_ANISO = 1160


def _row(base: int, i: int, spacing: int = 80) -> int:
    return base + i * spacing


def build(master_path: str = "/Game/BridgeTemplates/M_Weapon_Hero",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Weapon_Hero_Test",
          preview_png: Optional[str] = None,
          preview_mesh: str = C.DEFAULT_PREVIEW_MESH,
          preview_lighting: str = C.DEFAULT_PREVIEW_LIGHTING,
          preview_resolution: int = 512,
          preview_camera_yaw: float = 35.0,
          preview_camera_pitch: float = -15.0,
          preview_camera_distance: float = 0.0,
          instr_budget: int = 260,
          sampler_budget: int = 10,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary

    # Master material (create or wipe-and-rebuild).
    master_path = C.ensure_master_material(
        master_path, "Surface", "DefaultLit", "Opaque",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    # Custom BlendAngleCorrectedNormals (same as armor — detail normal RNM blend).
    custom = L.add_custom_expression(
        master_path,
        X_DERIVE3, Y_NORMAL - 80,
        ["BaseN", "DetailN", "Strength"],
        "Float3",
        "return BridgeBlendAngleCorrectedNormals(BaseN, DetailN, Strength);",
        ["/Plugin/UnrealBridge/BridgeSnippets.ush"],
        "Detail normal blend (RNM)")
    if not custom.success:
        raise RuntimeError(f"add_custom_expression failed: {custom.error}")

    ops = C.OpList()
    ops.add_literal(C.guid_to_str(custom.guid), "custom_blend_normals")

    # ---------- parameters (col X_PARAM) --------------------------------
    C.add_vector_param(ops, "vp_bc_tint", X_PARAM, _row(Y_BASE, 0),
                       "BaseColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Base", sort_priority=1)
    C.add_scalar_param(ops, "sp_rough_min", X_PARAM, _row(Y_BASE, 1),
                       "RoughnessMin", 0.15, group="01 Base", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_rough_max", X_PARAM, _row(Y_BASE, 2),
                       "RoughnessMax", 0.85, group="01 Base", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_metal_mult", X_PARAM, _row(Y_BASE, 3),
                       "MetallicScale", 1.0, group="01 Base", sort_priority=4,
                       slider_min=0.0, slider_max=2.0)
    C.add_scalar_param(ops, "sp_normal_int", X_PARAM, _row(Y_NORMAL, 0),
                       "NormalIntensity", 1.0, group="02 Normal", sort_priority=1,
                       slider_min=0.0, slider_max=2.0)

    C.add_scalar_param(ops, "sp_detail_scale", X_PARAM, _row(Y_DETAIL, 0),
                       "DetailNormalScale", 4.0, group="03 Detail", sort_priority=1,
                       slider_min=0.25, slider_max=16.0)
    C.add_scalar_param(ops, "sp_detail_int", X_PARAM, _row(Y_DETAIL, 1),
                       "DetailNormalIntensity", 0.75, group="03 Detail", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)

    C.add_vector_param(ops, "vp_emis_tint", X_PARAM, _row(Y_EMISSIVE, 0),
                       "EmissiveTint", "(R=1,G=0.75,B=0.25,A=1)",
                       group="04 Emissive", sort_priority=1)
    C.add_scalar_param(ops, "sp_emis_int", X_PARAM, _row(Y_EMISSIVE, 1),
                       "EmissiveIntensity", 8.0, group="04 Emissive", sort_priority=2,
                       slider_min=0.0, slider_max=50.0)
    C.add_scalar_param(ops, "sp_emis_mask_scale", X_PARAM, _row(Y_EMISSIVE, 2),
                       "EmissiveMaskScale", 1.0, group="04 Emissive", sort_priority=3,
                       slider_min=0.1, slider_max=16.0)
    C.add_scalar_param(ops, "sp_emis_mask_boost", X_PARAM, _row(Y_EMISSIVE, 3),
                       "EmissiveMaskBoost", 1.0, group="04 Emissive", sort_priority=4,
                       slider_min=0.0, slider_max=4.0)

    C.add_scalar_param(ops, "sp_pulse_freq", X_PARAM, _row(Y_EMISSIVE, 5),
                       "PulseFrequency", 2.0, group="05 Pulse", sort_priority=1,
                       slider_min=0.0, slider_max=20.0)
    C.add_scalar_param(ops, "sp_pulse_min", X_PARAM, _row(Y_EMISSIVE, 6),
                       "PulseMin", 0.35, group="05 Pulse", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)

    C.add_vector_param(ops, "vp_wear_color", X_PARAM, _row(Y_WEAR, 0),
                       "WearColor", "(R=0.08,G=0.06,B=0.05,A=1)",
                       group="06 Wear", sort_priority=1)
    C.add_scalar_param(ops, "sp_wear_rough", X_PARAM, _row(Y_WEAR, 1),
                       "WearRoughness", 0.70, group="06 Wear", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_wear_amount", X_PARAM, _row(Y_WEAR, 2),
                       "WearAmount", 0.35, group="06 Wear", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_wet_amount", X_PARAM, _row(Y_WETNESS, 0),
                       "WetnessAmount", 0.0, group="07 Wetness", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_anis_amount", X_PARAM, _row(Y_ANISO, 0),
                       "AnisotropyAmount", 0.40, group="08 Anisotropy", sort_priority=1,
                       slider_min=-1.0, slider_max=1.0)

    # ---------- texture samples (col X_TEX) -----------------------------
    ops.add("uv0", "TextureCoordinate", X_UV, _row(Y_DETAIL, 5))

    # UV1 — second UV set. CoordinateIndex=1 addresses the mesh's UV channel 1.
    ops.add("uv1", "TextureCoordinate", X_UV, _row(Y_EMISSIVE, 8))
    ops.setp("uv1", "CoordinateIndex", "1")

    C.add_texture_param_2d(ops, "tp_basecolor", X_TEX, Y_BASE,
                           "BaseColorTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=1)
    C.add_texture_param_2d(ops, "tp_orm", X_TEX, Y_WEAR - 250,
                           "ORMTex", C.DEFAULT_WHITE_TEX,
                           sampler_type="Masks",
                           group="Textures", sort_priority=2)
    C.add_texture_param_2d(ops, "tp_normal", X_TEX, Y_NORMAL,
                           "NormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=3)
    C.add_texture_param_2d(ops, "tp_detail_normal", X_TEX, Y_DETAIL + 220,
                           "DetailNormalTex", C.DEFAULT_NORMAL_TEX,
                           sampler_type="Normal",
                           group="Textures", sort_priority=4)
    C.add_texture_param_2d(ops, "tp_emissive", X_TEX, Y_EMISSIVE + 220,
                           "EmissiveTex", C.DEFAULT_BLACK_TEX,
                           sampler_type="Color",
                           group="Textures", sort_priority=5)
    C.add_texture_param_2d(ops, "tp_emis_mask", X_TEX, Y_EMISSIVE + 480,
                           "EmissiveMaskTex", C.DEFAULT_BLACK_TEX,
                           sampler_type="Masks",
                           group="Textures", sort_priority=6)

    # Main UV hookup.
    for tex in ("tp_basecolor", "tp_orm", "tp_normal", "tp_emissive"):
        ops.connect("uv0", "", tex, "UVs")

    # ---------- static switches -----------------------------------------
    def _switch(name: str, param_name: str, x: int, y: int, group: str) -> None:
        C.add_static_switch_param(ops, name, x, y,
                                  param_name, default=False,
                                  group=group, sort_priority=0)

    _switch("ss_use_detail", "UseDetailNormal",
            X_SWITCH, _row(Y_NORMAL, 0), "02 Normal")
    _switch("ss_use_emissive", "UseEmissive",
            X_SWITCH, _row(Y_EMISSIVE, 0), "04 Emissive")
    _switch("ss_use_pulse", "UsePulse",
            X_SWITCH, _row(Y_EMISSIVE, 4), "05 Pulse")
    _switch("ss_bc_wear", "UseWear",
            X_SWITCH, _row(Y_BASE, 0), "06 Wear")
    _switch("ss_rough_wear", "UseWear",
            X_SWITCH, _row(Y_WEAR, 0), "06 Wear")
    _switch("ss_bc_wet", "UseWetness",
            X_SWITCH, _row(Y_WETNESS - 150, 0), "07 Wetness")
    _switch("ss_rough_wet", "UseWetness",
            X_SWITCH, _row(Y_WETNESS, 0), "07 Wetness")
    _switch("ss_anis", "UseAnisotropy",
            X_SWITCH, _row(Y_ANISO, 0), "08 Anisotropy")

    # ---------- base PBR chain (identical to armor) ---------------------
    ops.add("mul_bc_tint", "Multiply", X_DERIVE1, Y_BASE)
    ops.connect("tp_basecolor", "RGB", "mul_bc_tint", "A")
    ops.connect("vp_bc_tint", "", "mul_bc_tint", "B")

    ops.add("lerp_bc_wear", "LinearInterpolate", X_DERIVE2, Y_WEAR - 120)
    ops.connect("mul_bc_tint", "", "lerp_bc_wear", "A")
    ops.connect("vp_wear_color", "", "lerp_bc_wear", "B")
    ops.connect("sp_wear_amount", "", "lerp_bc_wear", "Alpha")

    ops.connect("lerp_bc_wear", "", "ss_bc_wear", "True")
    ops.connect("mul_bc_tint", "", "ss_bc_wear", "False")

    ops.add("const_one_a", "Constant", X_DERIVE2, Y_WETNESS - 220)
    ops.setp("const_one_a", "R", "1.0")
    ops.add("const_wet_floor", "Constant", X_DERIVE2, Y_WETNESS - 140)
    ops.setp("const_wet_floor", "R", "0.55")

    ops.add("lerp_wet_coef", "LinearInterpolate", X_DERIVE3, Y_WETNESS - 200)
    ops.connect("const_one_a", "", "lerp_wet_coef", "A")
    ops.connect("const_wet_floor", "", "lerp_wet_coef", "B")
    ops.connect("sp_wet_amount", "", "lerp_wet_coef", "Alpha")

    ops.add("mul_bc_wet", "Multiply", X_DERIVE3, Y_BASE + 80)
    ops.connect("ss_bc_wear", "", "mul_bc_wet", "A")
    ops.connect("lerp_wet_coef", "", "mul_bc_wet", "B")

    ops.connect("mul_bc_wet", "", "ss_bc_wet", "True")
    ops.connect("ss_bc_wear", "", "ss_bc_wet", "False")

    # Roughness chain.
    ops.add("lerp_rough_base", "LinearInterpolate", X_DERIVE1, Y_WEAR - 240)
    ops.connect("sp_rough_min", "", "lerp_rough_base", "A")
    ops.connect("sp_rough_max", "", "lerp_rough_base", "B")
    ops.connect("tp_orm", "G", "lerp_rough_base", "Alpha")

    ops.add("lerp_rough_wear", "LinearInterpolate", X_DERIVE2, Y_WEAR - 60)
    ops.connect("lerp_rough_base", "", "lerp_rough_wear", "A")
    ops.connect("sp_wear_rough", "", "lerp_rough_wear", "B")
    ops.connect("sp_wear_amount", "", "lerp_rough_wear", "Alpha")

    ops.connect("lerp_rough_wear", "", "ss_rough_wear", "True")
    ops.connect("lerp_rough_base", "", "ss_rough_wear", "False")

    ops.add("const_rough_wet_floor", "Constant", X_DERIVE2, Y_WETNESS + 20)
    ops.setp("const_rough_wet_floor", "R", "0.08")

    ops.add("lerp_rough_wet", "LinearInterpolate", X_DERIVE3, Y_WETNESS)
    ops.connect("ss_rough_wear", "", "lerp_rough_wet", "A")
    ops.connect("const_rough_wet_floor", "", "lerp_rough_wet", "B")
    ops.connect("sp_wet_amount", "", "lerp_rough_wet", "Alpha")

    ops.connect("lerp_rough_wet", "", "ss_rough_wet", "True")
    ops.connect("ss_rough_wear", "", "ss_rough_wet", "False")

    # Metallic.
    ops.add("mul_metal", "Multiply", X_DERIVE2, Y_WEAR + 80)
    ops.connect("tp_orm", "B", "mul_metal", "A")
    ops.connect("sp_metal_mult", "", "mul_metal", "B")

    # Normal chain.
    ops.add("c3v_flat_normal", "Constant3Vector", X_DERIVE1, Y_NORMAL - 220)
    ops.setp("c3v_flat_normal", "Constant", "(R=0,G=0,B=1,A=0)")

    ops.add("lerp_base_normal", "LinearInterpolate", X_DERIVE2, Y_NORMAL - 120)
    ops.connect("c3v_flat_normal", "", "lerp_base_normal", "A")
    ops.connect("tp_normal", "RGB", "lerp_base_normal", "B")
    ops.connect("sp_normal_int", "", "lerp_base_normal", "Alpha")

    ops.add("mul_uv_detail", "Multiply", X_DERIVE1, Y_DETAIL + 120)
    ops.connect("uv0", "", "mul_uv_detail", "A")
    ops.connect("sp_detail_scale", "", "mul_uv_detail", "B")
    ops.connect("mul_uv_detail", "", "tp_detail_normal", "UVs")

    ops.add("lerp_detail_normal", "LinearInterpolate", X_DERIVE2, Y_DETAIL + 220)
    ops.connect("c3v_flat_normal", "", "lerp_detail_normal", "A")
    ops.connect("tp_detail_normal", "RGB", "lerp_detail_normal", "B")
    ops.connect("sp_detail_int", "", "lerp_detail_normal", "Alpha")

    ops.add("const_strength_one", "Constant", X_DERIVE2, Y_NORMAL + 20)
    ops.setp("const_strength_one", "R", "1.0")

    ops.connect("lerp_base_normal", "", "custom_blend_normals", "BaseN")
    ops.connect("lerp_detail_normal", "", "custom_blend_normals", "DetailN")
    ops.connect("const_strength_one", "", "custom_blend_normals", "Strength")

    ops.connect("custom_blend_normals", "", "ss_use_detail", "True")
    ops.connect("lerp_base_normal", "", "ss_use_detail", "False")

    # ---------- emissive chain (dual UV + mask + pulse) -----------------
    # emissive UV1 scaled for the mask (base emissive texture keeps UV0).
    ops.add("mul_uv1_scale", "Multiply", X_DERIVE1, Y_EMISSIVE + 480)
    ops.connect("uv1", "", "mul_uv1_scale", "A")
    ops.connect("sp_emis_mask_scale", "", "mul_uv1_scale", "B")
    ops.connect("mul_uv1_scale", "", "tp_emis_mask", "UVs")

    # emissive_base = tp_emissive × tint × intensity
    ops.add("mul_emis_tex_tint", "Multiply", X_DERIVE1, Y_EMISSIVE + 80)
    ops.connect("tp_emissive", "RGB", "mul_emis_tex_tint", "A")
    ops.connect("vp_emis_tint", "", "mul_emis_tex_tint", "B")

    ops.add("mul_emis_int", "Multiply", X_DERIVE2, Y_EMISSIVE + 80)
    ops.connect("mul_emis_tex_tint", "", "mul_emis_int", "A")
    ops.connect("sp_emis_int", "", "mul_emis_int", "B")

    # mask_weight = mask.R × boost (saturated so boost=0 → no mask, 1 → full mask)
    ops.add("mul_mask_boost", "Multiply", X_DERIVE1, Y_EMISSIVE + 580)
    ops.connect("tp_emis_mask", "R", "mul_mask_boost", "A")
    ops.connect("sp_emis_mask_boost", "", "mul_mask_boost", "B")

    ops.add("mask_sat", "Saturate", X_DERIVE2, Y_EMISSIVE + 580)
    ops.connect("mul_mask_boost", "", "mask_sat", "")

    # gated_emis = mul_emis_int × mask_sat
    ops.add("mul_emis_masked", "Multiply", X_DERIVE3, Y_EMISSIVE + 180)
    ops.connect("mul_emis_int", "", "mul_emis_masked", "A")
    ops.connect("mask_sat", "", "mul_emis_masked", "B")

    # Pulse: 0.5 + 0.5 * sin(Time * PulseFrequency)
    ops.add("time", "Time", X_DERIVE1, Y_EMISSIVE + 700)

    ops.add("mul_time_freq", "Multiply", X_DERIVE2, Y_EMISSIVE + 700)
    ops.connect("time", "", "mul_time_freq", "A")
    ops.connect("sp_pulse_freq", "", "mul_time_freq", "B")

    ops.add("sin_pulse", "Sine", X_DERIVE3, Y_EMISSIVE + 700)
    ops.connect("mul_time_freq", "", "sin_pulse", "")

    ops.add("const_half_1", "Constant", X_DERIVE3, Y_EMISSIVE + 780)
    ops.setp("const_half_1", "R", "0.5")
    ops.add("const_half_2", "Constant", X_DERIVE3, Y_EMISSIVE + 820)
    ops.setp("const_half_2", "R", "0.5")

    # pulse_norm = 0.5 + 0.5 * sin(t*f)  → [0..1]
    ops.add("half_sin", "Multiply", X_DERIVE3 + 200, Y_EMISSIVE + 720)
    ops.connect("sin_pulse", "", "half_sin", "A")
    ops.connect("const_half_1", "", "half_sin", "B")

    ops.add("pulse_norm", "Add", X_DERIVE3 + 400, Y_EMISSIVE + 720)
    ops.connect("half_sin", "", "pulse_norm", "A")
    ops.connect("const_half_2", "", "pulse_norm", "B")

    # pulse_factor = lerp(PulseMin, 1, pulse_norm)  — keeps a floor intensity
    ops.add("const_one_pulse", "Constant", X_DERIVE3 + 200, Y_EMISSIVE + 880)
    ops.setp("const_one_pulse", "R", "1.0")

    ops.add("pulse_factor", "LinearInterpolate", X_DERIVE3 + 600, Y_EMISSIVE + 760)
    ops.connect("sp_pulse_min", "", "pulse_factor", "A")
    ops.connect("const_one_pulse", "", "pulse_factor", "B")
    ops.connect("pulse_norm", "", "pulse_factor", "Alpha")

    # ss_use_pulse: True=pulse_factor, False=1.0
    ops.connect("pulse_factor", "", "ss_use_pulse", "True")
    ops.connect("const_one_pulse", "", "ss_use_pulse", "False")

    # final emissive pre-switch = mul_emis_masked × ss_use_pulse
    ops.add("mul_emis_pulsed", "Multiply", X_SWITCH - 160, Y_EMISSIVE + 260)
    ops.connect("mul_emis_masked", "", "mul_emis_pulsed", "A")
    ops.connect("ss_use_pulse", "", "mul_emis_pulsed", "B")

    ops.add("c3v_emis_zero", "Constant3Vector", X_SWITCH - 160, Y_EMISSIVE + 380)
    ops.setp("c3v_emis_zero", "Constant", "(R=0,G=0,B=0,A=0)")

    ops.connect("mul_emis_pulsed", "", "ss_use_emissive", "True")
    ops.connect("c3v_emis_zero", "", "ss_use_emissive", "False")

    # Anisotropy.
    ops.add("const_aniso_zero", "Constant", X_DERIVE3, Y_ANISO + 80)
    ops.setp("const_aniso_zero", "R", "0.0")
    ops.connect("sp_anis_amount", "", "ss_anis", "True")
    ops.connect("const_aniso_zero", "", "ss_anis", "False")

    # ---------- main outputs --------------------------------------------
    ops.connect_out("ss_bc_wet", "", "BaseColor")
    ops.connect_out("mul_metal", "", "Metallic")
    ops.connect_out("ss_rough_wet", "", "Roughness")
    ops.connect_out("tp_orm", "R", "AmbientOcclusion")
    ops.connect_out("ss_use_detail", "", "Normal")
    ops.connect_out("ss_use_emissive", "", "EmissiveColor")
    ops.connect_out("ss_anis", "", "Anisotropy")

    # ---------- comments ------------------------------------------------
    ops.comment(X_TEX - 60, Y_BASE - 110, 1900, 190,
                "Base PBR (BaseColor × Tint → ORM split → Metallic/AO)",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 60, Y_NORMAL - 270, 1900, 220,
                "Normal: base texture × intensity  (+ optional detail blend)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 60, Y_DETAIL - 40, 1900, 300,
                "Detail normal (UV × DetailNormalScale, RNM blend)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 60, Y_EMISSIVE - 60, 2200, 700,
                "Emissive: UV0 colour × tint × intensity × (UV1-tiled mask)"
                " — optional sine-driven Pulse",
                C.COMMENT_COLOR_EMISSIVE)
    ops.comment(X_TEX - 60, Y_WEAR - 320, 1900, 560,
                "Wear + Roughness (ORM.G → rough_min..rough_max, wear lerp)",
                C.COMMENT_COLOR_WEAR)
    ops.comment(X_TEX - 60, Y_WETNESS - 260, 1900, 300,
                "Wetness (base-color darken + roughness → ~0.08)",
                C.COMMENT_COLOR_WETNESS)
    ops.comment(X_TEX - 60, Y_ANISO - 60, 1900, 180,
                "Anisotropy (scalar override when UseAnisotropy = true)",
                C.COMMENT_COLOR_SWITCH)

    # Flush + compile + save.
    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")
    C.save_master(master_path)

    stats = C.collect_stats(master_path) if compile else {}
    budget = C.check_budget(stats, instr_budget, sampler_budget) if stats else {}

    out: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(result.ops_applied),
        "custom_node_guid": C.guid_to_str(custom.guid),
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
        out.update({k: budget[k] for k in ("over_instr", "over_sampler", "ok")})

    if mi_path:
        if unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            out["mi_path"] = mi_path
        else:
            mi_r = L.create_material_instance(master_path, mi_path)
            if mi_r.success:
                out["mi_path"] = str(mi_r.path) or mi_path
            else:
                out["mi_error"] = mi_r.error

    if preview_png:
        target = out.get("mi_path") or master_path
        ok = L.preview_material(target, preview_mesh, preview_lighting,
                                preview_resolution, preview_camera_yaw,
                                preview_camera_pitch, preview_camera_distance,
                                preview_png)
        out["preview_ok"] = bool(ok)
        out["preview_png"] = preview_png

    return out

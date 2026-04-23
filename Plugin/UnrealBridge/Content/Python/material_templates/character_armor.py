"""M3-2: M_Character_Armor — AAA character / armor master material.

Feature flags (all driven by StaticSwitchParameter — runtime-free when off):

  UseDetailNormal   — RNM-blend a second detail-normal texture onto the base
  UseEmissive       — multiply emissive texture × tint × intensity, else zero
  UseWear           — lerp base-color / roughness toward a wear tint + rough
  UseWetness        — lerp roughness toward 0.08 + darken base-color
  UseAnisotropy     — expose Anisotropy scalar, else 0

Parameters (all MI-overridable):

  Base:       BaseColorTex / BaseColorTint / ORMTex / NormalTex
              RoughnessMin / RoughnessMax / MetallicScale / NormalIntensity
  Detail:     DetailNormalTex / DetailNormalScale / DetailNormalIntensity
  Emissive:   EmissiveTex / EmissiveTint / EmissiveIntensity
  Wear:       WearColor / WearRoughness / WearAmount
  Wetness:    WetnessAmount
  Anisotropy: AnisotropyAmount

Budget targets (SM5 / High): <= 250 instructions, <= 10 samplers (achieved by
SSM_Wrap_WorldGroupSettings shared sampler on every texture → 0 unique
sampler slots beyond the global shared one).

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

X_PARAM = -1800
X_UV = -1800
X_TEX = -1400
X_DERIVE1 = -1050
X_DERIVE2 = -720
X_DERIVE3 = -420
X_SWITCH = -140
X_OUTPUT = 180

# y-band per logical section
Y_BASE = -900
Y_NORMAL = -480
Y_DETAIL = -220
Y_EMISSIVE = 220
Y_WEAR = 620
Y_WETNESS = 920
Y_ANISO = 1160

# Small per-node offset within a band (when multiple params live in a column)
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
          instr_budget: int = 250,
          sampler_budget: int = 10,
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Generate the Character_Armor master (+ optional MI + preview).

    Returns a dict with fields::

        master_path, mi_path, preview_png,
        ops_applied, compile_clean, compile_errors,
        max_instructions, sampler_count, vt_stack_count,
        over_instr, over_sampler, ok
    """
    L = unreal.UnrealBridgeMaterialLibrary

    # 1. Resolve the master material (create fresh or wipe-and-rebuild).
    master_path = C.ensure_master_material(
        master_path, "Surface", "DefaultLit", "Opaque",
        two_sided=False, use_material_attributes=False,
        rebuild=rebuild)

    # 2. Add the Custom HLSL normal-blend node — separate API call because
    #    apply_material_graph_ops cannot configure Custom's typed inputs.
    custom = L.add_custom_expression(
        master_path,
        X_DERIVE3, Y_NORMAL - 80,
        ["BaseN", "DetailN", "Strength"],
        "Float3",
        "return BridgeBlendAngleCorrectedNormals(BaseN, DetailN, Strength);",
        ["/Plugin/UnrealBridge/BridgeSnippets.ush"],
        "Detail normal blend (RNM)",
    )
    if not custom.success:
        raise RuntimeError(f"add_custom_expression failed: {custom.error}")
    custom_guid_str = C.guid_to_str(custom.guid)

    # 3. Build the ops batch.
    ops = C.OpList()
    ops.add_literal(custom_guid_str, "custom_blend_normals")

    # ---------- parameter block (col X_PARAM) ---------------------------
    C.add_vector_param(ops, "vp_bc_tint", X_PARAM, _row(Y_BASE, 0),
                       "BaseColorTint", "(R=1,G=1,B=1,A=1)",
                       group="01 Base", sort_priority=1)
    C.add_scalar_param(ops, "sp_rough_min", X_PARAM, _row(Y_BASE, 1),
                       "RoughnessMin", 0.15, group="01 Base", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_rough_max", X_PARAM, _row(Y_BASE, 2),
                       "RoughnessMax", 0.90, group="01 Base", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_metal_mult", X_PARAM, _row(Y_BASE, 3),
                       "MetallicScale", 0.0, group="01 Base", sort_priority=4,
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
                       "EmissiveTint", "(R=1,G=1,B=1,A=1)",
                       group="04 Emissive", sort_priority=1)
    C.add_scalar_param(ops, "sp_emis_int", X_PARAM, _row(Y_EMISSIVE, 1),
                       "EmissiveIntensity", 3.0, group="04 Emissive", sort_priority=2,
                       slider_min=0.0, slider_max=50.0)

    C.add_vector_param(ops, "vp_wear_color", X_PARAM, _row(Y_WEAR, 0),
                       "WearColor", "(R=0.08,G=0.06,B=0.05,A=1)",
                       group="05 Wear", sort_priority=1)
    C.add_scalar_param(ops, "sp_wear_rough", X_PARAM, _row(Y_WEAR, 1),
                       "WearRoughness", 0.70, group="05 Wear", sort_priority=2,
                       slider_min=0.0, slider_max=1.0)
    C.add_scalar_param(ops, "sp_wear_amount", X_PARAM, _row(Y_WEAR, 2),
                       "WearAmount", 0.35, group="05 Wear", sort_priority=3,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_wet_amount", X_PARAM, _row(Y_WETNESS, 0),
                       "WetnessAmount", 0.60, group="06 Wetness", sort_priority=1,
                       slider_min=0.0, slider_max=1.0)

    C.add_scalar_param(ops, "sp_anis_amount", X_PARAM, _row(Y_ANISO, 0),
                       "AnisotropyAmount", 0.60, group="07 Anisotropy", sort_priority=1,
                       slider_min=-1.0, slider_max=1.0)

    # ---------- texture samples (col X_TEX) -----------------------------
    ops.add("uv0", "TextureCoordinate", X_UV, _row(Y_DETAIL, 5))
    # CoordinateIndex defaults to 0 — we don't set it.

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

    # All textures share the default UV set (wire uv0 → each sampler's UVs).
    # DetailNormal gets a multiplied UV (see below).
    for tex in ("tp_basecolor", "tp_orm", "tp_normal", "tp_emissive"):
        ops.connect("uv0", "", tex, "UVs")

    # ---------- static switches (parameters — shared permutation keys) ---
    def _static_switch(name: str, param_name: str, x: int, y: int,
                       group: str) -> None:
        C.add_static_switch_param(ops, name, x, y,
                                  param_name, default=False,
                                  group=group, sort_priority=0)

    _static_switch("ss_use_detail", "UseDetailNormal",
                   X_SWITCH, _row(Y_NORMAL, 0), "02 Normal")
    _static_switch("ss_use_emissive", "UseEmissive",
                   X_SWITCH, _row(Y_EMISSIVE, 0), "04 Emissive")
    _static_switch("ss_bc_wear", "UseWear",
                   X_SWITCH, _row(Y_BASE, 0), "05 Wear")
    _static_switch("ss_rough_wear", "UseWear",
                   X_SWITCH, _row(Y_WEAR, 0), "05 Wear")
    _static_switch("ss_bc_wet", "UseWetness",
                   X_SWITCH, _row(Y_WETNESS - 150, 0), "06 Wetness")
    _static_switch("ss_rough_wet", "UseWetness",
                   X_SWITCH, _row(Y_WETNESS, 0), "06 Wetness")
    _static_switch("ss_anis", "UseAnisotropy",
                   X_SWITCH, _row(Y_ANISO, 0), "07 Anisotropy")

    # ---------- derived math -------------------------------------------

    # BaseColor chain: tex * tint → (wear lerp) → (wet darken) → BaseColor
    ops.add("mul_bc_tint", "Multiply", X_DERIVE1, Y_BASE)
    ops.connect("tp_basecolor", "RGB", "mul_bc_tint", "A")
    # VectorParameter exposes "", R, G, B, A — no "RGB" pin. Default output
    # is float4; the Multiply compiler auto-truncates against the float3 peer.
    ops.connect("vp_bc_tint", "", "mul_bc_tint", "B")

    ops.add("lerp_bc_wear", "LinearInterpolate", X_DERIVE2, Y_WEAR - 120)
    ops.connect("mul_bc_tint", "", "lerp_bc_wear", "A")
    ops.connect("vp_wear_color", "", "lerp_bc_wear", "B")
    ops.connect("sp_wear_amount", "", "lerp_bc_wear", "Alpha")

    # ss_bc_wear: True=lerp_bc_wear, False=mul_bc_tint
    ops.connect("lerp_bc_wear", "", "ss_bc_wear", "True")
    ops.connect("mul_bc_tint", "", "ss_bc_wear", "False")

    # Wetness darken coefficient: Lerp(1.0, 0.55, WetnessAmount)
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

    # ss_bc_wet: True=mul_bc_wet, False=ss_bc_wear
    ops.connect("mul_bc_wet", "", "ss_bc_wet", "True")
    ops.connect("ss_bc_wear", "", "ss_bc_wet", "False")

    # Roughness chain: Lerp(min, max, ORM.G) → wear lerp → wetness lerp
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

    # Metallic chain: ORM.B × MetallicScale
    ops.add("mul_metal", "Multiply", X_DERIVE2, Y_WEAR + 80)
    ops.connect("tp_orm", "B", "mul_metal", "A")
    ops.connect("sp_metal_mult", "", "mul_metal", "B")

    # Normal chain
    ops.add("c3v_flat_normal", "Constant3Vector", X_DERIVE1, Y_NORMAL - 220)
    ops.setp("c3v_flat_normal", "Constant", "(R=0,G=0,B=1,A=0)")

    ops.add("lerp_base_normal", "LinearInterpolate", X_DERIVE2, Y_NORMAL - 120)
    ops.connect("c3v_flat_normal", "", "lerp_base_normal", "A")
    ops.connect("tp_normal", "RGB", "lerp_base_normal", "B")
    ops.connect("sp_normal_int", "", "lerp_base_normal", "Alpha")

    # Detail UV = UV0 × DetailNormalScale
    ops.add("mul_uv_detail", "Multiply", X_DERIVE1, Y_DETAIL + 120)
    ops.connect("uv0", "", "mul_uv_detail", "A")
    ops.connect("sp_detail_scale", "", "mul_uv_detail", "B")
    ops.connect("mul_uv_detail", "", "tp_detail_normal", "UVs")

    ops.add("lerp_detail_normal", "LinearInterpolate", X_DERIVE2, Y_DETAIL + 220)
    ops.connect("c3v_flat_normal", "", "lerp_detail_normal", "A")
    ops.connect("tp_detail_normal", "RGB", "lerp_detail_normal", "B")
    ops.connect("sp_detail_int", "", "lerp_detail_normal", "Alpha")

    # Strength = 1 (the detail intensity lerp already scales how much
    # contribution the DetailN carries into the blend; pass-through is fine).
    ops.add("const_strength_one", "Constant", X_DERIVE2, Y_NORMAL + 20)
    ops.setp("const_strength_one", "R", "1.0")

    ops.connect("lerp_base_normal", "", "custom_blend_normals", "BaseN")
    ops.connect("lerp_detail_normal", "", "custom_blend_normals", "DetailN")
    ops.connect("const_strength_one", "", "custom_blend_normals", "Strength")

    ops.connect("custom_blend_normals", "", "ss_use_detail", "True")
    ops.connect("lerp_base_normal", "", "ss_use_detail", "False")

    # Emissive chain
    ops.add("mul_emis_tex_tint", "Multiply", X_DERIVE1, Y_EMISSIVE + 80)
    ops.connect("tp_emissive", "RGB", "mul_emis_tex_tint", "A")
    ops.connect("vp_emis_tint", "", "mul_emis_tex_tint", "B")

    ops.add("mul_emis_final", "Multiply", X_DERIVE2, Y_EMISSIVE + 80)
    ops.connect("mul_emis_tex_tint", "", "mul_emis_final", "A")
    ops.connect("sp_emis_int", "", "mul_emis_final", "B")

    ops.add("c3v_emis_zero", "Constant3Vector", X_DERIVE2, Y_EMISSIVE + 200)
    ops.setp("c3v_emis_zero", "Constant", "(R=0,G=0,B=0,A=0)")

    ops.connect("mul_emis_final", "", "ss_use_emissive", "True")
    ops.connect("c3v_emis_zero", "", "ss_use_emissive", "False")

    # Anisotropy chain
    ops.add("const_aniso_zero", "Constant", X_DERIVE3, Y_ANISO + 80)
    ops.setp("const_aniso_zero", "R", "0.0")
    ops.connect("sp_anis_amount", "", "ss_anis", "True")
    ops.connect("const_aniso_zero", "", "ss_anis", "False")

    # ---------- main output connections ---------------------------------
    ops.connect_out("ss_bc_wet", "", "BaseColor")
    ops.connect_out("mul_metal", "", "Metallic")
    ops.connect_out("ss_rough_wet", "", "Roughness")
    ops.connect_out("tp_orm", "R", "AmbientOcclusion")
    ops.connect_out("ss_use_detail", "", "Normal")
    ops.connect_out("ss_use_emissive", "", "EmissiveColor")
    ops.connect_out("ss_anis", "", "Anisotropy")

    # ---------- visual grouping ----------------------------------------
    ops.comment(X_TEX - 60, Y_BASE - 110, 1900, 190,
                "Base PBR (BaseColor × Tint → ORM split → Metallic/AO)",
                C.COMMENT_COLOR_PBR)
    ops.comment(X_TEX - 60, Y_NORMAL - 270, 1900, 220,
                "Normal: base texture × intensity  (+ optional detail blend)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 60, Y_DETAIL - 40, 1900, 300,
                "Detail normal (UV × DetailNormalScale, RNM blend)",
                C.COMMENT_COLOR_NORMAL)
    ops.comment(X_TEX - 60, Y_EMISSIVE - 60, 1900, 280,
                "Emissive (tex × tint × intensity)",
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

    # 4. Flush ops + compile.
    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")
    # Persist the master to disk — compile=True recompiles shaders but leaves
    # the asset dirty in memory; an editor restart would lose the build.
    C.save_master(master_path)

    # 5. Collect stats + budget check.
    stats = C.collect_stats(master_path) if compile else {}
    budget = C.check_budget(stats, instr_budget, sampler_budget) if stats else {}

    out: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(result.ops_applied),
        "custom_node_guid": custom_guid_str,
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
            out["mi_path"] = mi_path  # reuse existing MI — parent is already wired
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

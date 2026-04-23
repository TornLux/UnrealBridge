"""M3-7: M_Layered_Base + MF_Layer_{Metal, Fabric, Dirt} — layered-material framework.

Layers via ``UMaterialFunction`` + ``BlendMaterialAttributes`` rather than
UE's heavier MaterialLayerStacks system. MaterialLayerStacks requires two
special function types (``UMaterialFunctionMaterialLayer`` +
``UMaterialFunctionMaterialLayerBlend``) with their own editor UX — for a
3-4 layer blend driven by vertex colour masks the plain MF + Blend pattern
gives 90% of the authoring ergonomics with a quarter of the asset surface.

Three parametric layer MFs:

  MF_Layer_Metal   — Metal_BaseColor / Metal_Roughness / Metal_Metallic
                     (default: gunmetal grey, rough=0.30, metal=1.0)
  MF_Layer_Fabric  — Fabric_BaseColor / Fabric_Roughness / Fabric_Metallic
                     (default: deep-red cloth, rough=0.85, metal=0.0)
  MF_Layer_Dirt    — Dirt_BaseColor / Dirt_Roughness / Dirt_Metallic
                     (default: wet brown dirt, rough=0.95, metal=0.0)

Each MF outputs a ``MakeMaterialAttributes`` struct with a flat normal
(authorable detail normals would be a v2 pass). Parameters are exposed via
the standard UE MI-propagation: every ScalarParameter / VectorParameter
declared inside an MF shows up in the parent material's MI parameter list
verbatim, so designers override ``Metal_Roughness`` from the MI the same
way they'd override any scalar.

Master graph:

  blend_mf    = BlendMaterialAttributes(metal, fabric, VertexColor.R)
  dirt_mask   = VertexColor.G * Dirt_Opacity      # global attenuator
  final       = BlendMaterialAttributes(blend_mf, dirt, dirt_mask)
  (MaterialAttributes output)

Designers paint:
  * VertexColor.R → 0=metal, 1=fabric (per-vertex material picker)
  * VertexColor.G → dirt weight (grunge layer on top)

``Dirt_Opacity`` is a master-level ScalarParameter so a single MI slider
attenuates the whole dirt pass without touching vertex colours.

Why the MFs aren't a ``MaterialAttributeLayers`` stack: that expression
requires the two-function asset pattern above, and the current bridge C++
APIs only edit UMaterial graphs (not UMaterialFunction graphs). We route
around it by doing MF authoring through the raw ``MaterialEditingLibrary``
Python API and only the master through the batched bridge ops.
"""

from __future__ import annotations

from typing import Any, Dict, Optional, Tuple

import unreal

from . import _common as C


MF_METAL_PATH  = "/Game/BridgeTemplates/MF_Layer_Metal"
MF_FABRIC_PATH = "/Game/BridgeTemplates/MF_Layer_Fabric"
MF_DIRT_PATH   = "/Game/BridgeTemplates/MF_Layer_Dirt"


def _build_layer_mf(path: str, prefix: str,
                    roughness: float, metallic: float,
                    basecolor: Tuple[float, float, float, float],
                    description: str) -> str:
    """Create / rebuild one layer MaterialFunction.

    Graph: 2 ScalarParams + 1 VectorParam + flat-normal Constant3Vector →
    MakeMaterialAttributes → FunctionOutput. Parameters live in one group
    named after the prefix so they collate nicely in MI details panels.
    """
    L = unreal.UnrealBridgeMaterialLibrary
    MEL = unreal.MaterialEditingLibrary

    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        r = L.create_material_function(path, description, True, "BridgeLayers")
        if not r.success:
            raise RuntimeError(f"create_material_function({path}) failed: {r.error}")

    mf = unreal.EditorAssetLibrary.load_asset(path)
    MEL.delete_all_material_expressions_in_function(mf)

    x_param, x_mma, x_out = -600, -100, 300

    sp_rough = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionScalarParameter, x_param, -40)
    sp_rough.set_editor_property("parameter_name", f"{prefix}_Roughness")
    sp_rough.set_editor_property("default_value", roughness)
    sp_rough.set_editor_property("group", prefix)
    sp_rough.set_editor_property("slider_min", 0.0)
    sp_rough.set_editor_property("slider_max", 1.0)
    sp_rough.set_editor_property("sort_priority", 1)

    sp_metal = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionScalarParameter, x_param, 40)
    sp_metal.set_editor_property("parameter_name", f"{prefix}_Metallic")
    sp_metal.set_editor_property("default_value", metallic)
    sp_metal.set_editor_property("group", prefix)
    sp_metal.set_editor_property("slider_min", 0.0)
    sp_metal.set_editor_property("slider_max", 1.0)
    sp_metal.set_editor_property("sort_priority", 2)

    vp_bc = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionVectorParameter, x_param, 120)
    vp_bc.set_editor_property("parameter_name", f"{prefix}_BaseColor")
    vp_bc.set_editor_property("default_value", unreal.LinearColor(*basecolor))
    vp_bc.set_editor_property("group", prefix)
    vp_bc.set_editor_property("sort_priority", 3)

    c3v_flat = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionConstant3Vector, x_param, 220)
    c3v_flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 0.0))

    mma = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionMakeMaterialAttributes, x_mma, 60)

    # VectorParameter output "" is float4 — auto-truncated into BaseColor float3.
    MEL.connect_material_expressions(vp_bc, "", mma, "BaseColor")
    MEL.connect_material_expressions(sp_metal, "", mma, "Metallic")
    MEL.connect_material_expressions(sp_rough, "", mma, "Roughness")
    MEL.connect_material_expressions(c3v_flat, "", mma, "Normal")

    fo = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionFunctionOutput, x_out, 60)
    fo.set_editor_property("output_name", "Result")
    MEL.connect_material_expressions(mma, "", fo, "")

    MEL.update_material_function(mf)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return path


def build(master_path: str = "/Game/BridgeTemplates/M_Layered_Base",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Layered_Base_Test",
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    L = unreal.UnrealBridgeMaterialLibrary
    MEL = unreal.MaterialEditingLibrary

    # 1. Build the three layer MFs. Always rebuilt — they're cheap (~8 nodes each)
    # and we want them in lockstep with this template version.
    _build_layer_mf(MF_METAL_PATH,  "Metal",  0.30, 1.00,
                    (0.55, 0.55, 0.58, 1.0), "M3-7 Metal PBR layer")
    _build_layer_mf(MF_FABRIC_PATH, "Fabric", 0.85, 0.00,
                    (0.55, 0.25, 0.22, 1.0), "M3-7 Fabric PBR layer")
    _build_layer_mf(MF_DIRT_PATH,   "Dirt",   0.95, 0.00,
                    (0.20, 0.15, 0.10, 1.0), "M3-7 Dirt overlay layer")

    # 2. Master material — use_material_attributes=True so the output is one
    # MaterialAttributes pin that accepts a Blend chain.
    master_path = C.ensure_master_material(
        master_path, "Surface", "DefaultLit", "Opaque",
        two_sided=False, use_material_attributes=True,
        rebuild=rebuild)
    master = unreal.EditorAssetLibrary.load_asset(master_path)

    mf_metal  = unreal.EditorAssetLibrary.load_asset(MF_METAL_PATH)
    mf_fabric = unreal.EditorAssetLibrary.load_asset(MF_FABRIC_PATH)
    mf_dirt   = unreal.EditorAssetLibrary.load_asset(MF_DIRT_PATH)

    # 3. MFCall nodes — must be created via MEL directly so
    # ``set_material_function`` can populate the pin set from the MF's
    # FunctionInput / FunctionOutput signatures. A raw ``add`` op from
    # apply_material_graph_ops would leave the call node with no output pins,
    # which would fail at the connect step.
    #
    # Unique (x, y) positions so we can recover GUIDs via get_material_graph
    # below — ``material_expression_guid`` is not a UPROPERTY, so Python can't
    # read it directly off the returned expression pointer.
    call_metal_pos  = (-1200, -200)
    call_fabric_pos = (-1200,   40)
    call_dirt_pos   = (-1200,  280)

    call_metal = MEL.create_material_expression(
        master, unreal.MaterialExpressionMaterialFunctionCall, *call_metal_pos)
    call_metal.set_material_function(mf_metal)

    call_fabric = MEL.create_material_expression(
        master, unreal.MaterialExpressionMaterialFunctionCall, *call_fabric_pos)
    call_fabric.set_material_function(mf_fabric)

    call_dirt = MEL.create_material_expression(
        master, unreal.MaterialExpressionMaterialFunctionCall, *call_dirt_pos)
    call_dirt.set_material_function(mf_dirt)

    # Recover GUIDs via the graph query — the MEL-returned expression pointer
    # doesn't expose material_expression_guid on the Python side.
    graph = L.get_material_graph(master_path)

    def _find_guid(cls: str, pos: tuple) -> str:
        for n in graph.nodes:
            if str(n.class_name) == cls and int(n.x) == pos[0] and int(n.y) == pos[1]:
                return C.guid_to_str(n.guid)
        raise RuntimeError(f"could not find {cls} at {pos} in master graph")

    metal_guid  = _find_guid("MaterialFunctionCall", call_metal_pos)
    fabric_guid = _find_guid("MaterialFunctionCall", call_fabric_pos)
    dirt_guid   = _find_guid("MaterialFunctionCall", call_dirt_pos)

    # 4. Remainder of the master — batch-applied via the bridge ops list.
    ops = C.OpList()
    ops.add_literal(metal_guid,  "call_metal")
    ops.add_literal(fabric_guid, "call_fabric")
    ops.add_literal(dirt_guid,   "call_dirt")

    # Vertex colour drives the blend masks. R = metal→fabric blend, G = dirt weight.
    ops.add("vc", "VertexColor", -1200, 520)

    # Global dirt opacity — multiplies VC.G for a single MI attenuator.
    C.add_scalar_param(ops, "sp_dirt_opacity", -1200, 640,
                       "Dirt_Opacity", 1.0,
                       group="Dirt", sort_priority=99,
                       slider_min=0.0, slider_max=1.0)

    ops.add("mul_dirt_mask", "Multiply", -900, 580)
    ops.connect("vc", "G", "mul_dirt_mask", "A")
    ops.connect("sp_dirt_opacity", "", "mul_dirt_mask", "B")

    # BlendMaterialAttributes uses A / B / Alpha pin names (same as Lerp).
    ops.add("blend_mf", "BlendMaterialAttributes", -700, -80)
    ops.connect("call_metal",  "", "blend_mf", "A")
    ops.connect("call_fabric", "", "blend_mf", "B")
    ops.connect("vc", "R", "blend_mf", "Alpha")

    ops.add("blend_final", "BlendMaterialAttributes", -400, 100)
    ops.connect("blend_mf",  "", "blend_final", "A")
    ops.connect("call_dirt", "", "blend_final", "B")
    ops.connect("mul_dirt_mask", "", "blend_final", "Alpha")

    ops.connect_out("blend_final", "", "MaterialAttributes")

    ops.comment(-1260, -260, 1300, 1020,
                "M_Layered_Base — Metal + Fabric + Dirt via BlendMaterialAttributes",
                C.COMMENT_COLOR_PBR)

    result = ops.flush(master_path, compile)
    if not result.success:
        raise RuntimeError(
            f"apply_material_graph_ops failed at op #{result.failed_at_index}: "
            f"{result.error}")
    C.save_master(master_path)

    stats = C.collect_stats(master_path) if compile else {}

    out: Dict[str, Any] = {
        "master_path": master_path,
        "ops_applied": int(result.ops_applied),
        "mf_paths": {
            "metal":  MF_METAL_PATH,
            "fabric": MF_FABRIC_PATH,
            "dirt":   MF_DIRT_PATH,
        },
    }
    if stats:
        out.update({
            "max_instructions": stats["max_instructions"],
            "sampler_count": stats["sampler_count"],
            "compile_errors": stats["compile_errors"],
            "compile_clean": len(stats["compile_errors"]) == 0,
            "num_expressions": stats["num_expressions"],
        })

    if mi_path:
        if unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            out["mi_path"] = mi_path
        else:
            mi_r = L.create_material_instance(master_path, mi_path)
            if mi_r.success:
                out["mi_path"] = str(mi_r.path) or mi_path
            else:
                out["mi_error"] = mi_r.error

    return out

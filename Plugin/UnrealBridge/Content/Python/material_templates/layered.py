"""M3-7: M_Layered_Stack + ML_Layer_{Metal, Fabric, Dirt} + MLB_{Fabric_Over_Metal, Dirt_Over_Layers}.

UE-standard MaterialLayerStacks: three ``UMaterialFunctionMaterialLayer``
assets + two ``UMaterialFunctionMaterialLayerBlend`` assets wired into a
single ``UMaterialExpressionMaterialAttributeLayers`` node on the master.

Contrast with the previous MF + BlendMaterialAttributes hand-build at
``/Game/BridgeTemplates/M_Layered_Base``:

* The master graph is now a **single** MAL expression instead of a chain
  of BlendMaterialAttributes + MaterialFunctionCall nodes — the layer
  stack is data-driven via the MAL node's ``DefaultLayers`` struct rather
  than hand-routed in the graph.
* Designers edit the layer stack from the Material Editor's Layers panel
  (the native UX for AAA workflows). The old master can only be edited by
  re-running this script.
* MIs can replace individual layers / blends via the standard
  ``LayerFunctions`` + ``BlendFunctions`` parameter system (no custom
  parameter plumbing on our side).

Layer stack:

  [0] Metal   — base (no blend below it)
  [1] Fabric  — blended over Metal using VertexColor.R
  [2] Dirt    — blended over the previous result using VertexColor.G

Paint VertexColor.R to pick between metal / fabric, paint VertexColor.G
to grunge-mask dirt on top.

The previous ``M_Layered_Base`` master is replaced by ``M_Layered_Stack``.
The three old ``MF_Layer_*`` MFs (regular ``UMaterialFunction``) are
replaced by ``ML_Layer_*`` (``UMaterialFunctionMaterialLayer``).

**Asset-creation hazard**: ``UMaterialFunctionMaterialLayer`` and
``UMaterialFunctionMaterialLayerBlend`` are specialized MF subclasses
created via dedicated factories. Bundling all five factories + a master
build + MI create in one exec risks the asset-reference-completing modal
that already bit us on the Texture2DFactoryNew path. The pattern:

  1. First bridge exec → ``ensure_layer_stack_assets(rebuild=...)`` —
     creates the 5 MFs, saves, returns.
  2. Second bridge exec → ``build(rebuild=...)`` — master + MAL
     expression; asserts the 5 assets exist.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

import unreal

from . import _common as C


ML_METAL_PATH  = "/Game/BridgeTemplates/ML_Layer_Metal"
ML_FABRIC_PATH = "/Game/BridgeTemplates/ML_Layer_Fabric"
ML_DIRT_PATH   = "/Game/BridgeTemplates/ML_Layer_Dirt"

MLB_VC_R_PATH  = "/Game/BridgeTemplates/MLB_VertexColorR"
MLB_VC_G_PATH  = "/Game/BridgeTemplates/MLB_VertexColorG"


def _build_material_layer_mf(path: str, prefix: str,
                             roughness: float, metallic: float,
                             basecolor: Tuple[float, float, float, float],
                             description: str) -> str:
    """Create / rebuild one ``UMaterialFunctionMaterialLayer`` asset.

    Graph: 2 ScalarParams + 1 VectorParam + flat-normal Constant3Vector →
    MakeMaterialAttributes → FunctionOutput "Result". Parameters live in
    one group named after the prefix so they collate in MI details.
    """
    MEL = unreal.MaterialEditingLibrary
    atools = unreal.AssetToolsHelpers.get_asset_tools()

    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        pkg_path, name = path.rsplit("/", 1)
        factory = unreal.MaterialFunctionMaterialLayerFactory()
        atools.create_asset(name, pkg_path,
                            unreal.MaterialFunctionMaterialLayer, factory)

    mf = unreal.EditorAssetLibrary.load_asset(path)
    mf.set_editor_property("description", description)
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
    fo.set_editor_property("output_name", "Material Attributes")
    MEL.connect_material_expressions(mma, "", fo, "")

    MEL.update_material_function(mf)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return path


def _build_material_layer_blend_mf(path: str, vc_channel: str,
                                   description: str) -> str:
    """Create / rebuild one ``UMaterialFunctionMaterialLayerBlend`` asset.

    Graph: two MaterialAttributes inputs (Background, Layer) blended by
    VertexColor.<channel> via BlendMaterialAttributes, then output.
    ``vc_channel`` is ``R`` / ``G`` / ``B`` / ``A``.
    """
    MEL = unreal.MaterialEditingLibrary
    atools = unreal.AssetToolsHelpers.get_asset_tools()

    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        pkg_path, name = path.rsplit("/", 1)
        factory = unreal.MaterialFunctionMaterialLayerBlendFactory()
        atools.create_asset(name, pkg_path,
                            unreal.MaterialFunctionMaterialLayerBlend, factory)

    mf = unreal.EditorAssetLibrary.load_asset(path)
    mf.set_editor_property("description", description)
    MEL.delete_all_material_expressions_in_function(mf)

    x_in, x_vc, x_blend, x_out = -600, -600, -100, 300

    # FunctionInput 0 — the Background (what's underneath this layer).
    fi_bg = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionFunctionInput, x_in, -120)
    fi_bg.set_editor_property("input_name", "Background")
    fi_bg.set_editor_property("input_type",
                              unreal.FunctionInputType.FUNCTION_INPUT_MATERIAL_ATTRIBUTES)
    fi_bg.set_editor_property("sort_priority", 0)

    # FunctionInput 1 — the Layer (what's being blended on top).
    fi_layer = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionFunctionInput, x_in, 0)
    fi_layer.set_editor_property("input_name", "Layer")
    fi_layer.set_editor_property("input_type",
                                 unreal.FunctionInputType.FUNCTION_INPUT_MATERIAL_ATTRIBUTES)
    fi_layer.set_editor_property("sort_priority", 1)

    # VertexColor node — its <channel> output drives the blend alpha.
    vc = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionVertexColor, x_vc, 160)

    # BlendMaterialAttributes: A / B / Alpha pin-name convention.
    blend = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionBlendMaterialAttributes, x_blend, 0)

    MEL.connect_material_expressions(fi_bg,    "", blend, "A")
    MEL.connect_material_expressions(fi_layer, "", blend, "B")
    MEL.connect_material_expressions(vc, vc_channel, blend, "Alpha")

    fo = MEL.create_material_expression_in_function(
        mf, unreal.MaterialExpressionFunctionOutput, x_out, 0)
    fo.set_editor_property("output_name", "Material Attributes")
    MEL.connect_material_expressions(blend, "", fo, "")

    MEL.update_material_function(mf)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return path


def ensure_layer_stack_assets(rebuild: bool = False) -> Dict[str, str]:
    """Idempotent: ensure the 5 layer-stack MFs exist at their canonical paths.

    MUST be called from its own bridge exec, before ``build()``. Bundling
    all five factory creations with a master-material build risks the
    asset-reference-completing modal deadlock (see feedback_split_asset_ops
    memory + the roadmap's pitfall list).

    Returns the 5 asset paths by role.
    """
    if rebuild:
        for p in (ML_METAL_PATH, ML_FABRIC_PATH, ML_DIRT_PATH,
                  MLB_VC_R_PATH, MLB_VC_G_PATH):
            if unreal.EditorAssetLibrary.does_asset_exist(p):
                unreal.EditorAssetLibrary.delete_asset(p)

    _build_material_layer_mf(ML_METAL_PATH, "Metal", 0.30, 1.00,
                             (0.55, 0.55, 0.58, 1.0),
                             "M3-7 Metal PBR layer (MaterialLayerStacks)")
    _build_material_layer_mf(ML_FABRIC_PATH, "Fabric", 0.85, 0.00,
                             (0.55, 0.25, 0.22, 1.0),
                             "M3-7 Fabric PBR layer (MaterialLayerStacks)")
    _build_material_layer_mf(ML_DIRT_PATH, "Dirt", 0.95, 0.00,
                             (0.20, 0.15, 0.10, 1.0),
                             "M3-7 Dirt overlay layer (MaterialLayerStacks)")

    _build_material_layer_blend_mf(MLB_VC_R_PATH, "R",
                                   "M3-7 blend using VertexColor.R (metal→fabric)")
    _build_material_layer_blend_mf(MLB_VC_G_PATH, "G",
                                   "M3-7 blend using VertexColor.G (dirt overlay)")

    return {
        "ml_metal":  ML_METAL_PATH,
        "ml_fabric": ML_FABRIC_PATH,
        "ml_dirt":   ML_DIRT_PATH,
        "mlb_vc_r":  MLB_VC_R_PATH,
        "mlb_vc_g":  MLB_VC_G_PATH,
    }


def _assert_stack_assets_exist() -> None:
    missing = [p for p in (ML_METAL_PATH, ML_FABRIC_PATH, ML_DIRT_PATH,
                           MLB_VC_R_PATH, MLB_VC_G_PATH)
               if not unreal.EditorAssetLibrary.does_asset_exist(p)]
    if missing:
        raise RuntimeError(
            "Layer-stack MF assets missing: " + ", ".join(missing) + ". "
            "Call material_templates.layered.ensure_layer_stack_assets() in a "
            "separate bridge exec before building M_Layered_Stack.")


def build(master_path: str = "/Game/BridgeTemplates/M_Layered_Stack",
          mi_path: Optional[str] = "/Game/BridgeTemplates/MI_Layered_Stack_Test",
          compile: bool = True,
          rebuild: bool = False) -> Dict[str, Any]:
    """Build M_Layered_Stack with a MaterialAttributeLayers expression wired
    to the 5 layer-stack MFs produced by ``ensure_layer_stack_assets()``.
    """
    L = unreal.UnrealBridgeMaterialLibrary
    MEL = unreal.MaterialEditingLibrary

    _assert_stack_assets_exist()

    # Master — use_material_attributes=True so the output is one MaterialAttributes pin.
    master_path = C.ensure_master_material(
        master_path, "Surface", "DefaultLit", "Opaque",
        two_sided=False, use_material_attributes=True,
        rebuild=rebuild)
    master = unreal.EditorAssetLibrary.load_asset(master_path)

    # Load the 5 MFs so we can reference them in the DefaultLayers struct.
    ml_metal  = unreal.EditorAssetLibrary.load_asset(ML_METAL_PATH)
    ml_fabric = unreal.EditorAssetLibrary.load_asset(ML_FABRIC_PATH)
    ml_dirt   = unreal.EditorAssetLibrary.load_asset(ML_DIRT_PATH)
    mlb_r     = unreal.EditorAssetLibrary.load_asset(MLB_VC_R_PATH)
    mlb_g     = unreal.EditorAssetLibrary.load_asset(MLB_VC_G_PATH)

    # MaterialAttributeLayers expression — create via MEL (same pattern as
    # MaterialFunctionCall in weapon_hero.py) so its ctor runs
    # AddDefaultBackgroundLayer(). Then configure the stack via the bridge's
    # SetMaterialAttributeLayers UFUNCTION, which uses the engine's
    # authoritative FMaterialLayersFunctions::AddDefaultBackgroundLayer +
    # AppendBlendedLayer APIs. Setting default_layers via Python
    # set_editor_property directly crashes UE 5.7: the struct's EditorOnly
    # sub-array invariants (LayerGuids / LayerLinkStates / per-layer parallel
    # arrays) get violated and RebuildLayerGraph + GetID() downstream
    # dereference uninitialized memory.
    mal_pos = (-400, 0)
    MEL.create_material_expression(
        master, unreal.MaterialExpressionMaterialAttributeLayers, *mal_pos)

    # Recover MAL GUID via graph query (same trick as layered + weapon_hero).
    graph = L.get_material_graph(master_path)
    mal_guid_fguid = None
    for n in graph.nodes:
        if str(n.class_name) == "MaterialAttributeLayers" \
                and int(n.x) == mal_pos[0] and int(n.y) == mal_pos[1]:
            mal_guid_fguid = n.guid
            break
    if mal_guid_fguid is None:
        raise RuntimeError(f"could not find MaterialAttributeLayers at {mal_pos}")
    mal_guid_str = C.guid_to_str(mal_guid_fguid)

    # Delegate layer-stack config to the bridge C++ layer. Blends is one
    # shorter than Layers: Blends[i] is the blend MF that composites
    # Layers[i+1] onto the accumulated stack below it.
    stack_r = L.set_material_attribute_layers(
        master_path,
        mal_guid_fguid,
        [ml_metal, ml_fabric, ml_dirt],
        [mlb_r, mlb_g],
        ["Metal (base)", "Fabric (VC.R mask)", "Dirt (VC.G mask)"])
    if not stack_r.success:
        raise RuntimeError(f"set_material_attribute_layers failed: {stack_r.error}")

    # Wire the MAL output into the main MaterialAttributes slot via an ops list
    # — one op, but using OpList keeps the compile+flush pipeline consistent.
    ops = C.OpList()
    ops.add_literal(mal_guid_str, "mal")
    ops.connect_out("mal", "", "MaterialAttributes")

    ops.comment(-520, -180, 300, 360,
                "M_Layered_Stack — MaterialAttributeLayers with 3 layers",
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
            "ml_metal":  ML_METAL_PATH,
            "ml_fabric": ML_FABRIC_PATH,
            "ml_dirt":   ML_DIRT_PATH,
            "mlb_vc_r":  MLB_VC_R_PATH,
            "mlb_vc_g":  MLB_VC_G_PATH,
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

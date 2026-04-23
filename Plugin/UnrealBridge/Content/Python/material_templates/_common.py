"""Shared helpers for material master templates.

- ``OpList``  — name-tracking wrapper around ``apply_material_graph_ops``
  payload. Register each node with a symbolic name, then reference it
  by name on connect / set_prop ops. The list is flushed to the bridge
  as a single ``apply_material_graph_ops`` call.
- Engine default texture paths (safe to use as the ``Texture`` property
  on Texture2DParameter defaults — they ship with every UE install).
- ``collect_stats(path)`` — tiny wrapper that reads back instruction
  count / sampler usage / compile errors after a template build.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional

import unreal


# --- engine-safe default textures ------------------------------------------------

# 1x1 white, flat normal, 1x1 black. Present on every UE install — no need to
# author a texture before a fresh MI previews.
DEFAULT_WHITE_TEX = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
DEFAULT_BLACK_TEX = "/Engine/EngineResources/Black.Black"
DEFAULT_NORMAL_TEX = "/Engine/EngineMaterials/DefaultNormal.DefaultNormal"

# Default preview mesh shipped by the preview scene code.
DEFAULT_PREVIEW_MESH = "sphere"
DEFAULT_PREVIEW_LIGHTING = "studio"


# --- FLinearColor literals used in comments ------------------------------------

COMMENT_COLOR_PBR = unreal.LinearColor(0.25, 0.55, 0.95, 0.40)
COMMENT_COLOR_NORMAL = unreal.LinearColor(0.40, 0.85, 0.55, 0.40)
COMMENT_COLOR_EMISSIVE = unreal.LinearColor(0.95, 0.75, 0.25, 0.40)
COMMENT_COLOR_WEAR = unreal.LinearColor(0.75, 0.55, 0.35, 0.40)
COMMENT_COLOR_WETNESS = unreal.LinearColor(0.35, 0.65, 0.95, 0.35)
COMMENT_COLOR_SWITCH = unreal.LinearColor(0.85, 0.45, 0.85, 0.35)


def _rename_prop_kwarg(kw: Dict[str, Any]) -> Dict[str, Any]:
    """UE's Python renames UPROPERTY 'Property' to Python attr 'property_'."""
    rename = {"property": "property_"}
    return {rename.get(k, k): v for k, v in kw.items()}


def guid_to_str(guid: Any) -> str:
    """UE's default ``str(unreal.Guid)`` prints ``<Struct 'Guid' (0x...) {}>``
    — useless for FGuid::Parse on the C++ side. Use ``.to_string()`` (UE
    exposes it via StructOps), which returns the 32-hex-char digits form
    (``EGuidFormats::Digits``) — FGuid::Parse accepts that directly.
    """
    if isinstance(guid, str):
        return guid
    return str(guid.to_string())


class OpList:
    """Accumulator for graph ops with symbolic names.

    Usage::

        ops = OpList()
        ops.add("bc",   "Constant3Vector", -600, -100)
        ops.setp("bc",  "Constant", "(R=0.8,G=0.4,B=0.2,A=1)")
        ops.connect_out("bc", "", "BaseColor")
        result = ops.flush(path, compile=True)

    Names are resolved to ``$N`` refs when handed to the C++ batcher.
    """

    def __init__(self) -> None:
        self._ops: List[Any] = []
        self._names: Dict[str, str] = {}

    # ---- low-level ------------------------------------------------------

    def _push(self, kind: str, name: Optional[str] = None, **kw: Any) -> str:
        o = unreal.BridgeMaterialGraphOp()
        o.op = kind
        for k, v in _rename_prop_kwarg(kw).items():
            setattr(o, k, v)
        idx = len(self._ops)
        self._ops.append(o)
        ref = f"${idx}"
        if name is not None:
            if name in self._names:
                raise ValueError(f"OpList: duplicate node name '{name}'")
            self._names[name] = ref
        return ref

    def _ref(self, name_or_ref: str) -> str:
        """Resolve a symbolic name to the underlying ref.

        - Known name → the $N or guid string stored in ``add`` / ``add_literal``.
        - Starts with ``$`` → literal back-reference, pass through.
        - Anything else → assumed to be a raw guid string (``FGuid::Parse`` will
          reject malformed ones on the C++ side).
        """
        if name_or_ref in self._names:
            return self._names[name_or_ref]
        if name_or_ref.startswith("$"):
            return name_or_ref
        # Unknown symbolic name — most common cause is a typo in the template.
        # Raise early so the user sees a Python-side error instead of "bad
        # dst_ref" buried in op #N.
        raise KeyError(
            f"OpList: unknown node name '{name_or_ref}' — "
            f"did you forget an ops.add('{name_or_ref}', ...) or "
            f"ops.add_literal(guid, '{name_or_ref}') first?"
        )

    # ---- public ---------------------------------------------------------

    def add(self, name: str, class_name: str, x: int, y: int) -> str:
        return self._push("add", name=name, class_name=class_name, x=x, y=y)

    def add_literal(self, guid_or_ref: str, name: str) -> None:
        """Register an externally-created node (e.g. AddCustomExpression result)
        under a symbolic name so connect/setp ops can refer to it."""
        self._names[name] = guid_or_ref

    def comment(self, x: int, y: int, width: int, height: int,
                text: str, color: Optional[unreal.LinearColor] = None) -> str:
        kw: Dict[str, Any] = dict(x=x, y=y, width=width, height=height, text=text)
        if color is not None:
            kw["color"] = color
        return self._push("comment", **kw)

    def reroute(self, name: str, x: int, y: int) -> str:
        return self._push("reroute", name=name, x=x, y=y)

    def setp(self, target: str, prop: str, value: str) -> None:
        self._push("set_prop", dst_ref=self._ref(target),
                   property=prop, value=value)

    def setps(self, target: str, pairs: Dict[str, str]) -> None:
        for k, v in pairs.items():
            self.setp(target, k, v)

    def connect(self, src: str, src_out: str, dst: str, dst_in: str) -> None:
        self._push("connect",
                   src_ref=self._ref(src), src_output=src_out,
                   dst_ref=self._ref(dst), dst_input=dst_in)

    def connect_out(self, src: str, src_out: str, prop: str) -> None:
        self._push("connect_out",
                   src_ref=self._ref(src), src_output=src_out,
                   property=prop)

    # ---- dispatch -------------------------------------------------------

    def flush(self, material_path: str, compile: bool = True) -> Any:
        """Hand the accumulated ops to ApplyMaterialGraphOps. Returns the
        FBridgeMaterialGraphOpResult."""
        L = unreal.UnrealBridgeMaterialLibrary
        return L.apply_material_graph_ops(material_path, self._ops, compile)

    @property
    def op_count(self) -> int:
        return len(self._ops)


# --- parameter-node convenience -------------------------------------------------

def add_scalar_param(ops: OpList, name: str, x: int, y: int,
                     param_name: str, default: float,
                     group: str = "", sort_priority: int = 0,
                     slider_min: Optional[float] = None,
                     slider_max: Optional[float] = None) -> str:
    """Add a ScalarParameter and set its common properties."""
    ref = ops.add(name, "ScalarParameter", x, y)
    props: Dict[str, str] = {
        "ParameterName": param_name,
        "DefaultValue": f"{default}",
    }
    if group:
        props["Group"] = group
    if sort_priority:
        props["SortPriority"] = str(sort_priority)
    if slider_min is not None:
        props["SliderMin"] = f"{slider_min}"
    if slider_max is not None:
        props["SliderMax"] = f"{slider_max}"
    ops.setps(name, props)
    return ref


def add_vector_param(ops: OpList, name: str, x: int, y: int,
                     param_name: str, default_rgba: str,
                     group: str = "", sort_priority: int = 0) -> str:
    """Add a VectorParameter with a ``(R=..,G=..,B=..,A=..)`` default."""
    ref = ops.add(name, "VectorParameter", x, y)
    props: Dict[str, str] = {
        "ParameterName": param_name,
        "DefaultValue": default_rgba,
    }
    if group:
        props["Group"] = group
    if sort_priority:
        props["SortPriority"] = str(sort_priority)
    ops.setps(name, props)
    return ref


def add_static_switch_param(ops: OpList, name: str, x: int, y: int,
                            param_name: str, default: bool,
                            group: str = "", sort_priority: int = 0) -> str:
    ref = ops.add(name, "StaticSwitchParameter", x, y)
    props: Dict[str, str] = {
        "ParameterName": param_name,
        "DefaultValue": "true" if default else "false",
    }
    if group:
        props["Group"] = group
    if sort_priority:
        props["SortPriority"] = str(sort_priority)
    ops.setps(name, props)
    return ref


_SAMPLER_TYPE_PREFIX = "SAMPLERTYPE_"


def _normalize_sampler_type(s: str) -> str:
    """Accept either 'Color' / 'SAMPLERTYPE_Color' — emit the full enum token.

    ImportText accepts both forms on modern UE, but prefixing is stable across
    engine versions.
    """
    return s if s.startswith(_SAMPLER_TYPE_PREFIX) else _SAMPLER_TYPE_PREFIX + s


def add_texture_param_2d(ops: OpList, name: str, x: int, y: int,
                         param_name: str, default_texture: str,
                         sampler_type: str = "Color",
                         sampler_source: str = "SSM_Wrap_WorldGroupSettings",
                         group: str = "",
                         sort_priority: int = 0) -> str:
    """Add TextureSampleParameter2D with default texture + sampler config.

    ``sampler_type`` is the ``ESamplerType`` tail name — pass ``Color`` /
    ``Normal`` / ``Masks`` / ``Grayscale`` / ``LinearColor`` etc. (either bare
    or with the ``SAMPLERTYPE_`` prefix — normalized here).

    ``sampler_source=SSM_Wrap_WorldGroupSettings`` uses UE's shared wrap
    sampler — drops the per-texture sampler-slot cost to zero. Use
    ``SSM_FromTextureAsset`` to honor the texture's own sampling settings
    (at the cost of one sampler slot per unique texture).
    """
    ref = ops.add(name, "TextureSampleParameter2D", x, y)
    props: Dict[str, str] = {
        "ParameterName": param_name,
        "Texture": default_texture,
        "SamplerType": _normalize_sampler_type(sampler_type),
        "SamplerSource": sampler_source,
    }
    if group:
        props["Group"] = group
    if sort_priority:
        props["SortPriority"] = str(sort_priority)
    ops.setps(name, props)
    return ref


# --- post-build reporting -------------------------------------------------------

def clear_material_graph(material_path: str) -> int:
    """Delete every UMaterialExpression on a master material.

    Use before a template rebuild when the asset already exists — leaves the
    `UMaterial` itself (and any MIs pointing at it) intact so MI overrides
    re-resolve against the freshly-built graph by ParameterName.

    Returns the number of expressions deleted.
    """
    L = unreal.UnrealBridgeMaterialLibrary
    graph = L.get_material_graph(material_path)
    if not graph.found:
        return 0

    deleted = 0
    for node in graph.nodes:
        if L.delete_material_expression(material_path, node.guid):
            deleted += 1
    return deleted


def ensure_master_material(path: str,
                           domain: str = "Surface",
                           shading_model: str = "DefaultLit",
                           blend_mode: str = "Opaque",
                           two_sided: bool = False,
                           use_material_attributes: bool = False,
                           rebuild: bool = False) -> str:
    """Create a master `UMaterial` at `path`, or reuse an existing one.

    - If the asset doesn't exist yet: calls ``create_material``.
    - If it exists and ``rebuild`` is true: wipes all expressions and
      leaves the (now empty) asset in place for the caller to re-populate.
    - If it exists and ``rebuild`` is false: raises — explicit opt-in for
      destructive re-gen avoids silently clobbering hand-edited masters.

    Returns the resolved asset path.
    """
    L = unreal.UnrealBridgeMaterialLibrary
    exists = unreal.EditorAssetLibrary.does_asset_exist(path)

    if not exists:
        r = L.create_material(path, domain, shading_model, blend_mode,
                              two_sided, use_material_attributes)
        if not r.success:
            raise RuntimeError(f"create_material failed: {r.error}")
        return str(r.path) or path

    if not rebuild:
        raise RuntimeError(
            f"master material already exists at '{path}' — pass rebuild=True "
            f"to wipe its expressions and regenerate, or pick a fresh path")

    clear_material_graph(path)
    return path


def save_master(material_path: str) -> bool:
    """Persist the master material to disk after a build.

    ``apply_material_graph_ops(..., compile=True)`` only recompiles the shader
    map; the UMaterial itself sits dirty in memory and vanishes on editor
    restart if never saved. Templates must call this after the flush so the
    asset survives.
    """
    try:
        return bool(unreal.EditorAssetLibrary.save_asset(material_path,
                                                         only_if_is_dirty=False))
    except Exception as e:
        unreal.log_warning(f"save_master({material_path}) failed: {e}")
        return False


def collect_stats(material_path: str,
                  feature_level: str = "SM5",
                  quality: str = "High") -> Dict[str, Any]:
    """Read back instruction count / sampler usage / VT stacks / errors."""
    L = unreal.UnrealBridgeMaterialLibrary
    stats = L.get_material_stats(material_path, feature_level, quality)
    info = L.get_material_info(material_path)

    max_instr = 0
    shader_variants: List[Dict[str, Any]] = []
    for s in stats.shaders:
        shader_variants.append({
            "shader_type": str(s.shader_type),
            "description": str(s.shader_description),
            "instructions": int(s.instruction_count),
        })
        if int(s.instruction_count) > max_instr:
            max_instr = int(s.instruction_count)

    return {
        "path": str(stats.path),
        "feature_level": str(stats.feature_level),
        "quality": str(stats.quality_level),
        "shader_ready": bool(stats.shader_map_ready),
        "max_instructions": max_instr,
        "vt_stack_count": int(stats.virtual_texture_stack_count),
        "compile_errors": [str(e) for e in stats.compile_errors],
        "shaders": shader_variants,
        "num_expressions": int(info.num_expressions),
        "num_function_calls": int(info.num_function_calls),
        "sampler_count": _count_samplers(info),
    }


def _count_samplers(info: Any) -> int:
    """Upper-bound sampler estimate = count of TextureParameter entries.

    Real sampler slot usage depends on SamplerSource sharing; for a budget
    report we want 'distinct texture bindings', which is exactly what
    ``info.texture_parameters`` enumerates.
    """
    try:
        return len(list(info.texture_parameters))
    except Exception:
        return 0


def check_budget(stats: Dict[str, Any], instr: int, sampler: int) -> Dict[str, Any]:
    """Compare collected stats to a template budget."""
    over_instr = stats["max_instructions"] > instr
    over_sampler = stats["sampler_count"] > sampler
    return {
        "instr_budget": instr,
        "sampler_budget": sampler,
        "over_instr": over_instr,
        "over_sampler": over_sampler,
        "compile_clean": len(stats["compile_errors"]) == 0,
        "ok": not (over_instr or over_sampler or stats["compile_errors"]),
    }

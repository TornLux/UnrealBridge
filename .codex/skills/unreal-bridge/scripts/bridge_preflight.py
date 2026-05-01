"""AST preflight for bridge call sites.

Parse a Python script with `ast` and validate every
`unreal.UnrealBridge*Library.foo(...)` call and every
`unreal.BridgeXxx.YYY` enum reference against `bridge_manifest.json`.

Catches without ever round-tripping to UE:
  • Library name doesn't exist
  • Function name doesn't exist on the library (with did-you-mean)
  • Wrong positional arg count
  • Unknown kwarg name (with did-you-mean)
  • Same param given both positionally and by kwarg
  • Enum member that doesn't exist (with valid-members list)

Aliases tracked (3 common forms):
  1. `import unreal [as X]`                     → X.UnrealBridgeAssetLibrary.fn(...)
  2. `from unreal import UnrealBridgeAssetLibrary [as Y]` → Y.fn(...)
  3. `lib = unreal.UnrealBridgeAssetLibrary`    → lib.fn(...)

Same three forms for enums (`unreal.BridgeAssetSearchScope` and aliases of it).

Limitations (deliberate, first iteration):
  • Multi-hop aliases (`a = lib_alias; a.fn(...)`) are NOT tracked — only
    one hop from `unreal.X`. Models virtually never chain renames.
  • Type validation: not done — the manifest doesn't carry param types.
    Catches name/count/kwarg errors, not value-type errors.
"""

from __future__ import annotations

import ast
import difflib
import json
import os
from typing import List, Optional, Set

_DEFAULT_MANIFEST_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "bridge_manifest.json"
)
_DEFAULT_REDIRECTS_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "bridge_redirects.json"
)
_DEFAULT_RETURN_TYPES_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "bridge_return_types.json"
)

_MANIFEST_CACHE: Optional[dict] = None
_REDIRECTS_CACHE: Optional[dict] = None
_RETURN_TYPES_CACHE: Optional[dict] = None


def load_manifest(path: Optional[str] = None) -> Optional[dict]:
    """Read bridge_manifest.json (cached). Returns None if not present."""
    global _MANIFEST_CACHE
    if _MANIFEST_CACHE is not None and path is None:
        return _MANIFEST_CACHE
    p = path or _DEFAULT_MANIFEST_PATH
    if not os.path.isfile(p):
        return None
    try:
        with open(p, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None
    if path is None:
        _MANIFEST_CACHE = data
    return data


def load_redirects(path: Optional[str] = None) -> Optional[dict]:
    """Read bridge_redirects.json (cached). Returns None if not present."""
    global _REDIRECTS_CACHE
    if _REDIRECTS_CACHE is not None and path is None:
        return _REDIRECTS_CACHE
    p = path or _DEFAULT_REDIRECTS_PATH
    if not os.path.isfile(p):
        return None
    try:
        with open(p, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None
    if path is None:
        _REDIRECTS_CACHE = data
    return data


def _augment_return_types_from_manifest(return_types: Optional[dict],
                                          manifest: dict) -> dict:
    """Merge manifest-derived bridge struct schemas + auto-parsed function return
    shapes into the (optional) hand-curated return_types config. Hand entries win.
    """
    rt = dict(return_types) if return_types else {}
    type_attrs = dict(rt.get("type_attributes", {}))
    fn_returns = dict(rt.get("function_returns", {}))

    # Add bridge structs as type_attributes
    for struct_name, fields in manifest.get("structs", {}).items():
        if struct_name not in type_attrs:
            type_attrs[struct_name] = {
                "valid": list(fields),
                "_note": "Bridge USTRUCT (auto-extracted from manifest).",
            }

    # Derive function_returns from each function's `returns` string
    for lib_name, lib_data in manifest.get("libraries", {}).items():
        for fn_name, fn_meta in lib_data.get("functions", {}).items():
            key = f"{lib_name}.{fn_name}"
            if key in fn_returns:
                continue  # hand entry wins
            shape = _parse_return_shape(fn_meta.get("returns", ""))
            if shape:
                fn_returns[key] = shape

    rt["type_attributes"] = type_attrs
    rt["function_returns"] = fn_returns
    return rt


def _parse_return_shape(ret_str: str) -> Optional[dict]:
    """Convert a manifest `returns` string into a function_returns shape dict.

    Recognized forms:
      "Array[X]"                     → list[X]
      "(out_a=Array[X], out_b=...)"  → tuple of element types
      "BridgeXxx" / "SoftObjectPath" → scalar of that type
    Anything else (primitives, void, unknown) returns None — no shape tracked.
    """
    import re
    s = (ret_str or "").strip()
    if not s:
        return None

    # Strip " or None" / " or null" optionality suffix; remember it for the
    # shape-misuse detector ("var.attr" on a may_be_none binding warns).
    may_be_none = False
    new_s = re.sub(r"\s+or\s+(None|null)\s*$", "", s, flags=re.IGNORECASE).strip()
    if new_s != s:
        may_be_none = True
        s = new_s
    if not s or s.lower() in ("none", "null"):
        return None

    # Tuple form
    if s.startswith("(") and s.endswith(")"):
        inner = s[1:-1]
        parts = []
        # Split on commas at top level (avoid splitting inside Array[...])
        depth = 0
        buf = []
        for ch in inner:
            if ch in "[(":
                depth += 1
            elif ch in "])":
                depth -= 1
            if ch == "," and depth == 0:
                parts.append("".join(buf).strip())
                buf = []
            else:
                buf.append(ch)
        if buf:
            parts.append("".join(buf).strip())
        # Each part: strip "name=" prefix
        cleaned = []
        for p in parts:
            if "=" in p:
                p = p.split("=", 1)[1].strip()
            cleaned.append(p)
        return {"shape": "tuple", "tuple_element_types": cleaned, "may_be_none": may_be_none}

    # Array form
    m = re.match(r"(?:Array|List|list)\[([^\]]+)\]\s*$", s)
    if m:
        return {"shape": "list", "element_type": m.group(1).strip(), "may_be_none": may_be_none}

    # Scalar struct (only useful types we have schemas for)
    if s.startswith("Bridge") or s in ("SoftObjectPath", "AssetData"):
        return {"shape": "scalar", "type": s, "may_be_none": may_be_none}

    return None


def load_return_types(path: Optional[str] = None) -> Optional[dict]:
    """Read bridge_return_types.json (cached). Returns None if not present."""
    global _RETURN_TYPES_CACHE
    if _RETURN_TYPES_CACHE is not None and path is None:
        return _RETURN_TYPES_CACHE
    p = path or _DEFAULT_RETURN_TYPES_PATH
    if not os.path.isfile(p):
        return None
    try:
        with open(p, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None
    if path is None:
        _RETURN_TYPES_CACHE = data
    return data


def lint(code: str, manifest: Optional[dict] = None,
         redirects: Optional[dict] = None,
         return_types: Optional[dict] = None) -> "tuple[List[str], List[str]]":
    """Return (errors, warnings). Empty lists = fully clean.

    - errors block the call (preflight rejection, exit 3 in bridge.py)
    - warnings are advisory only (printed to stderr, call still proceeds)

    Each entry is a one-or-multi-line human-readable diagnostic that names the
    line, the offending symbol, and the corrected form.
    """
    if manifest is None:
        manifest = load_manifest()
    if redirects is None:
        redirects = load_redirects()
    if return_types is None:
        return_types = load_return_types()

    if manifest is None:
        return [], []  # No manifest available — silently skip (preflight is best-effort)

    # Auto-augment return_types from manifest:
    #  • Add every Bridge USTRUCT to type_attributes (so attribute access on a
    #    struct binding gets validated against real fields)
    #  • Derive scalar/list/tuple shape per function from its `returns` string,
    #    so the var tracker binds correctly without hand-maintained entries
    return_types = _augment_return_types_from_manifest(return_types, manifest)

    try:
        tree = ast.parse(code)
    except SyntaxError as e:
        return [f"preflight L{e.lineno}: SyntaxError: {e.msg}"], []

    libraries = manifest.get("libraries", {})
    enums = manifest.get("enums", {})

    unreal_aliases, lib_aliases, enum_aliases = _collect_aliases(tree, libraries, enums)

    # Redirects can fire even when no bridge libraries are imported (e.g. a
    # script that ONLY uses raw unreal.AssetRegistry — exactly the case we
    # want to flag). So check redirects first, regardless of bridge imports.
    warnings: List[str] = []
    if redirects:
        warnings.extend(_check_redirects(tree, unreal_aliases, redirects))

    # Attribute-confusion detector: track variables bound to bridge call results
    # and warn on attribute access that doesn't match the type's valid set.
    if return_types and (lib_aliases or unreal_aliases):
        warnings.extend(_check_attribute_confusion(
            tree, unreal_aliases, lib_aliases, return_types,
        ))

    # Cost hints: warn on calls to known-expensive functions without bounds.
    # Hard-coded list driven by audit-log evidence; see _COST_HINTS.
    if lib_aliases or unreal_aliases:
        warnings.extend(_check_cost_hints(tree, unreal_aliases, lib_aliases))

    # Shape-misuse: subscripting a struct, tuple-unpacking a list, chaining on
    # a may_be_none binding. Catches the failure class observed in the
    # SKILL-loaded UDS run (5/5 runtime errors fit this pattern).
    if return_types and (lib_aliases or unreal_aliases):
        warnings.extend(_check_shape_misuse(
            tree, unreal_aliases, lib_aliases, return_types,
        ))

    # Bridge-call validation only runs when bridge libraries are referenced.
    if not (unreal_aliases or lib_aliases or enum_aliases):
        return [], warnings

    errors: List[str] = []
    seen_call_ids = set()  # Avoid double-checking a Call's .func as an enum ref

    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            err = _check_call(node, unreal_aliases, lib_aliases, libraries)
            if err:
                errors.append(err)
            seen_call_ids.add(id(node.func))

    for node in ast.walk(tree):
        if isinstance(node, ast.Attribute) and id(node) not in seen_call_ids:
            err = _check_enum_ref(node, unreal_aliases, enum_aliases, enums)
            if err:
                errors.append(err)

    return errors, warnings


# ── AST helpers ───────────────────────────────────────────────────────────

def _collect_aliases(tree: ast.AST, libraries: dict, enums: dict):
    """Walk the tree to collect three alias maps:

      - unreal_aliases: names that refer to the `unreal` module
                       (`import unreal`, `import unreal as X`)
      - lib_aliases:    name → UnrealBridge*Library name
                       (from `from unreal import LibName [as Y]`,
                        and from `name = unreal.LibName` assignments)
      - enum_aliases:   name → BridgeXxx enum name (same patterns)

    First pass collects unreal_aliases. Second pass uses them to resolve
    `name = <unreal_alias>.LibName` assignments.
    """
    unreal_aliases: Set[str] = set()
    lib_aliases: dict = {}
    enum_aliases: dict = {}

    # Build wrapper → library reverse map: "Asset" → "UnrealBridgeAssetLibrary"
    # so `from unreal_bridge import Asset; Asset.fn(...)` is treated identically
    # to a `lib = unreal.UnrealBridgeAssetLibrary; lib.fn(...)` raw alias chain.
    # Without this mapping, ALL preflight checks (errors, redirects, attribute
    # confusion, cost hints, shape misuse) silently miss wrapper-form calls —
    # which is dominant when SKILL.md is loaded.
    wrapper_to_lib = {}
    for lib_name in libraries.keys():
        short = lib_name
        if short.startswith("UnrealBridge"):
            short = short[len("UnrealBridge"):]
        if short.endswith("Library"):
            short = short[:-len("Library")]
        if short:
            wrapper_to_lib[short] = lib_name

    # Pass 1: import statements (don't depend on prior aliases)
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                if alias.name == "unreal":
                    unreal_aliases.add(alias.asname or "unreal")
        elif isinstance(node, ast.ImportFrom):
            if node.module == "unreal":
                for alias in node.names:
                    bound = alias.asname or alias.name
                    if alias.name in libraries:
                        lib_aliases[bound] = alias.name
                    elif alias.name in enums:
                        enum_aliases[bound] = alias.name
            elif node.module == "unreal_bridge":
                # `from unreal_bridge import Asset [as A]` — bound name → lib_name
                for alias in node.names:
                    bound = alias.asname or alias.name
                    lib_full = wrapper_to_lib.get(alias.name)
                    if lib_full:
                        lib_aliases[bound] = lib_full

    # Pass 2: assignments of the form `X = <unreal_alias>.LibName`
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        if len(node.targets) != 1:
            continue
        target = node.targets[0]
        if not isinstance(target, ast.Name):
            continue
        chain = _attribute_chain(node.value)
        if len(chain) != 2:
            continue
        root, name = chain
        if root not in unreal_aliases:
            continue
        if name in libraries:
            lib_aliases[target.id] = name
        elif name in enums:
            enum_aliases[target.id] = name

    return unreal_aliases, lib_aliases, enum_aliases


def _attribute_chain(node: ast.AST) -> List[str]:
    """For a chain like `unreal.X.Y`, return ['unreal', 'X', 'Y']. Empty list if
    the root isn't a plain Name (e.g. result of a call, subscript, etc)."""
    parts: List[str] = []
    cur = node
    while isinstance(cur, ast.Attribute):
        parts.append(cur.attr)
        cur = cur.value
    if isinstance(cur, ast.Name):
        parts.append(cur.id)
        parts.reverse()
        return parts
    return []


# ── Validators ────────────────────────────────────────────────────────────

def _check_call(node: ast.Call, unreal_aliases: Set[str],
                lib_aliases: dict, libraries: dict) -> str:
    chain = _attribute_chain(node.func)

    # Resolve to (lib_name, fn_name) — accept three forms:
    #   3-part: <unreal_alias>.LibName.fn
    #   2-part: <lib_alias>.fn   (alias from import-from or assignment)
    if len(chain) == 3:
        root, lib, fn = chain
        if root not in unreal_aliases:
            return ""
        if not (lib.startswith("UnrealBridge") and lib.endswith("Library")):
            return ""
    elif len(chain) == 2:
        alias, fn = chain
        lib = lib_aliases.get(alias)
        if lib is None:
            return ""  # Not a tracked bridge-library alias
    else:
        return ""

    if lib not in libraries:
        cand = [k for k in libraries if k.startswith("UnrealBridge")]
        sugg = difflib.get_close_matches(lib, cand, n=2, cutoff=0.6)
        msg = f"preflight L{node.lineno}: '{lib}' is not a known UnrealBridge library."
        if sugg:
            msg += f" Did you mean: {', '.join(sugg)}?"
        return msg

    funcs = libraries[lib].get("functions", {})
    if fn not in funcs:
        sugg = difflib.get_close_matches(fn, list(funcs.keys()), n=3, cutoff=0.5)
        msg = f"preflight L{node.lineno}: {lib}.{fn}(...) - no such function."
        if sugg:
            msg += f" Did you mean: {', '.join(sugg)}?"
        return msg

    fn_meta = funcs[fn]
    params = fn_meta.get("params", [])
    pnames = [p["name"] for p in params]
    n_required = sum(1 for p in params if not p.get("has_default"))
    n_total = len(params)

    n_pos = len(node.args)
    kwargs_used: List[str] = []
    has_splat = False
    for kw in node.keywords:
        if kw.arg is None:
            has_splat = True  # **mapping — can't statically validate
            continue
        kwargs_used.append(kw.arg)

    if has_splat:
        # Best-effort: still check known kwargs and positional count.
        pass

    # 1. Unknown kwarg name
    for kw_name in kwargs_used:
        if kw_name not in pnames:
            sugg = difflib.get_close_matches(kw_name, pnames, n=2, cutoff=0.5)
            msg = (
                f"preflight L{node.lineno}: {lib}.{fn}() got unexpected kwarg "
                f"'{kw_name}'."
            )
            if sugg:
                msg += f" Did you mean: {', '.join(sugg)}?"
            msg += f"\n  signature: {fn}({', '.join(pnames)})"
            return msg

    # 2. Same param given both positionally and by kwarg
    for kw_name in kwargs_used:
        if kw_name in pnames[:n_pos]:
            return (
                f"preflight L{node.lineno}: {lib}.{fn}() got '{kw_name}' both "
                f"positionally (arg {pnames.index(kw_name) + 1}) and as a kwarg."
            )

    # 3. Too many positional args
    if n_pos > n_total:
        return (
            f"preflight L{node.lineno}: {lib}.{fn}() too many positional args: "
            f"takes {n_total}, got {n_pos}.\n"
            f"  signature: {fn}({', '.join(pnames)})"
        )

    # 4. Required args not satisfied (only when no **splat — splat could fill them)
    if not has_splat:
        bound = set(pnames[:n_pos]) | set(kwargs_used)
        missing = [p["name"] for p in params if not p.get("has_default") and p["name"] not in bound]
        if missing:
            return (
                f"preflight L{node.lineno}: {lib}.{fn}() missing required arg(s): "
                f"{', '.join(missing)}.\n"
                f"  signature: {fn}({', '.join(pnames)}) "
                f"({n_required} required of {n_total})"
            )

    return ""


_COST_HINTS = {
    # key: "{LibraryName}.{function}" — same form as preflight uses
    "UnrealBridgeAssetLibrary.list_assets_under_path": {
        "check": "include_subfolders_truthy",
        "param": "include_subfolders",
        "pos": 1,
        "reason": "Recursive walk under a broad path can return 100k+ entries on real projects.",
        "advice": "Restrict folder_path to a sub-tree (e.g. '/Game/MyFeature'), or pass include_subfolders=False.",
    },
    "UnrealBridgeAssetLibrary.list_assets_under_path_simple": {
        "check": "always",
        "reason": "Walks a folder tree without max_results — same scaling cliff as list_assets_under_path.",
        "advice": "Use search_assets_under_path(folder_path, query, max_results) when you can pre-filter.",
    },
    "UnrealBridgeAssetLibrary.search_assets": {
        "check": "max_results_unbounded",
        "param": "max_results",
        "pos": 5,
        "reason": "max_results=0 / -1 / very large = full asset registry walk (10s+ seconds on real projects).",
        "advice": "Pass a bound (max_results=100-1000 typical for agent workflows).",
    },
    "UnrealBridgeAssetLibrary.search_assets_in_all_content": {
        "check": "max_results_unbounded",
        "param": "max_results",
        "pos": 1,
        "reason": "Same as search_assets — empty query + unbounded max walks every asset.",
        "advice": "Bound max_results to 100-1000.",
    },
    "UnrealBridgeAssetLibrary.get_derived_classes": {
        "check": "broad_base_class",
        "param": "base_classes",
        "pos": 0,
        "broad_names": {"Object", "Actor", "ActorComponent", "Component", "Pawn", "SceneComponent"},
        "reason": "Walking from Object / Actor / Component / Pawn yields thousands of classes (multi-second GT block).",
        "advice": "Use the most specific base you can. e.g. ACharacter, UStaticMeshComponent, AVolume.",
    },
    "UnrealBridgeLevelLibrary.find_actors_by_class": {
        "check": "max_results_unbounded",
        "param": "max_results",
        "pos": 1,
        "reason": "max_results=-1 is unbounded; open-world / WP maps have 10k+ actors.",
        "advice": "Set a bound (max_results=50-500).",
    },
    "UnrealBridgeBlueprintLibrary.search_blueprint_nodes": {
        "check": "always",
        "reason": "Whole-graph node search; large BPs (12k+ nodes) take seconds.",
        "advice": "Constrain via node_type_filter or scope to a single function/event graph when possible.",
    },
}


def _check_cost_hints(tree: ast.AST, unreal_aliases: Set[str],
                      lib_aliases: dict) -> List[str]:
    """Detect calls to known-expensive functions without bounds and emit warnings.
    All entries are HARD-CODED for now (see _COST_HINTS); audit-log evidence is
    used to decide which functions to add. Easy to extend: one entry per call site
    we observe blowing up in real workflows."""
    warnings: List[str] = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        chain = _attribute_chain(node.func)
        # Resolve {Library}.{fn} key from either 3-part chain or alias 2-part
        if len(chain) == 3 and chain[0] in unreal_aliases:
            lib, fn = chain[1], chain[2]
        elif len(chain) == 2:
            lib = lib_aliases.get(chain[0])
            if lib is None:
                continue
            fn = chain[1]
        else:
            continue
        key = f"{lib}.{fn}"
        hint = _COST_HINTS.get(key)
        if not hint:
            continue
        if not _cost_hint_triggers(node, hint):
            continue
        warnings.append(
            f"preflight L{node.lineno} [WARN]: high-cost call '{key}'.\n"
            f"  Why:    {hint.get('reason', '')}\n"
            f"  Advice: {hint.get('advice', '')}"
        )
    return warnings


def _cost_hint_triggers(node: ast.Call, hint: dict) -> bool:
    """Decide whether this Call node fires the given cost hint."""
    check = hint.get("check")
    if check == "always":
        return True

    if check == "include_subfolders_truthy":
        v = _arg_value(node, hint["param"], hint["pos"])
        # Default-on if the user didn't supply (function has 2 required positional)
        if v is None:
            return True
        return _is_truthy_const(v)

    if check == "max_results_unbounded":
        v = _arg_value(node, hint["param"], hint["pos"])
        if v is None:
            return True  # required arg missing → preflight will error separately, but warn too
        c = _const_value(v)
        if c is None:
            return False  # non-const expression — can't reason; skip
        if isinstance(c, (int, float)) and (c <= 0 or c >= 100000):
            return True
        return False

    if check == "broad_base_class":
        v = _arg_value(node, hint["param"], hint["pos"])
        if v is None:
            return False
        broad = hint.get("broad_names", set())
        return _ast_uses_class_name(v, broad)

    return False


def _arg_value(node: ast.Call, name: str, pos: int) -> Optional[ast.AST]:
    """Get the AST node for arg `name` (positional-or-kwarg). Returns None if absent."""
    if pos < len(node.args):
        return node.args[pos]
    for kw in node.keywords:
        if kw.arg == name:
            return kw.value
    return None


def _const_value(node: Optional[ast.AST]):
    """Evaluate a Constant or UnaryOp(USub, Constant) node's literal value."""
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        if isinstance(node.operand, ast.Constant) and isinstance(node.operand.value, (int, float)):
            return -node.operand.value
    return None


def _is_truthy_const(node: Optional[ast.AST]) -> bool:
    c = _const_value(node)
    if c is None:
        return False
    return bool(c)


def _ast_uses_class_name(node: ast.AST, names: set) -> bool:
    """Recursively check if any leaf identifier or string in node matches names.
    Catches `[unreal.Object.static_class()]`, `unreal.AActor`, `'AActor'` etc."""
    for sub in ast.walk(node):
        if isinstance(sub, ast.Attribute) and sub.attr in names:
            return True
        if isinstance(sub, ast.Name) and sub.id in names:
            return True
        if isinstance(sub, ast.Constant) and isinstance(sub.value, str) and sub.value in names:
            return True
    return False


def _check_redirects(tree: ast.AST, unreal_aliases: Set[str], redirects: dict) -> List[str]:
    """Detect known raw-unreal.* fallback patterns and emit redirect warnings.

    Two pattern types in redirects.json:
      • direct_call: `unreal.X.Y(...)` — 3-part chain on a Call's .func
      • asset_registry_method: `<var>.method(...)` where var = unreal.AssetRegistryHelpers
                               .get_asset_registry(). Also matches static-form
                               `unreal.AssetRegistryHelpers.method(...)`.
    """
    if not unreal_aliases:
        return []

    entries = redirects.get("redirects", [])
    if not entries:
        return []

    # Collect variables assigned `unreal.AssetRegistryHelpers.get_asset_registry()`.
    ar_aliases = _collect_asset_registry_aliases(tree, unreal_aliases)

    # Build quick-lookup maps:
    #   direct_calls:           "unreal.X.Y" → entry
    #   asset_registry_methods: "method_name" → entry
    direct_calls = {}
    ar_methods = {}
    for e in entries:
        mt = e.get("match_type")
        if mt == "direct_call":
            pat = e.get("raw_pattern", "")
            if pat:
                direct_calls[pat] = e
        elif mt == "asset_registry_method":
            method = e.get("method", "")
            if method:
                ar_methods[method] = e

    warnings = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        chain = _attribute_chain(node.func)

        # Pattern A: direct unreal.X.Y(...) call
        if len(chain) == 3 and chain[0] in unreal_aliases:
            key = f"unreal.{chain[1]}.{chain[2]}"
            entry = direct_calls.get(key)
            if entry:
                warnings.append(_format_redirect(node.lineno, key, entry))
                continue

        # Pattern B: <ar_alias>.method(...) where ar_alias is a registry handle
        if len(chain) == 2 and chain[0] in ar_aliases:
            entry = ar_methods.get(chain[1])
            if entry:
                seen = f"{chain[0]}.{chain[1]} (asset registry handle)"
                warnings.append(_format_redirect(node.lineno, seen, entry))
                continue

        # Pattern C: static-form unreal.AssetRegistryHelpers.method(...)
        if (len(chain) == 3 and chain[0] in unreal_aliases
                and chain[1] == "AssetRegistryHelpers"):
            entry = ar_methods.get(chain[2])
            if entry:
                seen = f"unreal.AssetRegistryHelpers.{chain[2]}"
                warnings.append(_format_redirect(node.lineno, seen, entry))
                continue

    return warnings


def _collect_asset_registry_aliases(tree: ast.AST, unreal_aliases: Set[str]) -> Set[str]:
    """Find names assigned the result of unreal.AssetRegistryHelpers.get_asset_registry()."""
    aliases: Set[str] = set()
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        target = node.targets[0]
        if not isinstance(target, ast.Name):
            continue
        if not isinstance(node.value, ast.Call):
            continue
        chain = _attribute_chain(node.value.func)
        if (len(chain) == 3
                and chain[0] in unreal_aliases
                and chain[1] == "AssetRegistryHelpers"
                and chain[2] == "get_asset_registry"):
            aliases.add(target.id)
    return aliases


def _format_redirect(line: int, found: str, entry: dict) -> str:
    """Render one redirect warning."""
    bridge = entry.get("bridge_replacement", "<no replacement defined>")
    reason = entry.get("reason", "")
    indented = "\n    ".join(bridge.splitlines())
    msg = (
        f"preflight L{line} [WARN]: raw fallback detected.\n"
        f"  Found:   {found}(...)\n"
        f"  Use:     {indented}"
    )
    if reason:
        msg += f"\n  Why:     {reason}"
    return msg


def _check_shape_misuse(tree: ast.AST, unreal_aliases: Set[str],
                          lib_aliases: dict, return_types: dict) -> List[str]:
    """Detect three return-shape misuse patterns observed in the SKILL-loaded run:

      A) `a, b = func()` when func returns list/scalar (not tuple) — wrong arity
      B) `var[N]` subscript on a scalar struct binding — struct isn't sliceable
      C) `var.attr` on a may_be_none binding — chains crash if func returned None

    Each pattern emits a [WARN] with the failing call's known return shape and a
    concrete fix. Detection is conservative (false negatives possible if return
    shape isn't in manifest); false positives possible on (C) when the agent
    DID guard with `if var is not None`, but warning-only is fine — agent ignores
    if they're sure it's safe.
    """
    fn_returns = return_types.get("function_returns", {})
    warnings: List[str] = []

    def _resolve_call_to_function_key(call: ast.Call):
        chain = _attribute_chain(call.func)
        if len(chain) == 3 and chain[0] in unreal_aliases:
            return f"{chain[1]}.{chain[2]}"
        if len(chain) == 2:
            lib = lib_aliases.get(chain[0])
            if lib:
                return f"{lib}.{chain[1]}"
        return None

    # Pass 1: track scalar / list / tuple bindings + may_be_none flag
    var_meta: dict = {}  # name → dict(shape_kind, type_or_elements, may_be_none, fn_key)

    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if not isinstance(node.value, ast.Call):
                continue
            fn_key = _resolve_call_to_function_key(node.value)
            if not fn_key or fn_key not in fn_returns:
                continue
            shape = fn_returns[fn_key]
            kind = shape.get("shape")
            may_be_none = bool(shape.get("may_be_none"))

            # ── Pattern A: tuple-unpack arity check ──
            if isinstance(target, (ast.Tuple, ast.List)):
                target_n = len(target.elts)
                if kind == "list":
                    warnings.append(
                        f"preflight L{node.lineno} [WARN]: '{fn_key}' returns "
                        f"Array[{shape.get('element_type','?')}], not a tuple — "
                        f"`a, b = ...` will iterate the list and fail unless its "
                        f"length is exactly {target_n}.\n"
                        f"  Use:    var = {fn_key.split('.')[-1]}(...); for x in var: ...\n"
                        f"  Or:     [a, b] = ... only if you've verified len == {target_n}"
                    )
                elif kind == "scalar":
                    warnings.append(
                        f"preflight L{node.lineno} [WARN]: '{fn_key}' returns a single "
                        f"{shape.get('type','?')} struct, not a tuple — destructuring "
                        f"`a, b = ...` will fail.\n"
                        f"  Use: var = {fn_key.split('.')[-1]}(...); var.field_name"
                    )
                elif kind == "tuple":
                    expected = len(shape.get("tuple_element_types", []))
                    if expected and target_n != expected:
                        warnings.append(
                            f"preflight L{node.lineno} [WARN]: '{fn_key}' returns a "
                            f"{expected}-tuple but you unpack into {target_n} names."
                        )
                continue

            # Single-name binding
            if not isinstance(target, ast.Name):
                continue
            var_meta[target.id] = {
                "kind": kind,
                "shape": shape,
                "may_be_none": may_be_none,
                "fn_key": fn_key,
            }

    if not var_meta:
        return warnings

    # Pass 2: walk all uses of tracked vars; check Subscript and may_be_none chains
    seen = set()
    for node in ast.walk(tree):
        # ── Pattern B: subscript on a scalar struct ──
        if isinstance(node, ast.Subscript) and isinstance(node.value, ast.Name):
            meta = var_meta.get(node.value.id)
            if meta and meta["kind"] == "scalar":
                key = ("subscript", node.lineno, node.value.id)
                if key in seen:
                    continue
                seen.add(key)
                type_str = meta["shape"].get("type", "?")
                warnings.append(
                    f"preflight L{node.lineno} [WARN]: '{node.value.id}' is a "
                    f"{type_str} struct (from {meta['fn_key']}), not subscriptable.\n"
                    f"  Slicing `{node.value.id}[...]` will fail. Access fields by "
                    f"name: {node.value.id}.<field_name>"
                )
            continue

        # ── Pattern C: attribute chain on may_be_none binding ──
        if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name):
            meta = var_meta.get(node.value.id)
            if meta and meta["may_be_none"]:
                key = ("none", node.lineno, node.value.id, node.attr)
                if key in seen:
                    continue
                seen.add(key)
                warnings.append(
                    f"preflight L{node.lineno} [WARN]: '{node.value.id}' may be None "
                    f"({meta['fn_key']} returns 'X or None'). Accessing .{node.attr} "
                    f"will crash on the None case.\n"
                    f"  Guard: if {node.value.id} is not None: ... or "
                    f"`if {node.value.id}:` before the access."
                )
            continue

    return warnings


def _check_attribute_confusion(tree: ast.AST, unreal_aliases: Set[str],
                                lib_aliases: dict, return_types: dict) -> List[str]:
    """Track variables bound to bridge call results, then warn on attribute
    access that the bound type doesn't actually support.

    Catches the high-frequency confusion where the agent assumes a bridge
    function returned a UE object (e.g. AssetData) when it actually returned
    a string or SoftObjectPath.

    Tracking covers three Python binding patterns:
      • `var = lib.fn(...)`              — direct assignment
      • `a, b = lib.fn(...)`             — tuple unpacking (matches tuple shape)
      • `for x in <tracked_var>:`        — iter binds element type

    Detection is conservative: only flags attributes that are clearly wrong
    for the bound type (`str.<UE_attr>` always; SoftObjectPath access against
    a small valid-attr whitelist).
    """
    fn_returns = return_types.get("function_returns", {})
    type_attrs = return_types.get("type_attributes", {})
    str_builtins = set(return_types.get("string_builtin_methods", []))

    # var_types: name → element-type string (e.g. "str", "SoftObjectPath")
    var_types: dict = {}

    def _resolve_call_to_function_key(call: ast.Call) -> Optional[str]:
        """Map a Call node to a 'UnrealBridgeXxxLibrary.fn' key, or None."""
        chain = _attribute_chain(call.func)
        if len(chain) == 3 and chain[0] in unreal_aliases:
            return f"{chain[1]}.{chain[2]}"
        if len(chain) == 2:
            lib = lib_aliases.get(chain[0])
            if lib:
                return f"{lib}.{chain[1]}"
        return None

    # Pass 1: walk all assignments + for-loops, record bindings.
    for node in ast.walk(tree):
        # `var = bridge_call()`  or  `a, b = bridge_call()`
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            value = node.value
            if not isinstance(value, ast.Call):
                continue
            fn_key = _resolve_call_to_function_key(value)
            if not fn_key or fn_key not in fn_returns:
                continue
            shape = fn_returns[fn_key]

            if isinstance(target, ast.Name):
                # var = bridge_call() — bind to whole return shape
                if shape.get("shape") == "list":
                    # var is a list; iterating it yields element_type
                    var_types[target.id] = ("list", shape.get("element_type"))
                elif shape.get("shape") == "tuple":
                    # var is a tuple — agent will index/unpack it
                    var_types[target.id] = ("tuple", shape.get("tuple_element_types", []))
                elif shape.get("shape") == "scalar":
                    # var is a single struct (e.g. BridgeBlueprintSummary) —
                    # attribute access goes through type_attributes validation
                    var_types[target.id] = ("scalar", shape.get("type"))
            elif isinstance(target, (ast.Tuple, ast.List)):
                # a, b = bridge_call() — each name binds to one tuple element
                if shape.get("shape") == "tuple":
                    elem_types = shape.get("tuple_element_types", [])
                    for i, sub in enumerate(target.elts):
                        if isinstance(sub, ast.Name) and i < len(elem_types):
                            etype_str = elem_types[i]
                            # "list[X]" → bind sub.id as list of X
                            inner = _parse_list_inner(etype_str)
                            if inner:
                                var_types[sub.id] = ("list", inner)
                            else:
                                var_types[sub.id] = ("scalar", etype_str)

        # `for x in <tracked_var>:`  — single iter binds element type
        elif isinstance(node, ast.For):
            # Single var, single tracked iter
            if isinstance(node.target, ast.Name) and isinstance(node.iter, ast.Name):
                bound = var_types.get(node.iter.id)
                if bound and bound[0] == "list":
                    var_types[node.target.id] = ("scalar", bound[1])
                continue

            # `for a, b, ... in zip(t1, t2, ...):` — pair each name with its iterable's element type
            if (isinstance(node.target, ast.Tuple)
                    and isinstance(node.iter, ast.Call)
                    and isinstance(node.iter.func, ast.Name)
                    and node.iter.func.id == "zip"):
                for tgt_idx, sub in enumerate(node.target.elts):
                    if not isinstance(sub, ast.Name):
                        continue
                    if tgt_idx >= len(node.iter.args):
                        continue
                    arg = node.iter.args[tgt_idx]
                    if not isinstance(arg, ast.Name):
                        continue
                    bound = var_types.get(arg.id)
                    if bound and bound[0] == "list":
                        var_types[sub.id] = ("scalar", bound[1])
                continue

            # `for i, x in enumerate(<tracked_var>):` — second name binds element type
            if (isinstance(node.target, ast.Tuple)
                    and len(node.target.elts) == 2
                    and isinstance(node.iter, ast.Call)
                    and isinstance(node.iter.func, ast.Name)
                    and node.iter.func.id == "enumerate"
                    and node.iter.args
                    and isinstance(node.iter.args[0], ast.Name)):
                second = node.target.elts[1]
                if isinstance(second, ast.Name):
                    bound = var_types.get(node.iter.args[0].id)
                    if bound and bound[0] == "list":
                        var_types[second.id] = ("scalar", bound[1])

    if not var_types:
        return []

    # Pass 2: walk all Attribute accesses, check against bound types.
    warnings: List[str] = []
    seen = set()  # dedupe identical (line, var, attr) reports
    for node in ast.walk(tree):
        if not isinstance(node, ast.Attribute):
            continue
        if not isinstance(node.value, ast.Name):
            continue
        var = node.value.id
        attr = node.attr
        bound = var_types.get(var)
        if not bound:
            continue

        kind, type_str = bound
        # Only check scalar (single-element) bindings; for list/tuple, attribute
        # access is normal Python (.append, .__len__) and not the failure mode.
        if kind != "scalar":
            continue
        if not type_str:
            continue

        # Type-specific validation
        warning = None
        if type_str == "str":
            # str only — UE attributes are always wrong; skip Python builtins
            if attr in str_builtins or attr.startswith("_"):
                continue
            warning = (
                f"preflight L{node.lineno} [WARN]: '{var}' is a str (from a bridge call "
                f"that returns paths), but you accessed .{attr}.\n"
                f"  Bridge functions in the Asset/Level libraries return string paths, "
                f"not UE objects. To get object metadata, call a dedicated bridge\n"
                f"  introspection function (e.g. Asset.get_asset_class_path(asset_path={var}))\n"
                f"  rather than indexing into the path itself."
            )
        else:
            type_meta = type_attrs.get(type_str)
            if type_meta is None:
                continue
            valid = set(type_meta.get("valid", []))
            if attr in valid or attr.startswith("_"):
                continue
            note = type_meta.get('_note', '').strip()
            warning = (
                f"preflight L{node.lineno} [WARN]: '{var}' is a {type_str} but you "
                f"accessed .{attr}, which doesn't exist on {type_str}.\n"
                f"  Valid {type_str} attrs: {sorted(valid) or '(none)'}."
            )
            if note:
                warning += f"\n  Note: {note}"

        key = (node.lineno, var, attr)
        if warning and key not in seen:
            seen.add(key)
            warnings.append(warning)

    return warnings


def _parse_list_inner(type_str: str) -> Optional[str]:
    """Parse 'list[X]' or 'List[X]' or 'Array[X]' → 'X', else None."""
    import re
    m = re.match(r"(?:list|List|Array)\[([^\]]+)\]\s*$", (type_str or "").strip())
    return m.group(1).strip() if m else None


def _check_enum_ref(node: ast.Attribute, unreal_aliases: Set[str],
                    enum_aliases: dict, enums: dict) -> str:
    chain = _attribute_chain(node)

    # Accept two forms:
    #   3-part: <unreal_alias>.BridgeXxx.MEMBER
    #   2-part: <enum_alias>.MEMBER   (alias from import-from or assignment)
    if len(chain) == 3:
        root, enum_name, member = chain
        if root not in unreal_aliases:
            return ""
        if enum_name not in enums:
            return ""
    elif len(chain) == 2:
        alias, member = chain
        enum_name = enum_aliases.get(alias)
        if enum_name is None:
            return ""
    else:
        return ""

    members = enums.get(enum_name, [])
    if member in members:
        return ""
    sugg = difflib.get_close_matches(member, members, n=2, cutoff=0.5)
    msg = f"preflight L{node.lineno}: {enum_name}.{member} is not a valid enum member."
    if sugg:
        msg += f" Did you mean: {', '.join(sugg)}?"
    msg += f"\n  valid: {', '.join(members)}"
    return msg


# ── CLI driver: standalone linting of a script file ───────────────────────

def _cli() -> int:
    import argparse
    import sys

    parser = argparse.ArgumentParser(
        description="Lint a Python script for bridge-call errors without executing it."
    )
    parser.add_argument("file", help="Path to script (or `-` to read from stdin).")
    parser.add_argument(
        "--manifest",
        help=f"Manifest path (default: {_DEFAULT_MANIFEST_PATH}).",
    )
    args = parser.parse_args()

    if args.file == "-":
        code = sys.stdin.read()
    else:
        try:
            with open(args.file, encoding="utf-8") as f:
                code = f.read()
        except OSError as e:
            print(f"ERROR: cannot read {args.file}: {e}", file=sys.stderr)
            return 2

    manifest = load_manifest(args.manifest)
    if manifest is None:
        print(f"WARN: no manifest at {args.manifest or _DEFAULT_MANIFEST_PATH} — preflight skipped",
              file=sys.stderr)
        return 0

    errs, warns = lint(code, manifest)
    for w in warns:
        print(w, file=sys.stderr)
    if not errs:
        if not warns:
            print("preflight: clean")
        else:
            print(f"preflight: clean (with {len(warns)} warning(s))")
        return 0
    for e in errs:
        print(e, file=sys.stderr)
    return 1


if __name__ == "__main__":
    import sys
    sys.exit(_cli())

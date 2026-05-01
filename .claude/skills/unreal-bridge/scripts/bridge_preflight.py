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

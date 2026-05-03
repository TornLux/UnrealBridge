#!/usr/bin/env python3
"""Add Category="UnrealBridge|<area>" to UPROPERTY/UFUNCTION declarations in
plugin public headers that are exposed to Blueprints or the editor but lack a
Category specifier.

Required by `RunUAT.bat BuildPlugin -Rocket`, which packages the plugin as an
"Engine module" — UHT then enforces explicit Category on every reflected
property/function that's BP/editor-exposed.

Usage:
    python tools/add_categories.py             # apply edits
    python tools/add_categories.py --check     # dry run; non-zero exit if any change needed
    python tools/add_categories.py --diff      # show per-file change counts and target Category
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PUBLIC = REPO_ROOT / "Plugin" / "UnrealBridge" / "Source" / "UnrealBridge" / "Public"

EXPOSING = re.compile(
    r"\b(?:"
    r"Blueprint(?:ReadOnly|ReadWrite|Callable|Pure|NativeEvent|ImplementableEvent|"
    r"Assignable|AuthorityOnly|Getter|Setter)|"
    r"Edit(?:Anywhere|DefaultsOnly|InstanceOnly|FixedSize|InlineNew)|"
    r"Visible(?:Anywhere|DefaultsOnly|InstanceOnly)|"
    r"Exec"
    r")\b"
)

CATEGORY_PRESENT = re.compile(r"\bCategory\s*=", re.IGNORECASE)

# UPROPERTY / UFUNCTION declaration with one level of nested parens
# (covers `meta=(DisplayName="...", AdvancedDisplay="A,B")` style).
DECL = re.compile(r"\b(UPROPERTY|UFUNCTION)\(((?:[^()]|\([^()]*\))*)\)")

# Per-file-stem overrides where the derived name doesn't match the
# convention already used in existing UFUNCTIONs.
OVERRIDE: dict[str, str] = {
    "UnrealBridgeAnimLibrary": "UnrealBridge|Animation",
    "UnrealBridgeReactiveTypes": "UnrealBridge|Reactive",
    "UnrealBridgeReactiveAdapter": "UnrealBridge|Reactive",
    "UnrealBridgeReactiveListeners": "UnrealBridge|Reactive",
    "UnrealBridgeReactiveSubsystem": "UnrealBridge|Reactive",
    "UnrealBridgeTestAttributeSet": "BridgeTest",
}


def category_for(stem: str) -> str:
    if stem in OVERRIDE:
        return OVERRIDE[stem]
    s = stem
    if s.startswith("UnrealBridge"):
        s = s[len("UnrealBridge"):]
    if s.endswith("Library"):
        s = s[: -len("Library")]
    if not s:
        s = "Bridge"
    return f"UnrealBridge|{s}"


def transform_text(text: str, category: str) -> tuple[str, int]:
    changes = 0

    def repl(m: re.Match) -> str:
        nonlocal changes
        kind, meta = m.group(1), m.group(2)
        if not EXPOSING.search(meta):
            return m.group(0)
        if CATEGORY_PRESENT.search(meta):
            return m.group(0)
        meta_stripped = meta.strip()
        sep = ", " if meta_stripped else ""
        new_meta = f"{meta_stripped}{sep}Category = \"{category}\""
        changes += 1
        return f"{kind}({new_meta})"

    return DECL.sub(repl, text), changes


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="dry run; exit 1 if changes needed")
    ap.add_argument("--diff", action="store_true", help="print per-file change counts and target Category")
    args = ap.parse_args()

    files = sorted(PUBLIC.glob("*.h"))
    total_changes = 0
    edited = 0
    for f in files:
        text = f.read_text(encoding="utf-8")
        cat = category_for(f.stem)
        new_text, n = transform_text(text, cat)
        if n == 0:
            continue
        total_changes += n
        edited += 1
        rel = f.relative_to(REPO_ROOT)
        print(f"  {rel}  +{n}  ({cat})")
        if not args.check:
            f.write_text(new_text, encoding="utf-8")

    print(f"\n{total_changes} declarations updated across {edited} files")
    if args.check and total_changes:
        print("(--check: changes would be needed)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

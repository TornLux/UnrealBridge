"""Material master-template generators (M3).

Each submodule exposes a ``build(...)`` entry point that assembles one
AAA-aligned master material with sensible parameter defaults, optional
static-switch feature flags, a compile pass, and (optionally) a test
Material Instance + preview render.

Invocation is always via the bridge from the host side::

    python bridge.py exec-file -c "
    from material_templates import character_armor
    r = character_armor.build()
    print(r)
    "

All template builders assume they run inside the UE editor Python
environment — they import ``unreal`` directly and call the
``UnrealBridgeMaterialLibrary`` M2/M2.5 primitives.
"""

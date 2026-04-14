# GameplayAbilitySystem API

`unreal.UnrealBridgeGameplayAbilityLibrary` — introspection for the GameplayAbilitySystem plugin (abilities, effects, attribute sets, ability system components, gameplay tags).

All calls return empty structs / empty lists on failure (bad path, wrong class, missing actor) — no exceptions.

---

## GameplayAbility Blueprint Info

### get_gameplay_ability_blueprint_info(ability_blueprint_path) -> FBridgeGameplayAbilityInfo

Read metadata from a `UGameplayAbility` Blueprint's CDO.

```python
info = unreal.UnrealBridgeGameplayAbilityLibrary.get_gameplay_ability_blueprint_info(
    '/Game/Abilities/GA_Jump')
print(info.name, info.parent_class_name)
print('Instancing:', info.instancing_policy)
print('NetExecution:', info.net_execution_policy)
print('Tags:', list(info.ability_tags))
print('Cost GE:', info.cost_gameplay_effect_class)
print('Cooldown GE:', info.cooldown_gameplay_effect_class)
```

### FBridgeGameplayAbilityInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Generated class name |
| `parent_class_name` | str | Super class name |
| `instancing_policy` | str | `InstancedPerActor` / `InstancedPerExecution` / `NonInstanced` |
| `net_execution_policy` | str | `LocalPredicted` / `ServerInitiated` / `ServerOnly` / `LocalOnly` |
| `ability_tags` | list[str] | AssetTags on the CDO |
| `cost_gameplay_effect_class` | str | Path to the cost GE class, if any |
| `cooldown_gameplay_effect_class` | str | Path to the cooldown GE class, if any |

---

## GameplayEffect Blueprint Info

### get_gameplay_effect_blueprint_info(effect_blueprint_path) -> FBridgeGameplayEffectInfo

Read metadata from a `UGameplayEffect` Blueprint's CDO: duration/period policy, modifiers, stacking, and attached `UGameplayEffectComponent` entries (UE5.3+ tag containers live here).

> ⚠️ **Token cost: LOW–MEDIUM.** Output size scales with modifier count and number of GEComponents (each can emit many tag strings). A complex buff with 10+ modifiers and several tag-inheriting components can cost a few hundred tokens. Fine for single-asset inspection; don't loop over a large catalogue without a filter.

Duration/period magnitude extraction is best-effort: only constant `ScalableFloat` values resolve to numeric output; dynamic curves / `SetByCaller` / `AttributeBased` report via `magnitude_source` with no number.

```python
info = unreal.UnrealBridgeGameplayAbilityLibrary.get_gameplay_effect_blueprint_info(
    '/Game/Effects/GE_DamageOverTime')
print(info.name, info.duration_policy, info.duration_seconds, info.period_seconds)
for m in info.modifiers:
    print(f'  {m.attribute} {m.mod_op} {m.magnitude} [{m.magnitude_source}]')
for c in info.components:
    print(f'  Component: {c.class_name}')
    for t in c.tags:
        print(f'    {t}')
```

### FBridgeGameplayEffectInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Generated class name |
| `parent_class_name` | str | Super class name |
| `duration_policy` | str | `Instant` / `HasDuration` / `Infinite` |
| `duration_seconds` | float | Constant duration, or -1 when non-constant / infinite / instant |
| `period_seconds` | float | Constant period, 0 when non-periodic |
| `modifiers` | list[FBridgeGEModifierInfo] | Modifier entries |
| `stacking_type` | str | `None` / `AggregateBySource` / `AggregateByTarget` |
| `stack_limit_count` | int | Stack cap |
| `components` | list[FBridgeGEComponentInfo] | Attached `UGameplayEffectComponent`s |

### FBridgeGEModifierInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `attribute` | str | e.g. `Health.Damage` |
| `mod_op` | str | `Additive` / `Multiplicitive` / `Division` / `Override` |
| `magnitude` | float | Constant magnitude (0 when non-constant) |
| `magnitude_source` | str | `ScalableFloat` / `AttributeBased` / `CustomMagnitude` / `SetByCaller` |

### FBridgeGEComponentInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `class_name` | str | Component class name |
| `tags` | list[str] | Flat `"PropertyName: Tag"` entries for every `FGameplayTagContainer` / `FInheritedTagContainer` discovered via reflection |

---

## AttributeSet Info

### get_attribute_set_info(attribute_set_class_path) -> FBridgeAttributeSetInfo

Read an `UAttributeSet` class and its `FGameplayAttributeData` fields. Accepts:
- native class path: `/Script/MyModule.MyAttributeSet`
- BP asset path: `/Game/AS/BP_MyAttributeSet`

```python
info = unreal.UnrealBridgeGameplayAbilityLibrary.get_attribute_set_info(
    '/Script/MyGame.MyAttributeSet')
print(info.name, info.parent_class_name)
for a in info.attributes:
    print(f'  {a.name} ({a.type}) base={a.base_value}')
```

### FBridgeAttributeSetInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Class name |
| `parent_class_name` | str | Super class name |
| `attributes` | list[FBridgeAttributeInfo] | Reflected attribute fields |

### FBridgeAttributeInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | str | Attribute field name |
| `type` | str | Struct type (usually `GameplayAttributeData`) |
| `base_value` | float | CDO base value |

---

## List Abilities / Effects by Tag

### list_abilities_by_tag(tag_query, max_results) -> list[str]
### list_gameplay_effects_by_tag(tag_query, max_results) -> list[str]

Scan the AssetRegistry for ability / effect Blueprints whose AssetTags contain a tag matching `tag_query` (via `FGameplayTag::MatchesTag`, so parent queries match children). Returns asset paths.

> ⚠️ **Token cost: MEDIUM on large projects.** Walks every ability/effect Blueprint in the registry, loads each CDO to read tags, and returns every match. Pass `max_results > 0` to cap output — `0` means unlimited and can flood context on content-heavy projects. Use the most specific tag you can.

```python
paths = unreal.UnrealBridgeGameplayAbilityLibrary.list_abilities_by_tag('Ability.Combat', 50)
for p in paths:
    print(p)

ge_paths = unreal.UnrealBridgeGameplayAbilityLibrary.list_gameplay_effects_by_tag('Effect.Buff', 50)
```

---

## Actor AbilitySystem Snapshot

### get_actor_ability_system_info(actor_name) -> FBridgeActorAbilitySystemInfo

Find an actor in the editor world by label/name and dump its ASC state. Handles both actors implementing `IAbilitySystemInterface` and actors with a `UAbilitySystemComponent` subobject directly.

```python
info = unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_ability_system_info('BP_Hero_C_1')
if info.found:
    print('ActiveEffects:', info.active_effect_count)
    print('OwnedTags:', list(info.owned_tags))
    print('AttributeSets:', list(info.attribute_set_classes))
    for g in info.granted_abilities:
        print(f'  {g.ability_class_name} lvl={g.level} input={g.input_id} active={g.is_active}')
```

### FBridgeActorAbilitySystemInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `actor_name` | str | Resolved actor label |
| `found` | bool | `True` if an ASC was located |
| `granted_abilities` | list[FBridgeGrantedAbilityInfo] | Ability specs currently granted |
| `owned_tags` | list[str] | All currently owned tags (effects + loose) |
| `active_effect_count` | int | Count from `GetActiveEffects(FGameplayEffectQuery())` |
| `attribute_set_classes` | list[str] | Spawned attribute-set class names |

### FBridgeGrantedAbilityInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `ability_class_name` | str | Ability CDO class name |
| `level` | int | Spec level |
| `input_id` | int | Bound input ID (-1 if none) |
| `is_active` | bool | Currently active |

---

## List AttributeSets

### list_attribute_sets(filter, max_results) -> list[str]

List loaded `UAttributeSet` subclasses (native + already-loaded BP). BP attribute sets not yet loaded into memory will not appear — open a referencing asset first. Empty filter + `max_results=0` is refused.

```python
sets = unreal.UnrealBridgeGameplayAbilityLibrary.list_attribute_sets('MyGame', 50)
for s in sets:
    print(s)
```

---

## Live Attribute Read

### get_attribute_value(actor_name, attribute_name) -> FBridgeAttributeValue

Read the current and base value of an attribute on an actor's ASC. `attribute_name` may be qualified (`"MyAttributeSet.Health"`) or bare (`"Health"`).

```python
v = unreal.UnrealBridgeGameplayAbilityLibrary.get_attribute_value('BP_Hero_C_1', 'Health')
if v.found:
    print(f'{v.attribute_name}: {v.current_value} (base {v.base_value})')
```

### FBridgeAttributeValue fields

| Field | Type | Description |
|-------|------|-------------|
| `attribute_name` | str | Echoed query |
| `found` | bool | Resolved on a spawned AttributeSet |
| `current_value` | float | Modified value |
| `base_value` | float | Pre-modifier value |

---

## Active Effects

### get_actor_active_effects(actor_name, max_results) -> list[FBridgeActiveEffectInfo]

Enumerate currently active GameplayEffects on an actor's ASC with timing / stack data.

> ⚠️ **Token cost: LOW–MEDIUM.** Output scales with active-effect count × granted-tag count. A heavily buffed raid boss can easily have 20+ concurrent effects; pass `max_results` to cap. Design-time tag-inheritance from the GE class is **not** included — use `get_gameplay_effect_blueprint_info` on the class for that. Only `Spec.DynamicGrantedTags` (runtime granted) is emitted here.

```python
effs = unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_active_effects('BP_Hero_C_1', 0)
for e in effs:
    remain = 'infinite' if e.time_remaining < 0 else f'{e.time_remaining:.1f}s'
    print(f'{e.effect_class_name} stacks={e.stack_count} remaining={remain} period={e.period_seconds}')
    for t in e.dynamic_granted_tags:
        print(f'  +{t}')
```

### FBridgeActiveEffectInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `effect_class_name` | str | GE class name |
| `time_remaining` | float | Seconds left; -1 for infinite / no duration |
| `duration` | float | Total duration at application |
| `stack_count` | int | Current stacks |
| `period_seconds` | float | Period (0 = non-periodic) |
| `dynamic_granted_tags` | list[str] | Runtime dynamic tags on the spec |

---

## Tag Hierarchy Browse

### find_child_tags(parent_tag, recursive) -> list[str]

Enumerate children of a gameplay tag in the hierarchy. With `recursive=False`, returns only immediate children (one extra dot segment); `True` returns all descendants. Invalid parent → empty list.

```python
# Direct children
immediate = unreal.UnrealBridgeGameplayAbilityLibrary.find_child_tags('Mover', False)
# Whole subtree
subtree = unreal.UnrealBridgeGameplayAbilityLibrary.find_child_tags('Ability.Combat', True)
```

---

## Tag Parents

### get_tag_parents(tag_string) -> list[str]

Return the ancestor chain of a gameplay tag (root first, **excluding** the tag itself). Invalid tag → empty list.

> Token cost: LOW. Output is bounded by tag depth (typically 1–4 entries).

```python
# "A.B.C" -> ["A", "A.B"]
print(unreal.UnrealBridgeGameplayAbilityLibrary.get_tag_parents('Mover.IsOnGround'))
# -> ['Mover']
```

Pair with `find_child_tags` to walk the hierarchy in both directions.

---

## Actor Tag Query

### actor_has_gameplay_tag(actor_name, tag_string, exact_match) -> int | None

Test whether an actor's ASC currently owns a gameplay tag.

> ⚠️ **Python binding quirk.** Because this UFUNCTION returns `bool` with an `int32& OutTagCount` out-param, the Python wrapper collapses it to **`int` (the count) when true, `None` when false**. Do **not** try to unpack a tuple.

- `exact_match=True` — requires the exact tag (`GetTagCount(Tag) > 0`)
- `exact_match=False` — parent-matches too via `HasMatchingGameplayTag` (child tags satisfy parent queries)

Returns `None` on: invalid tag, actor not found, actor has no ASC, tag not owned.

```python
r = unreal.UnrealBridgeGameplayAbilityLibrary.actor_has_gameplay_tag(
    'BP_Hero_C_1', 'State.Stunned', False)
if r is not None:
    print(f'stunned, stacks={r}')
else:
    print('not stunned (or no ASC)')
```

---

## Ability Cooldown

### get_ability_cooldown_info(actor_name, ability_blueprint_path) -> FBridgeAbilityCooldownInfo

Query the cooldown state of a specific ability on an actor's ASC. Uses `UGameplayAbility::GetCooldownTimeRemainingAndDuration` with `ASC->AbilityActorInfo`, so the ability must currently be granted to the actor for `found=True`.

`cooldown_tags` is populated from the ability CDO regardless of whether the actor has an ASC (useful for pure metadata lookups — pass a bogus `actor_name` to just read the tags).

```python
info = unreal.UnrealBridgeGameplayAbilityLibrary.get_ability_cooldown_info(
    'BP_Hero_C_1', '/Game/Abilities/GA_Dash')
if info.found:
    status = 'ready' if not info.on_cooldown else f'{info.time_remaining:.1f}/{info.cooldown_duration:.1f}s'
    print(f'{info.ability_class_name}: {status}')
    print('cooldown tags:', list(info.cooldown_tags))
```

### FBridgeAbilityCooldownInfo fields

| Field | Type | Description |
|-------|------|-------------|
| `ability_class_name` | str | Ability class name (populated even when the spec wasn't found on the ASC) |
| `found` | bool | `True` when the ability spec was located on the ASC |
| `on_cooldown` | bool | `True` when `time_remaining > 0` |
| `time_remaining` | float | Seconds left (0 when off cooldown) |
| `cooldown_duration` | float | Total duration for the current application (0 when off cooldown) |
| `cooldown_tags` | list[str] | Tags that trigger cooldown for the ability (from CDO) |

---

## Find Active Effects by Tag

### find_active_effects_by_tag(actor_name, tag_query, max_results) -> list[FBridgeActiveEffectInfo]

Like `get_actor_active_effects`, but filtered to specs whose asset tags, granted tags (combined design-time), or `DynamicGrantedTags` contain the query tag (exact `HasTag` match — no parent walk).

> Token cost: LOW–MEDIUM. Scales with matching count only. Cheaper than `get_actor_active_effects` + client-side filtering because the scan happens in C++.

Invalid `tag_query` / missing actor / missing ASC → empty list (silent). Returns the same `FBridgeActiveEffectInfo` struct as `get_actor_active_effects`.

```python
dots = unreal.UnrealBridgeGameplayAbilityLibrary.find_active_effects_by_tag(
    'BP_Hero_C_1', 'Effect.Damage.OverTime', 0)
for e in dots:
    print(e.effect_class_name, e.time_remaining, e.stack_count)
```

---

## List Ability Blueprints

### list_ability_blueprints(filter, max_results) -> list[str]

Scan the AssetRegistry for **every** `UGameplayAbility` Blueprint asset path, optionally filtered by case-insensitive path substring. Empty filter + `max_results=0` is **refused** (returns `[]`) to prevent full-registry dumps.

Unlike `list_abilities_by_tag`, this does **not** require registered tags — it's the right starting point when you don't yet know what tags a project uses.

> ⚠️ **Token cost: MEDIUM–HIGH on large projects.** Walks every Blueprint asset and loads each to test the class hierarchy. Use a narrow filter (e.g. folder name) or small `max_results`.

```python
all_abilities = unreal.UnrealBridgeGameplayAbilityLibrary.list_ability_blueprints('/Game/Abilities', 100)
for p in all_abilities:
    print(p)
```

---

## Gameplay Tag Registry

### list_gameplay_tags(filter, max_results) -> list[str]

Dump registered gameplay tags from `UGameplayTagsManager`, optionally filtered by case-insensitive substring.

> ⚠️ **Token cost: MEDIUM–HIGH on projects with large tag hierarchies.** Game projects with GAS + input + cue systems often register 500–2000+ tags. Empty filter + `max_results=0` is **refused** to prevent accidental full dumps — provide either a substring or a cap. Prefer a specific prefix (e.g. `"Ability.Combat"`) over `""`.

```python
tags = unreal.UnrealBridgeGameplayAbilityLibrary.list_gameplay_tags('Ability.Combat', 100)
for t in tags:
    print(t)
```

---

## Native UE Python fallbacks

```python
# Give an ability at runtime (PIE) — ASC write ops live on the C++ side:
asc = unreal.AbilitySystemBlueprintLibrary.get_ability_system_component(actor)
# Drive grants / effect application via Blueprint or gameplay code rather than Python.
```

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

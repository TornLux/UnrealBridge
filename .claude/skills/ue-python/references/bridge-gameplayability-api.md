# GameplayAbilitySystem API

`unreal.UnrealBridgeGameplayAbilityLibrary` — introspection for the GameplayAbilitySystem plugin (abilities, effects, attribute sets, ability system components).

This library is a scaffold and is being filled out incrementally. If the API you need isn't here yet, fall back to native `unreal.AbilitySystemBlueprintLibrary` and `unreal.AbilitySystemGlobals`.

---

## GameplayAbility Blueprint Info

### get_gameplay_ability_blueprint_info(ability_blueprint_path) -> FBridgeGameplayAbilityInfo

Read metadata from a `UGameplayAbility` Blueprint's CDO (class default object).

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
| `ability_tags` | list[str] | AssetTags on the CDO (formerly AbilityTags) |
| `cost_gameplay_effect_class` | str | Path to the cost GE class, if any |
| `cooldown_gameplay_effect_class` | str | Path to the cooldown GE class, if any |

Empty strings / empty lists are returned on failure (bad path, wrong class) — no exception.

---

## Native UE Python fallbacks

```python
# Give an ability at runtime (PIE):
asc = unreal.AbilitySystemBlueprintLibrary.get_ability_system_component(actor)
# Most ASC operations live on the C++ side — typically drive them via Blueprint or gameplay code
# rather than Python.
```

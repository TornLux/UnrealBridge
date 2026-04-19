# UE Python API — Actor & Level Operations

## EditorActorSubsystem (preferred)

```python
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Query
actors = sub.get_all_level_actors()
selected = sub.get_selected_level_actors()

# Spawn
actor = sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))

# Destroy (DESTRUCTIVE)
sub.destroy_actor(actor)

# Selection
sub.set_selected_level_actors([actor1, actor2])
```

## Actor transform

```python
# Get
loc = actor.get_actor_location()
rot = actor.get_actor_rotation()
scale = actor.get_actor_scale3d()

# Set
actor.set_actor_location(unreal.Vector(100, 200, 300), False, False)
actor.set_actor_rotation(unreal.Rotator(0, 90, 0), False)
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))
```

## Actor properties

```python
# Label
label = actor.get_actor_label()
actor.set_actor_label('NewLabel')

# Folder
folder = actor.get_folder_path()

# Hidden
is_hidden = actor.is_hidden_ed()

# Generic property access
val = actor.get_editor_property('property_name')
actor.set_editor_property('property_name', value)
```

## Level loading

```python
import unreal

# Preferred (5.7+)
sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sub.load_level('/Game/Maps/MyLevel')

# Legacy (deprecated but works)
unreal.EditorLevelLibrary.load_level('/Game/Maps/MyLevel')
```

## Transactions (undo support)

```python
with unreal.ScopedEditorTransaction('Move actors') as trans:
    actor.set_actor_location(unreal.Vector(0, 0, 100), False, False)
```

## World info

```python
sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = sub.get_editor_world()
print(world.get_name())
```

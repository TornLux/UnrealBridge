"""
UnrealBridge helper functions for common editor operations.

These are pre-loaded in the UE Python environment and can be called
directly from scripts sent via the bridge.

Usage from bridge:
    python bridge.py exec "from unreal_bridge_helpers import *; print(list_assets('/Game/Props'))"
"""

import unreal


def list_assets(path, class_filter=None, recursive=True):
    """List all assets under a content path.

    Args:
        path: Content path (e.g. '/Game/Props')
        class_filter: Optional class name to filter by (e.g. 'StaticMesh')
        recursive: Whether to search subdirectories

    Returns:
        List of asset path strings.
    """
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()

    if recursive:
        assets = asset_registry.get_assets_by_path(path, recursive=True)
    else:
        assets = asset_registry.get_assets_by_path(path, recursive=False)

    paths = []
    for asset_data in assets:
        if class_filter and str(asset_data.asset_class_path.asset_name) != class_filter:
            continue
        paths.append(str(asset_data.get_full_name()))

    return paths


def get_selected_actors():
    """Get all currently selected actors in the level editor.

    Returns:
        List of dicts with actor info (name, class, location, rotation).
    """
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_selected_level_actors()

    result = []
    for actor in actors:
        loc = actor.get_actor_location()
        rot = actor.get_actor_rotation()
        result.append({
            "name": actor.get_name(),
            "label": actor.get_actor_label(),
            "class": actor.get_class().get_name(),
            "location": {"x": loc.x, "y": loc.y, "z": loc.z},
            "rotation": {"pitch": rot.pitch, "yaw": rot.yaw, "roll": rot.roll},
        })

    return result


def find_actors_by_class(class_name):
    """Find all actors of a given class in the current level.

    Args:
        class_name: Unreal class name (e.g. 'StaticMeshActor', 'PointLight')

    Returns:
        List of actor names.
    """
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    return [
        a.get_actor_label()
        for a in actors
        if a.get_class().get_name() == class_name
    ]


def get_actor_properties(actor_label):
    """Get properties of an actor by its label.

    Args:
        actor_label: The actor's display label in the editor.

    Returns:
        Dict of property names to values.
    """
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()

    for actor in actors:
        if actor.get_actor_label() == actor_label:
            loc = actor.get_actor_location()
            rot = actor.get_actor_rotation()
            scale = actor.get_actor_scale3d()
            return {
                "name": actor.get_name(),
                "label": actor.get_actor_label(),
                "class": actor.get_class().get_name(),
                "location": {"x": loc.x, "y": loc.y, "z": loc.z},
                "rotation": {"pitch": rot.pitch, "yaw": rot.yaw, "roll": rot.roll},
                "scale": {"x": scale.x, "y": scale.y, "z": scale.z},
                "is_hidden": actor.is_hidden_ed(),
                "folder_path": str(actor.get_folder_path()),
            }

    return None


def set_actor_transform(actor_label, location=None, rotation=None, scale=None):
    """Set the transform of an actor by its label.

    Args:
        actor_label: The actor's display label.
        location: Optional dict {"x":, "y":, "z":} or None to keep current.
        rotation: Optional dict {"pitch":, "yaw":, "roll":} or None.
        scale: Optional dict {"x":, "y":, "z":} or None.

    Returns:
        True if the actor was found and updated.
    """
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()

    for actor in actors:
        if actor.get_actor_label() == actor_label:
            if location:
                actor.set_actor_location(
                    unreal.Vector(location["x"], location["y"], location["z"]),
                    False, False
                )
            if rotation:
                actor.set_actor_rotation(
                    unreal.Rotator(rotation["pitch"], rotation["yaw"], rotation["roll"]),
                    False
                )
            if scale:
                actor.set_actor_scale3d(
                    unreal.Vector(scale["x"], scale["y"], scale["z"])
                )
            return True

    return False


def get_world_info():
    """Get information about the current world/level.

    Returns:
        Dict with level name, actor count, etc.
    """
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()

    return {
        "level_name": world.get_name() if world else "None",
        "actor_count": len(actors),
        "map_name": unreal.EditorLevelLibrary.get_editor_world().get_path_name() if world else "None",
    }

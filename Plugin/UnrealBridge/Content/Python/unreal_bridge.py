"""
Auto-generated kwargs-only wrapper for UnrealBridge*Library functions.

Regenerate after C++ header changes:
    python tools/gen_manifest.py

Usage from a script sent via the bridge:
    from unreal_bridge import Asset, Level
    paths, _ = Asset.search_assets_in_all_content(query='Hero', max_results=20)
    info = Level.get_actor_info(actor_path='/Persistent/Player')

Why kwargs-only? Positional-arg-order is the #1 source of model
hallucinations against bridge APIs — kwargs make the contract
structural rather than mnemonic.
"""

import unreal

_GENERATED_AT = '2026-05-02T10:49:43+00:00'
_UE_VERSION = '5.7.1-48512491+++UE5+Release-5.7'

class Anim:
    """Wraps unreal.UnrealBridgeAnimLibrary (kwargs-only)."""

    @staticmethod
    def add_anim_conduit(*, anim_blueprint_path, state_machine_graph_name, conduit_name, pos_x, pos_y):
        """X.add_anim_conduit(anim_blueprint_path, state_machine_graph_name, conduit_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_conduit(anim_blueprint_path, state_machine_graph_name, conduit_name, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_blend_list_by_bool(*, anim_blueprint_path, graph_name, pos_x, pos_y):
        """X.add_anim_graph_node_blend_list_by_bool(anim_blueprint_path, graph_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_blend_list_by_bool(anim_blueprint_path, graph_name, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_blend_list_by_int(*, anim_blueprint_path, graph_name, num_poses, pos_x, pos_y):
        """X.add_anim_graph_node_blend_list_by_int(anim_blueprint_path, graph_name, num_poses, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_blend_list_by_int(anim_blueprint_path, graph_name, num_poses, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_blend_space_player(*, anim_blueprint_path, graph_name, blend_space_path, pos_x, pos_y):
        """X.add_anim_graph_node_blend_space_player(anim_blueprint_path, graph_name, blend_space_path, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_blend_space_player(anim_blueprint_path, graph_name, blend_space_path, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_by_class_name(*, anim_blueprint_path, graph_name, short_class_name, pos_x, pos_y):
        """X.add_anim_graph_node_by_class_name(anim_blueprint_path, graph_name, short_class_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_by_class_name(anim_blueprint_path, graph_name, short_class_name, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_layered_bone_blend(*, anim_blueprint_path, graph_name, num_blend_poses, pos_x, pos_y):
        """X.add_anim_graph_node_layered_bone_blend(anim_blueprint_path, graph_name, num_blend_poses, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_layered_bone_blend(anim_blueprint_path, graph_name, num_blend_poses, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_linked_anim_layer(*, anim_blueprint_path, graph_name, interface_class_path, layer_name, pos_x, pos_y):
        """X.add_anim_graph_node_linked_anim_layer(anim_blueprint_path, graph_name, interface_class_path, layer_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_linked_anim_layer(anim_blueprint_path, graph_name, interface_class_path, layer_name, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_sequence_player(*, anim_blueprint_path, graph_name, sequence_path, pos_x, pos_y):
        """X.add_anim_graph_node_sequence_player(anim_blueprint_path, graph_name, sequence_path, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_sequence_player(anim_blueprint_path, graph_name, sequence_path, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_slot(*, anim_blueprint_path, graph_name, slot_name, pos_x, pos_y):
        """X.add_anim_graph_node_slot(anim_blueprint_path, graph_name, slot_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_slot(anim_blueprint_path, graph_name, slot_name, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_state_machine(*, anim_blueprint_path, graph_name, state_machine_name, pos_x, pos_y):
        """X.add_anim_graph_node_state_machine(anim_blueprint_path, graph_name, state_machine_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_state_machine(anim_blueprint_path, graph_name, state_machine_name, pos_x, pos_y)

    @staticmethod
    def add_anim_graph_node_two_way_blend(*, anim_blueprint_path, graph_name, pos_x, pos_y):
        """X.add_anim_graph_node_two_way_blend(anim_blueprint_path, graph_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_graph_node_two_way_blend(anim_blueprint_path, graph_name, pos_x, pos_y)

    @staticmethod
    def add_anim_notify(*, sequence_path, notify_name, trigger_time, duration):
        """X.add_anim_notify(sequence_path, notify_name, trigger_time, duration) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_notify(sequence_path, notify_name, trigger_time, duration)

    @staticmethod
    def add_anim_state(*, anim_blueprint_path, state_machine_graph_name, state_name, pos_x, pos_y):
        """X.add_anim_state(anim_blueprint_path, state_machine_graph_name, state_name, pos_x, pos_y) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_state(anim_blueprint_path, state_machine_graph_name, state_name, pos_x, pos_y)

    @staticmethod
    def add_anim_sync_marker(*, sequence_path, marker_name, time):
        """X.add_anim_sync_marker(sequence_path, marker_name, time) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_sync_marker(sequence_path, marker_name, time)

    @staticmethod
    def add_anim_transition(*, anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name):
        """X.add_anim_transition(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name) -> str"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_transition(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name)

    @staticmethod
    def add_montage_section(*, montage_path, section_name, start_time):
        """X.add_montage_section(montage_path, section_name, start_time) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.add_montage_section(montage_path, section_name, start_time)

    @staticmethod
    def add_skeleton_socket(*, skeleton_path, socket_name, parent_bone_name, relative_location, relative_rotation, relative_scale):
        """X.add_skeleton_socket(skeleton_path, socket_name, parent_bone_name, relative_location, relative_rotation, relative_scale) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.add_skeleton_socket(skeleton_path, socket_name, parent_bone_name, relative_location, relative_rotation, relative_scale)

    @staticmethod
    def auto_layout_anim_graph(*, anim_blueprint_path, graph_name, horizontal_spacing, vertical_spacing):
        """X.auto_layout_anim_graph(anim_blueprint_path, graph_name, horizontal_spacing, vertical_spacing) -> BridgeAnimLayoutResult"""
        return unreal.UnrealBridgeAnimLibrary.auto_layout_anim_graph(anim_blueprint_path, graph_name, horizontal_spacing, vertical_spacing)

    @staticmethod
    def auto_layout_state_machine(*, anim_blueprint_path, state_machine_graph_name, horizontal_spacing, vertical_spacing):
        """X.auto_layout_state_machine(anim_blueprint_path, state_machine_graph_name, horizontal_spacing, vertical_spacing) -> BridgeAnimLayoutResult"""
        return unreal.UnrealBridgeAnimLibrary.auto_layout_state_machine(anim_blueprint_path, state_machine_graph_name, horizontal_spacing, vertical_spacing)

    @staticmethod
    def connect_anim_graph_pins(*, anim_blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name):
        """X.connect_anim_graph_pins(anim_blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.connect_anim_graph_pins(anim_blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name)

    @staticmethod
    def disconnect_anim_graph_pin(*, anim_blueprint_path, graph_name, node_guid, pin_name):
        """X.disconnect_anim_graph_pin(anim_blueprint_path, graph_name, node_guid, pin_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.disconnect_anim_graph_pin(anim_blueprint_path, graph_name, node_guid, pin_name)

    @staticmethod
    def find_anim_graph_node_by_class(*, anim_blueprint_path, graph_name, short_class_name):
        """X.find_anim_graph_node_by_class(anim_blueprint_path, graph_name, short_class_name) -> str"""
        return unreal.UnrealBridgeAnimLibrary.find_anim_graph_node_by_class(anim_blueprint_path, graph_name, short_class_name)

    @staticmethod
    def get_anim_blueprint_info(*, anim_blueprint_path):
        """X.get_anim_blueprint_info(anim_blueprint_path) -> BridgeAnimBlueprintInfo"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_blueprint_info(anim_blueprint_path)

    @staticmethod
    def get_anim_curves(*, anim_blueprint_path):
        """X.get_anim_curves(anim_blueprint_path) -> Array[str]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_curves(anim_blueprint_path)

    @staticmethod
    def get_anim_graph_info(*, anim_blueprint_path):
        """X.get_anim_graph_info(anim_blueprint_path) -> Array[BridgeStateMachineInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_graph_info(anim_blueprint_path)

    @staticmethod
    def get_anim_graph_nodes(*, anim_blueprint_path):
        """X.get_anim_graph_nodes(anim_blueprint_path) -> Array[BridgeAnimGraphNodeInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_graph_nodes(anim_blueprint_path)

    @staticmethod
    def get_anim_linked_layers(*, anim_blueprint_path):
        """X.get_anim_linked_layers(anim_blueprint_path) -> Array[BridgeAnimLayerInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_linked_layers(anim_blueprint_path)

    @staticmethod
    def get_anim_node_details(*, anim_blueprint_path, node_index):
        """X.get_anim_node_details(anim_blueprint_path, node_index) -> Array[str]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_node_details(anim_blueprint_path, node_index)

    @staticmethod
    def get_anim_node_details_by_guid(*, anim_blueprint_path, graph_name, node_guid):
        """X.get_anim_node_details_by_guid(anim_blueprint_path, graph_name, node_guid) -> Array[str]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_node_details_by_guid(anim_blueprint_path, graph_name, node_guid)

    @staticmethod
    def get_anim_sequence_info(*, sequence_path):
        """X.get_anim_sequence_info(sequence_path) -> BridgeAnimSequenceInfo"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_sequence_info(sequence_path)

    @staticmethod
    def get_anim_slots(*, anim_blueprint_path):
        """X.get_anim_slots(anim_blueprint_path) -> Array[BridgeAnimSlotInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_slots(anim_blueprint_path)

    @staticmethod
    def get_anim_sync_markers(*, sequence_path):
        """X.get_anim_sync_markers(sequence_path) -> Array[BridgeAnimSyncMarker]"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_sync_markers(sequence_path)

    @staticmethod
    def get_anim_transition_rule_graph_name(*, anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name):
        """X.get_anim_transition_rule_graph_name(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name) -> str"""
        return unreal.UnrealBridgeAnimLibrary.get_anim_transition_rule_graph_name(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name)

    @staticmethod
    def get_blend_profile_entries(*, skeleton_path, profile_name):
        """X.get_blend_profile_entries(skeleton_path, profile_name) -> Array[BridgeBlendProfileEntry]"""
        return unreal.UnrealBridgeAnimLibrary.get_blend_profile_entries(skeleton_path, profile_name)

    @staticmethod
    def get_blend_space_info(*, blend_space_path):
        """X.get_blend_space_info(blend_space_path) -> BridgeBlendSpaceInfo"""
        return unreal.UnrealBridgeAnimLibrary.get_blend_space_info(blend_space_path)

    @staticmethod
    def get_montage_info(*, montage_path):
        """X.get_montage_info(montage_path) -> BridgeMontageInfo"""
        return unreal.UnrealBridgeAnimLibrary.get_montage_info(montage_path)

    @staticmethod
    def get_montage_slot_segments(*, montage_path):
        """X.get_montage_slot_segments(montage_path) -> Array[BridgeMontageSlotSegment]"""
        return unreal.UnrealBridgeAnimLibrary.get_montage_slot_segments(montage_path)

    @staticmethod
    def get_skeleton_blend_profiles(*, skeleton_path):
        """X.get_skeleton_blend_profiles(skeleton_path) -> Array[BridgeBlendProfileInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_skeleton_blend_profiles(skeleton_path)

    @staticmethod
    def get_skeleton_bone_tree(*, skeleton_path):
        """X.get_skeleton_bone_tree(skeleton_path) -> Array[BridgeBoneInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_skeleton_bone_tree(skeleton_path)

    @staticmethod
    def get_skeleton_sockets(*, skeleton_path):
        """X.get_skeleton_sockets(skeleton_path) -> Array[BridgeSocketInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_skeleton_sockets(skeleton_path)

    @staticmethod
    def get_skeleton_virtual_bones(*, skeleton_path):
        """X.get_skeleton_virtual_bones(skeleton_path) -> Array[BridgeVirtualBoneInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_skeleton_virtual_bones(skeleton_path)

    @staticmethod
    def list_anim_graph_nodes(*, anim_blueprint_path, graph_name):
        """X.list_anim_graph_nodes(anim_blueprint_path, graph_name) -> Array[str]"""
        return unreal.UnrealBridgeAnimLibrary.list_anim_graph_nodes(anim_blueprint_path, graph_name)

    @staticmethod
    def list_anim_graphs(*, anim_blueprint_path):
        """X.list_anim_graphs(anim_blueprint_path) -> Array[BridgeAnimGraphSummary]"""
        return unreal.UnrealBridgeAnimLibrary.list_anim_graphs(anim_blueprint_path)

    @staticmethod
    def list_assets_for_skeleton(*, skeleton_path, asset_type, max_results):
        """X.list_assets_for_skeleton(skeleton_path, asset_type, max_results) -> Array[str]"""
        return unreal.UnrealBridgeAnimLibrary.list_assets_for_skeleton(skeleton_path, asset_type, max_results)

    @staticmethod
    def remove_anim_graph_node(*, anim_blueprint_path, graph_name, node_guid):
        """X.remove_anim_graph_node(anim_blueprint_path, graph_name, node_guid) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.remove_anim_graph_node(anim_blueprint_path, graph_name, node_guid)

    @staticmethod
    def remove_anim_notifies_by_name(*, sequence_path, notify_name):
        """X.remove_anim_notifies_by_name(sequence_path, notify_name) -> int32"""
        return unreal.UnrealBridgeAnimLibrary.remove_anim_notifies_by_name(sequence_path, notify_name)

    @staticmethod
    def remove_anim_state(*, anim_blueprint_path, state_machine_graph_name, state_name):
        """X.remove_anim_state(anim_blueprint_path, state_machine_graph_name, state_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.remove_anim_state(anim_blueprint_path, state_machine_graph_name, state_name)

    @staticmethod
    def remove_anim_sync_markers_by_name(*, sequence_path, marker_name):
        """X.remove_anim_sync_markers_by_name(sequence_path, marker_name) -> int32"""
        return unreal.UnrealBridgeAnimLibrary.remove_anim_sync_markers_by_name(sequence_path, marker_name)

    @staticmethod
    def remove_anim_transition(*, anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name):
        """X.remove_anim_transition(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.remove_anim_transition(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name)

    @staticmethod
    def remove_montage_section(*, montage_path, section_name):
        """X.remove_montage_section(montage_path, section_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.remove_montage_section(montage_path, section_name)

    @staticmethod
    def remove_skeleton_socket(*, skeleton_path, socket_name):
        """X.remove_skeleton_socket(skeleton_path, socket_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.remove_skeleton_socket(skeleton_path, socket_name)

    @staticmethod
    def rename_anim_state(*, anim_blueprint_path, state_machine_graph_name, old_name, new_name):
        """X.rename_anim_state(anim_blueprint_path, state_machine_graph_name, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.rename_anim_state(anim_blueprint_path, state_machine_graph_name, old_name, new_name)

    @staticmethod
    def rename_skeleton_socket(*, skeleton_path, old_name, new_name):
        """X.rename_skeleton_socket(skeleton_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.rename_skeleton_socket(skeleton_path, old_name, new_name)

    @staticmethod
    def set_anim_graph_node_position(*, anim_blueprint_path, graph_name, node_guid, pos_x, pos_y):
        """X.set_anim_graph_node_position(anim_blueprint_path, graph_name, node_guid, pos_x, pos_y) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_graph_node_position(anim_blueprint_path, graph_name, node_guid, pos_x, pos_y)

    @staticmethod
    def set_anim_sequence_player_sequence(*, anim_blueprint_path, graph_name, node_guid, sequence_path):
        """X.set_anim_sequence_player_sequence(anim_blueprint_path, graph_name, node_guid, sequence_path) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_sequence_player_sequence(anim_blueprint_path, graph_name, node_guid, sequence_path)

    @staticmethod
    def set_anim_sequence_rate_scale(*, sequence_path, rate_scale):
        """X.set_anim_sequence_rate_scale(sequence_path, rate_scale) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_sequence_rate_scale(sequence_path, rate_scale)

    @staticmethod
    def set_anim_slot_name(*, anim_blueprint_path, graph_name, node_guid, slot_name):
        """X.set_anim_slot_name(anim_blueprint_path, graph_name, node_guid, slot_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_slot_name(anim_blueprint_path, graph_name, node_guid, slot_name)

    @staticmethod
    def set_anim_state_default(*, anim_blueprint_path, state_machine_graph_name, state_name):
        """X.set_anim_state_default(anim_blueprint_path, state_machine_graph_name, state_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_state_default(anim_blueprint_path, state_machine_graph_name, state_name)

    @staticmethod
    def set_anim_transition_const_rule(*, anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name, value):
        """X.set_anim_transition_const_rule(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name, value) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_transition_const_rule(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name, value)

    @staticmethod
    def set_anim_transition_properties(*, anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name, crossfade_duration, priority_order, bidirectional):
        """X.set_anim_transition_properties(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name, crossfade_duration, priority_order, bidirectional) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_anim_transition_properties(anim_blueprint_path, state_machine_graph_name, from_state_name, to_state_name, crossfade_duration, priority_order, bidirectional)

    @staticmethod
    def set_montage_blend_times(*, montage_path, blend_in_time, blend_out_time, blend_out_trigger_time, enable_auto_blend_out):
        """X.set_montage_blend_times(montage_path, blend_in_time, blend_out_time, blend_out_trigger_time, enable_auto_blend_out) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_montage_blend_times(montage_path, blend_in_time, blend_out_time, blend_out_trigger_time, enable_auto_blend_out)

    @staticmethod
    def set_montage_section_next(*, montage_path, section_name, next_section_name):
        """X.set_montage_section_next(montage_path, section_name, next_section_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_montage_section_next(montage_path, section_name, next_section_name)

    @staticmethod
    def set_montage_section_start_time(*, montage_path, section_name, start_time):
        """X.set_montage_section_start_time(montage_path, section_name, start_time) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_montage_section_start_time(montage_path, section_name, start_time)

    @staticmethod
    def set_skeleton_socket_transform(*, skeleton_path, socket_name, relative_location, relative_rotation, relative_scale):
        """X.set_skeleton_socket_transform(skeleton_path, socket_name, relative_location, relative_rotation, relative_scale) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_skeleton_socket_transform(skeleton_path, socket_name, relative_location, relative_rotation, relative_scale)


class Asset:
    """Wraps unreal.UnrealBridgeAssetLibrary (kwargs-only)."""

    @staticmethod
    def does_asset_exist(*, asset_path):
        """X.does_asset_exist(asset_path) -> bool"""
        return unreal.UnrealBridgeAssetLibrary.does_asset_exist(asset_path)

    @staticmethod
    def does_folder_exist(*, folder_path):
        """X.does_folder_exist(folder_path) -> bool"""
        return unreal.UnrealBridgeAssetLibrary.does_folder_exist(folder_path)

    @staticmethod
    def find_redirectors_under_path(*, folder_path, recursive):
        """X.find_redirectors_under_path(folder_path, recursive) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.find_redirectors_under_path(folder_path, recursive)

    @staticmethod
    def get_asset_class_path(*, asset_path):
        """X.get_asset_class_path(asset_path) -> str"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_class_path(asset_path)

    @staticmethod
    def get_asset_class_paths_batch(*, asset_paths):
        """X.get_asset_class_paths_batch(asset_paths) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_class_paths_batch(asset_paths)

    @staticmethod
    def get_asset_count_under_path(*, folder_path, class_filter, recursive):
        """X.get_asset_count_under_path(folder_path, class_filter, recursive) -> int32"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_count_under_path(folder_path, class_filter, recursive)

    @staticmethod
    def get_asset_disk_sizes_batch(*, asset_paths):
        """X.get_asset_disk_sizes_batch(asset_paths) -> Array[int64]"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_disk_sizes_batch(asset_paths)

    @staticmethod
    def get_asset_info(*, asset_path):
        """X.get_asset_info(asset_path) -> BridgeAssetInfo"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_info(asset_path)

    @staticmethod
    def get_asset_references(*, asset_path):
        """X.get_asset_references(asset_path) -> (out_dependencies=Array[SoftObjectPath], out_referencers=Array[SoftObjectPath])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_asset_references(asset_path)

    @staticmethod
    def get_asset_tag_value(*, asset_path, tag_name):
        """X.get_asset_tag_value(asset_path, tag_name) -> str"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_tag_value(asset_path, tag_name)

    @staticmethod
    def get_asset_tag_values_batch(*, asset_paths, tag_name):
        """X.get_asset_tag_values_batch(asset_paths, tag_name) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_asset_tag_values_batch(asset_paths, tag_name)

    @staticmethod
    def get_assets_by_class(*, class_path, search_sub_classes):
        """X.get_assets_by_class(class_path, search_sub_classes) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_by_class(class_path, search_sub_classes)

    @staticmethod
    def get_assets_by_package_paths(*, folder_paths, class_filter, recursive):
        """X.get_assets_by_package_paths(folder_paths, class_filter, recursive) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_by_package_paths(folder_paths, class_filter, recursive)

    @staticmethod
    def get_assets_by_tag_value(*, tag_name, tag_value, optional_class_path):
        """X.get_assets_by_tag_value(tag_name, tag_value, optional_class_path) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_by_tag_value(tag_name, tag_value, optional_class_path)

    @staticmethod
    def get_assets_of_classes(*, class_paths, search_sub_classes):
        """X.get_assets_of_classes(class_paths, search_sub_classes) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_of_classes(class_paths, search_sub_classes)

    @staticmethod
    def get_content_roots():
        """X.get_content_roots() -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_content_roots()

    @staticmethod
    def get_data_asset_soft_paths_by_asset_path(*, data_asset_path):
        """X.get_data_asset_soft_paths_by_asset_path(data_asset_path) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_data_asset_soft_paths_by_asset_path(data_asset_path)

    @staticmethod
    def get_data_asset_soft_paths_by_base_class(*, base_data_asset_class):
        """X.get_data_asset_soft_paths_by_base_class(base_data_asset_class) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.get_data_asset_soft_paths_by_base_class(base_data_asset_class)

    @staticmethod
    def get_data_assets_by_asset_path(*, data_asset_path):
        """X.get_data_assets_by_asset_path(data_asset_path) -> Array[AssetData]"""
        return unreal.UnrealBridgeAssetLibrary.get_data_assets_by_asset_path(data_asset_path)

    @staticmethod
    def get_data_assets_by_base_class(*, base_data_asset_class):
        """X.get_data_assets_by_base_class(base_data_asset_class) -> Array[AssetData]"""
        return unreal.UnrealBridgeAssetLibrary.get_data_assets_by_base_class(base_data_asset_class)

    @staticmethod
    def get_derived_classes(*, base_classes, excluded_classes):
        """X.get_derived_classes(base_classes, excluded_classes) -> Set[type(Class)]"""
        return unreal.UnrealBridgeAssetLibrary.get_derived_classes(base_classes, excluded_classes)

    @staticmethod
    def get_derived_classes_by_blueprint_path(*, blueprint_class_path):
        """X.get_derived_classes_by_blueprint_path(blueprint_class_path) -> Array[type(Class)]"""
        return unreal.UnrealBridgeAssetLibrary.get_derived_classes_by_blueprint_path(blueprint_class_path)

    @staticmethod
    def get_package_dependencies(*, package_name, hard_only):
        """X.get_package_dependencies(package_name, hard_only) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_package_dependencies(package_name, hard_only)

    @staticmethod
    def get_package_dependencies_recursive(*, package_name, hard_only, max_depth):
        """X.get_package_dependencies_recursive(package_name, hard_only, max_depth) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_package_dependencies_recursive(package_name, hard_only, max_depth)

    @staticmethod
    def get_package_referencers(*, package_name, hard_only):
        """X.get_package_referencers(package_name, hard_only) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_package_referencers(package_name, hard_only)

    @staticmethod
    def get_skeletal_mesh_info(*, asset_path):
        """X.get_skeletal_mesh_info(asset_path) -> BridgeSkeletalMeshInfo"""
        return unreal.UnrealBridgeAssetLibrary.get_skeletal_mesh_info(asset_path)

    @staticmethod
    def get_sound_info(*, asset_path):
        """X.get_sound_info(asset_path) -> BridgeSoundInfo"""
        return unreal.UnrealBridgeAssetLibrary.get_sound_info(asset_path)

    @staticmethod
    def get_static_mesh_info(*, asset_path):
        """X.get_static_mesh_info(asset_path) -> BridgeStaticMeshInfo"""
        return unreal.UnrealBridgeAssetLibrary.get_static_mesh_info(asset_path)

    @staticmethod
    def get_sub_folder_names(*, folder_path):
        """X.get_sub_folder_names(folder_path) -> Array[Name]"""
        return unreal.UnrealBridgeAssetLibrary.get_sub_folder_names(folder_path)

    @staticmethod
    def get_sub_folder_paths(*, folder_path):
        """X.get_sub_folder_paths(folder_path) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_sub_folder_paths(folder_path)

    @staticmethod
    def get_texture_info(*, asset_path):
        """X.get_texture_info(asset_path) -> BridgeTextureInfo"""
        return unreal.UnrealBridgeAssetLibrary.get_texture_info(asset_path)

    @staticmethod
    def get_total_disk_size_under_path(*, folder_path, class_filter, recursive):
        """X.get_total_disk_size_under_path(folder_path, class_filter, recursive) -> (int64, out_asset_count=int32)"""
        return unreal.UnrealBridgeAssetLibrary.get_total_disk_size_under_path(folder_path, class_filter, recursive)

    @staticmethod
    def list_assets_under_path(*, folder_path, include_subfolders):
        """X.list_assets_under_path(folder_path, include_subfolders) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.list_assets_under_path(folder_path, include_subfolders)

    @staticmethod
    def list_assets_under_path_simple(*, content_folder_path):
        """X.list_assets_under_path_simple(content_folder_path) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.list_assets_under_path_simple(content_folder_path)

    @staticmethod
    def resolve_redirector(*, asset_path):
        """X.resolve_redirector(asset_path) -> str"""
        return unreal.UnrealBridgeAssetLibrary.resolve_redirector(asset_path)

    @staticmethod
    def search_assets(*, query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path):
        """X.search_assets(query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path) -> (out_soft_paths=Array[SoftObjectPath], out_include_tokens_for_highlight=Array[str])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.search_assets(query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path)

    @staticmethod
    def search_assets_in_all_content(*, query, max_results):
        """X.search_assets_in_all_content(query, max_results) -> (out_soft_paths=Array[SoftObjectPath], out_include_tokens_for_highlight=Array[str])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.search_assets_in_all_content(query, max_results)

    @staticmethod
    def search_assets_under_path(*, content_folder_path, query, max_results):
        """X.search_assets_under_path(content_folder_path, query, max_results) -> (out_soft_paths=Array[SoftObjectPath], out_include_tokens_for_highlight=Array[str])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() to get the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md for idioms."""
        return unreal.UnrealBridgeAssetLibrary.search_assets_under_path(content_folder_path, query, max_results)


class Blueprint:
    """Wraps unreal.UnrealBridgeBlueprintLibrary (kwargs-only)."""

    @staticmethod
    def add_async_action_node(*, blueprint_path, graph_name, factory_class_path, factory_function_name, node_pos_x, node_pos_y):
        """X.add_async_action_node(blueprint_path, graph_name, factory_class_path, factory_function_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_async_action_node(blueprint_path, graph_name, factory_class_path, factory_function_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_blueprint_component(*, blueprint_path, component_class_path, component_name, parent_component_name):
        """X.add_blueprint_component(blueprint_path, component_class_path, component_name, parent_component_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_blueprint_component(blueprint_path, component_class_path, component_name, parent_component_name)

    @staticmethod
    def add_blueprint_interface(*, blueprint_path, interface_path):
        """X.add_blueprint_interface(blueprint_path, interface_path) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_blueprint_interface(blueprint_path, interface_path)

    @staticmethod
    def add_blueprint_variable(*, blueprint_path, name, type_string, default_value):
        """X.add_blueprint_variable(blueprint_path, name, type_string, default_value) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_blueprint_variable(blueprint_path, name, type_string, default_value)

    @staticmethod
    def add_branch_node(*, blueprint_path, graph_name, node_pos_x, node_pos_y):
        """X.add_branch_node(blueprint_path, graph_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_branch_node(blueprint_path, graph_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_break_struct_node(*, blueprint_path, graph_name, struct_path, x, y):
        """X.add_break_struct_node(blueprint_path, graph_name, struct_path, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_break_struct_node(blueprint_path, graph_name, struct_path, x, y)

    @staticmethod
    def add_breakpoint(*, blueprint_path, graph_name, node_guid, enabled):
        """X.add_breakpoint(blueprint_path, graph_name, node_guid, enabled) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_breakpoint(blueprint_path, graph_name, node_guid, enabled)

    @staticmethod
    def add_call_function_node(*, blueprint_path, graph_name, target_class_path, function_name, node_pos_x, node_pos_y):
        """X.add_call_function_node(blueprint_path, graph_name, target_class_path, function_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_call_function_node(blueprint_path, graph_name, target_class_path, function_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_cast_node(*, blueprint_path, graph_name, target_class_path, pure, node_pos_x, node_pos_y):
        """X.add_cast_node(blueprint_path, graph_name, target_class_path, pure, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_cast_node(blueprint_path, graph_name, target_class_path, pure, node_pos_x, node_pos_y)

    @staticmethod
    def add_comment_box(*, blueprint_path, graph_name, node_guids, text, x, y, width, height):
        """X.add_comment_box(blueprint_path, graph_name, node_guids, text, x, y, width, height) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_comment_box(blueprint_path, graph_name, node_guids, text, x, y, width, height)

    @staticmethod
    def add_custom_event_node(*, blueprint_path, graph_name, event_name, node_pos_x, node_pos_y):
        """X.add_custom_event_node(blueprint_path, graph_name, event_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_custom_event_node(blueprint_path, graph_name, event_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_delay_node(*, blueprint_path, graph_name, duration_seconds, x, y):
        """X.add_delay_node(blueprint_path, graph_name, duration_seconds, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_delay_node(blueprint_path, graph_name, duration_seconds, x, y)

    @staticmethod
    def add_dispatcher_bind_node(*, blueprint_path, graph_name, dispatcher_name, unbind, node_pos_x, node_pos_y):
        """X.add_dispatcher_bind_node(blueprint_path, graph_name, dispatcher_name, unbind, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_dispatcher_bind_node(blueprint_path, graph_name, dispatcher_name, unbind, node_pos_x, node_pos_y)

    @staticmethod
    def add_dispatcher_call_node(*, blueprint_path, graph_name, dispatcher_name, node_pos_x, node_pos_y):
        """X.add_dispatcher_call_node(blueprint_path, graph_name, dispatcher_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_dispatcher_call_node(blueprint_path, graph_name, dispatcher_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_dispatcher_event_node(*, blueprint_path, graph_name, dispatcher_name, x, y):
        """X.add_dispatcher_event_node(blueprint_path, graph_name, dispatcher_name, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_dispatcher_event_node(blueprint_path, graph_name, dispatcher_name, x, y)

    @staticmethod
    def add_enum_literal_node(*, blueprint_path, graph_name, enum_path, value_name, node_pos_x, node_pos_y):
        """X.add_enum_literal_node(blueprint_path, graph_name, enum_path, value_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_enum_literal_node(blueprint_path, graph_name, enum_path, value_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_event_dispatcher(*, blueprint_path, dispatcher_name):
        """X.add_event_dispatcher(blueprint_path, dispatcher_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_event_dispatcher(blueprint_path, dispatcher_name)

    @staticmethod
    def add_event_node(*, blueprint_path, graph_name, parent_class_path, event_name, node_pos_x, node_pos_y):
        """X.add_event_node(blueprint_path, graph_name, parent_class_path, event_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_event_node(blueprint_path, graph_name, parent_class_path, event_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_for_loop_node(*, blueprint_path, graph_name, with_break, x, y):
        """X.add_for_loop_node(blueprint_path, graph_name, with_break, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_for_loop_node(blueprint_path, graph_name, with_break, x, y)

    @staticmethod
    def add_foreach_node(*, blueprint_path, graph_name, with_break, x, y):
        """X.add_foreach_node(blueprint_path, graph_name, with_break, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_foreach_node(blueprint_path, graph_name, with_break, x, y)

    @staticmethod
    def add_function_local_variable(*, blueprint_path, function_name, variable_name, type_string, default_value):
        """X.add_function_local_variable(blueprint_path, function_name, variable_name, type_string, default_value) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_function_local_variable(blueprint_path, function_name, variable_name, type_string, default_value)

    @staticmethod
    def add_function_local_variable_node(*, blueprint_path, function_name, variable_name, is_set, node_pos_x, node_pos_y):
        """X.add_function_local_variable_node(blueprint_path, function_name, variable_name, is_set, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_function_local_variable_node(blueprint_path, function_name, variable_name, is_set, node_pos_x, node_pos_y)

    @staticmethod
    def add_function_parameter(*, blueprint_path, function_name, param_name, type_string, is_return):
        """X.add_function_parameter(blueprint_path, function_name, param_name, type_string, is_return) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_function_parameter(blueprint_path, function_name, param_name, type_string, is_return)

    @staticmethod
    def add_interface_message_node(*, blueprint_path, graph_name, interface_path, function_name, node_pos_x, node_pos_y):
        """X.add_interface_message_node(blueprint_path, graph_name, interface_path, function_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_interface_message_node(blueprint_path, graph_name, interface_path, function_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_make_array_node(*, blueprint_path, graph_name, node_pos_x, node_pos_y):
        """X.add_make_array_node(blueprint_path, graph_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_make_array_node(blueprint_path, graph_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_make_literal_node(*, blueprint_path, graph_name, type_string, value, x, y):
        """X.add_make_literal_node(blueprint_path, graph_name, type_string, value, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_make_literal_node(blueprint_path, graph_name, type_string, value, x, y)

    @staticmethod
    def add_make_struct_node(*, blueprint_path, graph_name, struct_path, x, y):
        """X.add_make_struct_node(blueprint_path, graph_name, struct_path, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_make_struct_node(blueprint_path, graph_name, struct_path, x, y)

    @staticmethod
    def add_node_by_class_name(*, blueprint_path, graph_name, node_class_path, node_pos_x, node_pos_y):
        """X.add_node_by_class_name(blueprint_path, graph_name, node_class_path, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_node_by_class_name(blueprint_path, graph_name, node_class_path, node_pos_x, node_pos_y)

    @staticmethod
    def add_reroute_node(*, blueprint_path, graph_name, x, y):
        """X.add_reroute_node(blueprint_path, graph_name, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_reroute_node(blueprint_path, graph_name, x, y)

    @staticmethod
    def add_select_node(*, blueprint_path, graph_name, x, y):
        """X.add_select_node(blueprint_path, graph_name, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_select_node(blueprint_path, graph_name, x, y)

    @staticmethod
    def add_self_node(*, blueprint_path, graph_name, node_pos_x, node_pos_y):
        """X.add_self_node(blueprint_path, graph_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_self_node(blueprint_path, graph_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_sequence_node(*, blueprint_path, graph_name, pin_count, node_pos_x, node_pos_y):
        """X.add_sequence_node(blueprint_path, graph_name, pin_count, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_sequence_node(blueprint_path, graph_name, pin_count, node_pos_x, node_pos_y)

    @staticmethod
    def add_set_timer_by_function_name_node(*, blueprint_path, graph_name, function_name, time_seconds, looping, x, y):
        """X.add_set_timer_by_function_name_node(blueprint_path, graph_name, function_name, time_seconds, looping, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_set_timer_by_function_name_node(blueprint_path, graph_name, function_name, time_seconds, looping, x, y)

    @staticmethod
    def add_spawn_actor_from_class_node(*, blueprint_path, graph_name, actor_class_path, x, y):
        """X.add_spawn_actor_from_class_node(blueprint_path, graph_name, actor_class_path, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_spawn_actor_from_class_node(blueprint_path, graph_name, actor_class_path, x, y)

    @staticmethod
    def add_timeline_node(*, blueprint_path, graph_name, timeline_template_name, x, y):
        """X.add_timeline_node(blueprint_path, graph_name, timeline_template_name, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_timeline_node(blueprint_path, graph_name, timeline_template_name, x, y)

    @staticmethod
    def add_variable_node(*, blueprint_path, graph_name, variable_name, is_set, node_pos_x, node_pos_y):
        """X.add_variable_node(blueprint_path, graph_name, variable_name, is_set, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_variable_node(blueprint_path, graph_name, variable_name, is_set, node_pos_x, node_pos_y)

    @staticmethod
    def add_while_loop_node(*, blueprint_path, graph_name, x, y):
        """X.add_while_loop_node(blueprint_path, graph_name, x, y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_while_loop_node(blueprint_path, graph_name, x, y)

    @staticmethod
    def align_nodes(*, blueprint_path, graph_name, node_guids, axis):
        """X.align_nodes(blueprint_path, graph_name, node_guids, axis) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.align_nodes(blueprint_path, graph_name, node_guids, axis)

    @staticmethod
    def apply_graph_ops(*, blueprint_path, ops_json):
        """X.apply_graph_ops(blueprint_path, ops_json) -> Array[BridgeGraphOpResult]"""
        return unreal.UnrealBridgeBlueprintLibrary.apply_graph_ops(blueprint_path, ops_json)

    @staticmethod
    def auto_insert_reroutes(*, blueprint_path, graph_name):
        """X.auto_insert_reroutes(blueprint_path, graph_name) -> int32"""
        return unreal.UnrealBridgeBlueprintLibrary.auto_insert_reroutes(blueprint_path, graph_name)

    @staticmethod
    def auto_layout_graph(*, blueprint_path, graph_name, strategy, anchor_node_guid, horizontal_spacing, vertical_spacing):
        """X.auto_layout_graph(blueprint_path, graph_name, strategy, anchor_node_guid, horizontal_spacing, vertical_spacing) -> BridgeLayoutResult"""
        return unreal.UnrealBridgeBlueprintLibrary.auto_layout_graph(blueprint_path, graph_name, strategy, anchor_node_guid, horizontal_spacing, vertical_spacing)

    @staticmethod
    def change_variable_type_with_report(*, blueprint_path, variable_name, new_type_string):
        """X.change_variable_type_with_report(blueprint_path, variable_name, new_type_string) -> Array[str] or None"""
        return unreal.UnrealBridgeBlueprintLibrary.change_variable_type_with_report(blueprint_path, variable_name, new_type_string)

    @staticmethod
    def clear_all_breakpoints(*, blueprint_path):
        """X.clear_all_breakpoints(blueprint_path) -> int32"""
        return unreal.UnrealBridgeBlueprintLibrary.clear_all_breakpoints(blueprint_path)

    @staticmethod
    def clear_last_breakpoint_hit(*, blueprint_path):
        """X.clear_last_breakpoint_hit(blueprint_path) -> None"""
        return unreal.UnrealBridgeBlueprintLibrary.clear_last_breakpoint_hit(blueprint_path)

    @staticmethod
    def clear_project_breakpoints(*, package_path):
        """X.clear_project_breakpoints(package_path) -> int32"""
        return unreal.UnrealBridgeBlueprintLibrary.clear_project_breakpoints(package_path)

    @staticmethod
    def collapse_nodes_to_function(*, blueprint_path, source_graph_name, node_guids, new_function_name):
        """X.collapse_nodes_to_function(blueprint_path, source_graph_name, node_guids, new_function_name) -> (str, out_new_graph_name=str)"""
        return unreal.UnrealBridgeBlueprintLibrary.collapse_nodes_to_function(blueprint_path, source_graph_name, node_guids, new_function_name)

    @staticmethod
    def collapse_nodes_to_macro(*, blueprint_path, source_graph_name, node_guids, new_macro_name):
        """X.collapse_nodes_to_macro(blueprint_path, source_graph_name, node_guids, new_macro_name) -> (str, out_new_graph_name=str)"""
        return unreal.UnrealBridgeBlueprintLibrary.collapse_nodes_to_macro(blueprint_path, source_graph_name, node_guids, new_macro_name)

    @staticmethod
    def connect_graph_pins(*, blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name):
        """X.connect_graph_pins(blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.connect_graph_pins(blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name)

    @staticmethod
    def create_function_graph(*, blueprint_path, function_name):
        """X.create_function_graph(blueprint_path, function_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.create_function_graph(blueprint_path, function_name)

    @staticmethod
    def create_macro_graph(*, blueprint_path, macro_name):
        """X.create_macro_graph(blueprint_path, macro_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.create_macro_graph(blueprint_path, macro_name)

    @staticmethod
    def describe_node(*, blueprint_path, graph_name, node_guid):
        """X.describe_node(blueprint_path, graph_name, node_guid) -> BridgeNodeDescription"""
        return unreal.UnrealBridgeBlueprintLibrary.describe_node(blueprint_path, graph_name, node_guid)

    @staticmethod
    def diff_graph_snapshots(*, before_json, after_json):
        """X.diff_graph_snapshots(before_json, after_json) -> BridgeGraphDiff"""
        return unreal.UnrealBridgeBlueprintLibrary.diff_graph_snapshots(before_json, after_json)

    @staticmethod
    def disconnect_graph_pin(*, blueprint_path, graph_name, node_guid, pin_name):
        """X.disconnect_graph_pin(blueprint_path, graph_name, node_guid, pin_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.disconnect_graph_pin(blueprint_path, graph_name, node_guid, pin_name)

    @staticmethod
    def disconnect_pin_link(*, blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name):
        """X.disconnect_pin_link(blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.disconnect_pin_link(blueprint_path, graph_name, source_node_guid, source_pin_name, target_node_guid, target_pin_name)

    @staticmethod
    def duplicate_graph_node(*, blueprint_path, graph_name, node_guid, node_pos_x, node_pos_y):
        """X.duplicate_graph_node(blueprint_path, graph_name, node_guid, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.duplicate_graph_node(blueprint_path, graph_name, node_guid, node_pos_x, node_pos_y)

    @staticmethod
    def ensure_function_exec_wired(*, blueprint_path, function_name):
        """X.ensure_function_exec_wired(blueprint_path, function_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.ensure_function_exec_wired(blueprint_path, function_name)

    @staticmethod
    def find_blueprint_debug_prints(*, package_path, max_results):
        """X.find_blueprint_debug_prints(package_path, max_results) -> Array[BridgeDebugPrintSite]"""
        return unreal.UnrealBridgeBlueprintLibrary.find_blueprint_debug_prints(package_path, max_results)

    @staticmethod
    def find_cdo_variable_overrides(*, defining_blueprint_path, variable_name, package_path):
        """X.find_cdo_variable_overrides(defining_blueprint_path, variable_name, package_path) -> Array[BridgeCdoOverride]"""
        return unreal.UnrealBridgeBlueprintLibrary.find_cdo_variable_overrides(defining_blueprint_path, variable_name, package_path)

    @staticmethod
    def find_event_handler_sites(*, blueprint_path, event_name):
        """X.find_event_handler_sites(blueprint_path, event_name) -> Array[BridgeReference]"""
        return unreal.UnrealBridgeBlueprintLibrary.find_event_handler_sites(blueprint_path, event_name)

    @staticmethod
    def find_function_call_sites(*, blueprint_path, function_name):
        """X.find_function_call_sites(blueprint_path, function_name) -> Array[BridgeReference]"""
        return unreal.UnrealBridgeBlueprintLibrary.find_function_call_sites(blueprint_path, function_name)

    @staticmethod
    def find_function_call_sites_global(*, function_name, owning_class_filter, package_path, max_results):
        """X.find_function_call_sites_global(function_name, owning_class_filter, package_path, max_results) -> Array[BridgeGlobalReference]"""
        return unreal.UnrealBridgeBlueprintLibrary.find_function_call_sites_global(function_name, owning_class_filter, package_path, max_results)

    @staticmethod
    def find_variable_references(*, blueprint_path, variable_name):
        """X.find_variable_references(blueprint_path, variable_name) -> Array[BridgeReference]"""
        return unreal.UnrealBridgeBlueprintLibrary.find_variable_references(blueprint_path, variable_name)

    @staticmethod
    def get_blueprint_class_hierarchy(*, blueprint_path):
        """X.get_blueprint_class_hierarchy(blueprint_path) -> Array[BridgeClassInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_class_hierarchy(blueprint_path)

    @staticmethod
    def get_blueprint_components(*, blueprint_path):
        """X.get_blueprint_components(blueprint_path) -> Array[BridgeComponentInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_components(blueprint_path)

    @staticmethod
    def get_blueprint_functions(*, blueprint_path, include_inherited=False):
        """X.get_blueprint_functions(blueprint_path, include_inherited=False) -> Array[BridgeFunctionInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_functions(blueprint_path, include_inherited)

    @staticmethod
    def get_blueprint_interfaces(*, blueprint_path):
        """X.get_blueprint_interfaces(blueprint_path) -> Array[BridgeInterfaceInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_interfaces(blueprint_path)

    @staticmethod
    def get_blueprint_overview(*, blueprint_path):
        """X.get_blueprint_overview(blueprint_path) -> BridgeBlueprintOverview or None"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_overview(blueprint_path)

    @staticmethod
    def get_blueprint_parent_class(*, blueprint_path):
        """X.get_blueprint_parent_class(blueprint_path) -> BridgeClassInfo or None"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_parent_class(blueprint_path)

    @staticmethod
    def get_blueprint_summary(*, blueprint_path):
        """X.get_blueprint_summary(blueprint_path) -> BridgeBlueprintSummary or None"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_summary(blueprint_path)

    @staticmethod
    def get_blueprint_variables(*, blueprint_path, include_inherited=False):
        """X.get_blueprint_variables(blueprint_path, include_inherited=False) -> Array[BridgeVariableInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_blueprint_variables(blueprint_path, include_inherited)

    @staticmethod
    def get_breakpoints(*, blueprint_path):
        """X.get_breakpoints(blueprint_path) -> Array[BridgeBreakpointInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_breakpoints(blueprint_path)

    @staticmethod
    def get_compile_errors(*, blueprint_path):
        """X.get_compile_errors(blueprint_path) -> Array[BridgeCompileMessage]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_compile_errors(blueprint_path)

    @staticmethod
    def get_component_property_values(*, blueprint_path, component_name):
        """X.get_component_property_values(blueprint_path, component_name) -> Array[BridgePropertyValue]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_component_property_values(blueprint_path, component_name)

    @staticmethod
    def get_editor_focus_state():
        """X.get_editor_focus_state() -> BridgeEditorFocusState"""
        return unreal.UnrealBridgeBlueprintLibrary.get_editor_focus_state()

    @staticmethod
    def get_event_dispatchers(*, blueprint_path):
        """X.get_event_dispatchers(blueprint_path) -> Array[BridgeEventDispatcherInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_event_dispatchers(blueprint_path)

    @staticmethod
    def get_function_call_graph(*, blueprint_path, function_name):
        """X.get_function_call_graph(blueprint_path, function_name) -> Array[BridgeCallEdge]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_function_call_graph(blueprint_path, function_name)

    @staticmethod
    def get_function_execution_flow(*, blueprint_path, function_name):
        """X.get_function_execution_flow(blueprint_path, function_name) -> Array[BridgeExecStep]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_function_execution_flow(blueprint_path, function_name)

    @staticmethod
    def get_function_local_variables(*, blueprint_path, function_name):
        """X.get_function_local_variables(blueprint_path, function_name) -> Array[BridgeVariableInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_function_local_variables(blueprint_path, function_name)

    @staticmethod
    def get_function_nodes(*, blueprint_path, function_name, node_type_filter):
        """X.get_function_nodes(blueprint_path, function_name, node_type_filter) -> Array[BridgeNodeInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_function_nodes(blueprint_path, function_name, node_type_filter)

    @staticmethod
    def get_function_signature(*, class_path, function_name):
        """X.get_function_signature(class_path, function_name) -> BridgeFunctionSignature"""
        return unreal.UnrealBridgeBlueprintLibrary.get_function_signature(class_path, function_name)

    @staticmethod
    def get_function_summary(*, blueprint_path, function_name):
        """X.get_function_summary(blueprint_path, function_name) -> BridgeFunctionSemantics or None"""
        return unreal.UnrealBridgeBlueprintLibrary.get_function_summary(blueprint_path, function_name)

    @staticmethod
    def get_graph_fingerprint(*, blueprint_path, graph_name):
        """X.get_graph_fingerprint(blueprint_path, graph_name) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.get_graph_fingerprint(blueprint_path, graph_name)

    @staticmethod
    def get_graph_names(*, blueprint_path):
        """X.get_graph_names(blueprint_path) -> Array[BridgeGraphInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_graph_names(blueprint_path)

    @staticmethod
    def get_last_breakpoint_hit(*, blueprint_path):
        """X.get_last_breakpoint_hit(blueprint_path) -> BridgeBreakpointHit"""
        return unreal.UnrealBridgeBlueprintLibrary.get_last_breakpoint_hit(blueprint_path)

    @staticmethod
    def get_node_layout(*, blueprint_path, graph_name, node_guid):
        """X.get_node_layout(blueprint_path, graph_name, node_guid) -> BridgeNodeLayout"""
        return unreal.UnrealBridgeBlueprintLibrary.get_node_layout(blueprint_path, graph_name, node_guid)

    @staticmethod
    def get_node_pin_connections(*, blueprint_path, function_name):
        """X.get_node_pin_connections(blueprint_path, function_name) -> Array[BridgePinConnection]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_node_pin_connections(blueprint_path, function_name)

    @staticmethod
    def get_node_pin_layouts(*, blueprint_path, graph_name, node_guid):
        """X.get_node_pin_layouts(blueprint_path, graph_name, node_guid) -> Array[BridgePinLayout]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_node_pin_layouts(blueprint_path, graph_name, node_guid)

    @staticmethod
    def get_node_pins(*, blueprint_path, graph_name, node_guid):
        """X.get_node_pins(blueprint_path, graph_name, node_guid) -> Array[BridgePinInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_node_pins(blueprint_path, graph_name, node_guid)

    @staticmethod
    def get_pie_node_coverage(*, blueprint_path):
        """X.get_pie_node_coverage(blueprint_path) -> Array[BridgeNodeCoverageEntry]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_pie_node_coverage(blueprint_path)

    @staticmethod
    def get_pin_default_value(*, blueprint_path, graph_name, node_guid, pin_name):
        """X.get_pin_default_value(blueprint_path, graph_name, node_guid, pin_name) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.get_pin_default_value(blueprint_path, graph_name, node_guid, pin_name)

    @staticmethod
    def get_rendered_node_info(*, blueprint_path, graph_name):
        """X.get_rendered_node_info(blueprint_path, graph_name) -> Array[BridgeRenderedNode]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_rendered_node_info(blueprint_path, graph_name)

    @staticmethod
    def get_timeline_info(*, blueprint_path):
        """X.get_timeline_info(blueprint_path) -> Array[BridgeTimelineInfo]"""
        return unreal.UnrealBridgeBlueprintLibrary.get_timeline_info(blueprint_path)

    @staticmethod
    def implement_interface_function(*, blueprint_path, interface_path, function_name):
        """X.implement_interface_function(blueprint_path, interface_path, function_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.implement_interface_function(blueprint_path, interface_path, function_name)

    @staticmethod
    def insert_node_on_wire(*, blueprint_path, graph_name, src_node_guid, src_pin_name, dst_node_guid, dst_pin_name, insert_node_guid, insert_in_pin_name, insert_out_pin_name):
        """X.insert_node_on_wire(blueprint_path, graph_name, src_node_guid, src_pin_name, dst_node_guid, dst_pin_name, insert_node_guid, insert_in_pin_name, insert_out_pin_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.insert_node_on_wire(blueprint_path, graph_name, src_node_guid, src_pin_name, dst_node_guid, dst_pin_name, insert_node_guid, insert_in_pin_name, insert_out_pin_name)

    @staticmethod
    def invoke_blueprint_function(*, blueprint_path, function_name, args_json):
        """X.invoke_blueprint_function(blueprint_path, function_name, args_json) -> (out_result_json=str, out_error=str) or None"""
        return unreal.UnrealBridgeBlueprintLibrary.invoke_blueprint_function(blueprint_path, function_name, args_json)

    @staticmethod
    def lint_blueprint(*, blueprint_path, severity_filter, oversized_function_threshold, long_exec_chain_threshold, large_graph_threshold):
        """X.lint_blueprint(blueprint_path, severity_filter, oversized_function_threshold, long_exec_chain_threshold, large_graph_threshold) -> Array[BridgeLintIssue]"""
        return unreal.UnrealBridgeBlueprintLibrary.lint_blueprint(blueprint_path, severity_filter, oversized_function_threshold, long_exec_chain_threshold, large_graph_threshold)

    @staticmethod
    def list_spawnable_actions(*, blueprint_path, graph_name, keyword, category_contains, owning_class_path, node_type, max_results):
        """X.list_spawnable_actions(blueprint_path, graph_name, keyword, category_contains, owning_class_path, node_type, max_results) -> Array[BridgeSpawnableAction]"""
        return unreal.UnrealBridgeBlueprintLibrary.list_spawnable_actions(blueprint_path, graph_name, keyword, category_contains, owning_class_path, node_type, max_results)

    @staticmethod
    def open_function_graph_for_render(*, blueprint_path, graph_name):
        """X.open_function_graph_for_render(blueprint_path, graph_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.open_function_graph_for_render(blueprint_path, graph_name)

    @staticmethod
    def predict_node_size(*, kind, param_a, param_b, param_int):
        """X.predict_node_size(kind, param_a, param_b, param_int) -> BridgeNodeSizeEstimate"""
        return unreal.UnrealBridgeBlueprintLibrary.predict_node_size(kind, param_a, param_b, param_int)

    @staticmethod
    def promote_pin_to_variable(*, blueprint_path, graph_name, node_guid, pin_name, variable_name, to_member_variable):
        """X.promote_pin_to_variable(blueprint_path, graph_name, node_guid, pin_name, variable_name, to_member_variable) -> (out_new_variable_name=str, out_new_node_guid=str) or None"""
        return unreal.UnrealBridgeBlueprintLibrary.promote_pin_to_variable(blueprint_path, graph_name, node_guid, pin_name, variable_name, to_member_variable)

    @staticmethod
    def recombine_struct_pin(*, blueprint_path, graph_name, node_guid, sub_pin_name):
        """X.recombine_struct_pin(blueprint_path, graph_name, node_guid, sub_pin_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.recombine_struct_pin(blueprint_path, graph_name, node_guid, sub_pin_name)

    @staticmethod
    def remove_blueprint_interface(*, blueprint_path, interface_name_or_path):
        """X.remove_blueprint_interface(blueprint_path, interface_name_or_path) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_blueprint_interface(blueprint_path, interface_name_or_path)

    @staticmethod
    def remove_blueprint_variable(*, blueprint_path, variable_name):
        """X.remove_blueprint_variable(blueprint_path, variable_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_blueprint_variable(blueprint_path, variable_name)

    @staticmethod
    def remove_breakpoint(*, blueprint_path, graph_name, node_guid):
        """X.remove_breakpoint(blueprint_path, graph_name, node_guid) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_breakpoint(blueprint_path, graph_name, node_guid)

    @staticmethod
    def remove_component(*, blueprint_path, component_name):
        """X.remove_component(blueprint_path, component_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_component(blueprint_path, component_name)

    @staticmethod
    def remove_event_dispatcher(*, blueprint_path, dispatcher_name):
        """X.remove_event_dispatcher(blueprint_path, dispatcher_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_event_dispatcher(blueprint_path, dispatcher_name)

    @staticmethod
    def remove_function_graph(*, blueprint_path, function_name):
        """X.remove_function_graph(blueprint_path, function_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_function_graph(blueprint_path, function_name)

    @staticmethod
    def remove_function_local_variable(*, blueprint_path, function_name, variable_name):
        """X.remove_function_local_variable(blueprint_path, function_name, variable_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_function_local_variable(blueprint_path, function_name, variable_name)

    @staticmethod
    def remove_function_parameter(*, blueprint_path, function_name, param_name):
        """X.remove_function_parameter(blueprint_path, function_name, param_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_function_parameter(blueprint_path, function_name, param_name)

    @staticmethod
    def remove_graph_node(*, blueprint_path, graph_name, node_guid):
        """X.remove_graph_node(blueprint_path, graph_name, node_guid) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_graph_node(blueprint_path, graph_name, node_guid)

    @staticmethod
    def remove_macro_graph(*, blueprint_path, macro_name):
        """X.remove_macro_graph(blueprint_path, macro_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.remove_macro_graph(blueprint_path, macro_name)

    @staticmethod
    def rename_blueprint_variable(*, blueprint_path, old_name, new_name):
        """X.rename_blueprint_variable(blueprint_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.rename_blueprint_variable(blueprint_path, old_name, new_name)

    @staticmethod
    def rename_event_dispatcher(*, blueprint_path, old_name, new_name):
        """X.rename_event_dispatcher(blueprint_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.rename_event_dispatcher(blueprint_path, old_name, new_name)

    @staticmethod
    def rename_function_global(*, defining_blueprint_path, old_name, new_name, package_path):
        """X.rename_function_global(defining_blueprint_path, old_name, new_name, package_path) -> BridgeRenameReport"""
        return unreal.UnrealBridgeBlueprintLibrary.rename_function_global(defining_blueprint_path, old_name, new_name, package_path)

    @staticmethod
    def rename_function_graph(*, blueprint_path, old_name, new_name):
        """X.rename_function_graph(blueprint_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.rename_function_graph(blueprint_path, old_name, new_name)

    @staticmethod
    def rename_function_local_variable(*, blueprint_path, function_name, old_name, new_name):
        """X.rename_function_local_variable(blueprint_path, function_name, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.rename_function_local_variable(blueprint_path, function_name, old_name, new_name)

    @staticmethod
    def rename_member_variable_global(*, defining_blueprint_path, old_name, new_name, package_path):
        """X.rename_member_variable_global(defining_blueprint_path, old_name, new_name, package_path) -> BridgeRenameReport"""
        return unreal.UnrealBridgeBlueprintLibrary.rename_member_variable_global(defining_blueprint_path, old_name, new_name, package_path)

    @staticmethod
    def reorder_component(*, blueprint_path, component_name, new_index):
        """X.reorder_component(blueprint_path, component_name, new_index) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.reorder_component(blueprint_path, component_name, new_index)

    @staticmethod
    def reorder_function_parameter(*, blueprint_path, function_name, param_name, new_index):
        """X.reorder_function_parameter(blueprint_path, function_name, param_name, new_index) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.reorder_function_parameter(blueprint_path, function_name, param_name, new_index)

    @staticmethod
    def reparent_blueprint(*, blueprint_path, new_parent_path):
        """X.reparent_blueprint(blueprint_path, new_parent_path) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.reparent_blueprint(blueprint_path, new_parent_path)

    @staticmethod
    def reparent_component(*, blueprint_path, component_name, new_parent_name):
        """X.reparent_component(blueprint_path, component_name, new_parent_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.reparent_component(blueprint_path, component_name, new_parent_name)

    @staticmethod
    def replace_node_preserving_connections(*, blueprint_path, graph_name, old_node_guid, new_node_class_path):
        """X.replace_node_preserving_connections(blueprint_path, graph_name, old_node_guid, new_node_class_path) -> BridgeReplaceNodeReport"""
        return unreal.UnrealBridgeBlueprintLibrary.replace_node_preserving_connections(blueprint_path, graph_name, old_node_guid, new_node_class_path)

    @staticmethod
    def resume_script_execution():
        """X.resume_script_execution() -> None"""
        return unreal.UnrealBridgeBlueprintLibrary.resume_script_execution()

    @staticmethod
    def search_blueprint_nodes(*, blueprint_path, query, node_type_filter):
        """X.search_blueprint_nodes(blueprint_path, query, node_type_filter) -> Array[BridgeNodeSearchResult]"""
        return unreal.UnrealBridgeBlueprintLibrary.search_blueprint_nodes(blueprint_path, query, node_type_filter)

    @staticmethod
    def set_blueprint_debug_object(*, blueprint_path, actor_name):
        """X.set_blueprint_debug_object(blueprint_path, actor_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_blueprint_debug_object(blueprint_path, actor_name)

    @staticmethod
    def set_blueprint_metadata(*, blueprint_path, display_name, description, category="UnrealBridge|Blueprint", namespace):
        """X.set_blueprint_metadata(blueprint_path, display_name, description, category="UnrealBridge|Blueprint", namespace) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_blueprint_metadata(blueprint_path, display_name, description, category, namespace)

    @staticmethod
    def set_blueprint_variable_default(*, blueprint_path, variable_name, value):
        """X.set_blueprint_variable_default(blueprint_path, variable_name, value) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_blueprint_variable_default(blueprint_path, variable_name, value)

    @staticmethod
    def set_comment_box_color(*, blueprint_path, graph_name, node_guid, color_or_preset):
        """X.set_comment_box_color(blueprint_path, graph_name, node_guid, color_or_preset) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_comment_box_color(blueprint_path, graph_name, node_guid, color_or_preset)

    @staticmethod
    def set_component_property(*, blueprint_path, component_name, property_name, value):
        """X.set_component_property(blueprint_path, component_name, property_name, value) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_component_property(blueprint_path, component_name, property_name, value)

    @staticmethod
    def set_data_table_row_handle_pin(*, blueprint_path, graph_name, node_guid, pin_name, data_table_path, row_name):
        """X.set_data_table_row_handle_pin(blueprint_path, graph_name, node_guid, pin_name, data_table_path, row_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_data_table_row_handle_pin(blueprint_path, graph_name, node_guid, pin_name, data_table_path, row_name)

    @staticmethod
    def set_function_local_variable_default(*, blueprint_path, function_name, variable_name, value):
        """X.set_function_local_variable_default(blueprint_path, function_name, variable_name, value) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_function_local_variable_default(blueprint_path, function_name, variable_name, value)

    @staticmethod
    def set_function_metadata(*, blueprint_path, function_name, pure, const, category="UnrealBridge|Blueprint", access_specifier):
        """X.set_function_metadata(blueprint_path, function_name, pure, const, category="UnrealBridge|Blueprint", access_specifier) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_function_metadata(blueprint_path, function_name, pure, const, category, access_specifier)

    @staticmethod
    def set_graph_node_position(*, blueprint_path, graph_name, node_guid, node_pos_x, node_pos_y):
        """X.set_graph_node_position(blueprint_path, graph_name, node_guid, node_pos_x, node_pos_y) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_graph_node_position(blueprint_path, graph_name, node_guid, node_pos_x, node_pos_y)

    @staticmethod
    def set_node_color(*, blueprint_path, graph_name, node_guid, color_or_preset):
        """X.set_node_color(blueprint_path, graph_name, node_guid, color_or_preset) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_node_color(blueprint_path, graph_name, node_guid, color_or_preset)

    @staticmethod
    def set_node_enabled(*, blueprint_path, graph_name, node_guid, enabled_state):
        """X.set_node_enabled(blueprint_path, graph_name, node_guid, enabled_state) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_node_enabled(blueprint_path, graph_name, node_guid, enabled_state)

    @staticmethod
    def set_pin_default_value(*, blueprint_path, graph_name, node_guid, pin_name, new_default_value):
        """X.set_pin_default_value(blueprint_path, graph_name, node_guid, pin_name, new_default_value) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_pin_default_value(blueprint_path, graph_name, node_guid, pin_name, new_default_value)

    @staticmethod
    def set_timeline_properties(*, blueprint_path, timeline_name, length, auto_play, loop, replicated, ignore_time_dilation):
        """X.set_timeline_properties(blueprint_path, timeline_name, length, auto_play, loop, replicated, ignore_time_dilation) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_timeline_properties(blueprint_path, timeline_name, length, auto_play, loop, replicated, ignore_time_dilation)

    @staticmethod
    def set_variable_type(*, blueprint_path, variable_name, new_type_string):
        """X.set_variable_type(blueprint_path, variable_name, new_type_string) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.set_variable_type(blueprint_path, variable_name, new_type_string)

    @staticmethod
    def snapshot_graph_json(*, blueprint_path, graph_name):
        """X.snapshot_graph_json(blueprint_path, graph_name) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.snapshot_graph_json(blueprint_path, graph_name)

    @staticmethod
    def spawn_node_by_action_key(*, blueprint_path, graph_name, action_key, node_pos_x, node_pos_y):
        """X.spawn_node_by_action_key(blueprint_path, graph_name, action_key, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.spawn_node_by_action_key(blueprint_path, graph_name, action_key, node_pos_x, node_pos_y)

    @staticmethod
    def split_struct_pin(*, blueprint_path, graph_name, node_guid, pin_name):
        """X.split_struct_pin(blueprint_path, graph_name, node_guid, pin_name) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.split_struct_pin(blueprint_path, graph_name, node_guid, pin_name)

    @staticmethod
    def straighten_exec_chain(*, blueprint_path, graph_name, start_node_guid, start_exec_pin_name):
        """X.straighten_exec_chain(blueprint_path, graph_name, start_node_guid, start_exec_pin_name) -> int32"""
        return unreal.UnrealBridgeBlueprintLibrary.straighten_exec_chain(blueprint_path, graph_name, start_node_guid, start_exec_pin_name)

    @staticmethod
    def update_comment_box(*, blueprint_path, graph_name, comment_guid, node_guids, text):
        """X.update_comment_box(blueprint_path, graph_name, comment_guid, node_guids, text) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.update_comment_box(blueprint_path, graph_name, comment_guid, node_guids, text)

    @staticmethod
    def wrap_nodes_in_comment_box(*, blueprint_path, graph_name, node_guids, text):
        """X.wrap_nodes_in_comment_box(blueprint_path, graph_name, node_guids, text) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.wrap_nodes_in_comment_box(blueprint_path, graph_name, node_guids, text)


class Chooser:
    """Wraps unreal.UnrealBridgeChooserLibrary (kwargs-only)."""

    @staticmethod
    def add_chooser_column_bool(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_bool(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_bool(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_by_struct_path(*, chooser_table_path, column_struct_path, binding_property_chain, context_index):
        """X.add_chooser_column_by_struct_path(chooser_table_path, column_struct_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_by_struct_path(chooser_table_path, column_struct_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_enum(*, chooser_table_path, binding_property_chain, enum_path, context_index):
        """X.add_chooser_column_enum(chooser_table_path, binding_property_chain, enum_path, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_enum(chooser_table_path, binding_property_chain, enum_path, context_index)

    @staticmethod
    def add_chooser_column_float_range(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_float_range(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_float_range(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_gameplay_tag(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_gameplay_tag(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_gameplay_tag(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_object(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_object(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_object(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_output_float(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_output_float(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_output_float(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_output_object(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_output_object(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_output_object(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_randomize(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_randomize(chooser_table_path, binding_property_chain, context_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_randomize(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_row(*, chooser_table_path):
        """X.add_chooser_row(chooser_table_path) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_row(chooser_table_path)

    @staticmethod
    def clear_chooser_fallback(*, chooser_table_path):
        """X.clear_chooser_fallback(chooser_table_path) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.clear_chooser_fallback(chooser_table_path)

    @staticmethod
    def clear_chooser_row_result(*, chooser_table_path, row_index):
        """X.clear_chooser_row_result(chooser_table_path, row_index) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.clear_chooser_row_result(chooser_table_path, row_index)

    @staticmethod
    def evaluate_chooser_multi_with_context_object(*, chooser_table_path, context_object_path):
        """X.evaluate_chooser_multi_with_context_object(chooser_table_path, context_object_path) -> Array[BridgeCHTRowResult]"""
        return unreal.UnrealBridgeChooserLibrary.evaluate_chooser_multi_with_context_object(chooser_table_path, context_object_path)

    @staticmethod
    def evaluate_chooser_with_context_object(*, chooser_table_path, context_object_path):
        """X.evaluate_chooser_with_context_object(chooser_table_path, context_object_path) -> BridgeCHTEvaluation"""
        return unreal.UnrealBridgeChooserLibrary.evaluate_chooser_with_context_object(chooser_table_path, context_object_path)

    @staticmethod
    def get_chooser_cell_raw(*, chooser_table_path, column_index, row_index):
        """X.get_chooser_cell_raw(chooser_table_path, column_index, row_index) -> str"""
        return unreal.UnrealBridgeChooserLibrary.get_chooser_cell_raw(chooser_table_path, column_index, row_index)

    @staticmethod
    def get_chooser_info(*, chooser_table_path):
        """X.get_chooser_info(chooser_table_path) -> BridgeCHTInfo"""
        return unreal.UnrealBridgeChooserLibrary.get_chooser_info(chooser_table_path)

    @staticmethod
    def get_chooser_row_result(*, chooser_table_path, row_index):
        """X.get_chooser_row_result(chooser_table_path, row_index) -> BridgeCHTRowResult"""
        return unreal.UnrealBridgeChooserLibrary.get_chooser_row_result(chooser_table_path, row_index)

    @staticmethod
    def insert_chooser_row(*, chooser_table_path, before_row):
        """X.insert_chooser_row(chooser_table_path, before_row) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.insert_chooser_row(chooser_table_path, before_row)

    @staticmethod
    def list_chooser_columns(*, chooser_table_path):
        """X.list_chooser_columns(chooser_table_path) -> Array[BridgeCHTColumn]"""
        return unreal.UnrealBridgeChooserLibrary.list_chooser_columns(chooser_table_path)

    @staticmethod
    def list_chooser_rows(*, chooser_table_path):
        """X.list_chooser_rows(chooser_table_path) -> Array[BridgeCHTRow]"""
        return unreal.UnrealBridgeChooserLibrary.list_chooser_rows(chooser_table_path)

    @staticmethod
    def list_possible_results(*, chooser_table_path):
        """X.list_possible_results(chooser_table_path) -> Array[BridgeCHTRowResult]"""
        return unreal.UnrealBridgeChooserLibrary.list_possible_results(chooser_table_path)

    @staticmethod
    def remove_chooser_column(*, chooser_table_path, column_index):
        """X.remove_chooser_column(chooser_table_path, column_index) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.remove_chooser_column(chooser_table_path, column_index)

    @staticmethod
    def remove_chooser_row(*, chooser_table_path, row_index):
        """X.remove_chooser_row(chooser_table_path, row_index) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.remove_chooser_row(chooser_table_path, row_index)

    @staticmethod
    def set_chooser_cell_raw(*, chooser_table_path, column_index, row_index, t3d_value):
        """X.set_chooser_cell_raw(chooser_table_path, column_index, row_index, t3d_value) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_cell_raw(chooser_table_path, column_index, row_index, t3d_value)

    @staticmethod
    def set_chooser_column_disabled(*, chooser_table_path, column_index, disabled):
        """X.set_chooser_column_disabled(chooser_table_path, column_index, disabled) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_column_disabled(chooser_table_path, column_index, disabled)

    @staticmethod
    def set_chooser_fallback_asset(*, chooser_table_path, asset_path):
        """X.set_chooser_fallback_asset(chooser_table_path, asset_path) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_fallback_asset(chooser_table_path, asset_path)

    @staticmethod
    def set_chooser_row_disabled(*, chooser_table_path, row_index, disabled):
        """X.set_chooser_row_disabled(chooser_table_path, row_index, disabled) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_row_disabled(chooser_table_path, row_index, disabled)

    @staticmethod
    def set_chooser_row_result_asset(*, chooser_table_path, row_index, asset_path):
        """X.set_chooser_row_result_asset(chooser_table_path, row_index, asset_path) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_row_result_asset(chooser_table_path, row_index, asset_path)

    @staticmethod
    def set_chooser_row_result_class(*, chooser_table_path, row_index, class_path):
        """X.set_chooser_row_result_class(chooser_table_path, row_index, class_path) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_row_result_class(chooser_table_path, row_index, class_path)

    @staticmethod
    def set_chooser_row_result_evaluate_chooser(*, chooser_table_path, row_index, sub_chooser_path):
        """X.set_chooser_row_result_evaluate_chooser(chooser_table_path, row_index, sub_chooser_path) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_row_result_evaluate_chooser(chooser_table_path, row_index, sub_chooser_path)


class Curve:
    """Wraps unreal.UnrealBridgeCurveLibrary (kwargs-only)."""

    @staticmethod
    def add_curve_key(*, curve_path, channel_index, key):
        """X.add_curve_key(curve_path, channel_index, key) -> int32"""
        return unreal.UnrealBridgeCurveLibrary.add_curve_key(curve_path, channel_index, key)

    @staticmethod
    def add_curve_table_row(*, curve_table_path, row_name, keys):
        """X.add_curve_table_row(curve_table_path, row_name, keys) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.add_curve_table_row(curve_table_path, row_name, keys)

    @staticmethod
    def auto_set_curve_tangents(*, curve_path, tension):
        """X.auto_set_curve_tangents(curve_path, tension) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.auto_set_curve_tangents(curve_path, tension)

    @staticmethod
    def clear_curve_keys(*, curve_path, channel_index):
        """X.clear_curve_keys(curve_path, channel_index) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.clear_curve_keys(curve_path, channel_index)

    @staticmethod
    def evaluate_curve(*, curve_path, channel_index, times):
        """X.evaluate_curve(curve_path, channel_index, times) -> Array[float]"""
        return unreal.UnrealBridgeCurveLibrary.evaluate_curve(curve_path, channel_index, times)

    @staticmethod
    def evaluate_curve_table_row(*, curve_table_path, row_name, times):
        """X.evaluate_curve_table_row(curve_table_path, row_name, times) -> Array[float]"""
        return unreal.UnrealBridgeCurveLibrary.evaluate_curve_table_row(curve_table_path, row_name, times)

    @staticmethod
    def get_curve_as_json_string(*, curve_path):
        """X.get_curve_as_json_string(curve_path) -> str"""
        return unreal.UnrealBridgeCurveLibrary.get_curve_as_json_string(curve_path)

    @staticmethod
    def get_curve_info(*, curve_path):
        """X.get_curve_info(curve_path) -> BridgeCurveInfo"""
        return unreal.UnrealBridgeCurveLibrary.get_curve_info(curve_path)

    @staticmethod
    def get_curve_keys(*, curve_path, channel_index):
        """X.get_curve_keys(curve_path, channel_index) -> Array[BridgeRichCurveKey]"""
        return unreal.UnrealBridgeCurveLibrary.get_curve_keys(curve_path, channel_index)

    @staticmethod
    def get_curve_table_info(*, curve_table_path):
        """X.get_curve_table_info(curve_table_path) -> BridgeCurveTableInfo"""
        return unreal.UnrealBridgeCurveLibrary.get_curve_table_info(curve_table_path)

    @staticmethod
    def get_curve_table_row_keys(*, curve_table_path, row_name):
        """X.get_curve_table_row_keys(curve_table_path, row_name) -> Array[BridgeRichCurveKey]"""
        return unreal.UnrealBridgeCurveLibrary.get_curve_table_row_keys(curve_table_path, row_name)

    @staticmethod
    def remove_curve_key_by_index(*, curve_path, channel_index, index):
        """X.remove_curve_key_by_index(curve_path, channel_index, index) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.remove_curve_key_by_index(curve_path, channel_index, index)

    @staticmethod
    def remove_curve_table_row(*, curve_table_path, row_name):
        """X.remove_curve_table_row(curve_table_path, row_name) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.remove_curve_table_row(curve_table_path, row_name)

    @staticmethod
    def rename_curve_table_row(*, curve_table_path, old_row_name, new_row_name):
        """X.rename_curve_table_row(curve_table_path, old_row_name, new_row_name) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.rename_curve_table_row(curve_table_path, old_row_name, new_row_name)

    @staticmethod
    def sample_curve_uniform(*, curve_path, channel_index, start_time, end_time, num_samples):
        """X.sample_curve_uniform(curve_path, channel_index, start_time, end_time, num_samples) -> Array[float]"""
        return unreal.UnrealBridgeCurveLibrary.sample_curve_uniform(curve_path, channel_index, start_time, end_time, num_samples)

    @staticmethod
    def set_curve_infinity_extrap(*, curve_path, pre_infinity_extrap, post_infinity_extrap):
        """X.set_curve_infinity_extrap(curve_path, pre_infinity_extrap, post_infinity_extrap) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.set_curve_infinity_extrap(curve_path, pre_infinity_extrap, post_infinity_extrap)

    @staticmethod
    def set_curve_key_tangents(*, curve_path, channel_index, index, tangent_mode, tangent_weight_mode, arrive_tangent, leave_tangent, arrive_tangent_weight, leave_tangent_weight):
        """X.set_curve_key_tangents(curve_path, channel_index, index, tangent_mode, tangent_weight_mode, arrive_tangent, leave_tangent, arrive_tangent_weight, leave_tangent_weight) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.set_curve_key_tangents(curve_path, channel_index, index, tangent_mode, tangent_weight_mode, arrive_tangent, leave_tangent, arrive_tangent_weight, leave_tangent_weight)

    @staticmethod
    def set_curve_keys(*, curve_path, channel_index, keys):
        """X.set_curve_keys(curve_path, channel_index, keys) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.set_curve_keys(curve_path, channel_index, keys)

    @staticmethod
    def set_curve_table_row_keys(*, curve_table_path, row_name, keys):
        """X.set_curve_table_row_keys(curve_table_path, row_name, keys) -> bool"""
        return unreal.UnrealBridgeCurveLibrary.set_curve_table_row_keys(curve_table_path, row_name, keys)


class DataTable:
    """Wraps unreal.UnrealBridgeDataTableLibrary (kwargs-only)."""

    @staticmethod
    def add_data_table_row(*, data_table_path, row_name, field_values):
        """X.add_data_table_row(data_table_path, row_name, field_values) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.add_data_table_row(data_table_path, row_name, field_values)

    @staticmethod
    def clear_data_table(*, data_table_path):
        """X.clear_data_table(data_table_path) -> int32"""
        return unreal.UnrealBridgeDataTableLibrary.clear_data_table(data_table_path)

    @staticmethod
    def copy_data_table_rows(*, source_data_table_path, dest_data_table_path, row_names, overwrite):
        """X.copy_data_table_rows(source_data_table_path, dest_data_table_path, row_names, overwrite) -> int32"""
        return unreal.UnrealBridgeDataTableLibrary.copy_data_table_rows(source_data_table_path, dest_data_table_path, row_names, overwrite)

    @staticmethod
    def diff_data_table_rows(*, data_table_path_a, row_name_a, data_table_path_b, row_name_b):
        """X.diff_data_table_rows(data_table_path_a, row_name_a, data_table_path_b, row_name_b) -> Array[str]"""
        return unreal.UnrealBridgeDataTableLibrary.diff_data_table_rows(data_table_path_a, row_name_a, data_table_path_b, row_name_b)

    @staticmethod
    def does_data_table_row_exist(*, data_table_path, row_name):
        """X.does_data_table_row_exist(data_table_path, row_name) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.does_data_table_row_exist(data_table_path, row_name)

    @staticmethod
    def duplicate_data_table_row(*, data_table_path, source_row_name, new_row_name):
        """X.duplicate_data_table_row(data_table_path, source_row_name, new_row_name) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.duplicate_data_table_row(data_table_path, source_row_name, new_row_name)

    @staticmethod
    def export_data_table_to_csv(*, data_table_path, out_csv_file_path):
        """X.export_data_table_to_csv(data_table_path, out_csv_file_path) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.export_data_table_to_csv(data_table_path, out_csv_file_path)

    @staticmethod
    def export_data_table_to_json(*, data_table_path, out_json_file_path):
        """X.export_data_table_to_json(data_table_path, out_json_file_path) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.export_data_table_to_json(data_table_path, out_json_file_path)

    @staticmethod
    def find_data_table_rows_by_field_value(*, data_table_path, field_name, value, case_sensitive):
        """X.find_data_table_rows_by_field_value(data_table_path, field_name, value, case_sensitive) -> Array[str]"""
        return unreal.UnrealBridgeDataTableLibrary.find_data_table_rows_by_field_value(data_table_path, field_name, value, case_sensitive)

    @staticmethod
    def get_data_table_as_json_string(*, data_table_path):
        """X.get_data_table_as_json_string(data_table_path) -> str"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_as_json_string(data_table_path)

    @staticmethod
    def get_data_table_column(*, data_table_path, field_name):
        """X.get_data_table_column(data_table_path, field_name) -> Array[str]"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_column(data_table_path, field_name)

    @staticmethod
    def get_data_table_column_types(*, data_table_path):
        """X.get_data_table_column_types(data_table_path) -> Array[BridgeDataTableColumn]"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_column_types(data_table_path)

    @staticmethod
    def get_data_table_row(*, data_table_path, row_name):
        """X.get_data_table_row(data_table_path, row_name) -> BridgeDataTableRow"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row(data_table_path, row_name)

    @staticmethod
    def get_data_table_row_as_json_string(*, data_table_path, row_name):
        """X.get_data_table_row_as_json_string(data_table_path, row_name) -> str"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row_as_json_string(data_table_path, row_name)

    @staticmethod
    def get_data_table_row_as_map(*, data_table_path, row_name):
        """X.get_data_table_row_as_map(data_table_path, row_name) -> Map[str, str]"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row_as_map(data_table_path, row_name)

    @staticmethod
    def get_data_table_row_defaults(*, data_table_path):
        """X.get_data_table_row_defaults(data_table_path) -> Map[str, str]"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row_defaults(data_table_path)

    @staticmethod
    def get_data_table_row_field(*, data_table_path, row_name, field_name):
        """X.get_data_table_row_field(data_table_path, row_name, field_name) -> str"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row_field(data_table_path, row_name, field_name)

    @staticmethod
    def get_data_table_row_names(*, data_table_path):
        """X.get_data_table_row_names(data_table_path) -> Array[str]"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row_names(data_table_path)

    @staticmethod
    def get_data_table_row_struct_path(*, data_table_path):
        """X.get_data_table_row_struct_path(data_table_path) -> str"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_row_struct_path(data_table_path)

    @staticmethod
    def get_data_table_rows(*, data_table_path):
        """X.get_data_table_rows(data_table_path) -> BridgeDataTableInfo"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_rows(data_table_path)

    @staticmethod
    def get_data_table_rows_as_json_array(*, data_table_path, row_filter, column_filter):
        """X.get_data_table_rows_as_json_array(data_table_path, row_filter, column_filter) -> str"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_rows_as_json_array(data_table_path, row_filter, column_filter)

    @staticmethod
    def get_data_table_rows_filtered(*, data_table_path, row_filter, column_filter):
        """X.get_data_table_rows_filtered(data_table_path, row_filter, column_filter) -> BridgeDataTableInfo"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_rows_filtered(data_table_path, row_filter, column_filter)

    @staticmethod
    def get_data_table_summary(*, data_table_path):
        """X.get_data_table_summary(data_table_path) -> BridgeDataTableInfo"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_table_summary(data_table_path)

    @staticmethod
    def get_data_tables_using_struct(*, row_struct_name):
        """X.get_data_tables_using_struct(row_struct_name) -> Array[str]"""
        return unreal.UnrealBridgeDataTableLibrary.get_data_tables_using_struct(row_struct_name)

    @staticmethod
    def import_data_table_from_csv(*, data_table_path, csv_file_path):
        """X.import_data_table_from_csv(data_table_path, csv_file_path) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.import_data_table_from_csv(data_table_path, csv_file_path)

    @staticmethod
    def import_data_table_from_json(*, data_table_path, json_file_path):
        """X.import_data_table_from_json(data_table_path, json_file_path) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.import_data_table_from_json(data_table_path, json_file_path)

    @staticmethod
    def remove_data_table_row(*, data_table_path, row_name):
        """X.remove_data_table_row(data_table_path, row_name) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.remove_data_table_row(data_table_path, row_name)

    @staticmethod
    def rename_data_table_row(*, data_table_path, old_row_name, new_row_name):
        """X.rename_data_table_row(data_table_path, old_row_name, new_row_name) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.rename_data_table_row(data_table_path, old_row_name, new_row_name)

    @staticmethod
    def reorder_data_table_rows(*, data_table_path, ordered_names):
        """X.reorder_data_table_rows(data_table_path, ordered_names) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.reorder_data_table_rows(data_table_path, ordered_names)

    @staticmethod
    def search_data_table_rows(*, data_table_path, keyword, column_filter):
        """X.search_data_table_rows(data_table_path, keyword, column_filter) -> Array[str]"""
        return unreal.UnrealBridgeDataTableLibrary.search_data_table_rows(data_table_path, keyword, column_filter)

    @staticmethod
    def set_data_table_row_field(*, data_table_path, row_name, field_name, exported_value):
        """X.set_data_table_row_field(data_table_path, row_name, field_name, exported_value) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.set_data_table_row_field(data_table_path, row_name, field_name, exported_value)

    @staticmethod
    def set_data_table_row_fields(*, data_table_path, row_name, field_values):
        """X.set_data_table_row_fields(data_table_path, row_name, field_values) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.set_data_table_row_fields(data_table_path, row_name, field_values)


class Editor:
    """Wraps unreal.UnrealBridgeEditorLibrary (kwargs-only)."""

    @staticmethod
    def bring_editor_to_front():
        """X.bring_editor_to_front() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.bring_editor_to_front()

    @staticmethod
    def capture_active_viewport(*, out_file_path, include_base64):
        """X.capture_active_viewport(out_file_path, include_base64) -> BridgeScreenshotResult"""
        return unreal.UnrealBridgeEditorLibrary.capture_active_viewport(out_file_path, include_base64)

    @staticmethod
    def capture_channel_from_pose(*, channel, location, rotation, fov, width, height, max_depth_clamp, out_file_path, include_base64):
        """X.capture_channel_from_pose(channel, location, rotation, fov, width, height, max_depth_clamp, out_file_path, include_base64) -> BridgeChannelCaptureResult"""
        return unreal.UnrealBridgeEditorLibrary.capture_channel_from_pose(channel, location, rotation, fov, width, height, max_depth_clamp, out_file_path, include_base64)

    @staticmethod
    def capture_viewport_channel(*, channel, out_file_path, width, height, max_depth_clamp, include_base64):
        """X.capture_viewport_channel(channel, out_file_path, width, height, max_depth_clamp, include_base64) -> BridgeChannelCaptureResult"""
        return unreal.UnrealBridgeEditorLibrary.capture_viewport_channel(channel, out_file_path, width, height, max_depth_clamp, include_base64)

    @staticmethod
    def capture_viewport_hit_proxy_map(*, out_file_path, include_base64):
        """X.capture_viewport_hit_proxy_map(out_file_path, include_base64) -> BridgeScreenshotResult"""
        return unreal.UnrealBridgeEditorLibrary.capture_viewport_hit_proxy_map(out_file_path, include_base64)

    @staticmethod
    def check_out_asset(*, asset_path):
        """X.check_out_asset(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.check_out_asset(asset_path)

    @staticmethod
    def clear_bridge_call_log():
        """X.clear_bridge_call_log() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.clear_bridge_call_log()

    @staticmethod
    def clear_log_buffer():
        """X.clear_log_buffer() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.clear_log_buffer()

    @staticmethod
    def close_all_asset_editors():
        """X.close_all_asset_editors() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.close_all_asset_editors()

    @staticmethod
    def close_asset_editor(*, asset_path):
        """X.close_asset_editor(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.close_asset_editor(asset_path)

    @staticmethod
    def close_editor_tab(*, tab_name):
        """X.close_editor_tab(tab_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.close_editor_tab(tab_name)

    @staticmethod
    def compile_blueprints(*, blueprint_paths):
        """X.compile_blueprints(blueprint_paths) -> Array[BridgeCompileResult]"""
        return unreal.UnrealBridgeEditorLibrary.compile_blueprints(blueprint_paths)

    @staticmethod
    def create_new_level(*, save_existing):
        """X.create_new_level(save_existing) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.create_new_level(save_existing)

    @staticmethod
    def does_asset_exist_on_disk(*, asset_path):
        """X.does_asset_exist_on_disk(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.does_asset_exist_on_disk(asset_path)

    @staticmethod
    def dump_bridge_signature_registry():
        """X.dump_bridge_signature_registry() -> str"""
        return unreal.UnrealBridgeEditorLibrary.dump_bridge_signature_registry()

    @staticmethod
    def execute_console_command(*, command):
        """X.execute_console_command(command) -> str"""
        return unreal.UnrealBridgeEditorLibrary.execute_console_command(command)

    @staticmethod
    def fixup_redirectors(*, paths):
        """X.fixup_redirectors(paths) -> int32"""
        return unreal.UnrealBridgeEditorLibrary.fixup_redirectors(paths)

    @staticmethod
    def flush_compilation():
        """X.flush_compilation() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.flush_compilation()

    @staticmethod
    def focus_viewport_on_selection():
        """X.focus_viewport_on_selection() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.focus_viewport_on_selection()

    @staticmethod
    def get_actor_under_viewport_pixel(*, x, y):
        """X.get_actor_under_viewport_pixel(x, y) -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_actor_under_viewport_pixel(x, y)

    @staticmethod
    def get_asset_compile_job_count():
        """X.get_asset_compile_job_count() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_asset_compile_job_count()

    @staticmethod
    def get_asset_disk_path(*, asset_path):
        """X.get_asset_disk_path(asset_path) -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_asset_disk_path(asset_path)

    @staticmethod
    def get_asset_file_size(*, asset_path):
        """X.get_asset_file_size(asset_path) -> int64"""
        return unreal.UnrealBridgeEditorLibrary.get_asset_file_size(asset_path)

    @staticmethod
    def get_asset_last_modified_time(*, asset_path):
        """X.get_asset_last_modified_time(asset_path) -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_asset_last_modified_time(asset_path)

    @staticmethod
    def get_asset_source_control_state(*, asset_path):
        """X.get_asset_source_control_state(asset_path) -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_asset_source_control_state(asset_path)

    @staticmethod
    def get_auto_save_directory():
        """X.get_auto_save_directory() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_auto_save_directory()

    @staticmethod
    def get_auto_save_interval_minutes():
        """X.get_auto_save_interval_minutes() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_auto_save_interval_minutes()

    @staticmethod
    def get_bridge_call_log(*, max_entries=100):
        """X.get_bridge_call_log(max_entries=100) -> Array[BridgeCallLogEntry]"""
        return unreal.UnrealBridgeEditorLibrary.get_bridge_call_log(max_entries)

    @staticmethod
    def get_bridge_call_log_capacity():
        """X.get_bridge_call_log_capacity() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_bridge_call_log_capacity()

    @staticmethod
    def get_bridge_call_stats():
        """X.get_bridge_call_stats() -> BridgeCallStats"""
        return unreal.UnrealBridgeEditorLibrary.get_bridge_call_stats()

    @staticmethod
    def get_c_var(*, name):
        """X.get_c_var(name) -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_c_var(name)

    @staticmethod
    def get_content_browser_path():
        """X.get_content_browser_path() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_content_browser_path()

    @staticmethod
    def get_content_browser_selection():
        """X.get_content_browser_selection() -> Array[str]"""
        return unreal.UnrealBridgeEditorLibrary.get_content_browser_selection()

    @staticmethod
    def get_coord_system():
        """X.get_coord_system() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_coord_system()

    @staticmethod
    def get_cpu_brand():
        """X.get_cpu_brand() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_cpu_brand()

    @staticmethod
    def get_cpu_core_count():
        """X.get_cpu_core_count() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_cpu_core_count()

    @staticmethod
    def get_current_world_actor_count():
        """X.get_current_world_actor_count() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_current_world_actor_count()

    @staticmethod
    def get_dirty_package_names():
        """X.get_dirty_package_names() -> Array[str]"""
        return unreal.UnrealBridgeEditorLibrary.get_dirty_package_names()

    @staticmethod
    def get_editor_build_config():
        """X.get_editor_build_config() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_editor_build_config()

    @staticmethod
    def get_editor_build_date():
        """X.get_editor_build_date() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_editor_build_date()

    @staticmethod
    def get_editor_process_id():
        """X.get_editor_process_id() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_editor_process_id()

    @staticmethod
    def get_editor_state():
        """X.get_editor_state() -> BridgeEditorState"""
        return unreal.UnrealBridgeEditorLibrary.get_editor_state()

    @staticmethod
    def get_editor_viewport_camera():
        """X.get_editor_viewport_camera() -> BridgeViewportCamera"""
        return unreal.UnrealBridgeEditorLibrary.get_editor_viewport_camera()

    @staticmethod
    def get_editor_world_name():
        """X.get_editor_world_name() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_editor_world_name()

    @staticmethod
    def get_enabled_plugins():
        """X.get_enabled_plugins() -> Array[str]"""
        return unreal.UnrealBridgeEditorLibrary.get_enabled_plugins()

    @staticmethod
    def get_engine_changelist():
        """X.get_engine_changelist() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_engine_changelist()

    @staticmethod
    def get_engine_directory():
        """X.get_engine_directory() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_engine_directory()

    @staticmethod
    def get_engine_uptime():
        """X.get_engine_uptime() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_engine_uptime()

    @staticmethod
    def get_engine_version():
        """X.get_engine_version() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_engine_version()

    @staticmethod
    def get_frame_rate():
        """X.get_frame_rate() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_frame_rate()

    @staticmethod
    def get_loaded_level_count():
        """X.get_loaded_level_count() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_loaded_level_count()

    @staticmethod
    def get_location_grid_size():
        """X.get_location_grid_size() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_location_grid_size()

    @staticmethod
    def get_log_buffer_capacity():
        """X.get_log_buffer_capacity() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_log_buffer_capacity()

    @staticmethod
    def get_log_buffer_size():
        """X.get_log_buffer_size() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_log_buffer_size()

    @staticmethod
    def get_log_file_path():
        """X.get_log_file_path() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_log_file_path()

    @staticmethod
    def get_machine_name():
        """X.get_machine_name() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_machine_name()

    @staticmethod
    def get_main_window_title():
        """X.get_main_window_title() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_main_window_title()

    @staticmethod
    def get_memory_usage_mb():
        """X.get_memory_usage_mb() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_memory_usage_mb()

    @staticmethod
    def get_module_binary_path(*, module_name):
        """X.get_module_binary_path(module_name) -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_module_binary_path(module_name)

    @staticmethod
    def get_now_utc():
        """X.get_now_utc() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_now_utc()

    @staticmethod
    def get_opened_assets():
        """X.get_opened_assets() -> Array[BridgeOpenedAsset]"""
        return unreal.UnrealBridgeEditorLibrary.get_opened_assets()

    @staticmethod
    def get_os_user_name():
        """X.get_os_user_name() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_os_user_name()

    @staticmethod
    def get_os_version():
        """X.get_os_version() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_os_version()

    @staticmethod
    def get_pie_net_mode():
        """X.get_pie_net_mode() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_pie_net_mode()

    @staticmethod
    def get_pie_world_time():
        """X.get_pie_world_time() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_pie_world_time()

    @staticmethod
    def get_project_company_name():
        """X.get_project_company_name() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_project_company_name()

    @staticmethod
    def get_project_content_directory():
        """X.get_project_content_directory() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_project_content_directory()

    @staticmethod
    def get_project_id():
        """X.get_project_id() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_project_id()

    @staticmethod
    def get_project_intermediate_directory():
        """X.get_project_intermediate_directory() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_project_intermediate_directory()

    @staticmethod
    def get_project_plugins_directory():
        """X.get_project_plugins_directory() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_project_plugins_directory()

    @staticmethod
    def get_project_version():
        """X.get_project_version() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_project_version()

    @staticmethod
    def get_recent_log_lines(*, num_lines=50, min_severity=""):
        """X.get_recent_log_lines(num_lines=50, min_severity="") -> Array[str]"""
        return unreal.UnrealBridgeEditorLibrary.get_recent_log_lines(num_lines, min_severity)

    @staticmethod
    def get_registered_module_names():
        """X.get_registered_module_names() -> Array[str]"""
        return unreal.UnrealBridgeEditorLibrary.get_registered_module_names()

    @staticmethod
    def get_rotation_grid_size():
        """X.get_rotation_grid_size() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_rotation_grid_size()

    @staticmethod
    def get_screenshot_directory():
        """X.get_screenshot_directory() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_screenshot_directory()

    @staticmethod
    def get_shader_compile_job_count():
        """X.get_shader_compile_job_count() -> int32"""
        return unreal.UnrealBridgeEditorLibrary.get_shader_compile_job_count()

    @staticmethod
    def get_source_control_provider_name():
        """X.get_source_control_provider_name() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_source_control_provider_name()

    @staticmethod
    def get_total_physical_memory_mb():
        """X.get_total_physical_memory_mb() -> float"""
        return unreal.UnrealBridgeEditorLibrary.get_total_physical_memory_mb()

    @staticmethod
    def get_viewport_show_flag(*, flag_name):
        """X.get_viewport_show_flag(flag_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.get_viewport_show_flag(flag_name)

    @staticmethod
    def get_viewport_size():
        """X.get_viewport_size() -> Vector2D"""
        return unreal.UnrealBridgeEditorLibrary.get_viewport_size()

    @staticmethod
    def get_viewport_type():
        """X.get_viewport_type() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_viewport_type()

    @staticmethod
    def get_viewport_view_mode():
        """X.get_viewport_view_mode() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_viewport_view_mode()

    @staticmethod
    def get_widget_mode():
        """X.get_widget_mode() -> str"""
        return unreal.UnrealBridgeEditorLibrary.get_widget_mode()

    @staticmethod
    def is_asset_dirty(*, asset_path):
        """X.is_asset_dirty(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_asset_dirty(asset_path)

    @staticmethod
    def is_asset_editor_open(*, asset_path):
        """X.is_asset_editor_open(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_asset_editor_open(asset_path)

    @staticmethod
    def is_asset_loaded(*, asset_path):
        """X.is_asset_loaded(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_asset_loaded(asset_path)

    @staticmethod
    def is_auto_save_enabled():
        """X.is_auto_save_enabled() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_auto_save_enabled()

    @staticmethod
    def is_compiling():
        """X.is_compiling() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_compiling()

    @staticmethod
    def is_editor_tab_open(*, tab_name):
        """X.is_editor_tab_open(tab_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_editor_tab_open(tab_name)

    @staticmethod
    def is_editor_world_dirty():
        """X.is_editor_world_dirty() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_editor_world_dirty()

    @staticmethod
    def is_engine_installed():
        """X.is_engine_installed() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_engine_installed()

    @staticmethod
    def is_grid_snap_enabled():
        """X.is_grid_snap_enabled() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_grid_snap_enabled()

    @staticmethod
    def is_in_pie():
        """X.is_in_pie() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_in_pie()

    @staticmethod
    def is_live_coding_compiling():
        """X.is_live_coding_compiling() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_live_coding_compiling()

    @staticmethod
    def is_live_coding_enabled():
        """X.is_live_coding_enabled() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_live_coding_enabled()

    @staticmethod
    def is_module_loaded(*, module_name):
        """X.is_module_loaded(module_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_module_loaded(module_name)

    @staticmethod
    def is_play_in_editor_paused():
        """X.is_play_in_editor_paused() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_play_in_editor_paused()

    @staticmethod
    def is_plugin_enabled(*, plugin_name):
        """X.is_plugin_enabled(plugin_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_plugin_enabled(plugin_name)

    @staticmethod
    def is_simulating():
        """X.is_simulating() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_simulating()

    @staticmethod
    def is_source_control_enabled():
        """X.is_source_control_enabled() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_source_control_enabled()

    @staticmethod
    def is_unattended_mode():
        """X.is_unattended_mode() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_unattended_mode()

    @staticmethod
    def is_viewport_realtime():
        """X.is_viewport_realtime() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.is_viewport_realtime()

    @staticmethod
    def list_c_vars(*, keyword):
        """X.list_c_vars(keyword) -> Array[str]"""
        return unreal.UnrealBridgeEditorLibrary.list_c_vars(keyword)

    @staticmethod
    def load_level(*, level_path, prompt_save_changes):
        """X.load_level(level_path, prompt_save_changes) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.load_level(level_path, prompt_save_changes)

    @staticmethod
    def load_module(*, module_name):
        """X.load_module(module_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.load_module(module_name)

    @staticmethod
    def mark_asset_dirty(*, asset_path):
        """X.mark_asset_dirty(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.mark_asset_dirty(asset_path)

    @staticmethod
    def open_asset(*, asset_path):
        """X.open_asset(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.open_asset(asset_path)

    @staticmethod
    def open_editor_tab(*, tab_name):
        """X.open_editor_tab(tab_name) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.open_editor_tab(tab_name)

    @staticmethod
    def pause_pie(*, paused):
        """X.pause_pie(paused) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.pause_pie(paused)

    @staticmethod
    def recompile_blueprint(*, blueprint_path):
        """X.recompile_blueprint(blueprint_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.recompile_blueprint(blueprint_path)

    @staticmethod
    def redo():
        """X.redo() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.redo()

    @staticmethod
    def reload_asset(*, asset_path):
        """X.reload_asset(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.reload_asset(asset_path)

    @staticmethod
    def save_all_dirty_assets(*, include_maps):
        """X.save_all_dirty_assets(include_maps) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.save_all_dirty_assets(include_maps)

    @staticmethod
    def save_asset(*, asset_path):
        """X.save_asset(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.save_asset(asset_path)

    @staticmethod
    def save_assets(*, asset_paths):
        """X.save_assets(asset_paths) -> int32"""
        return unreal.UnrealBridgeEditorLibrary.save_assets(asset_paths)

    @staticmethod
    def save_current_level():
        """X.save_current_level() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.save_current_level()

    @staticmethod
    def set_auto_save_enabled(*, enabled):
        """X.set_auto_save_enabled(enabled) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_auto_save_enabled(enabled)

    @staticmethod
    def set_auto_save_interval_minutes(*, minutes):
        """X.set_auto_save_interval_minutes(minutes) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_auto_save_interval_minutes(minutes)

    @staticmethod
    def set_bridge_call_log_capacity(*, capacity):
        """X.set_bridge_call_log_capacity(capacity) -> int32"""
        return unreal.UnrealBridgeEditorLibrary.set_bridge_call_log_capacity(capacity)

    @staticmethod
    def set_c_var(*, name, value):
        """X.set_c_var(name, value) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_c_var(name, value)

    @staticmethod
    def set_content_browser_path(*, folder_path):
        """X.set_content_browser_path(folder_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_content_browser_path(folder_path)

    @staticmethod
    def set_content_browser_selection(*, asset_paths):
        """X.set_content_browser_selection(asset_paths) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_content_browser_selection(asset_paths)

    @staticmethod
    def set_coord_system(*, system):
        """X.set_coord_system(system) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_coord_system(system)

    @staticmethod
    def set_editor_viewport_camera(*, location, rotation, fov):
        """X.set_editor_viewport_camera(location, rotation, fov) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_editor_viewport_camera(location, rotation, fov)

    @staticmethod
    def set_grid_snap_enabled(*, enabled):
        """X.set_grid_snap_enabled(enabled) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_grid_snap_enabled(enabled)

    @staticmethod
    def set_viewport_realtime(*, realtime):
        """X.set_viewport_realtime(realtime) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_viewport_realtime(realtime)

    @staticmethod
    def set_viewport_show_flag(*, flag_name, enabled):
        """X.set_viewport_show_flag(flag_name, enabled) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_viewport_show_flag(flag_name, enabled)

    @staticmethod
    def set_viewport_type(*, viewport_type):
        """X.set_viewport_type(viewport_type) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_viewport_type(viewport_type)

    @staticmethod
    def set_viewport_view_mode(*, mode):
        """X.set_viewport_view_mode(mode) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_viewport_view_mode(mode)

    @staticmethod
    def set_widget_mode(*, mode):
        """X.set_widget_mode(mode) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.set_widget_mode(mode)

    @staticmethod
    def show_editor_notification(*, message, duration_seconds=4.000000, success=True):
        """X.show_editor_notification(message, duration_seconds=4.000000, success=True) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.show_editor_notification(message, duration_seconds, success)

    @staticmethod
    def start_pie():
        """X.start_pie() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.start_pie()

    @staticmethod
    def start_simulate():
        """X.start_simulate() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.start_simulate()

    @staticmethod
    def stop_pie():
        """X.stop_pie() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.stop_pie()

    @staticmethod
    def sync_content_browser_to_asset(*, asset_path):
        """X.sync_content_browser_to_asset(asset_path) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.sync_content_browser_to_asset(asset_path)

    @staticmethod
    def take_high_res_screenshot(*, resolution_multiplier):
        """X.take_high_res_screenshot(resolution_multiplier) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.take_high_res_screenshot(resolution_multiplier)

    @staticmethod
    def trigger_garbage_collection(*, full_purge=False):
        """X.trigger_garbage_collection(full_purge=False) -> bool"""
        return unreal.UnrealBridgeEditorLibrary.trigger_garbage_collection(full_purge)

    @staticmethod
    def trigger_live_coding_compile(*, wait_for_completion):
        """X.trigger_live_coding_compile(wait_for_completion) -> BridgeLiveCodingResult"""
        return unreal.UnrealBridgeEditorLibrary.trigger_live_coding_compile(wait_for_completion)

    @staticmethod
    def undo():
        """X.undo() -> bool"""
        return unreal.UnrealBridgeEditorLibrary.undo()

    @staticmethod
    def write_log_message(*, message, severity="Log"):
        """X.write_log_message(message, severity="Log") -> bool"""
        return unreal.UnrealBridgeEditorLibrary.write_log_message(message, severity)


class GameplayAbility:
    """Wraps unreal.UnrealBridgeGameplayAbilityLibrary (kwargs-only)."""

    @staticmethod
    def actor_ability_meets_tag_requirements(*, actor_name, ability_blueprint_path):
        """X.actor_ability_meets_tag_requirements(actor_name, ability_blueprint_path) -> Array[str] or None"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.actor_ability_meets_tag_requirements(actor_name, ability_blueprint_path)

    @staticmethod
    def actor_has_gameplay_tag(*, actor_name, tag_string, exact_match):
        """X.actor_has_gameplay_tag(actor_name, tag_string, exact_match) -> int32 or None"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.actor_has_gameplay_tag(actor_name, tag_string, exact_match)

    @staticmethod
    def add_ability_call_function_node(*, ability_blueprint_path, graph_name, function_name, node_pos_x, node_pos_y):
        """X.add_ability_call_function_node(ability_blueprint_path, graph_name, function_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.add_ability_call_function_node(ability_blueprint_path, graph_name, function_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_ability_task_node(*, ability_blueprint_path, graph_name, task_class_path, factory_function_name, node_pos_x, node_pos_y):
        """X.add_ability_task_node(ability_blueprint_path, graph_name, task_class_path, factory_function_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.add_ability_task_node(ability_blueprint_path, graph_name, task_class_path, factory_function_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_ability_trigger(*, ability_blueprint_path, trigger_tag, trigger_source):
        """X.add_ability_trigger(ability_blueprint_path, trigger_tag, trigger_source) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.add_ability_trigger(ability_blueprint_path, trigger_tag, trigger_source)

    @staticmethod
    def add_ge_component(*, gameplay_effect_blueprint_path, component_class_path):
        """X.add_ge_component(gameplay_effect_blueprint_path, component_class_path) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.add_ge_component(gameplay_effect_blueprint_path, component_class_path)

    @staticmethod
    def add_ge_modifier_scalable(*, gameplay_effect_blueprint_path, attribute_set_class_path, attribute_field_name, mod_op, flat_magnitude):
        """X.add_ge_modifier_scalable(gameplay_effect_blueprint_path, attribute_set_class_path, attribute_field_name, mod_op, flat_magnitude) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.add_ge_modifier_scalable(gameplay_effect_blueprint_path, attribute_set_class_path, attribute_field_name, mod_op, flat_magnitude)

    @staticmethod
    def clear_ability_triggers(*, ability_blueprint_path):
        """X.clear_ability_triggers(ability_blueprint_path) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.clear_ability_triggers(ability_blueprint_path)

    @staticmethod
    def clear_ge_modifiers(*, gameplay_effect_blueprint_path):
        """X.clear_ge_modifiers(gameplay_effect_blueprint_path) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.clear_ge_modifiers(gameplay_effect_blueprint_path)

    @staticmethod
    def create_gameplay_ability_blueprint(*, dest_content_path, asset_name, parent_class_path):
        """X.create_gameplay_ability_blueprint(dest_content_path, asset_name, parent_class_path) -> str"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.create_gameplay_ability_blueprint(dest_content_path, asset_name, parent_class_path)

    @staticmethod
    def ensure_ability_system_component(*, actor_name, location="Actor"):
        """X.ensure_ability_system_component(actor_name, location="Actor") -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.ensure_ability_system_component(actor_name, location)

    @staticmethod
    def ensure_bridge_test_attribute_set(*, actor_name):
        """X.ensure_bridge_test_attribute_set(actor_name) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.ensure_bridge_test_attribute_set(actor_name)

    @staticmethod
    def find_active_effects_by_tag(*, actor_name, tag_query, max_results):
        """X.find_active_effects_by_tag(actor_name, tag_query, max_results) -> Array[BridgeActiveEffectInfo]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.find_active_effects_by_tag(actor_name, tag_query, max_results)

    @staticmethod
    def find_child_tags(*, parent_tag, recursive):
        """X.find_child_tags(parent_tag, recursive) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.find_child_tags(parent_tag, recursive)

    @staticmethod
    def find_gameplay_tag_references(*, tag_query, package_path, match_exact, max_results):
        """X.find_gameplay_tag_references(tag_query, package_path, match_exact, max_results) -> BridgeTagReferenceReport"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.find_gameplay_tag_references(tag_query, package_path, match_exact, max_results)

    @staticmethod
    def get_ability_cooldown_info(*, actor_name, ability_blueprint_path):
        """X.get_ability_cooldown_info(actor_name, ability_blueprint_path) -> BridgeAbilityCooldownInfo"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_ability_cooldown_info(actor_name, ability_blueprint_path)

    @staticmethod
    def get_ability_tag_requirements(*, ability_blueprint_path):
        """X.get_ability_tag_requirements(ability_blueprint_path) -> BridgeAbilityTagRequirements"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_ability_tag_requirements(ability_blueprint_path)

    @staticmethod
    def get_ability_triggers(*, ability_blueprint_path):
        """X.get_ability_triggers(ability_blueprint_path) -> Array[BridgeAbilityTriggerInfo]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_ability_triggers(ability_blueprint_path)

    @staticmethod
    def get_actor_ability_system_info(*, actor_name):
        """X.get_actor_ability_system_info(actor_name) -> BridgeActorAbilitySystemInfo"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_ability_system_info(actor_name)

    @staticmethod
    def get_actor_active_abilities(*, actor_name):
        """X.get_actor_active_abilities(actor_name) -> Array[BridgeActiveAbilityInfo]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_active_abilities(actor_name)

    @staticmethod
    def get_actor_active_effects(*, actor_name, max_results):
        """X.get_actor_active_effects(actor_name, max_results) -> Array[BridgeActiveEffectInfo]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_active_effects(actor_name, max_results)

    @staticmethod
    def get_actor_attributes(*, actor_name):
        """X.get_actor_attributes(actor_name) -> Array[BridgeAttributeValue]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_attributes(actor_name)

    @staticmethod
    def get_actor_blocked_ability_tags(*, actor_name):
        """X.get_actor_blocked_ability_tags(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_actor_blocked_ability_tags(actor_name)

    @staticmethod
    def get_attribute_set_info(*, attribute_set_class_path):
        """X.get_attribute_set_info(attribute_set_class_path) -> BridgeAttributeSetInfo"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_attribute_set_info(attribute_set_class_path)

    @staticmethod
    def get_attribute_value(*, actor_name, attribute_name):
        """X.get_attribute_value(actor_name, attribute_name) -> BridgeAttributeValue"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_attribute_value(actor_name, attribute_name)

    @staticmethod
    def get_gameplay_ability_blueprint_info(*, ability_blueprint_path):
        """X.get_gameplay_ability_blueprint_info(ability_blueprint_path) -> BridgeGameplayAbilityInfo"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_gameplay_ability_blueprint_info(ability_blueprint_path)

    @staticmethod
    def get_gameplay_effect_blueprint_info(*, effect_blueprint_path):
        """X.get_gameplay_effect_blueprint_info(effect_blueprint_path) -> BridgeGameplayEffectInfo"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_gameplay_effect_blueprint_info(effect_blueprint_path)

    @staticmethod
    def get_tag_parents(*, tag_string):
        """X.get_tag_parents(tag_string) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.get_tag_parents(tag_string)

    @staticmethod
    def is_valid_gameplay_tag(*, tag_string):
        """X.is_valid_gameplay_tag(tag_string) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.is_valid_gameplay_tag(tag_string)

    @staticmethod
    def list_abilities_by_tag(*, tag_query, max_results):
        """X.list_abilities_by_tag(tag_query, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_abilities_by_tag(tag_query, max_results)

    @staticmethod
    def list_ability_blueprints(*, filter, max_results):
        """X.list_ability_blueprints(filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_ability_blueprints(filter, max_results)

    @staticmethod
    def list_ability_task_classes(*, filter, max_results):
        """X.list_ability_task_classes(filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_ability_task_classes(filter, max_results)

    @staticmethod
    def list_ability_task_factories(*, task_class_path):
        """X.list_ability_task_factories(task_class_path) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_ability_task_factories(task_class_path)

    @staticmethod
    def list_attribute_set_blueprints(*, filter, max_results):
        """X.list_attribute_set_blueprints(filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_attribute_set_blueprints(filter, max_results)

    @staticmethod
    def list_attribute_sets(*, filter, max_results):
        """X.list_attribute_sets(filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_attribute_sets(filter, max_results)

    @staticmethod
    def list_gameplay_effect_blueprints(*, filter, max_results):
        """X.list_gameplay_effect_blueprints(filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_gameplay_effect_blueprints(filter, max_results)

    @staticmethod
    def list_gameplay_effects_by_tag(*, tag_query, max_results):
        """X.list_gameplay_effects_by_tag(tag_query, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_gameplay_effects_by_tag(tag_query, max_results)

    @staticmethod
    def list_gameplay_tags(*, filter, max_results):
        """X.list_gameplay_tags(filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.list_gameplay_tags(filter, max_results)

    @staticmethod
    def remove_ability_trigger_by_tag(*, ability_blueprint_path, trigger_tag):
        """X.remove_ability_trigger_by_tag(ability_blueprint_path, trigger_tag) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.remove_ability_trigger_by_tag(ability_blueprint_path, trigger_tag)

    @staticmethod
    def remove_ge_components_by_class(*, gameplay_effect_blueprint_path, component_class_path):
        """X.remove_ge_components_by_class(gameplay_effect_blueprint_path, component_class_path) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.remove_ge_components_by_class(gameplay_effect_blueprint_path, component_class_path)

    @staticmethod
    def remove_ge_modifier(*, gameplay_effect_blueprint_path, index):
        """X.remove_ge_modifier(gameplay_effect_blueprint_path, index) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.remove_ge_modifier(gameplay_effect_blueprint_path, index)

    @staticmethod
    def send_gameplay_event_by_name(*, actor_name, event_tag, event_magnitude):
        """X.send_gameplay_event_by_name(actor_name, event_tag, event_magnitude) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.send_gameplay_event_by_name(actor_name, event_tag, event_magnitude)

    @staticmethod
    def set_ability_cooldown(*, ability_blueprint_path, cooldown_gameplay_effect_class_path):
        """X.set_ability_cooldown(ability_blueprint_path, cooldown_gameplay_effect_class_path) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ability_cooldown(ability_blueprint_path, cooldown_gameplay_effect_class_path)

    @staticmethod
    def set_ability_cost(*, ability_blueprint_path, cost_gameplay_effect_class_path):
        """X.set_ability_cost(ability_blueprint_path, cost_gameplay_effect_class_path) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ability_cost(ability_blueprint_path, cost_gameplay_effect_class_path)

    @staticmethod
    def set_ability_instancing_policy(*, ability_blueprint_path, policy):
        """X.set_ability_instancing_policy(ability_blueprint_path, policy) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ability_instancing_policy(ability_blueprint_path, policy)

    @staticmethod
    def set_ability_net_execution_policy(*, ability_blueprint_path, policy):
        """X.set_ability_net_execution_policy(ability_blueprint_path, policy) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ability_net_execution_policy(ability_blueprint_path, policy)

    @staticmethod
    def set_ability_tag_container(*, ability_blueprint_path, container_name, tags):
        """X.set_ability_tag_container(ability_blueprint_path, container_name, tags) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ability_tag_container(ability_blueprint_path, container_name, tags)

    @staticmethod
    def set_actor_attribute_value(*, actor_name, attribute_name, value):
        """X.set_actor_attribute_value(actor_name, attribute_name, value) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_actor_attribute_value(actor_name, attribute_name, value)

    @staticmethod
    def set_gameplay_cue_tag(*, cue_notify_blueprint_path, tag_string):
        """X.set_gameplay_cue_tag(cue_notify_blueprint_path, tag_string) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_gameplay_cue_tag(cue_notify_blueprint_path, tag_string)

    @staticmethod
    def set_ge_component_inherited_tags(*, gameplay_effect_blueprint_path, component_index, field_name, tags):
        """X.set_ge_component_inherited_tags(gameplay_effect_blueprint_path, component_index, field_name, tags) -> int32"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ge_component_inherited_tags(gameplay_effect_blueprint_path, component_index, field_name, tags)

    @staticmethod
    def set_ge_scalable_float_field(*, gameplay_effect_blueprint_path, field_name, flat_value):
        """X.set_ge_scalable_float_field(gameplay_effect_blueprint_path, field_name, flat_value) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.set_ge_scalable_float_field(gameplay_effect_blueprint_path, field_name, flat_value)

    @staticmethod
    def tag_matches(*, tag_a, tag_b, exact_match):
        """X.tag_matches(tag_a, tag_b, exact_match) -> bool"""
        return unreal.UnrealBridgeGameplayAbilityLibrary.tag_matches(tag_a, tag_b, exact_match)


class Gameplay:
    """Wraps unreal.UnrealBridgeGameplayLibrary (kwargs-only)."""

    @staticmethod
    def add_force_to_pie_actor(*, actor_name, force):
        """X.add_force_to_pie_actor(actor_name, force) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_force_to_pie_actor(actor_name, force)

    @staticmethod
    def add_impulse_to_pie_actor(*, actor_name, impulse, velocity_change=False):
        """X.add_impulse_to_pie_actor(actor_name, impulse, velocity_change=False) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_impulse_to_pie_actor(actor_name, impulse, velocity_change)

    @staticmethod
    def add_mapping_context(*, mapping_context_path, priority=0):
        """X.add_mapping_context(mapping_context_path, priority=0) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_mapping_context(mapping_context_path, priority)

    @staticmethod
    def add_on_screen_debug_message(*, message, duration_seconds=4.000000, r=1.000000, g=1.000000, b=1.000000):
        """X.add_on_screen_debug_message(message, duration_seconds=4.000000, r=1.000000, g=1.000000, b=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_on_screen_debug_message(message, duration_seconds, r, g, b)

    @staticmethod
    def apply_damage_to_actor(*, target_actor_name, damage_amount):
        """X.apply_damage_to_actor(target_actor_name, damage_amount) -> float"""
        return unreal.UnrealBridgeGameplayLibrary.apply_damage_to_actor(target_actor_name, damage_amount)

    @staticmethod
    def apply_look_input(*, yaw_delta, pitch_delta):
        """X.apply_look_input(yaw_delta, pitch_delta) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.apply_look_input(yaw_delta, pitch_delta)

    @staticmethod
    def apply_movement_input(*, world_direction, scale_value=1.000000, force=False):
        """X.apply_movement_input(world_direction, scale_value=1.000000, force=False) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.apply_movement_input(world_direction, scale_value, force)

    @staticmethod
    def apply_radial_damage(*, origin, damage_amount, inner_radius, outer_radius):
        """X.apply_radial_damage(origin, damage_amount, inner_radius, outer_radius) -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.apply_radial_damage(origin, damage_amount, inner_radius, outer_radius)

    @staticmethod
    def clear_on_screen_debug_messages():
        """X.clear_on_screen_debug_messages() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.clear_on_screen_debug_messages()

    @staticmethod
    def clear_sticky_input(*, input_action_path=""):
        """X.clear_sticky_input(input_action_path="") -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.clear_sticky_input(input_action_path)

    @staticmethod
    def deproject_screen_to_world(*, normalized_x, normalized_y):
        """X.deproject_screen_to_world(normalized_x, normalized_y) -> (out_origin=Vector, out_direction=Vector) or None"""
        return unreal.UnrealBridgeGameplayLibrary.deproject_screen_to_world(normalized_x, normalized_y)

    @staticmethod
    def destroy_actor_in_pie(*, actor_name):
        """X.destroy_actor_in_pie(actor_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.destroy_actor_in_pie(actor_name)

    @staticmethod
    def draw_debug_arrow(*, start, end, arrow_size=20.000000, duration_seconds=5.000000):
        """X.draw_debug_arrow(start, end, arrow_size=20.000000, duration_seconds=5.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.draw_debug_arrow(start, end, arrow_size, duration_seconds)

    @staticmethod
    def draw_debug_box_at(*, center, extent, thickness=1.000000, duration_seconds=5.000000):
        """X.draw_debug_box_at(center, extent, thickness=1.000000, duration_seconds=5.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.draw_debug_box_at(center, extent, thickness, duration_seconds)

    @staticmethod
    def draw_debug_line(*, start, end, thickness=1.000000, duration_seconds=5.000000):
        """X.draw_debug_line(start, end, thickness=1.000000, duration_seconds=5.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.draw_debug_line(start, end, thickness, duration_seconds)

    @staticmethod
    def draw_debug_sphere_at(*, center, radius=50.000000, thickness=1.000000, duration_seconds=5.000000):
        """X.draw_debug_sphere_at(center, radius=50.000000, thickness=1.000000, duration_seconds=5.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.draw_debug_sphere_at(center, radius, thickness, duration_seconds)

    @staticmethod
    def draw_debug_string(*, text, location, duration_seconds=5.000000):
        """X.draw_debug_string(text, location, duration_seconds=5.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.draw_debug_string(text, location, duration_seconds)

    @staticmethod
    def find_nav_path(*, start_location, end_location):
        """X.find_nav_path(start_location, end_location) -> (out_waypoints=Array[Vector], out_path_length=float) or None"""
        return unreal.UnrealBridgeGameplayLibrary.find_nav_path(start_location, end_location)

    @staticmethod
    def find_pie_actors_by_class(*, class_path):
        """X.find_pie_actors_by_class(class_path) -> Array[str]"""
        return unreal.UnrealBridgeGameplayLibrary.find_pie_actors_by_class(class_path)

    @staticmethod
    def flush_persistent_debug_draws():
        """X.flush_persistent_debug_draws() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.flush_persistent_debug_draws()

    @staticmethod
    def get_actor_at_screen_position(*, normalized_x, normalized_y, max_distance=10000.000000):
        """X.get_actor_at_screen_position(normalized_x, normalized_y, max_distance=10000.000000) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_actor_at_screen_position(normalized_x, normalized_y, max_distance)

    @staticmethod
    def get_actor_controller(*, actor_name):
        """X.get_actor_controller(actor_name) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_actor_controller(actor_name)

    @staticmethod
    def get_actor_time_dilation(*, actor_name):
        """X.get_actor_time_dilation(actor_name) -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_actor_time_dilation(actor_name)

    @staticmethod
    def get_agent_observation(*, max_actor_distance=3000.000000, require_line_of_sight=True, class_filter=""):
        """X.get_agent_observation(max_actor_distance=3000.000000, require_line_of_sight=True, class_filter="") -> AgentObservation or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_agent_observation(max_actor_distance, require_line_of_sight, class_filter)

    @staticmethod
    def get_ai_pawns():
        """X.get_ai_pawns() -> Array[str]"""
        return unreal.UnrealBridgeGameplayLibrary.get_ai_pawns()

    @staticmethod
    def get_all_pawns():
        """X.get_all_pawns() -> Array[str]"""
        return unreal.UnrealBridgeGameplayLibrary.get_all_pawns()

    @staticmethod
    def get_camera_fov():
        """X.get_camera_fov() -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_camera_fov()

    @staticmethod
    def get_camera_hit_actor(*, max_distance=10000.000000):
        """X.get_camera_hit_actor(max_distance=10000.000000) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_camera_hit_actor(max_distance)

    @staticmethod
    def get_camera_hit_location(*, max_distance):
        """X.get_camera_hit_location(max_distance) -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_camera_hit_location(max_distance)

    @staticmethod
    def get_camera_view_point():
        """X.get_camera_view_point() -> (out_location=Vector, out_rotation=Rotator) or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_camera_view_point()

    @staticmethod
    def get_control_rotation():
        """X.get_control_rotation() -> Rotator or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_control_rotation()

    @staticmethod
    def get_distance_to_pawn(*, location):
        """X.get_distance_to_pawn(location) -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_distance_to_pawn(location)

    @staticmethod
    def get_game_mode_class_name():
        """X.get_game_mode_class_name() -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_game_mode_class_name()

    @staticmethod
    def get_game_state_class_name():
        """X.get_game_state_class_name() -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_game_state_class_name()

    @staticmethod
    def get_global_time_dilation():
        """X.get_global_time_dilation() -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_global_time_dilation()

    @staticmethod
    def get_input_action_triggers(*, input_action_path):
        """X.get_input_action_triggers(input_action_path) -> (out_trigger_names=Array[str], out_threshold_seconds=Array[float]) or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_action_triggers(input_action_path)

    @staticmethod
    def get_input_action_value_type(*, input_action_path):
        """X.get_input_action_value_type(input_action_path) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_action_value_type(input_action_path)

    @staticmethod
    def get_nav_mesh_bounds():
        """X.get_nav_mesh_bounds() -> (out_min=Vector, out_max=Vector) or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_nav_mesh_bounds()

    @staticmethod
    def get_pawn_capabilities():
        """X.get_pawn_capabilities() -> (jump_z_velocity=float, max_walk_speed=float, max_step_height=float, walkable_floor_angle_deg=float, capsule_radius=float, capsule_half_height=float, crouched_half_height=float, can_crouch=bool, can_jump=bool) or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_capabilities()

    @staticmethod
    def get_pawn_forward_vector():
        """X.get_pawn_forward_vector() -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_forward_vector()

    @staticmethod
    def get_pawn_ground_height(*, max_distance=5000.000000):
        """X.get_pawn_ground_height(max_distance=5000.000000) -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_ground_height(max_distance)

    @staticmethod
    def get_pawn_max_walk_speed():
        """X.get_pawn_max_walk_speed() -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_max_walk_speed()

    @staticmethod
    def get_pawn_right_vector():
        """X.get_pawn_right_vector() -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_right_vector()

    @staticmethod
    def get_pawn_speed():
        """X.get_pawn_speed() -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_speed()

    @staticmethod
    def get_pawn_up_vector():
        """X.get_pawn_up_vector() -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pawn_up_vector()

    @staticmethod
    def get_pie_actor_linear_velocity(*, actor_name):
        """X.get_pie_actor_linear_velocity(actor_name) -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_actor_linear_velocity(actor_name)

    @staticmethod
    def get_pie_actor_location(*, actor_name):
        """X.get_pie_actor_location(actor_name) -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_actor_location(actor_name)

    @staticmethod
    def get_pie_delta_seconds():
        """X.get_pie_delta_seconds() -> float"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_delta_seconds()

    @staticmethod
    def get_pie_frame_number():
        """X.get_pie_frame_number() -> int64"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_frame_number()

    @staticmethod
    def get_pie_num_ai_controllers():
        """X.get_pie_num_ai_controllers() -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_num_ai_controllers()

    @staticmethod
    def get_pie_num_players():
        """X.get_pie_num_players() -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_num_players()

    @staticmethod
    def get_pie_viewport_size():
        """X.get_pie_viewport_size() -> Vector2D or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_pie_viewport_size()

    @staticmethod
    def get_player_pawn_actor_name():
        """X.get_player_pawn_actor_name() -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_player_pawn_actor_name()

    @staticmethod
    def get_player_start_actor_name():
        """X.get_player_start_actor_name() -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_player_start_actor_name()

    @staticmethod
    def get_player_start_transform():
        """X.get_player_start_transform() -> (out_location=Vector, out_rotation=Rotator) or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_player_start_transform()

    @staticmethod
    def get_random_reachable_point_in_radius(*, origin, radius):
        """X.get_random_reachable_point_in_radius(origin, radius) -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_random_reachable_point_in_radius(origin, radius)

    @staticmethod
    def get_sticky_inputs():
        """X.get_sticky_inputs() -> (int32, out_paths=Array[str], out_values=Array[Vector])"""
        return unreal.UnrealBridgeGameplayLibrary.get_sticky_inputs()

    @staticmethod
    def inject_enhanced_input_axis(*, input_action_path, axis_value):
        """X.inject_enhanced_input_axis(input_action_path, axis_value) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.inject_enhanced_input_axis(input_action_path, axis_value)

    @staticmethod
    def is_actor_ai_controlled(*, actor_name):
        """X.is_actor_ai_controlled(actor_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.is_actor_ai_controlled(actor_name)

    @staticmethod
    def is_actor_visible_from_camera(*, actor_name, max_distance=10000.000000):
        """X.is_actor_visible_from_camera(actor_name, max_distance=10000.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.is_actor_visible_from_camera(actor_name, max_distance)

    @staticmethod
    def is_game_paused():
        """X.is_game_paused() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.is_game_paused()

    @staticmethod
    def is_in_pie():
        """X.is_in_pie() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.is_in_pie()

    @staticmethod
    def is_mapping_context_active(*, mapping_context_path):
        """X.is_mapping_context_active(mapping_context_path) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.is_mapping_context_active(mapping_context_path)

    @staticmethod
    def is_point_on_navmesh(*, point, tolerance=50.000000):
        """X.is_point_on_navmesh(point, tolerance=50.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.is_point_on_navmesh(point, tolerance)

    @staticmethod
    def jump():
        """X.jump() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.jump()

    @staticmethod
    def pause_game(*, paused):
        """X.pause_game(paused) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.pause_game(paused)

    @staticmethod
    def play_sound2d(*, sound_asset_path, volume_multiplier=1.000000, pitch_multiplier=1.000000):
        """X.play_sound2d(sound_asset_path, volume_multiplier=1.000000, pitch_multiplier=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.play_sound2d(sound_asset_path, volume_multiplier, pitch_multiplier)

    @staticmethod
    def play_sound_at_location(*, sound_asset_path, location, volume_multiplier=1.000000, pitch_multiplier=1.000000):
        """X.play_sound_at_location(sound_asset_path, location, volume_multiplier=1.000000, pitch_multiplier=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.play_sound_at_location(sound_asset_path, location, volume_multiplier, pitch_multiplier)

    @staticmethod
    def play_world_camera_shake(*, shake_class_path, epicenter, inner_radius, outer_radius, scale_multiplier=1.000000):
        """X.play_world_camera_shake(shake_class_path, epicenter, inner_radius, outer_radius, scale_multiplier=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.play_world_camera_shake(shake_class_path, epicenter, inner_radius, outer_radius, scale_multiplier)

    @staticmethod
    def press_key(*, key_name, pressed=True):
        """X.press_key(key_name, pressed=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.press_key(key_name, pressed)

    @staticmethod
    def project_point_to_navmesh(*, point, search_extent):
        """X.project_point_to_navmesh(point, search_extent) -> Vector or None"""
        return unreal.UnrealBridgeGameplayLibrary.project_point_to_navmesh(point, search_extent)

    @staticmethod
    def project_world_to_screen(*, world_location):
        """X.project_world_to_screen(world_location) -> Vector2D or None"""
        return unreal.UnrealBridgeGameplayLibrary.project_world_to_screen(world_location)

    @staticmethod
    def remove_mapping_context(*, mapping_context_path):
        """X.remove_mapping_context(mapping_context_path) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_mapping_context(mapping_context_path)

    @staticmethod
    def respawn_player_pawn():
        """X.respawn_player_pawn() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.respawn_player_pawn()

    @staticmethod
    def set_actor_time_dilation(*, actor_name, scale):
        """X.set_actor_time_dilation(actor_name, scale) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_actor_time_dilation(actor_name, scale)

    @staticmethod
    def set_camera_fov(*, fov):
        """X.set_camera_fov(fov) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_camera_fov(fov)

    @staticmethod
    def set_control_rotation(*, new_rotation):
        """X.set_control_rotation(new_rotation) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_control_rotation(new_rotation)

    @staticmethod
    def set_global_time_dilation(*, scale):
        """X.set_global_time_dilation(scale) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_global_time_dilation(scale)

    @staticmethod
    def set_pawn_gravity_scale(*, scale):
        """X.set_pawn_gravity_scale(scale) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_pawn_gravity_scale(scale)

    @staticmethod
    def set_pawn_max_walk_speed(*, speed):
        """X.set_pawn_max_walk_speed(speed) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_pawn_max_walk_speed(speed)

    @staticmethod
    def set_sticky_input(*, input_action_path, axis_value):
        """X.set_sticky_input(input_action_path, axis_value) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_sticky_input(input_action_path, axis_value)

    @staticmethod
    def simulate_jump_arc(*, start_location, initial_velocity, max_time, step_dt, max_path_length):
        """X.simulate_jump_arc(start_location, initial_velocity, max_time, step_dt, max_path_length) -> (out_land_location=Vector, out_land_actor_label=str) or None"""
        return unreal.UnrealBridgeGameplayLibrary.simulate_jump_arc(start_location, initial_velocity, max_time, step_dt, max_path_length)

    @staticmethod
    def spawn_actor_in_pie(*, class_path, location, rotation):
        """X.spawn_actor_in_pie(class_path, location, rotation) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.spawn_actor_in_pie(class_path, location, rotation)

    @staticmethod
    def start_camera_shake(*, shake_class_path, scale=1.000000):
        """X.start_camera_shake(shake_class_path, scale=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.start_camera_shake(shake_class_path, scale)

    @staticmethod
    def stop_all_camera_shakes(*, immediately=True):
        """X.stop_all_camera_shakes(immediately=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.stop_all_camera_shakes(immediately)

    @staticmethod
    def stop_camera_shake_by_class(*, shake_class_path, immediately=True):
        """X.stop_camera_shake_by_class(shake_class_path, immediately=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.stop_camera_shake_by_class(shake_class_path, immediately)

    @staticmethod
    def stop_jumping():
        """X.stop_jumping() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.stop_jumping()

    @staticmethod
    def teleport_pawn(*, new_location, new_rotation, snap_controller=True, stop_velocity=True):
        """X.teleport_pawn(new_location, new_rotation, snap_controller=True, stop_velocity=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.teleport_pawn(new_location, new_rotation, snap_controller, stop_velocity)

    @staticmethod
    def trigger_input_action(*, input_action_path, hold_seconds=-1.000000):
        """X.trigger_input_action(input_action_path, hold_seconds=-1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.trigger_input_action(input_action_path, hold_seconds)

    @staticmethod
    def unlock_camera_fov():
        """X.unlock_camera_fov() -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.unlock_camera_fov()

    @staticmethod
    def wake_pie_actor_physics(*, actor_name):
        """X.wake_pie_actor_physics(actor_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.wake_pie_actor_physics(actor_name)


class Level:
    """Wraps unreal.UnrealBridgeLevelLibrary (kwargs-only)."""

    @staticmethod
    def add_actor_tag(*, actor_name, tag):
        """X.add_actor_tag(actor_name, tag) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.add_actor_tag(actor_name, tag)

    @staticmethod
    def add_component_of_class(*, actor_name, component_class_path):
        """X.add_component_of_class(actor_name, component_class_path) -> str"""
        return unreal.UnrealBridgeLevelLibrary.add_component_of_class(actor_name, component_class_path)

    @staticmethod
    def attach_actor(*, child_name, parent_name, socket_name):
        """X.attach_actor(child_name, parent_name, socket_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.attach_actor(child_name, parent_name, socket_name)

    @staticmethod
    def box_trace_first_actor(*, start, end, box_half_extent):
        """X.box_trace_first_actor(start, end, box_half_extent) -> str"""
        return unreal.UnrealBridgeLevelLibrary.box_trace_first_actor(start, end, box_half_extent)

    @staticmethod
    def capture_anim_montage_timeline(*, anim_path, skeletal_mesh_path, num_time_samples, views, bone_overlay, per_view_framing, ground_grid, root_trajectory, ground_z, cell_width, cell_height, file_path):
        """X.capture_anim_montage_timeline(anim_path, skeletal_mesh_path, num_time_samples, views, bone_overlay, per_view_framing, ground_grid, root_trajectory, ground_z, cell_width, cell_height, file_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.capture_anim_montage_timeline(anim_path, skeletal_mesh_path, num_time_samples, views, bone_overlay, per_view_framing, ground_grid, root_trajectory, ground_z, cell_width, cell_height, file_path)

    @staticmethod
    def capture_anim_pose_grid(*, anim_path, time, skeletal_mesh_path, views, bone_overlay, per_view_framing, ground_grid, root_trajectory, ground_z, grid_cols, cell_width, cell_height, file_path):
        """X.capture_anim_pose_grid(anim_path, time, skeletal_mesh_path, views, bone_overlay, per_view_framing, ground_grid, root_trajectory, ground_z, grid_cols, cell_width, cell_height, file_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.capture_anim_pose_grid(anim_path, time, skeletal_mesh_path, views, bone_overlay, per_view_framing, ground_grid, root_trajectory, ground_z, grid_cols, cell_width, cell_height, file_path)

    @staticmethod
    def capture_from_pose(*, camera_location, camera_rotation, fov_deg, width, height, file_path):
        """X.capture_from_pose(camera_location, camera_rotation, fov_deg, width, height, file_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.capture_from_pose(camera_location, camera_rotation, fov_deg, width, height, file_path)

    @staticmethod
    def capture_ortho_top_down(*, center, world_size, width, height, file_path, camera_height=5000.000000):
        """X.capture_ortho_top_down(center, world_size, width, height, file_path, camera_height=5000.000000) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.capture_ortho_top_down(center, world_size, width, height, file_path, camera_height)

    @staticmethod
    def closest_point_on_segment(*, point, segment_start, segment_end):
        """X.closest_point_on_segment(point, segment_start, segment_end) -> Vector"""
        return unreal.UnrealBridgeLevelLibrary.closest_point_on_segment(point, segment_start, segment_end)

    @staticmethod
    def count_actors_by_tag(*, tag):
        """X.count_actors_by_tag(tag) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.count_actors_by_tag(tag)

    @staticmethod
    def count_actors_in_sublevel(*, package_name):
        """X.count_actors_in_sublevel(package_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.count_actors_in_sublevel(package_name)

    @staticmethod
    def deselect_all_actors():
        """X.deselect_all_actors() -> bool"""
        return unreal.UnrealBridgeLevelLibrary.deselect_all_actors()

    @staticmethod
    def destroy_actor(*, actor_name):
        """X.destroy_actor(actor_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.destroy_actor(actor_name)

    @staticmethod
    def destroy_actors(*, actor_names):
        """X.destroy_actors(actor_names) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.destroy_actors(actor_names)

    @staticmethod
    def detach_actor(*, actor_name):
        """X.detach_actor(actor_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.detach_actor(actor_name)

    @staticmethod
    def dissolve_folder(*, folder_path):
        """X.dissolve_folder(folder_path) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.dissolve_folder(folder_path)

    @staticmethod
    def distance_from_point_to_segment(*, point, segment_start, segment_end):
        """X.distance_from_point_to_segment(point, segment_start, segment_end) -> float"""
        return unreal.UnrealBridgeLevelLibrary.distance_from_point_to_segment(point, segment_start, segment_end)

    @staticmethod
    def duplicate_actors(*, actor_names):
        """X.duplicate_actors(actor_names) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.duplicate_actors(actor_names)

    @staticmethod
    def find_actors_by_class(*, class_path, max_results):
        """X.find_actors_by_class(class_path, max_results) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.find_actors_by_class(class_path, max_results)

    @staticmethod
    def find_actors_by_class_and_tag(*, class_filter, tag):
        """X.find_actors_by_class_and_tag(class_filter, tag) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.find_actors_by_class_and_tag(class_filter, tag)

    @staticmethod
    def find_actors_by_tag(*, tag):
        """X.find_actors_by_tag(tag) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.find_actors_by_tag(tag)

    @staticmethod
    def find_actors_in_cone(*, origin, direction, half_angle_deg, max_distance, class_filter):
        """X.find_actors_in_cone(origin, direction, half_angle_deg, max_distance, class_filter) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.find_actors_in_cone(origin, direction, half_angle_deg, max_distance, class_filter)

    @staticmethod
    def find_actors_in_radius(*, location, radius, class_filter):
        """X.find_actors_in_radius(location, radius, class_filter) -> Array[BridgeActorRadiusHit]"""
        return unreal.UnrealBridgeLevelLibrary.find_actors_in_radius(location, radius, class_filter)

    @staticmethod
    def find_nearest_actor(*, location, class_filter):
        """X.find_nearest_actor(location, class_filter) -> str"""
        return unreal.UnrealBridgeLevelLibrary.find_nearest_actor(location, class_filter)

    @staticmethod
    def flush_level_streaming():
        """X.flush_level_streaming() -> bool"""
        return unreal.UnrealBridgeLevelLibrary.flush_level_streaming()

    @staticmethod
    def get_actor_bounds(*, actor_name):
        """X.get_actor_bounds(actor_name) -> BridgeActorBounds"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_bounds(actor_name)

    @staticmethod
    def get_actor_class_hierarchy(*, actor_name):
        """X.get_actor_class_hierarchy(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_class_hierarchy(actor_name)

    @staticmethod
    def get_actor_class_histogram():
        """X.get_actor_class_histogram() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_class_histogram()

    @staticmethod
    def get_actor_collision_profile(*, actor_name):
        """X.get_actor_collision_profile(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_collision_profile(actor_name)

    @staticmethod
    def get_actor_components(*, actor_name):
        """X.get_actor_components(actor_name) -> Array[BridgeLevelComponentInfo]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_components(actor_name)

    @staticmethod
    def get_actor_count(*, class_filter):
        """X.get_actor_count(class_filter) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_count(class_filter)

    @staticmethod
    def get_actor_distance(*, actor_a, actor_b):
        """X.get_actor_distance(actor_a, actor_b) -> float"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_distance(actor_a, actor_b)

    @staticmethod
    def get_actor_folder(*, actor_name):
        """X.get_actor_folder(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_folder(actor_name)

    @staticmethod
    def get_actor_folders():
        """X.get_actor_folders() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_folders()

    @staticmethod
    def get_actor_ground_clearance(*, actor_name, max_distance=10000.000000):
        """X.get_actor_ground_clearance(actor_name, max_distance=10000.000000) -> float"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_ground_clearance(actor_name, max_distance)

    @staticmethod
    def get_actor_info(*, actor_name):
        """X.get_actor_info(actor_name) -> BridgeActorInfo"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_info(actor_name)

    @staticmethod
    def get_actor_level_package_name(*, actor_name):
        """X.get_actor_level_package_name(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_level_package_name(actor_name)

    @staticmethod
    def get_actor_lod_count(*, actor_name):
        """X.get_actor_lod_count(actor_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_lod_count(actor_name)

    @staticmethod
    def get_actor_material_slot_count(*, actor_name):
        """X.get_actor_material_slot_count(actor_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_material_slot_count(actor_name)

    @staticmethod
    def get_actor_materials(*, actor_name):
        """X.get_actor_materials(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_materials(actor_name)

    @staticmethod
    def get_actor_mesh(*, actor_name):
        """X.get_actor_mesh(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_mesh(actor_name)

    @staticmethod
    def get_actor_names(*, class_filter, tag_filter, name_filter):
        """X.get_actor_names(class_filter, tag_filter, name_filter) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_names(class_filter, tag_filter, name_filter)

    @staticmethod
    def get_actor_parent_class(*, actor_name):
        """X.get_actor_parent_class(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_parent_class(actor_name)

    @staticmethod
    def get_actor_property(*, actor_name, property_path):
        """X.get_actor_property(actor_name, property_path) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_property(actor_name, property_path)

    @staticmethod
    def get_actor_root_component_name(*, actor_name):
        """X.get_actor_root_component_name(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_root_component_name(actor_name)

    @staticmethod
    def get_actor_siblings(*, actor_name):
        """X.get_actor_siblings(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_siblings(actor_name)

    @staticmethod
    def get_actor_sockets(*, actor_name):
        """X.get_actor_sockets(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_sockets(actor_name)

    @staticmethod
    def get_actor_transform(*, actor_name):
        """X.get_actor_transform(actor_name) -> BridgeTransform"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_transform(actor_name)

    @staticmethod
    def get_actor_triangle_count(*, actor_name):
        """X.get_actor_triangle_count(actor_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_triangle_count(actor_name)

    @staticmethod
    def get_actor_vertex_count(*, actor_name):
        """X.get_actor_vertex_count(actor_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_actor_vertex_count(actor_name)

    @staticmethod
    def get_actors_in_box(*, min, max, class_filter):
        """X.get_actors_in_box(min, max, class_filter) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actors_in_box(min, max, class_filter)

    @staticmethod
    def get_actors_in_folder(*, folder_path, recursive):
        """X.get_actors_in_folder(folder_path, recursive) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actors_in_folder(folder_path, recursive)

    @staticmethod
    def get_actors_in_sublevel(*, package_name):
        """X.get_actors_in_sublevel(package_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_actors_in_sublevel(package_name)

    @staticmethod
    def get_all_actor_tags_in_level():
        """X.get_all_actor_tags_in_level() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_all_actor_tags_in_level()

    @staticmethod
    def get_all_descendants(*, actor_name):
        """X.get_all_descendants(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_all_descendants(actor_name)

    @staticmethod
    def get_attachment_depth(*, actor_name):
        """X.get_attachment_depth(actor_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_attachment_depth(actor_name)

    @staticmethod
    def get_attachment_tree(*, actor_name):
        """X.get_attachment_tree(actor_name) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_attachment_tree(actor_name)

    @staticmethod
    def get_component_world_transform(*, actor_name, component_name):
        """X.get_component_world_transform(actor_name, component_name) -> BridgeTransform"""
        return unreal.UnrealBridgeLevelLibrary.get_component_world_transform(actor_name, component_name)

    @staticmethod
    def get_current_level_path():
        """X.get_current_level_path() -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_current_level_path()

    @staticmethod
    def get_ground_height_at(*, x, y, start_height=100000.000000):
        """X.get_ground_height_at(x, y, start_height=100000.000000) -> float"""
        return unreal.UnrealBridgeLevelLibrary.get_ground_height_at(x, y, start_height)

    @staticmethod
    def get_ground_hit_actor(*, x, y, start_height=100000.000000):
        """X.get_ground_hit_actor(x, y, start_height=100000.000000) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_ground_hit_actor(x, y, start_height)

    @staticmethod
    def get_ground_normal_at(*, x, y, start_height=100000.000000):
        """X.get_ground_normal_at(x, y, start_height=100000.000000) -> Vector or None"""
        return unreal.UnrealBridgeLevelLibrary.get_ground_normal_at(x, y, start_height)

    @staticmethod
    def get_height_at(*, x, y, z_start, z_end):
        """X.get_height_at(x, y, z_start, z_end) -> (out_actor_label=str, out_ground_z=float) or None"""
        return unreal.UnrealBridgeLevelLibrary.get_height_at(x, y, z_start, z_end)

    @staticmethod
    def get_height_profile_along(*, start_xy, end_xy, sample_count, z_start, z_end):
        """X.get_height_profile_along(start_xy, end_xy, sample_count, z_start, z_end) -> (int32, out_heights=Array[float], out_actor_labels=Array[str])"""
        return unreal.UnrealBridgeLevelLibrary.get_height_profile_along(start_xy, end_xy, sample_count, z_start, z_end)

    @staticmethod
    def get_hidden_actor_names():
        """X.get_hidden_actor_names() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_hidden_actor_names()

    @staticmethod
    def get_kill_z():
        """X.get_kill_z() -> float"""
        return unreal.UnrealBridgeLevelLibrary.get_kill_z()

    @staticmethod
    def get_level_bounds():
        """X.get_level_bounds() -> BridgeActorBounds"""
        return unreal.UnrealBridgeLevelLibrary.get_level_bounds()

    @staticmethod
    def get_level_summary():
        """X.get_level_summary() -> BridgeLevelSummary"""
        return unreal.UnrealBridgeLevelLibrary.get_level_summary()

    @staticmethod
    def get_persistent_level_actor_count():
        """X.get_persistent_level_actor_count() -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_persistent_level_actor_count()

    @staticmethod
    def get_root_attach_parent(*, actor_name):
        """X.get_root_attach_parent(actor_name) -> str"""
        return unreal.UnrealBridgeLevelLibrary.get_root_attach_parent(actor_name)

    @staticmethod
    def get_selected_actors():
        """X.get_selected_actors() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_selected_actors()

    @staticmethod
    def get_selection_bounds():
        """X.get_selection_bounds() -> BridgeActorBounds"""
        return unreal.UnrealBridgeLevelLibrary.get_selection_bounds()

    @staticmethod
    def get_selection_centroid():
        """X.get_selection_centroid() -> Vector"""
        return unreal.UnrealBridgeLevelLibrary.get_selection_centroid()

    @staticmethod
    def get_selection_class_set():
        """X.get_selection_class_set() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.get_selection_class_set()

    @staticmethod
    def get_selection_count():
        """X.get_selection_count() -> int32"""
        return unreal.UnrealBridgeLevelLibrary.get_selection_count()

    @staticmethod
    def get_socket_world_transform(*, actor_name, component_name, socket_name):
        """X.get_socket_world_transform(actor_name, component_name, socket_name) -> BridgeTransform"""
        return unreal.UnrealBridgeLevelLibrary.get_socket_world_transform(actor_name, component_name, socket_name)

    @staticmethod
    def get_streaming_levels():
        """X.get_streaming_levels() -> Array[BridgeStreamingLevel]"""
        return unreal.UnrealBridgeLevelLibrary.get_streaming_levels()

    @staticmethod
    def get_world_gravity():
        """X.get_world_gravity() -> float"""
        return unreal.UnrealBridgeLevelLibrary.get_world_gravity()

    @staticmethod
    def invert_selection():
        """X.invert_selection() -> int32"""
        return unreal.UnrealBridgeLevelLibrary.invert_selection()

    @staticmethod
    def invoke_function_on_actor(*, actor_name, function_name, args_json):
        """X.invoke_function_on_actor(actor_name, function_name, args_json) -> (out_result_json=str, out_error=str) or None"""
        return unreal.UnrealBridgeLevelLibrary.invoke_function_on_actor(actor_name, function_name, args_json)

    @staticmethod
    def is_actor_in_cone(*, actor_name, origin, direction, half_angle_deg, max_distance):
        """X.is_actor_in_cone(actor_name, origin, direction, half_angle_deg, max_distance) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.is_actor_in_cone(actor_name, origin, direction, half_angle_deg, max_distance)

    @staticmethod
    def is_actor_of_class(*, actor_name, class_path):
        """X.is_actor_of_class(actor_name, class_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.is_actor_of_class(actor_name, class_path)

    @staticmethod
    def is_actor_selected(*, actor_name):
        """X.is_actor_selected(actor_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.is_actor_selected(actor_name)

    @staticmethod
    def is_folder_empty(*, folder_path):
        """X.is_folder_empty(folder_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.is_folder_empty(folder_path)

    @staticmethod
    def is_streaming_level_loaded(*, package_name):
        """X.is_streaming_level_loaded(package_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.is_streaming_level_loaded(package_name)

    @staticmethod
    def isolate_actors(*, keep_visible):
        """X.isolate_actors(keep_visible) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.isolate_actors(keep_visible)

    @staticmethod
    def line_trace_first_actor(*, start, end):
        """X.line_trace_first_actor(start, end) -> str"""
        return unreal.UnrealBridgeLevelLibrary.line_trace_first_actor(start, end)

    @staticmethod
    def line_trace_hit_info(*, start, end):
        """X.line_trace_hit_info(start, end) -> (out_actor_label=str, out_distance=float, out_impact_location=Vector) or None"""
        return unreal.UnrealBridgeLevelLibrary.line_trace_hit_info(start, end)

    @staticmethod
    def list_actor_properties(*, actor_name):
        """X.list_actor_properties(actor_name) -> Array[BridgePropertyInfo]"""
        return unreal.UnrealBridgeLevelLibrary.list_actor_properties(actor_name)

    @staticmethod
    def list_actors(*, class_filter, tag_filter, name_filter, selected_only, max_results):
        """X.list_actors(class_filter, tag_filter, name_filter, selected_only, max_results) -> Array[BridgeActorBrief]"""
        return unreal.UnrealBridgeLevelLibrary.list_actors(class_filter, tag_filter, name_filter, selected_only, max_results)

    @staticmethod
    def list_class_properties(*, class_path):
        """X.list_class_properties(class_path) -> Array[BridgePropertyInfo]"""
        return unreal.UnrealBridgeLevelLibrary.list_class_properties(class_path)

    @staticmethod
    def measure_ceiling_height(*, origin, max_up):
        """X.measure_ceiling_height(origin, max_up) -> float"""
        return unreal.UnrealBridgeLevelLibrary.measure_ceiling_height(origin, max_up)

    @staticmethod
    def mirror_actors(*, actor_names, axis):
        """X.mirror_actors(actor_names, axis) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.mirror_actors(actor_names, axis)

    @staticmethod
    def move_actor(*, actor_name, delta_location, delta_rotation):
        """X.move_actor(actor_name, delta_location, delta_rotation) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.move_actor(actor_name, delta_location, delta_rotation)

    @staticmethod
    def move_actors_to_folder(*, actor_names, folder_path):
        """X.move_actors_to_folder(actor_names, folder_path) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.move_actors_to_folder(actor_names, folder_path)

    @staticmethod
    def multi_line_trace_actors(*, start, end):
        """X.multi_line_trace_actors(start, end) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.multi_line_trace_actors(start, end)

    @staticmethod
    def multi_sphere_trace_actors(*, start, end, radius):
        """X.multi_sphere_trace_actors(start, end, radius) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.multi_sphere_trace_actors(start, end, radius)

    @staticmethod
    def nav_graph_add_edge(*, from_, to, cost):
        """X.nav_graph_add_edge(from_, to, cost) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_add_edge(from_, to, cost)

    @staticmethod
    def nav_graph_add_node(*, name, location):
        """X.nav_graph_add_node(name, location) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_add_node(name, location)

    @staticmethod
    def nav_graph_clear():
        """X.nav_graph_clear() -> None"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_clear()

    @staticmethod
    def nav_graph_get_node_location(*, name):
        """X.nav_graph_get_node_location(name) -> Vector or None"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_get_node_location(name)

    @staticmethod
    def nav_graph_list_nodes():
        """X.nav_graph_list_nodes() -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_list_nodes()

    @staticmethod
    def nav_graph_load_json(*, file_path):
        """X.nav_graph_load_json(file_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_load_json(file_path)

    @staticmethod
    def nav_graph_save_json(*, file_path):
        """X.nav_graph_save_json(file_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_save_json(file_path)

    @staticmethod
    def nav_graph_shortest_path(*, from_, to):
        """X.nav_graph_shortest_path(from_, to) -> (Array[str], out_total_cost=float)"""
        return unreal.UnrealBridgeLevelLibrary.nav_graph_shortest_path(from_, to)

    @staticmethod
    def offset_actors(*, actor_names, delta_location):
        """X.offset_actors(actor_names, delta_location) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.offset_actors(actor_names, delta_location)

    @staticmethod
    def overlap_sphere_actors(*, center, radius, class_filter):
        """X.overlap_sphere_actors(center, radius, class_filter) -> Array[str]"""
        return unreal.UnrealBridgeLevelLibrary.overlap_sphere_actors(center, radius, class_filter)

    @staticmethod
    def probe_fan_xy(*, origin, num_rays, max_distance, start_angle_deg, span_deg):
        """X.probe_fan_xy(origin, num_rays, max_distance, start_angle_deg, span_deg) -> (int32, out_distances=Array[float], out_actor_labels=Array[str])"""
        return unreal.UnrealBridgeLevelLibrary.probe_fan_xy(origin, num_rays, max_distance, start_angle_deg, span_deg)

    @staticmethod
    def remove_actor_tag(*, actor_name, tag):
        """X.remove_actor_tag(actor_name, tag) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.remove_actor_tag(actor_name, tag)

    @staticmethod
    def remove_component(*, actor_name, component_name):
        """X.remove_component(actor_name, component_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.remove_component(actor_name, component_name)

    @staticmethod
    def remove_tag_from_all_actors(*, tag):
        """X.remove_tag_from_all_actors(tag) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.remove_tag_from_all_actors(tag)

    @staticmethod
    def rename_folder(*, old_folder, new_folder):
        """X.rename_folder(old_folder, new_folder) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.rename_folder(old_folder, new_folder)

    @staticmethod
    def reset_actor_materials(*, actor_name):
        """X.reset_actor_materials(actor_name) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.reset_actor_materials(actor_name)

    @staticmethod
    def rotate_actors(*, actor_names, delta_rotation):
        """X.rotate_actors(actor_names, delta_rotation) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.rotate_actors(actor_names, delta_rotation)

    @staticmethod
    def scale_actors(*, actor_names, scale_multiplier):
        """X.scale_actors(actor_names, scale_multiplier) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.scale_actors(actor_names, scale_multiplier)

    @staticmethod
    def select_actors(*, actor_names, add_to_selection):
        """X.select_actors(actor_names, add_to_selection) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.select_actors(actor_names, add_to_selection)

    @staticmethod
    def select_actors_by_tag(*, tag, add_to_selection=False):
        """X.select_actors_by_tag(tag, add_to_selection=False) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.select_actors_by_tag(tag, add_to_selection)

    @staticmethod
    def select_actors_in_box(*, min, max, add_to_selection=False):
        """X.select_actors_in_box(min, max, add_to_selection=False) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.select_actors_in_box(min, max, add_to_selection)

    @staticmethod
    def select_actors_in_sphere(*, center, radius, add_to_selection=False):
        """X.select_actors_in_sphere(center, radius, add_to_selection=False) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.select_actors_in_sphere(center, radius, add_to_selection)

    @staticmethod
    def select_all_actors():
        """X.select_all_actors() -> int32"""
        return unreal.UnrealBridgeLevelLibrary.select_all_actors()

    @staticmethod
    def set_actor_collision_profile(*, actor_name, profile_name):
        """X.set_actor_collision_profile(actor_name, profile_name) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_collision_profile(actor_name, profile_name)

    @staticmethod
    def set_actor_enable_collision(*, actor_name, enabled):
        """X.set_actor_enable_collision(actor_name, enabled) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_enable_collision(actor_name, enabled)

    @staticmethod
    def set_actor_folder(*, actor_name, folder_path):
        """X.set_actor_folder(actor_name, folder_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_folder(actor_name, folder_path)

    @staticmethod
    def set_actor_hidden_in_editor(*, actor_name, hidden):
        """X.set_actor_hidden_in_editor(actor_name, hidden) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_hidden_in_editor(actor_name, hidden)

    @staticmethod
    def set_actor_hidden_in_game(*, actor_name, hidden):
        """X.set_actor_hidden_in_game(actor_name, hidden) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_hidden_in_game(actor_name, hidden)

    @staticmethod
    def set_actor_label(*, actor_name, new_label):
        """X.set_actor_label(actor_name, new_label) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_label(actor_name, new_label)

    @staticmethod
    def set_actor_material(*, actor_name, material_index, material_asset_path):
        """X.set_actor_material(actor_name, material_index, material_asset_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_material(actor_name, material_index, material_asset_path)

    @staticmethod
    def set_actor_mesh(*, actor_name, mesh_asset_path):
        """X.set_actor_mesh(actor_name, mesh_asset_path) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_mesh(actor_name, mesh_asset_path)

    @staticmethod
    def set_actor_property(*, actor_name, property_path, exported_value):
        """X.set_actor_property(actor_name, property_path, exported_value) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_property(actor_name, property_path, exported_value)

    @staticmethod
    def set_actor_simulate_physics(*, actor_name, simulate):
        """X.set_actor_simulate_physics(actor_name, simulate) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_simulate_physics(actor_name, simulate)

    @staticmethod
    def set_actor_transform(*, actor_name, location, rotation, scale):
        """X.set_actor_transform(actor_name, location, rotation, scale) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_actor_transform(actor_name, location, rotation, scale)

    @staticmethod
    def set_actors_uniform_scale(*, actor_names, uniform_scale):
        """X.set_actors_uniform_scale(actor_names, uniform_scale) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.set_actors_uniform_scale(actor_names, uniform_scale)

    @staticmethod
    def set_component_mobility(*, actor_name, component_name, mobility):
        """X.set_component_mobility(actor_name, component_name, mobility) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_component_mobility(actor_name, component_name, mobility)

    @staticmethod
    def set_component_relative_transform(*, actor_name, component_name, location, rotation, scale):
        """X.set_component_relative_transform(actor_name, component_name, location, rotation, scale) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_component_relative_transform(actor_name, component_name, location, rotation, scale)

    @staticmethod
    def set_component_visibility(*, actor_name, component_name, visible, propagate_to_children):
        """X.set_component_visibility(actor_name, component_name, visible, propagate_to_children) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_component_visibility(actor_name, component_name, visible, propagate_to_children)

    @staticmethod
    def set_kill_z(*, new_kill_z):
        """X.set_kill_z(new_kill_z) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_kill_z(new_kill_z)

    @staticmethod
    def set_streaming_level_loaded(*, package_name, loaded):
        """X.set_streaming_level_loaded(package_name, loaded) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_streaming_level_loaded(package_name, loaded)

    @staticmethod
    def set_streaming_level_visible(*, package_name, visible):
        """X.set_streaming_level_visible(package_name, visible) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_streaming_level_visible(package_name, visible)

    @staticmethod
    def set_world_gravity(*, gravity, override=True):
        """X.set_world_gravity(gravity, override=True) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.set_world_gravity(gravity, override)

    @staticmethod
    def show_all_actors():
        """X.show_all_actors() -> int32"""
        return unreal.UnrealBridgeLevelLibrary.show_all_actors()

    @staticmethod
    def snap_actor_to_floor(*, actor_name, max_distance=10000.000000):
        """X.snap_actor_to_floor(actor_name, max_distance=10000.000000) -> bool"""
        return unreal.UnrealBridgeLevelLibrary.snap_actor_to_floor(actor_name, max_distance)

    @staticmethod
    def snap_actors_to_grid(*, actor_names, grid_size):
        """X.snap_actors_to_grid(actor_names, grid_size) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.snap_actors_to_grid(actor_names, grid_size)

    @staticmethod
    def spawn_actor(*, class_path, location, rotation):
        """X.spawn_actor(class_path, location, rotation) -> str"""
        return unreal.UnrealBridgeLevelLibrary.spawn_actor(class_path, location, rotation)

    @staticmethod
    def sphere_trace_first_actor(*, start, end, radius):
        """X.sphere_trace_first_actor(start, end, radius) -> str"""
        return unreal.UnrealBridgeLevelLibrary.sphere_trace_first_actor(start, end, radius)

    @staticmethod
    def toggle_actors_hidden(*, actor_names):
        """X.toggle_actors_hidden(actor_names) -> int32"""
        return unreal.UnrealBridgeLevelLibrary.toggle_actors_hidden(actor_names)


class Material:
    """Wraps unreal.UnrealBridgeMaterialLibrary (kwargs-only)."""

    @staticmethod
    def add_custom_expression(*, material_path, x, y, input_names, output_type, code, include_paths, description):
        """X.add_custom_expression(material_path, x, y, input_names, output_type, code, include_paths, description) -> BridgeAddExpressionResult"""
        return unreal.UnrealBridgeMaterialLibrary.add_custom_expression(material_path, x, y, input_names, output_type, code, include_paths, description)

    @staticmethod
    def add_material_comment(*, material_path, x, y, width, height, text, color):
        """X.add_material_comment(material_path, x, y, width, height, text, color) -> Guid"""
        return unreal.UnrealBridgeMaterialLibrary.add_material_comment(material_path, x, y, width, height, text, color)

    @staticmethod
    def add_material_expression(*, material_path, expression_class, x, y):
        """X.add_material_expression(material_path, expression_class, x, y) -> BridgeAddExpressionResult"""
        return unreal.UnrealBridgeMaterialLibrary.add_material_expression(material_path, expression_class, x, y)

    @staticmethod
    def add_material_reroute(*, material_path, x, y):
        """X.add_material_reroute(material_path, x, y) -> Guid"""
        return unreal.UnrealBridgeMaterialLibrary.add_material_reroute(material_path, x, y)

    @staticmethod
    def analyze_material(*, material_path, instruction_budget, sampler_budget):
        """X.analyze_material(material_path, instruction_budget, sampler_budget) -> BridgeMaterialAnalysis"""
        return unreal.UnrealBridgeMaterialLibrary.analyze_material(material_path, instruction_budget, sampler_budget)

    @staticmethod
    def apply_material_graph_ops(*, material_path, ops, compile):
        """X.apply_material_graph_ops(material_path, ops, compile) -> BridgeMaterialGraphOpResult"""
        return unreal.UnrealBridgeMaterialLibrary.apply_material_graph_ops(material_path, ops, compile)

    @staticmethod
    def apply_post_process_material(*, volume_actor, material_path, weight):
        """X.apply_post_process_material(volume_actor, material_path, weight) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.apply_post_process_material(volume_actor, material_path, weight)

    @staticmethod
    def auto_fix_material(*, material_path, fixes, save_after):
        """X.auto_fix_material(material_path, fixes, save_after) -> BridgeMaterialAutoFixResult"""
        return unreal.UnrealBridgeMaterialLibrary.auto_fix_material(material_path, fixes, save_after)

    @staticmethod
    def auto_layout_material_graph(*, material_path, column_spacing, row_spacing):
        """X.auto_layout_material_graph(material_path, column_spacing, row_spacing) -> int32"""
        return unreal.UnrealBridgeMaterialLibrary.auto_layout_material_graph(material_path, column_spacing, row_spacing)

    @staticmethod
    def compile_material(*, material_path, save_after):
        """X.compile_material(material_path, save_after) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.compile_material(material_path, save_after)

    @staticmethod
    def connect_material_expressions(*, material_path, src_guid, src_output_name, dst_guid, dst_input_name):
        """X.connect_material_expressions(material_path, src_guid, src_output_name, dst_guid, dst_input_name) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.connect_material_expressions(material_path, src_guid, src_output_name, dst_guid, dst_input_name)

    @staticmethod
    def connect_material_output(*, material_path, src_guid, src_output_name, property_name):
        """X.connect_material_output(material_path, src_guid, src_output_name, property_name) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.connect_material_output(material_path, src_guid, src_output_name, property_name)

    @staticmethod
    def create_material(*, path, domain, shading_model, blend_mode, two_sided, use_material_attributes):
        """X.create_material(path, domain, shading_model, blend_mode, two_sided, use_material_attributes) -> BridgeCreateAssetResult"""
        return unreal.UnrealBridgeMaterialLibrary.create_material(path, domain, shading_model, blend_mode, two_sided, use_material_attributes)

    @staticmethod
    def create_material_function(*, path, description, expose_to_library, library_category):
        """X.create_material_function(path, description, expose_to_library, library_category) -> BridgeCreateAssetResult"""
        return unreal.UnrealBridgeMaterialLibrary.create_material_function(path, description, expose_to_library, library_category)

    @staticmethod
    def create_material_instance(*, parent_path, instance_path):
        """X.create_material_instance(parent_path, instance_path) -> BridgeCreateAssetResult"""
        return unreal.UnrealBridgeMaterialLibrary.create_material_instance(parent_path, instance_path)

    @staticmethod
    def create_post_process_material(*, path, blendable_location, output_alpha):
        """X.create_post_process_material(path, blendable_location, output_alpha) -> BridgeCreateAssetResult"""
        return unreal.UnrealBridgeMaterialLibrary.create_post_process_material(path, blendable_location, output_alpha)

    @staticmethod
    def delete_material_expression(*, material_path, guid):
        """X.delete_material_expression(material_path, guid) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.delete_material_expression(material_path, guid)

    @staticmethod
    def diff_material_graph_snapshots(*, before_json, after_json):
        """X.diff_material_graph_snapshots(before_json, after_json) -> str"""
        return unreal.UnrealBridgeMaterialLibrary.diff_material_graph_snapshots(before_json, after_json)

    @staticmethod
    def diff_mi_params(*, path_a, path_b):
        """X.diff_mi_params(path_a, path_b) -> str"""
        return unreal.UnrealBridgeMaterialLibrary.diff_mi_params(path_a, path_b)

    @staticmethod
    def disconnect_material_input(*, material_path, dst_guid, dst_input_name):
        """X.disconnect_material_input(material_path, dst_guid, dst_input_name) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.disconnect_material_input(material_path, dst_guid, dst_input_name)

    @staticmethod
    def disconnect_material_output(*, material_path, property_name):
        """X.disconnect_material_output(material_path, property_name) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.disconnect_material_output(material_path, property_name)

    @staticmethod
    def get_material_compile_errors(*, material_path, feature_level, quality):
        """X.get_material_compile_errors(material_path, feature_level, quality) -> Array[str]"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_compile_errors(material_path, feature_level, quality)

    @staticmethod
    def get_material_function(*, function_path):
        """X.get_material_function(function_path) -> BridgeMaterialFunctionInfo"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_function(function_path)

    @staticmethod
    def get_material_graph(*, material_path):
        """X.get_material_graph(material_path) -> BridgeMaterialGraph"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_graph(material_path)

    @staticmethod
    def get_material_info(*, material_path):
        """X.get_material_info(material_path) -> BridgeMaterialInfo"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_info(material_path)

    @staticmethod
    def get_material_instance_parameters(*, material_path):
        """X.get_material_instance_parameters(material_path) -> BridgeMaterialInstanceInfo"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_instance_parameters(material_path)

    @staticmethod
    def get_material_parameter_collection(*, collection_path):
        """X.get_material_parameter_collection(collection_path) -> BridgeMaterialParameterCollectionInfo"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_parameter_collection(collection_path)

    @staticmethod
    def get_material_shader_compile_status(*, material_path, feature_level, quality):
        """X.get_material_shader_compile_status(material_path, feature_level, quality) -> BridgeShaderCompileStatus"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_shader_compile_status(material_path, feature_level, quality)

    @staticmethod
    def get_material_stats(*, material_path, feature_level, quality):
        """X.get_material_stats(material_path, feature_level, quality) -> BridgeMaterialStats"""
        return unreal.UnrealBridgeMaterialLibrary.get_material_stats(material_path, feature_level, quality)

    @staticmethod
    def get_post_process_state():
        """X.get_post_process_state() -> Array[BridgePostProcessVolumeInfo]"""
        return unreal.UnrealBridgeMaterialLibrary.get_post_process_state()

    @staticmethod
    def get_shared_snippet(*, name):
        """X.get_shared_snippet(name) -> BridgeShaderSnippet"""
        return unreal.UnrealBridgeMaterialLibrary.get_shared_snippet(name)

    @staticmethod
    def list_material_functions(*, path_prefix, max_results):
        """X.list_material_functions(path_prefix, max_results) -> Array[BridgeMaterialFunctionSummary]"""
        return unreal.UnrealBridgeMaterialLibrary.list_material_functions(path_prefix, max_results)

    @staticmethod
    def list_material_instance_chain(*, material_path):
        """X.list_material_instance_chain(material_path) -> BridgeMaterialInstanceChain"""
        return unreal.UnrealBridgeMaterialLibrary.list_material_instance_chain(material_path)

    @staticmethod
    def list_shared_snippets():
        """X.list_shared_snippets() -> Array[BridgeShaderSnippet]"""
        return unreal.UnrealBridgeMaterialLibrary.list_shared_snippets()

    @staticmethod
    def preview_material(*, material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path):
        """X.preview_material(material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.preview_material(material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path)

    @staticmethod
    def preview_material_complexity(*, material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path):
        """X.preview_material_complexity(material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.preview_material_complexity(material_path, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path)

    @staticmethod
    def remove_post_process_material(*, volume_actor, material_path):
        """X.remove_post_process_material(volume_actor, material_path) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.remove_post_process_material(volume_actor, material_path)

    @staticmethod
    def set_material_attribute_layers(*, material_path, expression_guid, layers, blends, layer_names):
        """X.set_material_attribute_layers(material_path, expression_guid, layers, blends, layer_names) -> BridgeMaterialGraphOpResult"""
        return unreal.UnrealBridgeMaterialLibrary.set_material_attribute_layers(material_path, expression_guid, layers, blends, layer_names)

    @staticmethod
    def set_material_expression_properties(*, material_path, guid, properties):
        """X.set_material_expression_properties(material_path, guid, properties) -> int32"""
        return unreal.UnrealBridgeMaterialLibrary.set_material_expression_properties(material_path, guid, properties)

    @staticmethod
    def set_material_expression_property(*, material_path, guid, property_name, value):
        """X.set_material_expression_property(material_path, guid, property_name, value) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.set_material_expression_property(material_path, guid, property_name, value)

    @staticmethod
    def set_material_parameter_collection(*, collection_path, params):
        """X.set_material_parameter_collection(collection_path, params) -> BridgeMIParamResult"""
        return unreal.UnrealBridgeMaterialLibrary.set_material_parameter_collection(collection_path, params)

    @staticmethod
    def set_mi_and_preview(*, material_instance_path, params, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path):
        """X.set_mi_and_preview(material_instance_path, params, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path) -> bool"""
        return unreal.UnrealBridgeMaterialLibrary.set_mi_and_preview(material_instance_path, params, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, out_png_path)

    @staticmethod
    def set_mi_params(*, material_instance_path, params):
        """X.set_mi_params(material_instance_path, params) -> BridgeMIParamResult"""
        return unreal.UnrealBridgeMaterialLibrary.set_mi_params(material_instance_path, params)

    @staticmethod
    def snapshot_material_graph_json(*, material_path):
        """X.snapshot_material_graph_json(material_path) -> str"""
        return unreal.UnrealBridgeMaterialLibrary.snapshot_material_graph_json(material_path)

    @staticmethod
    def sweep_mi_params(*, material_instance_path, param_name, values, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, grid_cols, out_grid_path):
        """X.sweep_mi_params(material_instance_path, param_name, values, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, grid_cols, out_grid_path) -> Array[str]"""
        return unreal.UnrealBridgeMaterialLibrary.sweep_mi_params(material_instance_path, param_name, values, mesh, lighting, resolution, camera_yaw_deg, camera_pitch_deg, camera_distance, grid_cols, out_grid_path)


class Navigation:
    """Wraps unreal.UnrealBridgeNavigationLibrary (kwargs-only)."""

    @staticmethod
    def export_nav_mesh_to_obj(*, out_file_path):
        """X.export_nav_mesh_to_obj(out_file_path) -> str or None"""
        return unreal.UnrealBridgeNavigationLibrary.export_nav_mesh_to_obj(out_file_path)


class Perf:
    """Wraps unreal.UnrealBridgePerfLibrary (kwargs-only)."""

    @staticmethod
    def get_frame_timing():
        """X.get_frame_timing() -> BridgeFrameTiming"""
        return unreal.UnrealBridgePerfLibrary.get_frame_timing()

    @staticmethod
    def get_memory_stats():
        """X.get_memory_stats() -> BridgeMemoryStats"""
        return unreal.UnrealBridgePerfLibrary.get_memory_stats()

    @staticmethod
    def get_perf_snapshot(*, include_u_object_stats=False, u_object_top_n=20):
        """X.get_perf_snapshot(include_u_object_stats=False, u_object_top_n=20) -> BridgePerfSnapshot"""
        return unreal.UnrealBridgePerfLibrary.get_perf_snapshot(include_u_object_stats, u_object_top_n)

    @staticmethod
    def get_render_counters():
        """X.get_render_counters() -> BridgeRenderCounters"""
        return unreal.UnrealBridgePerfLibrary.get_render_counters()

    @staticmethod
    def get_u_object_stats(*, top_n=20):
        """X.get_u_object_stats(top_n=20) -> BridgeUObjectStats"""
        return unreal.UnrealBridgePerfLibrary.get_u_object_stats(top_n)


class PoseSearch:
    """Wraps unreal.UnrealBridgePoseSearchLibrary (kwargs-only)."""

    @staticmethod
    def add_animation_to_database(*, database_path, animation_asset_path, sampling_range_min, sampling_range_max, mirror_option, enabled):
        """X.add_animation_to_database(database_path, animation_asset_path, sampling_range_min, sampling_range_max, mirror_option, enabled) -> BridgePSDAddResult"""
        return unreal.UnrealBridgePoseSearchLibrary.add_animation_to_database(database_path, animation_asset_path, sampling_range_min, sampling_range_max, mirror_option, enabled)

    @staticmethod
    def add_blend_space_to_database(*, database_path, blend_space_path, h_samples, v_samples, use_grid_for_sampling, use_single_sample, blend_param_x, blend_param_y, sampling_range_min, sampling_range_max, mirror_option, enabled):
        """X.add_blend_space_to_database(database_path, blend_space_path, h_samples, v_samples, use_grid_for_sampling, use_single_sample, blend_param_x, blend_param_y, sampling_range_min, sampling_range_max, mirror_option, enabled) -> BridgePSDAddResult"""
        return unreal.UnrealBridgePoseSearchLibrary.add_blend_space_to_database(database_path, blend_space_path, h_samples, v_samples, use_grid_for_sampling, use_single_sample, blend_param_x, blend_param_y, sampling_range_min, sampling_range_max, mirror_option, enabled)

    @staticmethod
    def clear_database_animations(*, database_path):
        """X.clear_database_animations(database_path) -> int32"""
        return unreal.UnrealBridgePoseSearchLibrary.clear_database_animations(database_path)

    @staticmethod
    def find_databases_using_animation(*, animation_asset_path):
        """X.find_databases_using_animation(animation_asset_path) -> Array[str]"""
        return unreal.UnrealBridgePoseSearchLibrary.find_databases_using_animation(animation_asset_path)

    @staticmethod
    def get_database_info(*, database_path):
        """X.get_database_info(database_path) -> BridgePSDInfo"""
        return unreal.UnrealBridgePoseSearchLibrary.get_database_info(database_path)

    @staticmethod
    def get_index_status(*, database_path):
        """X.get_index_status(database_path) -> str"""
        return unreal.UnrealBridgePoseSearchLibrary.get_index_status(database_path)

    @staticmethod
    def get_schema_info(*, schema_path):
        """X.get_schema_info(schema_path) -> BridgePSSInfo"""
        return unreal.UnrealBridgePoseSearchLibrary.get_schema_info(schema_path)

    @staticmethod
    def invalidate_index(*, database_path):
        """X.invalidate_index(database_path) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.invalidate_index(database_path)

    @staticmethod
    def is_index_ready(*, database_path):
        """X.is_index_ready(database_path) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.is_index_ready(database_path)

    @staticmethod
    def list_database_animations(*, database_path):
        """X.list_database_animations(database_path) -> Array[BridgePSDAnimEntry]"""
        return unreal.UnrealBridgePoseSearchLibrary.list_database_animations(database_path)

    @staticmethod
    def list_schema_channels(*, schema_path):
        """X.list_schema_channels(schema_path) -> Array[BridgePSSChannel]"""
        return unreal.UnrealBridgePoseSearchLibrary.list_schema_channels(schema_path)

    @staticmethod
    def remove_database_animation_at(*, database_path, index):
        """X.remove_database_animation_at(database_path, index) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.remove_database_animation_at(database_path, index)

    @staticmethod
    def remove_database_animation_by_asset(*, database_path, animation_asset_path):
        """X.remove_database_animation_by_asset(database_path, animation_asset_path) -> int32"""
        return unreal.UnrealBridgePoseSearchLibrary.remove_database_animation_by_asset(database_path, animation_asset_path)

    @staticmethod
    def request_async_build_index(*, database_path):
        """X.request_async_build_index(database_path) -> str"""
        return unreal.UnrealBridgePoseSearchLibrary.request_async_build_index(database_path)

    @staticmethod
    def set_database_animation_enabled(*, database_path, index, enabled):
        """X.set_database_animation_enabled(database_path, index, enabled) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.set_database_animation_enabled(database_path, index, enabled)

    @staticmethod
    def set_database_animation_mirror_option(*, database_path, index, mirror_option):
        """X.set_database_animation_mirror_option(database_path, index, mirror_option) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.set_database_animation_mirror_option(database_path, index, mirror_option)

    @staticmethod
    def set_database_animation_sampling_range(*, database_path, index, sampling_range_min, sampling_range_max):
        """X.set_database_animation_sampling_range(database_path, index, sampling_range_min, sampling_range_max) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.set_database_animation_sampling_range(database_path, index, sampling_range_min, sampling_range_max)

    @staticmethod
    def set_database_blend_space_sampling(*, database_path, index, h_samples, v_samples, use_grid_for_sampling, use_single_sample, blend_param_x, blend_param_y):
        """X.set_database_blend_space_sampling(database_path, index, h_samples, v_samples, use_grid_for_sampling, use_single_sample, blend_param_x, blend_param_y) -> bool"""
        return unreal.UnrealBridgePoseSearchLibrary.set_database_blend_space_sampling(database_path, index, h_samples, v_samples, use_grid_for_sampling, use_single_sample, blend_param_x, blend_param_y)


class Reactive:
    """Wraps unreal.UnrealBridgeReactiveLibrary (kwargs-only)."""

    @staticmethod
    def clear_all(*, scope):
        """X.clear_all(scope) -> int32"""
        return unreal.UnrealBridgeReactiveLibrary.clear_all(scope)

    @staticmethod
    def defer_to_next_tick(*, script):
        """X.defer_to_next_tick(script) -> None"""
        return unreal.UnrealBridgeReactiveLibrary.defer_to_next_tick(script)

    @staticmethod
    def describe_trigger_context(*, trigger_type):
        """X.describe_trigger_context(trigger_type) -> Map[str, str]"""
        return unreal.UnrealBridgeReactiveLibrary.describe_trigger_context(trigger_type)

    @staticmethod
    def get_deferred_handler_count():
        """X.get_deferred_handler_count() -> int32"""
        return unreal.UnrealBridgeReactiveLibrary.get_deferred_handler_count()

    @staticmethod
    def get_handler(*, handler_id):
        """X.get_handler(handler_id) -> BridgeHandlerDetail"""
        return unreal.UnrealBridgeReactiveLibrary.get_handler(handler_id)

    @staticmethod
    def get_handler_stats(*, handler_id):
        """X.get_handler_stats(handler_id) -> BridgeHandlerStats"""
        return unreal.UnrealBridgeReactiveLibrary.get_handler_stats(handler_id)

    @staticmethod
    def get_persistence_path():
        """X.get_persistence_path() -> str"""
        return unreal.UnrealBridgeReactiveLibrary.get_persistence_path()

    @staticmethod
    def list_all_handlers(*, filter_scope, filter_trigger_type, filter_tag):
        """X.list_all_handlers(filter_scope, filter_trigger_type, filter_tag) -> Array[BridgeHandlerSummary]"""
        return unreal.UnrealBridgeReactiveLibrary.list_all_handlers(filter_scope, filter_trigger_type, filter_tag)

    @staticmethod
    def load_all_handlers():
        """X.load_all_handlers() -> int32"""
        return unreal.UnrealBridgeReactiveLibrary.load_all_handlers()

    @staticmethod
    def pause(*, handler_id):
        """X.pause(handler_id) -> bool"""
        return unreal.UnrealBridgeReactiveLibrary.pause(handler_id)

    @staticmethod
    def register_editor_asset_event(*, task_name, description, event_filter, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_editor_asset_event(task_name, description, event_filter, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_editor_asset_event(task_name, description, event_filter, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_editor_bp_compiled(*, task_name, description, blueprint_path_filter, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_editor_bp_compiled(task_name, description, blueprint_path_filter, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_editor_bp_compiled(task_name, description, blueprint_path_filter, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_editor_pie_event(*, task_name, description, phase_filter, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_editor_pie_event(task_name, description, phase_filter, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_editor_pie_event(task_name, description, phase_filter, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_actor_lifecycle(*, task_name, description, target_actor_name, event_type, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_actor_lifecycle(task_name, description, target_actor_name, event_type, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_actor_lifecycle(task_name, description, target_actor_name, event_type, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_anim_notify(*, task_name, description, target_actor_name, notify_name, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_anim_notify(task_name, description, target_actor_name, notify_name, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_anim_notify(task_name, description, target_actor_name, notify_name, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_attribute_changed(*, task_name, description, target_actor_name, attribute_name, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_attribute_changed(task_name, description, target_actor_name, attribute_name, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_attribute_changed(task_name, description, target_actor_name, attribute_name, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_gameplay_event(*, task_name, description, target_actor_name, event_tag, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_gameplay_event(task_name, description, target_actor_name, event_tag, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_gameplay_event(task_name, description, target_actor_name, event_tag, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_input_action(*, task_name, description, target_actor_name, input_action_path, trigger_event, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_input_action(task_name, description, target_actor_name, input_action_path, trigger_event, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_input_action(task_name, description, target_actor_name, input_action_path, trigger_event, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_movement_mode_changed(*, task_name, description, target_actor_name, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_movement_mode_changed(task_name, description, target_actor_name, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_movement_mode_changed(task_name, description, target_actor_name, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def register_runtime_timer(*, task_name, description, interval_seconds, script, script_path, tags, lifetime, error_policy, throttle_ms):
        """X.register_runtime_timer(task_name, description, interval_seconds, script, script_path, tags, lifetime, error_policy, throttle_ms) -> str"""
        return unreal.UnrealBridgeReactiveLibrary.register_runtime_timer(task_name, description, interval_seconds, script, script_path, tags, lifetime, error_policy, throttle_ms)

    @staticmethod
    def resume(*, handler_id):
        """X.resume(handler_id) -> bool"""
        return unreal.UnrealBridgeReactiveLibrary.resume(handler_id)

    @staticmethod
    def save_all_handlers():
        """X.save_all_handlers() -> bool"""
        return unreal.UnrealBridgeReactiveLibrary.save_all_handlers()

    @staticmethod
    def unregister(*, handler_id):
        """X.unregister(handler_id) -> bool"""
        return unreal.UnrealBridgeReactiveLibrary.unregister(handler_id)


class UMG:
    """Wraps unreal.UnrealBridgeUMGLibrary (kwargs-only)."""

    @staticmethod
    def get_widget_animations(*, widget_blueprint_path):
        """X.get_widget_animations(widget_blueprint_path) -> Array[BridgeWidgetAnimationInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_animations(widget_blueprint_path)

    @staticmethod
    def get_widget_bindings(*, widget_blueprint_path):
        """X.get_widget_bindings(widget_blueprint_path) -> Array[BridgeWidgetBindingInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_bindings(widget_blueprint_path)

    @staticmethod
    def get_widget_events(*, widget_blueprint_path):
        """X.get_widget_events(widget_blueprint_path) -> Array[BridgeWidgetEventInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_events(widget_blueprint_path)

    @staticmethod
    def get_widget_properties(*, widget_blueprint_path, widget_name):
        """X.get_widget_properties(widget_blueprint_path, widget_name) -> Array[BridgeWidgetPropertyValue]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_properties(widget_blueprint_path, widget_name)

    @staticmethod
    def get_widget_tree(*, widget_blueprint_path):
        """X.get_widget_tree(widget_blueprint_path) -> Array[BridgeWidgetInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_tree(widget_blueprint_path)

    @staticmethod
    def search_widgets(*, widget_blueprint_path, query):
        """X.search_widgets(widget_blueprint_path, query) -> Array[BridgeWidgetInfo]"""
        return unreal.UnrealBridgeUMGLibrary.search_widgets(widget_blueprint_path, query)

    @staticmethod
    def set_widget_property(*, widget_blueprint_path, widget_name, property_name, value):
        """X.set_widget_property(widget_blueprint_path, widget_name, property_name, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_widget_property(widget_blueprint_path, widget_name, property_name, value)


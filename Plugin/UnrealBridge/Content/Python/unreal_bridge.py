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

_GENERATED_AT = '2026-08-06T15:18:20+00:00'
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
    def add_anim_notify_state(*, animation_path, notify_state_class_path, notify_track_name, start_time, end_time):
        """X.add_anim_notify_state(animation_path, notify_state_class_path, notify_track_name, start_time, end_time) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.add_anim_notify_state(animation_path, notify_state_class_path, notify_track_name, start_time, end_time)

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
    def copy_and_apply_animation_modifiers(*, source_sequence_path, target_sequence_paths):
        """X.copy_and_apply_animation_modifiers(source_sequence_path, target_sequence_paths) -> int32"""
        return unreal.UnrealBridgeAnimLibrary.copy_and_apply_animation_modifiers(source_sequence_path, target_sequence_paths)

    @staticmethod
    def disconnect_anim_graph_pin(*, anim_blueprint_path, graph_name, node_guid, pin_name):
        """X.disconnect_anim_graph_pin(anim_blueprint_path, graph_name, node_guid, pin_name) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.disconnect_anim_graph_pin(anim_blueprint_path, graph_name, node_guid, pin_name)

    @staticmethod
    def ensure_pose_history_collected_bones(*, anim_blueprint_path, graph_name, node_guid, bone_names):
        """X.ensure_pose_history_collected_bones(anim_blueprint_path, graph_name, node_guid, bone_names) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.ensure_pose_history_collected_bones(anim_blueprint_path, graph_name, node_guid, bone_names)

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
        """X.get_anim_node_details(anim_blueprint_path, node_index) -> Array[str]  Index-based addressing is fragile + top-level AnimGraph only. For state-machine interiors / transition rules / sub-graphs, use get_anim_node_details_by_guid(abp_path, graph_name, node_guid)."""
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
    def get_motion_warping_notifies(*, animation_path):
        """X.get_motion_warping_notifies(animation_path) -> Array[BridgeMotionWarpingNotifyInfo]"""
        return unreal.UnrealBridgeAnimLibrary.get_motion_warping_notifies(animation_path)

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
    def remove_anim_notify_states_by_class(*, animation_path, notify_state_class_path):
        """X.remove_anim_notify_states_by_class(animation_path, notify_state_class_path) -> int32"""
        return unreal.UnrealBridgeAnimLibrary.remove_anim_notify_states_by_class(animation_path, notify_state_class_path)

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
    def set_motion_warping_notify(*, animation_path, warp_target_name, start_time, end_time):
        """X.set_motion_warping_notify(animation_path, warp_target_name, start_time, end_time) -> bool"""
        return unreal.UnrealBridgeAnimLibrary.set_motion_warping_notify(animation_path, warp_target_name, start_time, end_time)

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
    def find_assets_referencing_searchable_name(*, struct_type, value_name, package_path_filter, max_results):
        """X.find_assets_referencing_searchable_name(struct_type, value_name, package_path_filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.find_assets_referencing_searchable_name(struct_type, value_name, package_path_filter, max_results)

    @staticmethod
    def find_redirectors_under_path(*, folder_path, recursive):
        """X.find_redirectors_under_path(folder_path, recursive) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
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
        """X.get_asset_references(asset_path) -> (out_dependencies=Array[SoftObjectPath], out_referencers=Array[SoftObjectPath])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
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
        """X.get_assets_by_class(class_path, search_sub_classes) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_by_class(class_path, search_sub_classes)

    @staticmethod
    def get_assets_by_package_paths(*, folder_paths, class_filter, recursive):
        """X.get_assets_by_package_paths(folder_paths, class_filter, recursive) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_by_package_paths(folder_paths, class_filter, recursive)

    @staticmethod
    def get_assets_by_tag_value(*, tag_name, tag_value, optional_class_path):
        """X.get_assets_by_tag_value(tag_name, tag_value, optional_class_path) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_by_tag_value(tag_name, tag_value, optional_class_path)

    @staticmethod
    def get_assets_of_classes(*, class_paths, search_sub_classes):
        """X.get_assets_of_classes(class_paths, search_sub_classes) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.get_assets_of_classes(class_paths, search_sub_classes)

    @staticmethod
    def get_content_roots():
        """X.get_content_roots() -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.get_content_roots()

    @staticmethod
    def get_data_asset_soft_paths_by_asset_path(*, data_asset_path):
        """X.get_data_asset_soft_paths_by_asset_path(data_asset_path) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.get_data_asset_soft_paths_by_asset_path(data_asset_path)

    @staticmethod
    def get_data_asset_soft_paths_by_base_class(*, base_data_asset_class):
        """X.get_data_asset_soft_paths_by_base_class(base_data_asset_class) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
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
    def get_mesh_material_slots(*, mesh_asset_path):
        """X.get_mesh_material_slots(mesh_asset_path) -> Array[BridgeMeshMaterialSlot]"""
        return unreal.UnrealBridgeAssetLibrary.get_mesh_material_slots(mesh_asset_path)

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
    def get_searchable_names_used_by_asset(*, asset_path, struct_type_filter, max_results):
        """X.get_searchable_names_used_by_asset(asset_path, struct_type_filter, max_results) -> Array[BridgeSearchableNameRef]"""
        return unreal.UnrealBridgeAssetLibrary.get_searchable_names_used_by_asset(asset_path, struct_type_filter, max_results)

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
        """X.list_assets_under_path(folder_path, include_subfolders) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.list_assets_under_path(folder_path, include_subfolders)

    @staticmethod
    def list_assets_under_path_simple(*, content_folder_path):
        """X.list_assets_under_path_simple(content_folder_path) -> Array[SoftObjectPath]  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.list_assets_under_path_simple(content_folder_path)

    @staticmethod
    def list_searchable_name_values(*, struct_type, filter_prefix, max_results):
        """X.list_searchable_name_values(struct_type, filter_prefix, max_results) -> Array[str]"""
        return unreal.UnrealBridgeAssetLibrary.list_searchable_name_values(struct_type, filter_prefix, max_results)

    @staticmethod
    def resolve_redirector(*, asset_path):
        """X.resolve_redirector(asset_path) -> str"""
        return unreal.UnrealBridgeAssetLibrary.resolve_redirector(asset_path)

    @staticmethod
    def search_assets(*, query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path):
        """X.search_assets(query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path) -> (out_soft_paths=Array[SoftObjectPath], out_include_tokens_for_highlight=Array[str])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.search_assets(query, scope, class_filter, case_sensitive, whole_word, max_results, min_characters, custom_package_path)

    @staticmethod
    def search_assets_in_all_content(*, query, max_results):
        """X.search_assets_in_all_content(query, max_results) -> (out_soft_paths=Array[SoftObjectPath], out_include_tokens_for_highlight=Array[str])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.search_assets_in_all_content(query, max_results)

    @staticmethod
    def search_assets_under_path(*, content_folder_path, query, max_results):
        """X.search_assets_under_path(content_folder_path, query, max_results) -> (out_soft_paths=Array[SoftObjectPath], out_include_tokens_for_highlight=Array[str])  Note: SoftObjectPath does NOT stringify usefully — call .export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). See bridge-asset-api.md."""
        return unreal.UnrealBridgeAssetLibrary.search_assets_under_path(content_folder_path, query, max_results)

    @staticmethod
    def set_mesh_material(*, mesh_asset_path, material_index, material_asset_path, save=True):
        """X.set_mesh_material(mesh_asset_path, material_index, material_asset_path, save=True) -> BridgeMeshMaterialEditResult"""
        return unreal.UnrealBridgeAssetLibrary.set_mesh_material(mesh_asset_path, material_index, material_asset_path, save)

    @staticmethod
    def set_mesh_material_by_slot_name(*, mesh_asset_path, slot_name, material_asset_path, save=True):
        """X.set_mesh_material_by_slot_name(mesh_asset_path, slot_name, material_asset_path, save=True) -> BridgeMeshMaterialEditResult"""
        return unreal.UnrealBridgeAssetLibrary.set_mesh_material_by_slot_name(mesh_asset_path, slot_name, material_asset_path, save)

    @staticmethod
    def set_mesh_materials(*, mesh_asset_path, assignments, save=True):
        """X.set_mesh_materials(mesh_asset_path, assignments, save=True) -> BridgeMeshMaterialEditResult"""
        return unreal.UnrealBridgeAssetLibrary.set_mesh_materials(mesh_asset_path, assignments, save)


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
    def add_enhanced_input_action_event_node(*, blueprint_path, graph_name, input_action_path, node_pos_x, node_pos_y):
        """X.add_enhanced_input_action_event_node(blueprint_path, graph_name, input_action_path, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_enhanced_input_action_event_node(blueprint_path, graph_name, input_action_path, node_pos_x, node_pos_y)

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
    def add_external_variable_node(*, blueprint_path, graph_name, owner_class_path, variable_name, is_set, node_pos_x, node_pos_y):
        """X.add_external_variable_node(blueprint_path, graph_name, owner_class_path, variable_name, is_set, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_external_variable_node(blueprint_path, graph_name, owner_class_path, variable_name, is_set, node_pos_x, node_pos_y)

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
    def add_get_input_action_value_node(*, blueprint_path, graph_name, input_action_path, node_pos_x, node_pos_y):
        """X.add_get_input_action_value_node(blueprint_path, graph_name, input_action_path, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_get_input_action_value_node(blueprint_path, graph_name, input_action_path, node_pos_x, node_pos_y)

    @staticmethod
    def add_input_axis_key_event_node(*, blueprint_path, graph_name, axis_key_name, node_pos_x, node_pos_y):
        """X.add_input_axis_key_event_node(blueprint_path, graph_name, axis_key_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_input_axis_key_event_node(blueprint_path, graph_name, axis_key_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_input_key_event_node(*, blueprint_path, graph_name, key_name, node_pos_x, node_pos_y):
        """X.add_input_key_event_node(blueprint_path, graph_name, key_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_input_key_event_node(blueprint_path, graph_name, key_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_interface_message_node(*, blueprint_path, graph_name, interface_path, function_name, node_pos_x, node_pos_y):
        """X.add_interface_message_node(blueprint_path, graph_name, interface_path, function_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_interface_message_node(blueprint_path, graph_name, interface_path, function_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_legacy_input_action_event_node(*, blueprint_path, graph_name, action_name, node_pos_x, node_pos_y):
        """X.add_legacy_input_action_event_node(blueprint_path, graph_name, action_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_legacy_input_action_event_node(blueprint_path, graph_name, action_name, node_pos_x, node_pos_y)

    @staticmethod
    def add_legacy_input_axis_event_node(*, blueprint_path, graph_name, axis_name, node_pos_x, node_pos_y):
        """X.add_legacy_input_axis_event_node(blueprint_path, graph_name, axis_name, node_pos_x, node_pos_y) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.add_legacy_input_axis_event_node(blueprint_path, graph_name, axis_name, node_pos_x, node_pos_y)

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
    def add_pawn_input_begin_play_setup(*, blueprint_path, imc_path, priority, origin_x, origin_y):
        """X.add_pawn_input_begin_play_setup(blueprint_path, imc_path, priority, origin_x, origin_y) -> bool"""
        return unreal.UnrealBridgeBlueprintLibrary.add_pawn_input_begin_play_setup(blueprint_path, imc_path, priority, origin_x, origin_y)

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
    def wire_enhanced_input_action_to_function(*, blueprint_path, graph_name, input_action_path, trigger_event_pin, target_class_path, target_function_name, event_node_x, event_node_y, call_node_x, call_node_y, auto_wire_action_value=True):
        """X.wire_enhanced_input_action_to_function(blueprint_path, graph_name, input_action_path, trigger_event_pin, target_class_path, target_function_name, event_node_x, event_node_y, call_node_x, call_node_y, auto_wire_action_value=True) -> BridgeWireIAResult"""
        return unreal.UnrealBridgeBlueprintLibrary.wire_enhanced_input_action_to_function(blueprint_path, graph_name, input_action_path, trigger_event_pin, target_class_path, target_function_name, event_node_x, event_node_y, call_node_x, call_node_y, auto_wire_action_value)

    @staticmethod
    def wrap_nodes_in_comment_box(*, blueprint_path, graph_name, node_guids, text):
        """X.wrap_nodes_in_comment_box(blueprint_path, graph_name, node_guids, text) -> str"""
        return unreal.UnrealBridgeBlueprintLibrary.wrap_nodes_in_comment_box(blueprint_path, graph_name, node_guids, text)


class Chooser:
    """Wraps unreal.UnrealBridgeChooserLibrary (kwargs-only)."""

    @staticmethod
    def add_chooser_column_bool(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_bool(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_bool(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_by_struct_path(*, chooser_table_path, column_struct_path, binding_property_chain, context_index):
        """X.add_chooser_column_by_struct_path(chooser_table_path, column_struct_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_by_struct_path(chooser_table_path, column_struct_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_enum(*, chooser_table_path, binding_property_chain, enum_path, context_index):
        """X.add_chooser_column_enum(chooser_table_path, binding_property_chain, enum_path, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_enum(chooser_table_path, binding_property_chain, enum_path, context_index)

    @staticmethod
    def add_chooser_column_float_range(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_float_range(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_float_range(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_gameplay_tag(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_gameplay_tag(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_gameplay_tag(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_object(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_object(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_object(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_output_float(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_output_float(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_output_float(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_output_object(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_output_object(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
        return unreal.UnrealBridgeChooserLibrary.add_chooser_column_output_object(chooser_table_path, binding_property_chain, context_index)

    @staticmethod
    def add_chooser_column_randomize(*, chooser_table_path, binding_property_chain, context_index):
        """X.add_chooser_column_randomize(chooser_table_path, binding_property_chain, context_index) -> int32  If this is a freshly-created chooser (empty ContextData), call set_chooser_context_object_class FIRST — otherwise the editor binding widget shows 'NoPropertyBound' on every column. See bridge-chooser-api.md step 0."""
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
    def compile_chooser(*, chooser_table_path):
        """X.compile_chooser(chooser_table_path) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.compile_chooser(chooser_table_path)

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
    def get_last_chooser_error():
        """X.get_last_chooser_error() -> str"""
        return unreal.UnrealBridgeChooserLibrary.get_last_chooser_error()

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
    def move_chooser_column(*, chooser_table_path, source_index, target_index):
        """X.move_chooser_column(chooser_table_path, source_index, target_index) -> int32"""
        return unreal.UnrealBridgeChooserLibrary.move_chooser_column(chooser_table_path, source_index, target_index)

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
        """X.set_chooser_cell_raw(chooser_table_path, column_index, row_index, t3d_value) -> bool  Trap: BoolColumn cells use bare enum text ('MatchTrue'/'MatchFalse'/'MatchAny'), NOT a struct like '(Value=True)'. EnumColumn cells need explicit '(Comparison=MatchAny)' for wildcards — default '()' compares against int 0. See bridge-chooser-api.md cell-format table."""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_cell_raw(chooser_table_path, column_index, row_index, t3d_value)

    @staticmethod
    def set_chooser_column_disabled(*, chooser_table_path, column_index, disabled):
        """X.set_chooser_column_disabled(chooser_table_path, column_index, disabled) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_column_disabled(chooser_table_path, column_index, disabled)

    @staticmethod
    def set_chooser_context_object_class(*, chooser_table_path, context_class_path, direction):
        """X.set_chooser_context_object_class(chooser_table_path, context_class_path, direction) -> bool"""
        return unreal.UnrealBridgeChooserLibrary.set_chooser_context_object_class(chooser_table_path, context_class_path, direction)

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
    def create_data_table_from_csv(*, asset_path, row_struct_path, csv_content):
        """X.create_data_table_from_csv(asset_path, row_struct_path, csv_content) -> BridgeDataTableImportResult"""
        return unreal.UnrealBridgeDataTableLibrary.create_data_table_from_csv(asset_path, row_struct_path, csv_content)

    @staticmethod
    def create_data_table_from_json(*, asset_path, row_struct_path, json_content):
        """X.create_data_table_from_json(asset_path, row_struct_path, json_content) -> BridgeDataTableImportResult"""
        return unreal.UnrealBridgeDataTableLibrary.create_data_table_from_json(asset_path, row_struct_path, json_content)

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
    def import_data_table_from_csv_text(*, data_table_path, csv_content):
        """X.import_data_table_from_csv_text(data_table_path, csv_content) -> BridgeDataTableImportResult"""
        return unreal.UnrealBridgeDataTableLibrary.import_data_table_from_csv_text(data_table_path, csv_content)

    @staticmethod
    def import_data_table_from_json(*, data_table_path, json_file_path):
        """X.import_data_table_from_json(data_table_path, json_file_path) -> bool"""
        return unreal.UnrealBridgeDataTableLibrary.import_data_table_from_json(data_table_path, json_file_path)

    @staticmethod
    def import_data_table_from_json_text(*, data_table_path, json_content):
        """X.import_data_table_from_json_text(data_table_path, json_content) -> BridgeDataTableImportResult"""
        return unreal.UnrealBridgeDataTableLibrary.import_data_table_from_json_text(data_table_path, json_content)

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
    def capture_active_viewport_as_displayed(*, out_file_path, include_base64):
        """X.capture_active_viewport_as_displayed(out_file_path, include_base64) -> BridgeScreenshotResult"""
        return unreal.UnrealBridgeEditorLibrary.capture_active_viewport_as_displayed(out_file_path, include_base64)

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
    def add_ia_mapping_to_imc(*, mapping_context_path, input_action_path, key_name):
        """X.add_ia_mapping_to_imc(mapping_context_path, input_action_path, key_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_ia_mapping_to_imc(mapping_context_path, input_action_path, key_name)

    @staticmethod
    def add_impulse_to_pie_actor(*, actor_name, impulse, velocity_change=False):
        """X.add_impulse_to_pie_actor(actor_name, impulse, velocity_change=False) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_impulse_to_pie_actor(actor_name, impulse, velocity_change)

    @staticmethod
    def add_legacy_action_mapping(*, mapping_name, key_name, shift=False, ctrl=False, alt=False, cmd=False):
        """X.add_legacy_action_mapping(mapping_name, key_name, shift=False, ctrl=False, alt=False, cmd=False) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_legacy_action_mapping(mapping_name, key_name, shift, ctrl, alt, cmd)

    @staticmethod
    def add_legacy_axis_mapping(*, mapping_name, key_name, scale=1.000000):
        """X.add_legacy_axis_mapping(mapping_name, key_name, scale=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_legacy_axis_mapping(mapping_name, key_name, scale)

    @staticmethod
    def add_mapping_context(*, mapping_context_path, priority=0):
        """X.add_mapping_context(mapping_context_path, priority=0) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_mapping_context(mapping_context_path, priority)

    @staticmethod
    def add_modifier_to_ia(*, input_action_path, modifier_class, params_json="{}", save=True):
        """X.add_modifier_to_ia(input_action_path, modifier_class, params_json="{}", save=True) -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.add_modifier_to_ia(input_action_path, modifier_class, params_json, save)

    @staticmethod
    def add_modifier_to_imc_mapping(*, mapping_context_path, input_action_path, key_name, modifier_class, params_json="{}", save=True):
        """X.add_modifier_to_imc_mapping(mapping_context_path, input_action_path, key_name, modifier_class, params_json="{}", save=True) -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.add_modifier_to_imc_mapping(mapping_context_path, input_action_path, key_name, modifier_class, params_json, save)

    @staticmethod
    def add_on_screen_debug_message(*, message, duration_seconds=4.000000, r=1.000000, g=1.000000, b=1.000000):
        """X.add_on_screen_debug_message(message, duration_seconds=4.000000, r=1.000000, g=1.000000, b=1.000000) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.add_on_screen_debug_message(message, duration_seconds, r, g, b)

    @staticmethod
    def add_trigger_to_ia(*, input_action_path, trigger_class, params_json="{}", save=True):
        """X.add_trigger_to_ia(input_action_path, trigger_class, params_json="{}", save=True) -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.add_trigger_to_ia(input_action_path, trigger_class, params_json, save)

    @staticmethod
    def add_trigger_to_imc_mapping(*, mapping_context_path, input_action_path, key_name, trigger_class, params_json="{}", save=True):
        """X.add_trigger_to_imc_mapping(mapping_context_path, input_action_path, key_name, trigger_class, params_json="{}", save=True) -> int32"""
        return unreal.UnrealBridgeGameplayLibrary.add_trigger_to_imc_mapping(mapping_context_path, input_action_path, key_name, trigger_class, params_json, save)

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
    def create_input_action(*, package_path, value_type, description="", save=True):
        """X.create_input_action(package_path, value_type, description="", save=True) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.create_input_action(package_path, value_type, description, save)

    @staticmethod
    def create_input_mapping_context(*, package_path, description="", save=True):
        """X.create_input_mapping_context(package_path, description="", save=True) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.create_input_mapping_context(package_path, description, save)

    @staticmethod
    def deproject_screen_to_world(*, normalized_x, normalized_y):
        """X.deproject_screen_to_world(normalized_x, normalized_y) -> (out_origin=Vector, out_direction=Vector) or None"""
        return unreal.UnrealBridgeGameplayLibrary.deproject_screen_to_world(normalized_x, normalized_y)

    @staticmethod
    def destroy_actor_in_pie(*, actor_name):
        """X.destroy_actor_in_pie(actor_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.destroy_actor_in_pie(actor_name)

    @staticmethod
    def detect_key_conflicts(*, mapping_context_paths):
        """X.detect_key_conflicts(mapping_context_paths) -> Array[BridgeKeyConflict]"""
        return unreal.UnrealBridgeGameplayLibrary.detect_key_conflicts(mapping_context_paths)

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
    def dump_injected_input_queue():
        """X.dump_injected_input_queue() -> (int32, out_paths=Array[str], out_values=Array[Vector], out_hold_remaining_seconds=Array[float])"""
        return unreal.UnrealBridgeGameplayLibrary.dump_injected_input_queue()

    @staticmethod
    def duplicate_input_action(*, source_path, dest_path, save=True):
        """X.duplicate_input_action(source_path, dest_path, save=True) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.duplicate_input_action(source_path, dest_path, save)

    @staticmethod
    def duplicate_input_mapping_context(*, source_path, dest_path, save=True):
        """X.duplicate_input_mapping_context(source_path, dest_path, save=True) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.duplicate_input_mapping_context(source_path, dest_path, save)

    @staticmethod
    def find_input_action_references(*, input_action_path, blueprint_package_path_filter=""):
        """X.find_input_action_references(input_action_path, blueprint_package_path_filter="") -> Array[BridgeInputReference]"""
        return unreal.UnrealBridgeGameplayLibrary.find_input_action_references(input_action_path, blueprint_package_path_filter)

    @staticmethod
    def find_input_mapping_context_references(*, mapping_context_path, blueprint_package_path_filter=""):
        """X.find_input_mapping_context_references(mapping_context_path, blueprint_package_path_filter="") -> Array[BridgeInputReference]"""
        return unreal.UnrealBridgeGameplayLibrary.find_input_mapping_context_references(mapping_context_path, blueprint_package_path_filter)

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
    def get_active_mapping_context_stack():
        """X.get_active_mapping_context_stack() -> Array[BridgeMappingContextEntry]"""
        return unreal.UnrealBridgeGameplayLibrary.get_active_mapping_context_stack()

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
    def get_current_input_action_state(*, input_action_path):
        """X.get_current_input_action_state(input_action_path) -> BridgeInputActionState"""
        return unreal.UnrealBridgeGameplayLibrary.get_current_input_action_state(input_action_path)

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
    def get_input_action_modifiers_full(*, input_action_path):
        """X.get_input_action_modifiers_full(input_action_path) -> Array[BridgeInputComponentInstance]"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_action_modifiers_full(input_action_path)

    @staticmethod
    def get_input_action_triggers(*, input_action_path):
        """X.get_input_action_triggers(input_action_path) -> (out_trigger_names=Array[str], out_threshold_seconds=Array[float]) or None"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_action_triggers(input_action_path)

    @staticmethod
    def get_input_action_triggers_full(*, input_action_path):
        """X.get_input_action_triggers_full(input_action_path) -> Array[BridgeInputComponentInstance]"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_action_triggers_full(input_action_path)

    @staticmethod
    def get_input_action_value_type(*, input_action_path):
        """X.get_input_action_value_type(input_action_path) -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_action_value_type(input_action_path)

    @staticmethod
    def get_input_mapping_context_mappings(*, mapping_context_path):
        """X.get_input_mapping_context_mappings(mapping_context_path) -> Array[BridgeIMCMapping]"""
        return unreal.UnrealBridgeGameplayLibrary.get_input_mapping_context_mappings(mapping_context_path)

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
    def get_player_anim_instance(*, component_name=""):
        """X.get_player_anim_instance(component_name="") -> AnimInstance"""
        return unreal.UnrealBridgeGameplayLibrary.get_player_anim_instance(component_name)

    @staticmethod
    def get_player_pawn_actor_name():
        """X.get_player_pawn_actor_name() -> str"""
        return unreal.UnrealBridgeGameplayLibrary.get_player_pawn_actor_name()

    @staticmethod
    def get_player_skeletal_mesh_component(*, component_name=""):
        """X.get_player_skeletal_mesh_component(component_name="") -> SkeletalMeshComponent"""
        return unreal.UnrealBridgeGameplayLibrary.get_player_skeletal_mesh_component(component_name)

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
    def list_input_actions(*, content_path_filter, max_results=0):
        """X.list_input_actions(content_path_filter, max_results=0) -> Array[str]"""
        return unreal.UnrealBridgeGameplayLibrary.list_input_actions(content_path_filter, max_results)

    @staticmethod
    def list_input_actions_by_value_type(*, content_path_filter, value_type_filter, max_results=0):
        """X.list_input_actions_by_value_type(content_path_filter, value_type_filter, max_results=0) -> Array[str]"""
        return unreal.UnrealBridgeGameplayLibrary.list_input_actions_by_value_type(content_path_filter, value_type_filter, max_results)

    @staticmethod
    def list_input_mapping_contexts(*, content_path_filter, max_results=0):
        """X.list_input_mapping_contexts(content_path_filter, max_results=0) -> Array[str]"""
        return unreal.UnrealBridgeGameplayLibrary.list_input_mapping_contexts(content_path_filter, max_results)

    @staticmethod
    def list_legacy_action_mappings():
        """X.list_legacy_action_mappings() -> Array[BridgeLegacyActionMapping]"""
        return unreal.UnrealBridgeGameplayLibrary.list_legacy_action_mappings()

    @staticmethod
    def list_legacy_axis_mappings():
        """X.list_legacy_axis_mappings() -> Array[BridgeLegacyAxisMapping]"""
        return unreal.UnrealBridgeGameplayLibrary.list_legacy_axis_mappings()

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
    def remove_ia_mapping_from_imc(*, mapping_context_path, input_action_path, key_name):
        """X.remove_ia_mapping_from_imc(mapping_context_path, input_action_path, key_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_ia_mapping_from_imc(mapping_context_path, input_action_path, key_name)

    @staticmethod
    def remove_legacy_action_mapping(*, mapping_name, key_name):
        """X.remove_legacy_action_mapping(mapping_name, key_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_legacy_action_mapping(mapping_name, key_name)

    @staticmethod
    def remove_legacy_axis_mapping(*, mapping_name, key_name):
        """X.remove_legacy_axis_mapping(mapping_name, key_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_legacy_axis_mapping(mapping_name, key_name)

    @staticmethod
    def remove_mapping_context(*, mapping_context_path):
        """X.remove_mapping_context(mapping_context_path) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_mapping_context(mapping_context_path)

    @staticmethod
    def remove_modifier_from_ia(*, input_action_path, index, save=True):
        """X.remove_modifier_from_ia(input_action_path, index, save=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_modifier_from_ia(input_action_path, index, save)

    @staticmethod
    def remove_modifier_from_imc_mapping(*, mapping_context_path, input_action_path, key_name, index, save=True):
        """X.remove_modifier_from_imc_mapping(mapping_context_path, input_action_path, key_name, index, save=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_modifier_from_imc_mapping(mapping_context_path, input_action_path, key_name, index, save)

    @staticmethod
    def remove_trigger_from_ia(*, input_action_path, index, save=True):
        """X.remove_trigger_from_ia(input_action_path, index, save=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_trigger_from_ia(input_action_path, index, save)

    @staticmethod
    def remove_trigger_from_imc_mapping(*, mapping_context_path, input_action_path, key_name, index, save=True):
        """X.remove_trigger_from_imc_mapping(mapping_context_path, input_action_path, key_name, index, save=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.remove_trigger_from_imc_mapping(mapping_context_path, input_action_path, key_name, index, save)

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
    def set_imc_mapping_player_mappable_key_settings(*, mapping_context_path, input_action_path, key_name, player_mappable_key_settings_path, save=True):
        """X.set_imc_mapping_player_mappable_key_settings(mapping_context_path, input_action_path, key_name, player_mappable_key_settings_path, save=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_imc_mapping_player_mappable_key_settings(mapping_context_path, input_action_path, key_name, player_mappable_key_settings_path, save)

    @staticmethod
    def set_input_action_property(*, input_action_path, property_name, json_value, save=True):
        """X.set_input_action_property(input_action_path, property_name, json_value, save=True) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.set_input_action_property(input_action_path, property_name, json_value, save)

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
    def simulate_key_event(*, key_name, pressed, user_index=0):
        """X.simulate_key_event(key_name, pressed, user_index=0) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.simulate_key_event(key_name, pressed, user_index)

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
    def validate_input_bindings(*, blueprint_package_path_filter=""):
        """X.validate_input_bindings(blueprint_package_path_filter="") -> Array[BridgeInputBindingIssue]"""
        return unreal.UnrealBridgeGameplayLibrary.validate_input_bindings(blueprint_package_path_filter)

    @staticmethod
    def wake_pie_actor_physics(*, actor_name):
        """X.wake_pie_actor_physics(actor_name) -> bool"""
        return unreal.UnrealBridgeGameplayLibrary.wake_pie_actor_physics(actor_name)


class GameplayTag:
    """Wraps unreal.UnrealBridgeGameplayTagLibrary (kwargs-only)."""

    @staticmethod
    def add_gameplay_tag(*, new_tag, source_ini="", comment="", is_restricted=False):
        """X.add_gameplay_tag(new_tag, source_ini="", comment="", is_restricted=False) -> bool"""
        return unreal.UnrealBridgeGameplayTagLibrary.add_gameplay_tag(new_tag, source_ini, comment, is_restricted)

    @staticmethod
    def find_assets_referencing_tag(*, tag_string, include_children, package_path_filter, max_results):
        """X.find_assets_referencing_tag(tag_string, include_children, package_path_filter, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayTagLibrary.find_assets_referencing_tag(tag_string, include_children, package_path_filter, max_results)

    @staticmethod
    def get_tag_source_info(*, tag_string):
        """X.get_tag_source_info(tag_string) -> BridgeTagSourceInfo"""
        return unreal.UnrealBridgeGameplayTagLibrary.get_tag_source_info(tag_string)

    @staticmethod
    def list_all_registered_tags(*, filter_prefix, max_results):
        """X.list_all_registered_tags(filter_prefix, max_results) -> Array[str]"""
        return unreal.UnrealBridgeGameplayTagLibrary.list_all_registered_tags(filter_prefix, max_results)

    @staticmethod
    def list_gameplay_tag_redirects(*, source_ini_filter="", old_tag_prefix_filter=""):
        """X.list_gameplay_tag_redirects(source_ini_filter="", old_tag_prefix_filter="") -> Array[BridgeTagRedirectEntry]"""
        return unreal.UnrealBridgeGameplayTagLibrary.list_gameplay_tag_redirects(source_ini_filter, old_tag_prefix_filter)

    @staticmethod
    def list_tag_source_inis(*, filter_type=""):
        """X.list_tag_source_inis(filter_type="") -> Array[BridgeTagSourceListing]"""
        return unreal.UnrealBridgeGameplayTagLibrary.list_tag_source_inis(filter_type)

    @staticmethod
    def remove_gameplay_tag(*, tag_string):
        """X.remove_gameplay_tag(tag_string) -> bool"""
        return unreal.UnrealBridgeGameplayTagLibrary.remove_gameplay_tag(tag_string)

    @staticmethod
    def remove_gameplay_tag_redirect(*, old_tag, new_tag):
        """X.remove_gameplay_tag_redirect(old_tag, new_tag) -> bool"""
        return unreal.UnrealBridgeGameplayTagLibrary.remove_gameplay_tag_redirect(old_tag, new_tag)

    @staticmethod
    def rename_gameplay_tag(*, old_tag, new_tag, rename_children=True):
        """X.rename_gameplay_tag(old_tag, new_tag, rename_children=True) -> bool"""
        return unreal.UnrealBridgeGameplayTagLibrary.rename_gameplay_tag(old_tag, new_tag, rename_children)


class Geometry:
    """Wraps unreal.UnrealBridgeGeometryLibrary (kwargs-only)."""

    @staticmethod
    def append_box(*, handle, origin, size):
        """X.append_box(handle, origin, size) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.append_box(handle, origin, size)

    @staticmethod
    def append_cone(*, handle, origin, base_radius, height, radial_segments):
        """X.append_cone(handle, origin, base_radius, height, radial_segments) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.append_cone(handle, origin, base_radius, height, radial_segments)

    @staticmethod
    def append_cylinder(*, handle, origin, radius, height, radial_segments):
        """X.append_cylinder(handle, origin, radius, height, radial_segments) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.append_cylinder(handle, origin, radius, height, radial_segments)

    @staticmethod
    def append_sphere(*, handle, origin, radius, resolution_uv):
        """X.append_sphere(handle, origin, radius, resolution_uv) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.append_sphere(handle, origin, radius, resolution_uv)

    @staticmethod
    def bake_normals_to_texture(*, handle, new_texture_path, resolution):
        """X.bake_normals_to_texture(handle, new_texture_path, resolution) -> str"""
        return unreal.UnrealBridgeGeometryLibrary.bake_normals_to_texture(handle, new_texture_path, resolution)

    @staticmethod
    def bake_occlusion_to_vertex_color(*, handle, occlusion_rays=16):
        """X.bake_occlusion_to_vertex_color(handle, occlusion_rays=16) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.bake_occlusion_to_vertex_color(handle, occlusion_rays)

    @staticmethod
    def create_dynamic_mesh():
        """X.create_dynamic_mesh() -> int32"""
        return unreal.UnrealBridgeGeometryLibrary.create_dynamic_mesh()

    @staticmethod
    def extrude_selection(*, handle, selection_id, distance):
        """X.extrude_selection(handle, selection_id, distance) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.extrude_selection(handle, selection_id, distance)

    @staticmethod
    def get_mesh_info(*, handle):
        """X.get_mesh_info(handle) -> BridgeMeshInfo"""
        return unreal.UnrealBridgeGeometryLibrary.get_mesh_info(handle)

    @staticmethod
    def list_dynamic_mesh_handles():
        """X.list_dynamic_mesh_handles() -> Array[int32]"""
        return unreal.UnrealBridgeGeometryLibrary.list_dynamic_mesh_handles()

    @staticmethod
    def list_selections():
        """X.list_selections() -> Array[int32]"""
        return unreal.UnrealBridgeGeometryLibrary.list_selections()

    @staticmethod
    def load_mesh_from_component(*, actor_label, component_name, handle):
        """X.load_mesh_from_component(actor_label, component_name, handle) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.load_mesh_from_component(actor_label, component_name, handle)

    @staticmethod
    def load_mesh_from_static_mesh(*, handle, asset_path, lod=0):
        """X.load_mesh_from_static_mesh(handle, asset_path, lod=0) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.load_mesh_from_static_mesh(handle, asset_path, lod)

    @staticmethod
    def mesh_boolean(*, handle_a, handle_b, op):
        """X.mesh_boolean(handle_a, handle_b, op) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_boolean(handle_a, handle_b, op)

    @staticmethod
    def mesh_decimate(*, handle, target_tris):
        """X.mesh_decimate(handle, target_tris) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_decimate(handle, target_tris)

    @staticmethod
    def mesh_displace_from_texture(*, handle, texture_path, magnitude, uv_channel=0):
        """X.mesh_displace_from_texture(handle, texture_path, magnitude, uv_channel=0) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_displace_from_texture(handle, texture_path, magnitude, uv_channel)

    @staticmethod
    def mesh_smooth(*, handle, iterations, strength):
        """X.mesh_smooth(handle, iterations, strength) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_smooth(handle, iterations, strength)

    @staticmethod
    def mesh_transform(*, handle, transform):
        """X.mesh_transform(handle, transform) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_transform(handle, transform)

    @staticmethod
    def mesh_uniform_remesh(*, handle, target_tri_count):
        """X.mesh_uniform_remesh(handle, target_tri_count) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_uniform_remesh(handle, target_tri_count)

    @staticmethod
    def mesh_uv_unwrap(*, handle, method):
        """X.mesh_uv_unwrap(handle, method) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_uv_unwrap(handle, method)

    @staticmethod
    def mesh_voxel_merge(*, handles, cell_size_cm):
        """X.mesh_voxel_merge(handles, cell_size_cm) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.mesh_voxel_merge(handles, cell_size_cm)

    @staticmethod
    def recompute_normals_and_tangents(*, handle, angle_threshold_deg):
        """X.recompute_normals_and_tangents(handle, angle_threshold_deg) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.recompute_normals_and_tangents(handle, angle_threshold_deg)

    @staticmethod
    def release_dynamic_mesh(*, handle):
        """X.release_dynamic_mesh(handle) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.release_dynamic_mesh(handle)

    @staticmethod
    def release_selection(*, selection_id):
        """X.release_selection(selection_id) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.release_selection(selection_id)

    @staticmethod
    def save_mesh_to_existing_static_mesh(*, handle, existing_asset_path, replace_materials):
        """X.save_mesh_to_existing_static_mesh(handle, existing_asset_path, replace_materials) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.save_mesh_to_existing_static_mesh(handle, existing_asset_path, replace_materials)

    @staticmethod
    def save_mesh_to_new_static_mesh(*, handle, new_asset_path, material_list):
        """X.save_mesh_to_new_static_mesh(handle, new_asset_path, material_list) -> str"""
        return unreal.UnrealBridgeGeometryLibrary.save_mesh_to_new_static_mesh(handle, new_asset_path, material_list)

    @staticmethod
    def select_by_normal_direction(*, handle, normal, max_angle_deg):
        """X.select_by_normal_direction(handle, normal, max_angle_deg) -> int32"""
        return unreal.UnrealBridgeGeometryLibrary.select_by_normal_direction(handle, normal, max_angle_deg)

    @staticmethod
    def sweep_along_spline(*, handle, profile_xy, actor_label, component_name, num_path_samples=32):
        """X.sweep_along_spline(handle, profile_xy, actor_label, component_name, num_path_samples=32) -> bool"""
        return unreal.UnrealBridgeGeometryLibrary.sweep_along_spline(handle, profile_xy, actor_label, component_name, num_path_samples)


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


class Niagara:
    """Wraps unreal.UnrealBridgeNiagaraLibrary (kwargs-only)."""

    @staticmethod
    def add_niagara_emitter(*, system_path, name, emitter_asset_or_template_path="", save=True):
        """X.add_niagara_emitter(system_path, name, emitter_asset_or_template_path="", save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.add_niagara_emitter(system_path, name, emitter_asset_or_template_path, save)

    @staticmethod
    def add_niagara_module(*, system_path, emitter_id_or_name, usage, script_path, suggested_name="", index=-1, enabled=True, save=True):
        """X.add_niagara_module(system_path, emitter_id_or_name, usage, script_path, suggested_name="", index=-1, enabled=True, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.add_niagara_module(system_path, emitter_id_or_name, usage, script_path, suggested_name, index, enabled, save)

    @staticmethod
    def add_niagara_parameter_assignment(*, system_path, emitter_id_or_name, usage, parameter_name, type, value, index=-1, save=True):
        """X.add_niagara_parameter_assignment(system_path, emitter_id_or_name, usage, parameter_name, type, value, index=-1, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.add_niagara_parameter_assignment(system_path, emitter_id_or_name, usage, parameter_name, type, value, index, save)

    @staticmethod
    def add_niagara_renderer(*, system_path, emitter_id_or_name, renderer_type, name="", material_path="", mesh_path="", save=True):
        """X.add_niagara_renderer(system_path, emitter_id_or_name, renderer_type, name="", material_path="", mesh_path="", save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.add_niagara_renderer(system_path, emitter_id_or_name, renderer_type, name, material_path, mesh_path, save)

    @staticmethod
    def add_niagara_user_parameter(*, system_path, name, type, default_value, save=True):
        """X.add_niagara_user_parameter(system_path, name, type, default_value, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.add_niagara_user_parameter(system_path, name, type, default_value, save)

    @staticmethod
    def advance_niagara_preview(*, handle, seconds, tick_delta=0.016667):
        """X.advance_niagara_preview(handle, seconds, tick_delta=0.016667) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.advance_niagara_preview(handle, seconds, tick_delta)

    @staticmethod
    def compile_niagara_system(*, system_path, force=True, wait_for_gpu_shaders=True, save=True):
        """X.compile_niagara_system(system_path, force=True, wait_for_gpu_shaders=True, save=True) -> BridgeNiagaraCompileResult"""
        return unreal.UnrealBridgeNiagaraLibrary.compile_niagara_system(system_path, force, wait_for_gpu_shaders, save)

    @staticmethod
    def control_niagara_preview(*, handle, action):
        """X.control_niagara_preview(handle, action) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.control_niagara_preview(handle, action)

    @staticmethod
    def create_dissolve_effect(*, asset_path, style="Ash", material_path="", color=[0.080000, 0.800000, 1.000000, 1.000000], count=128, duration=2.000000, radius=100.000000, direction=[0.000000, 0.000000, 1.000000], save=True):
        """X.create_dissolve_effect(asset_path, style="Ash", material_path="", color=[0.080000, 0.800000, 1.000000, 1.000000], count=128, duration=2.000000, radius=100.000000, direction=[0.000000, 0.000000, 1.000000], save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_dissolve_effect(asset_path, style, material_path, color, count, duration, radius, direction, save)

    @staticmethod
    def create_explosion_effect(*, asset_path, style="Layered", material_path="", core_color=[1.000000, 0.120000, 0.010000, 1.000000], scale=1.000000, duration=1.500000, debris_count=64, shockwave=True, light=True, save=True):
        """X.create_explosion_effect(asset_path, style="Layered", material_path="", core_color=[1.000000, 0.120000, 0.010000, 1.000000], scale=1.000000, duration=1.500000, debris_count=64, shockwave=True, light=True, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_explosion_effect(asset_path, style, material_path, core_color, scale, duration, debris_count, shockwave, light, save)

    @staticmethod
    def create_niagara_emitter(*, asset_path, template_emitter_path="", add_default_modules_and_renderer=True, save=True):
        """X.create_niagara_emitter(asset_path, template_emitter_path="", add_default_modules_and_renderer=True, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_niagara_emitter(asset_path, template_emitter_path, add_default_modules_and_renderer, save)

    @staticmethod
    def create_niagara_system(*, asset_path, template_system_path="", save=True):
        """X.create_niagara_system(asset_path, template_system_path="", save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_niagara_system(asset_path, template_system_path, save)

    @staticmethod
    def create_niagara_system_from_recipe(*, asset_path, recipe, compile=True, save=True):
        """X.create_niagara_system_from_recipe(asset_path, recipe, compile=True, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_niagara_system_from_recipe(asset_path, recipe, compile, save)

    @staticmethod
    def create_spark_effect(*, asset_path, style="Directional", material_path="", color=[1.000000, 0.450000, 0.050000, 1.000000], count=48, speed=900.000000, lifetime=0.600000, gravity=-980.000000, collision=True, save=True):
        """X.create_spark_effect(asset_path, style="Directional", material_path="", color=[1.000000, 0.450000, 0.050000, 1.000000], count=48, speed=900.000000, lifetime=0.600000, gravity=-980.000000, collision=True, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_spark_effect(asset_path, style, material_path, color, count, speed, lifetime, gravity, collision, save)

    @staticmethod
    def create_weapon_trail_effect(*, asset_path, style="Ribbon", material_path="", color=[1.000000, 0.350000, 0.050000, 1.000000], width=12.000000, lifetime=0.350000, spawn_rate=90.000000, local_space=False, save=True):
        """X.create_weapon_trail_effect(asset_path, style="Ribbon", material_path="", color=[1.000000, 0.350000, 0.050000, 1.000000], width=12.000000, lifetime=0.350000, spawn_rate=90.000000, local_space=False, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.create_weapon_trail_effect(asset_path, style, material_path, color, width, lifetime, spawn_rate, local_space, save)

    @staticmethod
    def delete_niagara_asset(*, asset_path):
        """X.delete_niagara_asset(asset_path) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.delete_niagara_asset(asset_path)

    @staticmethod
    def duplicate_niagara_emitter(*, system_path, emitter_id_or_name, new_name, save=True):
        """X.duplicate_niagara_emitter(system_path, emitter_id_or_name, new_name, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.duplicate_niagara_emitter(system_path, emitter_id_or_name, new_name, save)

    @staticmethod
    def get_last_niagara_error():
        """X.get_last_niagara_error() -> str"""
        return unreal.UnrealBridgeNiagaraLibrary.get_last_niagara_error()

    @staticmethod
    def get_niagara_compile_diagnostics(*, system_path):
        """X.get_niagara_compile_diagnostics(system_path) -> BridgeNiagaraCompileResult"""
        return unreal.UnrealBridgeNiagaraLibrary.get_niagara_compile_diagnostics(system_path)

    @staticmethod
    def get_niagara_preview_info(*, handle):
        """X.get_niagara_preview_info(handle) -> BridgeNiagaraPreviewInfo"""
        return unreal.UnrealBridgeNiagaraLibrary.get_niagara_preview_info(handle)

    @staticmethod
    def get_niagara_renderer_property(*, system_path, emitter_id_or_name, renderer_id, property_name):
        """X.get_niagara_renderer_property(system_path, emitter_id_or_name, renderer_id, property_name) -> str"""
        return unreal.UnrealBridgeNiagaraLibrary.get_niagara_renderer_property(system_path, emitter_id_or_name, renderer_id, property_name)

    @staticmethod
    def get_niagara_script_info(*, script_path):
        """X.get_niagara_script_info(script_path) -> BridgeNiagaraScriptInfo"""
        return unreal.UnrealBridgeNiagaraLibrary.get_niagara_script_info(script_path)

    @staticmethod
    def get_niagara_system_info(*, system_path):
        """X.get_niagara_system_info(system_path) -> BridgeNiagaraSystemInfo"""
        return unreal.UnrealBridgeNiagaraLibrary.get_niagara_system_info(system_path)

    @staticmethod
    def is_niagara_api_available():
        """X.is_niagara_api_available() -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.is_niagara_api_available()

    @staticmethod
    def link_niagara_module_input(*, system_path, module_id, input_name, linked_parameter, save=True):
        """X.link_niagara_module_input(system_path, module_id, input_name, linked_parameter, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.link_niagara_module_input(system_path, module_id, input_name, linked_parameter, save)

    @staticmethod
    def list_niagara_emitters(*, system_path):
        """X.list_niagara_emitters(system_path) -> Array[BridgeNiagaraEmitterInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_emitters(system_path)

    @staticmethod
    def list_niagara_module_input_object_properties(*, system_path, module_id, input_name, include_advanced=False):
        """X.list_niagara_module_input_object_properties(system_path, module_id, input_name, include_advanced=False) -> Array[BridgeNiagaraPropertyInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_module_input_object_properties(system_path, module_id, input_name, include_advanced)

    @staticmethod
    def list_niagara_module_inputs(*, system_path, module_id, include_hidden=False):
        """X.list_niagara_module_inputs(system_path, module_id, include_hidden=False) -> Array[BridgeNiagaraModuleInputInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_module_inputs(system_path, module_id, include_hidden)

    @staticmethod
    def list_niagara_modules(*, system_path, emitter_id_or_name="", usage="All"):
        """X.list_niagara_modules(system_path, emitter_id_or_name="", usage="All") -> Array[BridgeNiagaraModuleInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_modules(system_path, emitter_id_or_name, usage)

    @staticmethod
    def list_niagara_previews():
        """X.list_niagara_previews() -> Array[BridgeNiagaraPreviewInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_previews()

    @staticmethod
    def list_niagara_renderer_properties(*, system_path, emitter_id_or_name, renderer_id, include_advanced=False):
        """X.list_niagara_renderer_properties(system_path, emitter_id_or_name, renderer_id, include_advanced=False) -> Array[BridgeNiagaraPropertyInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_renderer_properties(system_path, emitter_id_or_name, renderer_id, include_advanced)

    @staticmethod
    def list_niagara_renderers(*, system_path, emitter_id_or_name=""):
        """X.list_niagara_renderers(system_path, emitter_id_or_name="") -> Array[BridgeNiagaraRendererInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_renderers(system_path, emitter_id_or_name)

    @staticmethod
    def list_niagara_scripts(*, usage="Module", query="", max_results=500):
        """X.list_niagara_scripts(usage="Module", query="", max_results=500) -> Array[BridgeNiagaraScriptInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_scripts(usage, query, max_results)

    @staticmethod
    def list_niagara_templates(*, asset_type="All", query="", max_results=200):
        """X.list_niagara_templates(asset_type="All", query="", max_results=200) -> Array[BridgeNiagaraTemplateInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_templates(asset_type, query, max_results)

    @staticmethod
    def list_niagara_user_parameters(*, system_path):
        """X.list_niagara_user_parameters(system_path) -> Array[BridgeNiagaraParameterInfo]"""
        return unreal.UnrealBridgeNiagaraLibrary.list_niagara_user_parameters(system_path)

    @staticmethod
    def remove_all_niagara_previews():
        """X.remove_all_niagara_previews() -> int32"""
        return unreal.UnrealBridgeNiagaraLibrary.remove_all_niagara_previews()

    @staticmethod
    def remove_niagara_emitter(*, system_path, emitter_id_or_name, save=True):
        """X.remove_niagara_emitter(system_path, emitter_id_or_name, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.remove_niagara_emitter(system_path, emitter_id_or_name, save)

    @staticmethod
    def remove_niagara_module(*, system_path, module_id, save=True):
        """X.remove_niagara_module(system_path, module_id, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.remove_niagara_module(system_path, module_id, save)

    @staticmethod
    def remove_niagara_preview(*, handle):
        """X.remove_niagara_preview(handle) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.remove_niagara_preview(handle)

    @staticmethod
    def remove_niagara_renderer(*, system_path, emitter_id_or_name, renderer_id, save=True):
        """X.remove_niagara_renderer(system_path, emitter_id_or_name, renderer_id, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.remove_niagara_renderer(system_path, emitter_id_or_name, renderer_id, save)

    @staticmethod
    def remove_niagara_user_parameter(*, system_path, name, save=True):
        """X.remove_niagara_user_parameter(system_path, name, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.remove_niagara_user_parameter(system_path, name, save)

    @staticmethod
    def rename_niagara_emitter(*, system_path, emitter_id_or_name, new_name, save=True):
        """X.rename_niagara_emitter(system_path, emitter_id_or_name, new_name, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.rename_niagara_emitter(system_path, emitter_id_or_name, new_name, save)

    @staticmethod
    def rename_niagara_user_parameter(*, system_path, old_name, new_name, save=True):
        """X.rename_niagara_user_parameter(system_path, old_name, new_name, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.rename_niagara_user_parameter(system_path, old_name, new_name, save)

    @staticmethod
    def reset_niagara_module_input(*, system_path, module_id, input_name, save=True):
        """X.reset_niagara_module_input(system_path, module_id, input_name, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.reset_niagara_module_input(system_path, module_id, input_name, save)

    @staticmethod
    def set_niagara_emitter_enabled(*, system_path, emitter_id_or_name, enabled, save=True):
        """X.set_niagara_emitter_enabled(system_path, emitter_id_or_name, enabled, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_emitter_enabled(system_path, emitter_id_or_name, enabled, save)

    @staticmethod
    def set_niagara_emitter_properties(*, system_path, emitter_id_or_name, local_space, sim_target, deterministic, random_seed, use_fixed_bounds, fixed_bounds, save=True):
        """X.set_niagara_emitter_properties(system_path, emitter_id_or_name, local_space, sim_target, deterministic, random_seed, use_fixed_bounds, fixed_bounds, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_emitter_properties(system_path, emitter_id_or_name, local_space, sim_target, deterministic, random_seed, use_fixed_bounds, fixed_bounds, save)

    @staticmethod
    def set_niagara_module_data_interface_input(*, system_path, module_id, input_name, data_interface_class_path, properties, save=True):
        """X.set_niagara_module_data_interface_input(system_path, module_id, input_name, data_interface_class_path, properties, save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_module_data_interface_input(system_path, module_id, input_name, data_interface_class_path, properties, save)

    @staticmethod
    def set_niagara_module_dynamic_input(*, system_path, module_id, input_name, dynamic_input_script_path, suggested_name="", save=True):
        """X.set_niagara_module_dynamic_input(system_path, module_id, input_name, dynamic_input_script_path, suggested_name="", save=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_module_dynamic_input(system_path, module_id, input_name, dynamic_input_script_path, suggested_name, save)

    @staticmethod
    def set_niagara_module_enabled(*, system_path, module_id, enabled, save=True):
        """X.set_niagara_module_enabled(system_path, module_id, enabled, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_module_enabled(system_path, module_id, enabled, save)

    @staticmethod
    def set_niagara_module_input(*, system_path, module_id, input_name, value, save=True):
        """X.set_niagara_module_input(system_path, module_id, input_name, value, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_module_input(system_path, module_id, input_name, value, save)

    @staticmethod
    def set_niagara_module_input_object_property(*, system_path, module_id, input_name, property_name, value, save=True):
        """X.set_niagara_module_input_object_property(system_path, module_id, input_name, property_name, value, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_module_input_object_property(system_path, module_id, input_name, property_name, value, save)

    @staticmethod
    def set_niagara_module_object_input(*, system_path, module_id, input_name, object_path, save=True):
        """X.set_niagara_module_object_input(system_path, module_id, input_name, object_path, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_module_object_input(system_path, module_id, input_name, object_path, save)

    @staticmethod
    def set_niagara_preview_transform(*, handle, transform, teleport=False):
        """X.set_niagara_preview_transform(handle, transform, teleport=False) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_preview_transform(handle, transform, teleport)

    @staticmethod
    def set_niagara_preview_variable(*, handle, name, type, value):
        """X.set_niagara_preview_variable(handle, name, type, value) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_preview_variable(handle, name, type, value)

    @staticmethod
    def set_niagara_renderer_binding(*, system_path, emitter_id_or_name, renderer_id, binding_property, variable_name, source_mode="Particles", save=True):
        """X.set_niagara_renderer_binding(system_path, emitter_id_or_name, renderer_id, binding_property, variable_name, source_mode="Particles", save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_renderer_binding(system_path, emitter_id_or_name, renderer_id, binding_property, variable_name, source_mode, save)

    @staticmethod
    def set_niagara_renderer_enabled(*, system_path, emitter_id_or_name, renderer_id, enabled, save=True):
        """X.set_niagara_renderer_enabled(system_path, emitter_id_or_name, renderer_id, enabled, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_renderer_enabled(system_path, emitter_id_or_name, renderer_id, enabled, save)

    @staticmethod
    def set_niagara_renderer_material(*, system_path, emitter_id_or_name, renderer_id, material_path, material_index=0, save=True):
        """X.set_niagara_renderer_material(system_path, emitter_id_or_name, renderer_id, material_path, material_index=0, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_renderer_material(system_path, emitter_id_or_name, renderer_id, material_path, material_index, save)

    @staticmethod
    def set_niagara_renderer_property(*, system_path, emitter_id_or_name, renderer_id, property_name, value, save=True):
        """X.set_niagara_renderer_property(system_path, emitter_id_or_name, renderer_id, property_name, value, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_renderer_property(system_path, emitter_id_or_name, renderer_id, property_name, value, save)

    @staticmethod
    def set_niagara_system_effect_type(*, system_path, effect_type_path, save=True):
        """X.set_niagara_system_effect_type(system_path, effect_type_path, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_system_effect_type(system_path, effect_type_path, save)

    @staticmethod
    def set_niagara_system_fixed_bounds(*, system_path, enabled, bounds, save=True):
        """X.set_niagara_system_fixed_bounds(system_path, enabled, bounds, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_system_fixed_bounds(system_path, enabled, bounds, save)

    @staticmethod
    def set_niagara_system_warmup(*, system_path, warmup_time, tick_delta=0.033333, save=True):
        """X.set_niagara_system_warmup(system_path, warmup_time, tick_delta=0.033333, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_system_warmup(system_path, warmup_time, tick_delta, save)

    @staticmethod
    def set_niagara_user_parameter_default(*, system_path, name, value, save=True):
        """X.set_niagara_user_parameter_default(system_path, name, value, save=True) -> bool"""
        return unreal.UnrealBridgeNiagaraLibrary.set_niagara_user_parameter_default(system_path, name, value, save)

    @staticmethod
    def spawn_niagara_preview(*, system_path, transform, auto_activate=True, reset_on_change=True):
        """X.spawn_niagara_preview(system_path, transform, auto_activate=True, reset_on_change=True) -> BridgeNiagaraOperationResult"""
        return unreal.UnrealBridgeNiagaraLibrary.spawn_niagara_preview(system_path, transform, auto_activate, reset_on_change)

    @staticmethod
    def validate_niagara_system(*, system_path, check_materials=True, check_bounds=True, max_emitters=16, max_renderers_per_emitter=8, max_modules_per_emitter=64):
        """X.validate_niagara_system(system_path, check_materials=True, check_bounds=True, max_emitters=16, max_renderers_per_emitter=8, max_modules_per_emitter=64) -> BridgeNiagaraAuditResult"""
        return unreal.UnrealBridgeNiagaraLibrary.validate_niagara_system(system_path, check_materials, check_bounds, max_emitters, max_renderers_per_emitter, max_modules_per_emitter)


class PCG:
    """Wraps unreal.UnrealBridgePCGLibrary (kwargs-only)."""

    @staticmethod
    def cleanup_pcg_component(*, actor_label, component_name, remove_components=False):
        """X.cleanup_pcg_component(actor_label, component_name, remove_components=False) -> bool"""
        return unreal.UnrealBridgePCGLibrary.cleanup_pcg_component(actor_label, component_name, remove_components)

    @staticmethod
    def get_pcg_component_overrides(*, actor_label, component_name):
        """X.get_pcg_component_overrides(actor_label, component_name) -> Array[BridgePCGOverrideEntry]"""
        return unreal.UnrealBridgePCGLibrary.get_pcg_component_overrides(actor_label, component_name)

    @staticmethod
    def get_pcg_component_state(*, actor_label, component_name):
        """X.get_pcg_component_state(actor_label, component_name) -> BridgePCGComponentState"""
        return unreal.UnrealBridgePCGLibrary.get_pcg_component_state(actor_label, component_name)

    @staticmethod
    def list_pcg_components_in_level(*, level_filter, max=200):
        """X.list_pcg_components_in_level(level_filter, max=200) -> Array[BridgePCGComponentEntry]"""
        return unreal.UnrealBridgePCGLibrary.list_pcg_components_in_level(level_filter, max)

    @staticmethod
    def list_pcg_graph_assets(*, filter, max=200):
        """X.list_pcg_graph_assets(filter, max=200) -> Array[str]"""
        return unreal.UnrealBridgePCGLibrary.list_pcg_graph_assets(filter, max)

    @staticmethod
    def set_pcg_component_override(*, actor_label, component_name, name, exported_value):
        """X.set_pcg_component_override(actor_label, component_name, name, exported_value) -> bool"""
        return unreal.UnrealBridgePCGLibrary.set_pcg_component_override(actor_label, component_name, name, exported_value)

    @staticmethod
    def trigger_pcg_generate(*, actor_label, component_name, force=False):
        """X.trigger_pcg_generate(actor_label, component_name, force=False) -> bool"""
        return unreal.UnrealBridgePCGLibrary.trigger_pcg_generate(actor_label, component_name, force)

    @staticmethod
    def wait_for_pcg_generate(*, actor_label, component_name, timeout_sec=60.000000):
        """X.wait_for_pcg_generate(actor_label, component_name, timeout_sec=60.000000) -> BridgePCGWaitResult"""
        return unreal.UnrealBridgePCGLibrary.wait_for_pcg_generate(actor_label, component_name, timeout_sec)


class Perf:
    """Wraps unreal.UnrealBridgePerfLibrary (kwargs-only)."""

    @staticmethod
    def analyze_all_materials(*, top_n=30):
        """X.analyze_all_materials(top_n=30) -> BridgeAllMaterialsAnalysis"""
        return unreal.UnrealBridgePerfLibrary.analyze_all_materials(top_n)

    @staticmethod
    def begin_auto_hitch_capture(*, threshold_ms=50.000000, max_entries=100):
        """X.begin_auto_hitch_capture(threshold_ms=50.000000, max_entries=100) -> bool"""
        return unreal.UnrealBridgePerfLibrary.begin_auto_hitch_capture(threshold_ms, max_entries)

    @staticmethod
    def begin_insights_for_trace(*, utrace_path):
        """X.begin_insights_for_trace(utrace_path) -> BridgeInsightsLaunchResult"""
        return unreal.UnrealBridgePerfLibrary.begin_insights_for_trace(utrace_path)

    @staticmethod
    def clear_hitch_log():
        """X.clear_hitch_log() -> None"""
        return unreal.UnrealBridgePerfLibrary.clear_hitch_log()

    @staticmethod
    def compare_perf_snapshots(*, before, after, regression_threshold=0.100000):
        """X.compare_perf_snapshots(before, after, regression_threshold=0.100000) -> BridgePerfSnapshotDelta"""
        return unreal.UnrealBridgePerfLibrary.compare_perf_snapshots(before, after, regression_threshold)

    @staticmethod
    def end_auto_hitch_capture():
        """X.end_auto_hitch_capture() -> Array[BridgeAutoHitchEntry]"""
        return unreal.UnrealBridgePerfLibrary.end_auto_hitch_capture()

    @staticmethod
    def export_perf_samples_to_csv(*, output_path):
        """X.export_perf_samples_to_csv(output_path) -> bool"""
        return unreal.UnrealBridgePerfLibrary.export_perf_samples_to_csv(output_path)

    @staticmethod
    def get_actor_render_cost(*, actor_path):
        """X.get_actor_render_cost(actor_path) -> BridgeActorRenderCost"""
        return unreal.UnrealBridgePerfLibrary.get_actor_render_cost(actor_path)

    @staticmethod
    def get_asset_size_top_n(*, class_filter, top_n=50):
        """X.get_asset_size_top_n(class_filter, top_n=50) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_asset_size_top_n(class_filter, top_n)

    @staticmethod
    def get_audio_memory_breakdown(*, group_by="compression_format", mode="disk", max_groups=50):
        """X.get_audio_memory_breakdown(group_by="compression_format", mode="disk", max_groups=50) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_audio_memory_breakdown(group_by, mode, max_groups)

    @staticmethod
    def get_auto_hitch_state():
        """X.get_auto_hitch_state() -> BridgeAutoHitchState"""
        return unreal.UnrealBridgePerfLibrary.get_auto_hitch_state()

    @staticmethod
    def get_frame_time_histogram(*, bucket_ms=5.000000, max_bucket_ms=100.000000):
        """X.get_frame_time_histogram(bucket_ms=5.000000, max_bucket_ms=100.000000) -> Array[BridgeHistogramBucket]"""
        return unreal.UnrealBridgePerfLibrary.get_frame_time_histogram(bucket_ms, max_bucket_ms)

    @staticmethod
    def get_frame_time_percentiles(*, percentiles):
        """X.get_frame_time_percentiles(percentiles) -> Array[float]"""
        return unreal.UnrealBridgePerfLibrary.get_frame_time_percentiles(percentiles)

    @staticmethod
    def get_frame_timing():
        """X.get_frame_timing() -> BridgeFrameTiming"""
        return unreal.UnrealBridgePerfLibrary.get_frame_timing()

    @staticmethod
    def get_hitch_log(*, threshold_ms=50.000000, max_entries=50):
        """X.get_hitch_log(threshold_ms=50.000000, max_entries=50) -> Array[BridgeHitchEntry]"""
        return unreal.UnrealBridgePerfLibrary.get_hitch_log(threshold_ms, max_entries)

    @staticmethod
    def get_lod_distribution(*, class_filter, actor_filter):
        """X.get_lod_distribution(class_filter, actor_filter) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_lod_distribution(class_filter, actor_filter)

    @staticmethod
    def get_lumen_diagnostics():
        """X.get_lumen_diagnostics() -> BridgeLumenDiagnostics"""
        return unreal.UnrealBridgePerfLibrary.get_lumen_diagnostics()

    @staticmethod
    def get_memory_stats():
        """X.get_memory_stats() -> BridgeMemoryStats"""
        return unreal.UnrealBridgePerfLibrary.get_memory_stats()

    @staticmethod
    def get_mesh_memory_breakdown(*, group_by, mesh_type="all", mode="disk", max_groups=50):
        """X.get_mesh_memory_breakdown(group_by, mesh_type="all", mode="disk", max_groups=50) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_mesh_memory_breakdown(group_by, mesh_type, mode, max_groups)

    @staticmethod
    def get_nanite_stats():
        """X.get_nanite_stats() -> BridgeNaniteStats"""
        return unreal.UnrealBridgePerfLibrary.get_nanite_stats()

    @staticmethod
    def get_per_pass_gpu_timings():
        """X.get_per_pass_gpu_timings() -> BridgeGpuPassTimings"""
        return unreal.UnrealBridgePerfLibrary.get_per_pass_gpu_timings()

    @staticmethod
    def get_perf_sampling_state():
        """X.get_perf_sampling_state() -> BridgePerfSamplingState"""
        return unreal.UnrealBridgePerfLibrary.get_perf_sampling_state()

    @staticmethod
    def get_perf_snapshot(*, include_u_object_stats=False, u_object_top_n=20):
        """X.get_perf_snapshot(include_u_object_stats=False, u_object_top_n=20) -> BridgePerfSnapshot"""
        return unreal.UnrealBridgePerfLibrary.get_perf_snapshot(include_u_object_stats, u_object_top_n)

    @staticmethod
    def get_render_counters():
        """X.get_render_counters() -> BridgeRenderCounters"""
        return unreal.UnrealBridgePerfLibrary.get_render_counters()

    @staticmethod
    def get_render_target_memory(*, top_n=30):
        """X.get_render_target_memory(top_n=30) -> BridgeRenderTargetMemory"""
        return unreal.UnrealBridgePerfLibrary.get_render_target_memory(top_n)

    @staticmethod
    def get_shadow_caster_breakdown(*, top_n=30):
        """X.get_shadow_caster_breakdown(top_n=30) -> Array[BridgeActorRenderCost]"""
        return unreal.UnrealBridgePerfLibrary.get_shadow_caster_breakdown(top_n)

    @staticmethod
    def get_texture_memory_breakdown(*, group_by, mode="disk", max_groups=50):
        """X.get_texture_memory_breakdown(group_by, mode="disk", max_groups=50) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_texture_memory_breakdown(group_by, mode, max_groups)

    @staticmethod
    def get_texture_streaming_residency(*, top_n=30):
        """X.get_texture_streaming_residency(top_n=30) -> BridgeTextureStreamingState"""
        return unreal.UnrealBridgePerfLibrary.get_texture_streaming_residency(top_n)

    @staticmethod
    def get_trace_state():
        """X.get_trace_state() -> BridgeTraceState"""
        return unreal.UnrealBridgePerfLibrary.get_trace_state()

    @staticmethod
    def get_u_object_memory_breakdown(*, top_n=20):
        """X.get_u_object_memory_breakdown(top_n=20) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_u_object_memory_breakdown(top_n)

    @staticmethod
    def get_u_object_stats(*, top_n=20):
        """X.get_u_object_stats(top_n=20) -> BridgeUObjectStats"""
        return unreal.UnrealBridgePerfLibrary.get_u_object_stats(top_n)

    @staticmethod
    def get_visible_primitives_by_material(*, viewport_index=0, top_n=50):
        """X.get_visible_primitives_by_material(viewport_index=0, top_n=50) -> Array[BridgeMaterialRenderRow]"""
        return unreal.UnrealBridgePerfLibrary.get_visible_primitives_by_material(viewport_index, top_n)

    @staticmethod
    def get_world_actor_breakdown(*, level_filter, group_by="class", max_groups=200):
        """X.get_world_actor_breakdown(level_filter, group_by="class", max_groups=200) -> Array[BridgePerfBreakdownRow]"""
        return unreal.UnrealBridgePerfLibrary.get_world_actor_breakdown(level_filter, group_by, max_groups)

    @staticmethod
    def list_trace_channels():
        """X.list_trace_channels() -> Array[BridgeTraceChannelInfo]"""
        return unreal.UnrealBridgePerfLibrary.list_trace_channels()

    @staticmethod
    def parse_alloc_trace_to_summary(*, utrace_path):
        """X.parse_alloc_trace_to_summary(utrace_path) -> BridgePerfAllocSummary"""
        return unreal.UnrealBridgePerfLibrary.parse_alloc_trace_to_summary(utrace_path)

    @staticmethod
    def parse_cook_trace_to_summary(*, utrace_path, top_n=50):
        """X.parse_cook_trace_to_summary(utrace_path, top_n=50) -> BridgePerfCookSummary"""
        return unreal.UnrealBridgePerfLibrary.parse_cook_trace_to_summary(utrace_path, top_n)

    @staticmethod
    def parse_net_trace_to_summary(*, utrace_path):
        """X.parse_net_trace_to_summary(utrace_path) -> BridgePerfNetSummary"""
        return unreal.UnrealBridgePerfLibrary.parse_net_trace_to_summary(utrace_path)

    @staticmethod
    def parse_trace_to_summary(*, utrace_path, top_n=20, top_n_per_thread=10, top_n_counters=100):
        """X.parse_trace_to_summary(utrace_path, top_n=20, top_n_per_thread=10, top_n_counters=100) -> BridgePerfTraceSummary"""
        return unreal.UnrealBridgePerfLibrary.parse_trace_to_summary(utrace_path, top_n, top_n_per_thread, top_n_counters)

    @staticmethod
    def reset_frame_time_histogram():
        """X.reset_frame_time_histogram() -> None"""
        return unreal.UnrealBridgePerfLibrary.reset_frame_time_histogram()

    @staticmethod
    def start_perf_sampling(*, period_ms=100, max_samples=600, include_u_object_stats=False):
        """X.start_perf_sampling(period_ms=100, max_samples=600, include_u_object_stats=False) -> bool"""
        return unreal.UnrealBridgePerfLibrary.start_perf_sampling(period_ms, max_samples, include_u_object_stats)

    @staticmethod
    def start_trace_capture(*, channels, output_dir, max_size_mb=500):
        """X.start_trace_capture(channels, output_dir, max_size_mb=500) -> BridgeTraceStartResult"""
        return unreal.UnrealBridgePerfLibrary.start_trace_capture(channels, output_dir, max_size_mb)

    @staticmethod
    def stop_perf_sampling():
        """X.stop_perf_sampling() -> Array[BridgePerfSnapshot]"""
        return unreal.UnrealBridgePerfLibrary.stop_perf_sampling()

    @staticmethod
    def stop_trace_capture():
        """X.stop_trace_capture() -> BridgeTraceStopResult"""
        return unreal.UnrealBridgePerfLibrary.stop_trace_capture()


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
    def set_crashing_legs_channel(*, schema_path, left_thigh_bone, right_thigh_bone, left_foot_bone, right_foot_bone, weight=0.200000, allowed_tolerance=0.300000, use_continuing_pose=True):
        """X.set_crashing_legs_channel(schema_path, left_thigh_bone, right_thigh_bone, left_foot_bone, right_foot_bone, weight=0.200000, allowed_tolerance=0.300000, use_continuing_pose=True) -> BridgePSSCrashingLegsChannelResult"""
        return unreal.UnrealBridgePoseSearchLibrary.set_crashing_legs_channel(schema_path, left_thigh_bone, right_thigh_bone, left_foot_bone, right_foot_bone, weight, allowed_tolerance, use_continuing_pose)

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


class Procedural:
    """Wraps unreal.UnrealBridgeProceduralLibrary (kwargs-only)."""

    @staticmethod
    def add_instances_by_transforms(*, actor_name, xs, world_space):
        """X.add_instances_by_transforms(actor_name, xs, world_space) -> Array[int32]"""
        return unreal.UnrealBridgeProceduralLibrary.add_instances_by_transforms(actor_name, xs, world_space)

    @staticmethod
    def clear_instances(*, actor_name):
        """X.clear_instances(actor_name) -> bool"""
        return unreal.UnrealBridgeProceduralLibrary.clear_instances(actor_name)

    @staticmethod
    def ensure_procedural_ism_actor(*, tag, mesh_path, use_hism):
        """X.ensure_procedural_ism_actor(tag, mesh_path, use_hism) -> str"""
        return unreal.UnrealBridgeProceduralLibrary.ensure_procedural_ism_actor(tag, mesh_path, use_hism)

    @staticmethod
    def filter_points_by_density_mask(*, pts, texture_asset, bounds_xy, channel_index, threshold, seed):
        """X.filter_points_by_density_mask(pts, texture_asset, bounds_xy, channel_index, threshold, seed) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.filter_points_by_density_mask(pts, texture_asset, bounds_xy, channel_index, threshold, seed)

    @staticmethod
    def filter_points_by_landscape_layer(*, pts, landscape_label, layer_name, threshold):
        """X.filter_points_by_landscape_layer(pts, landscape_label, layer_name, threshold) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.filter_points_by_landscape_layer(pts, landscape_label, layer_name, threshold)

    @staticmethod
    def filter_points_by_min_distance(*, in_, min_dist):
        """X.filter_points_by_min_distance(in_, min_dist) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.filter_points_by_min_distance(in_, min_dist)

    @staticmethod
    def filter_points_by_overlap(*, pts, blocking_class_paths, radius):
        """X.filter_points_by_overlap(pts, blocking_class_paths, radius) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.filter_points_by_overlap(pts, blocking_class_paths, radius)

    @staticmethod
    def filter_points_by_slope(*, in_, max_slope_deg, bounce_up):
        """X.filter_points_by_slope(in_, max_slope_deg, bounce_up) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.filter_points_by_slope(in_, max_slope_deg, bounce_up)

    @staticmethod
    def filter_points_inside_actor(*, pts, container_actor_label, inside):
        """X.filter_points_inside_actor(pts, container_actor_label, inside) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.filter_points_inside_actor(pts, container_actor_label, inside)

    @staticmethod
    def jitter_transforms(*, xs, pos_sigma, rot_sigma, scale_min, scale_max, seed):
        """X.jitter_transforms(xs, pos_sigma, rot_sigma, scale_min, scale_max, seed) -> Array[Transform]"""
        return unreal.UnrealBridgeProceduralLibrary.jitter_transforms(xs, pos_sigma, rot_sigma, scale_min, scale_max, seed)

    @staticmethod
    def project_points_to_surface(*, in_, bounce_up, bounce_down):
        """X.project_points_to_surface(in_, bounce_up, bounce_down) -> (Array[Vector], out_hit_normals=Array[Vector])"""
        return unreal.UnrealBridgeProceduralLibrary.project_points_to_surface(in_, bounce_up, bounce_down)

    @staticmethod
    def rebuild_procedural_navigation(*, actor_name):
        """X.rebuild_procedural_navigation(actor_name) -> bool"""
        return unreal.UnrealBridgeProceduralLibrary.rebuild_procedural_navigation(actor_name)

    @staticmethod
    def remove_instances_by_ids(*, actor_name, instance_ids):
        """X.remove_instances_by_ids(actor_name, instance_ids) -> bool"""
        return unreal.UnrealBridgeProceduralLibrary.remove_instances_by_ids(actor_name, instance_ids)

    @staticmethod
    def sample_points_grid(*, bounds, spacing, jitter_ratio, seed):
        """X.sample_points_grid(bounds, spacing, jitter_ratio, seed) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_grid(bounds, spacing, jitter_ratio, seed)

    @staticmethod
    def sample_points_in_volume(*, volume_actor_label, count, seed, max_attempts):
        """X.sample_points_in_volume(volume_actor_label, count, seed, max_attempts) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_in_volume(volume_actor_label, count, seed, max_attempts)

    @staticmethod
    def sample_points_jitter_stratified(*, bounds, grid_resolution, seed):
        """X.sample_points_jitter_stratified(bounds, grid_resolution, seed) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_jitter_stratified(bounds, grid_resolution, seed)

    @staticmethod
    def sample_points_on_landscape(*, landscape_label, bounds2d, count, seed):
        """X.sample_points_on_landscape(landscape_label, bounds2d, count, seed) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_on_landscape(landscape_label, bounds2d, count, seed)

    @staticmethod
    def sample_points_on_spline(*, spline_actor_label, component_name, mode, count_or_spacing):
        """X.sample_points_on_spline(spline_actor_label, component_name, mode, count_or_spacing) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_on_spline(spline_actor_label, component_name, mode, count_or_spacing)

    @staticmethod
    def sample_points_on_surface(*, actor_label, count, seed, max_bounce_up):
        """X.sample_points_on_surface(actor_label, count, seed, max_bounce_up) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_on_surface(actor_label, count, seed, max_bounce_up)

    @staticmethod
    def sample_points_poisson_disk2d(*, bounds, min_radius, max_attempts, seed):
        """X.sample_points_poisson_disk2d(bounds, min_radius, max_attempts, seed) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_poisson_disk2d(bounds, min_radius, max_attempts, seed)

    @staticmethod
    def sample_points_poisson_disk3d(*, bounds, min_radius, max_attempts, seed):
        """X.sample_points_poisson_disk3d(bounds, min_radius, max_attempts, seed) -> Array[Vector]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_points_poisson_disk3d(bounds, min_radius, max_attempts, seed)

    @staticmethod
    def sample_transforms_along_spline(*, spline_actor_label, component_name, mode, count_or_spacing):
        """X.sample_transforms_along_spline(spline_actor_label, component_name, mode, count_or_spacing) -> Array[Transform]"""
        return unreal.UnrealBridgeProceduralLibrary.sample_transforms_along_spline(spline_actor_label, component_name, mode, count_or_spacing)

    @staticmethod
    def update_instance_transforms_by_ids(*, actor_name, ids, new_xs, world_space):
        """X.update_instance_transforms_by_ids(actor_name, ids, new_xs, world_space) -> bool"""
        return unreal.UnrealBridgeProceduralLibrary.update_instance_transforms_by_ids(actor_name, ids, new_xs, world_space)


class Property:
    """Wraps unreal.UnrealBridgePropertyLibrary (kwargs-only)."""

    @staticmethod
    def array_append_u_property(*, object_or_class_path, property_path, element_export_text, fire_change_notify=True):
        """X.array_append_u_property(object_or_class_path, property_path, element_export_text, fire_change_notify=True) -> bool"""
        return unreal.UnrealBridgePropertyLibrary.array_append_u_property(object_or_class_path, property_path, element_export_text, fire_change_notify)

    @staticmethod
    def array_clear_u_property(*, object_or_class_path, property_path, fire_change_notify=True):
        """X.array_clear_u_property(object_or_class_path, property_path, fire_change_notify=True) -> bool"""
        return unreal.UnrealBridgePropertyLibrary.array_clear_u_property(object_or_class_path, property_path, fire_change_notify)

    @staticmethod
    def array_remove_u_property(*, object_or_class_path, property_path, index, fire_change_notify=True):
        """X.array_remove_u_property(object_or_class_path, property_path, index, fire_change_notify=True) -> bool"""
        return unreal.UnrealBridgePropertyLibrary.array_remove_u_property(object_or_class_path, property_path, index, fire_change_notify)

    @staticmethod
    def get_asset_cdo_path(*, asset_path):
        """X.get_asset_cdo_path(asset_path) -> str"""
        return unreal.UnrealBridgePropertyLibrary.get_asset_cdo_path(asset_path)

    @staticmethod
    def get_u_property_as_export_text(*, object_or_class_path, property_path):
        """X.get_u_property_as_export_text(object_or_class_path, property_path) -> (str, out_success=bool)"""
        return unreal.UnrealBridgePropertyLibrary.get_u_property_as_export_text(object_or_class_path, property_path)

    @staticmethod
    def list_u_properties(*, object_or_class_path, include_inherited=True):
        """X.list_u_properties(object_or_class_path, include_inherited=True) -> Array[BridgeUPropertyInfo]"""
        return unreal.UnrealBridgePropertyLibrary.list_u_properties(object_or_class_path, include_inherited)

    @staticmethod
    def set_u_property_from_export_text(*, object_or_class_path, property_path, value_export_text, fire_change_notify=True):
        """X.set_u_property_from_export_text(object_or_class_path, property_path, value_export_text, fire_change_notify=True) -> bool"""
        return unreal.UnrealBridgePropertyLibrary.set_u_property_from_export_text(object_or_class_path, property_path, value_export_text, fire_change_notify)


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


class Rig:
    """Wraps unreal.UnrealBridgeRigLibrary (kwargs-only)."""

    @staticmethod
    def add_control_rig_bone(*, asset_path, name, parent_name, parent_type, transform, global_transform, imported_bone):
        """X.add_control_rig_bone(asset_path, name, parent_name, parent_type, transform, global_transform, imported_bone) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_bone(asset_path, name, parent_name, parent_type, transform, global_transform, imported_bone)

    @staticmethod
    def add_control_rig_branch_node(*, asset_path, graph_name, position, node_name):
        """X.add_control_rig_branch_node(asset_path, graph_name, position, node_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_branch_node(asset_path, graph_name, position, node_name)

    @staticmethod
    def add_control_rig_comment_node(*, asset_path, graph_name, comment_text, position, size, color, node_name):
        """X.add_control_rig_comment_node(asset_path, graph_name, comment_text, position, size, color, node_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_comment_node(asset_path, graph_name, comment_text, position, size, color, node_name)

    @staticmethod
    def add_control_rig_connector(*, asset_path, name, connector_type, description, optional, array):
        """X.add_control_rig_connector(asset_path, name, connector_type, description, optional, array) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_connector(asset_path, name, connector_type, description, optional, array)

    @staticmethod
    def add_control_rig_control(*, asset_path, name, parent_name, parent_type, control_type, initial_value, offset_transform, shape_transform, shape_name, shape_color, animatable):
        """X.add_control_rig_control(asset_path, name, parent_name, parent_type, control_type, initial_value, offset_transform, shape_transform, shape_name, shape_color, animatable) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_control(asset_path, name, parent_name, parent_type, control_type, initial_value, offset_transform, shape_transform, shape_name, shape_color, animatable)

    @staticmethod
    def add_control_rig_curve(*, asset_path, name, value):
        """X.add_control_rig_curve(asset_path, name, value) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_curve(asset_path, name, value)

    @staticmethod
    def add_control_rig_element_tag(*, asset_path, name, element_type, tag):
        """X.add_control_rig_element_tag(asset_path, name, element_type, tag) -> bool"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_element_tag(asset_path, name, element_type, tag)

    @staticmethod
    def add_control_rig_member_variable(*, asset_path, name, cpp_type, default_value, public, read_only):
        """X.add_control_rig_member_variable(asset_path, name, cpp_type, default_value, public, read_only) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_member_variable(asset_path, name, cpp_type, default_value, public, read_only)

    @staticmethod
    def add_control_rig_null(*, asset_path, name, parent_name, parent_type, transform, global_transform):
        """X.add_control_rig_null(asset_path, name, parent_name, parent_type, transform, global_transform) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_null(asset_path, name, parent_name, parent_type, transform, global_transform)

    @staticmethod
    def add_control_rig_template_node(*, asset_path, graph_name, notation, position, node_name):
        """X.add_control_rig_template_node(asset_path, graph_name, notation, position, node_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_template_node(asset_path, graph_name, notation, position, node_name)

    @staticmethod
    def add_control_rig_unit_node(*, asset_path, graph_name, unit_struct_path, method_name, position, node_name):
        """X.add_control_rig_unit_node(asset_path, graph_name, unit_struct_path, method_name, position, node_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_unit_node(asset_path, graph_name, unit_struct_path, method_name, position, node_name)

    @staticmethod
    def add_control_rig_variable_node(*, asset_path, graph_name, variable_name, cpp_type, cpp_type_object_path, getter, default_value, position, node_name, create_member_variable):
        """X.add_control_rig_variable_node(asset_path, graph_name, variable_name, cpp_type, cpp_type_object_path, getter, default_value, position, node_name, create_member_variable) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_control_rig_variable_node(asset_path, graph_name, variable_name, cpp_type, cpp_type_object_path, getter, default_value, position, node_name, create_member_variable)

    @staticmethod
    def add_default_ik_retarget_ops(*, asset_path):
        """X.add_default_ik_retarget_ops(asset_path) -> bool"""
        return unreal.UnrealBridgeRigLibrary.add_default_ik_retarget_ops(asset_path)

    @staticmethod
    def add_ik_retarget_op(*, asset_path, op_type_path, op_name):
        """X.add_ik_retarget_op(asset_path, op_type_path, op_name) -> int32"""
        return unreal.UnrealBridgeRigLibrary.add_ik_retarget_op(asset_path, op_type_path, op_name)

    @staticmethod
    def add_ik_rig_goal(*, asset_path, goal_name, bone_name):
        """X.add_ik_rig_goal(asset_path, goal_name, bone_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_ik_rig_goal(asset_path, goal_name, bone_name)

    @staticmethod
    def add_ik_rig_retarget_chain(*, asset_path, chain_name, start_bone, end_bone, goal_name):
        """X.add_ik_rig_retarget_chain(asset_path, chain_name, start_bone, end_bone, goal_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.add_ik_rig_retarget_chain(asset_path, chain_name, start_bone, end_bone, goal_name)

    @staticmethod
    def add_ik_rig_solver(*, asset_path, solver_type_path):
        """X.add_ik_rig_solver(asset_path, solver_type_path) -> int32"""
        return unreal.UnrealBridgeRigLibrary.add_ik_rig_solver(asset_path, solver_type_path)

    @staticmethod
    def analyze_animation_quality(*, animation_path, foot_bone_names, num_samples, contact_height_tolerance, foot_slide_speed_tolerance, joint_angular_delta_tolerance_degrees, max_reported_bones):
        """X.analyze_animation_quality(animation_path, foot_bone_names, num_samples, contact_height_tolerance, foot_slide_speed_tolerance, joint_angular_delta_tolerance_degrees, max_reported_bones) -> BridgeAnimationQualityReport"""
        return unreal.UnrealBridgeRigLibrary.analyze_animation_quality(animation_path, foot_bone_names, num_samples, contact_height_tolerance, foot_slide_speed_tolerance, joint_angular_delta_tolerance_degrees, max_reported_bones)

    @staticmethod
    def apply_ik_rig_auto_setup(*, asset_path, retarget_definition, full_body_ik):
        """X.apply_ik_rig_auto_setup(asset_path, retarget_definition, full_body_ik) -> bool"""
        return unreal.UnrealBridgeRigLibrary.apply_ik_rig_auto_setup(asset_path, retarget_definition, full_body_ik)

    @staticmethod
    def auto_align_ik_retarget_pose(*, asset_path, side, bone_names, method):
        """X.auto_align_ik_retarget_pose(asset_path, side, bone_names, method) -> bool"""
        return unreal.UnrealBridgeRigLibrary.auto_align_ik_retarget_pose(asset_path, side, bone_names, method)

    @staticmethod
    def auto_layout_control_rig_graph(*, asset_path, graph_name, horizontal_spacing, vertical_spacing):
        """X.auto_layout_control_rig_graph(asset_path, graph_name, horizontal_spacing, vertical_spacing) -> BridgeRigLayoutResult"""
        return unreal.UnrealBridgeRigLibrary.auto_layout_control_rig_graph(asset_path, graph_name, horizontal_spacing, vertical_spacing)

    @staticmethod
    def auto_map_ik_retarget_chains(*, asset_path, mapping_type, force_remap, op_name):
        """X.auto_map_ik_retarget_chains(asset_path, mapping_type, force_remap, op_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.auto_map_ik_retarget_chains(asset_path, mapping_type, force_remap, op_name)

    @staticmethod
    def batch_retarget_animations(*, retargeter_path, source_asset_paths, source_mesh_path, target_mesh_path, destination_folder, search, replace, prefix, suffix, include_referenced_assets, overwrite_existing, save):
        """X.batch_retarget_animations(retargeter_path, source_asset_paths, source_mesh_path, target_mesh_path, destination_folder, search, replace, prefix, suffix, include_referenced_assets, overwrite_existing, save) -> BridgeRetargetBatchResult"""
        return unreal.UnrealBridgeRigLibrary.batch_retarget_animations(retargeter_path, source_asset_paths, source_mesh_path, target_mesh_path, destination_folder, search, replace, prefix, suffix, include_referenced_assets, overwrite_existing, save)

    @staticmethod
    def compile_control_rig(*, asset_path, save):
        """X.compile_control_rig(asset_path, save) -> BridgeRigValidationReport"""
        return unreal.UnrealBridgeRigLibrary.compile_control_rig(asset_path, save)

    @staticmethod
    def configure_ik_retargeter_assets(*, asset_path, source_ik_rig_path, target_ik_rig_path, source_preview_mesh_path, target_preview_mesh_path):
        """X.configure_ik_retargeter_assets(asset_path, source_ik_rig_path, target_ik_rig_path, source_preview_mesh_path, target_preview_mesh_path) -> bool"""
        return unreal.UnrealBridgeRigLibrary.configure_ik_retargeter_assets(asset_path, source_ik_rig_path, target_ik_rig_path, source_preview_mesh_path, target_preview_mesh_path)

    @staticmethod
    def connect_control_rig_pins(*, asset_path, graph_name, output_pin_path, input_pin_path, create_cast_node):
        """X.connect_control_rig_pins(asset_path, graph_name, output_pin_path, input_pin_path, create_cast_node) -> bool"""
        return unreal.UnrealBridgeRigLibrary.connect_control_rig_pins(asset_path, graph_name, output_pin_path, input_pin_path, create_cast_node)

    @staticmethod
    def connect_ik_rig_goal_to_solver(*, asset_path, goal_name, solver_index):
        """X.connect_ik_rig_goal_to_solver(asset_path, goal_name, solver_index) -> bool"""
        return unreal.UnrealBridgeRigLibrary.connect_ik_rig_goal_to_solver(asset_path, goal_name, solver_index)

    @staticmethod
    def create_control_rig(*, asset_path, source_skeletal_asset_path, modular_rig, import_curves):
        """X.create_control_rig(asset_path, source_skeletal_asset_path, modular_rig, import_curves) -> BridgeRigOperationResult"""
        return unreal.UnrealBridgeRigLibrary.create_control_rig(asset_path, source_skeletal_asset_path, modular_rig, import_curves)

    @staticmethod
    def create_ik_retarget_pose(*, asset_path, pose_name, side):
        """X.create_ik_retarget_pose(asset_path, pose_name, side) -> str"""
        return unreal.UnrealBridgeRigLibrary.create_ik_retarget_pose(asset_path, pose_name, side)

    @staticmethod
    def create_ik_retargeter(*, asset_path, source_ik_rig_path, target_ik_rig_path, source_preview_mesh_path, target_preview_mesh_path, add_default_ops):
        """X.create_ik_retargeter(asset_path, source_ik_rig_path, target_ik_rig_path, source_preview_mesh_path, target_preview_mesh_path, add_default_ops) -> BridgeRigOperationResult"""
        return unreal.UnrealBridgeRigLibrary.create_ik_retargeter(asset_path, source_ik_rig_path, target_ik_rig_path, source_preview_mesh_path, target_preview_mesh_path, add_default_ops)

    @staticmethod
    def create_ik_rig(*, asset_path, skeletal_mesh_path):
        """X.create_ik_rig(asset_path, skeletal_mesh_path) -> BridgeRigOperationResult"""
        return unreal.UnrealBridgeRigLibrary.create_ik_rig(asset_path, skeletal_mesh_path)

    @staticmethod
    def disconnect_control_rig_pins(*, asset_path, graph_name, output_pin_path, input_pin_path):
        """X.disconnect_control_rig_pins(asset_path, graph_name, output_pin_path, input_pin_path) -> bool"""
        return unreal.UnrealBridgeRigLibrary.disconnect_control_rig_pins(asset_path, graph_name, output_pin_path, input_pin_path)

    @staticmethod
    def disconnect_ik_rig_goal_from_solver(*, asset_path, goal_name, solver_index):
        """X.disconnect_ik_rig_goal_from_solver(asset_path, goal_name, solver_index) -> bool"""
        return unreal.UnrealBridgeRigLibrary.disconnect_ik_rig_goal_from_solver(asset_path, goal_name, solver_index)

    @staticmethod
    def duplicate_ik_retarget_pose(*, asset_path, pose_name, new_name, side):
        """X.duplicate_ik_retarget_pose(asset_path, pose_name, new_name, side) -> str"""
        return unreal.UnrealBridgeRigLibrary.duplicate_ik_retarget_pose(asset_path, pose_name, new_name, side)

    @staticmethod
    def evaluate_control_rig(*, asset_path, event_name, input_controls):
        """X.evaluate_control_rig(asset_path, event_name, input_controls) -> BridgeControlRigEvaluationResult"""
        return unreal.UnrealBridgeRigLibrary.evaluate_control_rig(asset_path, event_name, input_controls)

    @staticmethod
    def get_control_rig_control_property(*, asset_path, control_name, property_path):
        """X.get_control_rig_control_property(asset_path, control_name, property_path) -> BridgeRigPropertyResult"""
        return unreal.UnrealBridgeRigLibrary.get_control_rig_control_property(asset_path, control_name, property_path)

    @staticmethod
    def get_control_rig_info(*, asset_path):
        """X.get_control_rig_info(asset_path) -> BridgeControlRigInfo"""
        return unreal.UnrealBridgeRigLibrary.get_control_rig_info(asset_path)

    @staticmethod
    def get_ik_retarget_op_property(*, asset_path, op_index, property_path):
        """X.get_ik_retarget_op_property(asset_path, op_index, property_path) -> BridgeRigPropertyResult"""
        return unreal.UnrealBridgeRigLibrary.get_ik_retarget_op_property(asset_path, op_index, property_path)

    @staticmethod
    def get_ik_retargeter_info(*, asset_path):
        """X.get_ik_retargeter_info(asset_path) -> BridgeIKRetargeterInfo"""
        return unreal.UnrealBridgeRigLibrary.get_ik_retargeter_info(asset_path)

    @staticmethod
    def get_ik_rig_info(*, asset_path):
        """X.get_ik_rig_info(asset_path) -> BridgeIKRigInfo"""
        return unreal.UnrealBridgeRigLibrary.get_ik_rig_info(asset_path)

    @staticmethod
    def get_ik_rig_property(*, asset_path, target_kind, solver_index, target_name, property_path):
        """X.get_ik_rig_property(asset_path, target_kind, solver_index, target_name, property_path) -> BridgeRigPropertyResult"""
        return unreal.UnrealBridgeRigLibrary.get_ik_rig_property(asset_path, target_kind, solver_index, target_name, property_path)

    @staticmethod
    def get_last_rig_error():
        """X.get_last_rig_error() -> str"""
        return unreal.UnrealBridgeRigLibrary.get_last_rig_error()

    @staticmethod
    def import_control_rig_hierarchy(*, asset_path, source_skeletal_asset_path, replace_existing, import_curves):
        """X.import_control_rig_hierarchy(asset_path, source_skeletal_asset_path, replace_existing, import_curves) -> bool"""
        return unreal.UnrealBridgeRigLibrary.import_control_rig_hierarchy(asset_path, source_skeletal_asset_path, replace_existing, import_curves)

    @staticmethod
    def is_rig_api_available():
        """X.is_rig_api_available() -> bool"""
        return unreal.UnrealBridgeRigLibrary.is_rig_api_available()

    @staticmethod
    def list_control_rig_control_properties(*, asset_path, control_name):
        """X.list_control_rig_control_properties(asset_path, control_name) -> Array[BridgeRigPropertyInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_control_rig_control_properties(asset_path, control_name)

    @staticmethod
    def list_control_rig_elements(*, asset_path, element_type):
        """X.list_control_rig_elements(asset_path, element_type) -> Array[BridgeRigElementInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_control_rig_elements(asset_path, element_type)

    @staticmethod
    def list_control_rig_graphs(*, asset_path):
        """X.list_control_rig_graphs(asset_path) -> Array[BridgeRigVMGraphInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_control_rig_graphs(asset_path)

    @staticmethod
    def list_control_rig_links(*, asset_path, graph_name):
        """X.list_control_rig_links(asset_path, graph_name) -> Array[BridgeRigVMLinkInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_control_rig_links(asset_path, graph_name)

    @staticmethod
    def list_control_rig_nodes(*, asset_path, graph_name):
        """X.list_control_rig_nodes(asset_path, graph_name) -> Array[BridgeRigVMNodeInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_control_rig_nodes(asset_path, graph_name)

    @staticmethod
    def list_ik_retarget_chain_mappings(*, asset_path, op_name):
        """X.list_ik_retarget_chain_mappings(asset_path, op_name) -> Array[BridgeIKChainMappingInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_retarget_chain_mappings(asset_path, op_name)

    @staticmethod
    def list_ik_retarget_op_properties(*, asset_path, op_index):
        """X.list_ik_retarget_op_properties(asset_path, op_index) -> Array[BridgeRigPropertyInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_retarget_op_properties(asset_path, op_index)

    @staticmethod
    def list_ik_retarget_ops(*, asset_path):
        """X.list_ik_retarget_ops(asset_path) -> Array[BridgeIKRetargetOpInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_retarget_ops(asset_path)

    @staticmethod
    def list_ik_retarget_poses(*, asset_path, side):
        """X.list_ik_retarget_poses(asset_path, side) -> Array[BridgeIKRetargetPoseInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_retarget_poses(asset_path, side)

    @staticmethod
    def list_ik_retarget_profiles(*, asset_path):
        """X.list_ik_retarget_profiles(asset_path) -> Array[str]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_retarget_profiles(asset_path)

    @staticmethod
    def list_ik_rig_goals(*, asset_path):
        """X.list_ik_rig_goals(asset_path) -> Array[BridgeIKGoalInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_rig_goals(asset_path)

    @staticmethod
    def list_ik_rig_properties(*, asset_path, target_kind, solver_index, target_name):
        """X.list_ik_rig_properties(asset_path, target_kind, solver_index, target_name) -> Array[BridgeRigPropertyInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_rig_properties(asset_path, target_kind, solver_index, target_name)

    @staticmethod
    def list_ik_rig_retarget_chains(*, asset_path):
        """X.list_ik_rig_retarget_chains(asset_path) -> Array[BridgeIKChainInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_rig_retarget_chains(asset_path)

    @staticmethod
    def list_ik_rig_solvers(*, asset_path):
        """X.list_ik_rig_solvers(asset_path) -> Array[BridgeIKSolverInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_ik_rig_solvers(asset_path)

    @staticmethod
    def list_rig_types(*, kind, query, max_results):
        """X.list_rig_types(kind, query, max_results) -> Array[BridgeRigTypeInfo]"""
        return unreal.UnrealBridgeRigLibrary.list_rig_types(kind, query, max_results)

    @staticmethod
    def move_ik_retarget_op(*, asset_path, op_index, target_index):
        """X.move_ik_retarget_op(asset_path, op_index, target_index) -> bool"""
        return unreal.UnrealBridgeRigLibrary.move_ik_retarget_op(asset_path, op_index, target_index)

    @staticmethod
    def move_ik_rig_solver(*, asset_path, solver_index, target_index):
        """X.move_ik_rig_solver(asset_path, solver_index, target_index) -> bool"""
        return unreal.UnrealBridgeRigLibrary.move_ik_rig_solver(asset_path, solver_index, target_index)

    @staticmethod
    def remove_control_rig_element(*, asset_path, name, element_type):
        """X.remove_control_rig_element(asset_path, name, element_type) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_control_rig_element(asset_path, name, element_type)

    @staticmethod
    def remove_control_rig_element_tag(*, asset_path, name, element_type, tag):
        """X.remove_control_rig_element_tag(asset_path, name, element_type, tag) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_control_rig_element_tag(asset_path, name, element_type, tag)

    @staticmethod
    def remove_control_rig_member_variable(*, asset_path, name):
        """X.remove_control_rig_member_variable(asset_path, name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_control_rig_member_variable(asset_path, name)

    @staticmethod
    def remove_control_rig_node(*, asset_path, graph_name, node_name):
        """X.remove_control_rig_node(asset_path, graph_name, node_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_control_rig_node(asset_path, graph_name, node_name)

    @staticmethod
    def remove_ik_retarget_op(*, asset_path, op_index):
        """X.remove_ik_retarget_op(asset_path, op_index) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_ik_retarget_op(asset_path, op_index)

    @staticmethod
    def remove_ik_retarget_pose(*, asset_path, pose_name, side):
        """X.remove_ik_retarget_pose(asset_path, pose_name, side) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_ik_retarget_pose(asset_path, pose_name, side)

    @staticmethod
    def remove_ik_retarget_profile(*, asset_path, profile_name):
        """X.remove_ik_retarget_profile(asset_path, profile_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_ik_retarget_profile(asset_path, profile_name)

    @staticmethod
    def remove_ik_rig_goal(*, asset_path, goal_name):
        """X.remove_ik_rig_goal(asset_path, goal_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_ik_rig_goal(asset_path, goal_name)

    @staticmethod
    def remove_ik_rig_retarget_chain(*, asset_path, chain_name):
        """X.remove_ik_rig_retarget_chain(asset_path, chain_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_ik_rig_retarget_chain(asset_path, chain_name)

    @staticmethod
    def remove_ik_rig_solver(*, asset_path, solver_index):
        """X.remove_ik_rig_solver(asset_path, solver_index) -> bool"""
        return unreal.UnrealBridgeRigLibrary.remove_ik_rig_solver(asset_path, solver_index)

    @staticmethod
    def rename_control_rig_element(*, asset_path, name, element_type, new_name):
        """X.rename_control_rig_element(asset_path, name, element_type, new_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.rename_control_rig_element(asset_path, name, element_type, new_name)

    @staticmethod
    def rename_ik_retarget_pose(*, asset_path, pose_name, new_name, side):
        """X.rename_ik_retarget_pose(asset_path, pose_name, new_name, side) -> bool"""
        return unreal.UnrealBridgeRigLibrary.rename_ik_retarget_pose(asset_path, pose_name, new_name, side)

    @staticmethod
    def rename_ik_rig_retarget_chain(*, asset_path, chain_name, new_name):
        """X.rename_ik_rig_retarget_chain(asset_path, chain_name, new_name) -> str"""
        return unreal.UnrealBridgeRigLibrary.rename_ik_rig_retarget_chain(asset_path, chain_name, new_name)

    @staticmethod
    def reparent_control_rig_element(*, asset_path, name, element_type, parent_name, parent_type, maintain_global_transform):
        """X.reparent_control_rig_element(asset_path, name, element_type, parent_name, parent_type, maintain_global_transform) -> bool"""
        return unreal.UnrealBridgeRigLibrary.reparent_control_rig_element(asset_path, name, element_type, parent_name, parent_type, maintain_global_transform)

    @staticmethod
    def reset_ik_retarget_pose(*, asset_path, side, pose_name, bone_names):
        """X.reset_ik_retarget_pose(asset_path, side, pose_name, bone_names) -> bool"""
        return unreal.UnrealBridgeRigLibrary.reset_ik_retarget_pose(asset_path, side, pose_name, bone_names)

    @staticmethod
    def save_current_ik_retarget_profile(*, asset_path, profile_name, apply_source_pose, apply_target_pose, force_all_ik_off):
        """X.save_current_ik_retarget_profile(asset_path, profile_name, apply_source_pose, apply_target_pose, force_all_ik_off) -> bool"""
        return unreal.UnrealBridgeRigLibrary.save_current_ik_retarget_profile(asset_path, profile_name, apply_source_pose, apply_target_pose, force_all_ik_off)

    @staticmethod
    def set_control_rig_control_property(*, asset_path, control_name, property_path, value):
        """X.set_control_rig_control_property(asset_path, control_name, property_path, value) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_control_rig_control_property(asset_path, control_name, property_path, value)

    @staticmethod
    def set_control_rig_control_shape(*, asset_path, control_name, shape_name, shape_color, visible):
        """X.set_control_rig_control_shape(asset_path, control_name, shape_name, shape_color, visible) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_control_rig_control_shape(asset_path, control_name, shape_name, shape_color, visible)

    @staticmethod
    def set_control_rig_element_transform(*, asset_path, name, element_type, transform, global_transform, initial, affect_children):
        """X.set_control_rig_element_transform(asset_path, name, element_type, transform, global_transform, initial, affect_children) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_control_rig_element_transform(asset_path, name, element_type, transform, global_transform, initial, affect_children)

    @staticmethod
    def set_control_rig_node_position(*, asset_path, graph_name, node_name, position):
        """X.set_control_rig_node_position(asset_path, graph_name, node_name, position) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_control_rig_node_position(asset_path, graph_name, node_name, position)

    @staticmethod
    def set_control_rig_pin_default_value(*, asset_path, graph_name, pin_path, default_value, resize_arrays):
        """X.set_control_rig_pin_default_value(asset_path, graph_name, pin_path, default_value, resize_arrays) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_control_rig_pin_default_value(asset_path, graph_name, pin_path, default_value, resize_arrays)

    @staticmethod
    def set_current_ik_retarget_pose(*, asset_path, pose_name, side):
        """X.set_current_ik_retarget_pose(asset_path, pose_name, side) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_current_ik_retarget_pose(asset_path, pose_name, side)

    @staticmethod
    def set_current_ik_retarget_profile(*, asset_path, profile_name):
        """X.set_current_ik_retarget_profile(asset_path, profile_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_current_ik_retarget_profile(asset_path, profile_name)

    @staticmethod
    def set_ik_retarget_chain_mapping(*, asset_path, target_chain_name, source_chain_name, op_name):
        """X.set_ik_retarget_chain_mapping(asset_path, target_chain_name, source_chain_name, op_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_retarget_chain_mapping(asset_path, target_chain_name, source_chain_name, op_name)

    @staticmethod
    def set_ik_retarget_op_enabled(*, asset_path, op_index, enabled):
        """X.set_ik_retarget_op_enabled(asset_path, op_index, enabled) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_retarget_op_enabled(asset_path, op_index, enabled)

    @staticmethod
    def set_ik_retarget_op_parent(*, asset_path, child_op_name, parent_op_name):
        """X.set_ik_retarget_op_parent(asset_path, child_op_name, parent_op_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_retarget_op_parent(asset_path, child_op_name, parent_op_name)

    @staticmethod
    def set_ik_retarget_op_property(*, asset_path, op_index, property_path, value):
        """X.set_ik_retarget_op_property(asset_path, op_index, property_path, value) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_retarget_op_property(asset_path, op_index, property_path, value)

    @staticmethod
    def set_ik_retarget_pose_bone_rotation(*, asset_path, side, bone_name, rotation_offset):
        """X.set_ik_retarget_pose_bone_rotation(asset_path, side, bone_name, rotation_offset) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_retarget_pose_bone_rotation(asset_path, side, bone_name, rotation_offset)

    @staticmethod
    def set_ik_retarget_pose_root_offset(*, asset_path, side, translation_offset):
        """X.set_ik_retarget_pose_root_offset(asset_path, side, translation_offset) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_retarget_pose_root_offset(asset_path, side, translation_offset)

    @staticmethod
    def set_ik_rig_bone_excluded(*, asset_path, bone_name, excluded):
        """X.set_ik_rig_bone_excluded(asset_path, bone_name, excluded) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_rig_bone_excluded(asset_path, bone_name, excluded)

    @staticmethod
    def set_ik_rig_property(*, asset_path, target_kind, solver_index, target_name, property_path, value):
        """X.set_ik_rig_property(asset_path, target_kind, solver_index, target_name, property_path, value) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_rig_property(asset_path, target_kind, solver_index, target_name, property_path, value)

    @staticmethod
    def set_ik_rig_retarget_root(*, asset_path, root_bone_name):
        """X.set_ik_rig_retarget_root(asset_path, root_bone_name) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_rig_retarget_root(asset_path, root_bone_name)

    @staticmethod
    def set_ik_rig_solver_bones(*, asset_path, solver_index, start_bone, end_bone):
        """X.set_ik_rig_solver_bones(asset_path, solver_index, start_bone, end_bone) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_rig_solver_bones(asset_path, solver_index, start_bone, end_bone)

    @staticmethod
    def set_ik_rig_solver_enabled(*, asset_path, solver_index, enabled):
        """X.set_ik_rig_solver_enabled(asset_path, solver_index, enabled) -> bool"""
        return unreal.UnrealBridgeRigLibrary.set_ik_rig_solver_enabled(asset_path, solver_index, enabled)

    @staticmethod
    def validate_control_rig(*, asset_path, save):
        """X.validate_control_rig(asset_path, save) -> BridgeRigValidationReport"""
        return unreal.UnrealBridgeRigLibrary.validate_control_rig(asset_path, save)

    @staticmethod
    def validate_ik_retargeter(*, asset_path, initialize_processor, save):
        """X.validate_ik_retargeter(asset_path, initialize_processor, save) -> BridgeRigValidationReport"""
        return unreal.UnrealBridgeRigLibrary.validate_ik_retargeter(asset_path, initialize_processor, save)

    @staticmethod
    def validate_ik_rig(*, asset_path, save):
        """X.validate_ik_rig(asset_path, save) -> BridgeRigValidationReport"""
        return unreal.UnrealBridgeRigLibrary.validate_ik_rig(asset_path, save)


class SmartObject:
    """Wraps unreal.UnrealBridgeSmartObjectLibrary (kwargs-only)."""

    @staticmethod
    def add_smart_object_behavior_definition(*, asset_path, behavior_class_path, slot_id="", insert_index=-1):
        """X.add_smart_object_behavior_definition(asset_path, behavior_class_path, slot_id="", insert_index=-1) -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_behavior_definition(asset_path, behavior_class_path, slot_id, insert_index)

    @staticmethod
    def add_smart_object_binding(*, asset_path, source_id, source_path, target_id, target_path):
        """X.add_smart_object_binding(asset_path, source_id, source_path, target_id, target_path) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_binding(asset_path, source_id, source_path, target_id, target_path)

    @staticmethod
    def add_smart_object_component(*, actor_path, definition_asset_path, component_name="SmartObject", can_be_part_of_collection=False, register_with_subsystem=True):
        """X.add_smart_object_component(actor_path, definition_asset_path, component_name="SmartObject", can_be_part_of_collection=False, register_with_subsystem=True) -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_component(actor_path, definition_asset_path, component_name, can_be_part_of_collection, register_with_subsystem)

    @staticmethod
    def add_smart_object_definition_data(*, asset_path, struct_type_path, slot_id="", insert_index=-1):
        """X.add_smart_object_definition_data(asset_path, struct_type_path, slot_id="", insert_index=-1) -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_definition_data(asset_path, struct_type_path, slot_id, insert_index)

    @staticmethod
    def add_smart_object_parameter(*, asset_path, name, type, default_value=""):
        """X.add_smart_object_parameter(asset_path, name, type, default_value="") -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_parameter(asset_path, name, type, default_value)

    @staticmethod
    def add_smart_object_slot(*, asset_path, name, offset, rotation, enabled=True, insert_index=-1):
        """X.add_smart_object_slot(asset_path, name, offset, rotation, enabled=True, insert_index=-1) -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_slot(asset_path, name, offset, rotation, enabled, insert_index)

    @staticmethod
    def add_smart_object_world_condition(*, asset_path, condition_struct_path, slot_id="", operator="And", expression_depth=0, invert=False, insert_index=-1):
        """X.add_smart_object_world_condition(asset_path, condition_struct_path, slot_id="", operator="And", expression_depth=0, invert=False, insert_index=-1) -> int32"""
        return unreal.UnrealBridgeSmartObjectLibrary.add_smart_object_world_condition(asset_path, condition_struct_path, slot_id, operator, expression_depth, invert, insert_index)

    @staticmethod
    def claim_smart_object_slot(*, slot_handle, user_actor_path="", claim_priority="Normal"):
        """X.claim_smart_object_slot(slot_handle, user_actor_path="", claim_priority="Normal") -> BridgeSmartObjectClaimResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.claim_smart_object_slot(slot_handle, user_actor_path, claim_priority)

    @staticmethod
    def control_persistent_smart_object_collection(*, collection_actor_path, action):
        """X.control_persistent_smart_object_collection(collection_actor_path, action) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.control_persistent_smart_object_collection(collection_actor_path, action)

    @staticmethod
    def control_smart_object_component(*, component_path, action):
        """X.control_smart_object_component(component_path, action) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.control_smart_object_component(component_path, action)

    @staticmethod
    def create_persistent_smart_object_collection(*, actor_label="SmartObjectPersistentCollection"):
        """X.create_persistent_smart_object_collection(actor_label="SmartObjectPersistentCollection") -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.create_persistent_smart_object_collection(actor_label)

    @staticmethod
    def create_runtime_smart_object(*, definition_asset_path, transform, owner_actor_path=""):
        """X.create_runtime_smart_object(definition_asset_path, transform, owner_actor_path="") -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.create_runtime_smart_object(definition_asset_path, transform, owner_actor_path)

    @staticmethod
    def create_smart_object_definition(*, asset_path):
        """X.create_smart_object_definition(asset_path) -> BridgeSmartObjectCreateResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.create_smart_object_definition(asset_path)

    @staticmethod
    def debug_smart_object_subsystem(*, action):
        """X.debug_smart_object_subsystem(action) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.debug_smart_object_subsystem(action)

    @staticmethod
    def destroy_persistent_smart_object_collection(*, collection_actor_path):
        """X.destroy_persistent_smart_object_collection(collection_actor_path) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.destroy_persistent_smart_object_collection(collection_actor_path)

    @staticmethod
    def destroy_runtime_smart_object(*, smart_object_handle):
        """X.destroy_runtime_smart_object(smart_object_handle) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.destroy_runtime_smart_object(smart_object_handle)

    @staticmethod
    def duplicate_smart_object_slot(*, asset_path, source_slot_id, new_name="", insert_index=-1):
        """X.duplicate_smart_object_slot(asset_path, source_slot_id, new_name="", insert_index=-1) -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.duplicate_smart_object_slot(asset_path, source_slot_id, new_name, insert_index)

    @staticmethod
    def find_smart_object_entrance(*, slot_handle, request):
        """X.find_smart_object_entrance(slot_handle, request) -> BridgeSmartObjectEntranceResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.find_smart_object_entrance(slot_handle, request)

    @staticmethod
    def get_last_smart_object_error():
        """X.get_last_smart_object_error() -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_last_smart_object_error()

    @staticmethod
    def get_smart_object_behavior_property(*, asset_path, behavior_object_path, property_path):
        """X.get_smart_object_behavior_property(asset_path, behavior_object_path, property_path) -> BridgeSmartObjectPropertyResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_behavior_property(asset_path, behavior_object_path, property_path)

    @staticmethod
    def get_smart_object_component_info(*, component_path):
        """X.get_smart_object_component_info(component_path) -> BridgeSmartObjectComponentInfo"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_component_info(component_path)

    @staticmethod
    def get_smart_object_definition_data_property(*, asset_path, data_id, property_path):
        """X.get_smart_object_definition_data_property(asset_path, data_id, property_path) -> BridgeSmartObjectPropertyResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_definition_data_property(asset_path, data_id, property_path)

    @staticmethod
    def get_smart_object_definition_info(*, asset_path):
        """X.get_smart_object_definition_info(asset_path) -> BridgeSmartObjectDefinitionInfo"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_definition_info(asset_path)

    @staticmethod
    def get_smart_object_definition_property(*, asset_path, property_path):
        """X.get_smart_object_definition_property(asset_path, property_path) -> BridgeSmartObjectPropertyResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_definition_property(asset_path, property_path)

    @staticmethod
    def get_smart_object_slot_property(*, asset_path, slot_id, property_path):
        """X.get_smart_object_slot_property(asset_path, slot_id, property_path) -> BridgeSmartObjectPropertyResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_slot_property(asset_path, slot_id, property_path)

    @staticmethod
    def get_smart_object_tag_query_json(*, asset_path, slot_id=""):
        """X.get_smart_object_tag_query_json(asset_path, slot_id="") -> str"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_tag_query_json(asset_path, slot_id)

    @staticmethod
    def get_smart_object_world_condition_property(*, asset_path, slot_id, condition_index, property_path):
        """X.get_smart_object_world_condition_property(asset_path, slot_id, condition_index, property_path) -> BridgeSmartObjectPropertyResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.get_smart_object_world_condition_property(asset_path, slot_id, condition_index, property_path)

    @staticmethod
    def is_smart_object_api_available():
        """X.is_smart_object_api_available() -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.is_smart_object_api_available()

    @staticmethod
    def list_persistent_smart_object_collection_entries(*, collection_actor_path):
        """X.list_persistent_smart_object_collection_entries(collection_actor_path) -> Array[BridgeSmartObjectCollectionEntryInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_persistent_smart_object_collection_entries(collection_actor_path)

    @staticmethod
    def list_persistent_smart_object_collections(*, pie_only=False):
        """X.list_persistent_smart_object_collections(pie_only=False) -> Array[BridgeSmartObjectCollectionInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_persistent_smart_object_collections(pie_only)

    @staticmethod
    def list_smart_object_behavior_definitions(*, asset_path, slot_id=""):
        """X.list_smart_object_behavior_definitions(asset_path, slot_id="") -> Array[BridgeSmartObjectBehaviorInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_behavior_definitions(asset_path, slot_id)

    @staticmethod
    def list_smart_object_behavior_properties(*, asset_path, behavior_object_path, include_inherited=True):
        """X.list_smart_object_behavior_properties(asset_path, behavior_object_path, include_inherited=True) -> Array[BridgeSmartObjectPropertyInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_behavior_properties(asset_path, behavior_object_path, include_inherited)

    @staticmethod
    def list_smart_object_behavior_types():
        """X.list_smart_object_behavior_types() -> Array[BridgeSmartObjectTypeInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_behavior_types()

    @staticmethod
    def list_smart_object_bindable_structs(*, asset_path):
        """X.list_smart_object_bindable_structs(asset_path) -> Array[BridgeSmartObjectBindableStructInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_bindable_structs(asset_path)

    @staticmethod
    def list_smart_object_bindings(*, asset_path):
        """X.list_smart_object_bindings(asset_path) -> Array[BridgeSmartObjectBindingInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_bindings(asset_path)

    @staticmethod
    def list_smart_object_claims():
        """X.list_smart_object_claims() -> Array[BridgeSmartObjectClaimResult]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_claims()

    @staticmethod
    def list_smart_object_components(*, pie_only=False):
        """X.list_smart_object_components(pie_only=False) -> Array[BridgeSmartObjectComponentInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_components(pie_only)

    @staticmethod
    def list_smart_object_definition_data(*, asset_path, slot_id=""):
        """X.list_smart_object_definition_data(asset_path, slot_id="") -> Array[BridgeSmartObjectDefinitionDataInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_definition_data(asset_path, slot_id)

    @staticmethod
    def list_smart_object_definition_data_properties(*, asset_path, data_id, include_inherited=True):
        """X.list_smart_object_definition_data_properties(asset_path, data_id, include_inherited=True) -> Array[BridgeSmartObjectPropertyInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_definition_data_properties(asset_path, data_id, include_inherited)

    @staticmethod
    def list_smart_object_definition_data_types(*, asset_path, slot_id=""):
        """X.list_smart_object_definition_data_types(asset_path, slot_id="") -> Array[BridgeSmartObjectTypeInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_definition_data_types(asset_path, slot_id)

    @staticmethod
    def list_smart_object_definition_properties(*, asset_path, include_inherited=True):
        """X.list_smart_object_definition_properties(asset_path, include_inherited=True) -> Array[BridgeSmartObjectPropertyInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_definition_properties(asset_path, include_inherited)

    @staticmethod
    def list_smart_object_parameters(*, asset_path):
        """X.list_smart_object_parameters(asset_path) -> Array[BridgeSmartObjectParameterInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_parameters(asset_path)

    @staticmethod
    def list_smart_object_runtime_slots(*, smart_object_handle="", component_path="", claim_priority="Normal"):
        """X.list_smart_object_runtime_slots(smart_object_handle="", component_path="", claim_priority="Normal") -> Array[BridgeSmartObjectRuntimeSlotInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_runtime_slots(smart_object_handle, component_path, claim_priority)

    @staticmethod
    def list_smart_object_slot_properties(*, asset_path, slot_id):
        """X.list_smart_object_slot_properties(asset_path, slot_id) -> Array[BridgeSmartObjectPropertyInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_slot_properties(asset_path, slot_id)

    @staticmethod
    def list_smart_object_slots(*, asset_path):
        """X.list_smart_object_slots(asset_path) -> Array[BridgeSmartObjectSlotInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_slots(asset_path)

    @staticmethod
    def list_smart_object_world_condition_properties(*, asset_path, slot_id, condition_index, include_inherited=True):
        """X.list_smart_object_world_condition_properties(asset_path, slot_id, condition_index, include_inherited=True) -> Array[BridgeSmartObjectPropertyInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_world_condition_properties(asset_path, slot_id, condition_index, include_inherited)

    @staticmethod
    def list_smart_object_world_condition_types(*, asset_path, slot_id=""):
        """X.list_smart_object_world_condition_types(asset_path, slot_id="") -> Array[BridgeSmartObjectTypeInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_world_condition_types(asset_path, slot_id)

    @staticmethod
    def list_smart_object_world_conditions(*, asset_path, slot_id=""):
        """X.list_smart_object_world_conditions(asset_path, slot_id="") -> Array[BridgeSmartObjectWorldConditionInfo]"""
        return unreal.UnrealBridgeSmartObjectLibrary.list_smart_object_world_conditions(asset_path, slot_id)

    @staticmethod
    def move_smart_object_behavior_definition(*, asset_path, behavior_object_path, new_index):
        """X.move_smart_object_behavior_definition(asset_path, behavior_object_path, new_index) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.move_smart_object_behavior_definition(asset_path, behavior_object_path, new_index)

    @staticmethod
    def move_smart_object_definition_data(*, asset_path, data_id, new_index):
        """X.move_smart_object_definition_data(asset_path, data_id, new_index) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.move_smart_object_definition_data(asset_path, data_id, new_index)

    @staticmethod
    def move_smart_object_slot(*, asset_path, slot_id, new_index):
        """X.move_smart_object_slot(asset_path, slot_id, new_index) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.move_smart_object_slot(asset_path, slot_id, new_index)

    @staticmethod
    def move_smart_object_world_condition(*, asset_path, slot_id, condition_index, new_index):
        """X.move_smart_object_world_condition(asset_path, slot_id, condition_index, new_index) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.move_smart_object_world_condition(asset_path, slot_id, condition_index, new_index)

    @staticmethod
    def occupy_smart_object_claim(*, claim_token, behavior_class_path):
        """X.occupy_smart_object_claim(claim_token, behavior_class_path) -> BridgeSmartObjectClaimResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.occupy_smart_object_claim(claim_token, behavior_class_path)

    @staticmethod
    def query_smart_objects(*, center, extent, user_tags, activity_tags, behavior_class_paths, activity_match="All", claim_priority="Normal", evaluate_conditions=True, include_claimed_slots=False, include_disabled_slots=False, user_actor_path="", sort_by_distance=True, max_results=0):
        """X.query_smart_objects(center, extent, user_tags, activity_tags, behavior_class_paths, activity_match="All", claim_priority="Normal", evaluate_conditions=True, include_claimed_slots=False, include_disabled_slots=False, user_actor_path="", sort_by_distance=True, max_results=0) -> Array[BridgeSmartObjectQueryResult]"""
        return unreal.UnrealBridgeSmartObjectLibrary.query_smart_objects(center, extent, user_tags, activity_tags, behavior_class_paths, activity_match, claim_priority, evaluate_conditions, include_claimed_slots, include_disabled_slots, user_actor_path, sort_by_distance, max_results)

    @staticmethod
    def release_smart_object_claim(*, claim_token):
        """X.release_smart_object_claim(claim_token) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.release_smart_object_claim(claim_token)

    @staticmethod
    def remove_smart_object_behavior_definition(*, asset_path, behavior_object_path):
        """X.remove_smart_object_behavior_definition(asset_path, behavior_object_path) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_behavior_definition(asset_path, behavior_object_path)

    @staticmethod
    def remove_smart_object_binding(*, asset_path, target_id, target_path):
        """X.remove_smart_object_binding(asset_path, target_id, target_path) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_binding(asset_path, target_id, target_path)

    @staticmethod
    def remove_smart_object_component(*, component_path):
        """X.remove_smart_object_component(component_path) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_component(component_path)

    @staticmethod
    def remove_smart_object_definition_data(*, asset_path, data_id):
        """X.remove_smart_object_definition_data(asset_path, data_id) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_definition_data(asset_path, data_id)

    @staticmethod
    def remove_smart_object_parameter(*, asset_path, name):
        """X.remove_smart_object_parameter(asset_path, name) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_parameter(asset_path, name)

    @staticmethod
    def remove_smart_object_slot(*, asset_path, slot_id):
        """X.remove_smart_object_slot(asset_path, slot_id) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_slot(asset_path, slot_id)

    @staticmethod
    def remove_smart_object_world_condition(*, asset_path, slot_id, condition_index):
        """X.remove_smart_object_world_condition(asset_path, slot_id, condition_index) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.remove_smart_object_world_condition(asset_path, slot_id, condition_index)

    @staticmethod
    def rename_smart_object_parameter(*, asset_path, old_name, new_name):
        """X.rename_smart_object_parameter(asset_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.rename_smart_object_parameter(asset_path, old_name, new_name)

    @staticmethod
    def send_smart_object_slot_event(*, slot_handle, event_tag):
        """X.send_smart_object_slot_event(slot_handle, event_tag) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.send_smart_object_slot_event(slot_handle, event_tag)

    @staticmethod
    def set_smart_object_behavior_property(*, asset_path, behavior_object_path, property_path, value):
        """X.set_smart_object_behavior_property(asset_path, behavior_object_path, property_path, value) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_behavior_property(asset_path, behavior_object_path, property_path, value)

    @staticmethod
    def set_smart_object_component_definition(*, component_path, definition_asset_path, register_with_subsystem=True):
        """X.set_smart_object_component_definition(component_path, definition_asset_path, register_with_subsystem=True) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_component_definition(component_path, definition_asset_path, register_with_subsystem)

    @staticmethod
    def set_smart_object_definition_data_property(*, asset_path, data_id, property_path, value):
        """X.set_smart_object_definition_data_property(asset_path, data_id, property_path, value) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_definition_data_property(asset_path, data_id, property_path, value)

    @staticmethod
    def set_smart_object_definition_property(*, asset_path, property_path, value):
        """X.set_smart_object_definition_property(asset_path, property_path, value) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_definition_property(asset_path, property_path, value)

    @staticmethod
    def set_smart_object_parameter_value(*, asset_path, name, value):
        """X.set_smart_object_parameter_value(asset_path, name, value) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_parameter_value(asset_path, name, value)

    @staticmethod
    def set_smart_object_runtime_enabled(*, smart_object_handle, enabled, reason_tag=""):
        """X.set_smart_object_runtime_enabled(smart_object_handle, enabled, reason_tag="") -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_runtime_enabled(smart_object_handle, enabled, reason_tag)

    @staticmethod
    def set_smart_object_runtime_slot_enabled(*, slot_handle, enabled):
        """X.set_smart_object_runtime_slot_enabled(slot_handle, enabled) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_runtime_slot_enabled(slot_handle, enabled)

    @staticmethod
    def set_smart_object_runtime_tags(*, handle, scope, tags, replace=True):
        """X.set_smart_object_runtime_tags(handle, scope, tags, replace=True) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_runtime_tags(handle, scope, tags, replace)

    @staticmethod
    def set_smart_object_slot_property(*, asset_path, slot_id, property_path, value):
        """X.set_smart_object_slot_property(asset_path, slot_id, property_path, value) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_slot_property(asset_path, slot_id, property_path, value)

    @staticmethod
    def set_smart_object_tag_policies(*, asset_path, user_tags_filtering_policy, activity_tags_merging_policy):
        """X.set_smart_object_tag_policies(asset_path, user_tags_filtering_policy, activity_tags_merging_policy) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_tag_policies(asset_path, user_tags_filtering_policy, activity_tags_merging_policy)

    @staticmethod
    def set_smart_object_tag_query_json(*, asset_path, query_json, slot_id=""):
        """X.set_smart_object_tag_query_json(asset_path, query_json, slot_id="") -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_tag_query_json(asset_path, query_json, slot_id)

    @staticmethod
    def set_smart_object_tags(*, asset_path, tags, slot_id="", tag_set="Activity"):
        """X.set_smart_object_tags(asset_path, tags, slot_id="", tag_set="Activity") -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_tags(asset_path, tags, slot_id, tag_set)

    @staticmethod
    def set_smart_object_world_condition_expression(*, asset_path, slot_id, condition_index, operator, expression_depth, invert):
        """X.set_smart_object_world_condition_expression(asset_path, slot_id, condition_index, operator, expression_depth, invert) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_world_condition_expression(asset_path, slot_id, condition_index, operator, expression_depth, invert)

    @staticmethod
    def set_smart_object_world_condition_property(*, asset_path, slot_id, condition_index, property_path, value):
        """X.set_smart_object_world_condition_property(asset_path, slot_id, condition_index, property_path, value) -> bool"""
        return unreal.UnrealBridgeSmartObjectLibrary.set_smart_object_world_condition_property(asset_path, slot_id, condition_index, property_path, value)

    @staticmethod
    def validate_smart_object_definition(*, asset_path):
        """X.validate_smart_object_definition(asset_path) -> BridgeSmartObjectValidationResult"""
        return unreal.UnrealBridgeSmartObjectLibrary.validate_smart_object_definition(asset_path)

    @staticmethod
    def validate_smart_object_definition_entrances(*, asset_path, owner_transform, request, skip_actor_path=""):
        """X.validate_smart_object_definition_entrances(asset_path, owner_transform, request, skip_actor_path="") -> Array[BridgeSmartObjectEntranceResult]"""
        return unreal.UnrealBridgeSmartObjectLibrary.validate_smart_object_definition_entrances(asset_path, owner_transform, request, skip_actor_path)


class StateTree:
    """Wraps unreal.UnrealBridgeStateTreeLibrary (kwargs-only)."""

    @staticmethod
    def add_state_tree_binding(*, asset_path, source_id, source_path, target_id, target_path, output_binding=False):
        """X.add_state_tree_binding(asset_path, source_id, source_path, target_id, target_path, output_binding=False) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.add_state_tree_binding(asset_path, source_id, source_path, target_id, target_path, output_binding)

    @staticmethod
    def add_state_tree_node(*, asset_path, owner_id, scope, type_path, insert_index=-1):
        """X.add_state_tree_node(asset_path, owner_id, scope, type_path, insert_index=-1) -> str"""
        return unreal.UnrealBridgeStateTreeLibrary.add_state_tree_node(asset_path, owner_id, scope, type_path, insert_index)

    @staticmethod
    def add_state_tree_root_parameter(*, asset_path, name, type, default_value=""):
        """X.add_state_tree_root_parameter(asset_path, name, type, default_value="") -> str"""
        return unreal.UnrealBridgeStateTreeLibrary.add_state_tree_root_parameter(asset_path, name, type, default_value)

    @staticmethod
    def add_state_tree_state(*, asset_path, parent_state_id, name, state_type="State", insert_index=-1):
        """X.add_state_tree_state(asset_path, parent_state_id, name, state_type="State", insert_index=-1) -> str"""
        return unreal.UnrealBridgeStateTreeLibrary.add_state_tree_state(asset_path, parent_state_id, name, state_type, insert_index)

    @staticmethod
    def add_state_tree_transition(*, asset_path, state_id, trigger, target_type, target_state_id="", required_event_tag="", insert_index=-1):
        """X.add_state_tree_transition(asset_path, state_id, trigger, target_type, target_state_id="", required_event_tag="", insert_index=-1) -> str"""
        return unreal.UnrealBridgeStateTreeLibrary.add_state_tree_transition(asset_path, state_id, trigger, target_type, target_state_id, required_event_tag, insert_index)

    @staticmethod
    def clear_state_tree_bindings_for_item(*, asset_path, item_id):
        """X.clear_state_tree_bindings_for_item(asset_path, item_id) -> int32"""
        return unreal.UnrealBridgeStateTreeLibrary.clear_state_tree_bindings_for_item(asset_path, item_id)

    @staticmethod
    def clear_state_tree_breakpoints(*, asset_path):
        """X.clear_state_tree_breakpoints(asset_path) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.clear_state_tree_breakpoints(asset_path)

    @staticmethod
    def compile_state_tree(*, asset_path, run_validation=True):
        """X.compile_state_tree(asset_path, run_validation=True) -> BridgeStateTreeCompileResult"""
        return unreal.UnrealBridgeStateTreeLibrary.compile_state_tree(asset_path, run_validation)

    @staticmethod
    def control_state_tree_component(*, component_path, action, reason="UnrealBridge"):
        """X.control_state_tree_component(component_path, action, reason="UnrealBridge") -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.control_state_tree_component(component_path, action, reason)

    @staticmethod
    def create_state_tree(*, asset_path, schema_class_path):
        """X.create_state_tree(asset_path, schema_class_path) -> BridgeStateTreeCreateResult"""
        return unreal.UnrealBridgeStateTreeLibrary.create_state_tree(asset_path, schema_class_path)

    @staticmethod
    def get_last_state_tree_error():
        """X.get_last_state_tree_error() -> str"""
        return unreal.UnrealBridgeStateTreeLibrary.get_last_state_tree_error()

    @staticmethod
    def get_state_tree_component_info(*, component_path):
        """X.get_state_tree_component_info(component_path) -> BridgeStateTreeComponentInfo"""
        return unreal.UnrealBridgeStateTreeLibrary.get_state_tree_component_info(component_path)

    @staticmethod
    def get_state_tree_info(*, asset_path):
        """X.get_state_tree_info(asset_path) -> BridgeStateTreeAssetInfo"""
        return unreal.UnrealBridgeStateTreeLibrary.get_state_tree_info(asset_path)

    @staticmethod
    def get_state_tree_node_property(*, asset_path, node_id, data_source, property_path):
        """X.get_state_tree_node_property(asset_path, node_id, data_source, property_path) -> BridgeStateTreePropertyResult"""
        return unreal.UnrealBridgeStateTreeLibrary.get_state_tree_node_property(asset_path, node_id, data_source, property_path)

    @staticmethod
    def get_state_tree_state_property(*, asset_path, state_id, property_path):
        """X.get_state_tree_state_property(asset_path, state_id, property_path) -> BridgeStateTreePropertyResult"""
        return unreal.UnrealBridgeStateTreeLibrary.get_state_tree_state_property(asset_path, state_id, property_path)

    @staticmethod
    def get_state_tree_transition_property(*, asset_path, transition_id, property_path):
        """X.get_state_tree_transition_property(asset_path, transition_id, property_path) -> BridgeStateTreePropertyResult"""
        return unreal.UnrealBridgeStateTreeLibrary.get_state_tree_transition_property(asset_path, transition_id, property_path)

    @staticmethod
    def is_state_tree_api_available():
        """X.is_state_tree_api_available() -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.is_state_tree_api_available()

    @staticmethod
    def list_state_tree_bindable_structs(*, asset_path, target_id=""):
        """X.list_state_tree_bindable_structs(asset_path, target_id="") -> Array[BridgeStateTreeBindableStructInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_bindable_structs(asset_path, target_id)

    @staticmethod
    def list_state_tree_bindings(*, asset_path):
        """X.list_state_tree_bindings(asset_path) -> Array[BridgeStateTreeBindingInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_bindings(asset_path)

    @staticmethod
    def list_state_tree_breakpoints(*, asset_path):
        """X.list_state_tree_breakpoints(asset_path) -> Array[BridgeStateTreeBreakpointInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_breakpoints(asset_path)

    @staticmethod
    def list_state_tree_components(*, pie_only=False):
        """X.list_state_tree_components(pie_only=False) -> Array[BridgeStateTreeComponentInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_components(pie_only)

    @staticmethod
    def list_state_tree_node_properties(*, asset_path, node_id, data_source="Instance", include_inherited=True):
        """X.list_state_tree_node_properties(asset_path, node_id, data_source="Instance", include_inherited=True) -> Array[BridgeStateTreePropertyInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_node_properties(asset_path, node_id, data_source, include_inherited)

    @staticmethod
    def list_state_tree_node_types(*, asset_path, kind="", include_disallowed=False):
        """X.list_state_tree_node_types(asset_path, kind="", include_disallowed=False) -> Array[BridgeStateTreeNodeTypeInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_node_types(asset_path, kind, include_disallowed)

    @staticmethod
    def list_state_tree_nodes(*, asset_path, scope=""):
        """X.list_state_tree_nodes(asset_path, scope="") -> Array[BridgeStateTreeNodeInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_nodes(asset_path, scope)

    @staticmethod
    def list_state_tree_parameters(*, asset_path, scope_id=""):
        """X.list_state_tree_parameters(asset_path, scope_id="") -> Array[BridgeStateTreeParameterInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_parameters(asset_path, scope_id)

    @staticmethod
    def list_state_tree_states(*, asset_path):
        """X.list_state_tree_states(asset_path) -> Array[BridgeStateTreeStateInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_states(asset_path)

    @staticmethod
    def list_state_tree_transitions(*, asset_path, state_id=""):
        """X.list_state_tree_transitions(asset_path, state_id="") -> Array[BridgeStateTreeTransitionInfo]"""
        return unreal.UnrealBridgeStateTreeLibrary.list_state_tree_transitions(asset_path, state_id)

    @staticmethod
    def move_state_tree_node(*, asset_path, node_id, new_index):
        """X.move_state_tree_node(asset_path, node_id, new_index) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.move_state_tree_node(asset_path, node_id, new_index)

    @staticmethod
    def move_state_tree_state(*, asset_path, state_id, new_parent_state_id, insert_index=-1):
        """X.move_state_tree_state(asset_path, state_id, new_parent_state_id, insert_index=-1) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.move_state_tree_state(asset_path, state_id, new_parent_state_id, insert_index)

    @staticmethod
    def move_state_tree_transition(*, asset_path, transition_id, new_index):
        """X.move_state_tree_transition(asset_path, transition_id, new_index) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.move_state_tree_transition(asset_path, transition_id, new_index)

    @staticmethod
    def remove_state_tree_binding(*, asset_path, target_id, target_path):
        """X.remove_state_tree_binding(asset_path, target_id, target_path) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.remove_state_tree_binding(asset_path, target_id, target_path)

    @staticmethod
    def remove_state_tree_node(*, asset_path, node_id):
        """X.remove_state_tree_node(asset_path, node_id) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.remove_state_tree_node(asset_path, node_id)

    @staticmethod
    def remove_state_tree_root_parameter(*, asset_path, name):
        """X.remove_state_tree_root_parameter(asset_path, name) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.remove_state_tree_root_parameter(asset_path, name)

    @staticmethod
    def remove_state_tree_state(*, asset_path, state_id):
        """X.remove_state_tree_state(asset_path, state_id) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.remove_state_tree_state(asset_path, state_id)

    @staticmethod
    def remove_state_tree_transition(*, asset_path, transition_id):
        """X.remove_state_tree_transition(asset_path, transition_id) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.remove_state_tree_transition(asset_path, transition_id)

    @staticmethod
    def rename_state_tree_root_parameter(*, asset_path, old_name, new_name):
        """X.rename_state_tree_root_parameter(asset_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.rename_state_tree_root_parameter(asset_path, old_name, new_name)

    @staticmethod
    def send_state_tree_component_event(*, component_path, event_tag, origin="UnrealBridge"):
        """X.send_state_tree_component_event(component_path, event_tag, origin="UnrealBridge") -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.send_state_tree_component_event(component_path, event_tag, origin)

    @staticmethod
    def set_state_tree_breakpoint(*, asset_path, item_id, breakpoint_type, enabled):
        """X.set_state_tree_breakpoint(asset_path, item_id, breakpoint_type, enabled) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_breakpoint(asset_path, item_id, breakpoint_type, enabled)

    @staticmethod
    def set_state_tree_component_asset(*, component_path, asset_path):
        """X.set_state_tree_component_asset(component_path, asset_path) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_component_asset(component_path, asset_path)

    @staticmethod
    def set_state_tree_linked_asset(*, asset_path, state_id, linked_asset_path):
        """X.set_state_tree_linked_asset(asset_path, state_id, linked_asset_path) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_linked_asset(asset_path, state_id, linked_asset_path)

    @staticmethod
    def set_state_tree_linked_state(*, asset_path, state_id, linked_state_id):
        """X.set_state_tree_linked_state(asset_path, state_id, linked_state_id) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_linked_state(asset_path, state_id, linked_state_id)

    @staticmethod
    def set_state_tree_node_property(*, asset_path, node_id, data_source, property_path, value):
        """X.set_state_tree_node_property(asset_path, node_id, data_source, property_path, value) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_node_property(asset_path, node_id, data_source, property_path, value)

    @staticmethod
    def set_state_tree_parameter_value(*, asset_path, scope_id, name, value, mark_overridden=True):
        """X.set_state_tree_parameter_value(asset_path, scope_id, name, value, mark_overridden=True) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_parameter_value(asset_path, scope_id, name, value, mark_overridden)

    @staticmethod
    def set_state_tree_state_property(*, asset_path, state_id, property_path, value):
        """X.set_state_tree_state_property(asset_path, state_id, property_path, value) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_state_property(asset_path, state_id, property_path, value)

    @staticmethod
    def set_state_tree_state_type(*, asset_path, state_id, state_type):
        """X.set_state_tree_state_type(asset_path, state_id, state_type) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_state_type(asset_path, state_id, state_type)

    @staticmethod
    def set_state_tree_transition_property(*, asset_path, transition_id, property_path, value):
        """X.set_state_tree_transition_property(asset_path, transition_id, property_path, value) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_transition_property(asset_path, transition_id, property_path, value)

    @staticmethod
    def set_state_tree_transition_target(*, asset_path, transition_id, target_type, target_state_id=""):
        """X.set_state_tree_transition_target(asset_path, transition_id, target_type, target_state_id="") -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.set_state_tree_transition_target(asset_path, transition_id, target_type, target_state_id)

    @staticmethod
    def validate_state_tree(*, asset_path):
        """X.validate_state_tree(asset_path) -> bool"""
        return unreal.UnrealBridgeStateTreeLibrary.validate_state_tree(asset_path)


class Struct:
    """Wraps unreal.UnrealBridgeStructLibrary (kwargs-only)."""

    @staticmethod
    def add_struct_variable(*, struct_path, name, type_string, default_value):
        """X.add_struct_variable(struct_path, name, type_string, default_value) -> bool"""
        return unreal.UnrealBridgeStructLibrary.add_struct_variable(struct_path, name, type_string, default_value)

    @staticmethod
    def change_struct_variable_type(*, struct_path, name, new_type_string):
        """X.change_struct_variable_type(struct_path, name, new_type_string) -> bool"""
        return unreal.UnrealBridgeStructLibrary.change_struct_variable_type(struct_path, name, new_type_string)

    @staticmethod
    def create_user_defined_struct(*, asset_path):
        """X.create_user_defined_struct(asset_path) -> BridgeStructCreateResult"""
        return unreal.UnrealBridgeStructLibrary.create_user_defined_struct(asset_path)

    @staticmethod
    def get_struct_info(*, struct_path):
        """X.get_struct_info(struct_path) -> BridgeStructInfo"""
        return unreal.UnrealBridgeStructLibrary.get_struct_info(struct_path)

    @staticmethod
    def get_struct_variables(*, struct_path):
        """X.get_struct_variables(struct_path) -> Array[BridgeStructVariableInfo]"""
        return unreal.UnrealBridgeStructLibrary.get_struct_variables(struct_path)

    @staticmethod
    def move_struct_variable(*, struct_path, name, new_index):
        """X.move_struct_variable(struct_path, name, new_index) -> bool"""
        return unreal.UnrealBridgeStructLibrary.move_struct_variable(struct_path, name, new_index)

    @staticmethod
    def remove_struct_variable(*, struct_path, name):
        """X.remove_struct_variable(struct_path, name) -> bool"""
        return unreal.UnrealBridgeStructLibrary.remove_struct_variable(struct_path, name)

    @staticmethod
    def rename_struct_variable(*, struct_path, old_name, new_name):
        """X.rename_struct_variable(struct_path, old_name, new_name) -> bool"""
        return unreal.UnrealBridgeStructLibrary.rename_struct_variable(struct_path, old_name, new_name)

    @staticmethod
    def set_struct_tooltip(*, struct_path, tooltip):
        """X.set_struct_tooltip(struct_path, tooltip) -> bool"""
        return unreal.UnrealBridgeStructLibrary.set_struct_tooltip(struct_path, tooltip)

    @staticmethod
    def set_struct_variable_default(*, struct_path, name, default_value):
        """X.set_struct_variable_default(struct_path, name, default_value) -> bool"""
        return unreal.UnrealBridgeStructLibrary.set_struct_variable_default(struct_path, name, default_value)

    @staticmethod
    def set_struct_variable_edit_on_instance(*, struct_path, name, edit_on_instance):
        """X.set_struct_variable_edit_on_instance(struct_path, name, edit_on_instance) -> bool"""
        return unreal.UnrealBridgeStructLibrary.set_struct_variable_edit_on_instance(struct_path, name, edit_on_instance)

    @staticmethod
    def set_struct_variable_tooltip(*, struct_path, name, tooltip="── Metadata ───────────────────────────────────────────────"):
        """X.set_struct_variable_tooltip(struct_path, name, tooltip="── Metadata ───────────────────────────────────────────────") -> bool"""
        return unreal.UnrealBridgeStructLibrary.set_struct_variable_tooltip(struct_path, name, tooltip)


class UMG:
    """Wraps unreal.UnrealBridgeUMGLibrary (kwargs-only)."""

    @staticmethod
    def add_mvvm_binding(*, widget_blueprint_path, view_model_name, source_field_path, destination_widget_name, destination_field_path, mode):
        """X.add_mvvm_binding(widget_blueprint_path, view_model_name, source_field_path, destination_widget_name, destination_field_path, mode) -> str"""
        return unreal.UnrealBridgeUMGLibrary.add_mvvm_binding(widget_blueprint_path, view_model_name, source_field_path, destination_widget_name, destination_field_path, mode)

    @staticmethod
    def add_mvvm_view_model(*, widget_blueprint_path, view_model_name, view_model_class_path, creation_type, creation_data, optional, create_getter, create_setter):
        """X.add_mvvm_view_model(widget_blueprint_path, view_model_name, view_model_class_path, creation_type, creation_data, optional, create_getter, create_setter) -> str"""
        return unreal.UnrealBridgeUMGLibrary.add_mvvm_view_model(widget_blueprint_path, view_model_name, view_model_class_path, creation_type, creation_data, optional, create_getter, create_setter)

    @staticmethod
    def add_widget(*, widget_blueprint_path, widget_class_path, widget_name, parent_name, insert_index):
        """X.add_widget(widget_blueprint_path, widget_class_path, widget_name, parent_name, insert_index) -> BridgeWidgetOperationResult"""
        return unreal.UnrealBridgeUMGLibrary.add_widget(widget_blueprint_path, widget_class_path, widget_name, parent_name, insert_index)

    @staticmethod
    def add_widget_animation_color_keys(*, widget_blueprint_path, animation_name, widget_name, property_name, keys, interpolation):
        """X.add_widget_animation_color_keys(widget_blueprint_path, animation_name, widget_name, property_name, keys, interpolation) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.add_widget_animation_color_keys(widget_blueprint_path, animation_name, widget_name, property_name, keys, interpolation)

    @staticmethod
    def add_widget_animation_float_keys(*, widget_blueprint_path, animation_name, widget_name, property_name, keys, interpolation):
        """X.add_widget_animation_float_keys(widget_blueprint_path, animation_name, widget_name, property_name, keys, interpolation) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.add_widget_animation_float_keys(widget_blueprint_path, animation_name, widget_name, property_name, keys, interpolation)

    @staticmethod
    def add_widget_animation_transform_keys(*, widget_blueprint_path, animation_name, widget_name, keys, interpolation):
        """X.add_widget_animation_transform_keys(widget_blueprint_path, animation_name, widget_name, keys, interpolation) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.add_widget_animation_transform_keys(widget_blueprint_path, animation_name, widget_name, keys, interpolation)

    @staticmethod
    def click_live_button(*, instance_handle, widget_name):
        """X.click_live_button(instance_handle, widget_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.click_live_button(instance_handle, widget_name)

    @staticmethod
    def compile_and_validate_widget_blueprint(*, widget_blueprint_path, save, check_accessibility):
        """X.compile_and_validate_widget_blueprint(widget_blueprint_path, save, check_accessibility) -> BridgeWidgetValidationReport"""
        return unreal.UnrealBridgeUMGLibrary.compile_and_validate_widget_blueprint(widget_blueprint_path, save, check_accessibility)

    @staticmethod
    def create_mvvm_view_model_blueprint(*, asset_path, parent_class_path):
        """X.create_mvvm_view_model_blueprint(asset_path, parent_class_path) -> BridgeWidgetOperationResult"""
        return unreal.UnrealBridgeUMGLibrary.create_mvvm_view_model_blueprint(asset_path, parent_class_path)

    @staticmethod
    def create_widget_animation(*, widget_blueprint_path, animation_name, duration_seconds, display_rate):
        """X.create_widget_animation(widget_blueprint_path, animation_name, duration_seconds, display_rate) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.create_widget_animation(widget_blueprint_path, animation_name, duration_seconds, display_rate)

    @staticmethod
    def create_widget_blueprint(*, asset_path, parent_class_path):
        """X.create_widget_blueprint(asset_path, parent_class_path) -> BridgeWidgetOperationResult"""
        return unreal.UnrealBridgeUMGLibrary.create_widget_blueprint(asset_path, parent_class_path)

    @staticmethod
    def focus_live_widget(*, instance_handle, widget_name):
        """X.focus_live_widget(instance_handle, widget_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.focus_live_widget(instance_handle, widget_name)

    @staticmethod
    def get_live_view_model_property(*, instance_handle, view_model_name, property_path):
        """X.get_live_view_model_property(instance_handle, view_model_name, property_path) -> str"""
        return unreal.UnrealBridgeUMGLibrary.get_live_view_model_property(instance_handle, view_model_name, property_path)

    @staticmethod
    def get_live_widget_property(*, instance_handle, widget_name, property_path):
        """X.get_live_widget_property(instance_handle, widget_name, property_path) -> str"""
        return unreal.UnrealBridgeUMGLibrary.get_live_widget_property(instance_handle, widget_name, property_path)

    @staticmethod
    def get_live_widget_tree(*, instance_handle):
        """X.get_live_widget_tree(instance_handle) -> Array[BridgeLiveWidgetInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_live_widget_tree(instance_handle)

    @staticmethod
    def get_mvvm_bindings(*, widget_blueprint_path):
        """X.get_mvvm_bindings(widget_blueprint_path) -> Array[BridgeMVVMBindingInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_mvvm_bindings(widget_blueprint_path)

    @staticmethod
    def get_mvvm_view_models(*, widget_blueprint_path):
        """X.get_mvvm_view_models(widget_blueprint_path) -> Array[BridgeMVVMViewModelInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_mvvm_view_models(widget_blueprint_path)

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
    def get_widget_slot_properties(*, widget_blueprint_path, widget_name):
        """X.get_widget_slot_properties(widget_blueprint_path, widget_name) -> Array[BridgeWidgetPropertyValue]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_slot_properties(widget_blueprint_path, widget_name)

    @staticmethod
    def get_widget_tree(*, widget_blueprint_path):
        """X.get_widget_tree(widget_blueprint_path) -> Array[BridgeWidgetInfo]"""
        return unreal.UnrealBridgeUMGLibrary.get_widget_tree(widget_blueprint_path)

    @staticmethod
    def list_widget_classes(*, query, include_abstract, max_results):
        """X.list_widget_classes(query, include_abstract, max_results) -> Array[BridgeWidgetClassInfo]"""
        return unreal.UnrealBridgeUMGLibrary.list_widget_classes(query, include_abstract, max_results)

    @staticmethod
    def play_live_widget_animation(*, instance_handle, animation_name, start_time, num_loops, play_mode, playback_speed):
        """X.play_live_widget_animation(instance_handle, animation_name, start_time, num_loops, play_mode, playback_speed) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.play_live_widget_animation(instance_handle, animation_name, start_time, num_loops, play_mode, playback_speed)

    @staticmethod
    def remove_all_widget_instances():
        """X.remove_all_widget_instances() -> int32"""
        return unreal.UnrealBridgeUMGLibrary.remove_all_widget_instances()

    @staticmethod
    def remove_mvvm_binding(*, widget_blueprint_path, binding_id):
        """X.remove_mvvm_binding(widget_blueprint_path, binding_id) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.remove_mvvm_binding(widget_blueprint_path, binding_id)

    @staticmethod
    def remove_mvvm_view_model(*, widget_blueprint_path, view_model_name):
        """X.remove_mvvm_view_model(widget_blueprint_path, view_model_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.remove_mvvm_view_model(widget_blueprint_path, view_model_name)

    @staticmethod
    def remove_widget(*, widget_blueprint_path, widget_name):
        """X.remove_widget(widget_blueprint_path, widget_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.remove_widget(widget_blueprint_path, widget_name)

    @staticmethod
    def remove_widget_animation(*, widget_blueprint_path, animation_name):
        """X.remove_widget_animation(widget_blueprint_path, animation_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.remove_widget_animation(widget_blueprint_path, animation_name)

    @staticmethod
    def remove_widget_animation_track(*, widget_blueprint_path, animation_name, widget_name, property_name):
        """X.remove_widget_animation_track(widget_blueprint_path, animation_name, widget_name, property_name) -> int32"""
        return unreal.UnrealBridgeUMGLibrary.remove_widget_animation_track(widget_blueprint_path, animation_name, widget_name, property_name)

    @staticmethod
    def remove_widget_instance(*, instance_handle):
        """X.remove_widget_instance(instance_handle) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.remove_widget_instance(instance_handle)

    @staticmethod
    def rename_widget(*, widget_blueprint_path, widget_name, new_name):
        """X.rename_widget(widget_blueprint_path, widget_name, new_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.rename_widget(widget_blueprint_path, widget_name, new_name)

    @staticmethod
    def reparent_widget(*, widget_blueprint_path, widget_name, new_parent_name, insert_index):
        """X.reparent_widget(widget_blueprint_path, widget_name, new_parent_name, insert_index) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.reparent_widget(widget_blueprint_path, widget_name, new_parent_name, insert_index)

    @staticmethod
    def search_widgets(*, widget_blueprint_path, query):
        """X.search_widgets(widget_blueprint_path, query) -> Array[BridgeWidgetInfo]"""
        return unreal.UnrealBridgeUMGLibrary.search_widgets(widget_blueprint_path, query)

    @staticmethod
    def set_canvas_slot_layout(*, widget_blueprint_path, widget_name, position, size, anchor_minimum, anchor_maximum, alignment, auto_size, z_order):
        """X.set_canvas_slot_layout(widget_blueprint_path, widget_name, position, size, anchor_minimum, anchor_maximum, alignment, auto_size, z_order) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_canvas_slot_layout(widget_blueprint_path, widget_name, position, size, anchor_minimum, anchor_maximum, alignment, auto_size, z_order)

    @staticmethod
    def set_live_view_model_property(*, instance_handle, view_model_name, property_path, value):
        """X.set_live_view_model_property(instance_handle, view_model_name, property_path, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_view_model_property(instance_handle, view_model_name, property_path, value)

    @staticmethod
    def set_live_widget_checked(*, instance_handle, widget_name, checked):
        """X.set_live_widget_checked(instance_handle, widget_name, checked) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_widget_checked(instance_handle, widget_name, checked)

    @staticmethod
    def set_live_widget_material_scalar(*, instance_handle, widget_name, parameter_name, value):
        """X.set_live_widget_material_scalar(instance_handle, widget_name, parameter_name, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_widget_material_scalar(instance_handle, widget_name, parameter_name, value)

    @staticmethod
    def set_live_widget_material_vector(*, instance_handle, widget_name, parameter_name, value):
        """X.set_live_widget_material_vector(instance_handle, widget_name, parameter_name, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_widget_material_vector(instance_handle, widget_name, parameter_name, value)

    @staticmethod
    def set_live_widget_property(*, instance_handle, widget_name, property_path, value):
        """X.set_live_widget_property(instance_handle, widget_name, property_path, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_widget_property(instance_handle, widget_name, property_path, value)

    @staticmethod
    def set_live_widget_text(*, instance_handle, widget_name, text):
        """X.set_live_widget_text(instance_handle, widget_name, text) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_widget_text(instance_handle, widget_name, text)

    @staticmethod
    def set_live_widget_value(*, instance_handle, widget_name, value):
        """X.set_live_widget_value(instance_handle, widget_name, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_live_widget_value(instance_handle, widget_name, value)

    @staticmethod
    def set_mvvm_view_settings(*, widget_blueprint_path, initialize_sources_on_construct, initialize_bindings_on_construct, initialize_events_on_construct, create_view_without_bindings):
        """X.set_mvvm_view_settings(widget_blueprint_path, initialize_sources_on_construct, initialize_bindings_on_construct, initialize_events_on_construct, create_view_without_bindings) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_mvvm_view_settings(widget_blueprint_path, initialize_sources_on_construct, initialize_bindings_on_construct, initialize_events_on_construct, create_view_without_bindings)

    @staticmethod
    def set_view_model_field_notify(*, view_model_blueprint_path, variable_name, enabled):
        """X.set_view_model_field_notify(view_model_blueprint_path, variable_name, enabled) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_view_model_field_notify(view_model_blueprint_path, variable_name, enabled)

    @staticmethod
    def set_widget_brush(*, widget_blueprint_path, widget_name, brush_property_path, resource_path, tint, draw_as, image_size, margin):
        """X.set_widget_brush(widget_blueprint_path, widget_name, brush_property_path, resource_path, tint, draw_as, image_size, margin) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_widget_brush(widget_blueprint_path, widget_name, brush_property_path, resource_path, tint, draw_as, image_size, margin)

    @staticmethod
    def set_widget_is_variable(*, widget_blueprint_path, widget_name, is_variable):
        """X.set_widget_is_variable(widget_blueprint_path, widget_name, is_variable) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_widget_is_variable(widget_blueprint_path, widget_name, is_variable)

    @staticmethod
    def set_widget_property(*, widget_blueprint_path, widget_name, property_name, value):
        """X.set_widget_property(widget_blueprint_path, widget_name, property_name, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_widget_property(widget_blueprint_path, widget_name, property_name, value)

    @staticmethod
    def set_widget_slot_property(*, widget_blueprint_path, widget_name, property_name, value):
        """X.set_widget_slot_property(widget_blueprint_path, widget_name, property_name, value) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.set_widget_slot_property(widget_blueprint_path, widget_name, property_name, value)

    @staticmethod
    def spawn_widget_instance(*, widget_blueprint_path, z_order):
        """X.spawn_widget_instance(widget_blueprint_path, z_order) -> str"""
        return unreal.UnrealBridgeUMGLibrary.spawn_widget_instance(widget_blueprint_path, z_order)

    @staticmethod
    def stop_live_widget_animation(*, instance_handle, animation_name):
        """X.stop_live_widget_animation(instance_handle, animation_name) -> bool"""
        return unreal.UnrealBridgeUMGLibrary.stop_live_widget_animation(instance_handle, animation_name)


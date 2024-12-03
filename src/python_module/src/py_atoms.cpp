// SPDX-License-Identifier: Apache-2.0
#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wattributes"
#endif

// #include <caf/all.hpp>
// #include <caf/config.hpp>
// #include <caf/io/all.hpp>

#include "xstudio/atoms.hpp"
#include "py_opaque.hpp"
#include "py_config.hpp"

namespace caf::python {

namespace py = pybind11;
using namespace xstudio::bookmark;
using namespace xstudio::broadcast;
using namespace xstudio::colour_pipeline;
using namespace xstudio::data_source;
using namespace xstudio::global;
using namespace xstudio::global_store;
using namespace xstudio::history;
using namespace xstudio::json_store;
using namespace xstudio::media;
using namespace xstudio::media_cache;
using namespace xstudio::media_hook;
using namespace xstudio::media_metadata;
using namespace xstudio::media_reader;
using namespace xstudio::module;
using namespace xstudio::playhead;
using namespace xstudio::playlist;
using namespace xstudio::plugin_manager;
using namespace xstudio::session;
using namespace xstudio::sync;
using namespace xstudio::thumbnail;
using namespace xstudio::timeline;
using namespace xstudio::ui;
using namespace xstudio::ui::keypress_monitor;
using namespace xstudio::ui::qml;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui::model_data;
using namespace xstudio::utility;
using namespace xstudio;

#define ADD_ATOM(atom_namespace, atom_name)                                                    \
    add_message_type<atom_name>(                                                               \
        #atom_name,                                                                            \
        #atom_namespace "::" #atom_name,                                                       \
        [](py::module &m, const std::string &name) {                                           \
            py::class_<atom_name>(m, name.c_str()).def(py::init<>());                          \
        })

void py_config::add_atoms() {
    ADD_ATOM(xstudio::ui::qml, backend_atom);
    ADD_ATOM(xstudio::broadcast, join_broadcast_atom);
    ADD_ATOM(xstudio::broadcast, leave_broadcast_atom);
    ADD_ATOM(xstudio::broadcast, broadcast_down_atom);
    ADD_ATOM(xstudio::sync, authorise_connection_atom);
    ADD_ATOM(xstudio::sync, get_sync_atom);
    ADD_ATOM(xstudio::sync, request_connection_atom);
    ADD_ATOM(xstudio::media_hook, get_media_hook_atom);
    ADD_ATOM(xstudio::media_hook, gather_media_sources_atom);
    ADD_ATOM(xstudio::media_metadata, get_metadata_atom);


    ADD_ATOM(xstudio::timeline, active_range_atom);
    ADD_ATOM(xstudio::timeline, available_range_atom);
    ADD_ATOM(xstudio::timeline, bake_atom);
    ADD_ATOM(xstudio::timeline, duration_atom);
    ADD_ATOM(xstudio::timeline, erase_item_at_frame_atom);
    ADD_ATOM(xstudio::timeline, erase_item_atom);
    ADD_ATOM(xstudio::timeline, insert_item_at_frame_atom);
    ADD_ATOM(xstudio::timeline, insert_item_atom);
    ADD_ATOM(xstudio::timeline, item_atom);
    ADD_ATOM(xstudio::timeline, item_name_atom);
    ADD_ATOM(xstudio::timeline, item_lock_atom);
    ADD_ATOM(xstudio::timeline, item_flag_atom);
    ADD_ATOM(xstudio::timeline, item_marker_atom);
    ADD_ATOM(xstudio::timeline, item_prop_atom);
    ADD_ATOM(xstudio::timeline, focus_atom);
    ADD_ATOM(xstudio::timeline, move_item_atom);
    ADD_ATOM(xstudio::timeline, move_item_at_frame_atom);
    ADD_ATOM(xstudio::timeline, remove_item_at_frame_atom);
    ADD_ATOM(xstudio::timeline, remove_item_atom);
    ADD_ATOM(xstudio::timeline, split_item_atom);
    ADD_ATOM(xstudio::timeline, split_item_at_frame_atom);
    ADD_ATOM(xstudio::timeline, trimmed_range_atom);

    ADD_ATOM(xstudio::thumbnail, cache_path_atom);
    ADD_ATOM(xstudio::thumbnail, cache_stats_atom);
    // ADD_ATOM(xstudio::embedded_python, python_exec_atom);
    // ADD_ATOM(xstudio::embedded_python, python_eval_file_atom);
    // ADD_ATOM(xstudio::embedded_python, python_eval_atom);
    // ADD_ATOM(xstudio::embedded_python, python_eval_locals_atom);
    // ADD_ATOM(xstudio::embedded_python, python_output_atom);
    // ADD_ATOM(xstudio::embedded_python, python_create_session_atom);
    // ADD_ATOM(xstudio::embedded_python, python_remove_session_atom);
    // ADD_ATOM(xstudio::embedded_python, python_session_input_atom);
    // ADD_ATOM(xstudio::embedded_python, python_session_interrupt_atom);
    ADD_ATOM(xstudio::playlist, add_media_atom);
    ADD_ATOM(xstudio::playlist, convert_to_contact_sheet_atom);
    ADD_ATOM(xstudio::playlist, convert_to_timeline_atom);
    ADD_ATOM(xstudio::playlist, convert_to_subset_atom);
    ADD_ATOM(xstudio::playlist, copy_container_atom);
    ADD_ATOM(xstudio::playlist, copy_container_to_atom);
    ADD_ATOM(xstudio::playlist, create_contact_sheet_atom);
    ADD_ATOM(xstudio::playlist, create_container_atom);
    ADD_ATOM(xstudio::playlist, create_divider_atom);
    ADD_ATOM(xstudio::playlist, create_group_atom);
    ADD_ATOM(xstudio::playlist, create_playhead_atom);
    ADD_ATOM(xstudio::playlist, create_subset_atom);
    ADD_ATOM(xstudio::playlist, create_timeline_atom);
    ADD_ATOM(xstudio::playlist, duplicate_container_atom);
    ADD_ATOM(xstudio::playlist, expanded_atom);
    ADD_ATOM(xstudio::playlist, get_change_event_group_atom);
    ADD_ATOM(xstudio::playlist, get_container_atom);
    ADD_ATOM(xstudio::playlist, get_media_atom);
    ADD_ATOM(xstudio::playlist, get_media_uuid_atom);
    ADD_ATOM(xstudio::playlist, get_next_media_atom);
    ADD_ATOM(xstudio::playlist, get_playhead_atom);
    ADD_ATOM(xstudio::playlist, insert_container_atom);
    ADD_ATOM(xstudio::playlist, loading_media_atom);
    ADD_ATOM(xstudio::playlist, media_content_changed_atom);
    ADD_ATOM(xstudio::playlist, move_container_atom);
    ADD_ATOM(xstudio::playlist, move_container_to_atom);
    ADD_ATOM(xstudio::playlist, move_media_atom);
    ADD_ATOM(xstudio::playlist, copy_media_atom);
    ADD_ATOM(xstudio::playlist, reflag_container_atom);
    ADD_ATOM(xstudio::playlist, remove_container_atom);
    ADD_ATOM(xstudio::playlist, remove_media_atom);
    ADD_ATOM(xstudio::playlist, remove_orphans_atom);
    ADD_ATOM(xstudio::playlist, rename_container_atom);
    ADD_ATOM(xstudio::playlist, selection_actor_atom);
    ADD_ATOM(xstudio::playlist, select_all_media_atom);
    ADD_ATOM(xstudio::playlist, select_media_atom);
    ADD_ATOM(xstudio::playlist, set_playlist_in_viewer_atom);
    ADD_ATOM(xstudio::playlist, sort_by_media_display_info_atom);
    ADD_ATOM(xstudio::session, session_atom);
    ADD_ATOM(xstudio::session, session_request_atom);
    ADD_ATOM(xstudio::session, add_playlist_atom);
    ADD_ATOM(xstudio::session, get_playlist_atom);
    ADD_ATOM(xstudio::session, active_media_container_atom);
    ADD_ATOM(xstudio::session, viewport_active_media_container_atom);
    ADD_ATOM(xstudio::session, get_playlists_atom);
    ADD_ATOM(xstudio::session, media_rate_atom);
    ADD_ATOM(xstudio::session, load_uris_atom);
    ADD_ATOM(xstudio::session, merge_playlist_atom);
    ADD_ATOM(xstudio::session, merge_session_atom);
    ADD_ATOM(xstudio::session, path_atom);
    ADD_ATOM(xstudio::session, remove_serialise_target_atom);
    ADD_ATOM(xstudio::session, export_atom);
    ADD_ATOM(xstudio::session, import_atom);
    ADD_ATOM(xstudio::media_reader, clear_precache_queue_atom);
    ADD_ATOM(xstudio::media_reader, get_image_atom);
    ADD_ATOM(xstudio::media_reader, get_thumbnail_atom);
    ADD_ATOM(xstudio::media_reader, process_thumbnail_atom);
    ADD_ATOM(xstudio::media_reader, get_media_detail_atom);
    ADD_ATOM(xstudio::media_reader, precache_audio_atom);
    ADD_ATOM(xstudio::media_reader, playback_precache_atom);
    ADD_ATOM(xstudio::media_reader, read_precache_image_atom);
    ADD_ATOM(xstudio::media_reader, do_precache_work_atom);
    ADD_ATOM(xstudio::media_reader, get_reader_atom);
    ADD_ATOM(xstudio::media_reader, push_image_atom);
    ADD_ATOM(xstudio::media_reader, retire_readers_atom);
    ADD_ATOM(xstudio::media_reader, supported_atom);
    ADD_ATOM(xstudio::media_cache, count_atom);
    ADD_ATOM(xstudio::media_cache, erase_atom);
    ADD_ATOM(xstudio::media_cache, keys_atom);
    ADD_ATOM(xstudio::media_cache, preserve_atom);
    ADD_ATOM(xstudio::media_cache, unpreserve_atom);
    ADD_ATOM(xstudio::media_cache, retrieve_atom);
    ADD_ATOM(xstudio::media_cache, size_atom);
    ADD_ATOM(xstudio::media_cache, store_atom);
    ADD_ATOM(xstudio::colour_pipeline, colour_pipeline_atom);
    ADD_ATOM(xstudio::colour_pipeline, get_colour_pipe_data_atom);
    ADD_ATOM(xstudio::colour_pipeline, get_colour_pipe_params_atom);

    ADD_ATOM(xstudio::module, add_attribute_atom);
    ADD_ATOM(xstudio::module, attribute_role_data_atom);
    ADD_ATOM(xstudio::module, attribute_value_atom);
    ADD_ATOM(xstudio::module, module_ui_events_group_atom);
    ADD_ATOM(xstudio::module, change_attribute_request_atom);
    ADD_ATOM(xstudio::module, change_attribute_value_atom);
    ADD_ATOM(xstudio::module, attribute_deleted_event_atom);
    ADD_ATOM(xstudio::module, change_attribute_event_atom);
    ADD_ATOM(xstudio::module, redraw_viewport_group_atom);
    ADD_ATOM(xstudio::module, connect_to_ui_atom);
    ADD_ATOM(xstudio::module, deserialise_atom);
    ADD_ATOM(xstudio::module, disconnect_from_ui_atom);
    ADD_ATOM(xstudio::module, join_module_attr_events_atom);
    ADD_ATOM(xstudio::module, leave_module_attr_events_atom);
    ADD_ATOM(xstudio::module, grab_all_keyboard_input_atom);
    ADD_ATOM(xstudio::module, grab_all_mouse_input_atom);
    ADD_ATOM(xstudio::module, attribute_uuids_atom);
    ADD_ATOM(xstudio::module, module_add_menu_item_atom);
    ADD_ATOM(xstudio::module, module_remove_menu_item_atom);
    ADD_ATOM(xstudio::module, remove_attribute_atom);
    ADD_ATOM(xstudio::module, set_node_data_atom);

    ADD_ATOM(xstudio::global, exit_atom);
    ADD_ATOM(xstudio::global, api_exit_atom);
    ADD_ATOM(xstudio::global, busy_atom);
    ADD_ATOM(xstudio::global, create_studio_atom);
    ADD_ATOM(xstudio::global, get_api_mode_atom);
    ADD_ATOM(xstudio::global, get_application_mode_atom);
    ADD_ATOM(xstudio::global, get_global_audio_cache_atom);
    ADD_ATOM(xstudio::global, get_global_image_cache_atom);
    ADD_ATOM(xstudio::global, get_global_playhead_events_atom);
    ADD_ATOM(xstudio::global, get_global_store_atom);
    ADD_ATOM(xstudio::global, get_global_thumbnail_atom);
    ADD_ATOM(xstudio::global, get_plugin_manager_atom);
    ADD_ATOM(xstudio::global, get_python_atom);
    ADD_ATOM(xstudio::global, get_studio_atom);
    ADD_ATOM(xstudio::global, get_scanner_atom);
    ADD_ATOM(xstudio::global, remote_session_name_atom);
    ADD_ATOM(xstudio::global, status_atom);
    ADD_ATOM(xstudio::global, get_actor_from_registry_atom);

    ADD_ATOM(xstudio::media, acquire_media_detail_atom);
    ADD_ATOM(xstudio::media, add_media_source_atom);
    ADD_ATOM(xstudio::media, add_media_stream_atom);
    ADD_ATOM(xstudio::media, current_media_atom);
    ADD_ATOM(xstudio::media, current_media_source_atom);
    ADD_ATOM(xstudio::media, current_media_stream_atom);
    ADD_ATOM(xstudio::media, get_edit_list_atom);
    ADD_ATOM(xstudio::media, get_media_details_atom); // DEPRECATED
    ADD_ATOM(xstudio::media, get_media_pointer_atom);
    ADD_ATOM(xstudio::media, get_media_pointers_atom);
    ADD_ATOM(xstudio::media, get_media_source_atom);
    ADD_ATOM(xstudio::media, get_media_source_names_atom);
    ADD_ATOM(xstudio::media, get_media_stream_atom);
    ADD_ATOM(xstudio::media, get_media_type_atom);
    ADD_ATOM(xstudio::media, media_status_atom);
    ADD_ATOM(xstudio::media, get_stream_detail_atom);
    ADD_ATOM(xstudio::media, invalidate_cache_atom);
    ADD_ATOM(xstudio::media, rescan_atom);
    ADD_ATOM(xstudio::media, media_reference_atom);
    ADD_ATOM(xstudio::media, source_offset_frames_atom);
    ADD_ATOM(xstudio::global_store, autosave_atom);
    ADD_ATOM(xstudio::global_store, do_autosave_atom);
    ADD_ATOM(xstudio::global_store, save_atom);
    ADD_ATOM(xstudio::utility, change_atom);
    ADD_ATOM(xstudio::utility, clear_atom);
    ADD_ATOM(xstudio::utility, detail_atom);
    ADD_ATOM(xstudio::utility, duplicate_atom);
    ADD_ATOM(xstudio::utility, event_atom);
    ADD_ATOM(xstudio::utility, get_event_group_atom);
    ADD_ATOM(xstudio::utility, get_group_atom);
    ADD_ATOM(xstudio::utility, last_changed_atom);
    ADD_ATOM(xstudio::utility, move_atom);
    ADD_ATOM(xstudio::utility, name_atom);
    ADD_ATOM(xstudio::utility, parent_atom);
    ADD_ATOM(xstudio::utility, rate_atom);
    ADD_ATOM(xstudio::utility, remove_atom);
    ADD_ATOM(xstudio::utility, serialise_atom);
    ADD_ATOM(xstudio::utility, type_atom);
    ADD_ATOM(xstudio::utility, uuid_atom);
    ADD_ATOM(xstudio::utility, version_atom);
    ADD_ATOM(xstudio::json_store, get_json_atom);
    ADD_ATOM(xstudio::json_store, jsonstore_change_atom);
    ADD_ATOM(xstudio::json_store, patch_atom);
    ADD_ATOM(xstudio::json_store, set_json_atom);
    ADD_ATOM(xstudio::json_store, subscribe_atom);
    ADD_ATOM(xstudio::json_store, unsubscribe_atom);
    ADD_ATOM(xstudio::json_store, update_atom);
    ADD_ATOM(xstudio::json_store, erase_json_atom);
    ADD_ATOM(xstudio::json_store, merge_json_atom);
    ADD_ATOM(xstudio::playhead, actual_playback_rate_atom);
    ADD_ATOM(xstudio::playhead, buffer_atom);
    ADD_ATOM(xstudio::playhead, child_playheads_deleted_atom);
    ADD_ATOM(xstudio::playhead, compare_mode_atom);
    ADD_ATOM(xstudio::playhead, duration_flicks_atom);
    ADD_ATOM(xstudio::playhead, duration_frames_atom);
    ADD_ATOM(xstudio::playhead, flicks_to_logical_frame_atom);
    ADD_ATOM(xstudio::playhead, get_selected_sources_atom);
    ADD_ATOM(xstudio::playhead, get_selection_atom);
    ADD_ATOM(xstudio::playhead, selection_changed_atom);
    ADD_ATOM(xstudio::playhead, jump_atom);
    ADD_ATOM(xstudio::playhead, key_child_playhead_atom);
    ADD_ATOM(xstudio::playhead, key_playhead_index_atom);
    ADD_ATOM(xstudio::playhead, logical_frame_atom);
    ADD_ATOM(xstudio::playhead, check_logical_frame_changing_atom);
    ADD_ATOM(xstudio::playhead, logical_frame_to_flicks_atom);
    ADD_ATOM(xstudio::playhead, media_frame_to_flicks_atom);
    ADD_ATOM(xstudio::playhead, loop_atom);
    ADD_ATOM(xstudio::playhead, media_atom);
    ADD_ATOM(xstudio::playhead, media_source_atom);
    ADD_ATOM(xstudio::playhead, overflow_mode_atom);
    ADD_ATOM(xstudio::playhead, play_atom);
    ADD_ATOM(xstudio::playhead, play_forward_atom);
    ADD_ATOM(xstudio::playhead, play_rate_mode_atom);
    ADD_ATOM(xstudio::playhead, playhead_rate_atom);
    ADD_ATOM(xstudio::playhead, position_atom);
    ADD_ATOM(xstudio::playhead, precache_atom);
    ADD_ATOM(xstudio::playhead, full_precache_atom);
    ADD_ATOM(xstudio::playhead, redraw_viewport_atom);
    ADD_ATOM(xstudio::playhead, future_frames_atom);
    ADD_ATOM(xstudio::playhead, select_next_media_atom);
    ADD_ATOM(xstudio::playhead, show_atom);
    ADD_ATOM(xstudio::playhead, dropped_frame_atom);
    ADD_ATOM(xstudio::playhead, simple_loop_end_atom);
    ADD_ATOM(xstudio::playhead, simple_loop_start_atom);
    ADD_ATOM(xstudio::playhead, source_atom);
    ADD_ATOM(xstudio::playhead, step_atom);
    ADD_ATOM(xstudio::playhead, use_loop_range_atom);
    ADD_ATOM(xstudio::playhead, velocity_atom);
    ADD_ATOM(xstudio::playhead, velocity_multiplier_atom);
    ADD_ATOM(xstudio::playhead, delete_selection_from_playlist_atom);
    ADD_ATOM(xstudio::playhead, evict_selection_from_cache_atom);
    ADD_ATOM(xstudio::playhead, fps_event_group_atom);
    ADD_ATOM(xstudio::playhead, viewport_events_group_atom);
    ADD_ATOM(xstudio::playhead, monitored_atom);
    ADD_ATOM(xstudio::playhead, media_logical_frame_atom);
    // ADD_ATOM(xstudio::audio, push_samples_atom);
    // ADD_ATOM(xstudio::http_client, http_get_atom);
    // ADD_ATOM(xstudio::http_client, http_get_simple_atom);
    // ADD_ATOM(xstudio::http_client, http_post_atom);
    // ADD_ATOM(xstudio::http_client, http_post_simple_atom);
    // ADD_ATOM(xstudio::http_client, http_put_atom);
    // ADD_ATOM(xstudio::http_client, http_put_simple_atom);
    // ADD_ATOM(xstudio::http_client, http_delete_atom);
    // ADD_ATOM(xstudio::http_client, http_delete_simple_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_acquire_authentication_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_acquire_token_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_authenticate_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_credential_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_entity_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_entity_filter_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_entity_search_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_host_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_info_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_link_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_preferences_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_projects_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_refresh_token_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_schema_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_schema_entity_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_schema_entity_fields_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_authentication_source_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_text_search_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_update_entity_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_create_entity_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_upload_atom);
    // ADD_ATOM(xstudio::shotgun_client, shotgun_image_atom);
    ADD_ATOM(xstudio::plugin_manager, add_path_atom);
    ADD_ATOM(xstudio::plugin_manager, spawn_plugin_atom);
    ADD_ATOM(xstudio::plugin_manager, spawn_plugin_base_atom);
    ADD_ATOM(xstudio::plugin_manager, spawn_plugin_ui_atom);
    ADD_ATOM(xstudio::plugin_manager, enable_atom);
    ADD_ATOM(xstudio::plugin_manager, get_resident_atom);
    ADD_ATOM(xstudio::data_source, get_data_atom);
    ADD_ATOM(xstudio::data_source, put_data_atom);
    ADD_ATOM(xstudio::data_source, post_data_atom);
    ADD_ATOM(xstudio::data_source, use_data_atom);
    ADD_ATOM(xstudio::bookmark, get_bookmark_atom);
    ADD_ATOM(xstudio::bookmark, get_bookmarks_atom);
    ADD_ATOM(xstudio::bookmark, add_bookmark_atom);
    ADD_ATOM(xstudio::bookmark, remove_bookmark_atom);
    ADD_ATOM(xstudio::bookmark, bookmark_detail_atom);
    ADD_ATOM(xstudio::bookmark, associate_bookmark_atom);
    ADD_ATOM(xstudio::bookmark, bookmark_change_atom);
    ADD_ATOM(xstudio::bookmark, render_annotations_atom);

    ADD_ATOM(xstudio::history, undo_atom);
    ADD_ATOM(xstudio::history, redo_atom);
    ADD_ATOM(xstudio::history, log_atom);
    ADD_ATOM(xstudio::history, history_atom);

    ADD_ATOM(xstudio::ui::viewport, viewport_playhead_atom);
    ADD_ATOM(xstudio::ui::viewport, quickview_media_atom);
    ADD_ATOM(xstudio::ui::viewport, viewport_atom);
    ADD_ATOM(xstudio::ui::viewport, hud_settings_atom);
    ADD_ATOM(xstudio::ui::viewport, viewport_layout_atom);    

    ADD_ATOM(xstudio::ui, show_message_box_atom);

    ADD_ATOM(xstudio::ui::keypress_monitor, register_hotkey_atom);
    ADD_ATOM(xstudio::ui::keypress_monitor, hotkey_event_atom);

    ADD_ATOM(xstudio::ui::model_data, menu_node_activated_atom);
}
} // namespace caf::python

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0
import xstudio.qml.helpers 1.0

Item
{
	property alias accent_color_string: accent_color_string
	property alias accent_gradient: accent_gradient
	property alias autosave: autosave

	property alias do_shutdown_anim: do_shutdown_anim
    property alias check_unsaved_session: check_unsaved_session
	property alias latest_version: latest_version
	property alias loop_mode: loop_mode
	property alias restore_play_state_after_scrub: restore_play_state_after_scrub
    property alias enable_presentation_mode: enable_presentation_mode
    property alias last_auto_save: last_auto_save
    property alias media_flags: media_flags
	property alias start_play_on_load: start_play_on_load
	property alias timeline_indicator: timeline_indicator
	property alias timeline_units: timeline_units
 	property alias do_overlay_messages: do_overlay_messages
    property alias xplayer_window: xplayer_window
    property alias channel: channel
    property alias exposure: exposure
    property alias velocity: velocity
    property alias note_category: note_category
    property alias note_depth: note_depth
    property alias note_colour: note_colour
    property alias compare: compare
    property alias source: source
    property alias view: view
    property alias display: display
    property alias python_history: python_history
    property alias recent_history: recent_history
    property alias session_compression: session_compression
    property alias quickview_all_incoming_media: quickview_all_incoming_media
    property alias session_link_prefix: session_link_prefix
    property alias click_to_toggle_play: click_to_toggle_play
 	// property alias panel_geoms: panel_geoms
    property alias control_values: control_values
    property alias popout_viewer_visible: popout_viewer_visible
    property alias image_cache_limit: image_cache_limit
    property alias auto_gather_sources: auto_gather_sources
    property alias audio_latency_millisecs: audio_latency_millisecs
    property alias cycle_through_playlist: cycle_through_playlist
    property alias new_media_rate: new_media_rate
    property alias viewport_scrub_sensitivity: viewport_scrub_sensitivity
    property alias default_playhead_compare_mode: default_playhead_compare_mode
    property alias default_media_folder: default_media_folder
    property alias snapshot_paths: snapshot_paths

    property color accent_color: '#bb7700'

    XsModelProperty {
        id: note_category
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/bookmark/note_category", "pathRole")
    }

    XsModelProperty {
        id: snapshot_paths
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/snapshot/paths", "pathRole")
    }

    XsModelProperty {
        id: note_depth
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/bookmark/note_depth", "pathRole")
    }

    XsModelProperty {
        id: note_colour
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/bookmark/note_colour", "pathRole")
    }

    XsModelProperty {
        id: velocity
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/velocity", "pathRole")
    }

    XsModelProperty {
        id: exposure
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/exposure", "pathRole")
    }

    XsModelProperty {
        id: new_media_rate
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/media_rate", "pathRole")
    }

    XsModelProperty {
        id: click_to_toggle_play
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/click_to_toggle_play", "pathRole")
    }

    XsModelProperty {
        id: session_link_prefix
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/session_link_prefix", "pathRole")
    }

    XsModelProperty {
        id: session_compression
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/compression", "pathRole")
    }

    XsModelProperty {
        id: quickview_all_incoming_media
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/quickview_all_incoming_media", "pathRole")
    }

    XsModelProperty {
        id: xplayer_window
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/xplayer_window", "pathRole")
    }

    XsModelProperty {
        id: image_cache_limit
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/image_cache/max_size", "pathRole")
    }

    XsModelProperty {
        id: media_flags
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/media_flags", "pathRole")
    }

    XsModelProperty {
        id: auto_gather_sources
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/media_reader/auto_gather_sources", "pathRole")
    }

    XsModelProperty {
        id: viewport_scrub_sensitivity
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/viewport/viewport_scrub_sensitivity", "pathRole")
    }

    XsModelProperty {
        id: compare
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/compare", "pathRole")
    }

    XsModelProperty {
        id: channel
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/channel", "pathRole")
    }

    XsModelProperty {
        id: display
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/display", "pathRole")
    }

    XsModelProperty {
        id: view
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/view", "pathRole")
    }

    XsModelProperty {
        id: source
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/source", "pathRole")
    }

    XsModelProperty {
        id: control_values
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/control_values", "pathRole")
    }

    XsModelProperty {
        id: do_overlay_messages
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/do_overlay_messages", "pathRole")
    }

    XsModelProperty {
        id: do_shutdown_anim
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/do_shutdown_anim", "pathRole")
    }

    XsModelProperty {
        id: check_unsaved_session
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/check_unsaved_session", "pathRole")
    }

    XsModelProperty {
        id: restore_play_state_after_scrub
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/restore_play_state_after_scrub", "pathRole")
    }

    XsModelProperty {
        id: timeline_units
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/timeline_units", "pathRole")
    }

    XsModelProperty {
        id: timeline_indicator
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/timeline_indicator", "pathRole")
    }

    XsModelProperty {
        id: loop_mode
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/loop_mode", "pathRole")
    }

    XsModelProperty {
        id: autosave
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/autosave", "pathRole")
        onValueChanged: {
            if(app_window.globalStoreModel.autosave != value)  {app_window.globalStoreModel.autosave = value}
        }
    }

    XsModelProperty {
        id: accent_color_string
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/accent_color", "pathRole")
        onValueChanged: {
            if(!app_window.initialized){
                styleGradient.highlightColorString = value
                initialized = true
            }
        }
    }

    XsModelProperty {
        id: audio_latency_millisecs
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/audio/audio_latency_millisecs", "pathRole")
    }

    XsModelProperty {
        id: latest_version
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/latest_version", "pathRole")
    }

    XsModelProperty {
        id: enable_presentation_mode
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/enable_presentation_mode", "pathRole")
    }

    XsModelProperty {
        id: last_auto_save
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/core/session/autosave/last_auto_save", "pathRole")
    }

    XsModelProperty {
        id: start_play_on_load
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/start_play_on_load", "pathRole")
    }

    XsModelProperty {
        id: view_top_toolbar
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/view_top_toolbar", "pathRole")
    }

    XsModelProperty {
        id: popout_viewer_visible
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/popout_viewer_visible", "pathRole")
    }

    XsModelProperty {
        id: python_history
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/python_history", "pathRole")
    }

    XsModelProperty {
        id: recent_history
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/recent_history", "pathRole")
    }

    XsModelProperty {
        id: cycle_through_playlist
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/cycle_through_playlist", "pathRole")
    }

    XsModelProperty {
        id: default_playhead_compare_mode
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/default_playhead_compare_mode", "pathRole")
    }

    XsModelProperty {
        id: default_media_folder
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/default_media_folder", "pathRole")
    }

    Gradient {
        id:accent_gradient
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: accent_color }
    }

    Component
    {
        id:stopComponent
        GradientStop {}
    }

    Component.onCompleted:
	{
	}

    function assignAccentGradient() {
        var value = accent_color_string.value
        assignColorStringToGradient(value, accent_gradient)

        if (value.indexOf(',') === -1) {
            // solid color
            accent_color = value
        } else {
            // gradient color
            accent_color = "#666666"
        }
        // root.refreshAllControls()
    }

    Component.onDestruction: {
        // settings.state = page.state
    }


    function assignColorStringToGradient(colorstring, gradobj) {
        var spl
        if (colorstring.indexOf(',') === -1) {
            // solid color
            spl = [colorstring]
        } else {
            // gradient color
            spl = colorstring.split(',')
        }
        var stops = []
        var interval = 1.0
        if (spl.length > 1) {
            interval = 1.00 / (spl.length - 1)
        }
        var currStop = 0.0
        var stop
        for (var index in spl) {
            stop = stopComponent.createObject(app_window, {"position":currStop,"color":spl[index]});
            stops.push(stop)
            currStop = currStop + interval
        }
        gradobj.stops = stops
    }
}
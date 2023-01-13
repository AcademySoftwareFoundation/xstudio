// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

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
    property alias compare: compare
    property alias source: source
    property alias view: view
    property alias display: display
    property alias volume: volume
    property alias mute: mute
    property alias python_history: python_history
    property alias recent_history: recent_history
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

    property color accent_color: '#bb7700'

    XsPreferenceStore {
        id: click_to_toggle_play
        path: "/ui/qml/click_to_toggle_play"
    }

    XsPreferenceStore {
        id: session_link_prefix
        path: "/core/session/session_link_prefix"
    }

    XsPreferenceStore {
        id: xplayer_window
        path: "/ui/qml/xplayer_window"
    }

    XsPreferenceStore {
        id: volume
        path: "/ui/qml/volume"
    }

    XsPreferenceStore {
        id: mute
        path: "/ui/qml/audio_mute"
    }

    XsPreferenceStore {
        id: velocity
        path: "/ui/qml/velocity"
    }

    XsPreferenceStore {
        id: image_cache_limit
        path: "/core/image_cache/max_size"
    }

    XsPreferenceStore {
        id: media_flags
        path: "/core/session/media_flags"
    }

    XsPreferenceStore {
        id: auto_gather_sources
        path: "/core/media_reader/auto_gather_sources"
    }

    XsPreferenceStore {
        id: new_media_rate
        path: "/core/session/media_rate"
    }

    XsPreferenceStore {
        id: viewport_scrub_sensitivity
        path: "/ui/viewport/viewport_scrub_sensitivity"
    }

    XsPreferenceStore {
        id: compare
        path: "/ui/qml/compare"
    }

    XsPreferenceStore {
        id: exposure
        path: "/ui/qml/exposure"
    }

    XsPreferenceStore {
        id: channel
        path: "/ui/qml/channel"
     }
    XsPreferenceStore {
        id: display
        path: "/ui/qml/display"
     }
    XsPreferenceStore {
        id: view
        path: "/ui/qml/view"
     }
    XsPreferenceStore {
        id: source
        path: "/ui/qml/source"
     }

    XsPreferenceStore {
        id: control_values
        path: "/ui/qml/control_values"
     }

    // XsPreferenceStore {
    //     id: panel_geoms
    //     path: "/ui/qml/panel_geoms"
    // }

    XsPreferenceStore {
        id: do_overlay_messages
        path: "/ui/qml/do_overlay_messages"
     }

    XsPreferenceStore {
        id: do_shutdown_anim
        path: "/ui/qml/do_shutdown_anim"
     }

    XsPreferenceStore {
        id: check_unsaved_session
        path: "/ui/qml/check_unsaved_session"
     }

    XsPreferenceStore {
        id: restore_play_state_after_scrub
        path: "/ui/qml/restore_play_state_after_scrub"
     }

    XsPreferenceStore {
        id: timeline_units
        path: "/ui/qml/timeline_units"
     }

    XsPreferenceStore {
        id: timeline_indicator
        path: "/ui/qml/timeline_indicator"
     }

    XsPreferenceStore {
        id: loop_mode
        path: "/ui/qml/loop_mode"
     }

    XsPreferenceStore {
        id: autosave
        path: "/ui/qml/autosave"
        onValue_changed: { if(global_store.autosave != value)  {global_store.autosave = value}}
     }

    XsPreferenceStore {
        id: accent_color_string
        path: "/ui/qml/accent_color"
        onValue_changed: {
            if(!app_window.initialized){
                styleGradient.highlightColorString = value
                initialized = true
            }
        }
     }

     XsPreferenceStore {
        id: audio_latency_millisecs
        path: "/core/audio/audio_latency_millisecs"
     }

    XsPreferenceStore {
        id: latest_version
        path: "/ui/qml/latest_version"
     }

    XsPreferenceStore {
        id: enable_presentation_mode
        path: "/ui/qml/enable_presentation_mode"
     }

    XsPreferenceStore {
        id: last_auto_save
        path: "/core/session/autosave/last_auto_save"
     }

    XsPreferenceStore {
        id: start_play_on_load
        path: "/ui/qml/start_play_on_load"
     }

    XsPreferenceStore {
        id: view_top_toolbar
        path: "/ui/qml/view_top_toolbar"
     }

     XsPreferenceStore {
        id: popout_viewer_visible
        path: "/ui/qml/popout_viewer_visible"
     }

     XsPreferenceStore {
        id: python_history
        path: "/ui/qml/python_history"
     }

    XsPreferenceStore {
        id: recent_history
        path: "/ui/qml/recent_history"
    }

    XsPreferenceStore {
        id: cycle_through_playlist
        path: "/ui/qml/cycle_through_playlist"
    }

    XsPreferenceStore {
        id: default_playhead_compare_mode
        path: "/ui/qml/default_playhead_compare_mode"
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
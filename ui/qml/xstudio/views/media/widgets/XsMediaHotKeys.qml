// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtGraphicalEffects 1.15
import QtQml.Models 2.14

import xStudio 1.0

import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0
import "../functions"


XsHotkeyArea {

    id: hotkey_area
    anchors.fill: parent
    context: "" + parent
    focus: true

    property alias delete_selected_hotkey: delete_selected_hotkey
    property alias select_all_hotkey: select_all_hotkey
    property alias deselect_all_hotkey: deselect_all_hotkey
    property alias reload_selected_media_hotkey: reload_selected_media_hotkey

    XsHotkey {
        id: select_all_hotkey
        sequence: "Ctrl+A"
        name: "Select All Media in Playlist"
        description: "Selects all the media in the playlist/subset"
        context: "" + parent
        componentName: "Media List"
        onActivated: {
            selectAll()
        }
    }
    XsHotkey {
        id: deselect_all_hotkey
        sequence: "Ctrl+D"
        name: "Deselect All Media"
        description: "De-selects all the media in the playlist/subset"
        context: "" + parent
        componentName: "Media List"
        onActivated: {
            deselectAll()
        }
    }
    XsHotkey {
        id: delete_selected_hotkey
        sequence: "Delete"
        name: "Delete Selected Media"
        description: "Removes selected media from media list"
        context: "" + parent
        componentName: "Media List"
        onActivated: {
            deleteSelected()
        }
    }
    XsHotkey {
        sequence: "Shift+Up"
        name: "Add to selected media (upwards)"
        description: "Adds the media item immediately above the first selected media item."
        context: "" + parent
        onActivated: selectUp()
        componentName: "Media List"
    }
    XsHotkey {
        sequence: "Shift+Down"
        name: "Add to selected media (downwards)"
        description: "Adds the media item immediately below the last selected media item."
        context: "" + parent
        onActivated: selectDown()
        componentName: "Media List"
    }


    XsHotkey {
        id: reload_selected_media_hotkey
        sequence: "SHIFT+~"
        name: "Reload Selected Media"
        description: "Reload selected media items."
        context: "" + parent
        onActivated: theSessionData.rescanMedia(mediaSelectionModel.selectedIndexes)
        componentName: "Media List"
    }
}
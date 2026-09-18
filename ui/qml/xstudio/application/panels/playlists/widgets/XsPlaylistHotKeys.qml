// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0

XsHotkeyArea {

    id: hotkey_area
    anchors.fill: parent
    context: ""+hotkey_area
    focus: true

    property alias add_playlist_hotkey: add_playlist_hotkey
    property alias add_subset_hotkey: add_subset_hotkey
    property alias add_sequence_hotkey: add_sequence_hotkey
    property alias add_contact_sheet_hotkey: add_contact_sheet_hotkey
    property alias add_divider_hotkey: add_divider_hotkey
    property alias add_dated_divider_hotkey: add_dated_divider_hotkey
    property alias add_media_hotkey: add_media_hotkey

    property alias copy_names_to_clipboard_hotkey: copy_names_to_clipboard_hotkey
    property alias rename_hotkey: rename_hotkey
    property alias duplicate_hotkey: duplicate_hotkey
    property alias combine_hotkey: combine_hotkey
    property alias remove_unused_hotkey: remove_unused_hotkey
    property alias remove_selected: remove_selected

    XsHotkey {
        id: add_playlist_hotkey
        name: "Add Playlist"
        description: "Add New Playlist"
        onActivated: playlist_functions.addPlaylistChoice()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: add_subset_hotkey
        name: "Add Subset"
        description: "Add New Subset"
        onActivated: playlist_functions.addSubsetChoice()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: add_sequence_hotkey
        name: "Add Sequence"
        description: "Add New Sequence"
        onActivated: playlist_functions.addSequenceChoice()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: add_contact_sheet_hotkey
        name: "Add Contact Sheet"
        description: "Add New Contact Sheet"
        onActivated: playlist_functions.addContactSheetChoice()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: add_divider_hotkey
        name: "Add Divider"
        description: "Add New Divider"
        onActivated: playlist_functions.addDividerChoice()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: add_dated_divider_hotkey
        name: "Add Dated Divider"
        description: "Add New Dated Divider"
        onActivated: playlist_functions.addDatedDivider()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: add_media_hotkey
        name: "Add Media"
        description: "Add New Media"
        onActivated: file_functions.loadMedia(undefined)

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: copy_names_to_clipboard_hotkey
        name: "Copy Names"
        description: "Copy Selected Names To Clipboard"
        onActivated: playlist_functions.copySelectedNames()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: rename_hotkey
        name: "Rename"
        description: "Rename Playlist"
        onActivated: playlist_functions.renamePlaylist()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: duplicate_hotkey
        name: "Duplicate"
        description: "Duplicate Playlist"
        onActivated: playlist_functions.duplicate()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: combine_hotkey
        name: "Combine"
        description: "Combine Playlists"
        onActivated: playlist_functions.duplicate()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: remove_unused_hotkey
        name: "Remove Unused Media"
        description: "Remove Unused Media From Playlist"
        onActivated: playlist_functions.removeUnusedMedia()

        context: hotkey_area.context
        componentName: "Playlist"
    }

    XsHotkey {
        id: remove_selected
        name: "Remove Selected"
        description: "Remove Selected Playlists"
        onActivated: removeSelected()

        context: hotkey_area.context
        componentName: "Playlist"
    }
}
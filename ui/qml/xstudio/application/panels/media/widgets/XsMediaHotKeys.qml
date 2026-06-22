// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0
import "../functions"


XsHotkeyArea {

    id: hotkey_area
    anchors.fill: parent
    context: ""+hotkey_area
    focus: true

    property alias add_from_clipboard_hotkey: add_from_clipboard_hotkey
    property alias add_media_hotkey: add_media_hotkey
    property alias add_to_new_contact_sheet_hotkey: add_to_new_contact_sheet_hotkey
    property alias add_to_new_contact_sheets_hotkey: add_to_new_contact_sheets_hotkey
    property alias add_to_new_playlist_hotkey: add_to_new_playlist_hotkey
    property alias add_to_new_sequence_hotkey: add_to_new_sequence_hotkey
    property alias add_to_new_subset_hotkey: add_to_new_subset_hotkey
    property alias cycle_colour_hotkey: cycle_colour_hotkey
    property alias delete_offline_hotkey: delete_offline_hotkey
    property alias delete_selected_hotkey: delete_selected_hotkey
    property alias deselect_all_hotkey: deselect_all_hotkey
    property alias invert_selection_hotkey: invert_selection_hotkey
    property alias reload_selected_media_hotkey: reload_selected_media_hotkey
    property alias select_all_hotkey: select_all_hotkey
    property alias select_offline_hotkey: select_offline_hotkey
    property alias select_adjusted_hotkey: select_adjusted_hotkey
    property alias duplicate_media_hotkey: duplicate_media_hotkey
    property alias reveal_media_on_disk_hotkey: reveal_media_on_disk_hotkey

    property alias file_name_to_clipboard_hotkey: file_name_to_clipboard_hotkey
    property alias file_path_to_clipboard_hotkey: file_path_to_clipboard_hotkey
    property alias quickview_to_clipboard_hotkey: quickview_to_clipboard_hotkey

    property alias add_to_new_playlist_context_hotkey: add_to_new_playlist_context_hotkey
    property alias emaillink_to_clipboard_hotkey: emaillink_to_clipboard_hotkey
    property alias open_in_quickview_hotkey: open_in_quickview_hotkey
    property alias set_media_fps_hotkey: open_in_quickview_hotkey
    property alias set_media_aspect_hotkey: set_media_aspect_hotkey

    XsHotkey {
        id: set_media_aspect_hotkey
        name: "Set Media Pixel Aspect"
        description: "Set Media Pixel Aspect"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.setMediaAspect()
    }

    XsHotkey {
        id: set_media_fps_hotkey
        name: "Set Media FPS"
        description: "Set Media FPS"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.setMediaFPS()
    }

    XsHotkey {
        id: open_in_quickview_hotkey
        name: "Open In QuickView"
        description: "Open In QuickView"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.openInQuickView()
    }

    XsHotkey {
        id: emaillink_to_clipboard_hotkey
        name: "Copy Email Link"
        description: "Copy Email Link To Clipboard"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.copyEmailLinkToClipboard()
    }

    XsHotkey {
        id: quickview_to_clipboard_hotkey
        name: "Copy QuickView Shell Command"
        description: "Copy QuickView Shell Command To Clipboard"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.copyQuickViewToClipboard()
    }

    XsHotkey {
        id: file_name_to_clipboard_hotkey
        name: "Copy File Name To Clipboard"
        description: "Copy Selected File Names To Clipboard"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.copyFileNamesToClipboard()
    }

    XsHotkey {
        id: file_path_to_clipboard_hotkey
        name: "Copy File Path To Clipboard"
        description: "Copy Selected File Paths To Clipboard"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.copyFilePathsToClipboard()
    }

    XsHotkey {
        id: reveal_media_on_disk_hotkey
        name: "Reveal Media On Disk"
        description: "Reveal Media On Disk"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: helpers.showURIS(mediaSelectionModel.getSelectedMediaUrl())
    }

    XsHotkey {
        id: duplicate_media_hotkey
        name: "Duplicate Selected Media"
        description: "Duplicate Selected Media"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.duplicate()
    }

    XsHotkey {
        id: select_adjusted_hotkey
        name: "Select Adjusted Media"
        description: "Select Adjusted Media"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.selectAllAdjusted()
    }

    XsHotkey {
        id: add_media_hotkey
        name: "Add Media "
        description: "Add Media To Playlist"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: file_functions.loadMedia(undefined)
    }

    XsHotkey {
        id: add_to_new_playlist_hotkey
        name: "Add To New Playlist"
        description: "Add Media To New Playlist"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.addToNewPlaylistDialog()
    }

    XsHotkey {
        id: add_to_new_playlist_context_hotkey
        name: "Add To New Playlist (C/M)"
        description: "Add Media To New Playlist (Copy or Move)"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.addtoNewPlaylistContext()
    }

    XsHotkey {
        id: add_to_new_subset_hotkey
        name: "Add To New Subset"
        description: "Add Media To New Subset"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.addToNewSubsetDialog()
    }

    XsHotkey {
        id: add_to_new_sequence_hotkey
        name: "Add To New Sequence"
        description: "Add Media To New Sequence"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.addToNewSequenceDialog()
    }

    XsHotkey {
        id: add_to_new_contact_sheet_hotkey
        name: "Add To New Contact Sheet"
        description: "Add Media To New Contact Sheet"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.addToNewContactSheetDialog()
    }

    XsHotkey {
        id: add_to_new_contact_sheets_hotkey
        name: "Add To New Contact Sheets"
        description: "Add Media To New Contact Sheets"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.addToNewContactSheetsDialog()
    }

    XsHotkey {
        id: add_from_clipboard_hotkey
        name: "Add From Clipboard"
        description: "Add Media From Clipboard"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: file_functions.addMediaFromClipboard()
    }

    XsHotkey {
        id: select_all_hotkey
        sequence: "Ctrl+A"
        name: "Select All Media in Playlist"
        description: "Selects all the media in the playlist/subset"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.selectAll()
    }

    XsHotkey {
        id: invert_selection_hotkey
        name: "Invert Media Selection"
        description: "Inverts selection in playlist/subset"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.invertSelection()
    }

    XsHotkey {
        id: select_offline_hotkey
        name: "Select Offline Media"
        description: "Sellect Offline Media in playlist/subset"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.selectAllOffline()
    }

    XsHotkey {
        id: paste_hotkey
        sequence: "Ctrl+V"
        name: "Paste From Clipboard"
        description: "Paste Clipboard Content into Media List"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: file_functions.addMediaFromClipboard()
    }


    XsHotkey {
        id: cycle_colour_hotkey
        context: hotkey_area.context
        sequence:  "Shift+C"
        name: "Cycle Media Colour"
        description: "Cycle Selected Media Colour"
        onActivated: media_list_functions.cycleColour(mediaSelectionModel.selectedIndexes)
        componentName: "Media List"
    }

    XsHotkey {
        id: deselect_all_hotkey
        sequence: "Ctrl+D"
        name: "Deselect All Media"
        description: "De-selects all the media in the playlist/subset"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.deselectAll()
    }

    XsHotkey {
        id: delete_selected_hotkey
        sequence: "Delete"
        name: "Delete Selected Media"
        description: "Removes selected media from media list"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.deleteSelected()
    }

    XsHotkey {
        id: delete_offline_hotkey
        name: "Delete Offline Media"
        description: "Removes Offline Media from media list"
        context: hotkey_area.context
        componentName: "Media List"
        onActivated: media_list_functions.deleteOffline()
    }

    XsHotkey {
        sequence: "Shift+Up"
        name: "Add to selected media (upwards)"
        description: "Adds the media item immediately above the first selected media item."
        context: hotkey_area.context
        onActivated: media_list_functions.selectUp()
        componentName: "Media List"
    }

    XsHotkey {
        sequence: "Shift+Down"
        name: "Add to selected media (downwards)"
        description: "Adds the media item immediately below the last selected media item."
        context: hotkey_area.context
        onActivated: media_list_functions.selectDown()
        componentName: "Media List"
    }


    XsHotkey {
        id: reload_selected_media_hotkey
        sequence: "U"
        name: "Reload Selected Media"
        description: "Reload selected media items."
        context: hotkey_area.context
        onActivated: theSessionData.rescanMedia(mediaSelectionModel.selectedIndexes)
        componentName: "Media List"
    }

    XsHotkey {
        id: frame_hotkey
        sequence: "F"
        name: "Frame Selected"
        description: "Frame Selected"
        context: hotkey_area.context
        onActivated: frameSelectedMedia()
        componentName: "Media List"
    }

    // add hotkeys for setting flag colours
    Repeater {
        model: flagColours.length
        Item {
            XsHotkey {
                sequence: "Ctrl+Alt+" + index
                name: "Set flag #" + index + ": " + flagColours[index].name
                description: "Set flag #" + index + ": " + flagColours[index].name
                context: hotkey_area.context
                onActivated: media_list_functions.setColour(mediaSelectionModel.selectedIndexes, index)
                componentName: "Media List"
            }
        }
    }
}
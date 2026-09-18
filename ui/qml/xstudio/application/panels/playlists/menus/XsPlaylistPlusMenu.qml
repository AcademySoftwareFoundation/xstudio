// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0

XsPopupMenu {

    id: plusMenu
    visible: false
    menu_model_name: "playlist_plus_button_menu"
    property var path: ""

    // property idenfies the 'panel' that is the anticedent of this
    // menu instance. As this menu is instanced multiple times in the
    // xstudio interface we use this context property to ensure our
    // 'onActivated' callback/signal is only triggered in the corresponding
    // XsMenuModelItem instance.
    property var panelContext: helpers.contextPanel(plusMenu)

    XsMenuModelItem {
        text: "Playlist"
        menuItemType: "button"
        menuPath: path
        hotkeyUuid: hotkey_area.add_playlist_hotkey.uuid
        menuItemPosition: 1
        menuModelName: plusMenu.menu_model_name
        onActivated: playlist_functions.addPlaylistChoice()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Subset"
        menuItemType: "button"
        hotkeyUuid: hotkey_area.add_subset_hotkey.uuid
        menuPath: path
        menuItemPosition: 2
        menuModelName: plusMenu.menu_model_name
        onActivated: playlist_functions.addSubsetChoice()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Sequence"
        menuItemType: "button"
        hotkeyUuid: hotkey_area.add_sequence_hotkey.uuid
        menuPath: path
        menuItemPosition: 3
        menuModelName: plusMenu.menu_model_name
        onActivated: playlist_functions.addSequenceChoice()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Contact Sheet"
        menuItemType: "button"
        hotkeyUuid: hotkey_area.add_contact_sheet_hotkey.uuid
        menuPath: path
        menuItemPosition: 3
        menuModelName: plusMenu.menu_model_name
        onActivated: playlist_functions.addContactSheetChoice()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: path
        menuItemPosition: 3.2
        menuModelName: plusMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Divider"
        menuItemType: "button"
        menuPath: path
        hotkeyUuid: hotkey_area.add_divider_hotkey.uuid
        menuItemPosition: 3.5
        menuModelName: plusMenu.menu_model_name
        onActivated: playlist_functions.addDividerChoice()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Dated Divider"
        menuItemType: "button"
        menuPath: path
        hotkeyUuid: hotkey_area.add_dated_divider_hotkey.uuid
        menuItemPosition: 3.6
        menuModelName: plusMenu.menu_model_name
        onActivated: playlist_functions.addDatedDivider()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: path
        menuItemPosition: 4
        menuModelName: plusMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Media ..."
        menuItemType: "button"
        menuPath: path
        menuItemPosition: 5
        menuModelName: plusMenu.menu_model_name
        onActivated: file_functions.loadMedia(undefined)
        panelContext: plusMenu.panelContext
        hotkeyUuid: hotkey_area.add_media_hotkey.uuid
    }
}

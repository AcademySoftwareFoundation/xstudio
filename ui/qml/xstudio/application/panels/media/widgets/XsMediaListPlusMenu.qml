// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0

import "../functions/"


XsPopupMenu {

    id: plusMenu
    visible: false
    menu_model_name: "medialist_plus_button_menu"

    // property idenfies the 'panel' that is the anticedent of this
    // menu instance. As this menu is instanced multiple times in the
    // xstudio interface we use this context property to ensure our
    // 'onActivated' callback/signal is only triggered in the corresponding
    // XsMenuModelItem instance.
    property var panelContext: helpers.contextPanel(plusMenu)

    XsMenuModelItem {
        text: "Add To New Playlist"
        hotkeyUuid: hotkey_area.add_to_new_playlist_hotkey.uuid
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: plusMenu.menu_model_name
        onActivated: media_list_functions.addToNewPlaylistDialog()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Add To New Subset"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: plusMenu.menu_model_name
        hotkeyUuid: hotkey_area.add_to_new_subset_hotkey.uuid
        onActivated: media_list_functions.addToNewSubsetDialog()
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Add To New Sequence"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: plusMenu.menu_model_name
        onActivated: media_list_functions.addToNewSequenceDialog()
        panelContext: plusMenu.panelContext
        hotkeyUuid: hotkey_area.add_to_new_sequence_hotkey.uuid
    }

    XsMenuModelItem {
        text: "Add To New Contact Sheet"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: plusMenu.menu_model_name
        onActivated: media_list_functions.addToNewContactSheetDialog()
        hotkeyUuid: hotkey_area.add_to_new_contact_sheet_hotkey.uuid
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Add To New Contact Sheets..."
        menuPath: ""
        menuItemPosition: 3.5
        menuModelName: plusMenu.menu_model_name
        panelContext: plusMenu.panelContext
        onActivated: media_list_functions.addToNewContactSheetsDialog()
        hotkeyUuid: hotkey_area.add_to_new_contact_sheets_hotkey.uuid
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 4
        menuModelName: plusMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Add Media ..."
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 5
        menuModelName: plusMenu.menu_model_name
        onActivated: file_functions.loadMedia(undefined)
        panelContext: plusMenu.panelContext
        hotkeyUuid: hotkey_area.add_media_hotkey.uuid
    }

    XsMenuModelItem {
        text: "Add Media From Clipboard"
        menuPath: ""
        menuItemPosition: 6
        hotkeyUuid: hotkey_area.add_from_clipboard_hotkey.uuid
        menuModelName: plusMenu.menu_model_name
        onActivated: file_functions.addMediaFromClipboard()
    }
}

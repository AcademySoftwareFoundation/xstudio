// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

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
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: plusMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                plusMenu.addPlaylist,
                "Add Playlist",
                "Enter a name for the new playlist.",
                "New Playlist",
                ["Cancel", "Add"])
        }
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Add To New Subset"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: plusMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                plusMenu.addSubset,
                "Add Subset",
                "Enter a name for the new subset.",
                "New Subset",
                ["Cancel", "Add"])
        }
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Add To New Sequence"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: plusMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                plusMenu.addTimeline,
                "Add Sequence",
                "Enter a name for the new sequence.",
                "New Sequence",
                ["Cancel", "Add"])
        }
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Add To New Contact Sheet"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: plusMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                plusMenu.addContactSheet,
                "Add Contact Sheet",
                "Enter a name for the new contact sheet.",
                "New Contact Sheet",
                ["Cancel", "Add"])
        }
        panelContext: plusMenu.panelContext
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
        onActivated: {
            file_functions.loadMedia(undefined)
        }
        panelContext: plusMenu.panelContext
    }

    function addPlaylist(new_name, button) {
        if (button == "Add") {
            let pl = theSessionData.createPlaylist(new_name)
            theSessionData.copyRows(mediaSelectionModel.selectedIndexes, 0, pl)
        }
    }

    function addSubset(new_name, button) {
        if (button == "Add") {
            addToNewSubset(new_name)
        }
    }

    function addContactSheet(new_name, button) {
        if (button == "Add") {
            addToNewContactSheet(new_name)
        }
    }    

    function addTimeline(new_name, button) {
        if (button == "Add") {
            addToNewSequence(new_name)
        }
    }
}

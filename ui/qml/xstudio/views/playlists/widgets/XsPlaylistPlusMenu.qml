// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0
import xstudio.qml.models 1.0

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
        text: "Subset"
        menuItemType: "button"
        menuPath: path
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
        text: "Sequence"
        menuItemType: "button"
        menuPath: path
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
        text: "Contact Sheet"
        menuItemType: "button"
        menuPath: path
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
        menuPath: path
        menuItemPosition: 3.2
        menuModelName: plusMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Divider"
        menuItemType: "button"
        menuPath: path
        menuItemPosition: 3.5
        menuModelName: plusMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                plusMenu.addDivider,
                "Add Playlist Divider",
                "Enter a name for the new divider.",
                "New Divider",
                ["Cancel", "Add"])
        }
        panelContext: plusMenu.panelContext
    }

    XsMenuModelItem {
        text: "Dated Divider"
        menuItemType: "button"
        menuPath: path
        menuItemPosition: 3.6
        menuModelName: plusMenu.menu_model_name
        onActivated: theSessionData.createDivider(yyyymmdd("-"))
        panelContext: plusMenu.panelContext

        function yyyymmdd(separator="", date=new Date()) {
            return String(date.getFullYear()) + separator + String(date.getMonth()+1).padStart(2, '0') + separator + String(date.getUTCDate()).padStart(2, '0')
        }
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
        onActivated: {
            file_functions.loadMedia(undefined)
        }
        panelContext: plusMenu.panelContext
    }

    function addPlaylist(new_name, button) {
        if (button == "Add") {
            theSessionData.createPlaylist(new_name)
        }
    }

    function addSubset(new_name, button) {
        if (button == "Add") {
            theSessionData.createSubItem(new_name, "Subset")
        }
    }

    function addContactSheet(new_name, button) {
        if (button == "Add") {
            theSessionData.createSubItem(new_name, "ContactSheet")
        }
    }

    function addTimeline(new_name, button) {
        if (button == "Add") {
            theSessionData.createSubItem(new_name, "Timeline")
        }
    }

    function addDivider(new_name, button) {
        if (button == "Add") {
            theSessionData.createDivider(new_name)
        }
    }

}

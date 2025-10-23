// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts


import xStudio 1.0
import xstudio.qml.models 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0
import QuickFuture 1.0
import xstudio.qml.viewport 1.0

XsPopupMenu {
    id: rightClickMenu
    visible: false

    property var popupSelectionModel
    property var popupDelegateModel

    menu_model_name: "notehistory_menu_"+rightClickMenu

    Clipboard {
       id: clipboard
    }

    function selectAll() {
        popupSelectionModel.select(
            helpers.createItemSelectionFromList(ShotBrowserHelpers.getAllIndexes(popupDelegateModel)),
            ItemSelectionModel.Select
        )
    }
    XsHotkey {
        id: select_all_key
        name: "select_all_key"+rightClickMenu
        sequence: "Ctrl+A"
    }

    XsMenuModelItem {
        text: "Add To New Playlist"
        menuItemPosition: 1
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated:  ShotBrowserHelpers.addToNewPlaylist(popupSelectionModel.selectedIndexes)
    }

    XsMenuModelItem {
        text: "Load Annotations"
        menuItemPosition: 1.5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated:  ShotBrowserHelpers.loadAnnotations(popupSelectionModel.selectedIndexes)
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 2
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Select All"
        menuItemPosition: 3
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated:  selectAll()
        hotkeyUuid: select_all_key.uuid
    }
    XsMenuModelItem {
        text: "Deselect All"
        menuItemPosition: 4
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: popupSelectionModel.clear()
    }
    XsMenuModelItem {
        text: "Invert Selection"
        menuItemPosition: 5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: popupSelectionModel.select(
            helpers.createItemSelectionFromList(ShotBrowserHelpers.getAllIndexes(popupDelegateModel)),
            ItemSelectionModel.Toggle
        )
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 6
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Reveal In ShotGrid..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuItemPosition: 7
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInShotgrid(popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "Content"
        menuItemPosition: 8
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: {
            clipboard.text = ShotBrowserHelpers.getNote(popupSelectionModel.selectedIndexes);
            clipboard.html = ShotBrowserHelpers.getNoteHTML(popupSelectionModel.selectedIndexes);;
        }
    }
    XsMenuModelItem {
        text: "ShotGrid Link"
        menuItemPosition: 9
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = ShotBrowserHelpers.getLink(popupSelectionModel.selectedIndexes).join("\n")
    }
    XsMenuModelItem {
        text: "JSON"
        menuItemPosition: 10
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = JSON.stringify(ShotBrowserHelpers.getJSON(popupSelectionModel.selectedIndexes))
    }
    XsMenuModelItem {
        text: "DNUuid"
        menuItemPosition: 11
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = ShotBrowserHelpers.getDNUuid(popupSelectionModel.selectedIndexes).join("\n")

        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer Selected" sub menu in the top level menu
            setMenuPathPosition("Copy To Clipboard", 8.0)
        }
    }
}

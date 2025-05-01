// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QuickFuture 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0


XsPopupMenu {
    id: rightClickMenu
    visible: false

    property var popupSelectionModel
    property var popupDelegateModel

    menu_model_name: "playlisthistory_menu_"+rightClickMenu

    Clipboard {
       id: clipboard
    }

    XsMenuModelItem {
        text: "Select All"
        menuItemPosition: 1
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: popupSelectionModel.select(
            helpers.createItemSelectionFromList(ShotBrowserHelpers.getAllIndexes(popupDelegateModel)),
            ItemSelectionModel.Select
        )
    }
    XsMenuModelItem {
        text: "Deselect All"
        menuItemPosition: 2
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: popupSelectionModel.clear()
    }
    XsMenuModelItem {
        text: "Invert Selection"
        menuItemPosition: 3
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: popupSelectionModel.select(
            helpers.createItemSelectionFromList(ShotBrowserHelpers.getAllIndexes(popupDelegateModel)),
            ItemSelectionModel.Toggle
        )
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 4
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Reveal In ShotGrid..." + (enabled ? "" : " (Production Only)")
        enabled: ShotBrowserEngine.shotGridUserType != "User"
        menuItemPosition: 5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInShotgrid(popupSelectionModel.selectedIndexes)
    }

    XsMenuModelItem {
        text: "Copy JSON"
        menuItemPosition: 6.5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = JSON.stringify(ShotBrowserHelpers.getJSON(popupSelectionModel.selectedIndexes))
    }
    XsMenuModelItem {
        text: "Copy DNUuid"
        menuItemPosition: 6.6
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = ShotBrowserHelpers.getDNUuid(popupSelectionModel.selectedIndexes).join("\n")
    }

}

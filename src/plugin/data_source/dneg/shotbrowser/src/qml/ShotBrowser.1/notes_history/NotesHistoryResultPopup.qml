// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.clipboard 1.0
import QuickFuture 1.0

XsPopupMenu {
    id: rightClickMenu
    visible: false

    property var popupSelectionModel
    property var popupDelegateModel

    menu_model_name: "notehistory_menu_"+rightClickMenu

    Clipboard {
       id: clipboard
    }

    XsMenuModelItem {
        text: "Add To New Playlist"
        menuItemPosition: 1
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated:  ShotBrowserHelpers.addToNewPlaylist(popupSelectionModel.selectedIndexes)
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
        onActivated:  popupSelectionModel.select(
            helpers.createItemSelectionFromList(ShotBrowserHelpers.getAllIndexes(popupDelegateModel)),
            ItemSelectionModel.Select
        )
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
        text: "Reveal In ShotGrid"
        menuItemPosition: 7
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInShotgrid(popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "Copy"
        menuItemPosition: 8
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = ShotBrowserHelpers.getNote(popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "Copy JSON"
        menuItemPosition: 9
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = JSON.stringify(ShotBrowserHelpers.getJSON(popupSelectionModel.selectedIndexes))
    }
}

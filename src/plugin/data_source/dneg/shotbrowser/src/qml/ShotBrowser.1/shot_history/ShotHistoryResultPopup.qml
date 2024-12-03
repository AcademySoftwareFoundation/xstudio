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

    menu_model_name: "shothistory_menu_"+rightClickMenu

    Clipboard {
       id: clipboard
    }

    XsMenuModelItem {
        text: "Add To New Playlist"
        menuItemPosition: 0.1
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated:  ShotBrowserHelpers.addToNewPlaylist(popupSelectionModel.selectedIndexes)
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 0.5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Select All"
        menuItemPosition: 1
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated:  popupSelectionModel.select(
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
        text: "Reveal In ShotGrid"
        menuItemPosition: 5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInShotgrid(popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "Reveal In Ivy"
        menuItemPosition: 6
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInIvy(popupSelectionModel.selectedIndexes)
    }

    XsMenuModelItem {
        text: "Copy JSON"
        menuItemPosition: 6.5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = JSON.stringify(ShotBrowserHelpers.getJSON(popupSelectionModel.selectedIndexes))
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 7
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
    }
    // XsMenuModelItem {
    //     text: "Transfer Selected"
    //     menuItemPosition: 10
    //     menuPath: ""
    //     menuModelName: rightClickMenu.menu_model_name
    //     onActivated: {}
    // }


    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("chn", popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("lon", popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("mtl", popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("mum", popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("syd", popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "To Vancouver"
        menuItemPosition: 6
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("van", popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 7
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Open Transfer Tool"
        menuItemPosition: 8
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: {
            let uuids = []
            if(popupSelectionModel.selectedIndexes.length) {
                let indexes = ShotBrowserHelpers.mapIndexesToResultModel(popupSelectionModel.selectedIndexes)
                let m = indexes[0].model
                for(let i = 0; i< indexes.length; i++) {
                    let uuid = m.get(indexes[i], "stalkUuidRole")
                    if(uuid)
                        uuids.push(helpers.QUuidToQString(uuid))
                }
            }
            helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), helpers.getEnv("SHOT"), "--", "maketransfer"].concat(uuids))
        }

        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer Selected" sub menu in the top level menu
            setMenuPathPosition("Transfer Selected", 8.0)
        }
    }
}

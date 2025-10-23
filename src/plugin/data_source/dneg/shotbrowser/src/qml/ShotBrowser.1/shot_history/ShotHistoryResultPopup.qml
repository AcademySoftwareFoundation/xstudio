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

    menu_model_name: "shothistory_menu_"+rightClickMenu

    Clipboard {
       id: clipboard
    }

    XsPreference {
       id: fullTransfer
       path: "/plugin/data_source/shotbrowser/transfer/full"
    }

    XsPreference {
       id: transferLeafs
       path: "/plugin/data_source/shotbrowser/transfer/leafs"
    }

    property var leaves: fullTransfer.value ? [] : transferLeafs.value

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
        onActivated:  selectAll()
        hotkeyUuid: select_all_key.uuid
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
        enabled: ShotBrowserEngine.shotGridLoginAllowed
        menuItemPosition: 5
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInShotgrid(popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "Reveal In Ivy..."
        menuItemPosition: 6
        menuPath: ""
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.revealInIvy(popupSelectionModel.selectedIndexes)
    }
    XsMenuModelItem {
        text: "ShotGrid Link"
        menuItemPosition: 6
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = ShotBrowserHelpers.getLink(popupSelectionModel.selectedIndexes).join("\n")
    }

    XsMenuModelItem {
        text: "Copy JSON"
        menuItemPosition: 6.5
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = JSON.stringify(ShotBrowserHelpers.getJSON(popupSelectionModel.selectedIndexes))
    }

    XsMenuModelItem {
        text: "Copy DNUuid"
        menuItemPosition: 6.6
        menuPath: "Copy To Clipboard"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: clipboard.text = ShotBrowserHelpers.getDNUuid(popupSelectionModel.selectedIndexes).join("\n")
        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer Selected" sub menu in the top level menu
            setMenuPathPosition("Copy To Clipboard", 6.5)
        }
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
        text: "To Here"
        menuItemPosition: 0.5
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer(helpers.getEnv("DNSITEDATA_SHORT_NAME"), popupSelectionModel.selectedIndexes, leaves)
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuItemPosition: 0.6
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "To Chennai"
        menuItemPosition: 1
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("chn", popupSelectionModel.selectedIndexes, leaves)
    }
    XsMenuModelItem {
        text: "To London"
        menuItemPosition: 2
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("lon", popupSelectionModel.selectedIndexes, leaves)
    }
    XsMenuModelItem {
        text: "To Montreal"
        menuItemPosition: 3
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("mtl", popupSelectionModel.selectedIndexes, leaves)
    }
    XsMenuModelItem {
        text: "To Mumbai"
        menuItemPosition: 4
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("mum", popupSelectionModel.selectedIndexes, leaves)
    }
    XsMenuModelItem {
        text: "To Sydney"
        menuItemPosition: 5
        menuPath: "Transfer Selected"
        menuModelName: rightClickMenu.menu_model_name
        onActivated: ShotBrowserHelpers.transfer("syd", popupSelectionModel.selectedIndexes, leaves)
    }
    // XsMenuModelItem {
    //     text: "To Vancouver"
    //     menuItemPosition: 6
    //     menuPath: "Transfer Selected"
    //     menuModelName: rightClickMenu.menu_model_name
    //     onActivated: ShotBrowserHelpers.transfer("van", popupSelectionModel.selectedIndexes)
    // }
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
            helpers.startDetachedProcess("dnenv-do", [helpers.getEnv("SHOW"), "--", "maketransfer"].concat(uuids))
        }

        Component.onCompleted: {
            // we need this so the menu model knows where to insert the
            // "Transfer Selected" sub menu in the top level menu
            setMenuPathPosition("Transfer Selected", 8.0)
        }
    }
}

import xstudio.qml.models 1.0
import xStudio 1.0
import QtQml.Models 2.14
import QtQuick 2.12

import xstudio.qml.helpers 1.0

XsPopupMenu {

    id: timelineMenu
    visible: false
    menu_model_name: "timeline_track_menu_"

    property var panelContext: helpers.contextPanel(timelineMenu)
    property var theTimeline: panelContext.theTimeline
    property var timelineSelection: theTimeline.timelineSelection
    property var debugSetMenuPathPosition: debug_menu.setMenuPathPosition

    property var currentTrackIndex: timelineSelection.selectedIndexes.length ? timelineSelection.selectedIndexes[0] : null

    onVisibleChanged: visible && updateFlags()


    Component.onCompleted: {
        // need to reorder snippet menus..
        let rc = embeddedPython.trackMenuModel.rowCount();
        for(let i=0; i < embeddedPython.trackMenuModel.rowCount(); i++) {
            let fi = embeddedPython.trackMenuModel.index(i, 0)
            let si = embeddedPython.trackMenuModel.mapToSource(fi)
            let mp = si.model.get(si, "menuPathRole")
            helpers.setMenuPathPosition(mp,"timeline_track_menu_", 33 + ((1.0/rc)*i) )
        }
    }


    function updateItemSelectionHorizontal(l,r) {
        timelineSelection.select(helpers.createItemSelection(
                theSessionData.modifyItemSelectionHorizontal(timelineSelection.selectedIndexes, l, r)
            ), ItemSelectionModel.ClearAndSelect)
    }

    function updateItemSelectionVertical(u,d) {
        timelineSelection.select(helpers.createItemSelection(
                theSessionData.modifyItemSelectionVertical(timelineSelection.selectedIndexes, u, d)
            ), ItemSelectionModel.ClearAndSelect)
    }

    XsMenuModelItem {
        text: "Move Up"
        menuPath: "Select"
        menuItemPosition: 1
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.select_up_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(1, -1)
    }

    XsMenuModelItem {
        text: "Move Down"
        menuPath: "Select"
        menuItemPosition: 2
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.select_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(-1, 1)
    }

    XsMenuModelItem {
        text: "Expand Up"
        menuPath: "Select"
        menuItemPosition: 3
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_up_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(1, 0)
    }

    XsMenuModelItem {
        text: "Expand Down"
        menuPath: "Select"
        menuItemPosition: 4
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(0, 1)
    }

    XsMenuModelItem {
        text: "Contract Up"
        menuPath: "Select"
        menuItemPosition: 5
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_up_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(-1, 0)
    }

    XsMenuModelItem {
        text: "Contract Down"
        menuPath: "Select"
        menuItemPosition: 6
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(0, -1)
    }

    XsMenuModelItem {
        text: "Expand Up And Down"
        menuPath: "Select"
        menuItemPosition: 7
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_up_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(1, 1)
    }

    XsMenuModelItem {
        text: "Contract Up And Down"
        menuPath: "Select"
        menuItemPosition: 8
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_up_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(-1, -1)
    }



    function updateFlags() {
        if(currentTrackIndex) {
            let m = currentTrackIndex.model
            disabledTrack.isChecked = !m.get(currentTrackIndex, "enabledRole")
            lockedTrack.isChecked= m.get(currentTrackIndex, "lockedRole")
        }
        debug_menu.setMenuPathPosition("Debug", 40)
    }

    XsMenuModelItem {
        text: qsTr("Rename Track...")
        menuPath: ""
        menuItemPosition: 2
        menuModelName: timelineMenu.menu_model_name
        property var currentIndex: null
        onActivated: {
            let indexes = timelineSelection.selectedIndexes
            for(let i=0;i<indexes.length; i++) {
                currentIndex = indexes[i]
                dialogHelpers.textInputDialog(
                    acceptResult,
                    "Rename Track",
                    "Enter Track Name.",
                    theSessionData.get(indexes[i], "nameRole"),
                    ["Cancel", "Rename"])
            }
        }

        function acceptResult(new_name, button) {
            if (button == "Rename") {
                theTimeline.setItemName(currentIndex, new_name)
            }
        }
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Duplicate Tracks")
        menuPath: ""
        menuItemPosition: 4
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.duplicateTracks(timelineSelection.selectedIndexes)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Flatten Selected Tracks")
        menuPath: ""
        menuItemPosition: 6
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            theSessionData.bakeTimelineItems(timelineSelection.selectedIndexes)
            theTimeline.deleteItems(timelineSelection.selectedIndexes)
        }
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Insert Track Above")
        menuPath: ""
        menuItemPosition: 8
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.insertTrackAbove(timelineSelection.selectedIndexes)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 10
        menuModelName: timelineMenu.menu_model_name
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 20
        menuModelName: timelineMenu.menu_model_name
    }

    XsFlagMenuInserter {
        text: qsTr("Set Track Colour")
        menuModelName: timelineMenu.menu_model_name
        menuPath: ""
        menuPosition: 22
        onFlagSet: theTimeline.flagItems(timelineSelection.selectedIndexes, flag == "#00000000" ? "": flag)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 24
        menuModelName: timelineMenu.menu_model_name
    }

    XsMenuModelItem {
        text: qsTr("Set As Conform Track")
        menuPath: ""
        menuItemPosition: 26
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.conformSourceIndex = helpers.makePersistent(currentTrackIndex)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        id: disabledTrack
        text: qsTr("Disable Tracks")
        menuItemType: "toggle"
        menuPath: ""
        menuItemPosition: 26.5
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            theTimeline.enableItems(timelineSelection.selectedIndexes, isChecked)
            isChecked = !isChecked
        }
        isChecked: false
        panelContext: timelineMenu.panelContext

    }

    XsMenuModelItem {
        id: lockedTrack
        text: qsTr("Lock Tracks")
        menuItemType: "toggle"
        menuPath: ""
        menuItemPosition: 28
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            theTimeline.lockItems(timelineSelection.selectedIndexes, !isChecked)
            isChecked = !isChecked
        }
        isChecked: false
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Move Track Up")
        menuPath: ""
        menuItemPosition: 30
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.moveItems(timelineSelection.selectedIndexes, -1)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Move Track Down")
        menuPath: ""
        menuItemPosition: 32
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.moveItems(timelineSelection.selectedIndexes, 1)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: "Snippet"
        menuItemType: "divider"
        menuItemPosition: 32.5
        menuPath: ""
        menuModelName: timelineMenu.menu_model_name
    }

    Repeater {
        model: DelegateModel {
            model: embeddedPython.trackMenuModel
            delegate: Item {XsMenuModelItem {
                text: nameRole
                menuPath: menuPathRole
                menuItemPosition: (index*0.01)+32.5
                menuModelName: timelineMenu.menu_model_name
                onActivated: embeddedPython.pyEvalFile(scriptPathRole)
            }}
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 34
        menuModelName: timelineMenu.menu_model_name
    }

    XsMenuModelItem {
        text: qsTr("Remove Selected Tracks")
        menuPath: ""
        menuItemPosition: 36
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.deleteItems(timelineSelection.selectedIndexes)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 38
        menuModelName: timelineMenu.menu_model_name
    }

    XsMenuModelItem {
        id: debug_menu
        text: qsTr("Dump JSON")
        menuPath: "Debug"
        menuItemPosition: 0
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            for(let i=0;i<timelineSelection.selectedIndexes.length;i++) {
                console.log(timelineSelection.selectedIndexes[i])
                console.log(timelineSelection.selectedIndexes[i].model)
                console.log(timelineSelection.selectedIndexes[i].model.get(timelineSelection.selectedIndexes[i], "jsonTextRole"))
            }
        }
        panelContext: timelineMenu.panelContext

    }
}
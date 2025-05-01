// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Qt.labs.qmlmodels

import xstudio.qml.models 1.0
import xStudio 1.0


XsPopupMenu {

    id: timelineMenu
    visible: false
    menu_model_name: "timeline_clip_menu_"

    property var panelContext: helpers.contextPanel(timelineMenu)
    property var theTimeline: panelContext.theTimeline
    property var timelineSelection: theTimeline.timelineSelection

    property var debugSetMenuPathPosition: debug_menu.setMenuPathPosition

    property var currentClipIndex: timelineSelection.selectedIndexes.length ? helpers.makePersistent(timelineSelection.selectedIndexes[0]) : null

    onCurrentClipIndexChanged: {
        if(currentClipIndex && currentClipIndex.valid) {
            let m = currentClipIndex.model
            disabledClip.isChecked = !m.get(currentClipIndex, "enabledRole")
            lockedClip.isChecked= m.get(currentClipIndex, "lockedRole")
        }
    }

    Component.onCompleted: {
        // need to reorder snippet menus..
        let rc = embeddedPython.clipMenuModel.rowCount();
        for(let i=0; i < embeddedPython.clipMenuModel.rowCount(); i++) {
            let fi = embeddedPython.clipMenuModel.index(i, 0)
            let si = embeddedPython.clipMenuModel.mapToSource(fi)
            let mp = si.model.get(si, "menuPathRole")
            helpers.setMenuPathPosition(mp,"timeline_clip_menu_", 81 + ((1.0/rc)*i) )
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


    function getOffline() {
        let tindex = theSessionData.getTimelineIndex(theTimeline.timelineModel.rootIndex)
        let mlist = theSessionData.index(0, 0, tindex)
        let clips = theSessionData.searchRecursiveList("Clip", "typeRole", theTimeline.timelineModel.rootIndex,0,-1,-1)
        // with media uuid
        let clipsWithBadMedia = []
        for(let i = 0; i< clips.length; i++) {
            let cmu = theSessionData.get(clips[i], "clipMediaUuidRole")
            if(cmu != undefined && cmu != "{00000000-0000-0000-0000-000000000000}") {
                // test media..
                // locate media index..
                let mindex = theSessionData.search(cmu, "actorUuidRole", mlist)
                if(mindex.valid) {
                    // check media offline
                    let state = theSessionData.get(mindex, "mediaStatusRole")
                    if(state != undefined && state != "Online")
                        clipsWithBadMedia.push(clips[i])
                } else {
                    clipsWithBadMedia.push(clips[i])
                }
            }
        }
        return clipsWithBadMedia
    }

    function getInvalid() {
        let invalidClips = theSessionData.searchRecursiveList(false, "activeRangeValidRole", theTimeline.timelineModel.rootIndex,0,-1,-1)
        let offlineClips = getOffline();
        return invalidClips.filter(value => !offlineClips.includes(value));
    }

    XsMenuModelItem {
        text: "Select None"
        menuPath: "Select"
        menuItemPosition: 0.3
        menuModelName: timelineMenu.menu_model_name
        panelContext: timelineMenu.panelContext
        onActivated: timelineSelection.select(helpers.createItemSelection([]), ItemSelectionModel.ClearAndSelect)
    }

    XsMenuModelItem {
        text: "Select Offline/Invalid"
        menuPath: "Select"
        menuItemPosition: 0.4
        menuModelName: timelineMenu.menu_model_name
        panelContext: timelineMenu.panelContext
        onActivated: timelineSelection.select(helpers.createItemSelection(getOffline().concat(getInvalid())), ItemSelectionModel.ClearAndSelect)
    }

    XsMenuModelItem {
        text: "Select Offline"
        menuPath: "Select"
        menuItemPosition: 0.5
        menuModelName: timelineMenu.menu_model_name
        panelContext: timelineMenu.panelContext
        onActivated: timelineSelection.select(helpers.createItemSelection(getOffline()), ItemSelectionModel.ClearAndSelect)
    }

    XsMenuModelItem {
        text: "Select Invalid"
        menuPath: "Select"
        menuItemPosition: 0.6
        menuModelName: timelineMenu.menu_model_name
        panelContext: timelineMenu.panelContext
        onActivated: timelineSelection.select(helpers.createItemSelection(getInvalid()), ItemSelectionModel.ClearAndSelect)
    }

    XsMenuModelItem {
        text: "Select Next"
        menuPath: "Select"
        menuItemPosition: 1
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.select_next_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(-1, 1)
    }

    XsMenuModelItem {
        text: "Select Previous"
        menuPath: "Select"
        menuItemPosition: 2
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.select_previous_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(1, -1)
    }

    XsMenuModelItem {
        text: "Select Around"
        menuPath: "Select"
        menuItemPosition: 2.5
        menuModelName: timelineMenu.menu_model_name
        // hotkeyUuid: theTimeline.select_next_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: {
            let items = [].concat(timelineSelection.selectedIndexes)
            let newitems = []
            for(let i = 0; i < items.length; i++) {
                let expanded = theSessionData.modifyItemSelectionHorizontal([items[i]], 1, 1)
                newitems = newitems.concat(
                    expanded.filter(value => !newitems.includes(value))
                )
            }
            timelineSelection.select(helpers.createItemSelection(newitems), ItemSelectionModel.ClearAndSelect)
        }
    }

    XsMenuModelItem {
        text: "Expand Next Selection"
        menuPath: "Select"
        menuItemPosition: 3
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_next_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(0,+1)
    }

    XsMenuModelItem {
        text: "Expand Previous Selection"
        menuPath: "Select"
        menuItemPosition: 4
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_previous_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(1, 0)
    }

    XsMenuModelItem {
        text: "Contract Next Selection"
        menuPath: "Select"
        menuItemPosition: 5
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_next_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(0, -1)
    }

    XsMenuModelItem {
        text: "Contract Previous Selection"
        menuPath: "Select"
        menuItemPosition: 6
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_previous_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(-1, 0)
    }

    XsMenuModelItem {
        text: "Expand Selection"
        menuPath: "Select"
        menuItemPosition: 7
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_both_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(1, 1)
    }

    XsMenuModelItem {
        text: "Contract Selection"
        menuPath: "Select"
        menuItemPosition: 8
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_both_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionHorizontal(-1, -1)
    }

    XsMenuModelItem {
        text: "Selection Up"
        menuPath: "Select"
        menuItemPosition: 10
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.select_up_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(1, -1)
    }

    XsMenuModelItem {
        text: "Selection Down"
        menuPath: "Select"
        menuItemPosition: 11
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.select_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(-1, 1)
    }

    XsMenuModelItem {
        text: "Expand Up"
        menuPath: "Select"
        menuItemPosition: 12
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_up_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(1, 0)
    }

    XsMenuModelItem {
        text: "Expand Down"
        menuPath: "Select"
        menuItemPosition: 13
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(0, 1)
    }

    XsMenuModelItem {
        text: "Contract Up"
        menuPath: "Select"
        menuItemPosition: 14
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_up_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(-1, 0)
    }

    XsMenuModelItem {
        text: "Contract Down"
        menuPath: "Select"
        menuItemPosition: 14
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(0, -1)
    }

    XsMenuModelItem {
        text: "Expand Up And Down"
        menuPath: "Select"
        menuItemPosition: 15
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.expand_up_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(1, 1)
    }

    XsMenuModelItem {
        text: "Contract Up And Down"
        menuPath: "Select"
        menuItemPosition: 17
        menuModelName: timelineMenu.menu_model_name
        hotkeyUuid: theTimeline.contract_up_down_hotkey.uuid
        panelContext: timelineMenu.panelContext
        onActivated: updateItemSelectionVertical(-1, -1)
    }


    XsFlagMenuInserter {
        // panelContext: timelineMenu.panelContext
        text: qsTr("Media Colour")
        menuModelName: timelineMenu.menu_model_name
        menuPath: ""
        menuPosition: 2
        onFlagSet: (flag, flag_text) => {
            let sindexs = timelineSelection.selectedIndexes
            if(sindexs.length) {
                let m = sindexs[0].model
                let pindex = m.getPlaylistIndex(sindexs[0])
                let mlist = m.index(0, 0, pindex)

                for(let i = 0; i< sindexs.length; i++) {
                    let mediaIndex = m.search(m.get(sindexs[i],"clipMediaUuidRole"), "actorUuidRole", mlist)

                    m.set(mediaIndex, flag, "flagColourRole")
                    if (flag_text)
                        m.set(mediaIndex, flag_text, "flagTextRole")
                }
            }
        }
        panelContext: timelineMenu.panelContext
    }

    XsFlagMenuInserter {
        text: qsTr("Clip Colour")
        menuModelName: timelineMenu.menu_model_name
        menuPath: ""
        menuPosition: 3
        onFlagSet: (flag, flag_text) => {
            theTimeline.flagItems(timelineSelection.selectedIndexes, flag == "#00000000" ? "": flag)
        }
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 4
        menuModelName: timelineMenu.menu_model_name
    }

    XsMenuModelItem {
        text: qsTr("Flatten To New Track")
        menuPath: ""
        menuItemPosition: 5
        menuModelName: timelineMenu.menu_model_name
        onActivated: theSessionData.bakeTimelineItems(timelineSelection.selectedIndexes)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Duplicate")
        menuPath: ""
        menuItemPosition: 6
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.duplicateClips(timelineSelection.selectedIndexes)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Move Left")
        menuPath: ""
        menuItemPosition: 6.1
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            let ordered = [].concat(timelineSelection.selectedIndexes)
            ordered.sort((a,b) => a.row - b.row)
            for(let i=0; i < ordered.length; i++) {
                theTimeline.moveItem(ordered[i], -1)
            }
        }
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        text: qsTr("Move Right")
        menuPath: ""
        menuItemPosition: 6.2
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            let ordered = [].concat(timelineSelection.selectedIndexes)
            ordered.sort((b,a) => a.row - b.row)
            for(let i=0; i < ordered.length; i++) {
                theTimeline.moveItem(ordered[i], 1)
            }
        }
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 7
        menuModelName: timelineMenu.menu_model_name
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 9
        menuModelName: timelineMenu.menu_model_name
    }


    XsMenuModelItem {
        id: disabledClip
        text: qsTr("Disable Clips")
        menuPath: ""
        menuItemType: "toggle"
        menuItemPosition: 10
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            theTimeline.enableItems(timelineSelection.selectedIndexes, isChecked)
            isChecked = !isChecked
        }
        isChecked: false
        panelContext: timelineMenu.panelContext
    }

   XsMenuModelItem {
        id: lockedClip
        text: qsTr("Lock Clips")
        menuPath: ""
        menuItemType: "toggle"
        menuItemPosition: 11
        menuModelName: timelineMenu.menu_model_name
        onActivated: {
            theTimeline.lockItems(timelineSelection.selectedIndexes, !isChecked)
            isChecked = !isChecked
        }
        isChecked: false
        panelContext: timelineMenu.panelContext
    }


    XsMenuModelItem {
        text: "Snippet"
        menuItemType: "divider"
        menuItemPosition: 80
        menuPath: ""
        menuModelName: timelineMenu.menu_model_name
    }

    Repeater {
        model: DelegateModel {
            model: embeddedPython.clipMenuModel
            delegate: Item {XsMenuModelItem {
                text: nameRole
                menuPath: menuPathRole
                menuItemPosition: (index*0.01)+80
                menuModelName: timelineMenu.menu_model_name
                onActivated: embeddedPython.pyEvalFile(scriptPathRole)
            }}
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 90
        menuModelName: timelineMenu.menu_model_name
    }


    XsMenuModelItem {
        text: qsTr("Remove Clips")
        menuPath: ""
        menuItemPosition: 91
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.deleteItems(timelineSelection.selectedIndexes)
        panelContext: timelineMenu.panelContext
    }

    XsMenuModelItem {
        id: debug_menu
        text: qsTr("Dump JSON")
        menuPath: ""
        menuItemPosition: 95
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
// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.qmlmodels

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import QuickFuture 1.0

XsListView {
	id: control

    spacing:1

    property int rightSpacing: control.height < control.contentHeight ? 12 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}

    ScrollBar.vertical: XsScrollBar {
        visible: control.height < control.contentHeight
        parent: control.parent
        anchors.top: control.top
        anchors.right: control.right
        anchors.bottom: control.bottom
        x: -5
    }

    property bool isDragging: false
    property int draggingOffset: 0
    property int draggingY: 0

    model: presetsTreeModel

    delegate: DelegateChooser {
        id: chooser
        role: "typeRole"

        DelegateChoice {
            roleValue: "group";
            XsSBPresetGroupDelegate{
                width: control.width - control.rightSpacing
                height: btnHeight-2
                delegateModel: control.model
                expandedModel: presetsExpandedModel
                selectionModel: presetsSelectionModel
            }
        }
        DelegateChoice {
            roleValue: "preset";
            XsSBPresetDelegate{
                width: control.width - height - control.rightSpacing
                height: btnHeight-2
                delegateModel: control.model
                selectionModel: presetsSelectionModel
            }
        }
        DelegateChoice {
            roleValue: "presets";
            Rectangle {
                visible: false
                width: control.width - control.rightSpacing
                height: -1
            }
        }
        DelegateChoice {
            roleValue: "term";
            Rectangle {
                visible: true
                width: control.width - control.rightSpacing
                height: btnHeight-2
            }
        }
    }

    Connections {
        target: presetsExpandedModel
        function onSelectionChanged(selected, deselected) {
            // console.log("presetsExpandedModel onSelectionChanged", selected, deselected)
            // update tree model with expand
            // map to from.. ack..
            for(let i = 0; i<selected.length;i++) {
                let ri = helpers.createListFromRange(selected[i])
                for(let ii =0;ii < ri.length; ii++) {
                    let fi = presetsTreeModel.model.mapFromSource(ri[ii])
                    let ti = presetsTreeModel.mapFromModel(fi)
                    if(ti.valid) {
                        presetsTreeModel.expandRow(ti.row)
                    }
                }
            }

            for(let i = 0; i<deselected.length;i++) {
                let ri = helpers.createListFromRange(deselected[i])
                for(let ii =0;ii < ri.length; ii++) {
                    let fi = presetsTreeModel.model.mapFromSource(ri[ii])
                    let ti = presetsTreeModel.mapFromModel(fi)
                    if(ti.valid) {
                        presetsTreeModel.collapseRow(ti.row)
                    }
                }
            }
        }
    }

    XsSBPresetEditPopup {
        id: presetEditPopup
    }

    XsPopupMenu {
        id: presetMenu
        property var presetModelIndex: null
        property var filterModelIndex: null
        visible: false
        menu_model_name: "presetMenu"+control

        XsMenuModelItem {
            text: "Duplicate Presets"
            menuItemPosition: 1
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            onActivated: {
                let indexs = []
                for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                    indexs.push(helpers.makePersistent(presetsSelectionModel.selectedIndexes[i]))

                for(let i=0;i<indexs.length; i++)
                    ShotBrowserEngine.presetsModel.duplicate(indexs[i])
            }
        }

        XsMenuModelItem {
            text: "Copy Selected Presets"
            menuItemPosition: 2
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            onActivated: clipboard.text = JSON.stringify(ShotBrowserEngine.presetsModel.copy(presetsSelectionModel.selectedIndexes))
        }

        XsMenuModelItem {
            text: "Paste Preset"
            menuItemPosition: 3
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            onActivated: ShotBrowserEngine.presetsModel.paste(
                JSON.parse(clipboard.text),
                presetMenu.presetModelIndex.row+1,
                presetMenu.presetModelIndex.parent
            )
        }

        XsMenuModelItem {
            text: "Reset Presets"
            menuItemPosition: 3.5
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            enabled: presetMenu.presetModelIndex ? ShotBrowserEngine.presetsModel.get(presetMenu.presetModelIndex, "updateRole") : false
            onActivated: {
                let indexs = []
                for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                    indexs.push(helpers.makePersistent(presetsSelectionModel.selectedIndexes[i]))

                ShotBrowserEngine.presetsModel.resetPresets(
                    indexs
                )
            }
        }

        XsMenuModelItem {
            text: "Remove Presets"
            menuItemPosition: 4
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            onActivated: {
                let indexs = []
                for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                    indexs.push(helpers.makePersistent(presetsSelectionModel.selectedIndexes[i]))

                for(let i=0;i<indexs.length; i++) {
                    let m = indexs[i].model
                    let sys = m.get(indexs[i], "updateRole")
                    if(sys != undefined) {
                        m.set(indexs[i], true, "hiddenRole")
                    } else {
                        ShotBrowserEngine.presetsModel.removeRows(indexs[i].row, 1, indexs[i].parent)
                    }
                }
            }
        }


        XsMenuModelItem {
            menuItemPosition: 5
            menuItemType: "divider"
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
        }

        XsMenuModelItem {
            text: "Move Up"
            menuItemPosition: 6
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            enabled: presetMenu.filterModelIndex && presetMenu.filterModelIndex.row
            onActivated: {
                // because we use a view, the previous item in the base model isn't
                // the previous in the view..
                let indexs = []
                for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                    indexs.push(helpers.makePersistent(presetMenu.filterModelIndex.model.mapFromSource(presetsSelectionModel.selectedIndexes[i])))

                indexs.sort((a, b) =>  a.row - b.row)

                for(let i=0; i < indexs.length; i++) {
                    let mi = indexs[i].model.mapToSource(indexs[i])
                    let p = mi.parent
                    let rpi = indexs[i].model.mapToSource(
                    	indexs[i].model.index(indexs[i].row-1, 0, indexs[i].parent)
                    )
                    ShotBrowserEngine.presetsModel.moveRows(p, mi.row, 1, p, rpi.row)
                }
            }
        }


        XsMenuModelItem {
            text: "Move Down"
            menuItemPosition: 7
            menuPath: ""
            menuModelName: presetMenu.menu_model_name
            enabled: presetMenu.filterModelIndex && presetMenu.filterModelIndex ? presetMenu.filterModelIndex.row != presetMenu.filterModelIndex.model.rowCount(presetMenu.filterModelIndex.parent) - 1 : false
            onActivated: {
                // because we use a view, the previous item in the base model isn't
                // the previous in the view..
                let indexs = []
                for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                    indexs.push(helpers.makePersistent(presetMenu.filterModelIndex.model.mapFromSource(presetsSelectionModel.selectedIndexes[i])))

                indexs.sort((a, b) =>  b.row - a.row)

                for(let i=0; i < indexs.length; i++) {
                    let mi = indexs[i].model.mapToSource(indexs[i])
                    let p = mi.parent
                    let rpi = indexs[i].model.mapToSource(
                        indexs[i].model.index(indexs[i].row+1, 0, indexs[i].parent)
                    )
                    ShotBrowserEngine.presetsModel.moveRows(p, mi.row, 1, p, rpi.row+1)
                }
            }
        }

        // XsMenuModelItem {
        //     text: "Reset Preset"
        //     menuPath: ""
        //     menuModelName: presetMenu.menu_model_name
        //     enabled: {
        //     	if(presetMenu.presetModelIndex) {
        //     		let v = presetMenu.presetModelIndex.model.get(presetMenu.presetModelIndex,"updateRole")
        //     		return v != undefined ? v : false
        //     	}
        //     	return false
        //     }
        //     onActivated: ShotBrowserEngine.presetsModel.resetPresets([presetMenu.presetModelIndex])
        // }
    }

    XsPopupMenu {
        id: groupMenu

        property var presetModelIndex: null
        property var filterModelIndex: null
        property var flags: force_update ? updateFlags() : updateFlags()
        property bool force_update: false

        visible: false
        menu_model_name: "groupMenu"+control

        function updateFlags() {
            if(groupMenu.presetModelIndex != null) {
                let tmp = ShotBrowserEngine.presetsModel.get(groupMenu.presetModelIndex, "flagRole")
                if(tmp != undefined)
                    return tmp
            }
            return []
        }

        Component.onCompleted: {
            // make sure the 'Add' sub-menu appears in the correct place
            helpers.setMenuPathPosition("Behaviour", groupMenu.menu_model_name, 5.1)
        }


        function getSelectedGroupsLocal() {
            let indexes = []
            for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                if(ShotBrowserEngine.presetsModel.get(presetsSelectionModel.selectedIndexes[i], "typeRole") == "group")
                    indexes.push(helpers.makePersistent(groupMenu.filterModelIndex.model.mapFromSource(presetsSelectionModel.selectedIndexes[i])))

            return indexes
        }

        function getSelectedGroups() {
            let indexes = []
            for(let i=0;i<presetsSelectionModel.selectedIndexes.length; i++)
                if(ShotBrowserEngine.presetsModel.get(presetsSelectionModel.selectedIndexes[i], "typeRole") == "group")
                    indexes.push(helpers.makePersistent(presetsSelectionModel.selectedIndexes[i]))

            return indexes
        }

        XsMenuModelItem {
            text: "Duplicate Groups"
            menuItemPosition: 1
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: {
                let indexs = groupMenu.getSelectedGroups()
                indexs.sort((a, b) =>  a.row - b.row)
                let oldy = control.contentY
                for(let i=0; i < indexs.length; i++) {
                    groupMenu.presetModelIndex.model.duplicate(indexs[i])
                }
                control.contentY = oldy
            }
        }

        XsMenuModelItem {
            text: "Export As System Preset Group..."
            menuPath: ""
            menuItemPosition: 1.5
            menuModelName: groupMenu.menu_model_name
            onActivated: dialogHelpers.showFileDialog(
                    function(fileUrl, undefined, func) {
                        if(fileUrl)
                            Future.promise(
                                ShotBrowserEngine.presetsModel.exportAsSystemPresetsFuture(
                                    fileUrl,
                                    groupMenu.presetModelIndex
                                )
                            ).then(function(string) {
                                    dialogHelpers.errorDialogFunc("Export As System Presets", "Export As System Presets complete.\n\n" + string)
                                },
                                function(err) {
                                    dialogHelpers.errorDialogFunc("Export As System Presets", "Export As System Presets failed.\n\n" + err)
                                }
                            )
                    },
                    file_functions.defaultSessionFolder(),
                    "Export Presets",
                    "json",
                    ["JSON (*.json)"],
                    false,
                    false
                )
        }

        XsMenuModelItem {
            text: "Copy Group"
            menuItemPosition: 2
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: clipboard.text = JSON.stringify(ShotBrowserEngine.presetsModel.copy([groupMenu.presetModelIndex]))
        }
        XsMenuModelItem {
            text: "Paste Group"
            menuItemPosition: 3
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: ShotBrowserEngine.presetsModel.paste(
                JSON.parse(clipboard.text),
                groupMenu.presetModelIndex.row+1,
                groupMenu.presetModelIndex.parent
            )
        }
        XsMenuModelItem {
            text: "Remove Groups"
            menuItemPosition: 4
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: {
                let gindex = groupMenu.getSelectedGroups()
                gindex.sort((a, b) =>  a.row - b.row)

                for(let g=0; g < gindex.length; g++) {
                    let sys = ShotBrowserEngine.presetsModel.get(gindex[g], "updateRole")
                    if(sys != undefined) {
                        ShotBrowserEngine.presetsModel.set(gindex[g], true, "hiddenRole")
                    } else {
                        ShotBrowserEngine.presetsModel.removeRows(gindex[g].row, 1, gindex[g].parent)
                    }
                }
            }
        }

        XsMenuModelItem {
            text: "Reset Presets"
            menuItemPosition: 4.5
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: {
                let gindex = groupMenu.getSelectedGroups()
                for(let g=0; g < gindex.length; g++) {

                    let indexs = []
                    let ind = ShotBrowserEngine.presetsModel.index(1, 0, gindex[g])
                    for(let i=0;i<ShotBrowserEngine.presetsModel.rowCount(ind); i++)
                        indexs.push(helpers.makePersistent(ShotBrowserEngine.presetsModel.index(i,0,ind)))

                    ShotBrowserEngine.presetsModel.resetPresets(
                        indexs
                    )
                }
            }
        }
        XsMenuModelItem {
            menuItemPosition: 5
            menuItemType: "divider"
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
        }

        Repeater {
            model: ShotBrowserEngine.presetsModel.groupFlags

            Item {
                XsMenuModelItem {
                    text: modelData
                    menuItemType: "toggle"
                    menuPath: "Behaviour"
                    menuItemPosition: index + 1
                    menuModelName: groupMenu.menu_model_name
                    isChecked: groupMenu.flags.includes(modelData)

                    onActivated: {
                        if(isChecked) {
                            ShotBrowserEngine.presetsModel.set(groupMenu.presetModelIndex, Array.from(groupMenu.flags).filter(r => r !== modelData), "flagRole")
                        } else {
                            let tmp = groupMenu.flags
                            tmp.push(modelData)
                            ShotBrowserEngine.presetsModel.set(groupMenu.presetModelIndex, tmp, "flagRole")
                        }
                        groupMenu.force_update = !groupMenu.force_update
                    }
                }
            }
        }

        XsMenuModelItem {
            text: "Paste Preset"
            menuItemPosition: 6
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: {
                ShotBrowserEngine.presetsModel.paste(
                    JSON.parse(clipboard.text),
                    ShotBrowserEngine.presetsModel.rowCount(ShotBrowserEngine.presetsModel.index(1,0,groupMenu.presetModelIndex)),
                    ShotBrowserEngine.presetsModel.index(1,0,groupMenu.presetModelIndex)
                )
            }
        }

        XsMenuModelItem {
            menuItemPosition: 7
            menuItemType: "divider"
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
        }

        XsMenuModelItem {
            text: "Move Groups Up"
            menuItemPosition: 8
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            enabled: groupMenu.filterModelIndex && groupMenu.filterModelIndex.row
            onActivated: {
                let indexs = groupMenu.getSelectedGroupsLocal()
                indexs.sort((a, b) =>  a.row - b.row)
                let oldy = control.contentY

                for(let i=0; i < indexs.length; i++) {
                    let mi = indexs[i].model.mapToSource(indexs[i])
                    let p = mi.parent
                    let rpi = indexs[i].model.mapToSource(
                        indexs[i].model.index(indexs[i].row-1, 0, indexs[i].parent)
                    )
                    ShotBrowserEngine.presetsModel.moveRows(p, mi.row, 1, p, rpi.row)
                }
                control.contentY = oldy
            }
        }

        XsMenuModelItem {
            text: "Move Groups Down"
            menuItemPosition: 9
            menuPath: ""
            menuModelName: groupMenu.menu_model_name

            enabled: groupMenu.filterModelIndex ? groupMenu.filterModelIndex.row != groupMenu.filterModelIndex.model.rowCount(groupMenu.filterModelIndex.parent) - 1 : false
            onActivated: {
                let indexs = groupMenu.getSelectedGroupsLocal()
                indexs.sort((a, b) =>  b.row - a.row)

                let oldy = control.contentY
                for(let i=0; i < indexs.length; i++) {
                    let mi = indexs[i].model.mapToSource(indexs[i])
                    let p = mi.parent
                    let rpi = indexs[i].model.mapToSource(
                        indexs[i].model.index(indexs[i].row+1, 0, indexs[i].parent)
                    )
                    ShotBrowserEngine.presetsModel.moveRows(p, mi.row, 1, p, rpi.row+1)
                }
                control.contentY = oldy
            }
        }
    }
}

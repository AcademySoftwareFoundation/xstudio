// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

XsListView {
	id: control

    spacing:1

    property int rightSpacing: control.height < control.contentHeight ? 12 : 0
    Behavior on rightSpacing {NumberAnimation {duration: 150}}

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
                x: height
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
            // console.log("presetsExpandedModel onSelectionChanged")
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

        visible: false
        menu_model_name: "groupMenu"+control

        XsMenuModelItem {
            text: "Duplicate Group"
            menuItemPosition: 1
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: groupMenu.presetModelIndex.model.duplicate(groupMenu.presetModelIndex)
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
            text: "Remove Group"
            menuItemPosition: 4
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            onActivated: {
                let m = groupMenu.presetModelIndex.model
                let sys = m.get(groupMenu.presetModelIndex, "updateRole")
                if(sys != undefined) {
                    m.set(groupMenu.presetModelIndex, true, "hiddenRole")
                } else {
                    ShotBrowserEngine.presetsModel.removeRows(groupMenu.presetModelIndex.row, 1, groupMenu.presetModelIndex.parent)
                }
            }
        }
        XsMenuModelItem {
            menuItemPosition: 5
            menuItemType: "divider"
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
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
            text: "Move Up"
            menuItemPosition: 8
            menuPath: ""
            menuModelName: groupMenu.menu_model_name
            enabled: groupMenu.filterModelIndex && groupMenu.filterModelIndex.row
            onActivated: {

                // argh this is horridly complex..
                // because we use a view, the previous item in the base model isn't
                // the previous in the view..
                let p = groupMenu.presetModelIndex.parent
                let rpi = groupMenu.filterModelIndex.model.mapToSource(
                	groupMenu.filterModelIndex.model.index(groupMenu.filterModelIndex.row-1,0,groupMenu.filterModelIndex.parent)
                )

                let oldy = control.contentY
                ShotBrowserEngine.presetsModel.moveRows(p, groupMenu.presetModelIndex.row, 1, p, rpi.row)
                control.contentY = oldy


            }
        }

        XsMenuModelItem {
            text: "Move Down"
            menuItemPosition: 9
            menuPath: ""
            menuModelName: groupMenu.menu_model_name

            enabled: groupMenu.filterModelIndex ? groupMenu.filterModelIndex.row != groupMenu.filterModelIndex.model.rowCount(groupMenu.filterModelIndex.parent) - 1 : false
            onActivated: {
                let p = groupMenu.presetModelIndex.parent
                let rpi = groupMenu.filterModelIndex.model.mapToSource(
                	groupMenu.filterModelIndex.model.index(groupMenu.filterModelIndex.row+1,0,groupMenu.filterModelIndex.parent)
                )
                let oldy = control.contentY
                ShotBrowserEngine.presetsModel.moveRows(p, groupMenu.presetModelIndex.row, 1, p, rpi.row+1)
                control.contentY = oldy
            }
        }
        // XsMenuModelItem {
        //     text: (groupMenu.presetModelIndex && groupMenu.presetModelIndex.model.get(groupMenu.presetModelIndex,"hiddenRole") ? "Show" : "Hide") + " Group"
        //     menuPath: ""
        //     menuModelName: groupMenu.menu_model_name
        //     onActivated: {
        //     	let m = groupMenu.presetModelIndex.model
        //     	let i = groupMenu.presetModelIndex
        //     	m.set(i, !m.get(i,"hiddenRole"),"hiddenRole")
        //     }
        // }
    }
}

// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.clipboard 1.0

XsWindow{
    id: presetEditPopup

    property var presetIndex: helpers.qModelIndex()
    property string entityType: "Versions"
    property string entityName: ""
    property string entityCategory: "Group"

    property int itemHeight: 24

    property bool isSystemPreset: false

    property int dragWidth: btnWidth/2
    property int termWidth: btnWidth*4
    property int modeWidth: itemHeight
    property int closeWidth: btnWidth

    minimumWidth: 500
    minimumHeight: 400

    width: 460
    height: 40 + 200 + (nameDiv.height + coln.spacing*2) //+ presetList.height
    // transientParent: parent
    // remove focus from text widgets.
    onClosing: coln.focus = true

    onPresetIndexChanged: {
        if(presetIndex.valid) {
            presetTermModel.model = null
            presetTermModel.model = ShotBrowserEngine.presetsModel
            presetTermModel.rootIndex = presetIndex
            presetList.model.newTermParent = presetIndex
            isSystemPreset = ShotBrowserEngine.presetsModel.get(presetIndex, "updateRole") != undefined
        }
    }

    QTreeModelToTableModel {
        id: presetTermModel
        model: ShotBrowserEngine.presetsModel
    }

    ItemSelectionModel {
        id: termSelection
        model: ShotBrowserEngine.presetsModel
    }

    ColumnLayout { id: coln
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Item{ id: nameDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight*1.2

            RowLayout {
                width: parent.width
                height: parent.height
                spacing: 1

                Item{
                    Layout.preferredWidth: dragWidth
                    Layout.fillHeight: true
                }
                Item {
                    Layout.preferredWidth: height
                    Layout.fillHeight: true
                }
                XsText{
                    Layout.preferredWidth: termWidth
                    Layout.fillHeight: true
                    text: entityCategory+" name:"
                }
                Item {
                    Layout.preferredWidth: modeWidth
                    Layout.fillHeight: true
                }
                XsTextField{ id: nameEditDiv
                    Layout.fillWidth: true
                    Layout.preferredWidth: btnWidth*4.2
                    Layout.fillHeight: true
                    text: entityName
                    placeholderText: entityName
                    onEditingFinished: {
                        if(ShotBrowserEngine.presetsModel.get(presetIndex.parent,"typeRole") == "presets")
                            ShotBrowserEngine.presetsModel.set(presetIndex, text, "nameRole")
                        else
                            ShotBrowserEngine.presetsModel.set(presetIndex.parent, text, "nameRole")
                    }

                    background:
                    Rectangle{
                        color: nameEditDiv.activeFocus? Qt.darker(palette.highlight, 1.5): nameEditDiv.hovered? Qt.lighter(palette.base, 2):Qt.lighter(palette.base, 1.5)
                        border.width: nameEditDiv.hovered || nameEditDiv.active? 1:0
                        border.color: palette.highlight
                        opacity: enabled? 0.7 : 0.3
                    }
                }
                Item{
                    Layout.preferredWidth: closeWidth + itemHeight + 2
                    Layout.fillHeight: true
                }
                XsPrimaryButton{ id: moreBtn
                    Layout.minimumWidth: height
                    Layout.maximumWidth: height
                    Layout.fillHeight: true

                    imgSrc: "qrc:/icons/more_vert.svg"
                    // scale: 0.95
                    isActive: termMenu.visible
                    onClicked:{
                        if(termMenu.visible)
                            termMenu.visible = false
                        else
                            termMenu.showMenu(moreBtn, width/2, height/2)
                    }
                }
            }
        }

        XsListView{ id: presetList
            Layout.fillWidth: true
            Layout.fillHeight: true

            property int rightSpacing: height < contentHeight ? 14 : 0
            Behavior on rightSpacing {NumberAnimation {duration: 150}}

            ScrollBar.vertical: XsScrollBar {
                visible: presetList.height < presetList.contentHeight
                parent: presetList
                anchors.top: presetList.top
                anchors.right: presetList.right
                anchors.bottom: presetList.bottom
                x: -5
            }

            model: DelegateModel {
                id: presetDelegateModel
                property var notifyModel: presetTermModel.rootIndex.valid ? presetTermModel : []
                model: notifyModel

                property var newTermParent: null

                delegate: XsSBPresetEditItem{
                    width: presetList.width - presetList.rightSpacing
                    height: itemHeight
                    termModel: ShotBrowserEngine.presetsModel.termLists[entityType]
                    delegateModel: presetDelegateModel
                }
            }

            interactive: true
            spacing: 1

            footer: Item{
                    width: presetList.width - presetList.rightSpacing
                    height: itemHeight
                    XsSBPresetEditNewItem{
                        anchors.fill: parent
                        anchors.topMargin: 1
                        termModel: ShotBrowserEngine.presetsModel.termLists[entityType]
                }
            }
        }

        RowLayout{
            Layout.fillWidth: true
            Layout.maximumHeight: itemHeight
            Layout.minimumHeight: itemHeight

            // XsLabel{
            //     visible: isSystemPreset
            //     color: palette.highlight
            //     text: "This is a System Preset, changes will not be preserved."
            //     Layout.fillHeight: true
            // }
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true
            }

            XsPrimaryButton{
                Layout.preferredWidth: presetEditPopup.width/3
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight
                text: "Close"
                onClicked: {
                    close()
                }
            }
        }
    }

    XsPopupMenu {
        id: equationMenu
        visible: false
        menu_model_name: "equationMenu"+presetEditPopup
        property var termModelIndex: helpers.qModelIndex()

        property var isLiveLink: ShotBrowserEngine.presetsModel.get(termModelIndex, "livelinkRole")
        property var isNegate: ShotBrowserEngine.presetsModel.get(termModelIndex, "negatedRole")
        property bool isEqual: !isLiveLink && !isNegate

        XsMenuModelItem {
            text: "Equals"
            menuPath: ""
            menuModelName: equationMenu.menu_model_name
            enabled: !equationMenu.isEqual

            onActivated: {
                if(equationMenu.isNegate != undefined && equationMenu.isNegate)
                    ShotBrowserEngine.presetsModel.set(equationMenu.termModelIndex, false, "negatedRole")
                if(equationMenu.isLiveLink != undefined && equationMenu.isLiveLink)
                    ShotBrowserEngine.presetsModel.set(equationMenu.termModelIndex, false, "livelinkRole")
            }
        }
        XsMenuModelItem {
            text: "Negates"
            menuPath: ""
            menuModelName: equationMenu.menu_model_name
            enabled: equationMenu.isNegate != undefined && !equationMenu.isNegate

            onActivated: {
                if(equationMenu.isLiveLink != undefined && equationMenu.isLiveLink)
                    ShotBrowserEngine.presetsModel.set(equationMenu.termModelIndex, false, "livelinkRole")
                ShotBrowserEngine.presetsModel.set(equationMenu.termModelIndex, true, "negatedRole")
            }
        }
        XsMenuModelItem {
            text: "Live Link"
            menuPath: ""
            menuModelName: equationMenu.menu_model_name
            enabled: equationMenu.isLiveLink != undefined && !equationMenu.isLiveLink

            onActivated: {
                ShotBrowserEngine.presetsModel.set(equationMenu.termModelIndex, true, "livelinkRole")

                if(equationMenu.isNegate != undefined && equationMenu.isNegate)
                    ShotBrowserEngine.presetsModel.set(equationMenu.termModelIndex, false, "negatedRole")
            }
        }
    }

    XsPopupMenu {
        id: termTypeMenu
        visible: false
        menu_model_name: "termTypeMenu"+presetEditPopup
        property var termModelIndex: helpers.qModelIndex()

        Repeater {
            model: ShotBrowserEngine.presetsModel.termLists[entityType]
            Item {
                XsMenuModelItem {
                    menuItemType: "button"
                    text: modelData
                    menuPath: ""
                    menuItemPosition: index
                    menuModelName: termTypeMenu.menu_model_name
                    onActivated: {
                        let row = ShotBrowserEngine.presetsModel.rowCount(termTypeMenu.termModelIndex)
                        let i = ShotBrowserEngine.presetsModel.insertTerm(
                            modelData,
                            row,
                            termTypeMenu.termModelIndex
                        )

                        if(i.valid) {
                            let t = ShotBrowserEngine.presetsModel.get(i, "termRole")
                            let tm = ShotBrowserEngine.presetsModel.termModel(t, entityType, projectId)
                            if(tm.length && tm.get(tm.index(0,0), "nameRole") == "True") {
                                ShotBrowserEngine.presetsModel.set(i, "True", "valueRole")
                            }
                        }
                    }
                }
            }
        }
    }

    XsPopupMenu {
        id: termMenu
        visible: false
        menu_model_name: "termMenu"+presetEditPopup

        Clipboard{
          id: clipboard
        }

        XsMenuModelItem {
            text: "Move Up"
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            menuItemPosition: 1
            enabled: {
                let si = termSelection.selectedIndexes
                for(let i = 0; i < si.length; i++) {
                    if(si[i].row)
                        return true
                }
                return false
            }
            onActivated: {
                let ordered = [].concat(termSelection.selectedIndexes)
                ordered.sort((a,b) => a.row - b.row)

                for(let i = 0; i < ordered.length; i++) {
                    if(ordered[i].row)
                        ShotBrowserEngine.presetsModel.moveRows(ordered[i].parent, ordered[i].row, 1, ordered[i].parent, ordered[i].row-1)
                }
            }
        }
        XsMenuModelItem {
            text: "Move Down"
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            menuItemPosition: 2
            enabled: {
                let si = termSelection.selectedIndexes
                for(let i = 0; i < si.length; i++) {
                    let rc = ShotBrowserEngine.presetsModel.rowCount(si[i].parent)-1
                    if(si[i].row < rc)
                        return true
                }
                return false
            }
            onActivated: {
                let ordered = [].concat(termSelection.selectedIndexes)
                ordered.sort((b,a) => a.row - b.row)

                for(let i = 0; i < ordered.length; i++) {
                    if(ordered[i].row < ShotBrowserEngine.presetsModel.rowCount(ordered[i].parent)-1)
                        ShotBrowserEngine.presetsModel.moveRows(ordered[i].parent, ordered[i].row, 1, ordered[i].parent, ordered[i].row+2)
                }
            }
        }
        XsMenuModelItem {
            menuItemType: "divider"
            menuPath: ""
            menuItemPosition: 3
            menuModelName: termMenu.menu_model_name
        }

        XsMenuModelItem {
            text: "Duplicate"
            menuPath: ""
            menuItemPosition: 4
            menuModelName: termMenu.menu_model_name
            onActivated: {
                let si = termSelection.selectedIndexes
                for(let i = 0; i < si.length; i++) {
                    ShotBrowserEngine.presetsModel.duplicate(si[i])
                }
            }
        }

        XsMenuModelItem {
            text: "Copy Terms"
            menuItemPosition: 4.5
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            onActivated: clipboard.text = JSON.stringify(ShotBrowserEngine.presetsModel.copy(termSelection.selectedIndexes))
        }

        XsMenuModelItem {
            text: "Paste Terms"
            menuItemPosition: 4.6
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            onActivated: {
                if(termSelection.selectedIndexes.length) {
                    ShotBrowserEngine.presetsModel.paste(
                        JSON.parse(clipboard.text),
                        termSelection.selectedIndexes[0].row+1,
                        termSelection.selectedIndexes[0].parent
                    )
                } else {
                    ShotBrowserEngine.presetsModel.paste(
                        JSON.parse(clipboard.text),
                        ShotBrowserEngine.presetsModel.rowCount(presetTermModel.rootIndex),
                        presetTermModel.rootIndex
                    )
                }
            }
        }

        XsMenuModelItem {
            text: "Paste Values From Clipboard"
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            menuItemPosition: 5
            onActivated: {
                let si = termSelection.selectedIndexes

                let values = clipboard.text.split("\n")
                if(values.length) {
                    let first = values[0].trim()
                    values = values.reverse()

                    for(let i = 0;i<values.length;i++) {
                        let v = values[i].trim()
                        if(v.length) {
                            for(let ii = 0; ii < si.length; ii++) {
                                if(v == first) {
                                    ShotBrowserEngine.presetsModel.set(ShotBrowserEngine.presetsModel.index(si[ii].row, 0, si[ii].parent), v, "valueRole")
                                } else {
                                    ShotBrowserEngine.presetsModel.duplicate(si[ii])
                                    ShotBrowserEngine.presetsModel.set(ShotBrowserEngine.presetsModel.index(si[ii].row+1, 0, si[ii].parent), v, "valueRole")
                                }
                            }
                        }
                    }
                }
            }
        }

        XsMenuModelItem {
            menuItemType: "divider"
            menuPath: ""
            menuItemPosition: 6
            menuModelName: termMenu.menu_model_name
        }
        XsMenuModelItem {
            text: "Remove"
            menuPath: ""
            menuItemPosition: 7
            menuModelName: termMenu.menu_model_name
            enabled: termSelection.selectedIndexes.length
            onActivated: {
                let ordered = [].concat(termSelection.selectedIndexes)
                ordered.sort((a,b) => b.row - a.row)

                for(let i = 0; i < ordered.length; i++) {
                    ShotBrowserEngine.presetsModel.removeRows(ordered[i].row, 1, ordered[i].parent)
                }
                termSelection.clear()
            }
        }
    }


}


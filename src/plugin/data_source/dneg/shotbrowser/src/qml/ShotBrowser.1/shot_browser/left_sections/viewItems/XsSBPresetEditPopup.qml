// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.14

import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14
import ShotBrowser 1.0
import xStudio 1.0
import xstudio.qml.clipboard 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0


XsWindow{
    id: presetEditPopup

    property var presetIndex: helpers.qModelIndex()
    property string entityType: "Versions"
    property string entityName: ""
    property string entityCategory: "Group"

    property int itemHeight: 24

    property int dragWidth: btnWidth/2
    property int termWidth: btnWidth*4
    property int modeWidth: itemHeight
    property int closeWidth: btnWidth

    width: 460
    height: 40 + 200 + (nameDiv.height + coln.spacing*2) //+ presetList.height
    // transientParent: parent
    // remove focus from text widgets.
    onClosing: coln.focus = true

    onPresetIndexChanged: {
        if(presetIndex.valid) {
            presetTermModel.rootIndex = presetIndex
            presetList.model.newTermParent = presetIndex
        }
    }

    QTreeModelToTableModel {
        id: presetTermModel
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

            }
        }

        XsListView{ id: presetList
            Layout.fillWidth: true
            Layout.fillHeight: true

            property int rightSpacing: height < contentHeight ? 14 : 0
            Behavior on rightSpacing {NumberAnimation {duration: 150}}

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

            Item{
                Layout.preferredWidth: parent.width / 3 * 2
                Layout.fillHeight: true
            }

            XsPrimaryButton{
                Layout.fillWidth: true
                Layout.fillHeight: true
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
        property var termModelIndex: helpers.qModelIndex()


        Clipboard{
          id: clipboard
        }

        XsMenuModelItem {
            text: "Move Up"
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            menuItemPosition: 1
            enabled: termMenu.termModelIndex.valid && termMenu.termModelIndex.row
            onActivated: {
                let i = termMenu.termModelIndex
                ShotBrowserEngine.presetsModel.moveRows(i.parent, i.row, 1, i.parent, i.row-1)
            }
        }
        XsMenuModelItem {
            text: "Move Down"
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            menuItemPosition: 2
            enabled: termMenu.termModelIndex.valid && termMenu.termModelIndex.row < termMenu.termModelIndex.model.rowCount(termMenu.termModelIndex.parent)-1
            onActivated: {
                let i = termMenu.termModelIndex
                ShotBrowserEngine.presetsModel.moveRows(i.parent, i.row, 1, i.parent, i.row+2)
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
            onActivated: ShotBrowserEngine.presetsModel.duplicate(termMenu.termModelIndex)
        }
        XsMenuModelItem {
            text: "Paste Values From Clipboard"
            menuPath: ""
            menuModelName: termMenu.menu_model_name
            menuItemPosition: 5
            onActivated: {
                let values = clipboard.text.split("\n")
                if(values.length) {
                    let first = values[0].trim()
                    values = values.reverse()

                    for(let i = 0;i<values.length;i++) {
                        let v = values[i].trim()
                        if(v.length) {
                            if(v == first) {
                                ShotBrowserEngine.presetsModel.set(ShotBrowserEngine.presetsModel.index(termMenu.termModelIndex.row, 0, termMenu.termModelIndex.parent), v, "valueRole")
                            } else {
                                ShotBrowserEngine.presetsModel.duplicate(termMenu.termModelIndex)
                                ShotBrowserEngine.presetsModel.set(ShotBrowserEngine.presetsModel.index(termMenu.termModelIndex.row+1, 0, termMenu.termModelIndex.parent), v, "valueRole")
                            }
                        }
                    }
                }
            }
        }
    }


}


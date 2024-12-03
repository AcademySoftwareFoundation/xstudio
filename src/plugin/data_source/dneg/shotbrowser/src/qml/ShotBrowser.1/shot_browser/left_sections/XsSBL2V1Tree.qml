// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0


Item{

    XsGradientRectangle{
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: panelPadding
        spacing: panelPadding

        Rectangle{
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            color: panelColor
        }

        RowLayout { id: headerDiv
            Layout.fillWidth: true;
            Layout.preferredHeight: btnHeight
            spacing: buttonSpacing
            z: 1

            XsSBTreeSearchButton{ id: searchBtn
                Layout.fillWidth: isExpanded
                Layout.minimumWidth: btnWidth
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                isExpanded: false
                hint: "Search..."
                model: ShotBrowserFilterModel {
                    sourceModel: ShotBrowserEngine.presetsModel.termModel("ShotSequenceList", "", projectId)
                }
                onIndexSelected: {
                    // possibility of id collisions ?
                    let mid = index.model.get(index, "idRole")
                    let bi = sequenceBaseModel.searchRecursive(mid, "idRole")
                    if(bi.valid) {
                        let fi = sequenceFilterModel.mapFromSource(bi)
                        if(fi.valid) {
                            let parents = helpers.getParentIndexes([fi])
                            for(let i = 0; i< parents.length; i++){
                                if(parents[i].valid) {
                                    // find in tree.. Order ?
                                    let ti = sequenceTreeModel.mapFromModel(parents[i])
                                    if(ti.valid) {
                                        sequenceTreeModel.expandRow(ti.row)
                                    }
                                }
                            }
                            // should now be visible
                            let ti = sequenceTreeModel.mapFromModel(fi)
                            sequenceSelectionModel.select(ti, ItemSelectionModel.ClearAndSelect)
                        }
                    }
                }
            }
            Item{
                Layout.fillWidth: !searchBtn.isExpanded
                Layout.preferredWidth: searchBtn.isExpanded? buttonSpacing : buttonSpacing*8
                Layout.preferredHeight: parent.height
            }
            XsButtonWithImageAndText{ id: liveLinkBtn
                Layout.fillWidth: !searchBtn.isExpanded
                Layout.minimumWidth: btnWidth
                Layout.preferredWidth: searchBtn.isExpanded? btnWidth : btnWidth*2
                Layout.maximumWidth: btnWidth*2
                Layout.preferredHeight: parent.height
                iconSrc: "qrc:/icons/link.svg"
                iconText: "Link"
                textDiv.visible: true
                isActive: sequenceTreeLiveLink && !isPaused
                onClicked: {
                    // searchBtn.isExpanded = false
                    sequenceTreeLiveLink  = !sequenceTreeLiveLink
                }
            }
            Item{
                Layout.fillWidth: !searchBtn.isExpanded
                Layout.preferredWidth: searchBtn.isExpanded? buttonSpacing : buttonSpacing*8
                Layout.preferredHeight: parent.height
            }
            XsPrimaryButton{ id: filterBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/filter.svg"
                isActive: sequenceFilterModel && sequenceFilterModel.hideStatus.length
                onClicked: {
                    // searchBtn.isExpanded = false
                    if(shotFilterPopup.visible) {
                        shotFilterPopup.visible = false
                    } else {
                        shotFilterPopup.showMenu(
                            filterBtn,
                            width/2,
                            height/2);
                    }
                }
            }
        }

        XsPopupMenu {
            id: shotFilterPopup
            menu_model_name: "shot_filter_popup"
            visible: false

            closePolicy: filterBtn.hovered ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside

            XsMenuModelItem {
                text: "Show Status"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 0
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: prefs.showStatus
                onActivated: prefs.showStatus = !prefs.showStatus
            }
            XsMenuModelItem {
                text: "Show Unit"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 0.1
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: prefs.showUnit
                onActivated: prefs.showUnit = !prefs.showUnit
            }

            XsMenuModelItem {
                text: "Hide Empty"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 0.9
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: prefs.hideEmpty
                onActivated: prefs.hideEmpty = !prefs.hideEmpty
            }

            XsMenuModelItem {
                text: "Hide Unit"
                menuItemType: "divider"
                menuPath: ""
                menuItemPosition: 1
                menuModelName: shotFilterPopup.menu_model_name
            }

            XsMenuModelItem {
                text: "No Unit"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 10
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: sequenceFilterModel && sequenceFilterModel.unitFilter.includes("No Unit")
                onActivated: {
                    if(isChecked) {
                        sequenceFilterModel.unitFilter = Array.from(sequenceFilterModel.unitFilter).filter(r => r !== "No Unit")
                    } else {
                        let tmp = sequenceFilterModel.unitFilter
                        tmp.push("No Unit")
                        sequenceFilterModel.unitFilter = tmp
                    }
                }
            }

            Repeater {
                model:  DelegateModel {
                    property var notifyUnitModel: ShotBrowserEngine.presetsModel.termModel("Unit", "Version", projectId)
                    onNotifyUnitModelChanged: {
                        if(sequenceFilterModel && projectIndex) {
                            if( projectIndex.model.get(projectIndex,"nameRole") in prefs.filterUnit)
                                sequenceFilterModel.unitFilter = prefs.filterUnit[projectIndex.model.get(projectIndex,"nameRole")]
                            else
                                sequenceFilterModel.unitFilter = []
                        }
                    }
                    model: notifyUnitModel
                    delegate :
                        Item {
                            XsMenuModelItem {
                                text: nameRole
                                menuItemType: "toggle"
                                menuPath: ""
                                menuItemPosition: index + 10
                                menuModelName: shotFilterPopup.menu_model_name
                                isChecked: sequenceFilterModel && sequenceFilterModel.unitFilter.includes(nameRole)
                                onActivated: {
                                    if(isChecked) {
                                        sequenceFilterModel.unitFilter = Array.from(sequenceFilterModel.unitFilter).filter(r => r !== nameRole)
                                    } else {
                                        let tmp = sequenceFilterModel.unitFilter
                                        tmp.push(nameRole)
                                        sequenceFilterModel.unitFilter = tmp
                                    }
                                }
                            }
                        }
                }
            }

            XsMenuModelItem {
                text: "Hide Status"
                menuItemType: "divider"
                menuPath: ""
                menuItemPosition: 100
                menuModelName: shotFilterPopup.menu_model_name
            }

            Repeater {
                model:  DelegateModel {
                    property var notifyModel: ShotBrowserEngine.presetsModel.termModel("Shot Status")
                    model: notifyModel
                    delegate :
                        Item {
                            XsMenuModelItem {
                                text: nameRole
                                menuItemType: "toggle"
                                menuPath: ""
                                menuItemPosition: index + 101
                                menuModelName: shotFilterPopup.menu_model_name
                                isChecked: (prefs.hideStatus.includes(nameRole) || prefs.hideStatus.includes(idRole))
                                onActivated: {
                                    if(isChecked) {
                                        prefs.hideStatus = Array.from(prefs.hideStatus).filter(r => r !== idRole && r !== nameRole)
                                    } else {
                                        let tmp = prefs.hideStatus
                                        tmp.push(idRole)
                                        tmp.push(nameRole)
                                        prefs.hideStatus = tmp
                                    }
                                }
                            }
                        }
                }
            }
        }

        Rectangle{
            Layout.fillWidth: true;
            Layout.fillHeight: true;
            color: panelColor

            XsListView {
                id: sequenceTreeView
                anchors.fill: parent
                spacing: 1

                ScrollBar.vertical: XsScrollBar{visible: sequenceTreeView.height < sequenceTreeView.contentHeight}
                property int rightSpacing: sequenceTreeView.height < sequenceTreeView.contentHeight ? 10 : 0
                Behavior on rightSpacing {NumberAnimation {duration: 150}}

                model: sequenceTreeModel

                Connections {
                    target: sequenceSelectionModel
                    function onSelectionChanged(selected, deselected) {
                        if(selected.length){
                            sequenceTreeView.positionViewAtIndex(selected[0].topLeft.row, ListView.Visible)
                        }
                    }
                }

                delegate: DelegateChooser {
                    id: chooser
                    role: "typeRole"

                    DelegateChoice {
                        roleValue: "Sequence";
                        XsSBSequenceDelegate{
                            width: sequenceTreeView.width - sequenceTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: sequenceTreeModel
                            selectionModel: sequenceSelectionModel
                            showStatus: prefs.showStatus
                        }
                    }
                    DelegateChoice {
                        roleValue: "Shot";
                        XsSBShotDelegate{
                            width: sequenceTreeView.width - sequenceTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: sequenceTreeModel
                            selectionModel: sequenceSelectionModel
                            showUnit: prefs.showUnit
                            showStatus: prefs.showStatus
                        }
                    }
                }
            }
        }
    }
}
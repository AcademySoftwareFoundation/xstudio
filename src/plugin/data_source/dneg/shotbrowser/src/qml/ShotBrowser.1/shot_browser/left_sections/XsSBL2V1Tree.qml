// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.qmlmodels

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
                isExpanded: true
                hint: "Search..."
                model: assetMode ? assetListMode : seqListMode

                ShotBrowserSequenceFilterModel {
                    id: seqListMode
                    sourceModel: ShotBrowserEngine.presetsModel.termModel("ShotSequenceList", "", projectId)
                    hideStatus: prefs.hideStatus
                    hideEmpty: prefs.hideEmpty
                    showHidden: prefs.showHidden
                    typeFilter: prefs.filterType
                    unitFilter: {
                        if( projectIndex && projectIndex.model && projectIndex.model.get(projectIndex,"nameRole") in prefs.filterUnit)
                            return prefs.filterUnit[projectIndex.model.get(projectIndex,"nameRole")]
                        return []
                    }
                }

                ShotBrowserSequenceFilterModel {
                    id: assetListMode
                    sourceModel: ShotBrowserEngine.presetsModel.termModel("AssetList", "", projectId)
                    hideStatus: prefs.hideStatus
                }

                function selectInTree(currentIndex) {
                    // possibility of id collisions ?
                    if(currentIndex != -1) {
                        let index = model.index(currentIndex, 0)
                        let mid = index.model.get(index, "idRole")
                        let baseModel = sequenceBaseModel
                        let filterModel = sequenceFilterModel
                        let selectionModel = sequenceSelectionModel
                        let treemodel = sequenceTreeModel

                        if(assetMode) {
                            baseModel = assetBaseModel
                            filterModel = assetFilterModel
                            selectionModel = assetSelectionModel
                            treemodel = assetTreeModel
                        }

                        let bi = baseModel.searchRecursive(mid, "idRole")
                        if(bi.valid) {
                            let fi = filterModel.mapFromSource(bi)
                            if(fi.valid) {
                                let parents = helpers.getParentIndexes([fi])
                                for(let i = 0; i< parents.length; i++){
                                    if(parents[i].valid) {
                                        // find in tree.. Order ?
                                        let ti = treemodel.mapFromModel(parents[i])
                                        if(ti.valid) {
                                            treemodel.expandRow(ti.row)
                                        }
                                    }
                                }
                                // should now be visible
                                let ti = treemodel.mapFromModel(fi)
                                selectionModel.select(ti, ItemSelectionModel.ClearAndSelect)
                            }
                        }
                    }
                }

                onIndexSelected: (index) => {
                    selectInTree(index)
                }
            }

            Item{
                Layout.fillWidth: !searchBtn.isExpanded
                Layout.preferredWidth: searchBtn.isExpanded? buttonSpacing : buttonSpacing*8
                Layout.preferredHeight: parent.height
            }
            XsPrimaryButton{ id: liveLinkBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/link.svg"

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
                text: "Show Type"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 0.2
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: prefs.showType
                onActivated: prefs.showType = !prefs.showType
            }

            XsMenuModelItem {
                text: "Show Visibility Icon"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 0.3
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: prefs.showVisibility
                onActivated: prefs.showVisibility = !prefs.showVisibility
            }

            XsMenuModelItem {
                text: "Misc"
                menuItemType: "divider"
                menuPath: ""
                menuItemPosition: 0.5
                menuModelName: shotFilterPopup.menu_model_name
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
                text: "Only Show Visible"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 0.95
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: !prefs.showHidden
                onActivated: prefs.showHidden = !prefs.showHidden
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
                text: "Hide Type"
                menuItemType: "divider"
                menuPath: ""
                menuItemPosition: 100
                menuModelName: shotFilterPopup.menu_model_name
            }

            XsMenuModelItem {
                text: "No Type"
                menuItemType: "toggle"
                menuPath: ""
                menuItemPosition: 110
                menuModelName: shotFilterPopup.menu_model_name
                isChecked: sequenceFilterModel && sequenceFilterModel.typeFilter.includes("No Type")
                onActivated: {
                    if(isChecked) {
                        sequenceFilterModel.typeFilter = Array.from(sequenceFilterModel.typeFilter).filter(r => r !== "No Type")
                    } else {
                        let tmp = sequenceFilterModel.typeFilter
                        tmp.push("No Type")
                        sequenceFilterModel.typeFilter = tmp
                        prefs.filterType = sequenceFilterModel.typeFilter
                    }
                }
            }

            Repeater {
                model:  DelegateModel {
                    property var notifyTypeModel: sequenceFilterModel && sequenceFilterModel.sourceModel ? sequenceFilterModel.sourceModel.types : []
                    onNotifyTypeModelChanged: {
                        if(sequenceFilterModel)
                            sequenceFilterModel.typeFilter = prefs.filterType
                    }
                    model: notifyTypeModel
                    delegate :
                        Item {
                            XsMenuModelItem {
                                text: modelData
                                menuItemType: "toggle"
                                menuPath: ""
                                menuItemPosition: index + 110
                                menuModelName: shotFilterPopup.menu_model_name
                                isChecked: sequenceFilterModel && sequenceFilterModel.typeFilter.includes(modelData)
                                onActivated: {
                                    if(isChecked) {
                                        sequenceFilterModel.typeFilter = Array.from(sequenceFilterModel.typeFilter).filter(r => r !== modelData)

                                    } else {
                                        let tmp = sequenceFilterModel.typeFilter
                                        tmp.push(modelData)
                                        sequenceFilterModel.typeFilter = tmp
                                    }
                                    prefs.filterType = sequenceFilterModel.typeFilter
                                }
                            }
                        }
                }
            }


            XsMenuModelItem {
                text: "Hide Status"
                menuItemType: "divider"
                menuPath: ""
                menuItemPosition: 200
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
                                menuItemPosition: index + 201
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
            visible: !assetMode

            XsListView {
                id: sequenceTreeView
                anchors.fill: parent
                spacing: 1

                ScrollBar.vertical: XsScrollBar{visible: sequenceTreeView.height < sequenceTreeView.contentHeight}
                property int rightSpacing: sequenceTreeView.height < sequenceTreeView.contentHeight ? 12 : 0
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
                    role: "typeRole"

                    DelegateChoice {
                        roleValue: "Sequence";
                        XsSBSequenceDelegate{
                            width: sequenceTreeView.width - sequenceTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: sequenceTreeModel
                            selectionModel: sequenceSelectionModel
                            showStatus: prefs.showStatus
                            showType: prefs.showType
                            showVisibility: prefs.showVisibility
                        }
                    }
                    DelegateChoice {
                        roleValue: "Project";
                        XsSBSequenceDelegate{
                            width: sequenceTreeView.width - sequenceTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: sequenceTreeModel
                            selectionModel: sequenceSelectionModel
                            showVisibility: prefs.showVisibility
                        }
                    }
                    DelegateChoice {
                        roleValue: "Episode";
                        XsSBSequenceDelegate{
                            width: sequenceTreeView.width - sequenceTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: sequenceTreeModel
                            selectionModel: sequenceSelectionModel
                            showStatus: prefs.showStatus
                            showType: prefs.showType
                            showVisibility: prefs.showVisibility
                        }
                    }
                    DelegateChoice {
                        roleValue: "Asset";
                        XsSBSequenceDelegate{
                            width: sequenceTreeView.width - sequenceTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: sequenceTreeModel
                            selectionModel: sequenceSelectionModel
                            showStatus: prefs.showStatus
                            showType: prefs.showType
                            showVisibility: prefs.showVisibility
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
                            showType: prefs.showType
                            showVisibility: prefs.showVisibility
                        }
                    }
                }
            }
        }
        Rectangle{
            Layout.fillWidth: true;
            Layout.fillHeight: true;
            color: panelColor
            visible: assetMode

            XsListView {
                id: assetTreeView
                anchors.fill: parent
                spacing: 1

                ScrollBar.vertical: XsScrollBar{visible: assetTreeView.height < assetTreeView.contentHeight}
                property int rightSpacing: assetTreeView.height < assetTreeView.contentHeight ? 10 : 0
                Behavior on rightSpacing {NumberAnimation {duration: 150}}

                model: assetTreeModel

                Connections {
                    target: assetSelectionModel
                    function onSelectionChanged(selected, deselected) {
                        if(selected.length){
                            assetTreeView.positionViewAtIndex(selected[0].topLeft.row, ListView.Visible)
                        }
                    }
                }

                delegate: DelegateChooser {
                    role: "typeRole"

                    DelegateChoice {
                        roleValue: "Asset";
                        XsSBSequenceDelegate{
                            width: assetTreeView.width - assetTreeView.rightSpacing
                            height: btnHeight-4
                            delegateModel: assetTreeModel
                            selectionModel: assetSelectionModel
                            showStatus: prefs.showStatus
                            showType: prefs.showType
                            showVisibility: prefs.showVisibility
                        }
                    }
                }
            }
        }
    }
}
// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0


RowLayout{
    spacing: buttonSpacing
    function showOrHideLoginDialog(){

        if(!loginDialog.visible){
            loginDialog.x = appWindow.x + appWindow.width/3
            loginDialog.y = appWindow.y + appWindow.height/4
            loginDialog.visible = true
        }
        else{
            loginDialog.visible = true
        }

    }

    readonly property int butWidth: btnWidth * 0.8

    XsSBLoginDialog{
        id: loginDialog
    }

    XsPrimaryButton{
        id: firstButton
        Layout.minimumWidth: butWidth * 1.5
        Layout.maximumWidth: butWidth * 1.5
        Layout.fillHeight: true
        imgSrc: "qrc:///shotbrowser_icons/nature.svg"
        text: "Tree View"
        isActive: currentCategory == "Tree" && !sequenceTreeShowPresets
        onClicked: {
            if(currentCategory != "Tree")
                resultsBaseModel.setResultData([])

            currentCategory = "Tree"
            sequenceTreeShowPresets = false
        }
    }
    XsPrimaryButton{
        Layout.minimumWidth: firstButton.width
        Layout.maximumWidth: firstButton.width
        Layout.fillHeight: true
        imgSrc: "qrc:///shotbrowser_icons/tree_plus.svg"
        text: "Tree Plus"
        isActive: currentCategory == "Tree" && sequenceTreeShowPresets
        onClicked: {
            if(currentCategory != "Tree")
                resultsBaseModel.setResultData([])
            currentCategory = "Tree"
            sequenceTreeShowPresets = true
        }
    }
    XsPrimaryButton{
        Layout.minimumWidth: firstButton.width
        Layout.maximumWidth: firstButton.width
        Layout.fillHeight: true
        imgSrc: "qrc:///shotbrowser_icons/globe.svg"
        text: "Global View"
        isActive: currentCategory == "Recent"
        onClicked: {
            resultsBaseModel.setResultData([])
            currentCategory = "Recent"
        }
    }
    XsPrimaryButton{
        Layout.minimumWidth: firstButton.width
        Layout.maximumWidth: firstButton.width
        Layout.fillHeight: true
        imgSrc: "qrc:///shotbrowser_icons/settings.svg"
        text: "Setup View"
        isActive: currentCategory == "Menus"
        onClicked: {
            resultsBaseModel.setResultData([])
            currentCategory = "Menus"
        }
    }



    XsComboBoxEditable{
        id: combo
        model: ShotBrowserFilterModel {
            divisionFilter: prefs.filterProjects
            projectStatusFilter: prefs.filterProjectStatus
        }

        Connections {
            target: ShotBrowserEngine
            function onReadyChanged() {
                if(ShotBrowserEngine.ready && combo.model.sourceModel != ShotBrowserEngine.presetsModel.termModel("Project"))
                    combo.model.sourceModel = ShotBrowserEngine.presetsModel.termModel("Project")
            }
        }

        textRole: "nameRole"
        Layout.fillWidth: true
        Layout.minimumWidth: btnWidth * 1.3
        Layout.fillHeight: true
        Layout.leftMargin: 4
        textField.font.weight: Font.Black

        onActivated: projectIndex = model.mapToSource(model.index(index, 0))
        onAccepted: {
            projectIndex = model.mapToSource(model.index(currentIndex, 0))
            focus = false
        }

        Connections {
            target: panel
            function onProjectIndexChanged() {
                // console.log(projectIndex, projectIndex.row)
                if(combo.currentIndex != combo.model.mapFromSource(projectIndex).row) {
                    combo.currentIndex = combo.model.mapFromSource(projectIndex).row
                }
            }
        }
        Component.onCompleted: {
            if(ShotBrowserEngine.ready)
                combo.model.sourceModel = ShotBrowserEngine.presetsModel.termModel("Project")

            if(projectIndex && projectIndex.valid && combo.currentIndex != model.mapFromSource(projectIndex).row) {
                combo.currentIndex = model.mapFromSource(projectIndex).row
            }
        }
    }

    XsPrimaryButton{ id: filterBtn
        Layout.minimumWidth: butWidth
        Layout.maximumWidth: butWidth
        Layout.fillHeight: true
        Layout.rightMargin: 4

        imgSrc: "qrc:/icons/filter.svg"
        isActive: combo.model && combo.model.divisionFilter.length
        onClicked: {
            // searchBtn.isExpanded = false
            if(projectFilterPopup.visible) {
                projectFilterPopup.visible = false
            } else {
                projectFilterPopup.showMenu(
                    filterBtn,
                    width/2,
                    height/2);
            }
        }
    }

    XsPopupMenu {
        id: projectFilterPopup
        menu_model_name: "project_filter_popup"
        visible: false

        closePolicy: filterBtn.hovered ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        XsMenuModelItem {
            menuItemType: "divider"
            text: "Hide Status"
            menuItemPosition: 1
            menuPath: ""
            menuModelName: projectFilterPopup.menu_model_name
        }

        Repeater {
            model: [
                {"name": "Awarded", "id": "awrd"},
                {"name": "Bidding", "id": "bld"},
                {"name": "Complete", "id": "cmpt"},
                {"name": "Error", "id": "err"},
                {"name": "In Progress", "id": "ip"},
                {"name": "N/A", "id": "na"},
                {"name": "On Hold", "id": "hld"},
                {"name": "Projected", "id": "prjd"},
                {"name": "Waiting To Start", "id": "wtg"}
            ]
            Item {
                XsMenuModelItem {
                    text: modelData.name
                    menuItemType: "toggle"
                    menuPath: ""
                    menuItemPosition: index + 1
                    menuModelName: projectFilterPopup.menu_model_name
                    isChecked: combo.model && combo.model.projectStatusFilter.includes(modelData.id)
                    onActivated: {
                        if(isChecked) {
                            prefs.filterProjectStatus = Array.from(combo.model.projectStatusFilter).filter(r => r !== modelData.id)
                        } else {
                            let tmp = combo.model.projectStatusFilter
                            tmp.push(modelData.id)
                            prefs.filterProjectStatus = tmp
                        }
                    }
                }
            }
        }


        XsMenuModelItem {
            menuItemType: "divider"
            text: "Hide Division"
            menuItemPosition: 99
            menuPath: ""
            menuModelName: projectFilterPopup.menu_model_name
        }

        Repeater {
            model: ["Feature Animation", "Visual Effects", "ReDefine", "TV", "dneg360"]
            Item {
                XsMenuModelItem {
                    text: modelData
                    menuItemType: "toggle"
                    menuPath: ""
                    menuItemPosition: index + 100
                    menuModelName: projectFilterPopup.menu_model_name
                    isChecked: combo.model && combo.model.divisionFilter.includes(modelData)
                    onActivated: {
                        if(isChecked) {
                            prefs.filterProjects = Array.from(combo.model.divisionFilter).filter(r => r !== modelData)
                        } else {
                            let tmp = combo.model.divisionFilter
                            tmp.push(modelData)
                            prefs.filterProjects = tmp
                        }
                    }
                }
            }
        }
    }


    XsPrimaryButton{ id: credentialsBtn
        Layout.minimumWidth: butWidth
        Layout.maximumWidth: butWidth
        Layout.fillHeight: true
        imgSrc: "qrc:///shotbrowser_icons/manage_accounts.svg"
        isActive: loginDialog.visible
        onClicked: {
            showOrHideLoginDialog()
        }
    }

}

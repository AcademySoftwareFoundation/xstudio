// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

XsGradientRectangle{
    id: presetView

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: (currentCategory == "Tree")? panelPadding : 0
        spacing: panelPadding

        Rectangle{
            Layout.fillWidth: true;
            Layout.preferredHeight: 2;
            color: panelColor
        }

        RowLayout { id: headerDiv
            Layout.fillWidth: true;
            Layout.preferredHeight: btnHeight
            spacing: buttonSpacing

            XsPrimaryButton{ id: addBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/add.svg"
                onClicked: {
                    if(addMenu.visible) addMenu.visible = false
                    else{
                        addMenu.showMenu(
                            addBtn,
                            width/2,
                            height/2);
                    }
                }
            }

            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: parent.height
            }

            XsPrimaryButton{ id: moreBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/more_vert.svg"
                onClicked:{
                    if(moreMenu.visible) moreMenu.visible = false
                    else{
                        moreMenu.showMenu(
                            moreBtn,
                            width/2,
                            height/2);
                    }
                }
            }
        }

        Rectangle{
            Layout.fillWidth: true;
            Layout.fillHeight: true;
            color: panelColor

            XsSBPresetsView{
                anchors.fill: parent
            }
        }

    }


    function addGroupItem(type){

        let m = ShotBrowserEngine.presetsModel
        let i = m.insertGroup(type, -1)

        if(currentCategory == "Tree") {
            m.set(i, "tree", "userDataRole")
            // this shouldn't be required..
            treeModel.invalidate()
        }
        else if(currentCategory == "Menus") {
            m.set(i, "menus", "userDataRole")
            menuModel.invalidate()
        }
        else if(currentCategory == "Recent") {
            m.set(i, "recent", "userDataRole")
            recentModel.invalidate()
        }

    }

    XsPopupMenu {
        id: addMenu
        visible: false
        menu_model_name: "addMenu"+presetView
    }
    XsMenuModelItem {
        property var type: "Versions"
        text: type+" Group"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: addMenu.menu_model_name
        onActivated: {
            addGroupItem(type)
        }
    }
    XsMenuModelItem {
        property var type: "Notes"
        text: type+" Group"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: addMenu.menu_model_name
        onActivated: {
            addGroupItem(type)
        }
    }
    XsMenuModelItem {
        property var type: "Playlists"
        text: type+" Group"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: addMenu.menu_model_name
        onActivated: {
            addGroupItem(type)
        }
    }


    XsPopupMenu {
        id: moreMenu
        visible: false
        menu_model_name: "moreMenu"+presetView
    }

   XsMenuModelItem {
        text: qsTr("Only Show Favourites")
        menuPath: ""
        menuItemType: "toggle"
        menuItemPosition: 0.5
        menuModelName: moreMenu.menu_model_name
        onActivated: prefs.showOnlyFavourites = !prefs.showOnlyFavourites
        isChecked: prefs.showOnlyFavourites
    }

    XsMenuModelItem {
        text: "Undo"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: moreMenu.menu_model_name
        onActivated: ShotBrowserEngine.undo()
    }

    XsMenuModelItem {
        text: "Redo"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: moreMenu.menu_model_name
        onActivated: ShotBrowserEngine.redo()
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: moreMenu.menu_model_name
    }

    // XsMenuModelItem {
    //     menuItemType: "divider"
    //     menuPath: ""
    //     menuItemPosition: 3
    //     menuModelName: moreMenu.menu_model_name
    // }
    XsMenuModelItem {
        text: "Backup Presets..."
        menuPath: ""
        menuItemPosition: 3.5
        menuModelName: moreMenu.menu_model_name
        onActivated: dialogHelpers.showFileDialog(
                function(fileUrl, undefined, func) {
                    if(fileUrl)
                        Future.promise(
                            ShotBrowserEngine.presetsModel.backupPresetsFuture(
                                fileUrl
                            )
                        ).then(function(string) {
                                dialogHelpers.errorDialogFunc("Backup Presets", "Backup Presets complete.\n\n" + string)
                            },
                            function(err) {
                                dialogHelpers.errorDialogFunc("Backup Presets", "Backup Presets failed.\n\n" + err)
                            }
                        )
                },
                file_functions.defaultSessionFolder(),
                "Backup Presets",
                "json",
                ["JSON (*.json)"],
                false,
                false
            )
    }

    XsMenuModelItem {
        text: "Restore Presets..."
        menuPath: ""
        menuItemPosition: 3.6
        menuModelName: moreMenu.menu_model_name
        onActivated: dialogHelpers.showFileDialog(
                function(fileUrl, button, func) {
                    if(fileUrl)
                        Future.promise(
                            ShotBrowserEngine.presetsModel.restorePresetsFuture(
                                fileUrl
                            )
                        ).then(function(string) {
                                dialogHelpers.errorDialogFunc("Restore Presets", "Restore Presets complete.\n\n" + string)
                            },
                            function(err) {
                                dialogHelpers.errorDialogFunc("Restore Presets", "Restore Presets failed.\n\n" + err)
                            }
                        )
                },
                file_functions.defaultSessionFolder(),
                "Backup Presets",
                "json",
                ["JSON (*.json)"],
                true,
                false
            )
    }

    XsMenuModelItem {
        text: "Export As System Presets..."
        menuPath: ""
        menuItemPosition: 4
        menuModelName: moreMenu.menu_model_name
        onActivated: dialogHelpers.showFileDialog(
                function(fileUrl, undefined, func) {
                    if(fileUrl)
                        Future.promise(
                            ShotBrowserEngine.presetsModel.exportAsSystemPresetsFuture(
                                fileUrl
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
        text: "Reload All System Presets"
        menuPath: ""
        menuItemPosition: 5
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            // set all indexs to visible.
            let hidden = ShotBrowserEngine.presetsModel.searchRecursiveList(
                true, "hiddenRole",
                ShotBrowserEngine.presetsModel.index(-1,-1),
                0,-1
            )
            for(let i=0; i<hidden.length; i++)
                ShotBrowserEngine.presetsModel.set(hidden[i], false, "hiddenRole")

            let changed = ShotBrowserEngine.presetsModel.searchRecursiveList(
                true, "updateRole",
                ShotBrowserEngine.presetsModel.index(-1,-1),
                0,-1
            )
            ShotBrowserEngine.presetsModel.resetPresets(changed)
        }
    }
}
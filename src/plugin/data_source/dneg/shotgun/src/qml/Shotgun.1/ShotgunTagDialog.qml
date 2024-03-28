// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient
import QtQuick.Controls.Styles 1.4 //for TextFieldStyle
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1

XsWindow {
	id: shotgunDialog
    centerOnOpen: true
    onTop: false

    width: 600
    height: 600

    title: "ShotGrid Tag Browser"

    XsWindowStateSaver
    {
        windowObj: shotgunDialog
        windowName: "shotgun_tag_browser"
    }

    property var data_source: null
    property var tagMethod: null
    property var untagMethod: null
    property var newTagMethod: null
    property var renameTagMethod: null
    property var removeTagMethod: null
    property var activeTags: []
    signal updateView()

    XsTimer {
        id: delayTimer
    }

    onVisibleChanged: {
        if(visible)
            updateActive()
    }

    function doAction(id, name) {
        if(actionMode.currentText == "Assign") {
            tagMethod(id)
            delayTimer.setTimeout(function() {
                updateActive()
            }, 1000)
        } else if(actionMode.currentText == "De-Assign") {
            untagMethod(id)
            delayTimer.setTimeout(function() {
                updateActive()
            }, 1000)
        } else if(actionMode.currentText == "Rename") {
            renameTagMethod(id, name)
        } else if(actionMode.currentText == "Remove") {
            removeTagMethod(id)
        }
    }

    function updateActive() {
        if(shotgunDialog.visible) {
            if(app_window.mediaSelectionModel.selectedIndexes.length) {
                Future.promise(
                    app_window.sessionModel.getJSONFuture(app_window.mediaSelectionModel.selectedIndexes[0], "/metadata/shotgun/version/relationships/tags/data")
                ).then(function(json_string) {
                    activeTags = []
                    let reftags = JSON.parse(json_string)
                    for(let i =0;i<reftags.length;i++){
                        activeTags.push(reftags[i]["id"])
                    }
                    updateView()
                })
            } else {
                activeTags = []
                updateView()
            }
        }
    }

    Connections {
        target: app_window.mediaSelectionModel
        function onSelectionChanged(selected,deselected) {
            updateActive()
        }
    }

    ColumnLayout {
        anchors.fill: parent

        Rectangle {
            color: "transparent"
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.margins: 2

            RowLayout {
                id: headitem
                anchors.fill: parent

                XsButton {
                    text: "New Tag"
                    onClicked: newTagMethod()
                }
                XsComboBox { id: actionMode
                    // Layout.fillWidth: true
                    model: ["Assign", "De-Assign", "Rename"]//, "Remove"]
                    currentIndex: 0
                    Layout.preferredWidth: 200
                    Layout.fillHeight: true
                }

                XsButton {
                    text: "Refresh"
                    Layout.alignment: Qt.AlignRight
                    onClicked: data_source.updateModel("referenceTagModel")
                }
            }
        }

        DelegateModel {
            id: tagDelegate

            model: data_source.connected ? data_source.termModels.referenceTagModel : null

            delegate: Rectangle {
                color: "transparent"
                width: grid.cellWidth
                height: grid.cellHeight
                XsButton {
                    id: but
                    isActive: activeTags.includes(idRole)
                    anchors.fill: parent
                    anchors.margins: 2
                    text: nameRole
                    onClicked: doAction(idRole, nameRole)

                    Connections {
                        target: shotgunDialog
                        function onUpdateView() {
                            but.isActive = activeTags.includes(idRole)
                        }
                    }
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 5
            clip: true
            cellHeight:30
            cellWidth:100
            ScrollBar.vertical: XsScrollBar {
                policy: grid.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            model: tagDelegate
        }
    }
}
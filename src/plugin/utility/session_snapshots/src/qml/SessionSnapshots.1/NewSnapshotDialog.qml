// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.0

import xStudio 1.0

XsWindow {

    id: popup

    title: "Select a Snapshot Folder"
    property string body: ""
    property string initialText: ""
    width: 400
    height: 200
    signal response(variant text, variant button_press)
    property var chaser

    FileDialog {
        id: select_path_dialog
        title: "Select Snapshot Path"
        folder: file_functions.defaultSessionFolder()

        selectFolder: true
        selectExisting: true
        selectMultiple: false

        onAccepted: {
            snapshot_folder.text = fileUrls[0]
            snapshot_name.text = fileUrls[0].toString().split('/').pop()
        }
    }

    ColumnLayout {

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 20

        RowLayout {

            Layout.fillWidth: true
            Layout.preferredHeight: 30

            XsTextField{

                id: snapshot_folder
                Layout.fillWidth: true
                Layout.fillHeight: true
                wrapMode: Text.Wrap
                clip: true
                focus: true
                placeholderText: "Select Snapshot Folder"
                horizontalAlignment: TextInput.AlignLeft
                onActiveFocusChanged:{
                    if(activeFocus) selectAll()
                }

            }

            XsSimpleButton {

                text: "Browse ..."
                width: XsStyleSheet.primaryButtonStdWidth*2
                Layout.fillHeight: true
                onClicked: {
                    select_path_dialog.open()
                }
            }
        }

        RowLayout {

            Layout.fillWidth: true
            Layout.preferredHeight: 30

            XsTextField
            {
                id: snapshot_name
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: ""
                placeholderText: "Set Menu Title"
                wrapMode: Text.Wrap
                clip: true
                focus: true
                horizontalAlignment: TextInput.AlignLeft
                onActiveFocusChanged:{
                    if(activeFocus) selectAll()
                }

            }

        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            XsSimpleButton {
                text: "Cancel"
                minWidth: XsStyleSheet.primaryButtonStdWidth*2
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    popup.visible = false
                }
            }
            XsSimpleButton {
                text: "Select Folder"
                minWidth: XsStyleSheet.primaryButtonStdWidth*2
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    if(snapshot_name.text.length && snapshot_folder.text.length) {
                        let v = snapshot_paths
                        v.push({'path': snapshot_folder.text, "name": snapshot_name.text})
                        snapshot_paths = v
                        snapshot_folder.text = ""
                        snapshot_name.text = ""
                        popup.visible = false
                    } else {
                        dialogHelpers.errorDialogFunc(
                            "Error",
                            "You must select a folder from the filesystem and provide a snapshot menu title."
                            )
                    }
                }
            }
        }
    }
}
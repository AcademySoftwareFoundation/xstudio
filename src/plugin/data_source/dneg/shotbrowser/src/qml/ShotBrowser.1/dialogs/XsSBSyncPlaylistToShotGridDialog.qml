// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.14
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import QuickFuture 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

XsWindow {
    id: dialog
    title: "Push Contents To ShotGrid"

    property var playlistProperties: null

    property int validMediaCount: 0
    property int invalidMediaCount: 0
    property bool linking: false

    property var plIndex: playlistProperties ? playlistProperties.values.index : null

    onPlIndexChanged: {
        if(visible && playlistProperties && playlistProperties.values.actorUuidRole) {
            linking = true
            ShotBrowserHelpers.getValidMediaCount(helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole), updateValidMediaCount)
        }
    }

    function updateValidMediaCount(json) {
        try {
            var data = JSON.parse(json)
            validMediaCount = data["result"]["valid"]
            invalidMediaCount = data["result"]["invalid"]
        } catch(err) {
            console.log(err)
        }
        linking = false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        GridLayout {

            id: main_layout
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            XsLabel {
                text: "Name : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            XsText {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                enabled: false
                text: playlistProperties ? playlistProperties.values.nameRole : ""
            }
            XsLabel {
                text: "Valid ShotGrid Media : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }
            XsLabel {
                text: linking ? "Linking media to versions..." : validMediaCount + " / " + (validMediaCount+invalidMediaCount)
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
            }

            XsLabel {
                Layout.fillHeight: true
            }
        }

        RowLayout {
            id: myFooter
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 35

            focus: true
            Keys.onReturnPressed: accept()
            Keys.onEscapePressed: reject()

            XsPrimaryButton {
                text: qsTr("Cancel")
                Layout.leftMargin: 10
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: dialog.width / 5

                onClicked: close()
            }
            Item{Layout.fillWidth: true}
            XsPrimaryButton {
                text: "Push To ShotGrid"
                highlighted: !linking
                enabled: !linking

                Layout.rightMargin: 10
                Layout.minimumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10

                // function onSyncCallback(json) {
                //     try {
                //         var data = JSON.parse(json)
                //         if(data["data"]["id"]){
                //             ShotBrowserHelpers.loadShotGridPlaylist(data["data"]["id"], data["data"]["attributes"]["code"])
                //         } else {
                //             console.log(err)
                //             dialogHelpers.errorDialogFunc("Sync Playlist To ShotGrid", "Sync of Playlist to ShotGrid failed.\n\n" + data)
                //         }
                //     } catch(err) {
                //         console.log(err)
                //         dialogHelpers.errorDialogFunc("Sync Playlist To ShotGrid", "Sync of Playlist to ShotGrid failed.\n\n" + json)
                //     }
                // }

                onClicked: {
                    ShotBrowserHelpers.syncPlaylistToShotGrid(helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole))
                    close()
                }
            }
        }
    }
}


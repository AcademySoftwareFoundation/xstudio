// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts



import QuickFuture 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

XsWindow {
    id: dialog
    title: "Push Contents To ShotGrid Playlist"

    property var playlistProperties: null

    property int validMediaCount: 0
    property int invalidMediaCount: 0
    property bool linking: false

    minimumWidth: 500
    minimumHeight: 80

    property var plIndex: playlistProperties ? playlistProperties.values.index : null

    onPlIndexChanged: {
        if(visible && playlistProperties && playlistProperties.values.actorUuidRole) {
            linking = true
            ShotBrowserHelpers.getValidMediaCount(helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole), updateValidMediaCount)
        }
    }

    XsPreference {
        id: loadOnPublishPref
        path: "/plugin/data_source/shotbrowser/load_playlist_on_publish"
    }

    function openOnCreate(json) {
        try {
            var data = JSON.parse(json)
            if(data["data"]["id"]){
                ShotBrowserHelpers.loadShotGridPlaylist(data["data"]["id"], data["data"]["attributes"]["code"])
            } else {
                console.log(err)
                dialogHelpers.errorDialogFunc("Push Playlist To ShotGrid", "Publishing of Playlist to ShotGrid failed.\n\n" + data)
            }
        } catch(err) {
            console.log(err)
            dialogHelpers.errorDialogFunc("Push Playlist To ShotGrid", "Push of Playlist to ShotGrid failed.\n\n" + json)
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
            Layout.minimumHeight: 30
            // Layout.margins: 10
            spacing: 10
            focus: true

            Keys.onReturnPressed: accept()
            Keys.onEscapePressed: reject()

            XsPrimaryButton {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: dialog.width / 5

                onClicked: close()
            }
            Item{Layout.fillWidth: true}
            XsPrimaryButton {
                text: "Overwrite Playlist"
                highlighted: !linking
                enabled: !linking

                Layout.minimumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    ShotBrowserHelpers.syncPlaylistToShotGrid(
                        helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole),
                        false,
                        loadOnPublishPref.value ? openOnCreate : null
                    )
                    close()
                }
            }
            XsPrimaryButton {
                text: "Append To Playlist"
                highlighted: !linking
                enabled: !linking

                Layout.minimumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    ShotBrowserHelpers.syncPlaylistToShotGrid(
                        helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole),
                        true,
                        loadOnPublishPref.value ? openOnCreate : null
                    )
                    close()
                }
            }
        }
    }
}


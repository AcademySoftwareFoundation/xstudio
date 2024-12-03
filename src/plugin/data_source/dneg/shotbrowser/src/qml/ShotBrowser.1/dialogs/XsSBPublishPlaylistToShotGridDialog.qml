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
    title: "Create ShotGrid Playlist"

    property var playlistProperties: null
    property var plIndex: playlistProperties ? playlistProperties.values.index : null

    property int validMediaCount: 0
    property int invalidMediaCount: 0
    property bool linking: false

    property alias playlist_name: playlist_name_.text
    property alias site_name: siteCB.currentText
    property alias playlist_type: ptype.currentText
    property int project_id: 0

    onVisibleChanged: {
        if(visible) {
            playlist_name_.selectAll()
            playlist_name_.forceActiveFocus()
        }
    }

    onPlIndexChanged: {
        if(visible && playlistProperties && playlistProperties.values.actorUuidRole) {
            linking = true
            ShotBrowserHelpers.getValidMediaCount(helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole), updateValidMediaCount)
        }
    }

    XsPreference {
        id: locationPref
        path: "/plugin/data_source/shotbrowser/browser/location"
    }

    XsPreference {
        id: projectPref
        path: "/plugin/data_source/shotbrowser/browser/project"
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

    function publish() {
		// console.log(
		// 	helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole),
		// 	project_id,
		// 	playlist_name,
		// 	site_name,
		// 	playlist_type
		// )

		ShotBrowserHelpers.publishPlaylistToShotGrid(
			helpers.QVariantFromUuidString(playlistProperties.values.actorUuidRole),
			project_id,
			playlist_name,
			site_name,
			playlist_type
		)
		close()
    }

    // function openOnCreate(json) {
    //     try {
    //         var data = JSON.parse(json)
    //         if(data["data"]["id"]){
    //         	ShotBrowserHelpers.loadShotGridPlaylist(data["data"]["id"], data["data"]["attributes"]["code"])
    //         } else {
    //             console.log(err)
    //             dialogHelpers.errorDialogFunc("Publish Playlist To ShotGrid", "Publishing of Playlist to ShotGrid failed.\n\n" + data)
    //         }
    //     } catch(err) {
    //     	console.log(err)
    //         dialogHelpers.errorDialogFunc("Publish Playlist To ShotGrid", "Publishing of Playlist to ShotGrid failed.\n\n" + json)
    //     }
    // }

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
                id: project_label
                text: "Project : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            XsComboBoxEditable{ id: project
                Layout.fillWidth: true
                Layout.preferredHeight: project_label.height*2
                Layout.rightMargin: 6

                model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Project") : []
                textRole: "nameRole"
                valueRole: "idRole"

                onModelChanged: {
                	if(model.length) {
					    let pid = model.searchRecursive(projectPref.value, "nameRole")
                        currentIndex = pid.row
                		project_id = model.get(pid, "idRole")
                	}
				}

                onActivated: {
                	project_id = parseInt(currentValue)
                }
            }


            XsLabel {
                text: "Site : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            XsComboBoxEditable{ id: siteCB
                Layout.fillWidth: true
                Layout.preferredHeight: project_label.height*2
                Layout.rightMargin: 6

                model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Site") : []
                textRole: "nameRole"

                onModelChanged: {
                	currentIndex = find(helpers.expandEnvVars(locationPref.value), Qt.MatchExactly)
                }

                onActivated: {
                	locationPref.value = currentText
                }
            }

            XsLabel {
                id: ptype_label
                text: "Type : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            XsComboBoxEditable{ id: ptype
                Layout.fillWidth: true
                Layout.preferredHeight: project_label.height*2
                Layout.rightMargin: 6

                model: ShotBrowserEngine.ready ? ShotBrowserEngine.presetsModel.termModel("Playlist Type") : []
                textRole: "nameRole"
                onModelChanged: {
                	currentIndex = find("Dailies", Qt.MatchExactly)
                }
            }

            XsLabel {
                text: "Name : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            XsTextEdit {
                id: playlist_name_
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
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
            Keys.onReturnPressed: publish()
            Keys.onEscapePressed: close()

            XsPrimaryButton {
                text: qsTr("Cancel")
                Layout.leftMargin: 10
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: dialog.width / 5

                onClicked: close()
            }
            Item{
            	Layout.fillWidth: true
            }
            XsPrimaryButton {
                text: "Create"
                highlighted: !linking
                enabled: !linking

                Layout.rightMargin: 10
                Layout.minimumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10
                onClicked: publish()
            }
        }
    }
}


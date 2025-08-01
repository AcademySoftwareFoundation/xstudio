// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xStudio 1.0

import xstudio.qml.module 1.0

XsDialogModal {
    id: dialog
    title: "Push Contents To ShotGrid"

    property var playlist_uuid: null
    property int validMediaCount: 0
    property int invalidMediaCount: 0
    property bool linking: false

    property alias playlist_name: playlist_name_.text

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

            TextField {
                id: playlist_name_
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                readOnly: true

                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                font.pixelSize: XsStyle.popupControlFontSize
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                onAccepted: dialog.accept()
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }
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

            XsRoundButton {
                text: qsTr("Cancel")
                Layout.leftMargin: 10
                Layout.bottomMargin: 10
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: dialog.width / 5

                onClicked: {
                     reject()
                }
            }
            XsHSpacer{}
            XsRoundButton {
                text: "Push To ShotGrid"
                highlighted: !linking
                enabled: !linking

                Layout.rightMargin: 10
                Layout.minimumWidth: dialog.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10
                onClicked: {
                    accept()
                }
            }
        }
    }
}


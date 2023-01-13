// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialogModal {
    id: dlg
    property string path
    property var payload

    width: 400
    height: 150

    keepCentered: true
    centerOnOpen: true

    title: "Replace Current Session?"

    function replaceSession() {
        session.load(path, payload)
        app_window.session.new_recent_path(path)
    }

    function replaceSessionTest() {
        // session.newSession("Untitled Session")
        if(session.modified) {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSessionDialog.qml")
            dialog.saved.connect(replaceSession)
            dialog.cancelled.connect(replaceSession)
            dialog.open()
        } else {
            replaceSession()
        }
    }

    function importSession() {
        session.import(path, payload)
        app_window.session.new_recent_path(path)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: text
            text: "Replace Current Session?"
            font.pixelSize: 16
            font.hintingPreference: Font.PreferNoHinting
            font.family: XsStyle.fontFamily
            color: XsStyle.hoverColor
            padding: 10
            font.bold: true
            focus: true

            Keys.onReturnPressed: importSession()
        }


        RowLayout {
            id: myFooter
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 35
            focus: true
            Keys.onReturnPressed: importSession()

            XsRoundButton {
                text: qsTr("Cancel")

                Layout.leftMargin: 10

                Layout.minimumWidth: dlg.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10

                onClicked: close()
            }
            XsHSpacer{}
            XsRoundButton {
                text: qsTr("Import Session")
                highlighted: true

                Layout.minimumWidth: dlg.width / 4
                Layout.fillWidth: true
                Layout.bottomMargin: 10
                Layout.fillHeight: true

                // enabled: false
                onClicked: {
                    importSession()
                    close()
                }
            }
            XsHSpacer{}
            XsRoundButton {
                text: qsTr("Replace Session")

                Layout.rightMargin: 10

                Layout.minimumWidth: dlg.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10

                onClicked:{
                    replaceSessionTest()
                    close()
                }
            }
        }
    }
}

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialogModal {
    id: dlg

    property string comment
    property string action_text

    keepCentered: true
    centerOnOpen: true

    signal canceled()
    signal dont_save()
    signal saved()

    width: 500
    height: 150

    title: "Save Session"

    function saved_saved() {
        saved()
    }

    function saveSession() {
        if(app_window.session.pathNative != "") {
            if(app_window.session.save_session_path(app_window.session.path) == ""){
                app_window.session.new_recent_path(app_window.session.path)
                accept()
                saved()
            } else {
                accept()
                canceled()
            }
        }
        else {
            var dialog = XsUtils.openDialog("qrc:/dialogs/XsSaveSessionDialog.qml")
            dialog.open()
            dialog.saved.connect(saved_saved)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true

            text: dlg.comment
            font.pixelSize: 16
            font.hintingPreference: Font.PreferNoHinting
            font.family: XsStyle.fontFamily
            color: XsStyle.hoverColor
            padding: 10
            font.bold: true
            focus: true

            Keys.onReturnPressed: saveSession()
            //Keys.onEscapePressed: reject()
        }

        RowLayout {
            id: myFooter
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 35
            focus: true

            Keys.onReturnPressed: saveSession()
            //Keys.onEscapePressed: reject()

            XsRoundButton {
                text: qsTr("Cancel")

                Layout.leftMargin: 10

                Layout.minimumWidth: dlg.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10

                onClicked: {
                    canceled()
                    reject()
                }
            }
            XsHSpacer{}
            XsRoundButton {
                text: qsTr("Don't Save")

                Layout.minimumWidth: dlg.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10

                onClicked:{
                    accept()
                    dont_save()
                }
            }
            XsHSpacer{}
            XsRoundButton {
                text: dlg.action_text
                highlighted: true

                Layout.rightMargin: 10
                Layout.minimumWidth: dlg.width / 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10

                // enabled: false
                onClicked: {
                    saveSession()
                }
            }
        }
    }
}

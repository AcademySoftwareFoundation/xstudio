// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.3

import xStudio 1.1

XsDialog {
	id: control
    title: "Select Folder"
    minimumHeight: 130
    minimumWidth: 300

    keepCentered: true
    centerOnOpen: true

    property alias okay_text: okay.text
    property alias cancel_text: cancel.text
    property alias text: text_control.text
    property alias path: path_control.text
    property alias input: text_control

    signal cancelled()
    signal okayed()


    function okaying() {
    	okayed()
    	accept()
    }
    function cancelling() {
    	cancelled()
    	reject()
    }

    FileDialog {
        id: select_path_dialog
        title: "Select Snapshot Path"
        folder: path_control.text || app_window.sessionFunction.defaultSessionFolder() || shortcuts.home

        selectFolder: true
        selectExisting: true
        selectMultiple: false

        onAccepted: {
            path_control.text = select_path_dialog.fileUrls[0]
        }
    }

    Connections {
        target: control
        function onVisibleChanged() {
            if(visible){
    			text_control.selectAll()
    	        text_control.forceActiveFocus()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextField {
                id: path_control
                text: ""
                placeholderText: "Select Snapshot Folder..."

                Layout.fillWidth: true
                Layout.fillHeight: true

                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                font.pixelSize: XsStyle.sessionBarFontSize
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                onAccepted: okaying()
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }
            }
            XsRoundButton {
                id: browse
                text: "Browse..."

                Layout.fillHeight: true
                Layout.minimumWidth: control.width / 5

                onClicked: select_path_dialog.open()
            }
        }

        TextField {
            id: text_control
            placeholderText: "Set Menu Title"
            text: ""

            Layout.fillWidth: true
            Layout.fillHeight: true

            selectByMouse: true
            font.family: XsStyle.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            font.pixelSize: XsStyle.sessionBarFontSize
            color: XsStyle.hoverColor
            selectionColor: XsStyle.highlightColor
            onAccepted: okaying()
            background: Rectangle {
                anchors.fill: parent
                color: XsStyle.popupBackground
                radius: 5
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 20

            focus: true
            Keys.onReturnPressed: okayed()
            Keys.onEscapePressed: cancelled()

            XsRoundButton {
            	id: cancel
                text: "Cancel"

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: control.width / 5

                onClicked: {
                    cancelling()
                }
            }
            XsHSpacer{}
            XsRoundButton {
            	id: okay
                text: "Done"
                highlighted: true

                Layout.minimumWidth: control.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    forceActiveFocus()
                    if(text_control.text == "")
                        text_control.text = path_control.text.split('/').pop()
                    okaying()
                }
            }
        }
    }
}
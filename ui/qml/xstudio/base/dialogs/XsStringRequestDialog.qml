// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3

import xStudio 1.0

XsDialogModal {
	id: control

    minimumHeight: 85
    minimumWidth: 300

    keepCentered: true
    centerOnOpen: true

    property alias okay_text: okay.text
    property alias secondary_okay_text: secondary_okay.text
    property alias cancel_text: cancel.text
    property alias text: text_control.text
    property alias echoMode: text_control.echoMode
    property alias input: text_control

    signal cancelled()
    signal okayed()
    signal secondary_okayed()


    function okaying() {
    	okayed()
    	accept()
    }
    function secondary_okaying() {
    	secondary_okayed()
    	accept()
    }
    function cancelling() {
    	cancelled()
    	reject()
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

        TextField {
            id: text_control
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

            //focus: true
            Keys.onReturnPressed: okayed()
            Keys.onEscapePressed: cancelled()

            XsRoundButton {
            	id: cancel
                text: qsTr("Cancel")

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: control.width / 5

                onClicked: {
                    cancelling()
                }
            }
            XsHSpacer{}
            XsRoundButton {
            	id: secondary_okay
                text: ""

                visible: text != ""
                Layout.fillWidth: true
                Layout.minimumWidth: control.width / 5
                Layout.fillHeight: true

                onClicked:{
                    secondary_okaying()
                }
            }
            XsHSpacer{}
            XsRoundButton {
            	id: okay
                text: "Okay"
                highlighted: true

                Layout.minimumWidth: control.width / 5
                Layout.fillWidth: true
                Layout.fillHeight: true

                onClicked: {
                    okaying()
                }
            }
        }
    }
}
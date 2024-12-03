// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0

XsViewerToolbarButtonBase {

    property real fromValue: 0.0
    property real toValue: 10.0
    property real stepSize: 0.1
    property real sensitivity: float_scrub_sensitivity ? float_scrub_sensitivity : 0.01
    property int decimalDigits: 2

    property var value_ : value ? value : 0.0
    property var prevValue : default_value ? default_value : 0.0

    valueText: value_.toFixed(decimalDigits)

    showBorder: mouseArea.containsMouse

    MouseArea {

        id: mouseArea
        anchors.fill: parent
        cursorShape: Qt.SizeHorCursor
        hoverEnabled: true

        property real prevMX: 0.0
        property real valueOnPress: 0.0

        onMouseXChanged: {
            if(pressed)
            {
                var delta = (mouseX - prevMX)*sensitivity
                value = Math.round((valueOnPress + delta)/stepSize)*stepSize
            }
        }
        onPressed: {
            prevMX = mouseX
            valueOnPress = value
        }

        onDoubleClicked: {
            if(value == default_value){
                value = prevValue
            }
            else{
                prevValue = value
                value = default_value
            }
        }
    }
}


// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import xStudio 1.0
import xstudio.qml.module 1.0

Rectangle {

    id: control
    property var value_: value ? value : 0
    property var min_: integer_min ? integer_min : 0
    property var max_: integer_max ? integer_max : 1
    color: "transparent"

    function setValue(v) {
        value = v
    }

    onValue_Changed: {
        // need this check to stop nasty feedback, as ui event changes the 
        // value in the backend, that is broacast back to the front end that
        // changes the value here, which changes the backend value ad infinitum
        if (!slider.pressed) {
            slider.value = value_;
        }   
    }

    Rectangle {

        id: text_value
        width: 40
        color: XsStyle.mediaInfoBarOffsetBgColor
        border.width: 1
        border.color: ma3.containsMouse ? XsStyle.hoverColor : XsStyle.mainColor
        height: valueInput.font.pixelSize*1.4

        MouseArea {
            id: ma3
            anchors.fill: parent
            hoverEnabled: true
        }

        TextInput {

            id: valueInput
            text: value_.toFixed(0) // 'value' property provided via the xStudio attributes model
            
            color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
            selectByMouse: true
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            anchors.fill: parent

            font {
                family: XsStyle.fontFamily
            }

            onEditingFinished: {
                focus = false
                setValue(parseInt(text))
            }
        }
    }

    Slider {

        id: slider
        to: max_
        from: min_
        stepSize: 1
        orientation: Qt.Horizontal
        value: 0.0

        anchors.left: text_value.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.leftMargin: 5

        handle: Rectangle {
            id: sliderHandle
            y: (slider.height - height) / 2
            x: slider.visualPosition * (slider.width - width)
            width:10
            height:10
            radius:10
            color:XsStyle.hoverColor
        }

        onValueChanged : {
            if (pressed) {
                control.setValue(value)
            }
        }

        background: Rectangle {
            // bar right of handle
            color: XsStyle.transportBackground
            border.width: 1
            y: (slider.height - height) / 2
            height: 6
            radius: width / 2
            border.color: XsStyle.transportBorderColor
            Rectangle {
                // bar left of handle
                width: slider.visualPosition * parent.width
                x: 0
                height: parent.height
                color: XsStyle.primaryColor//XsStyle.highlightColor
                gradient:styleGradient.accent_gradient
                radius: parent.radius
                border.width: 1
                border.color: XsStyle.transportBorderColor
            }
        }
    }
}

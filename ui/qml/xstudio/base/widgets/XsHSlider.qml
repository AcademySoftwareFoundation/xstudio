// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

Rectangle {
    color: "transparent"
    id: control

    property int value
    property int to: 100
    property int from: 0
    property int factor: 1
    property int stepSize: 1
    property int orientation: Qt.Horizontal
    property alias slider: slider
    property alias pressed: slider.pressed

    onValueChanged : {
        slider.value = value
    }

    GridLayout {

        anchors.fill: parent

        signal unPressed()

        columns: 5
        rowSpacing: 1

        Label {
            id: from
            text: {'' + Math.round(control.value/control.factor)}
            font.family: XsStyle.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            color: XsStyle.hoverColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        }


        Slider {
            id: slider
            to: control.to
            from: control.from
            stepSize: control.stepSize
            orientation: Qt.Horizontal
            value: control.value

            snapMode: Slider.SnapAlways
            Layout.fillWidth: true
            Layout.fillHeight: true
            //Layout.preferredWidth: 300
            Layout.preferredHeight: 10
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            handle: Rectangle {
                id: sliderHandle
                x: slider.visualPosition * (slider.width - width)
                y: (slider.height - height) / 2
                width:10
                height:10
                radius:10
                color:XsStyle.hoverColor
            }

            onValueChanged : {
                control.value = value
            }

            background: Rectangle {
                // bar right of handle
                color: XsStyle.transportBackground
                border.width: 1
                y: (slider.height - height) / 2
                width: parent.width
                height: 6
                radius: height / 2
                border.color: XsStyle.transportBorderColor
                Rectangle {
                    // bar left of handle
                    //width: (1.0-slider.visualPosition) * parent.width
                    x: slider.visualPosition * parent.width
                    //width: parent.width
                    color: "#414141"//preferences.accent_color
                    gradient:preferences.accent_gradient
                    radius: parent.radius
                    border.width: 1
                    border.color: XsStyle.transportBorderColor
                }
            }

            onPressedChanged : {
                if (pressed === false) {
                    unPressed()
                }
            }
        }
    }
}
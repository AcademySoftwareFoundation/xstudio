// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

GridLayout {
    id: control

    property int value
    property int to: 100
    property int from: 0
    property int factor: 1
    property int stepSize: 1
    property int orientation: Qt.Vertical
    property alias slider: slider
    property alias pressed: slider.pressed

    signal unPressed()

    columns: 1
    rowSpacing: 5

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

    onValueChanged : {
        slider.value = value
    }

    Slider {
        id: slider
        to: control.to
        from: control.from
        stepSize: control.stepSize
        orientation: Qt.Vertical
        value: control.value

        snapMode: Slider.SnapAlways
        Layout.fillHeight: true
        Layout.preferredWidth: 10
        Layout.preferredHeight: 300
        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

        handle: Rectangle {
            id: sliderHandle
            x: (slider.width - width) / 2
            y: slider.visualPosition * (slider.height - height)
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
            x: (slider.width - width) / 2
            width: 6
            radius: width / 2
            border.color: XsStyle.transportBorderColor
            Rectangle {
                // bar left of handle
                height: (1.0-slider.visualPosition) * parent.height
                y: slider.visualPosition * parent.height
                width: parent.width
                color: XsStyle.primaryColor//XsStyle.highlightColor
                gradient:styleGradient.accent_gradient
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
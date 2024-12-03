// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import xStudio 1.0

Rectangle {

    id: control
    color: "transparent"

    property var value_: value ? value : 0.0
    property var min_: float_scrub_min ? float_scrub_min : 0.0
    property var max_: float_scrub_max ? float_scrub_max : 1.0
    property var step_: float_scrub_step ? float_scrub_step : 1.0

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

    RowLayout {

        anchors.fill: parent

        Rectangle {

            id: text_value
            width: 40
            border.width: 1
            height: valueInput.font.pixelSize*1.4
            color: "transparent"

            MouseArea {
                id: ma3
                anchors.fill: parent
                hoverEnabled: true
            }

            XsTextInput {

                id: valueInput
                text: value_.toFixed(2) // 'value' property provided via the xStudio attributes model
                selectByMouse: true
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                anchors.fill: parent
                onEditingFinished: {
                    focus = false
                    setValue(parseFloat(text))
                }
            }
        }

        XsSlider {

            id: slider
            value: 0.0
            to: max_
            from: min_

            stepSize: step_
            orientation: Qt.Horizontal
            fillColor: palette.base
            baseColor: palette.highlight

            Layout.fillWidth: true
            Layout.fillHeight: true

            onValueChanged: {
                if (pressed) {
                    control.setValue(value)
                }
            }

        }
    }
}

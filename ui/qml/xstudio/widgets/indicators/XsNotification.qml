// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

import xStudio 1.0

// notify widget

Item {
    id: control
    property string text: ""
    property string type: ""
    property real percentage: 0.0

    XsToolTip {
        text: control.text
        delay: 0
        x: -width/2
        y: parent.height
        visible: hover.hovered
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    XsImage {
        source: control.type == "INFO" ? "qrc:/icons/check_circle.svg" : "qrc:/icons/warning.svg"
        visible: ["INFO", "WARN"].includes(control.type)
        imgOverlayColor: control.type == "INFO" ? palette.highlight : "RED"
        anchors.fill: parent
        anchors.margins: 2
    }

    XsGradientRectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * (percentage/100.0)
        orientation: Gradient.Horizontal
        topColor: "transparent"
        bottomColor: palette.highlight
        flatTheme: false
        visible: ["PROGRESS_PERCENTAGE", "PROGRESS_RANGE"].includes(control.type)
    }

    XsBusyIndicator{
        anchors.fill: parent
        anchors.margins: 2
        running: visible
        visible: [ "PROCESSING", "PROGRESS_PERCENTAGE", "PROGRESS_RANGE"].includes(control.type)
        scale: 0.8
        z:1
    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

// notify widget

Item {
    property alias text: tt.text
    property string type: "INFO"
    property real percentage: 0.0

    XsToolTip {
        id: tt
        delay: 0
        x: -width/2
        y: parent.height
        visible: hover.hovered
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    XsIcon {
        source: type == "INFO" ? "qrc:/icons/check_circle.svg" : "qrc:/icons/warning.svg"
        visible: ["INFO", "WARN"].includes(type)
        imgOverlayColor: type == "INFO" ? XsStyleSheet.accentColor : "RED"
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
        bottomColor: XsStyleSheet.accentColor
        useFlatTheme: false
        visible: ["PROGRESS_PERCENTAGE", "PROGRESS_RANGE"].includes(type)
    }

    XsBusyIndicator{
        anchors.fill: parent
        anchors.margins: 2
        running: visible
        visible: [ "PROCESSING", "PROGRESS_PERCENTAGE", "PROGRESS_RANGE"].includes(type)
        scale: 0.8
        z:1
    }
}

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudioReskin 1.0

ToolTip {
    id: widget
    
    property alias textDiv: textDiv
    property alias metricsDiv: metricsDiv

    property color bgColor: palette.text
    property color textColor: palette.base

    delay: 100
    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    TextMetrics {
        id: metricsDiv
        font: textDiv.font
        text: textDiv.text
    }

    contentItem: Text {
        id: textDiv
        text: widget.text
        font: widget.font
        color: textColor
        // width: widget.width
        leftPadding: 4
        rightPadding: 4
        wrapMode: Text.Wrap //WrapAnywhere
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 32
        color: bgColor

        Rectangle {
            id: shadowDiv
            implicitWidth: parent.width
            implicitHeight: parent.height
            color: "#000000"
            opacity: 0.2
            x: 2
            y: -2
            z: -1
        }
    }

}
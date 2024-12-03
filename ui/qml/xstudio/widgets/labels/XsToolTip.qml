// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.0

ToolTip {
    id: widget

    property alias textDiv: textDiv
    property alias metricsDiv: metricsDiv

    property color bgColor: palette.text
    property color textColor: palette.base

    delay: 500

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    width: metricsDiv.width + (XsStyleSheet.panelPadding*3)

    rightPadding: XsStyleSheet.panelPadding
    leftPadding: XsStyleSheet.panelPadding

    TextMetrics {
        id: metricsDiv
        font: widget.font
        text: widget.text
    }

    contentItem: Text {
        id: textDiv
        text: widget.text
        font: widget.font
        color: textColor
        wrapMode: Text.Wrap
    }

    background: Rectangle {
        color: bgColor

        Rectangle {
            id: shadowDiv
            color: "#000000"
            opacity: 0.2
            x: 2
            y: -2
            z: -1
        }
    }

}
// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

TabButton {
    id: control
    palette.windowText: XsStyleSheet.primaryTextColor
    palette.brightText: XsStyleSheet.primaryTextColor

    palette.window: XsStyleSheet.panelTitleBarColor
    palette.mid: XsStyleSheet.panelTitleBarColor
    palette.dark: XsStyleSheet.panelTitleBarColor

    // font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    font.bold: checked


    background: XsGradientRectangle {
        anchors.fill: parent

        border.color: hoverhandle.hovered ? XsStyleSheet.accentColor : "transparent"
        border.width: 1

        flatColor: XsStyleSheet.controlColour

        topColor: control.down ? XsStyleSheet.accentColor : XsStyleSheet.controlColour
        bottomColor: control.down ? XsStyleSheet.accentColor : XsStyleSheet.widgetBgNormalColor

        Rectangle{
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.leftMargin: 2
            anchors.bottomMargin: 3
            anchors.bottom: parent.bottom

            height: 2
            radius: 2
            color: XsStyleSheet.accentColor
            visible: checked
        }
    }

    HoverHandler {
    	id: hoverhandle
    }
}

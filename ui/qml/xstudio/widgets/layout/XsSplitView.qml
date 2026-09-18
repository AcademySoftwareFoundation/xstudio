// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

SplitView {
    property real thumbWidth: XsStyleSheet.panelPadding / 2
    property color colorHandleBg: XsStyleSheet.panelBgColor

    focus: false

    orientation: Qt.Horizontal

    handle: Rectangle {
        implicitWidth: thumbWidth
        implicitHeight: thumbWidth
        color: colorHandleBg

        Rectangle{
            anchors.fill: parent
            color: parent.SplitHandle.pressed ? XsStyleSheet.accentColor : (
                parent.SplitHandle.hovered ? XsStyleSheet.primaryTextColor : "transparent"
            )
        }
    }
}
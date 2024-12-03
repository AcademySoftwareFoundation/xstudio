// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

import xStudio 1.0

SplitView {

    property real thumbWidth: XsStyleSheet.panelPadding/2
    property color colorNormal: XsStyleSheet.primaryTextColor
    property color colorActive: XsStyleSheet.accentColor

    focus: false

    orientation: Qt.Horizontal

    handle: Rectangle {
        implicitWidth: thumbWidth
        implicitHeight: thumbWidth
        color: XsStyleSheet.panelBgColor

        Rectangle{
            anchors.fill: parent
            color: parent.SplitHandle.pressed ? colorActive : parent.SplitHandle.hovered ? colorNormal : "transparent"
        }
    }
}
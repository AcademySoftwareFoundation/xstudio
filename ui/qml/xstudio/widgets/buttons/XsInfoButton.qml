// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

Item {
    property string tooltipText
    property alias maxWidth: tooltip.maxWidth

    XsIcon {

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 5
        width: height
        height: parent.height -2
        source: "qrc:/icons/help.svg"
        imgOverlayColor: ma.containsMouse ? XsStyleSheet.accentColor : XsStyleSheet.hintColor
        antialiasing: true
        smooth: true

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
        }

    }

    XsToolTip {
        id: tooltip
        text: tooltipText
        maxWidth: popup == undefined ? parent.width * 0.7 : popup.width * 0.7
        visible: ma.containsMouse
    }

}
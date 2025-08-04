// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

Item {

    property string tooltipText

    XsIcon {

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 5
        width: height
        height: parent.height -2
        source: "qrc:/icons/help.svg"
        imgOverlayColor: ma.containsMouse ? palette.highlight : XsStyleSheet.hintColor
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
        maxWidth: popup.width*0.7
        visible: ma.containsMouse
    }

}
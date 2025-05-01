// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts



import xstudio.qml.models 1.0
import xStudio 1.0

Item {

    Layout.fillWidth: true
    Layout.preferredWidth: 20
    Layout.fillHeight: true

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
        text: descriptionRole
        maxWidth: prefsLabelWidth*1.5
        visible: ma.containsMouse
    }

}

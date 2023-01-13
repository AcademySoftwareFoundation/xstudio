// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

Item {
    property alias source: myIcon.source
    property alias iconColor: myIconOverlay.color
    property alias sourceSize: myIcon.sourceSize
    property alias myIcon:myIcon

    Image {
        id: myIcon
        // visible: playerWidget.controlsVisible
        sourceSize.height: parent.height
        sourceSize.width: parent.width
        width: parent.height
        height: parent.height
        antialiasing: true
        anchors.centerIn: parent
    }
    ColorOverlay{
        id: myIconOverlay
        // visible: playerWidget.controlsVisible
        anchors.fill: myIcon
        source:myIcon
        color:  XsStyle.hoverColor
        antialiasing: true
    }
}

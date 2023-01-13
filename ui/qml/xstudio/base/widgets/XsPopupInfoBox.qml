// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

Rectangle {

    id: popupBox
    width: 100
    height: 100*opacity
    visible: opacity != 0
    opacity: 0
    property var text: ""
    radius: XsStyle.menuRadius
    property real target_y: 0.0
    y: target_y-height

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    border {
        color: XsStyle.menuBorderColor
        width: XsStyle.menuBorderWidth
    }
    color: XsStyle.mainBackground

    Text {

        id: messageText
        text: popupBox.text ? popupBox.text : ""
        wrapMode: Text.Wrap
        clip: true

        anchors.fill: parent
        anchors.margins: 10

        font.pixelSize: XsStyle.popupControlFontSize
        font.family: XsStyle.controlTitleFontFamily
        color: XsStyle.controlTitleColor
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

    }
}
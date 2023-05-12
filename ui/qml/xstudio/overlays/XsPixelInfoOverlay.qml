// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQml 2.14
import xstudio.qml.module 1.0

import xStudio 1.0

Item {

    id: pixel_info
    anchors.top: parent.top
    anchors.right: parent.right
    width: 100
    height: 100
    property string pixel_info_text: ""

    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: 0.3    
    }

    Text {
        anchors.fill: parent
        text: pixel_info_text
        color: "white"
        font.pixelSize: XsStyle.frameErrorFontSize
        font.family: XsStyle.frameErrorFontFamily
        font.weight: XsStyle.frameErrorFontWeight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        wrapMode: Text.Wrap
        padding: 10
        opacity: 1
    }

}

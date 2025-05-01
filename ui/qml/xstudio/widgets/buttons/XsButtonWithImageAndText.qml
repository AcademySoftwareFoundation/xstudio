// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts

import xStudio 1.0


XsPrimaryButton{ id: widget

    text: ""
    imgSrc: ""
    textDiv.visible: false

    property string iconText: text
    property alias iconSrc: iconDiv.source
    property alias iconRotation: iconDiv.rotation
    property real paddingSpace: 5
    property alias textDiv: textDiv
    

    XsIcon{ id: iconDiv
        x: paddingSpace*2
        source: ""
        anchors.verticalCenter: parent.verticalCenter
        opacity: enabled ? 1 : 0.3
    }
    XsText{ id: textDiv
        text: iconText
        width: parent.width - iconDiv.width - iconDiv.x*2 - anchors.leftMargin
        height: parent.height
        font.pixelSize: XsStyleSheet.fontSize*1.2
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: iconDiv.right
        anchors.leftMargin: paddingSpace
        horizontalAlignment: Text.AlignLeft
        font.bold: true
        opacity: enabled ? 1 : 0.3
        elide: Text.ElideRight
    }

}
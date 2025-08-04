// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts

import xStudio 1.0


XsPrimaryButton{ id: widget

    text: ""
    imgSrc: ""
    textDiv.visible: false
    implicitWidth: layout.width+4

    property string iconText: text
    property alias iconSrc: iconDiv.source
    property alias iconRotation: iconDiv.rotation
    property real paddingSpace: 5
    property alias textDiv: textDiv
    
    RowLayout {

        id: layout
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        XsIcon{ 
            id: iconDiv
            //Layout.fillHeight: true
            //Layout.preferredWidth: height
            source: ""
            opacity: enabled ? 1 : 0.3
        }

        XsText{ 
            id: textDiv
            text: iconText
            Layout.fillHeight: true
            Layout.fillWidth: true
            font.pixelSize: XsStyleSheet.fontSize*1.2
            horizontalAlignment: Text.AlignLeft
            font.bold: true
            opacity: enabled ? 1 : 0.3
            //elide: Text.ElideRight
        }
    
    }

}
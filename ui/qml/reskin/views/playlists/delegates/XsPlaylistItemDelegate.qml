// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: contentDiv
    text: title
    width: parent.width; height: parent.height
    

    font.pixelSize: textSize
    font.family: textFont
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.33

    contentItem:
    Item{
        anchors.fill: parent

       
        Text {
            id: textDiv2
            text: contentDiv.text+"-"+index
            font: contentDiv.font
            color: textColorNormal 
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            topPadding: 2
            bottomPadding: 2
            leftPadding: 8
            
            // anchors.horizontalCenter: parent.horizontalCenter
            elide: Text.ElideRight
            // width: parent.width
            height: parent.height

            // XsToolTip{
            //     text: contentDiv.text
            //     visible: contentDiv.hovered && parent.truncated
            //     width: metricsDiv.width == 0? 0 : contentDiv.width
            // }
        }
    }

    background:
    Rectangle {
        id: bgDiv2
        implicitWidth: 100
        implicitHeight: 40
        border.color: contentDiv.down || contentDiv.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: contentDiv.down? bgColorPressed : forcedBgColorNormal
        
        Rectangle {
            id: bgFocusDiv2
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: contentDiv.activeFocus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }
    }

    onPressed: focus = true
    onReleased: focus = false

}
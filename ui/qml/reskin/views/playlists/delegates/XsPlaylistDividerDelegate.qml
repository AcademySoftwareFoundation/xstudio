// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: dividerDiv
    text: _title
    width: parent.width; 
    height: parent.height

    property color hintColor: XsStyleSheet.hintColor
    property color dividerColor: XsStyleSheet.menuBarColor
    property real btnPadding: XsStyleSheet.menuPadding
    // isSubDivider //#TODO


    font.pixelSize: textSize
    font.family: textFont
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.33

    contentItem:
    Item{
        anchors.fill: parent

        Rectangle{ id: leftLine; height: 1; color: dividerColor; anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left; anchors.leftMargin: isSubDivider? 48:24
            anchors.right: textDiv.left; anchors.rightMargin: 8
        }
        
        Rectangle{ id: rightLine; height: 1; color: dividerColor; anchors.verticalCenter: parent.verticalCenter
            anchors.left: textDiv.right; anchors.leftMargin: 8
            anchors.right: moreBtn.visible? moreBtn.left : parent.right; anchors.rightMargin: 4
        }
        Text {
            id: textDiv
            text: dividerDiv.text+"-"+index
            font: dividerDiv.font
            color: hintColor 
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            topPadding: 2
            bottomPadding: 2
            
            anchors.horizontalCenter: parent.horizontalCenter
            elide: Text.ElideRight
            // width: parent.width
            height: parent.height

            // XsToolTip{
            //     text: dividerDiv.text
            //     visible: dividerDiv.hovered && parent.truncated
            //     width: metricsDiv.width == 0? 0 : dividerDiv.width
            // }
        }

        XsSecondaryButton{ id: moreBtn
            visible: dividerDiv.hovered
            width: 16
            height: 16
            imgSrc: "qrc:/assets/icons/new/more_horiz.svg"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            imgOverlayColor: hintColor
        }
    }

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: dividerDiv.down || dividerDiv.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: dividerDiv.down? bgColorPressed : forcedBgColorNormal
        
        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: dividerDiv.activeFocus
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
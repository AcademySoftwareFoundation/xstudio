// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Item {
    id: dividerDiv
    width: parent.width
    height: parent.height
    opacity: enabled ? 1.0 : 0.33

    property real itemRealHeight: XsStyleSheet.widgetStdHeight -2
    property real panelPadding: XsStyleSheet.panelPadding
    property real buttonWidth: XsStyleSheet.secondaryButtonStdWidth

    property color panelColor: XsStyleSheet.panelBgColor
    property color bgColorPressed: XsStyleSheet.widgetBgNormalColor
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
   
    property color hintColor: XsStyleSheet.hintColor
    property color dividerColor: XsStyleSheet.menuBarColor
    
    property bool isSelected: false
    property bool isSubDivider: false

    Button{ id: visibleItemDiv

        y: panelPadding
        width: parent.width
        height: itemRealHeight
        
        text: nameRole
        font.pixelSize: textSize
        font.family: textFont
        hoverEnabled: true
        padding: 0
        
        contentItem:
        Item{
            width: parent.width - panelPadding //for scrollbar
            height: parent.height //- 5
            anchors.top: parent.top
            anchors.topMargin: panelPadding
            anchors.bottom: parent.bottom
            anchors.bottomMargin: panelPadding

            Rectangle{ id: leftLine; height: 1; color: isSelected?hintColor:dividerColor; anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left; anchors.leftMargin: isSubDivider? buttonWidth*2+panelPadding : buttonWidth
                anchors.right: textDiv.left; anchors.rightMargin: panelPadding*2

            }
            Rectangle{ id: rightLine; height: 1; color: isSelected?hintColor:dividerColor; anchors.verticalCenter: parent.verticalCenter
                anchors.left: textDiv.right; anchors.leftMargin: panelPadding*2
                anchors.right: moreBtn.visible? moreBtn.left : parent.right; anchors.rightMargin: moreBtn.visible? panelPadding : panelPadding*2
            }
            XsText {
                id: textDiv
                property real titlePadding: leftLine.anchors.leftMargin
                text: visibleItemDiv.text
                font: visibleItemDiv.font
                color: hintColor 
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                width: (parent.width-textDiv.titlePadding*2) > textWidth? textWidth : (parent.width-textDiv.titlePadding*2)
                height: parent.height

                // XsToolTip{
                    //     text: visibleItemDiv.text
                    //     visible: visibleItemDiv.hovered && parent.truncated
                    //     width: metricsDiv.width == 0? 0 : visibleItemDiv.width
                    // }
                }

            XsSecondaryButton{ id: moreBtn
                visible: visibleItemDiv.hovered
                width: buttonWidth
                height: buttonWidth
                imgSrc: "qrc:/icons/more_horiz.svg"
                anchors.right: parent.right
                anchors.rightMargin: panelPadding
                anchors.verticalCenter: parent.verticalCenter
                imgOverlayColor: hintColor
            }
        }

        background:
        Rectangle {
            id: bgDiv
            implicitWidth: 100
            implicitHeight: 40
            border.color: visibleItemDiv.down || visibleItemDiv.hovered ? borderColorHovered: borderColorNormal
            border.width: borderWidth
            color: visibleItemDiv.down || isSelected? bgColorPressed : forcedBgColorNormal

            Rectangle {
                id: bgFocusDiv
                implicitWidth: parent.width+borderWidth
                implicitHeight: parent.height+borderWidth
                visible: visibleItemDiv.activeFocus
                color: "transparent"
                opacity: 0.33
                border.color: borderColorHovered
                border.width: borderWidth
                anchors.centerIn: parent
            }
        }

        onPressed: {
            focus = true
            isSelected = !isSelected
        }
        onReleased: focus = false
    }

}
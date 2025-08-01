// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.1

CheckBox { id: widget

    property var indicatorType: "tick" //"box"
    property alias imgIndicator: imgIndicator
    property alias indicatorItem: indicatorItem
    property alias textItem: textItem

    property color bgColorChecked: palette.highlight
    property color bgColorNormal: palette.base

    property color indicatorColor: palette.text
    property color textColorChecked: palette.text
    property color textColorNormal: "light grey"

    property color borderColor: palette.base
    property real borderWidth: 1

    property bool forcedTextHover: false
    property bool forcedHover: false

    font.pixelSize: XsStyle.menuFontSize
    font.family: XsStyle.fontFamily
    checked: false
    activeFocusOnTab: true


    indicator:
    Rectangle { id: indicatorItem
        implicitWidth: 22
        implicitHeight: 22
        x: widget.leftPadding
        y: widget.height/2 - height/2
        radius: 3 //indicatorType=="box" ? 3: implicitHeight/2
        color: indicatorType=="box" ?bgColorNormal: widget.pressed? bgColorChecked: bgColorNormal

        border.color: widget.hovered || forcedHover? bgColorChecked: borderColor
        border.width: borderWidth

        Rectangle {
            width: parent.width/2
            height: width
            anchors.centerIn: parent
            radius: 2
            color: widget.hovered || forcedHover? indicatorColor: Qt.darker(indicatorColor, 1.2)
            visible: widget.checked && indicatorType=="box"
        }
        Image { id: imgIndicator
            visible: widget.checked && indicatorType=="tick"
            source: "qrc:///feather_icons/check.svg"
            sourceSize.height: widget.indicator.width*0.8
            sourceSize.width: widget.indicator.height*0.8
            anchors.centerIn: parent
            layer {
                textureSize: Qt.size(imgIndicator.width, imgIndicator.height)
                enabled: true
                effect:
                ColorOverlay {
                    anchors.fill: imgIndicator; color: widget.hovered || forcedHover? indicatorColor: Qt.darker(indicatorColor, 1.2)
                }
            }
        }
    }
            // layer {
            //     enabled: true
            //     effect:
            //     ColorOverlay {
            //         color: widget.hovered? indicatorColor: Qt.darker(indicatorColor, 1.2)

    contentItem:
    Text { id: textItem
        text: widget.text
        font: widget.font
        opacity: enabled ? 1.0: 0.3
        color: widget.hovered || forcedTextHover || forcedHover? textColorChecked: textColorNormal
        verticalAlignment: Text.AlignVCenter
        leftPadding: widget.indicator.width + widget.spacing
        elide: Text.ElideRight

        XsToolTip{
            x: indicatorItem.width+2
            text: parent.text
            visible: (widget.hovered) && parent.truncated
            width: textItem.contentWidth == 0? 0 : parent.width-indicatorItem.width
        }
    }

    onCheckedChanged: focus=!focus
}

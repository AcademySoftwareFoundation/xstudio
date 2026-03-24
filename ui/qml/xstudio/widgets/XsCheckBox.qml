// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects
import QtQuick.Controls.Basic

import xStudio 1.0

CheckBox { id: widget

    property alias indicatorItem: indicatorItem
    property alias textItem: textItem

    property color indicatorColor: XsStyleSheet.primaryTextColor
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color forcedBgColorNormal: bgColorNormal

    property bool forcedTextHover: false
    property bool forcedHover: false

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    padding: 0
    checked: false
    activeFocusOnTab: true


    indicator:
    Rectangle { id: indicatorItem
        implicitWidth: 22
        implicitHeight: 22
        x: widget.leftPadding
        y: widget.height/2 - height/2
        // color: widget.pressed? XsStyleSheet.accentColor: "transparent"
        color: "transparent"

        XsGradientRectangle{
            id: bgDiv
            anchors.fill: parent

            flatColor: topColor
            topColor: widget.pressed? XsStyleSheet.accentColor: forcedBgColorNormal==bgColorNormal?XsStyleSheet.controlColour:forcedBgColorNormal
            bottomColor: widget.pressed? XsStyleSheet.accentColor: forcedBgColorNormal
        }

        border.color: widget.hovered || forcedHover? XsStyleSheet.accentColor : "transparent"
        border.width: 1

        XsIcon {
            id: imgIndicator
            visible: widget.checked
            source: "qrc:/icons/check.svg"
            anchors.centerIn: parent
            imgOverlayColor: widget.hovered || forcedHover? indicatorColor: Qt.darker(indicatorColor, 1.2)
        }
    }

    contentItem:
    XsText { id: textItem
        text: widget.text
        font: widget.font
        opacity: enabled ? 1.0: 0.3
        color: XsStyleSheet.primaryTextColor //widget.hovered || forcedTextHover || forcedHover
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
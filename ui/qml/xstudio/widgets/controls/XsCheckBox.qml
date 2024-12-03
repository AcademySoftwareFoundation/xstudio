// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.12

import xStudio 1.0

CheckBox { id: widget

    property alias indicatorItem: indicatorItem
    property alias textItem: textItem

    property color indicatorColor: palette.text
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color forcedBgColorNormal: bgColorNormal

    property bool forcedTextHover: false
    property bool forcedHover: false

    property string tooltip_text

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
        // color: widget.pressed? palette.highlight: "transparent"
        color: "transparent"

        XsGradientRectangle{
            id: bgDiv
            anchors.fill: parent

            flatColor: topColor
            topColor: widget.pressed? palette.highlight: forcedBgColorNormal==bgColorNormal?XsStyleSheet.controlColour:forcedBgColorNormal
            bottomColor: widget.pressed? palette.highlight: forcedBgColorNormal
        }

        border.color: widget.hovered || forcedHover? palette.highlight : "transparent"
        border.width: 1

        Image { id: imgIndicator
            visible: widget.checked
            source: "qrc:/icons/check.svg"
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

    contentItem:
    XsText { id: textItem
        text: widget.text
        font: widget.font
        opacity: enabled ? 1.0: 0.3
        color: palette.text //widget.hovered || forcedTextHover || forcedHover
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
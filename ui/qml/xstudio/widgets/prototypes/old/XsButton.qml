// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15 //for ColorOverlay

import xStudio 1.0

Button {
    id: widget

    // font.pixelSize: XsStyle.menuFontSize
    // font.family: XsStyle.fontFamily

    property alias bgDiv: bgDiv
    property alias image: image
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: palette.base
    property color borderColorNormal: palette.base
    property real borderWidth: 1
    property real borderRadius: 2

    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"
    property var textElide: textDiv.elide
    property alias textDiv: textDiv

    property string imgSrc: ""

    property bool isActive: false
    property bool subtleActive: false

    property var tooltip: ""
    property var tooltipTitle: ""
    hoverEnabled: true

    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled ? 1.0: bgColorNormal!=palette.base?1: 0.3
        Image {
            id: image
            visible: imgSrc!=""
            source: imgSrc
            sourceSize.height: parent.height/1.5
            sourceSize.width: parent.width/1.5
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
            anchors.centerIn: parent
            smooth: true
            antialiasing: true
            layer {
                enabled: true
                effect:
                ColorOverlay {
                    color: widget.down || widget.hovered? ((subtleActive && isActive) ? textColorNormal : textColorPressed) : (subtleActive && isActive) ? bgColorPressed : textColorNormal
                }
            }
        }
        Text {
            id: textDiv
            text: widget.text
            font: widget.font
            color: widget.down || widget.hovered? textColorPressed: textColorNormal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            topPadding: 1
            anchors.horizontalCenter: parent.horizontalCenter
            elide: Text.ElideRight
            width: parent.width
            height: parent.height
        }
    }

    ToolTip.text: widget.text
    ToolTip.visible: widget.hovered && textDiv.truncated

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        // opacity: enabled ? 1: 0.3
        color: enabled && widget.hoverEnabled? widget.down || (isActive && !subtleActive)? bgColorPressed: bgColorNormal : Qt.darker(bgColorNormal,1.5)
        border.color: widget.down || widget.hovered ? bgColorPressed: borderColorNormal
        border.width: borderWidth
        radius: borderRadius
    }

    /*onPressed: focus = true
    onReleased: focus = false*/
}


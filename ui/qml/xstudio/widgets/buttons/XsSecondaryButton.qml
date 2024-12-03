// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0

Button {
    id: widget

    text: ""

    property alias imageDiv: imageDiv
    property alias imgSrc: imageDiv.source
    property bool isActive: false
    property bool isColoured: false
    property bool showHoverOnActive: false
    property bool onlyVisualyEnabled: false
    property real imageSrcSize: 16
    property bool forcedHover: false

    property color imgOverlayColor: "#C1C1C1"
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property alias toolTip: toolTip

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    hoverEnabled: true
    smooth: true
    antialiasing: true

    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled || onlyVisualyEnabled ? 1.0 : 0.33
        clip: true

        XsImage {
            id: imageDiv
            source: ""
            imgOverlayColor: isColoured ? palette.highlight : widget.imgOverlayColor
            anchors.centerIn: parent
            sourceSize.height: imageSrcSize
            sourceSize.width: imageSrcSize

            Behavior on rotation {NumberAnimation{duration: 150 }}
        }
        XsText {
            id: textDiv
            visible: imgSrc==""
            text: widget.text
            font: widget.font
            color: textColorNormal
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
        }
    }

    background:
    Rectangle {
        id: bgDiv
        // border.color: widget.down || widget.hovered ? borderColorHovered: borderColorNormal
        border.color: widget.down || widget.hovered  || widget.forcedHover? (showHoverOnActive && isActive)? imgOverlayColor:borderColorHovered: borderColorNormal
        border.width: borderWidth
        color: widget.down || isActive? bgColorPressed : forcedBgColorNormal

        Rectangle {
            id: bgFocusDiv
            implicitWidth: parent.width+borderWidth
            implicitHeight: parent.height+borderWidth
            visible: widget.activeFocus
            color: "transparent"
            opacity: 0.33
            border.color: borderColorHovered
            border.width: borderWidth
            anchors.centerIn: parent
        }
    }


    XsToolTip{
        id: toolTip
        text: widget.text
        visible: false
        width: visible? text.width + 15 : 0
        x: 0 //#TODO: flex/pointer
    }

}


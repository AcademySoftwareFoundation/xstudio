// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: widget
 
    property alias imgSrc: imageDiv.source
    property bool isActive: false
    property bool isActiveViaIndicator: true
    property bool isActiveIndicatorAtLeft: false

    property alias imageDiv: imageDiv
    property color imgOverlayColor: palette.text
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    focusPolicy: Qt.NoFocus

    hoverEnabled: true 

    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled ? 1.0 : 0.33

        XsImage {
            id: imageDiv
            sourceSize.height: 20 //24
            sourceSize.width: 20 //24
            anchors.centerIn: parent
            imgOverlayColor: !pressed && (isActive && !isActiveViaIndicator)? palette.highlight : palette.text
        }

        //#TODO: just for timeline-test
        XsText {
            id: textDiv
            visible: imgSrc==""
            text: widget.text
            font: widget.font
            color: textColorNormal 
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: parent.height
        }
        XsToolTip{
            text: widget.text
            visible: textDiv.visible? widget.hovered && textDiv.truncated : widget.hovered && widget.text!=""
            width: metricsDiv.width == 0? 0 : textDiv.textWidth +10
            // height: widget.height
            x: widget.width //#TODO: flex/pointer
            y: widget.height
        }
    }

    background:
    Rectangle {
        id: bgDiv
        implicitWidth: 100
        implicitHeight: 40
        border.color: widget.down || widget.hovered ? borderColorHovered: borderColorNormal
        border.width: borderWidth

        gradient: Gradient {
            GradientStop { position: 0.0; color: widget.down || (isActive && !isActiveViaIndicator)? bgColorPressed: "#33FFFFFF" }
            GradientStop { position: 1.0; color: widget.down || (isActive && !isActiveViaIndicator)? bgColorPressed: forcedBgColorNormal }
        }

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
        Rectangle{
            anchors.bottom: parent.bottom
            width: isActiveIndicatorAtLeft? borderWidth*3 : parent.width; 
            height: isActiveIndicatorAtLeft? parent.height : borderWidth*3
            color: isActiveViaIndicator && isActive? bgColorPressed : "transparent"
        }
    }

    /*onPressed: focus = true
    onReleased: focus = false*/

}


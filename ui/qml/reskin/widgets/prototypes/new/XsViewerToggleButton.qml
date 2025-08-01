// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: widget

    //#TODO: shortform support on rezise (text-model), via signal

    property bool isActive: false

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property color textColorPressed: "#F1F1F1"
    property color textColorNormal: "light grey"
    property var textElide: textDiv.elide
    property alias textDiv: textDiv
    property bool toggleState: false

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.33

    contentItem:
    Item{
        anchors.fill: parent
       
        Text {
            id: textDiv
            text: widget.toggleState? widget.text+" <b>ON</b>" : widget.text+" <b>OFF</b>"
            font: widget.font
            color: textColorPressed 
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            topPadding: 4
            bottomPadding: 4
            leftPadding: 20
            rightPadding: 20
            elide: Text.ElideRight
            width: parent.width
            height: parent.height

            XsToolTip{
                text: parent.text
                visible: widget.hovered && parent.truncated
                width: widget.width == 0? 0 : 150
                x: 0
            }
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
            GradientStop { position: 0.0; color: widget.down || isActive? bgColorPressed: "#33FFFFFF" }
            GradientStop { position: 1.0; color: widget.down || isActive? bgColorPressed: forcedBgColorNormal }
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
    }

    onPressed: focus = true
    onReleased: focus = false

    onClicked: widget.toggleState = !widget.toggleState
}


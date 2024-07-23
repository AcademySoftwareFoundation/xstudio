// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Button {
    id: widget

    text: ""
    width: 10
    height: 10

    property string hotkeyText: ""
    property string shortText: text
    property bool isShortened: false
    property real shortThresholdWidth: 99
    property bool isShortTextOnly: false
    property real shortOnlyThresholdWidth: 60

    onWidthChanged: {
        if(width < shortThresholdWidth) {
            isShortened = true
            if(width < shortOnlyThresholdWidth) isShortTextOnly = true
            else isShortTextOnly = false
        }
        else {
            isShortened = false
            isShortTextOnly = false
        }
    }

    property bool isActive: false

    property color bgColorPressed: palette.highlight
    property color bgColorNormal: XsStyleSheet.widgetBgNormalColor
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1

    property color textColor: XsStyleSheet.secondaryTextColor
    property var textElide: textDiv.elide
    property alias textDiv: textDiv

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily
    hoverEnabled: true

    contentItem:
    Item{
        anchors.fill: parent
        opacity: enabled ? 1.0 : 0.33
        
        Item{ 
            width: parent.width>itemsWidth? itemsWidth : parent.width
            height: parent.height
            anchors.centerIn: parent
            clip: true

            property real itemsWidth: textDiv.textWidth +statusDiv.textWidth
        
            XsText {
                id: textDiv
                text: isShortened? 
                    widget.shortText : 
                    hotkeyText==""? 
                        widget.text : 
                        widget.text+" ("+hotkeyText+")"
                color: textColor

                anchors.verticalCenter: parent.verticalCenter
            }
            XsText {
                id: statusDiv
                text: isShortTextOnly? "" : value ? " ON":" OFF"
                font.bold: true

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: textDiv.left
                anchors.leftMargin: textDiv.textWidth
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

    /*onPressed: focus = true
    onReleased: focus = false*/
    focusPolicy: Qt.NoFocus

    onFocusChanged: {
        console.log("Button focus", focus)
    }

    onClicked: value = !value
}


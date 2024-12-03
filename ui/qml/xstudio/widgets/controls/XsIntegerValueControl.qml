// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import xStudio 1.0

Control
{

    id: widget
    enabled: true
    Layout.fillWidth: true
    Layout.fillHeight: true

    property bool isPressed: false //mouseArea.containsPress
    property bool isMouseHovered: mouseArea.containsMouse
    property string text
    property string shortText: text.substring(0,3)
    property int fromValue: 0
    property int toValue: 100
    property int defaultValue: toValue
    property int prevValue: defaultValue/2
    property var stepSize: 0.25

    property alias valueText: valueDiv.text

    property bool isShortened: false
    property real shortThresholdWidth: 75
    property bool isShortTextOnly: false
    property bool showValueWhenShortened: false
    property real shortOnlyThresholdWidth: 60

    property color textColor: XsStyleSheet.secondaryTextColor
    property color bgColorPressed: palette.highlight
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    property bool isBgGradientVisible: true

    property bool isActive: false
    property bool subtleActive: false

    signal editingCompleted()
    focusPolicy: Qt.NoFocus
    clip: true

    onWidthChanged: {
        if(width < shortThresholdWidth) {
            isShortened = true
            if(width < shortOnlyThresholdWidth) {
                if(showValueWhenShortened) isShortTextOnly = false
                else isShortTextOnly = true
            }
            else isShortTextOnly = false
        }
        else {
            isShortened = false
            isShortTextOnly = false
        }
    }

    background:
        Rectangle {
            id: bgDiv
            implicitWidth: 100
            implicitHeight: 40
            border.color: widget.isPressed || widget.hovered ? bgColorPressed: borderColorNormal
            border.width: borderWidth
            color: "transparent"

            XsGradientRectangle{
                visible: isBgGradientVisible
                anchors.fill: parent
                flatColor: topColor
                topColor: isPressed || (isActive && !subtleActive)? bgColorPressed: XsStyleSheet.controlColour
                bottomColor: isPressed || (isActive && !subtleActive)? bgColorPressed: forcedBgColorNormal
            }

            Rectangle {
                id: bgFocusDiv
                implicitWidth: parent.width+borderWidth
                implicitHeight: parent.height+borderWidth
                visible: widget.activeFocus
                color: "transparent"
                opacity: 0.33
                border.color: bgColorPressed
                border.width: borderWidth
                anchors.centerIn: parent
            }
        }


    Rectangle{id: midPoint; width:0; height:1; color:"transparent"; x:parent.width/1.5 } //anchors.centerIn: parent}
    Item{
        width: valueDiv.visible?
            parent.width>itemsWidth? itemsWidth : parent.width
            : textDiv.width
        height: textDiv.height
        anchors.centerIn: parent
        clip: true
        opacity: enabled ? 1.0 : 0.33

        property real itemsWidth: textDiv.textWidth + valueDiv.textWidth

        XsText{ id: textDiv
            text: isShortened?
                showValueWhenShortened? "" : widget.shortText
                : widget.text
            color: textColor
            horizontalAlignment: Text.AlignRight
            anchors.verticalCenter: parent.verticalCenter
            clip: true
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
        }

        Rectangle{
            visible: !isBgGradientVisible
            width: valueDiv.width
            height: valueDiv.height
            color: palette.base

            anchors.verticalCenter: valueDiv.verticalCenter
            anchors.left: valueDiv.left
            anchors.leftMargin: 2.8
        }
        XsTextField{ id: valueDiv
            visible: text
            text: "" + value
            bgColorNormal: "transparent"
            borderColor: bgColorNormal
            //focus: isMouseHovered && !isPressed
            onFocusChanged:{
                if(focus) {
                    // drawDialog.requestActivate()
                    selectAll()
                    forceActiveFocus()
                }
                else{
                    deselect()
                }
            }
            maximumLength: 5
            // inputMask: "900"
            inputMethodHints: Qt.ImhDigitsOnly
            // // validator: IntValidator {bottom: 0; top: 100;}
            selectByMouse: false
            width: textWidth

            horizontalAlignment: Text.AlignHCenter
            anchors.left: textDiv.right
            // topPadding: (widget.height-height)/2
            anchors.verticalCenter: parent.verticalCenter

            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily

            // Rectangle{anchors.fill: parent; color: "yellow"; opacity:.3}

            onEditingFinished:{
                // console.log(widget.text,"onEd_F: ", text)
            }
            onEditingCompleted:{
                // console.log(widget.text,"onEd_C: ", text)
                // accepted()

                // console.log("OnAcc: ", text)
                // // if(currentTool != "Erase"){ //#todo
                //     if(parseFloat(text) >= toValue) {
                //         value = toValue
                //     }
                //     else if(parseFloat(text) <= fromValue) {
                //         value = fromValue
                //     }
                //     else {
                //         value = parseFloat(text)
                //     }
                //     text = "" + value
                //     selectAll()
                // // }
            }

            onAccepted:{
                // if(currentTool != "Erase"){ //#todo
                    if(parseInt(text) >= toValue) {
                        value = toValue
                    }
                    else if(parseInt(text) <= fromValue) {
                        value = fromValue
                    }
                    else {
                        value = parseInt(text)
                    }
                    selectAll()
                // }
            }
        }

    }

    MouseArea{
        id: mouseArea
        anchors.fill: parent
        cursorShape: Qt.SizeHorCursor
        hoverEnabled: true
        propagateComposedEvents: true

        property real mouseXOnPress: 0
        property int valueOnPress: defaultValue
        property int lastValue: defaultValue

        onMouseXChanged: {

            if(pressed) {
                value = Math.min(
                    toValue,
                    Math.max(
                        valueOnPress + (mouseX - mouseXOnPress)*stepSize,
                        fromValue)
                        )
            }
        }

        onPressed: {
            mouseXOnPress = mouseX
            if (value != defaultValue) {
                lastValue = value
            }
            valueOnPress = value
        }

        onDoubleClicked: {
            if(value == defaultValue){
                value = lastValue
            }
            else{
                value = defaultValue
            }
        }
    }
}

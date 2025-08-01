// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Controls 1.4
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Control{ id: widget
    enabled: true
    property bool isPressed: false //mouseArea.containsPress
    property bool isMouseHovered: mouseArea.containsMouse
    property string text: ""
    property string shortText: ""
    property real fromValue: 1
    property real toValue: 100
    property real defaultValue: toValue
    property real prevValue: defaultValue/2
    property real valueText: value !== undefined ? value : 0
    property alias stepSize: mouseArea.stepSize
    property int decimalDigits: 2

    property bool isShortened: false
    property real shortThresholdWidth: 99
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

            Rectangle{
                visible: isBgGradientVisible
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: isPressed || (isActive && !subtleActive)? bgColorPressed: "#33FFFFFF" }
                    GradientStop { position: 1.0; color: isPressed || (isActive && !subtleActive)? bgColorPressed: forcedBgColorNormal }
                }
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
        anchors.centerIn: parent
        width: valueDiv.visible? textDiv.width+valueDiv.width : textDiv.width
        height: textDiv.height

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
            text: isShortTextOnly?
                showValueWhenShortened ? valueText : ""
                : " " + valueText.toFixed(decimalDigits)
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
                console.log(widget.text,"OnAccepted: ", text)
                // if(currentTool != "Erase"){ //#todo
                    if(parseFloat(text) >= toValue) {
                        valueText = toValue
                    }
                    else if(parseFloat(text) <= fromValue) {
                        valueText = fromValue
                    }
                    else {
                        valueText = parseFloat(text)
                    }
                    text = "" + valueText
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

        property real prevMX: 0
        property real deltaMX: 0.0
        property real stepSize: 0.25
        property int valueOnPress: 0

        onMouseXChanged: {
            if(isPressed)
            {
                deltaMX = mouseX - prevMX
                let deltaValue = parseFloat(deltaMX*stepSize)
                let valueToApply = valueOnPress + deltaValue //Math.round(valueOnPress + deltaValue)

                if(deltaMX>0)
                {
                    if(valueToApply >= toValue) {
                        value = toValue
                        valueOnPress = toValue
                        prevMX = mouseX
                    }
                    else {
                        value = valueToApply
                    }
                }
                else {
                    if(valueToApply < fromValue){
                        value = fromValue
                        valueOnPress = fromValue
                        prevMX = mouseX
                    }
                    else {
                        value = valueToApply
                    }
                }
            }
        }
        onPressed: {
            prevMX = mouseX
            valueOnPress = value

            isPressed = true
            //focus = true
        }
        onReleased: {
            isPressed = false
            //focus = false
        }
        onDoubleClicked: {
            if(value == defaultValue){
                value = prevValue
            }
            else{
                prevValue = value
                value = defaultValue
            }
        }
    }
}
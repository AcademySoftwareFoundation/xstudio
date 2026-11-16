// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic


import QtQuick.Layouts


import xStudio 1.0

Control
{

    id: widget
    enabled: true
    Layout.fillWidth: true
    Layout.fillHeight: true

    // The value shown and scrubbed. Bind it from the caller; the control never
    // writes it itself, it emits valueEdited(newValue) and the caller pushes
    // that to its model, whose echo comes back through the binding.
    property var value
    signal valueEdited(var newValue)
    readonly property bool bound: value !== undefined && value !== null && !isNaN(value)

    property bool isPressed: false //mouseArea.containsPress
    property bool isMouseHovered: mouseArea.containsMouse
    property string text
    property string shortText: text.substring(0,3)
    property int fromValue: 0
    property int toValue: 100
    // Optional: double-click toggles between this and the previous value.
    // No default means double-click does nothing.
    property var defaultValue: undefined
    property var stepSize: 0.25

    property alias valueText: valueDiv.text

    property bool isShortened: false
    property real shortThresholdWidth: 75
    property bool isShortTextOnly: false
    property bool showValueWhenShortened: false
    property real shortOnlyThresholdWidth: 60

    property color textColor: XsStyleSheet.secondaryTextColor
    property color bgColorPressed: XsStyleSheet.accentColor
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

    function edit(newValue) {
        newValue = Math.min(toValue, Math.max(newValue, fromValue))
        if (newValue !== value) valueEdited(newValue)
    }
    function ignoreUnbound(what) {
        if (typeof helpers !== "undefined")
            helpers.logWarning("XsIntegerValueControl '" + text + "': " + what + " ignored, value is unbound")
    }

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


    RowLayout {

        anchors.centerIn: parent
        clip: true
        opacity: enabled ? 1.0 : 0.33
        spacing: 0

        property real itemsWidth: textDiv.textWidth + valueDiv.textWidth

        XsText{ id: textDiv
            text: isShortened?
                showValueWhenShortened? "" : widget.shortText
                : widget.text
            color: textColor
            clip: true
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
        }

        XsTextField{ id: valueDiv
            visible: text
            text: "" + value
            borderColor: "transparent"
            onFocusChanged:{
                if(focus) {
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

            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily

            onAccepted:{
                if (!bound) { ignoreUnbound("text entry"); return }
                var v = parseInt(text)
                if (!isNaN(v)) edit(v)
                selectAll()
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
        property real valueOnPress: 0
        property real lastValue: 0

        onMouseXChanged: {
            if (pressed && bound)
                edit(valueOnPress + (mouseX - mouseXOnPress)*stepSize)
        }

        onPressed: {
            if (!bound) { ignoreUnbound("drag"); return }
            mouseXOnPress = mouseX
            if (value != defaultValue) {
                lastValue = value
            }
            valueOnPress = value
        }

        onDoubleClicked: {
            if (!bound) { ignoreUnbound("double-click"); return }
            if (defaultValue === undefined) return
            edit(value == defaultValue ? lastValue : defaultValue)
        }
    }
}

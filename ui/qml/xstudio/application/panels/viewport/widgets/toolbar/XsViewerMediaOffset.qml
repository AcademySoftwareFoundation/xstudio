// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.models 1.0

RowLayout {

    id: widget
    property real sensitivity: 0.1
    property int stepSize: 1
    property int default_value: 0

    XsText
    {
        id: titleDisplay
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignRight
        text: "Offset"
        color: XsStyleSheet.secondaryTextColor
        horizontalAlignment: Text.AlignRight
        clip: true
        XsToolTip{
            text: "Hold Shift key for faster scrubbing or type desired offset into box."
            maxWidth: 160
            visible: mouseArea.containsMouse
            x: 20
            y: titleDisplay.height + 5
        }
        MouseArea {

            id: mouseArea
            anchors.fill: parent
            cursorShape: Qt.SizeHorCursor
            hoverEnabled: true

            property real prevMX: 0.0
            property real valueOnPress: 0.0
            property int prevValue: default_value

            onPositionChanged: (mouse)=> {
                if (mouse.buttons == Qt.LeftButton) {
                    var delta = (mouse.x - prevMX)*sensitivity*((mouse.modifiers & Qt.ShiftModifier) ? 10.0 : 1.0)
                    viewportPlayhead.sourceOffsetFrames = Math.round((valueOnPress + delta)/stepSize)*stepSize
                }
            }
            onPressed: {
                prevMX = mouseX
                valueOnPress = viewportPlayhead.sourceOffsetFrames
            }

            onDoubleClicked: {
                if(viewportPlayhead.sourceOffsetFrames == default_value){
                    viewportPlayhead.sourceOffsetFrames = prevValue
                }
                else{
                    prevValue = viewportPlayhead.sourceOffsetFrames
                    viewportPlayhead.sourceOffsetFrames = default_value
                }
            }
        }    
    }

    XsTextField {

        id: valueDisplay
        Layout.minimumWidth: 30
        Layout.alignment: Qt.AlignLeft
        text: "" + viewportPlayhead.sourceOffsetFrames
        clip: true
        font.bold: true
        validator: IntValidator
        property var backendValue: viewportPlayhead.sourceOffsetFrames
        horizontalAlignment: Text.AlignCenter
        onBackendValueChanged: {
            if (text != ("" + backendValue)) {
                text = "" + backendValue
            }
        }
        onTextEdited: {
            var newValue = parseInt(text)
            if (!isNaN(newValue)) {
                viewportPlayhead.sourceOffsetFrames = newValue
            }
        }
        background: Rectangle{
            color: "transparent"
            border.width: valueDisplay.hovered || valueDisplay.activeFocus ? 1:0
            border.color: XsStyleSheet.accentColor
        }
    }
}

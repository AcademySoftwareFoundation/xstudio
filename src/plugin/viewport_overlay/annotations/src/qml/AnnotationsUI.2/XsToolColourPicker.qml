// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.bookmarks 1.0

GridLayout {

    columns: horizontal ? -1 : 1
    rows: horizontal ? 1 : -1
    rowSpacing: 1
    columnSpacing: 1

    property string colourAttributeTitle: "Pen Colour"
    property var attr_group_model
    property var presetModel

    // Exposed for XsColourPresets scope resolution
    property var currentColorPresetModel: presetModel
    property alias tool_colour_value: colour_attr.value

    Layout.fillWidth: horizontal ? false : true
    Layout.fillHeight: horizontal ? true : false

    XsAttributeValue {
        id: colour_attr
        attributeTitle: colourAttributeTitle
        model: attr_group_model
    }

    XsColourDialog {
        id: colourDialog
        title: "Please pick a colour"
        property var lastColour

        linkColour: tool_colour_value

        onCurrentColourChanged: {
            tool_colour_value = currentColour
        }
        onAccepted: {
            close()
        }
        onRejected: {
            tool_colour_value = lastColour
            close()
        }
        onVisibleChanged: {
            if (visible) {
                currentColour = tool_colour_value
                lastColour = tool_colour_value
            }
        }
    }

    Item {

        id: colourProp

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ? horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false

        property bool isPressed: false
        property bool isMouseHovered: colorMArea.containsMouse

        XsGradientRectangle {
            anchors.fill: parent
            border.color: colourProp.isMouseHovered ? XsStyleSheet.accentColor : "transparent"
            border.width: 1

            flatColor: topColor
            topColor: colourProp.isPressed ? XsStyleSheet.accentColor : XsStyleSheet.controlColour
            bottomColor: colourProp.isPressed ? XsStyleSheet.accentColor : "#1AFFFFFF"
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 2
            Layout.margins: 2

            XsLabel {
                text: "Colour"
                color: XsStyleSheet.secondaryTextColor
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: tool_colour_value ? tool_colour_value : "grey"
                border.width: 1
                border.color: "black"
            }
        }

        MouseArea {
            id: colorMArea
            propagateComposedEvents: true
            hoverEnabled: true
            anchors.fill: parent
            onClicked: {
                parent.isPressed = false
                colourDialog.show()
            }
            onPressed: {
                parent.isPressed = true
            }
            onReleased: {
                parent.isPressed = false
            }
        }
    }

    XsColourPresets {
        id: row3_colourpresets

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ? horizontal ? -1 : buttonHeight*2 : 0
        Layout.preferredWidth: visible ? horizontal ? buttonHeight*4 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3
import QtGraphicalEffects 1.15

import xStudio 1.0
import xstudio.qml.models 1.0

Item{

    XsAttributeValue {
        id: __action_attr
        attributeTitle: "action_attribute"
        model: annotations_model_data
    }

    property alias action_attribute: __action_attr.value

    ListView{ id: toolActionUndoRedo

        width: parent.width - framePadding*2
        height: buttonHeight
        x: framePadding
        y: framePadding + spacing/2

        spacing: itemSpacing
        interactive: false
        orientation: ListView.Horizontal

        model: ListModel{
            id: modelUndoRedo
            ListElement{
                action: "Undo"
            }
            ListElement{
                action: "Redo"
            }
        }

        delegate: XsPrimaryButton{
            text: model.action
            width: toolActionUndoRedo.width/modelUndoRedo.count - toolActionUndoRedo.spacing
            height: buttonHeight

            onClicked: {
                action_attribute = text
            }
        }
    }

    ListView{ id: toolActionCopyPasteClear

        width: parent.width - framePadding*2
        height: buttonHeight
        x: framePadding //+ spacing/2
        y: toolActionUndoRedo.y + toolActionUndoRedo.height + spacing

        spacing: itemSpacing
        interactive: false
        orientation: ListView.Horizontal

        model:
        ListModel{
            id: modelCopyPasteClear
            ListElement{ action: "Clear" }
        }

        delegate:
        XsPrimaryButton{
            text: model.action
            width: toolActionCopyPasteClear.width/modelCopyPasteClear.count - toolActionCopyPasteClear.spacing
            height: buttonHeight
            enabled: text == "Clear"

            onClicked: {
                action_attribute = text
            }

        }
    }

    Rectangle{ id: displayAnnotations
        width: parent.width - framePadding
        height: buttonHeight;
        color: "transparent";
        anchors.top: toolActionCopyPasteClear.bottom
        anchors.topMargin: colSpacing
        anchors.horizontalCenter: parent.horizontalCenter

        Text{
            text: "Display Annotations"
            font.pixelSize: fontSize
            font.family: fontFamily
            color: toolInactiveTextColor
            width: parent.width
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: framePadding
        }
    }

    // Sigh - hooking up the draw mode backen attr to the combo box here
    // is horrible! Need something better than this!
    XsModuleData {
        id: annotations_tool_draw_mode_model
        modelDataName: "annotations_tool_draw_mode"
    }

    XsAttributeValue {
        id: __display_mode
        attributeTitle: "Display Mode"
        model: annotations_tool_draw_mode_model
    }
    property alias display_mode: __display_mode.value

    XsAttributeValue {
        id: __display_mode_options
        attributeTitle: "Display Mode"
        model: annotations_tool_draw_mode_model
        role: "combo_box_options"
    }
    property alias display_mode_options: __display_mode_options.value

    XsComboBox {

        id: dropdownAnnotations

        model: display_mode_options
        width: parent.width/1.3;
        height: buttonHeight
        anchors.top: displayAnnotations.bottom
        // anchors.topMargin: itemSpacing
        anchors.horizontalCenter: parent.horizontalCenter
        onCurrentTextChanged: {
            if (currentText != display_mode && display_mode != undefined) {
                display_mode = currentText
            }
        }
        property var displayModeValue: display_mode
        onDisplayModeValueChanged: {

            if (display_mode_options.indexOf(display_mode) != -1) {
                currentIndex = display_mode_options.indexOf(display_mode)
            }
        }

    }

}


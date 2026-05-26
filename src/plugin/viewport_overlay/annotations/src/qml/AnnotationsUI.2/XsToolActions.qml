// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.bookmarks 1.0


GridLayout {

    id: toolActionsDiv
    columns: horizontal ? -1 : 1
    rows: horizontal ? 1 : -1
    rowSpacing: itemSpacing
    columnSpacing: itemSpacing

    XsAttributeValue {
        id: __action_attr
        attributeTitle: "action_attribute"
        model: annotations_model_data
    }

    property alias action_attribute: __action_attr.value

    XsHotkeyReference {
        id: annotation_undo_hk
        hotkeyName: "Undo (Annotation edit)"
    }

    XsHotkeyReference {
        id: annotation_redo_hk
        hotkeyName: "Redo (Annotation edit)"
    }

    XsHotkeyReference {
        id: annotation_delete_all_hk
        hotkeyName: "Delete all strokes"
    }

    XsHotkeyReference {
        id: toggle_visibility_hk
        hotkeyName: "Toggle annotation visibility"
    }

    XsAttributeValue {
        id: __annotations_visible
        attributeTitle: "Visibility"
        model: annotations_model_data
    }

    property alias annotations_visible: __annotations_visible.value

    RowLayout {
        spacing: itemSpacing
        Layout.fillWidth: horizontal ? false : true

        Repeater {

            model: ListModel{
                id: modelUndoRedo

                Component.onCompleted: {
                    append({
                        "action": "Undo",
                        "icon": "qrc:///anno_icons/undo.svg",
                        "toolTip":  "Hotkey: " + annotation_undo_hk.sequence});
                    append({
                        "action": "Redo",
                        "icon": "qrc:///anno_icons/redo.svg",
                        "toolTip":  "Hotkey: " + annotation_redo_hk.sequence});
                    append({
                        "action": "Clear",
                        "icon": "qrc:///anno_icons/delete.svg",
                        "toolTip":  "Hotkey: " + annotation_delete_all_hk.sequence});
                }

            }


            XsPrimaryButton {

                text: "" //model.action

                imgSrc: model.icon
                toolTip: model.toolTip

                Layout.fillWidth: horizontal ? false : true
                Layout.preferredHeight: horizontal ? -1 : 24
                Layout.preferredWidth: horizontal ? 24 : -1
                Layout.fillHeight: horizontal ? true : false

                onClicked: {
                    action_attribute = [model.action, view.name]
                }

            }

        }
    }

    Item {
        Layout.fillWidth: horizontal ? true : false
        Layout.fillHeight: horizontal ? false : true
        visible: horizontal
    }

    XsPrimaryButton {
        text: "Visibility"
        toolTip: "Toggle annotation visibility. Hotkey: " + toggle_visibility_hk.sequence
        isActive: annotations_visible

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
        Layout.preferredWidth: horizontal ? 70 : -1

        onClicked: {
            action_attribute = annotations_visible
                ? ["HideVisibility", view.name]
                : ["ShowVisibility", view.name]
        }
    }

}

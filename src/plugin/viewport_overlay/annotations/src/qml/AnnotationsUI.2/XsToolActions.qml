// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.bookmarks 1.0


RowLayout { 
    
    id: toolActionsDiv
    spacing: itemSpacing

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
                action_attribute = model.action + " " + view.name
            }

        }
        
    }

}


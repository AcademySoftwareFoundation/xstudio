// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xstudio.qml.bookmarks 1.0



import xStudio 1.0
import xstudio.qml.models 1.0

RowLayout { 
    
    id: toolActionsDiv
    spacing: itemSpacing

    XsAttributeValue {
        id: __action_attr
        attributeTitle: "action_attribute"
        model: annotations_model_data
    }

    property alias action_attribute: __action_attr.value

    Repeater { 
        
        model: ListModel{
            id: modelUndoRedo
            ListElement{
                action: "Undo"
                icon: "qrc:///anno_icons/undo.svg"
            }
            ListElement{
                action: "Redo"
                icon: "qrc:///anno_icons/redo.svg"
            }
            ListElement{
                action: "Clear"
                icon: "qrc:///anno_icons/delete.svg"
            }
        }

        XsPrimaryButton {

            text: "" //model.action
            imgSrc: model.icon
            Layout.fillWidth: horizontal ? false : true
            Layout.preferredHeight: horizontal ? -1 : 24
            Layout.preferredWidth: horizontal ? 24 : -1
            Layout.fillHeight: horizontal ? true : false
    
            onClicked: {
                action_attribute = model.action
            }
        }
        
    }

}


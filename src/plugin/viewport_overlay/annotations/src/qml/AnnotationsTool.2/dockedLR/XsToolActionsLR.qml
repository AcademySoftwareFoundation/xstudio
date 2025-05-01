// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xstudio.qml.bookmarks 1.0



import xStudio 1.0
import xstudio.qml.models 1.0

Item{ id: toolActionsDiv

    property alias toolActionUndoRedo: toolActionUndoRedo

    XsAttributeValue {
        id: __action_attr
        attributeTitle: "action_attribute"
        model: annotations_model_data
    }

    property alias action_attribute: __action_attr.value

    ListView{ id: toolActionUndoRedo

        x: framePadding
        width: parent.width - x*2
        height: XsStyleSheet.primaryButtonStdHeight

        spacing: itemSpacing
        interactive: false
        orientation: ListView.Horizontal

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

        delegate: XsPrimaryButton{
            text: "" //model.action
            imgSrc: model.icon
            width: toolActionUndoRedo.width/modelUndoRedo.count - toolActionUndoRedo.spacing
            height: toolActionUndoRedo.height 

            onClicked: {
                action_attribute = model.action
            }
        }
        
    }

}


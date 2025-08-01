// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0

Item{ 
    
    // Rectangle{ id: divLine
    //     anchors.centerIn: parent
    //     width: parent.width - height*2
    //     height: frameWidth
    //     color: frameColor
    //     opacity: frameOpacity
    // }

    // note this model only has one item, which is the 'scribble mode' attribute
    // in the backend.
    XsModuleAttributesModel {
        id: anno_draw_mode_backend
        attributesGroupNames: "anno_scribble_mode_backend_0"
    }

    // we have to use a repeater to hook the model into the ListView
    Repeater {

        id: the_view
        width: parent.width-framePadding_x2
        height: buttonHeight
        model: anno_draw_mode_backend

        ListView{ 
            
            id: drawList
        
            // x: spacing/2
            // width: parent.width/1.3
            // height: buttonHeight/1.3
            width: parent.width-framePadding_x2
            height: buttonHeight
            x: framePadding + spacing/2
            anchors.verticalCenter: parent.verticalCenter
            spacing: itemSpacing //(itemSpacing!==0)?itemSpacing/2: 0
            clip: true
            interactive: false
            orientation: ListView.Horizontal

            model: combo_box_options
            delegate:
            XsButton{
                text: (combo_box_options && index >= 0) ? combo_box_options[index] : ""
                width: drawList.width/combo_box_options.length-drawList.spacing
                height: drawList.height
                // radius: (height/1.2)
                enabled: combo_box_options_enabled ? combo_box_options_enabled[index] : true
                isActive: value == text
                font.pixelSize: fontSize
            
                onClicked: {
                    // 'value' comes from the model .. it's a connection to 
                    // the value of the 'Scribble Mode' attribute in the backend
                    value = text
                }
            }
        }
    }
}


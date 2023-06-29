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

    XsModuleAttributesModel {
        id: anno_font_options
        attributesGroupNames: "annotations_tool_fonts"
    }

    Repeater {

        // Using a repeater here - but 'vp_mouse_wheel_behaviour_attr' only
        // has one row by the way. The use of a repeater means the model role
        // data are all visible in the XsComboBox instance.
        model: anno_font_options

        XsComboBox { 
        
            id: dropdownFonts
    
            width: parent.width-framePadding_x2 -itemSpacing*2
            height: buttonHeight
            model: combo_box_options
            anchors.centerIn: parent
            property var value_: value ? value : null
            onValue_Changed: {
                currentIndex = indexOfValue(value_)
            }
            Component.onCompleted: currentIndex = indexOfValue(value_)
            onCurrentValueChanged: {
                value = currentValue;
            }
    
        }
    }
    
}
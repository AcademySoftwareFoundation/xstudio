// SPDX-License-Identifier: Apache-2.0

import QtQuick

import QtQuick.Layouts

import xstudio.qml.bookmarks 1.0

import xStudio 1.0
import xstudio.qml.models 1.0

Item{ 
    
    id: textCategory

    XsModuleData {
        id: anno_font_options
        modelDataName: "annotations_tool_fonts"
    }

    Repeater {

        // Using a repeater here - but 'vp_mouse_wheel_behaviour_attr' only
        // has one row by the way. The use of a repeater means the model role
        // data are all visible in the XsComboBox instance.
        model: anno_font_options

        XsComboBox {

            id: dropdownFonts
            x: itemSpacing*2
            width: parent.width
            height: parent.height
            model: combo_box_options

            property var value_: value ? value : null
            onValue_Changed: {
                currentIndex = indexOfValue(value_)
            }

            onCurrentValueChanged: {
                value = currentValue;
            }

            Component.onCompleted: currentIndex = indexOfValue(value_)
        }
    }


}
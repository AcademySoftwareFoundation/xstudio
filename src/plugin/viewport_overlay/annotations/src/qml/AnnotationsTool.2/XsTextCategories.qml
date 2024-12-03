// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.models 1.0

Item{ id: textCategory

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
            height: buttonHeight
            model: combo_box_options
            anchors.verticalCenter: parent.verticalCenter

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
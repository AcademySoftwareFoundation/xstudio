// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xstudio.qml.models 1.0
import xStudio 1.0

XsComboBox {

    textColorNormal: XsStyleSheet.primaryTextColor

    property var attr_title
    property var attr_model_name
    property alias model_data: model_data

    /* Access the attribute data for the OCIO controls */
    XsModuleData {
        id: model_data
        modelDataName: attr_model_name
    }

    XsAttributeValue {
        id: options
        attributeTitle: attr_title
        role: "combo_box_options"
        model: model_data
    }

    XsAttributeValue {
        id: attr_value
        attributeTitle: attr_title
        model: model_data
    }

    model: options.value

    property var value_: attr_value.value

    onValue_Changed: {
        currentIndex = indexOfValue(value_)
    }

    Component.onCompleted: currentIndex = indexOfValue(value_)

    onCurrentValueChanged: {
        if (attr_value.value != currentValue) {
            attr_value.value = currentValue;
        }
    }

    /*XsAttributeValue {
        id: tooltip
        attributeTitle: attr_title
        role: "tooltip"
        model: model_data
    }

    XsToolTip {
        visible: text != "" && combo_box.hovered
        text: tooltip.value ? tooltip.value : ""
    }*/
    
}



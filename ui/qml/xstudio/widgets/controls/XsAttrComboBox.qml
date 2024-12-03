// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xstudio.qml.models 1.0
import xStudio 1.0

XsComboBox {

    textColorNormal: palette.text

    property var attr_title
    property var attr_model_name

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
}



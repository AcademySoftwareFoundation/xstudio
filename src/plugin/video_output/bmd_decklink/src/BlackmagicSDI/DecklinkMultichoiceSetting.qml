// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

XsComboBox {

    id: combo_box

    property var attr_name
    property var attrs_model

    XsAttributeValue {
        id: __value
        attributeTitle: attr_name
        model: attrs_model
    }
    property alias value: __value.value

    XsAttributeValue {
        id: __choices
        attributeTitle: attr_name
        model: attrs_model
        role: "combo_box_options"
    }
    property alias choices: __choices.value

    model: choices

    property var value_: value ? value : null
    onValue_Changed: {
        currentIndex = indexOfValue(value_)
    }
    Component.onCompleted: currentIndex = indexOfValue(value_)
    onCurrentValueChanged: {
        if (value != currentValue) {
            value = currentValue;
        }
    }
}


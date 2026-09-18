// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import QtQml 2.15

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

GridLayout {

    id: toolProperties

    property var root

    columns: horizontal ? -1 : 1
    rows: horizontal ? 1 : -1
    rowSpacing: 1
    columnSpacing: 1

    XsModuleData {
        id: annotations_model_data
        modelDataName: "annotations_tool_settings"
    }

    XsToolAttrControl {
        attr_group_model: annotations_model_data
        attr_title: "Dodge Softness"
        text: "Softness"
    }

    XsToolAttrControl {
        attr_group_model: annotations_model_data
        attr_title: "Dodge Size"
        text: "Size"
    }

    XsToolAttrControl {
        attr_group_model: annotations_model_data
        attr_title: "Dodge Intensity"
        text: "Intensity"
    }

    XsBlendModeToggle {
        attr_group_model: annotations_model_data
        blendModeAttributeTitle: "Dodge Blend Mode"
    }

}

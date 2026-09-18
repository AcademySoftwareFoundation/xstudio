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

    XsTextCategories {
        id: textCategories

        Layout.fillWidth: horizontal ? false : true
        Layout.preferredHeight: visible ? horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
        Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
        Layout.fillHeight: horizontal ? true : false
    }

    XsToolAttrControl {
        attr_group_model: annotations_model_data
        attr_title: "Text Size"
        text: "Size"
    }

    XsToolAttrControl {
        attr_group_model: annotations_model_data
        attr_title: "Pen Opacity"
        text: "Opacity"
    }

    XsToolAttrControl {
        attr_group_model: annotations_model_data
        attr_title: "Text Background Opacity"
        text: "BG Opa."
    }

    XsToolColourPicker {
        colourAttributeTitle: "Pen Colour"
        attr_group_model: annotations_model_data
        presetModel: textColourPresetsModel
    }

}

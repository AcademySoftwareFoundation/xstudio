// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item {

    property string blendModeAttributeTitle
    property var attr_group_model

    Layout.fillWidth: horizontal ? false : true
    Layout.preferredHeight: visible ? horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight : 0
    Layout.preferredWidth: visible ? horizontal ? XsStyleSheet.primaryButtonStdHeight*3 : -1 : 0
    Layout.fillHeight: horizontal ? true : false

    XsAttributeValue {
        id: blend_mode_val
        attributeTitle: blendModeAttributeTitle
        model: attr_group_model
    }

    RowLayout {
        anchors.fill: parent
        spacing: 1

        XsPrimaryButton {
            text: "Add"
            Layout.fillWidth: true
            Layout.fillHeight: true
            isActive: blend_mode_val.value === "Add"
            onClicked: blend_mode_val.value = "Add"
        }
        XsPrimaryButton {
            text: "Mult"
            Layout.fillWidth: true
            Layout.fillHeight: true
            isActive: blend_mode_val.value === "Mult"
            onClicked: blend_mode_val.value = "Mult"
        }
    }
}

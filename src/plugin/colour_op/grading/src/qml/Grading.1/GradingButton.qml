// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0

import xstudio.qml.module 1.0

XsTrayButton {
    // prototype: true

    anchors.fill: parent
    text: "Draw"
    source: "qrc:/icons/colour_correction.png"
    tooltip: "Open the Colour Correction Panel.  Apply SOP and LGG colour offsets to selected Media."
    buttonPadding: pad
    toggled_on: gradingToolActive
    onClicked: {
        // toggle the value in the "grading_tool_active" backend attribute
        if (grading_settings.grading_tool_active != null) {
            grading_settings.grading_tool_active = !grading_settings.grading_tool_active
        }
    }

    property var gradingDialog

    property bool dialogVisible: gradingDialog ? gradingDialog.visible : false

    onDialogVisibleChanged: {
        if (grading_settings.grading_tool_active && dialogVisible == false) {
            grading_settings.grading_tool_active = false
        }
    }

    // connect to the backend module to give access to attributes
    XsModuleAttributes {
        id: grading_settings
        attributesGroupNames: "grading_settings"
    }

    // make a read only binding to the "grading_tool_active" backend attribute
    property bool gradingToolActive: grading_settings.grading_tool_active ? grading_settings.grading_tool_active : false

    onGradingToolActiveChanged:
    {
        // there are two GradingButtons - one for main win, one for pop-out,
        // but we only want one instance of the GradingDialog .. this test
        // should ensure that is the case
        if (sessionWidget.is_main_window) {
            if (gradingDialog === undefined) {
                try {
                    gradingDialog = Qt.createQmlObject("import Grading 1.0; GradingDialog {}", app_window, "dynamic")
                } catch (err) {
                    console.error(err);
                }
            }
            if (gradingToolActive) {
                gradingDialog.show()
                gradingDialog.requestActivate()
            } else {
                gradingDialog.hide()
            }
        }
    }

}
// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0


XsTrayButton {
    // prototype: true

    anchors.fill: parent
    text: "Draw"
    source: "qrc:/icons/colour_correction.png"
    tooltip: "Open the Demo Grading Tool Panel."
    buttonPadding: pad
    toggled_on: gradingToolActive
    onClicked: {
        // toggle the value in the "grading_tool_active" backend attribute
        if (grading_demo_backend_settings.grading_tool_active != null) {
            grading_demo_backend_settings.grading_tool_active = !grading_demo_backend_settings.grading_tool_active
        }
    }

    Text {
        id: txt
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 3
        anchors.bottom: parent.bottom
        text: "demo"        
        color: "yellow"
        font.pixelSize: 9
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter    
    }

    DropShadow {
        anchors.fill: txt
        source: txt
        verticalOffset: 2
        color: "black"
        radius: 1
        samples: 3
    }

    property var gradingDemoDialog

    property bool dialogVisible: gradingDemoDialog ? gradingDemoDialog.visible : false

    onDialogVisibleChanged: {
        if (grading_demo_backend_settings.grading_tool_active && dialogVisible == false) {
            grading_demo_backend_settings.grading_tool_active = false
        }
    }

    // connect to the backend module to give access to attributes
    XsModuleAttributes {
        id: grading_demo_backend_settings
        attributesGroupNames: "grading_demo_settings"
    }

    // make a read only binding to the "grading_tool_active" backend attribute
    property bool gradingToolActive: grading_demo_backend_settings.grading_tool_active ? grading_demo_backend_settings.grading_tool_active : false

    onGradingToolActiveChanged:
    {
        // there are two GradingDemoButtons - one for main win, one for pop-out,
        // but we only want one instance of the GradingDemoDialog .. this test
        // should ensure that is the case
        if (sessionWidget.is_main_window) {
            if (gradingDemoDialog === undefined) {
                try {
                    gradingDemoDialog = Qt.createQmlObject("import GradingDemo 1.0; GradingDemoDialog {}", app_window, "dnyamic")
                } catch (err) {
                    console.error(err);
                }
            }
            if (gradingToolActive) {
                gradingDemoDialog.show()
                gradingDemoDialog.requestActivate()
            } else {
                gradingDemoDialog.hide()
            }
        }
    }

}
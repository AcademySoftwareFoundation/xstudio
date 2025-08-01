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
    source: "qrc:///icons/drawing.png"
    tooltip: "Open the Annotations Panel.  Sketch, Laser Onion-Skinning and Text modes."
    buttonPadding: pad
    toggled_on: annotationToolActive
    onClicked: {
        // toggle the value in the "annotations_tool_active" backend attribute
        if (anno_tool_backend_settings.annotations_tool_active != null) {
            anno_tool_backend_settings.annotations_tool_active = !anno_tool_backend_settings.annotations_tool_active
        }
    }

    property var drawingDialog


    property bool dialogVisible: drawingDialog ? drawingDialog.visible : false

    onDialogVisibleChanged: {
        if (anno_tool_backend_settings.annotations_tool_active && dialogVisible == false) {
            anno_tool_backend_settings.annotations_tool_active = false
        }
    }

    // connect to the backend module to give access to attributes
    XsModuleAttributes {
        id: anno_tool_backend_settings
        attributesGroupNames: "annotations_tool_settings_0"
    }

    // make a read only binding to the "annotations_tool_active" backend attribute
    property bool annotationToolActive: anno_tool_backend_settings.annotations_tool_active ? anno_tool_backend_settings.annotations_tool_active : false

    onAnnotationToolActiveChanged:
    {
        // there are two AnnotationsButtons - one for main win, one for pop-out,
        // but we only want one instance of the AnnotationsDialog .. this test
        // should ensure that is the case
        if (sessionWidget.is_main_window) {
            if (drawingDialog === undefined) {
                drawingDialog = Qt.createQmlObject("import AnnotationsTool 1.0; AnnotationsDialog {}", app_window, "dnyamic")
            }
            if (annotationToolActive) {
                drawingDialog.show()
                drawingDialog.requestActivate()
            } else {
                drawingDialog.hide()
            }
        }
    }

}
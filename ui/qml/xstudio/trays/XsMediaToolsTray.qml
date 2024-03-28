// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Layouts 1.15

import xstudio.qml.module 1.0

import xStudio 1.0

RowLayout {

    property int pad: 0

    property var transformDialog: undefined
    property var colourDialog: undefined
    property var bookmarkDialog: undefined
    property var notesDialog: undefined
    property var drawingDialog: undefined
    property var shotgunBrowserDialog: undefined

    XsTrayButton {
        Layout.fillHeight: true
        text: "Settings Dialog"
        source: "qrc:///feather_icons/settings.svg"
        buttonPadding: pad
        prototype: false
        tooltip: "Open the Display Settings Panel.  Configure the HUD, Mask."
        toggled_on: sessionWidget.is_main_window ? sessionWidget.settings_dialog_visible : false
        visible: sessionWidget.is_main_window
        onClicked: {
            sessionWidget.toggleSettingsDialog()
        }
    }

    XsTrayButton {
        Layout.fillHeight: true
        prototype: true
        text: "Transform"
        source: "qrc:/icons/transform.png"
        tooltip: "Open the Transform Panel.  Move, rotate, scale and skew selected Media.."
        buttonPadding: pad
        toggled_on: transformDialog ? transformDialog.visible : false
        onClicked: {
            toggleTransformDialog()
        }
    }

    // this is a mess as we wanted to put the grading tool to the left of the
    // notes button ... with re-skin UI this will go away.
    XsOrderedModuleAttributesModel {
        id: attrs0
        attributesGroupNames: "media_tools_buttons_0"
    }


    ListView {
        id: extra_dudes0
        Layout.fillHeight: true
        Layout.minimumWidth: count * 32
        model: attrs0
        focus: true
        orientation: ListView.Horizontal
        delegate:
            Item {
                id: parent_item
                width: 32
                height: extra_dudes0.height
                property var dynamic_widget
                property var qml_code_: qml_code ? qml_code : null

                onQml_code_Changed: {
                    if (qml_code_) {
                        dynamic_widget = Qt.createQmlObject(qml_code_, parent_item)
                    }
                }
            }
    }

    XsTrayButton {
        Layout.fillHeight: true
        text: "Notes"
        source: "qrc:/icons/notes.png"
        tooltip: "Open the Notes Panel.  Add notes to Media and Sequences."
        buttonPadding: pad
        toggled_on: notesDialog ? notesDialog.visible : false
        onClicked: {
            toggleNotesDialog()
        }
    }

    XsOrderedModuleAttributesModel {
        id: attrs
        attributesGroupNames: "media_tools_buttons"
    }

    ListView {
        id: extra_dudes
        Layout.fillHeight: true
        Layout.minimumWidth: count * 32
        model: attrs
        focus: true
        orientation: ListView.Horizontal
        delegate:
            Item {
                id: parent_item
                width: 32
                height: extra_dudes.height
                property var dynamic_widget
                property var qml_code_: qml_code ? qml_code : null

                onQml_code_Changed: {
                    if (qml_code_) {
                        dynamic_widget = Qt.createQmlObject(qml_code_, parent_item)
                    }
                }
            }
    }

    function toggleTransformDialog()
    {
        if (transformDialog === undefined) {
            transformDialog = do_component_create("qrc:/dialogs/XsTransformDialog.qml")
            transformDialog.show()
        } else if (transformDialog.visible) {
            transformDialog.hide()
        } else {
            transformDialog.show()
        }

    }

    function toggleColourDialog()
    {
        if (colourDialog === undefined) {
            colourDialog = do_component_create("qrc:/dialogs/XsColourCorrectionDialog.qml")
            colourDialog.show()
        } else if (colourDialog.visible) {
            colourDialog.hide()
        } else {
            colourDialog.show()
        }
    }

    function toggleNotesDialog(show_window=undefined)
    {
        if (notesDialog === undefined) {
            notesDialog = do_component_create("qrc:/dialogs/XsNotesDialog.qml")
            notesDialog.show()
            notesDialog.requestActivate()
        } else {
            if(show_window === undefined) {
                show_window = !notesDialog.visible
            }

            if (show_window) {
                notesDialog.show()
                notesDialog.requestActivate()
            } else {
                notesDialog.hide()
            }
        }
    }

    function toggleDrawDialog()
    {
        if (drawingDialog === undefined) {
            drawingDialog = Qt.createQmlObject("import AnnotationsTool 1.0; AnnotationsDialog {}", app_window, "dnyamic")
            drawingDialog.show()
        } else if (drawingDialog.visible) {
            drawingDialog.hide()
        } else {
            drawingDialog.show()
        }
    }

    function do_component_create(component_path) {
        var component = Qt.createComponent(component_path);
        if (component.status == Component.Ready) {
            return component.createObject(app_window, {x: 100, y: 100});
        } else {
            // Error Handling
            console.log("Error loading component:", component.errorString());
            return undefined
        }
    }

}

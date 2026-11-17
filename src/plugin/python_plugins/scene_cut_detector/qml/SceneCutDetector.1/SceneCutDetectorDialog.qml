// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xstudio.qml.models 1.0
import xStudio 1.0

XsWindow {

    id: dialog
    width: 420
    title: "Detect Scene Cuts"
    minimumWidth: 400
    minimumHeight: 260

    flags: Qt.platform.os === "windows" ? Qt.Window : Qt.Dialog

    property var widgetHeight: 24

    XsModuleData {
        id: plugin_attrs
        modelDataName: "scene_cut_detector_attrs"
    }
    XsAttributeValue {
        id: threshold
        attributeTitle: "Cut Threshold"
        model: plugin_attrs
    }
    XsAttributeValue {
        id: min_scene_len
        attributeTitle: "Minimum Scene Length (frames)"
        model: plugin_attrs
    }

    ColumnLayout {

        width: parent.width

        GridLayout {

            Layout.fillWidth: true
            Layout.margins: 20
            columnSpacing: 12
            rowSpacing: 14
            columns: 2

            XsText {
                text: "Detector"
                Layout.alignment: Qt.AlignRight
            }

            XsAttrComboBox {
                id: detectorChoice
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 220
                Layout.preferredHeight: widgetHeight
                attr_title: "Detector"
                attr_model_name: "scene_cut_detector_attrs"
            }

            XsText {
                text: "Cut Threshold"
                Layout.alignment: Qt.AlignRight
            }

            XsTextField {
                id: thresholdField
                Layout.preferredWidth: 220
                Layout.preferredHeight: widgetHeight
                text: threshold.value != undefined ? threshold.value.toString() : ""
                onEditingFinished: threshold.value = parseFloat(text)
                onAccepted: threshold.value = parseFloat(text)
            }

            XsText {
                text: "Min Scene Length (frames)"
                Layout.alignment: Qt.AlignRight
            }

            XsTextField {
                id: minSceneLenField
                Layout.preferredWidth: 220
                Layout.preferredHeight: widgetHeight
                text: min_scene_len.value != undefined ? min_scene_len.value.toString() : ""
                onEditingFinished: min_scene_len.value = parseInt(text)
                onAccepted: min_scene_len.value = parseInt(text)
            }
        }
    }

    RowLayout {

        spacing: 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10

        Item {
            Layout.fillWidth: true
        }

        XsSimpleButton {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Cancel")
            width: XsStyleSheet.primaryButtonStdWidth*2
            onClicked: {
                dialog.hide()
                dialog.destroy()
            }
        }

        XsSimpleButton {
            Layout.alignment: Qt.AlignRight
            Layout.leftMargin: 5
            text: qsTr("Detect Scene Cuts")
            width: XsStyleSheet.primaryButtonStdWidth*3
            onClicked: {

                var return_val = python_callback("run_from_dialog")

                if (Array.isArray(return_val)) {
                    // run_from_dialog returns [True, message]
                    dialogHelpers.messageDialogFunc("Detect Scene Cuts", return_val[1], "Ok")
                    if (return_val[0] == true) {
                        dialog.visible = false
                    }
                } else {
                    // report (likely) error of some sort
                    dialogHelpers.errorDialogFunc("Detect Scene Cuts", return_val)
                }
            }
        }
    }
}

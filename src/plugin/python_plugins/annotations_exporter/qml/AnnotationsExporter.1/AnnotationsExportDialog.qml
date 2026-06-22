// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import xstudio.qml.models 1.0
import xStudio 1.0
import "."

XsWindow {

	id: dialog
    width: 560
    title: "Annotations Export"
    minimumWidth: 540
    minimumHeight: 360

    flags: Qt.platform.os === "windows" ? Qt.Window : Qt.Dialog

    onWidthChanged: {
        console.log("W", width, height)
    }

    property var widgetHeight: 24

    XsModuleData {
        id: plugin_attrs
        modelDataName: "annotation_export_attrs"
    }
    XsAttributeValue {
        id: user_name
        attributeTitle: "User Name"
        model: plugin_attrs
    }
    XsAttributeValue {
        id: output_folder
        attributeTitle: "Output Folder"
        model: plugin_attrs
    }
    XsAttributeValue {
        id: resolution_options
        attributeTitle: "Resolution Options"
        model: plugin_attrs
    }
    XsAttributeValue {
        id: resolution
        attributeTitle: "Resolution"
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

            AnnotationsExportDivider {
                Layout.preferredHeight: widgetHeight
                Layout.fillWidth: true
                Layout.columnSpan: 2
                label: "Annotations Export Options"
            }

            XsText {
                text: "Scope"
                Layout.alignment: Qt.AlignRight
            }

            AnnotationsExportComboBox { 
        
                id: scopeChoice
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 200
                Layout.preferredHeight: widgetHeight
                attr_title: "Scope"
                attr_model_name: "annotation_export_attrs"

            }

            XsText {
                text: "Export Mode"
                Layout.alignment: Qt.AlignRight
            }

            AnnotationsExportComboBox { 
        
                id: exportMode
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 200
                Layout.preferredHeight: widgetHeight
                attr_title: "Export Mode"
                attr_model_name: "annotation_export_attrs"

            }

            XsText {
                text: "File Type"
                Layout.alignment: Qt.AlignRight
            }

            AnnotationsExportComboBox { 
        
                id: exportFileType
                Layout.alignment: Qt.AlignLeft
                Layout.minimumWidth: 200
                Layout.preferredHeight: widgetHeight
                attr_title: "File Type"
                attr_model_name: "annotation_export_attrs"
                enabled: exportMode.currentText != "Greasepencil Package"
                onEnabledChanged: {
                    if (!enabled) override("PNG")
                }

            }

            XsText {
                text: "Output Resolution"
                Layout.alignment: Qt.AlignRight
            }

            XsComboBoxEditable { 
        
                id: resolution_choice
                model: resolution_options.value
                Layout.alignment: Qt.AlignLeft
                Layout.preferredWidth: 324
                Layout.preferredHeight: widgetHeight

                currentIndex: model.indexOf(resolution.value)

                onAccepted: {
                    // note, accepted() is emitted rather often as we have linked
                    // it to onEditingFinished on XsComboBox.
                    const regex = /([0-9]+)x([0-9]+)/;
                    var resmatch = regex.exec(editText)
                    if (resmatch && resmatch.length >= 3) {
                        for (var i = 0; i < resolution_options.value.length; ++i) {
                            var rf = regex.exec(resolution_options.value[i])
                            if (rf && rf.length >= 3 && rf[1] == resmatch[1] && rf[2] == resmatch[2]) {
                                // already have this res
                                return
                            }
                        }
                        var v = resolution_options.value
                        v.push(editText)
                        resolution_options.value = v
                        currentIndex = v.length-1
                    }
                }

                onClearButtonPressed: (clearedIndex)=> {
                    if (clearedIndex != 0 && resolution_options.value[clearedIndex+1][1]) {
                        var v = resolution_options.value
                        v.splice(clearedIndex,1)
                        resolution_options.value = v
                        currentIndex = 0
                    }
                }

            }

            XsText {
                text: exportMode.currentText != "Greasepencil Package" ? "Image(s) Name" : "Package Name"
                Layout.alignment: Qt.AlignRight
                enabled: outputName.enabled
            }

            XsTextField { 

                id: outputName
                Layout.fillWidth: true
                Layout.preferredHeight: widgetHeight
                clip: true
                placeholderText: "Enter a name for the image files"
                text: enabled ? user_name.value : "N/A"
                enabled: exportMode.currentText != "Annotated Images - Use Media Name and Frame"
                onEditingFinished: user_name.value = text
                onAccepted: user_name.value = text
                background: Rectangle{
                    color: outputName.activeFocus? Qt.darker(XsStyleSheet.accentColor, 1.5): outputName.hovered? Qt.lighter(XsStyleSheet.panelBgColor, 2):Qt.lighter(XsStyleSheet.panelBgColor, 1.5)
                    border.width: outputName.hovered || outputName.active? 1:0
                    border.color: XsStyleSheet.accentColor
                    opacity: 0.7
                }


            }

            XsText {
                text: "Output Folder"
                Layout.alignment: Qt.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true
                XsTextField{ 

                    id: outputFolder
                    Layout.fillWidth: true
                    Layout.preferredHeight: widgetHeight
                    clip: true
                    placeholderText: "Enter or browse for output folder"
                    text: output_folder.value != undefined ? output_folder.value : ""
                    onEditingFinished: output_folder.value = text
                    onAccepted: output_folder.value = text
                    background: Rectangle{
                        color: outputFolder.activeFocus? Qt.darker(XsStyleSheet.accentColor, 1.5): outputFolder.hovered? Qt.lighter(XsStyleSheet.panelBgColor, 2):Qt.lighter(XsStyleSheet.panelBgColor, 1.5)
                        border.width: outputFolder.hovered || outputFolder.active? 1:0
                        border.color: XsStyleSheet.accentColor
                        opacity: 0.7
                    }

                }
                
                XsPrimaryButton {
                    id: browse_btn
                    Layout.preferredHeight: widgetHeight
                    text: "Browse ..."
                    onClicked: {
                        dialogHelpers.showFolderDialog(
                            browse_btn.selectFolder,
                            outputFolder.text != "" ? outputFolder.text : file_functions.defaultSessionFolder(),
                            "Export Annotations Destination Folder"
                        )
                    }

                    function selectFolder(folderName) {
                        if (folderName != false) {
                            output_folder.value = folderName
                            outputFolder.text = folderName
                        }
                    }

                }
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
            text: qsTr("Export")
            width: XsStyleSheet.primaryButtonStdWidth*2
            //enabled: outputFile.text != ""
            onClicked: {
    
                var return_val = python_callback(
                    "do_export",
                    scopeChoice.currentText,
                    exportMode.currentText,
                    outputName.text,
                    outputFolder.text,
                    exportFileType.currentText,
                    resolution_choice.currentText
                )
                if (Array.isArray(return_val)) {
                    // do export should return [True, message]
        	        dialogHelpers.messageDialogFunc("Annotations Export", return_val[1], "Ok")
                    if (return_val[0] == true) {
                        dialog.visible = false
                    }
                } else {
                    // report (likely) error of some sort
        	        dialogHelpers.errorDialogFunc("Annotations Export", return_val)
                }

            }
        }
    }

}
// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import xstudio.qml.models 1.0
import xStudio 1.0

XsWindow {

	id: dialog
	width: 450
	height: 300

    property int imageSourceWidth: 1920
    property int imageSourceHeight: 1080

    property int viewerWidth: 0
    property int viewerHeight: 0

    maximumHeight: height
    maximumWidth: width

    minimumHeight: height
    minimumWidth: width
    color: "transparent"

    title: "Viewer Snapshot"

    XsGradientRectangle{ id: bgDiv
        z: -10
        anchors.fill: parent
    }

    function saveScreenshot(path, folder) {

        var result = studio.renderScreenShotToDisk(
            typeof path != "url" ? helpers.QUrlFromQString(""+path) : path,
            0,
            parseInt(widthInput.text),
            parseInt(heightInput.text))

        if (result != "") {
            dialogHelpers.errorDialogFunc("Snapshot Failed", result)
        } else {
            dialogHelpers.messageDialogFunc("Snapshot Saved", "Snapshot image saved to path " + path)
        }
        dialog.close()

    }

    // parent: sessionWidget
    // x: Math.max(0, (sessionWidget.width - width) / 2)
    // y: (sessionWidget.height - height) / 2
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        focus: true
        Keys.onReturnPressed:{
            btnSave.clicked()
        }
        Keys.onEscapePressed: {
            btnCancel.clicked()
        }

        Rectangle {
            color: "transparent"
            Layout.fillHeight: true
        }

        Rectangle {
            color: "transparent"
            height: 160
            Layout.fillWidth: true

            GridLayout {
                id: main_layout
                columns: 2
                rows: 2
                rowSpacing: 12//20
                anchors.centerIn: parent

                XsLabel {
                    text: "Image Size :"
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }

                XsComboBox {
                    id: resolutionChoice
                    model:
                    ListModel {
                        id: imgModel
                        ListElement {
                            text: "Original"
                        }
                        ListElement {
                            text: "Half"
                        }
                        ListElement {
                            text: "xStudio Viewport Size"
                        }
                        ListElement {
                            text: "User Defined"
                        }
                    }
                    width: 200
                    height: 24
                    onActivated: {
                        widthInput.enabled = currentIndex == 3
                        heightInput.enabled = currentIndex == 3
                        widthInput.text = currentIndex == 0 ? imageSourceWidth: currentIndex == 1 ? Math.round(imageSourceWidth / 2) : currentIndex == 2 ? viewerWidth : widthInput.text
                        heightInput.text = currentIndex == 0 ? imageSourceHeight: currentIndex == 1 ? Math.round(imageSourceHeight / 2) : currentIndex == 2 ? viewerHeight : heightInput.text
                    }
                }

                Rectangle{color: "transparent"; width:10; height:10}
                RowLayout {
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                    XsLabel {
                        text: "Width :"
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        opacity: widthInput.enabled ? 1: 0.5
                    }
                    XsTextField {
                        id: widthInput
                        text: imageSourceWidth
                        width: 50
                        height: 24
                        enabled: false
                        selectByMouse: true
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        validator: IntValidator{bottom: 1}

                        onEditingFinished: {
                            focus = false
                        }
                    }

                    XsLabel {
                        text: "Height :"
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        opacity: heightInput.enabled ? 1: 0.5
                    }
                    XsTextField {
                        id: heightInput
                        text: imageSourceHeight
                        width: 50
                        height: 24
                        enabled: false
                        selectByMouse: true
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        validator: IntValidator{bottom: 1}

                        onEditingFinished: {
                            focus = false
                        }
                    }
                }

                XsLabel {
                    text: "OCIO Display:"
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }

                XsAttrComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                    attr_title: "Display"
                    attr_model_name: "snapshot_viewport_toolbar"
                }

                XsLabel {
                    text: "OCIO View:"
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }

                XsAttrComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                    attr_title: "View"
                    attr_model_name: "snapshot_viewport_toolbar"
                }

            }
        }

        Rectangle {
            color: "transparent"
            Layout.fillHeight: true
        }

        RowLayout {

            Layout.alignment: Qt.AlignRight
            Layout.fillHeight: false
            spacing: 5

            XsSimpleButton {
                id: btnCancel
                text: qsTr("Cancel")
                width: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: dialog.visible = false
            }
            XsSimpleButton {
                id: btnToClipboard
                text: qsTr("Save to Clipboard")
                width: XsStyleSheet.primaryButtonStdWidth*4
                onClicked: {
                    var err_msg = studio.renderScreenShotToClipboard(
                        parseInt(widthInput.text),
                        parseInt(heightInput.text))
                    if (err_msg != "") {
                        dialogHelpers.errorDialogFunc("Snapshot Failed", err_msg)
                    } else {
                        dialogHelpers.messageDialogFunc("Screenshot", "Screenshot copied to clipboard.", "OK")
                    }
                    dialog.close()
                }
            }
            XsSimpleButton {
                id: btnSave
                text: qsTr("Save")
                width: XsStyleSheet.primaryButtonStdWidth*2
                onClicked: {
                    dialog.close()
                    dialogHelpers.showFileDialog(
                        dialog.saveScreenshot,
                        file_functions.defaultSessionFolder(),
                        "Save Screenshot",
                        "jpg",
                        [ "JPEG files (*.jpg)", "PNG files (*.png)", "TIF files (*.tif *.tiff)", "EXR files (*.exr)" ],
                        false,
                        false
                        )                    
                }
            }

        }
    }
}
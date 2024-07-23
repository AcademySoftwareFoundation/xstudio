// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.0

import xStudio 1.1

XsDialog {

    property var playhead: sessionWidget.viewport.playhead
    property var playerWidget: sessionWidget.playerWidget

    property var resolution: app_window.mediaImageSource.values.resolutionRole ? app_window.mediaImageSource.values.resolutionRole : ""
    property var pixelAspect:  app_window.mediaImageSource.values.aspectRole ? app_window.mediaImageSource.values.aspectRole : 1.0

    property var viewerWidth: playerWidget.viewport.width
    property var viewerHeight: playerWidget.viewport.height

    property int imageSourceWidth: 1920
    property int imageSourceHeight: 1080

    centerOnOpen: true

    onImageSourceWidthChanged: {
        if (resolutionChoice.currentIndex == 0) widthInput.text = imageSourceWidth
        else if (resolutionChoice.currentIndex == 1) widthInput.text = imageSourceWidth/2
    }

    onImageSourceHeightChanged: {
        if (resolutionChoice.currentIndex == 0) heightInput.text = imageSourceHeight
        else if (resolutionChoice.currentIndex == 1) heightInput.text = imageSourceHeight/2
    }

    onViewerWidthChanged: {
        if (resolutionChoice.currentIndex == 2) widthInput.text = viewerWidth
    }

    onViewerHeightChanged: {
        if (resolutionChoice.currentIndex == 2) heightInput.text = viewerHeight
    }

    onResolutionChanged: {
        var r = resolution.split("x")
        if (r.length == 2) {
            imageSourceWidth = parseInt(r[0])
            imageSourceHeight = parseInt(r[1])/pixelAspect
        }
    }

    onPixelAspectChanged: {
        var r = resolution.split("x")
        if (r.length == 2) {
            imageSourceWidth = parseInt(r[0])
            imageSourceHeight = parseInt(r[1])/pixelAspect
        }
    }

    id: dlg
    width: 390
    height: 270
    maximumHeight: height
    maximumWidth: width

    minimumHeight: height
    minimumWidth: width

    title: "Viewer Snapshot"
    // parent: sessionWidget
    // x: Math.max(0, (sessionWidget.width - width) / 2)
    // y: (sessionWidget.height - height) / 2
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Rectangle {
            color: "transparent"
            Layout.fillHeight: true
        }

        FileDialog {
            id: filedialog
            title: qsTr("Name / select a file to save")
            selectMultiple: false
            selectFolder: false
            selectExisting: false
            nameFilters: [ "JPEG files (*.jpg)", "PNG files (*.png)", "TIF files (*.tif *.tiff)", "EXR files (*.exr)" ]
            property var suffixes: ["jpg", "png", "tif", "exr"]
            property var formatIdx: formatBox.currentIndex
            defaultSuffix: suffixes[formatIdx]
            selectedNameFilter: nameFilters[formatIdx]
            onAccepted: {
                var fixedfileUrl = fileUrl.toString().includes("." + suffixes[formatIdx]) ? fileUrl : (fileUrl + "." + suffixes[formatIdx])
                playerWidget.viewport.renderImageToFile(
                    fixedfileUrl,
                    formatIdx,
                    slider.value,
                    widthInput.text,
                    heightInput.text,
                    bakeColorBox.checked)
                dlg.close()
            }
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
                    id: formatLbl
                    text: "File Format :"
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                XsComboBox {
                    id: formatBox
                    model:
                    ListModel {
                        id: formatModel
                        ListElement {
                            text: "JPG"
                        }
                        ListElement {
                            text: "PNG"
                        }
                        ListElement {
                            text: "TIF"
                        }
                        ListElement {
                            text: "EXR"
                        }
                    }
                    width: 200
                    height: 24
                    onCurrentIndexChanged: {
                        filedialog.selectedNameFilter = filedialog.nameFilters[currentIndex]
                    }
                }

                XsLabel {
                    text: "Compression :"
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    opacity: slider.enabled ? 1: 0.5
                }
                XsHSlider {
                    id: slider
                    factor: 10
                    width: 200
                    height: 24
                    implicitWidth: 200
                    implicitHeight: 24
                    Layout.fillWidth: false
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    enabled: formatBox.currentIndex == 0 || formatBox.currentIndex == 1
                    opacity: enabled ? 1: 0.5
                    value: 9
                }

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
                            text: "User defined"
                        }
                    }
                    width: 200
                    height: 24
                    onActivated: {
                        widthInput.enabled = currentIndex == 3
                        heightInput.enabled = currentIndex == 3
                        widthInput.text = currentIndex == 0 ? imageSourceWidth: currentIndex == 1 ? Math.round(imageSourceWidth / 2) : currentIndex == 2 ? viewerWidth : widthInput.text
                        heightInput.text = currentIndex == 0 ? imageSourceHeight: currentIndex == 1 ? Math.round(imageSourceHeight / 2) : currentIndex == 2 ? viewerHeight : viewerHeight.text
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
                        // anchors.fill: parent
                        id: widthInput
                        text: imageSourceWidth
                        // width: font.pixelSize*2
                        width: 50
                        height: 24
                        enabled: false
                        color: enabled ? XsStyle.controlColor: XsStyle.controlColorDisabled
                        selectByMouse: true
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        validator: IntValidator{bottom: 1}

                        font {
                            family: XsStyle.fontFamily
                        }

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
                        // anchors.fill: parent
                        id: heightInput
                        text: imageSourceHeight
                        // width: font.pixelSize*2
                        width: 50
                        height: 24
                        enabled: false
                        color: enabled ? XsStyle.controlColor: XsStyle.controlColorDisabled
                        selectByMouse: true
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        validator: IntValidator{bottom: 1}

                        font {
                            family: XsStyle.fontFamily
                        }

                        onEditingFinished: {
                            focus = false
                        }
                    }
                }

                XsLabel {
                    text: "Bake Colour :"
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                }
                XsCheckbox {
                    id: bakeColorBox
                    checked: true
                    leftPadding: 0
                    // onTriggered: {
                    //     checked = !checked
                    // }
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
            spacing: 10

            XsButton {
                id: btnCancel
                text: qsTr("Close")
                width: 125
                implicitHeight: 24

                MouseArea {
                    id: maCancel
                    hoverEnabled: true
                    anchors.fill: btnCancel
                    cursorShape: Qt.PointingHandCursor
                    onClicked: accept()
                }
            }

            XsButton {
                id: btnSave
                text: qsTr("Save Snapshot...")
                width: 125
                implicitHeight: 24

                MouseArea {
                    id: maSave
                    hoverEnabled: true
                    anchors.fill: btnSave
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        filedialog.open()
                    }
                }

            }
        }
    }
}
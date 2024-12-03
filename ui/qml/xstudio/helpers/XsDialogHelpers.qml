// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Dialogs 1.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

Item {

    id: root

    Loader {
        id: loader
    }


    function hideLastDialog() {
        loader.item.visible = false
    }

    function showDialog(callback) {
        loader.item.x = root.width/2 - loader.item.width/2
        loader.item.y = root.height/2 - loader.item.height/2
        if (callback) loader.item.response.connect(callback)
        loader.item.visible = true
    }

    property var dialogVisible: loader.item ? loader.item.visible != undefined ? loader.item.visible : false : false

    onDialogVisibleChanged: {
        if (!visible) appWindow.grabFocus()
    }

    /**************************************************************

    FileDialog

    ****************************************************************/

    Component {
        id: fileDialog

        FileDialog {
            property var chaser

            onAccepted: {
                if (selectMultiple)
                    result(fileUrls, folder, chaser)
                else
                    result(fileUrl, folder, chaser)
                // unload (important!)
                loader.sourceComponent = undefined
            }
            onRejected: {
                result(false, undefined, chaser)
                // unload (important!)
                loader.sourceComponent = undefined
            }
            signal result(variant _path, variant _folder, variant _chaser);
        }
    }

    function showFileDialog(
        resultCallback,
        folder,
        title,
        defaultSuffix,
        nameFilters,
        selectExisting,
        selectMultiple,
        chaser) {

        loader.sourceComponent = fileDialog
        loader.item.result.connect(resultCallback)
        if (folder) loader.item.folder = folder
        loader.item.title = title
        loader.item.defaultSuffix = defaultSuffix ? defaultSuffix : ""
        loader.item.nameFilters = nameFilters
        loader.item.selectExisting = selectExisting
        loader.item.selectMultiple = selectMultiple
        loader.item.chaser = chaser
        loader.item.selectFolder = false
        loader.item.open()

    }

    function showFileDialogFolderMode(
        resultCallback,
        folder,
        title) {

        loader.sourceComponent = fileDialog
        loader.item.result.connect(resultCallback)
        loader.item.folder = folder
        loader.item.title = title
        loader.item.selectFolder = true
        loader.item.selectExisting = true
        loader.item.selectMultiple = false
        loader.item.open()

    }

    /**************************************************************

    xSTUDIO Simple Error Message Box

    ****************************************************************/

    Component {
        id: errorDialog
        XsWindow {

            id: errorDialog
            title: "Error"
            property string body: ""
            width: 400
            height: 200


            ColumnLayout {

                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                focus: true
                Keys.onEscapePressed: errorDialog.visible = false

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 20

                    Image {
                        id: thumbnailImgDiv
                        width: 40
                        height: 40
                        source: "qrc:/icons/error.svg"
                    }
                    XsText {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: body
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        font.weight: Font.Bold
                        font.pixelSize: XsStyleSheet.fontSize*1.2
                    }
                }
                XsSimpleButton {
                    text: "Close"
                    width: XsStyleSheet.primaryButtonStdWidth*2
                    Layout.alignment: Qt.AlignRight
                    onClicked: {
                        errorDialog.visible = false
                    }
                }
            }
            onVisibleChanged: {
                if (!visible) {
                    loader.sourceComponent = undefined
                }
            }
        }
    }

    function errorDialogFunc(error_title, error_body) {
        loader.sourceComponent = errorDialog
        loader.item.title = error_title
        loader.item.body = error_body
        showDialog(undefined)
    }

    /**************************************************************

    xSTUDIO Simple Mutli-Choice Dialog Box

    ****************************************************************/

    Component {

        id: multiChoice

        XsWindow {
            id: popup

            title: ""
            property string body: ""
            property var choices: []
            width: 400
            height: 200
            signal response(variant p, variant chaser)
            property var chaser

            ColumnLayout {

                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                focus: true
                Keys.onReturnPressed: {
                    popup.response(popup.choices[popup.choices.length-1], popup.chaser)
                    popup.visible = false
                }
                Keys.onEscapePressed: {
                    popup.response(popup.choices[0], popup.chaser)
                    popup.visible = false
                }

                XsText {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: body
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font.weight: Font.Bold
                    font.pixelSize: XsStyleSheet.fontSize*1.2
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    Repeater {
                        model: popup.choices

                        XsSimpleButton {
                            text: popup.choices[index]
                            Layout.alignment: Qt.AlignRight
                            onClicked: {
                                popup.response(popup.choices[index], popup.chaser)
                                popup.visible = false
                            }
                        }
                    }
                }
            }
            onVisibleChanged: {
                if (!visible) {
                    loader.sourceComponent = undefined
                }
            }
        }
    }

    function multiChoiceDialog(
        callback,
        title,
        body,
        choices,
        chaserFunc)
    {
        loader.sourceComponent = multiChoice
        loader.item.title = title
        loader.item.body = body
        loader.item.choices = choices
        loader.item.chaser = chaserFunc
        showDialog(callback)
    }

    /**************************************************************

    xSTUDIO Simple Text Entry Dialog Box

    ****************************************************************/

    Component {

        id: textInput

        XsStringRequestDialog {
            onVisibleChanged: {
                if (!visible) {
                    loader.sourceComponent = undefined
                }
            }
        }
    }

    function textInputDialog(
        callback,
        title,
        body,
        initialText,
        choices)
    {
        loader.sourceComponent = textInput
        loader.item.title = title
        loader.item.body = body
        loader.item.initialText = initialText
        loader.item.choices = choices
        loader.item.area = false
        showDialog(callback)
    }

    function numberInputDialog(
        callback,
        title,
        body,
        initialText,
        choices)
    {
        loader.sourceComponent = textInput
        loader.item.title = title
        loader.item.body = body
        // loader.item.inputMethodHints = Qt.ImhFormattedNumbersOnly
        loader.item.initialText = initialText
        loader.item.choices = choices
        loader.item.area = false
        showDialog(callback)
    }


    function textAreaInputDialog(
        callback,
        title,
        body,
        initialText,
        choices)
    {
        loader.sourceComponent = textInput
        loader.item.title = title
        loader.item.body = body
        loader.item.initialText = initialText
        loader.item.choices = choices
        loader.item.area = true
        showDialog(callback)
    }

}
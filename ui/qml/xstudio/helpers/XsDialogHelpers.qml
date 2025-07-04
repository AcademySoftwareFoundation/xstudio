// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

Item {

    id: root

    Loader {
        id: loader
    }

    Loader {
        id: file_dlg_loader
    }

    function hideLastDialog() {
        if (loader.item) {
            loader.item.visible = false
        }
    }

    function showDialog(callback) {
        loader.item.x = root.width/2 - loader.item.width/2
        loader.item.y = root.height/2 - loader.item.height/2
        if (callback) loader.item.response.connect(callback)
        loader.item.visible = true
        loader.item.show()
        loader.item.requestActivate()
    }

    property var dialogVisible: loader.item ? loader.item.visible != undefined ? loader.item.visible : false : false

    onDialogVisibleChanged: {
        if (!visible)
            appWindow.grabFocus()
    }

    property var fileDialogVisible: file_dlg_loader.item ? file_dlg_loader.item.visible != undefined ? file_dlg_loader.item.visible : false : false

    onFileDialogVisibleChanged: {
        if (!visible) appWindow.grabFocus()
    }

    /**************************************************************

    FileDialog

    ****************************************************************/

    Component {
        id: fileDialog

        FileDialog {
            property var chaser
            signal resultSignal(variant _path, variant _folder, variant _chaser);

            onAccepted: {
                if (fileMode==FileDialog.OpenFiles)
                    resultSignal(selectedFiles, currentFolder, chaser)
                else
                    resultSignal(selectedFile, currentFolder, chaser)
                // unload (important!)
                // if (loader.item == fileDialog)
                file_dlg_loader.sourceComponent = undefined
            }
            onRejected: {
                resultSignal(false, undefined, chaser)
                // unload (important!)
                file_dlg_loader.sourceComponent = undefined
            }
        }
    }

    Component {
        id: folderDialog

        FolderDialog {
            property var chaser
            signal resultSignal(variant _path, variant _chaser);

            onAccepted: {
                resultSignal(selectedFolder, chaser)
                // unload (important!)
                // if (loader.item == folderDialog)
                file_dlg_loader.sourceComponent = undefined
            }
            onRejected: {
                resultSignal(undefined, chaser)
                // unload (important!)
                file_dlg_loader.sourceComponent = undefined
            }
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

        file_dlg_loader.sourceComponent = fileDialog
        file_dlg_loader.item.resultSignal.connect(resultCallback)
        file_dlg_loader.item.currentFolder = folder
        file_dlg_loader.item.title = title
        file_dlg_loader.item.defaultSuffix = defaultSuffix ? defaultSuffix : ""
        file_dlg_loader.item.nameFilters = nameFilters
        file_dlg_loader.item.fileMode = selectExisting ? (selectMultiple ? FileDialog.OpenFiles : FileDialog.OpenFile) : FileDialog.SaveFile
        file_dlg_loader.item.chaser = chaser
        file_dlg_loader.item.open()
    }

    function showFolderDialog(
        resultCallback,
        folder,
        title) {

        file_dlg_loader.sourceComponent = folderDialog
        file_dlg_loader.item.resultSignal.connect(resultCallback)
        file_dlg_loader.item.currentFolder = folder
        file_dlg_loader.item.title = title
        file_dlg_loader.item.open()
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
            property bool is_error: true
            width: 400
            height: 200
            property string buttonText: "Close"

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
                        visible: is_error
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
                    text: buttonText
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
        hideLastDialog()
        loader.sourceComponent = errorDialog
        loader.item.title = error_title
        loader.item.body = error_body
        loader.item.is_error = true
        showDialog(undefined)
    }

    function messageDialogFunc(message_title, message_body, button_text) {
        hideLastDialog()
        loader.sourceComponent = errorDialog
        loader.item.title = message_title
        loader.item.body = message_body
        loader.item.is_error = false
        if (button_text != undefined) loader.item.buttonText = button_text
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
                    popup.visible = false
                    popup.response(popup.choices[popup.choices.length-1], popup.chaser)
                }
                Keys.onEscapePressed: {
                    popup.visible = false
                    popup.response(popup.choices[0], popup.chaser)
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

    Component {
        id: sequenceInput

        XsSequenceRequestDialog {
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

    function sequenceInputDialog(
        callback,
        title,
        body,
        initialText,
        choices)
    {
        loader.sourceComponent = sequenceInput
        loader.item.title = title
        loader.item.body = body
        loader.item.initialText = initialText
        loader.item.choices = choices
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


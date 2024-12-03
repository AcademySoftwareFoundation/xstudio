// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudio 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.embedded_python 1.0

Item{

    id: panel
    anchors.fill: parent

    XsGradientRectangle{
        anchors.fill: parent
    }

    property var session_uuid: embeddedPython.sessionId

    XsPreference {
        id: pythonHistory
        path: "/ui/qml/python_history"
    }

    onVisibleChanged: {
        if(visible) {
            connect()
        }
    }

    Component.onCompleted: {
        connect()
    }

    function connect() {
        if(output.text.length == 0) {
            output.text = embeddedPython.text
            output.cursorPosition = output.length
        }
        output.forceActiveFocus()
    }

    Connections {
        target: embeddedPython
        function onStdoutEvent(str) {
            output.text = output.text + str
            output.cursorPosition = output.length
            // output.text = output.text + str.replace(/(?:\r\n|\r|\n)/g, '<br>')
        }
        function onStderrEvent(str) {
            output.text = output.text + str
            output.cursorPosition = output.length
            // output.text = output.text + "<font color=\"#999999\">"+ str.replace(/(?:\r\n|\r|\n)/g, '<br>')+"</font>"
            // output.append("<font color=\"#999999\">"+str+"</font>")
        }
    }

    function saveSelectionAs(path, folder, chaserFunc) {
        if (path == false) return; // save was cancelled

        var spath = "" + path
        if (!spath.endsWith(".py")) {
            path = path + ".py"
        }
        embeddedPython.saveSnippet(path, output.selectedText+"\n");
    }

    XsPopupMenu {
        id: rightClickMenu
        visible: false
        menu_model_name: "python_popup"+rightClickMenu

        XsMenuModelItem {
            text: "Copy"
            menuItemPosition: 1
            menuPath: ""
            menuModelName: rightClickMenu.menu_model_name
            onActivated: output.copy()
        }

        XsMenuModelItem {
            text: "Paste"
            menuItemPosition: 2
            menuPath: ""
            menuModelName: rightClickMenu.menu_model_name
            onActivated: output.pasteAction()
        }
        XsMenuModelItem {
            menuItemType: "divider"
            menuPath: ""
            menuItemPosition: 3
            menuModelName: rightClickMenu.menu_model_name
        }

        XsMenuModelItem {
            text: "Save Selection As Snippet"
            menuItemPosition: 4
            menuPath: ""
            menuModelName: rightClickMenu.menu_model_name
            onActivated: {
                dialogHelpers.showFileDialog(
                    saveSelectionAs,
                    file_functions.defaultSessionFolder(),
                    "Save Selected Text As Snippet",
                    "py",
                    ["python (*.py)"],
                    false,
                    false
                    )
            }
        }
    }

    MouseArea{ id: mArea
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        z:3

        onPressed: (mouse)=>{
            // required for doubleclick to work
            if (mouse.button == Qt.RightButton){
                if(rightClickMenu.visible) rightClickMenu.visible = false
                else{
                    let ppos = mapToItem(rightClickMenu.parent, mouseX, mouseY)
                    rightClickMenu.x = ppos.x
                    rightClickMenu.y = ppos.y
                    rightClickMenu.visible = true
                }
            }
        }
    }

    Rectangle{
        anchors.fill: view_output
        color: XsStyleSheet.panelBgColor
    }

    ScrollView { id: view_output
        z:2
        anchors.fill: parent
        anchors.margins: XsStyleSheet.panelPadding

        TextArea {
            id: output
            // anchors.fill: parent
            //readOnly: true
            text: ""
            width: parent.width
            height: Math.max(implicitHeight, parent.height)
            // textFormat: TextArea.RichText
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            color: palette.text//"#909090"
            selectionColor: XsStyleSheet.hintColor // XsStyle.highlightColor
            wrapMode: TextEdit.WordWrap
            selectByMouse: true
            property int history_index: 0
            // add history, ctrl-d. ctrl-c
            focus: true

            Keys.onReturnPressed: {
                // get last line.
                // only capute after first 4 chars
                if(event.modifiers == Qt.ControlModifier) {
                    var input = selectedText
                    text += selectedText + "\n"
                    embeddedPython.sendInput(input)
                    if(input.length && pythonHistory.value[pythonHistory.value.length-1] != input) {
                        let tmp = pythonHistory.value
                        tmp.push(selectedText)
                        pythonHistory.value = tmp
                    }
                    history_index = pythonHistory.value.length
                } else {
                    var input = text.substring(text.lastIndexOf("\n")+5)
                    text += "\n"
                    embeddedPython.sendInput(input)
                    if(input.length && pythonHistory.value[pythonHistory.value.length-1] != input) {
                        let tmp = pythonHistory.value
                        tmp.push(input)
                        pythonHistory.value = tmp
                    }
                    history_index = pythonHistory.value.length
                }
            }

            Keys.onUpPressed: {
                if(pythonHistory.value.length) {
                    if(history_index != 0) {
                        history_index--
                    }
                    var input = pythonHistory.value[history_index]
                    text = text.substring(0, text.lastIndexOf("\n")+5) + input
                    cursorPosition = text.length
                }
            }

            Keys.onDownPressed: {
                if(pythonHistory.value.length) {
                    var input = ""

                    if(history_index >= pythonHistory.value.length-1) {
                        //blank..
                    } else {
                        history_index++
                        input = pythonHistory.value[history_index]
                    }
                    text = text.substring(0, text.lastIndexOf("\n")+5) + input
                    cursorPosition = text.length
                }
            }

            function pasteAction() {
                if(cursorPosition < text.lastIndexOf("\n")+5)
                        cursorPosition = text.length
                paste()
            }

            Keys.onPressed: {
                if ((event.key == Qt.Key_C) && (event.modifiers & Qt.ControlModifier)) {
                    embeddedPython.sendInterrupt()
                    event.accepted = true;
                } else if(event.key == Qt.Key_Copy) {
                    event.accepted = true;
                    output.copy()
                } else if(event.key == Qt.Key_Paste) {
                    event.accepted = true;
                    pasteAction()
                } else if(event.key == Qt.Key_Home) {
                    cursorPosition = text.lastIndexOf("\n")+5
                    event.accepted = true;
                } else if(event.key == Qt.Key_Backspace || event.key == Qt.Key_Left) {
                    if(cursorPosition <= text.lastIndexOf("\n")+5){
                        event.accepted = true;
                    }
                } else if(cursorPosition < text.lastIndexOf("\n")+5) {
                    cursorPosition = text.length
                    event.accepted = false;
                }
            }
        }
    }
}
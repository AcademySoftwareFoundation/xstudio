// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xstudio.qml.models 1.0
import xStudio 1.0

Item {
    id: delegateRoot

    property string searchFilter: ""

    width: ListView.view ? ListView.view.width : 0
    height: matchesFilter ? XsStyleSheet.widgetStdHeight + 4 : 0
    visible: matchesFilter
    clip: true

    property bool matchesFilter: {
        if (searchFilter === "")
            return true
        return hotkeyName.toLowerCase().indexOf(searchFilter) >= 0 ||
               hotkeySequence.toLowerCase().indexOf(searchFilter) >= 0 ||
               hotkeyDescription.toLowerCase().indexOf(searchFilter) >= 0
    }

    property bool isCapturing: false
    property string capturedSequence: ""
    property string conflictText: ""

    // Divider line
    Rectangle {
        width: parent.width
        anchors.bottom: parent.bottom
        height: 1
        color: XsStyleSheet.widgetBgNormalColor
    }

    // Hover highlight
    Rectangle {
        anchors.fill: parent
        color: mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Hotkey name
        XsLabel {
            id: nameLabel
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
            Layout.preferredWidth: row_widths[0]
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            text: hotkeyName
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight

            TextMetrics {
                font: nameLabel.font
                text: nameLabel.text
                onWidthChanged: setRowMinWidth(width, 0)
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: XsStyleSheet.widgetBgNormalColor
        }

        // Shortcut display / capture area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4

            RowLayout {
                anchors.fill: parent
                anchors.margins: 2
                spacing: 4

                // Shortcut button - click to capture
                Rectangle {
                    id: shortcutButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 2
                    color: delegateRoot.isCapturing ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.3) :
                           delegateRoot.conflictText !== "" ? Qt.rgba(1, 0.6, 0, 0.12) :
                           shortcutMouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                    border.color: delegateRoot.isCapturing ? palette.highlight :
                                  delegateRoot.conflictText !== "" ? "#E6A020" :
                                  shortcutMouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.2) : "transparent"
                    border.width: delegateRoot.isCapturing ? 2 : delegateRoot.conflictText !== "" ? 1 : 1
                    radius: 3

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        XsText {
                            Layout.alignment: Qt.AlignVCenter
                            text: delegateRoot.isCapturing ? "Press key..." : hotkeySequence
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            font.italic: delegateRoot.isCapturing
                            color: delegateRoot.isCapturing ? palette.highlight :
                                   delegateRoot.conflictText !== "" ? "#E6A020" : palette.text
                        }

                        XsText {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            visible: delegateRoot.conflictText !== "" && !delegateRoot.isCapturing
                            text: delegateRoot.conflictText !== "" ? "Conflicts with: " + delegateRoot.conflictText : ""
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: XsStyleSheet.fontSize - 1
                            font.italic: true
                            color: "#E6A020"
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: shortcutMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (!delegateRoot.isCapturing) {
                                delegateRoot.isCapturing = true
                                captureInput.forceActiveFocus()
                            }
                        }
                    }

                    TextMetrics {
                        id: seqMetrics
                        font.pixelSize: XsStyleSheet.fontSize
                        text: hotkeySequence
                        onWidthChanged: setRowMinWidth(width + 20, 1)
                    }
                }

                // Reset button (visible when modified)
                Rectangle {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    Layout.alignment: Qt.AlignVCenter
                    radius: 2
                    color: resetMa.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                    visible: true

                    XsText {
                        anchors.centerIn: parent
                        text: "\u21BA"  // ↺ reset symbol
                        font.pixelSize: 14
                        color: resetMa.containsMouse ? palette.highlight : XsStyleSheet.hintColor
                    }

                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            hotkeysModel.resetHotkey(index)
                            delegateRoot.conflictText = ""
                        }
                    }

                    XsToolTip {
                        text: "Reset to default"
                        visible: resetMa.containsMouse
                    }
                }
            }
        }

    }

    // Hidden text input for key capture
    TextInput {
        id: captureInput
        width: 0
        height: 0
        visible: false
        focus: delegateRoot.isCapturing

        Keys.onPressed: function(event) {
            if (!delegateRoot.isCapturing)
                return

            // Ignore lone modifier keys
            if (event.key === Qt.Key_Shift || event.key === Qt.Key_Control ||
                event.key === Qt.Key_Alt || event.key === Qt.Key_Meta) {
                event.accepted = true
                return
            }

            // Escape cancels capture
            if (event.key === Qt.Key_Escape) {
                delegateRoot.isCapturing = false
                event.accepted = true
                return
            }

            // Build sequence string
            var parts = []
            if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
            if (event.modifiers & Qt.AltModifier) parts.push("Alt")
            if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
            if (event.modifiers & Qt.MetaModifier) parts.push("Meta")

            // Get key name
            var keyName = ""
            var keyText = event.text

            // Map common keys
            var keyMap = {}
            keyMap[Qt.Key_Space] = "Space Bar"
            keyMap[Qt.Key_Return] = "Return"
            keyMap[Qt.Key_Enter] = "Enter"
            keyMap[Qt.Key_Tab] = "Tab"
            keyMap[Qt.Key_Backspace] = "Backspace"
            keyMap[Qt.Key_Delete] = "Delete"
            keyMap[Qt.Key_Insert] = "Insert"
            keyMap[Qt.Key_Home] = "Home"
            keyMap[Qt.Key_End] = "End"
            keyMap[Qt.Key_PageUp] = "PageUp"
            keyMap[Qt.Key_PageDown] = "PageDown"
            keyMap[Qt.Key_Left] = "Left"
            keyMap[Qt.Key_Right] = "Right"
            keyMap[Qt.Key_Up] = "Up"
            keyMap[Qt.Key_Down] = "Down"
            keyMap[Qt.Key_Escape] = "Escape"
            keyMap[Qt.Key_F1] = "F1"
            keyMap[Qt.Key_F2] = "F2"
            keyMap[Qt.Key_F3] = "F3"
            keyMap[Qt.Key_F4] = "F4"
            keyMap[Qt.Key_F5] = "F5"
            keyMap[Qt.Key_F6] = "F6"
            keyMap[Qt.Key_F7] = "F7"
            keyMap[Qt.Key_F8] = "F8"
            keyMap[Qt.Key_F9] = "F9"
            keyMap[Qt.Key_F10] = "F10"
            keyMap[Qt.Key_F11] = "F11"
            keyMap[Qt.Key_F12] = "F12"

            if (event.key in keyMap) {
                keyName = keyMap[event.key]
            } else if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
                keyName = String.fromCharCode(event.key)
            } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                keyName = String.fromCharCode(event.key)
            } else if (keyText && keyText.length === 1) {
                keyName = keyText.toUpperCase()
            } else {
                // Unknown key - cancel
                delegateRoot.isCapturing = false
                event.accepted = true
                return
            }

            parts.push(keyName)
            var newSequence = parts.join("+")

            // Check for conflicts before rebinding
            var conflict = hotkeysModel.checkConflict(index, newSequence)
            delegateRoot.conflictText = conflict

            // Apply the rebind (still allowed even with conflict)
            hotkeysModel.rebindHotkey(index, newSequence)
            delegateRoot.isCapturing = false
            event.accepted = true
        }

        onActiveFocusChanged: {
            if (!activeFocus && delegateRoot.isCapturing) {
                delegateRoot.isCapturing = false
            }
        }
    }
}

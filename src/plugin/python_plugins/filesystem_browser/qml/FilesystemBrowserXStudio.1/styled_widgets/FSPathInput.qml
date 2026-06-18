// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0
import xstudio.qml.models 1.0

import "."
import ".."

RowLayout {

    spacing: 1
    
    XsPrimaryButton {
        
        Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
        Layout.preferredWidth: 32
        onClicked: {sendCommand({"action": "go_to_parent_folder"})}
        ToolTip.text: "Refresh directory contents (SCAN)"
        imgSrc: "qrc:/icons/arrow_upward.svg"
    }

    XsPrimaryButton {
        
        Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
        Layout.preferredWidth: 32
        enabled: !deepScan
        onClicked: sendCommand({"action": "force_scan"})
        imgSrc: "qrc:/icons/scanner.svg"
        XsToolTip {
            text: "Deep scan all subfolders for more media"
            visible: parent.hovered
        }
    }

    XsText {
        text: "Path"
        verticalAlignment: Text.AlignVCenter
        Layout.leftMargin: 4
    }

    XsAttributeValue {
        id: completions_attr
        attributeTitle: "completions_attr"
        model: pluginData
        
        onValueChanged: {
             var rawVal = value
             if (rawVal) {
                 try {
                     completionList = JSON.parse(rawVal)
                     if (completionList.length > 0 && pathField.activeFocus) {
                         completionPopup.open()
                     } else {
                         completionPopup.close()
                     }
                 } catch(e) {
                     completionList = []
                 }
             }
        }
    }

    XsTextField {

        id: pathField
        Layout.fillWidth: true
        Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
        Layout.leftMargin: 4
        text: current_path_attr.value || "/"
        onTextChanged: {
            // This ensures that even if user is typing, a programmatic update
            // to current_path_attr.value (e.g. from SCAN button) can force a refresh if needed.
            // However, standard QML binding `text: ...` usually breaks if user edits.
            // We'll add a listener to the attribute to force it back if it changes externally.
        }
        
        Connections {
            target: current_path_attr
            function onValueChanged() {
                pathField.text = current_path_attr.value || "/"
            }
        }
        
        focus: true
        selectByMouse: true
        
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        background: Rectangle{
            color: pathField.activeFocus? Qt.darker(XsStyleSheet.accentColor, 1.5): pathField.hovered? Qt.lighter(XsStyleSheet.panelBgColor, 2):Qt.lighter(XsStyleSheet.panelBgColor, 1.5)
            border.width: pathField.hovered || pathField.active? 1:0
            border.color: XsStyleSheet.accentColor
            opacity: enabled? 0.7 : 0.3
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onTapped: (eventPoint, button) => {
                if (button == Qt.RightButton) {
                    pathContextMenu.showMenu(parent, eventPoint.position.x, eventPoint.position.y)
                } else if (tapCount == 3) {
                    // Make sure the path field retains focus when triple clicking.
                    pathField.forceActiveFocus()
                }
            }
        }

        XsPopupMenu {
            id: pathContextMenu
            menu_model_name: "mnu"+pathContextMenu
            
            background: Rectangle {
                implicitWidth: 120
                implicitHeight: 75 // 3 items
                color: XsStyleSheet.panelBgColor
                border.color: XsStyleSheet.menuBorderColor
                radius: 3
            }
            
            XsMenuModelItem {
                menuModelName: pathContextMenu.menu_model_name
                menuPath: ""
                text: "Copy"
                onActivated: pathField.copy()
            }
            XsMenuModelItem {
                menuModelName: pathContextMenu.menu_model_name
                menuPath: ""
                text: "Paste"
                onActivated: pathField.paste()
            }
            XsMenuModelItem {
                menuModelName: pathContextMenu.menu_model_name
                menuPath: ""
                text: "Clear"
                onActivated: pathField.clear()
            }
        }
        
        onAccepted: {
            sendCommand({"action": "change_path", "path": text})
        }
        
        onTextEdited: {
            sendCommand({"action": "complete_path", "path": text})
        }
        
        // Keys handling for completion (omitted for brevity, assume similar to before)
        // Keys handling for completion
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: (event) => {
            // TAB
            if (event.key === Qt.Key_Tab) {
                event.accepted = true;
                
                var hasCompleted = false;
                
                // 1. Try Single Match or Common Prefix Completion first
                if (completionList.length > 0) {
                    var prefix = getCommonPrefix(completionList);
                    // If we have a single match, prefix is the match itself.
                    
                    // If the calculated prefix is longer than what we currently have, utilize it.
                    // This covers both "Single Match" and "Partial Shell Completion"
                    if (prefix.length > text.length) {
                        text = prefix;
                        hasCompleted = true;
                        sendCommand({"action": "complete_path", "path": text});
                    }
                }
                
                // 2. If we didn't extend the text (ambiguous state), then Cycle through the list
                if (!hasCompleted && completionPopup.opened && completionListView.count > 0) {
                    if (event.modifiers & Qt.ShiftModifier) {
                        completionListView.currentIndex = (completionListView.currentIndex - 1 + completionListView.count) % completionListView.count;
                    } else {
                        completionListView.currentIndex = (completionListView.currentIndex + 1) % completionListView.count;
                    }
                }
            } 
            // UP / DOWN
            else if (event.key === Qt.Key_Up) {
                if (completionPopup.opened && completionList.length > 0) {
                    event.accepted = true;
                    completionListView.decrementCurrentIndex();
                }
            }
            else if (event.key === Qt.Key_Down) {
                    if (completionPopup.opened && completionList.length > 0) {
                    event.accepted = true;
                    completionListView.incrementCurrentIndex();
                }
            }
            // RIGHT
            else if (event.key === Qt.Key_Right) {
                if (completionPopup.opened && completionListView.currentItem) {
                        // Drill Down
                        event.accepted = true;
                        text = completionList[completionListView.currentIndex];
                        // Reset selection
                        completionListView.currentIndex = 0;
                }
            }
            // ENTER / RETURN
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                event.accepted = true;
                // Always submit the current text, regardless of completion popup
                sendCommand({"action": "change_path", "path": text});
                completionPopup.close();
            }
            // ESC
            else if (event.key === Qt.Key_Escape) {
                event.accepted = true;
                completionPopup.close();
            }
            // CTRL+BACKSPACE
            else if (event.key === Qt.Key_Backspace) {
                if (event.modifiers & Qt.ControlModifier || event.modifiers & Qt.AltModifier) {
                    event.accepted = true;
                    // Directory Delete
                    var txt = text;
                    if (txt.endsWith("/")) txt = txt.slice(0, -1);
                    var lastSlash = txt.lastIndexOf("/");
                    if (lastSlash !== -1) {
                        text = txt.substring(0, lastSlash + 1);
                    } else {
                        text = "";
                    }
                }
            }
        }
        
        // Keep completion popup
        Popup { 
            id: completionPopup
            width: parent.width 
            height: 200
            y: parent.height + 2 // Offset slightly
            background: Rectangle { color: XsStyleSheet.panelBgColor; border.color: XsStyleSheet.menuBorderColor }
            contentItem: ListView { 
                id: completionListView
                model: completionList
                clip: true
                highlight: Rectangle { 
                    color: XsStyleSheet.accentColor
                    opacity: 0.5
                }
                highlightMoveDuration: 0
                delegate: Item {
                    width: parent.width
                    height: 25
                    Rectangle { anchors.fill: parent; color: "transparent" }
                    XsText { 
                        text: modelData
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 5
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            pathField.text = modelData
                            completionPopup.close()
                            pathField.forceActiveFocus()
                            sendCommand({"action": "complete_path", "path": pathField.text})
                        }
                    }
                }
            }
        }
    }
    
    XsPrimaryButton {

        id: historyBtn
        Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
        Layout.preferredWidth: 32
        
        imgSrc: "qrc:/icons/history.svg"
        
        property var lastCloseTime: 0
        
        onClicked: {
            var timeSinceClose = Date.now() - lastCloseTime
            if (timeSinceClose > 100) {
                pathPopup.open()
            }
        }
        
        Popup {
            id: pathPopup
            y: parent.height
            x: parent.width - width // Right align with button
            width: 500
            height: 300
            padding: 0
            
            onClosed: {
                historyBtn.lastCloseTime = Date.now()
            }
            
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            
            background: Rectangle {
                color: XsStyleSheet.panelTitleBarColor
                border.color: XsStyleSheet.menuBorderColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                    color: XsStyleSheet.panelBgColor
                    XsLabel { 
                        text: "QUICK ACCESS"
                        color: "#ffffff"
                        font.bold: true
                        anchors.centerIn: parent
                    }
                }
                
                ListView {
                    id: combinedView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: combinedList
                    
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 25
                        color: mouseArea.containsMouse ? XsStyleSheet.baseColor : "transparent"
                        
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                sendCommand({"action": "change_path", "path": modelData.path})
                                pathPopup.close()
                            }
                        }
                        
                        RowLayout {
                            anchors.fill: parent
                            spacing: 5
                            
                            // Pin Toggle Button
                            XsSecondaryButton {
                                Layout.preferredWidth: 25
                                Layout.preferredHeight: 25
                                                                
                                imgSrc: "qrc:/icons/keep.svg"
                                imgOverlayColor: modelData.isPinned ? XsStyleSheet.accentColor : XsStyleSheet.primaryTextColor
                                
                                onClicked: {
                                    if (modelData.isPinned) {
                                        sendCommand({"action": "remove_pin", "path": modelData.path})
                                    } else {
                                        sendCommand({"action": "add_pin", "name": modelData.name, "path": modelData.path})
                                    }
                                }
                            }
                            
                            // Path Name
                            XsText {
                                text: modelData.name
                                color: "#ffffff"
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                            }
                            
                            // Path Hint (Right aligned, faded)
                            XsText {
                                text: modelData.path
                                color: "#ffffff"
                                Layout.preferredWidth: parent.width * 0.4
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                                visible: parent.width > 300
                            }
                            
                            Item { Layout.preferredWidth: 5 }
                        }
                    }
                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }
    }
}
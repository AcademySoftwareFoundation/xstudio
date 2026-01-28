
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xStudio 1.0

Rectangle {
    id: root
    color: "#222222"
    anchors.fill: parent // Ensure it fills the panel

    // Access the attributes exposed by the plugin
    XsModuleData {
        id: pluginData
        modelDataName: "Filesystem Browser" 
    }
    XsAttributeValue {
        id: files_attr
        attributeTitle: "file_list"
        model: pluginData
        role: "value" // Explicitly request value role
        
        function updateList() {
             var rawVal = value
             try {
                if (typeof(rawVal) === "string" && rawVal !== "") {
                    if (rawVal === "[]") {
                         fileList = []
                    } else {
                        var parsed = JSON.parse(rawVal)
                        fileList = parsed
                    }
                }
             } catch(e) {
                console.log("files_attr: Parse Error: " + e)
             }
        }

        onValueChanged: updateList()
        Component.onCompleted: updateList()
    }
    
    XsAttributeValue {
        id: current_path_attr
        attributeTitle: "current_path"
        model: pluginData
    }

    XsAttributeValue {
        id: command_attr
        attributeTitle: "command_channel"
        model: pluginData
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

    XsAttributeValue {
        id: searching_attr
        attributeTitle: "searching"
        model: pluginData
    }

    function sendCommand(cmd) {
        command_attr.value = JSON.stringify(cmd)
    }

    // Local property to hold the parsed JSON file list
    property var fileList: []
    property var completionList: []
    
    // Sorting State
    property string sortColumn: "name"
    property int sortOrder: 1 // 1 for asc, -1 for desc

    // Column Widths (Default values)
    property real colWidthName: 250
    property real colWidthPath: 400
    property real colWidthSize: 80
    property real colWidthFrames: 120

    function sortFiles(column) {
        if (sortColumn === column) {
            sortOrder *= -1
        } else {
            sortColumn = column
            sortOrder = 1
        }

        var list = fileList.slice() // Copy
        list.sort(function(a, b) {
            var valA = a[column]
            var valB = b[column]
            
            // Handle undefined
            if (valA === undefined) valA = ""
            if (valB === undefined) valB = ""
            
            // Numeric sort for Size (parse KB)
            if (column === "size_str") {
                 var numA = parseFloat(valA)
                 var numB = parseFloat(valB)
                 if (isNaN(numA)) numA = 0
                 if (isNaN(numB)) numB = 0
                 return (numA - numB) * sortOrder
            }
            
            // Numeric sort for Frames (count)
            if (column === "frames") {
                 var numA = parseInt(valA)
                 var numB = parseInt(valB)
                 if (isNaN(numA)) numA = 0
                 if (isNaN(numB)) numB = 0
                 return (numA - numB) * sortOrder
            }

            // String sort
            if (typeof(valA) === "string") valA = valA.toLowerCase()
            if (typeof(valB) === "string") valB = valB.toLowerCase()

            if (valA < valB) return -1 * sortOrder
            if (valA > valB) return 1 * sortOrder
            return 0
        })
        
        fileList = list
    }

    // Layout Constants - Hardcoded for reliability
    property real rowHeight: 30
    property color textColor: "#e0e0e0"
    property color hintColor: "#aaaaaa"
    property real fontSize: 12

    // Debug: Show dimensions - Removed


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // Path Input Row
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: rowHeight
            spacing: 5
            
            Text {
                text: "Path:"
                color: hintColor
                verticalAlignment: Text.AlignVCenter
            }

            TextField {
                id: pathField
                Layout.fillWidth: true
                Layout.preferredHeight: rowHeight
                text: current_path_attr.value ? current_path_attr.value : "/"
                selectByMouse: true
                color: "white"
                font.pixelSize: fontSize
                verticalAlignment: Text.AlignVCenter
                leftPadding: 5
                
                background: Rectangle { 
                    color: "#333333" 
                    border.color: "#555555"
                    border.width: 1 
                }
                
                onAccepted: {
                    sendCommand({"action": "change_path", "path": text})
                }
                
                onTextEdited: {
                    sendCommand({"action": "complete_path", "path": text})
                }
                
                function getCommonPrefix(strings) {
                    if (!strings || strings.length === 0) return "";
                    var sorted = strings.slice().sort();
                    var s1 = sorted[0];
                    var s2 = sorted[sorted.length - 1];
                    var i = 0;
                    while (i < s1.length && i < s2.length && s1.charAt(i) === s2.charAt(i)) {
                        i++;
                    }
                    return s1.substring(0, i);
                }

                Keys.onPressed: {
                    // TAB
                    if (event.key === Qt.Key_Tab) {
                        event.accepted = true;
                        if (completionPopup.opened && completionListView.count > 0) {
                            // Scenario A: Cycle
                            if (event.modifiers & Qt.ShiftModifier) {
                                completionListView.currentIndex = (completionListView.currentIndex - 1 + completionListView.count) % completionListView.count;
                            } else {
                                completionListView.currentIndex = (completionListView.currentIndex + 1) % completionListView.count;
                            }
                        } else if (completionList.length > 0) {
                            // Scenario B: Shell Completion
                            if (completionList.length === 1) {
                                text = completionList[0];
                            } else {
                                var prefix = getCommonPrefix(completionList);
                                if (prefix.length > text.length) {
                                    text = prefix;
                                }
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
                        if (completionPopup.opened && completionListView.currentItem) {
                             text = completionList[completionListView.currentIndex];
                             completionPopup.close();
                             completionListView.currentIndex = -1;
                        } else {
                             sendCommand({"action": "change_path", "path": text});
                             completionPopup.close();
                        }
                    }
                    // ESC
                    else if (event.key === Qt.Key_Escape) {
                        event.accepted = true;
                        completionPopup.close();
                    }
                    // CTRL+BACKSPACE (or Option+Delete on Mac which maps to some key... usually Backspace + Alt modifier)
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

                // Auto-completion Popup
                Popup {
                    id: completionPopup
                    y: parent.height
                    width: parent.width
                    height: Math.min(completionList.length * 25, 200)
                    padding: 0
                    margins: 0
                    closePolicy: Popup.CloseOnEscape
                    
                    background: Rectangle {
                        color: "#333333"
                        border.color: "#555555"
                    }
                    
                    contentItem: ListView {
                        id: completionListView
                        model: completionList
                        clip: true
                        highlight: Rectangle { color: "#444444" }
                        highlightMoveDuration: 0
                        
                        delegate: Item {
                            width: parent.width
                            height: 25
                            
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent" // Highlight handled by ListView
                            }
                            
                            Text {
                                text: modelData
                                color: "#cccccc"
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 5
                                font.pixelSize: fontSize
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
            
            Button {
                text: "Browse"
                Layout.preferredHeight: rowHeight
                onClicked: {
                   sendCommand({"action": "request_browser"})
                }
            }

            Button {
                text: "Go"
                Layout.preferredHeight: rowHeight
                onClicked: {
                    sendCommand({"action": "change_path", "path": pathField.text})
                }
            }
        }

        // Filter Field
        TextField {
            id: filterField
            Layout.fillWidth: true
            Layout.preferredHeight: rowHeight
            placeholderText: "Filter..."
            placeholderTextColor: "#888888" // Ensure visibility
            color: "white"
            font.pixelSize: fontSize
            verticalAlignment: Text.AlignVCenter
            leftPadding: 5
            
            background: Rectangle { 
                color: "#333333" 
                border.color: "#555555"
                border.width: 1
            }
        }

        // Table Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: rowHeight
            color: "#2a2a2a" // Background
            
            RowLayout {
                anchors.fill: parent
                spacing: 0
                
                // --- Name Column ---
                Rectangle {
                     Layout.preferredWidth: colWidthName
                     Layout.fillHeight: true
                     color: "transparent"
                     Text {
                         text: "Name " + (sortColumn === "name" ? (sortOrder === 1 ? "▲" : "▼") : "")
                         anchors.fill: parent
                         verticalAlignment: Text.AlignVCenter
                         leftPadding: 5
                         color: hintColor
                         font.pixelSize: fontSize
                         font.weight: Font.DemiBold
                     }
                     MouseArea {
                         anchors.fill: parent
                         onClicked: sortFiles("name")
                         cursorShape: Qt.PointingHandCursor
                     }
                     // Resize Handle
                     Rectangle {
                         width: 5; height: parent.height
                         anchors.right: parent.right
                         color: "transparent"
                         MouseArea {
                             anchors.fill: parent
                             cursorShape: Qt.SplitHCursor
                             drag.target: parent
                             drag.axis: Drag.XAxis
                             property real startX
                             onPressed: startX = mouseX
                             onPositionChanged: {
                                if (pressed) {
                                    var delta = mouseX - startX
                                    if (colWidthName + delta > 50) colWidthName += delta
                                }
                             }
                         }
                     }
                }

                // --- Path Column ---
                Rectangle {
                     Layout.preferredWidth: colWidthPath
                     Layout.fillHeight: true
                     color: "transparent"
                     Text {
                         text: "Path " + (sortColumn === "relpath" ? (sortOrder === 1 ? "▲" : "▼") : "")
                         anchors.fill: parent
                         verticalAlignment: Text.AlignVCenter
                         leftPadding: 5
                         color: hintColor
                         font.pixelSize: fontSize
                         font.weight: Font.DemiBold
                     }
                     MouseArea {
                         anchors.fill: parent
                         onClicked: sortFiles("relpath")
                         cursorShape: Qt.PointingHandCursor
                     }
                     // Resize Handle
                     Rectangle {
                         width: 5; height: parent.height
                         anchors.right: parent.right
                         color: "transparent"
                         MouseArea {
                             anchors.fill: parent
                             cursorShape: Qt.SplitHCursor
                             property real startX
                             onPressed: startX = mouseX
                             onPositionChanged: {
                                if (pressed) {
                                    var delta = mouseX - startX
                                    if (colWidthPath + delta > 50) colWidthPath += delta
                                }
                             }
                         }
                     }
                }

                // --- Size Column ---
                Rectangle {
                     Layout.preferredWidth: colWidthSize
                     Layout.fillHeight: true
                     color: "transparent"
                     Text {
                         text: "Size " + (sortColumn === "size_str" ? (sortOrder === 1 ? "▲" : "▼") : "")
                         anchors.fill: parent
                         verticalAlignment: Text.AlignVCenter
                         leftPadding: 5
                         color: hintColor
                         font.pixelSize: fontSize
                         font.weight: Font.DemiBold
                     }
                     MouseArea {
                         anchors.fill: parent
                         onClicked: sortFiles("size_str")
                         cursorShape: Qt.PointingHandCursor
                     }
                     // Resize Handle
                     Rectangle {
                         width: 5; height: parent.height
                         anchors.right: parent.right
                         color: "transparent"
                         MouseArea {
                             anchors.fill: parent
                             cursorShape: Qt.SplitHCursor
                             property real startX
                             onPressed: startX = mouseX
                             onPositionChanged: {
                                if (pressed) {
                                    var delta = mouseX - startX
                                    if (colWidthSize + delta > 50) colWidthSize += delta
                                }
                             }
                         }
                     }
                }

                // --- Frames Column ---
                Rectangle {
                     Layout.preferredWidth: colWidthFrames
                     Layout.fillHeight: true
                     color: "transparent"
                     Text {
                         text: "Frames " + (sortColumn === "frames" ? (sortOrder === 1 ? "▲" : "▼") : "")
                         anchors.fill: parent
                         verticalAlignment: Text.AlignVCenter
                         leftPadding: 5
                         color: hintColor
                         font.pixelSize: fontSize
                         font.weight: Font.DemiBold
                     }
                     MouseArea {
                         anchors.fill: parent
                         onClicked: sortFiles("frames")
                         cursorShape: Qt.PointingHandCursor
                     }
                     // Resize Handle (Optional on last column?)
                     Rectangle {
                         width: 5; height: parent.height
                         anchors.right: parent.right
                         color: "transparent"
                         MouseArea {
                             anchors.fill: parent
                             cursorShape: Qt.SplitHCursor
                             property real startX
                             onPressed: startX = mouseX
                             onPositionChanged: {
                                if (pressed) {
                                    var delta = mouseX - startX
                                    if (colWidthFrames + delta > 50) colWidthFrames += delta
                                }
                             }
                         }
                     }
                }
                
                // Spacer to take up remaining space if columns are small
                Item { Layout.fillWidth: true }
            }
        }

        // File List
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // Explicit Background for List
            Rectangle {
                anchors.fill: parent
                color: "#222222" 
            }
            
            ListView {
                id: fileListView
                anchors.fill: parent
                anchors.rightMargin: 12
                clip: true
                
                model: fileList
                
                delegate: Rectangle {
                    id: delegate
                    width: root.width - 20 // Use root width minus margins
                    
                    property bool matchesFilter: filterField.text === "" || (modelData.path && modelData.path.toLowerCase().indexOf(filterField.text.toLowerCase()) !== -1)
                    
                    
                    height: matchesFilter ? rowHeight : 0
                    visible: matchesFilter

                    property bool isSelected: ListView.isCurrentItem
                    property bool isHovered: false

                    Rectangle {
                        anchors.fill: parent
                        color: isSelected ? "#555555" : (isHovered ? "#333333" : "#222222") // Lighter grey for selection
                        opacity: 1.0 
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0
                        
                        Text { 
                            text: modelData.name !== undefined ? modelData.name : ""
                            Layout.preferredWidth: colWidthName
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                            leftPadding: 5
                            color: isSelected ? "#ffffff" : "#cccccc" 
                            font.pixelSize: fontSize
                        }
                        Text { 
                            text: modelData.path !== undefined ? modelData.path : ""
                            Layout.preferredWidth: colWidthPath 
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                            leftPadding: 5
                            color: isSelected ? "#dddddd" : "#999999"
                            font.pixelSize: fontSize
                        }
                        Text { 
                            text: modelData.size_str !== undefined ? modelData.size_str : ""
                            Layout.preferredWidth: colWidthSize 
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 5
                            color: isSelected ? "#dddddd" : "#999999"
                            font.pixelSize: fontSize
                        }
                        Text { 
                            text: modelData.frames !== undefined ? modelData.frames : ""
                            Layout.preferredWidth: colWidthFrames 
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 5
                            color: isSelected ? "#dddddd" : "#999999"
                            font.pixelSize: fontSize
                        }
                        
                        Item { Layout.fillWidth: true }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: isHovered = true
                        onExited: isHovered = false
                        onClicked: {
                            fileListView.currentIndex = index
                            sendCommand({"action": "load_file", "path": modelData.path})
                        }
                    }
                }
            }
            
            ScrollBar {
                id: vbar
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 10
                policy: ScrollBar.AsNeeded
                active: true // Always show if needed
                orientation: Qt.Vertical
                size: fileListView.visibleArea.heightRatio
                position: fileListView.visibleArea.yPosition
                
                onPositionChanged: {
                     if (pressed) {
                         fileListView.contentY = position * fileListView.contentHeight
                     }
                }
            }

            // Loading Indicator
            BusyIndicator {
                id: loadingIndicator
                anchors.centerIn: parent
                visible: searching_attr.value !== undefined ? searching_attr.value : false
                running: visible
                palette.dark: "#ffffff" // Try to make it visible
                z: 100 // Ensure it's on top
            }
        }
    }
}

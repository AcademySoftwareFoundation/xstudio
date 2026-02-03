import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQuick.Shapes 1.15    // Added for vector icon

import xStudio 1.0
import xstudio.qml.models 1.0
import xStudio 1.0

Rectangle {
    id: root
    color: "#222222"
    anchors.fill: parent // Ensure it fills the panel

    // Access the attributes exposed by the plugin
    property string currentFilterTime: "Any"
    property string currentFilterVersion: "All Versions"

    XsModuleData {
        id: pluginData
        modelDataName: "Filesystem Browser" 
    }
    
    // Additional Attributes for History/Pins
    XsAttributeValue {
        id: history_attr
        attributeTitle: "history_paths"
        model: pluginData
        role: "value"
        
        function updateList() {
            var rawVal = value
            try {
                if (typeof(rawVal) === "string" && rawVal !== "") {
                    if (rawVal === "[]") {
                        historyList = []
                    } else {
                        var parsed = JSON.parse(rawVal)
                        historyList = parsed
                    }
                } else {
                    historyList = []
                }
            } catch(e) {
                console.log("history_attr: Parse Error: " + e)
                historyList = []
            }
        }
        
        onValueChanged: updateList()
        Component.onCompleted: updateList()
    }
    
    XsAttributeValue {
        id: pinned_attr
        attributeTitle: "pinned_paths"
        model: pluginData
        role: "value"
        
        function updateList() {
            var rawVal = value
            try {
                if (typeof(rawVal) === "string" && rawVal !== "") {
                    if (rawVal === "[]") {
                        pinnedList = []
                    } else {
                        var parsed = JSON.parse(rawVal)
                        pinnedList = parsed
                    }
                } else {
                    pinnedList = []
                }
            } catch(e) {
                console.log("pinned_attr: Parse Error: " + e)
                pinnedList = []
            }
        }
        
        onValueChanged: updateList()
        Component.onCompleted: updateList()
    }
    
    property var historyList: []
    property var pinnedList: []
    property var combinedList: []

    function updateCombinedList() {
        var combined = []
        var seen = new Set() // Set of paths
        
        // 1. Add Pinned Items
        if (pinnedList) {
            for (var i = 0; i < pinnedList.length; i++) {
                var p = pinnedList[i]
                combined.push({
                    "name": p.name,
                    "path": p.path,
                    "isPinned": true
                })
                // Add to seen set (mock Set using object for ES5/QML compat if needed, but modern QML has Set)
                // actually JS in QML usually has Set. If not, use object keys.
                seen.add(p.path)
            }
        }
        
        // 2. Add History Items
        if (historyList) {
            for (var j = 0; j < historyList.length; j++) {
                var h = historyList[j]
                if (!seen.has(h)) {
                     // Determine name (basename)
                     var name = h
                     if (h && h.indexOf("/") !== -1) {
                         var parts = h.split("/")
                         // Handle trailing slash
                         var last = parts[parts.length-1]
                         if (!last && parts.length > 1) last = parts[parts.length-2]
                         if (last) name = last
                     }
                     
                     combined.push({
                        "name": name, 
                        "path": h,
                        "isPinned": false
                     })
                     seen.add(h)
                }
            }
        }
        
        combinedList = combined
    }
    
    // Trigger update when source lists change
    onHistoryListChanged: updateCombinedList()
    onPinnedListChanged: updateCombinedList()
    
    property bool isCurrentPinned: {
        var curr = current_path_attr.value
        for(var i=0; i<pinnedList.length; i++) {
            if (pinnedList[i].path === curr) return true
        }
        return false
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
        id: scanned_attr
        attributeTitle: "scanned_count"
        model: pluginData
    }

    XsAttributeValue {
        id: progress_attr
        attributeTitle: "scan_progress"
        model: pluginData
    }
    
    // Filters
    XsAttributeValue { 
        id: filter_time_attr
        attributeTitle: "filter_time"
        model: pluginData
        role: "value" 
        onValueChanged: {
            console.log("QML: filter_time changed to: " + value)
            currentFilterTime = value || "Any"
        }
        Component.onCompleted: {
             console.log("QML: filter_time init value: " + value)
             currentFilterTime = value || "Any"
        }
    }
    XsAttributeValue { 
        id: filter_version_attr
        attributeTitle: "filter_version"
        model: pluginData
        role: "value"
        onValueChanged: {
             console.log("QML: filter_version changed to: " + value)
             currentFilterVersion = value || "All Versions"
        }
        Component.onCompleted: {
             console.log("QML: filter_version init value: " + value)
             currentFilterVersion = value || "All Versions"
        }
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
    property real colWidthOwner: 80
    property real colWidthVersion: 60
    property real colWidthDate: 140
    property real colWidthSize: 80
    property real colWidthFrames: 120
    property real colWidthPath: 300

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
            // Note: "frames" is string "1-100" or simple "1". 
            // Better to sort by first frame?
            // Or just string sort.
            
            // Numeric sort for Date
            if (column === "date") {
                // valA is float timestamp
                return (valA - valB) * sortOrder
            }
            // Numeric sort for Version
            if (column === "version") {
                var vA = a["version"] || 0
                var vB = b["version"] || 0
                return (vA - vB) * sortOrder
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
    
    function formatDate(timestamp) {
        if (!timestamp) return ""
        var d = new Date(timestamp * 1000)
        return d.toLocaleString(Qt.locale(), Locale.ShortFormat)
    }

    function getCommonPrefix(strings) {
        if (!strings || strings.length === 0) return "";
        var prefix = strings[0];
        for (var i = 1; i < strings.length; i++) {
            while (strings[i].indexOf(prefix) !== 0) {
                prefix = prefix.substring(0, prefix.length - 1);
                if (prefix === "") return "";
            }
        }
        return prefix;
    }

    // Layout Constants - Hardcoded for reliability
    property real rowHeight: 30
    property color textColor: "#e0e0e0"
    property color hintColor: "#aaaaaa"
    property real fontSize: 12



    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

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
                focus: true
                
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
                    background: Rectangle { color: "#333333"; border.color: "#555555" }
                    contentItem: ListView { 
                        id: completionListView
                        model: completionList
                        clip: true
                        highlight: Rectangle { color: "#444444" }
                        highlightMoveDuration: 0
                        delegate: Item {
                            width: parent.width
                            height: 25
                            Rectangle { anchors.fill: parent; color: "transparent" }
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
                id: historyBtn
                Layout.preferredHeight: rowHeight
                Layout.preferredWidth: rowHeight
                
                // Using a down arrow character for simplicity if icon not available, 
                // but user asked for "Down Triangle".
                text: "▼" 
                font.pixelSize: 10
                
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "#e0e0e0"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: parent.down || pathPopup.opened ? "#222222" : (parent.hovered ? "#444444" : "transparent")
                    border.width: 0
                }

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
                        color: "#2a2a2a"
                        border.color: "#555555"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        spacing: 0

                        // Header
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 25
                            color: "#333333"
                            Label { 
                                text: "QUICK ACCESS"
                                color: "#aaaaaa"
                                font.pixelSize: 10
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
                                color: mouseArea.containsMouse ? "#444444" : "transparent"
                                
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
                                    Button {
                                        Layout.preferredWidth: 25
                                        Layout.preferredHeight: 25
                                        
                                        background: Rectangle {
                                            color: "transparent"
                                        }
                                        
                                        contentItem: Shape {
                                            anchors.centerIn: parent
                                            width: 14
                                            height: 14
                                            
                                            // Scale the 24x24 SVG path to our 14x14 box
                                            scale: 14/24.0
                                            transformOrigin: Item.Center

                                            ShapePath {
                                                strokeWidth: 0
                                                strokeColor: "transparent"
                                                fillColor: modelData.isPinned ? "#ffffff" : "#444444"
                                                
                                                // M16 12V4h1V2H7v2h1v8l-2 5v2h6v3.5l1 1 1-1V19h6v-2l-2-5z  (Standard Pin)
                                                // Coordinate system is roughly 24x24
                                                PathSvg {
                                                    path: "M16 12V4h1V2H7v2h1v8l-2 5v2h6v3.5l1 1 1-1V19h6v-2l-2-5z"
                                                }
                                            }
                                        }
                                        
                                        onClicked: {
                                            if (modelData.isPinned) {
                                                sendCommand({"action": "remove_pin", "path": modelData.path})
                                            } else {
                                                sendCommand({"action": "add_pin", "name": modelData.name, "path": modelData.path})
                                            }
                                        }
                                    }
                                    
                                    // Path Name
                                    Text {
                                        text: modelData.name
                                        color: "#e0e0e0"
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    
                                    // Path Hint (Right aligned, faded)
                                    Text {
                                        text: modelData.path
                                        color: "#666666"
                                        font.pixelSize: 9
                                        Layout.preferredWidth: parent.width * 0.4
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
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
    

        
        // Filter Row
        RowLayout {
             Layout.fillWidth: true
             Layout.preferredHeight: rowHeight
             spacing: 5
             
             ComboBox {
                 id: filterTimeCombo
                 model: ["Any", "Last 1 day", "Last 2 days", "Last 1 week", "Last 1 month"]
                 Layout.preferredWidth: 120
                 Layout.preferredHeight: rowHeight
                 currentIndex: model.indexOf(currentFilterTime)
                 onActivated: {
                     console.log("Time Filter Changed to: " + currentText)
                     // Send command to update backend
                     sendCommand({"action": "set_attribute", "name": "filter_time", "value": currentText})
                     // Optimistically update local state (backend update will confirm it via onValueChanged)
                     currentFilterTime = currentText
                     fileListView.forceLayout() 
                 }
                 delegate: ItemDelegate {
                      width: ListView.view.width
                      contentItem: Text {
                          text: modelData
                          color: "#e0e0e0"
                          font.pixelSize: fontSize
                          elide: Text.ElideRight
                          verticalAlignment: Text.AlignVCenter
                      }
                      background: Rectangle {
                          color: parent.highlighted ? "#444444" : "#222222"
                      }
                      highlighted: filterTimeCombo.highlightedIndex === index
                  }

                  popup: Popup {
                      y: parent.height - 1
                      width: parent.width
                      implicitHeight: contentItem.implicitHeight
                      padding: 1

                      contentItem: ListView {
                          clip: true
                          implicitHeight: contentHeight
                          model: filterTimeCombo.popup.visible ? filterTimeCombo.delegateModel : null
                          currentIndex: filterTimeCombo.highlightedIndex
                          ScrollIndicator.vertical: ScrollIndicator { }
                      }

                      background: Rectangle {
                          border.color: "#555555"
                          color: "#222222"
                      }
                  }
             }
             
             ComboBox {
                 id: filterVersionCombo
                 model: ["All Versions", "Latest Version", "Latest 2 Versions"]
                 Layout.preferredWidth: 140
                 Layout.preferredHeight: rowHeight
                 currentIndex: model.indexOf(currentFilterVersion)
                 onActivated: {
                      sendCommand({"action": "set_attribute", "name": "filter_version", "value": currentText})
                      currentFilterVersion = currentText
                      fileListView.forceLayout()
                 }
                 delegate: ItemDelegate {
                      width: ListView.view.width
                      contentItem: Text {
                          text: modelData
                          color: "#e0e0e0"
                          font.pixelSize: fontSize
                          elide: Text.ElideRight
                          verticalAlignment: Text.AlignVCenter
                      }
                      background: Rectangle {
                          color: parent.highlighted ? "#444444" : "#222222"
                      }
                      highlighted: filterVersionCombo.highlightedIndex === index
                  }
                  
                  popup: Popup {
                      y: parent.height - 1
                      width: parent.width
                      implicitHeight: contentItem.implicitHeight
                      padding: 1

                      contentItem: ListView {
                          clip: true
                          implicitHeight: contentHeight
                          model: filterVersionCombo.popup.visible ? filterVersionCombo.delegateModel : null
                          currentIndex: filterVersionCombo.highlightedIndex
                          ScrollIndicator.vertical: ScrollIndicator { }
                      }

                      background: Rectangle {
                          border.color: "#555555"
                          color: "#222222"
                      }
                  }
             }

            // Text Filter
            TextField {
                id: filterField
                Layout.fillWidth: true
                Layout.preferredHeight: rowHeight
                placeholderText: "Filter String..."
                placeholderTextColor: "#888888" 
                color: "white"
                font.pixelSize: fontSize
                leftPadding: 5
                background: Rectangle { color: "#333333"; border.color: "#555555" }
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
                
                // Helper to create columns
                component HeaderColumn: Rectangle {
                    property string title
                    property string colId
                    property alias colWidth: rect.width
                    property bool resizable: true
                    id: rect
                    Layout.fillHeight: true
                    color: "transparent"
                    Layout.preferredWidth: width
                    Text {
                        text: title + (sortColumn === colId ? (sortOrder === 1 ? " ▲" : " ▼") : "")
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 5
                        color: hintColor
                        font.pixelSize: fontSize
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: sortFiles(colId)
                        cursorShape: Qt.PointingHandCursor
                    }
                    Rectangle {
                         visible: resizable
                         width: 5; height: parent.height
                         anchors.right: parent.right
                         color: "transparent"
                         MouseArea {
                             anchors.fill: parent; cursorShape: Qt.SplitHCursor
                             drag.target: rect; drag.axis: Drag.XAxis
                             property real startX
                             onPressed: startX = mouseX
                             onPositionChanged: if(pressed) { var d=mouseX-startX; if(rect.width+d>30) rect.width+=d }
                         }
                    }
                }

                HeaderColumn { title: "Name"; colId: "name"; width: colWidthName; onWidthChanged: colWidthName=width }
                HeaderColumn { title: "Version"; colId: "version"; width: colWidthVersion; onWidthChanged: colWidthVersion=width }
                HeaderColumn { title: "Owner"; colId: "owner"; width: colWidthOwner; onWidthChanged: colWidthOwner=width }
                HeaderColumn { title: "Date"; colId: "date"; width: colWidthDate; onWidthChanged: colWidthDate=width }
                HeaderColumn { title: "Size"; colId: "size_str"; width: colWidthSize; onWidthChanged: colWidthSize=width }
                HeaderColumn { title: "Frames"; colId: "frames"; width: colWidthFrames; onWidthChanged: colWidthFrames=width }
                HeaderColumn { title: "Path"; colId: "relpath"; Layout.fillWidth: true; resizable: false }
            }
        }

        // File List
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            Rectangle { anchors.fill: parent; color: "#222222" }
            
            ListView {
                id: fileListView
                anchors.fill: parent
                anchors.rightMargin: 12
                clip: true
                model: fileList
                
                delegate: Rectangle {
                    id: delegate
                    width: root.width - 20
                    property bool matchesFilter: {
                        // Text Filter
                        var filterText = filterField.text.trim();
                        var textMatch = true;
                        if (filterText !== "") {
                            var terms = filterText.toLowerCase().split(/\s+/);
                            var nameLower = (modelData.name || "").toLowerCase();
                            for (var i = 0; i < terms.length; i++) {
                                if (nameLower.indexOf(terms[i]) === -1) {
                                    textMatch = false;
                                    break;
                                }
                            }
                        }
                        if (!textMatch) return false;
                        
                        // Time Filter
                        var timeMatch = true;
                        var t_val = currentFilterTime; // e.g. "Last 1 day"
                        if (t_val !== "Any" && modelData.date) {
                             var now = Date.now() / 1000.0;
                             var diff = now - modelData.date;
                             var day = 86400;
                             if (t_val === "Last 1 day") timeMatch = diff <= day;
                             else if (t_val === "Last 2 days") timeMatch = diff <= 2*day;
                             else if (t_val === "Last 1 week") timeMatch = diff <= 7*day;
                             else if (t_val === "Last 1 month") timeMatch = diff <= 30*day;
                        }
                        if (!timeMatch) return false;
                        
                        // Version Filter
                        var verMatch = true;
                        var v_val = currentFilterVersion;
                        if (v_val === "Latest Version") {
                            verMatch = modelData.is_latest_version === true;
                        } else if (v_val === "Latest 2 Versions") {
                             // Using version_rank exposed by scanner.py
                             if (modelData.version_rank !== undefined) {
                                  verMatch = (modelData.version_rank <= 1);
                             }
                        }
                        
                        return verMatch;
                    }
                        

                    
                    height: matchesFilter ? rowHeight : 0
                    visible: matchesFilter

                    property bool isSelected: ListView.isCurrentItem
                    property bool isHovered: false

                    Rectangle {
                        anchors.fill: parent
                        color: isSelected ? "#555555" : (isHovered ? "#333333" : (index % 2 == 0 ? "#222222" : "#252525"))
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0
                        
                        // Cells
                        component Cell: Text {
                            property real w
                            Layout.preferredWidth: w
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 5
                            color: isSelected ? "#ffffff" : "#cccccc"
                            font.pixelSize: fontSize
                        }
                        
                        Cell { text: modelData.name || ""; w: colWidthName }
                        Cell { text: modelData.version ? "v"+modelData.version : ""; w: colWidthVersion; color: isSelected?"#eee":"#999" }
                        Cell { text: modelData.owner || ""; w: colWidthOwner; color: isSelected?"#eee":"#999" }
                        Cell { text: formatDate(modelData.date); w: colWidthDate; color: isSelected?"#eee":"#999" }
                        Cell { text: modelData.size_str || ""; w: colWidthSize; horizontalAlignment: Text.AlignRight; rightPadding: 5 }
                        Cell { text: modelData.frames || ""; w: colWidthFrames }
                        Cell { text: modelData.relpath || ""; Layout.fillWidth: true; color: isSelected?"#eee":"#888" }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: isHovered = true
                        onExited: isHovered = false
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: {
                            fileListView.currentIndex = index
                            if (mouse.button === Qt.RightButton) {
                                contextMenu.popup()
                            }
                        }
                        onDoubleClicked: {
                            fileListView.currentIndex = index
                            sendCommand({"action": "load_file", "path": modelData.path})
                        }
                    }
                    
                    Menu {
                        id: contextMenu
                        
                        background: Rectangle {
                            implicitWidth: 150
                            implicitHeight: 40
                            color: "#333333"
                            border.color: "#555555"
                            radius: 3
                        }
                        
                        delegate: MenuItem {
                            id: menuItem
                            
                            contentItem: Text {
                                text: menuItem.text
                                color: "#e0e0e0"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                leftPadding: 10
                            }
                            
                            background: Rectangle {
                                implicitWidth: 150
                                implicitHeight: 25
                                color: menuItem.highlighted ? "#555555" : "transparent"
                            }
                        }

                        MenuItem {
                            text: "Replace"
                            onTriggered: sendCommand({"action": "replace_current_media", "path": modelData.path})
                        }
                        MenuItem {
                            text: "Compare with"
                            onTriggered: sendCommand({"action": "compare_with_current_media", "path": modelData.path})
                        }
                    }
                }
            }
            
            ScrollBar {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                active: true
                policy: ScrollBar.AsNeeded
                size: fileListView.visibleArea.heightRatio
                position: fileListView.visibleArea.yPosition
                onPositionChanged: if(pressed) fileListView.contentY = position * fileListView.contentHeight
            }
        }
        
        // Progress Bar (Bottom)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "transparent"
            visible: searching_attr.value === true
            
            RowLayout {
                anchors.fill: parent
                spacing: 10
                
                ProgressBar {
                    id: scanProgress
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6 // Slimmer progress bar
                    Layout.alignment: Qt.AlignVCenter
                    from: 0
                    to: 100
                    value: progress_attr.value
                    indeterminate: true 
                    
                    background: Rectangle {
                        implicitWidth: 200
                        implicitHeight: 6
                        color: "#444444"
                        radius: 3
                    }
                    contentItem: Item {
                        implicitWidth: 200
                        implicitHeight: 4
                        
                        Rectangle {
                            width: scanProgress.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: "#17a81a"
                        }
                    }
                }
                
                Text {
                    text: "Scanning: " + (parseInt(scanned_attr.value) || 0) + " items..."
                    color: hintColor
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }
}

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
                    buildTree()
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
            refreshFiltering()
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
             refreshFiltering()
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
        id: scan_required_attr
        attributeTitle: "scan_required"
        model: pluginData
        role: "value"
    }

    XsAttributeValue {
        id: auto_scan_threshold_attr
        attributeTitle: "auto_scan_threshold"
        model: pluginData
        role: "value"
    }

    XsAttributeValue {
        id: searching_attr
        attributeTitle: "searching"
        model: pluginData
    }

    XsAttributeValue {
        id: scanned_dirs_attr
        attributeTitle: "scanned_dirs"
        model: pluginData
        role: "value"
        onValueChanged: {
             try {
                 var val = value
                 if (val && val !== "[]") {
                     scannedDirsList = JSON.parse(val)
                 } else {
                     scannedDirsList = []
                 }
             } catch(e) { }
        }
    }
    
    XsAttributeValue {
        id: depth_limit_attr
        attributeTitle: "recursion_limit"
        model: pluginData
        role: "value"
    }

    property var scannedDirsList: []

    function sendCommand(cmd) {
        command_attr.value = JSON.stringify(cmd)
    }

    // Local property to hold the parsed JSON file list
    property var fileList: []
    property var completionList: []
    
    // Sorting State
    property string sortColumn: "name"
    property int sortOrder: 1 // 1 for asc, -1 for desc
    
    // View Mode: 0=List, 1=Tree, 2=Grouped
    property int viewMode: 2 
    onViewModeChanged: buildTree()


    // Column Widths (Default values)
    property real minWidthName: 250
    property real colWidthName: 250 // kept for legacy reference or init
    property real colWidthOwner: 80
    property real colWidthVersion: 60
    property real colWidthDate: 140
    property real colWidthSize: 80
    property real colWidthFrames: 120
    
    // Width Calculations
    readonly property real fixedColumnsWidth: colWidthVersion + colWidthOwner + colWidthDate + colWidthSize + colWidthFrames + 20 // +20 spacer
    property real totalContentWidth: Math.max(fileListView.width, minWidthName + fixedColumnsWidth + 10) // +10 margin/padding


    // tree logic
    property var treeRoots: []
    property var visibleTreeList: []
    property var collapsedPaths: ({})

    function isVisible(data) {
        if (!data) return true;
        
        // Text Filter
        var filterText = filterField.text.trim();
        if (filterText !== "") {
            if (data.name.toLowerCase().indexOf(filterText.toLowerCase()) === -1) return false;
        }
        
        // Time Filter
        var t_val = currentFilterTime;
        if (t_val !== "Any" && data.date) {
             var now = Date.now() / 1000.0;
             var diff = now - data.date;
             var day = 86400;
             var timeMatch = true;
             if (t_val === "Last 1 day") timeMatch = diff <= day;
             else if (t_val === "Last 2 days") timeMatch = diff <= 2*day;
             else if (t_val === "Last 1 week") timeMatch = diff <= 7*day;
             else if (t_val === "Last 1 month") timeMatch = diff <= 30*day;
             
             if (!timeMatch) return false;
        }
        
        // Version Filter
        var v_val = currentFilterVersion;
        if (v_val === "Latest Version") {
            if (data.is_latest_version !== true) return false;
        } else if (v_val === "Latest 2 Versions") {
             if (data.version_rank !== undefined && data.version_rank > 1) return false;
        }
        
        return true;
    }

    function updateTreeVisibility(nodes) {
        var hasVisible = false;
        for(var i=0; i<nodes.length; i++) {
            var node = nodes[i];
            
            if (node.isFolder) {
                // Recursive check for folders
                var childrenVisible = updateTreeVisibility(node.children);
                // In Tree/Grouped view, folder is visible if it has visible children
                // OR if the folder itself matches the text filter? 
                // Usually for pruning, we only care if content is visible.
                // But if user searches for "folderName", they might expect to see it.
                // For now, strict pruning: only show if children are visible.
                node.visible = childrenVisible; 
            } else {
                // File/Sequence
                node.visible = isVisible(node.data);
            }
            
            if (node.visible) hasVisible = true;
        }
        return hasVisible;
    }
    
    function refreshFiltering() {
        updateTreeVisibility(treeRoots);
        flattenTree();
    }

    function buildTree() {
        var roots = []
        
        if (viewMode === 0) {
            // LIST VIEW: Flat list, no hierarchy logic
            for(var i=0; i<fileList.length; i++) {
                var file = fileList[i]
                
                // User requested to hide directories in List view
                var isDir = (file.is_folder === true || file.type === "Folder")
                if (isDir) continue
                
                var node = {
                    "name": file.name,
                    "path": file.path,
                    "isFolder": false,
                    "data": file,
                    "children": [],
                    "expanded": false,
                    "visible": true // Default
                }
                roots.push(node)
            }
            treeRoots = roots
            refreshFiltering() // Calculate visibility and flatten
            sortTree()
            return
        }

        // TREE / GROUPED VIEW
        var lookups = {}
        
        function getFolderNode(path, name, parent) {
           if (lookups[path]) return lookups[path];
           var node = {
               "name": name,
               "path": path,
               "isFolder": true,
               "children": [],
               "data": null,
               "expanded": (collapsedPaths[path] === undefined),
               "visible": true
           }
           lookups[path] = node
           if (parent) parent.children.push(node);
           else roots.push(node);
           return node
        }
        
        var rootAbs = current_path_attr.value || ""
        if (rootAbs !== "" && rootAbs.charAt(rootAbs.length-1) !== '/') rootAbs += '/'

        for(var i=0; i<fileList.length; i++) {
            var file = fileList[i]
            var parts = file.relpath.split("/")
            var currentRelPath = ""
            var parentNode = null
            
            var isDir = (file.is_folder === true || file.type === "Folder")
            
            var loopMax = parts.length - 1
            
            for(var j=0; j<loopMax; j++) {
                var pName = parts[j]
                currentRelPath = (currentRelPath ? currentRelPath + "/" : "") + pName
                var absPath = rootAbs + currentRelPath
                parentNode = getFolderNode(absPath, pName, parentNode)
            }
            
            if (isDir) {
                var pName = parts[parts.length-1]
                currentRelPath = (currentRelPath ? currentRelPath + "/" : "") + pName
                var absPath = rootAbs + currentRelPath
                
                var folderNode = getFolderNode(absPath, pName, parentNode)
                if (!folderNode.data) {
                    folderNode.data = file
                }
            } else {
                var leafName = parts[parts.length-1]
                var leaf = {
                   "name": leafName,
                   "path": file.path, 
                   "isFolder": false,
                   "data": file,
                   "children": [],
                   "expanded": false,
                   "visible": true
                }
                if (parentNode) parentNode.children.push(leaf);
                else roots.push(leaf);
            }
        }

        function compressNodes(nodes) {
             for (var i = 0; i < nodes.length; i++) {
                 var node = nodes[i];
                 if (node.isFolder) {
                      if(node.children.length > 0) compressNodes(node.children);
                      
                      while (node.children.length === 1) {
                           var child = node.children[0];
                           node.name = node.name + "/" + child.name;
                           node.path = child.path; 
                           node.data = child.data;
                           node.isFolder = child.isFolder;
                           node.children = child.children;
                           
                           if (node.isFolder) {
                               node.expanded = (collapsedPaths[node.path] === undefined);
                           } else {
                               node.expanded = false;
                           }
                      }
                 }
             }
        }
        
        // Only compress if in Grouped mode (2)
        if (viewMode === 2) {
            compressNodes(roots)
        }
        
        treeRoots = roots
        refreshFiltering() // Calculate visibility and flatten
        sortTree()
    }

    function sortTree() {
        var col = sortColumn
        var ord = sortOrder
        
        function recursiveSort(nodes) {
             nodes.sort(function(a, b) {
                 if (a.isFolder !== b.isFolder) return (a.isFolder ? -1 : 1);
                 
                 if (a.isFolder) return a.name.localeCompare(b.name);
                 
                 var valA = a.data ? a.data[col] : ""
                 var valB = b.data ? b.data[col] : ""
                 
                 if (col === "size_str") {
                     var nA = parseFloat(valA) || 0
                     var nB = parseFloat(valB) || 0
                     return (nA - nB) * ord
                 }
                 if (col === "date" || col === "version" || col === "frames") {
                      return ((a.data ? (a.data[col]||0) : 0) - (b.data ? (b.data[col]||0) : 0)) * ord
                 }
                 
                 var sA = String(valA).toLowerCase()
                 var sB = String(valB).toLowerCase()
                 if (sA < sB) return -1 * ord
                 if (sA > sB) return 1 * ord
                 return 0
             })
             
             for(var i=0; i<nodes.length; i++) {
                 if (nodes[i].children.length > 0) recursiveSort(nodes[i].children)
             }
        }
        recursiveSort(treeRoots)
        flattenTree()
    }
    
    function flattenTree() {
        var visible = []
        function traverse(nodes, depth) {
            for(var i=0; i<nodes.length; i++) {
                var node = nodes[i]
                if (node.visible) {
                    node.depth = depth 
                    visible.push(node)
                    if (node.isFolder && node.expanded) {
                        traverse(node.children, depth + 1)
                    }
                }
            }
        }
        traverse(treeRoots, 0);
        visibleTreeList = visible
    }
    
    function toggleExpand(index) {
        var node = visibleTreeList[index]
        if (node && node.isFolder) {
            node.expanded = !node.expanded
            if (node.expanded) delete collapsedPaths[node.path];
            else collapsedPaths[node.path] = true;
            flattenTree()
        }
    }

    function sortFiles(column) {
        if (sortColumn === column) {
            sortOrder *= -1
        } else {
            sortColumn = column
            sortOrder = 1
        }
        sortTree()
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

    // Toggle for Directory Tree
    property bool showDirectoryTree: true
    
    Action {
        id: toggleTreeAction
        text: "Toggle Directory Tree"
        shortcut: "Ctrl+T"
        onTriggered: showDirectoryTree = !showDirectoryTree
    }

    // Layout Constants - Hardcoded for reliability
    property real rowHeight: 30
    property color textColor: "#e0e0e0"
    property color hintColor: "#aaaaaa"
    property real fontSize: 12



    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        
        handle: Rectangle {
            implicitWidth: 4
            color: SplitHandle.pressed ? "#555555" : (SplitHandle.hovered ? "#444444" : "#222222")
        }

        // Tree Container (Manages Collapsed/Expanded states)
        Item {
            SplitView.preferredWidth: showDirectoryTree ? 250 : 30
            SplitView.minimumWidth: showDirectoryTree ? 150 : 30
            SplitView.maximumWidth: showDirectoryTree ? 400 : 30
            
            // Collapsed State (Sidebar)
            Rectangle {
                anchors.fill: parent
                color: "#1a1a1a"
                visible: !showDirectoryTree
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    Item { height: 10 } // Top spacer
                    
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: "▶"
                        
                        background: Rectangle {
                            color: "transparent"
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#aaaaaa"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                        
                        onClicked: showDirectoryTree = true
                        
                        ToolTip.visible: hovered
                        ToolTip.text: "Expand Directory Tree"
                    }
                    
                    Item { Layout.fillHeight: true } // Bottom spacer
                }
            }

            // Expanded State (Tree with Header)
            ColumnLayout {
                anchors.fill: parent
                visible: showDirectoryTree
                spacing: 0
                
                // Header with Pin Button
                Rectangle {
                    id: treeHeader
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: "#222222"
                    z: 10
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 5
                        
                        Text {
                            text: "Directories"
                            color: "#e0e0e0"
                            font.bold: true
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }
                        
                        Button {
                            text: dirTree.isPinned ? "Pinned" : "Pin"
                            Layout.preferredHeight: 20
                            flat: true
                            
                            background: Rectangle {
                                color: parent.down ? "#444444" : "transparent"
                                border.color: "#555555"
                                border.width: 1
                                radius: 2
                            }
                            
                            contentItem: Text {
                                text: parent.text
                                color: dirTree.isPinned ? "#4facfe" : "#aaaaaa"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: dirTree.isPinned = !dirTree.isPinned
                        }
                        
                        Button {
                            text: "Hide"
                            Layout.preferredHeight: 20
                            flat: true
                            
                            background: Rectangle {
                                color: parent.down ? "#444444" : "transparent"
                                radius: 2
                            }
                             contentItem: Text {
                                text: "×"
                                color: "#aaaaaa"
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: showDirectoryTree = false
                        }
                    }
                    
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#333333"
                        anchors.bottom: parent.bottom
                    }
                }

                // Directory Tree Component
                DirectoryTree {
                    id: dirTree
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    pluginData: pluginData
                    currentPath: current_path_attr.value
                    
                    onSendCommand: (cmd) => root.sendCommand(cmd)
                    
                    // Pinning Logic (State)
                    property bool isPinned: false
                    property int autoHideThreshold: auto_scan_threshold_attr.value || 4
                }
            }
            
            // Auto-hide Logic Connection
            Connections {
                target: dirTree
                function onCurrentPathChanged() {
                    // console.warn("DEBUG: Path changed to " + dirTree.currentPath)
                    if (!showDirectoryTree) return;
                    if (dirTree.isPinned) return;
                    
                    var p = dirTree.currentPath;
                    if (!p || p === "/") return;
                    
                    var threshold = dirTree.autoHideThreshold;
                    
                    // Calculate depth
                    var parts = p.split("/");
                    var depth = 0;
                    for (var i=0; i<parts.length; i++) {
                        if (parts[i] !== "") depth++;
                    }
                    
                    if (depth > threshold) {
                        console.warn("Auto-hiding directory tree. Depth " + depth + " > " + threshold);
                        showDirectoryTree = false;
                    }
                }
            }
        }

        ColumnLayout {
            SplitView.fillWidth: true
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

              // recursion limit
              RowLayout {
                   spacing: 5
                   Label { 
                       text: "Depth:"
                       color: "#aaaaaa"
                       font.pixelSize: fontSize
                       verticalAlignment: Text.AlignVCenter
                   }
                   SpinBox {
                       id: depthSpin
                       from: 1
                       to: 10
                       value: parseInt(depth_limit_attr.value) || 6
                       editable: true
                       Layout.preferredWidth: 80
                       Layout.preferredHeight: rowHeight
                       
                       onValueModified: {
                           sendCommand({"action": "set_attribute", "name": "recursion_limit", "value": value})
                           // Optimistic update
                           depth_limit_attr.value = value
                       }
                       
                       // Customizing background to match dark theme
                       contentItem: TextInput {
                            z: 2
                            text: depthSpin.textFromValue(depthSpin.value, depthSpin.locale)
                            font: depthSpin.font
                            color: "#e0e0e0"
                            selectionColor: "#21be2b"
                            selectedTextColor: "#ffffff"
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                            readOnly: !depthSpin.editable
                            validator: depthSpin.validator
                            inputMethodHints: Qt.ImhDigitsOnly
                        }
                        background: Rectangle {
                            implicitWidth: 80
                            implicitHeight: rowHeight
                            color: "#333333"
                            border.color: "#555555"
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
                onTextEdited: refreshFiltering()
            }
        }



        // Table Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: rowHeight
            color: "#2a2a2a" // Background
            
            Item {
                anchors.fill: parent
                clip: true
                RowLayout {
                    x: -fileListView.contentX
                    width: Math.max(parent.width, totalContentWidth)
                    height: parent.height
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

                    HeaderColumn { title: "Name"; colId: "name"; Layout.fillWidth: true; Layout.minimumWidth: minWidthName; resizable: false }
                    HeaderColumn { title: "Version"; colId: "version"; width: colWidthVersion; onWidthChanged: colWidthVersion=width }
                    HeaderColumn { title: "Frames"; colId: "frames"; width: colWidthFrames; onWidthChanged: colWidthFrames=width }
                    HeaderColumn { title: "Owner"; colId: "owner"; width: colWidthOwner; onWidthChanged: colWidthOwner=width }
                    HeaderColumn { title: "Date"; colId: "date"; width: colWidthDate; onWidthChanged: colWidthDate=width }
                    HeaderColumn { title: "Size"; colId: "size_str"; width: colWidthSize; onWidthChanged: colWidthSize=width }
                    Item { width: 20 } // Spacer at end
                }
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
                model: visibleTreeList
                
                contentWidth: totalContentWidth
                flickableDirection: Flickable.HorizontalAndVerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                
                // Manual Scan Overlay
                Rectangle {
                    anchors.centerIn: parent
                    width: 200
                    height: 100
                    color: "#333333"
                    visible: scan_required_attr.value === true
                    z: 100 // Ensure it's on top
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 10
                        
                        Text {
                            text: "Manual Scan Required"
                            color: "#aaaaaa"
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Button {
                            text: "Scan Directory"
                            Layout.alignment: Qt.AlignHCenter
                            onClicked: sendCommand({"action": "force_scan"})
                            
                            background: Rectangle {
                                color: parent.down ? "#444444" : "#555555"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 12
                            }
                        }
                    }
                }
                
                delegate: Rectangle {
                    id: delegate
                    width: totalContentWidth
                    height: rowHeight

                    property bool isSelected: ListView.isCurrentItem
                    property bool isHovered: false

                    Rectangle {
                        anchors.fill: parent
                        color: isSelected ? "#555555" : (isHovered ? "#333333" : (index % 2 == 0 ? "#222222" : "#252525"))
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: isHovered = true
                        onExited: isHovered = false
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            fileListView.currentIndex = index
                            if (mouse.button === Qt.RightButton) {
                                contextMenu.popup()
                            }
                        }
                        onDoubleClicked: (mouse) => {
                            fileListView.currentIndex = index
                            if (modelData.isFolder) {
                                sendCommand({"action": "change_path", "path": modelData.path})
                            } else {
                                sendCommand({"action": "load_file", "path": modelData.path})
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0
                        
                        // Cells
                        component Cell: Text {
                            property real w
                            property int elideMode: Text.ElideRight
                            Layout.preferredWidth: w
                            Layout.fillHeight: true
                            verticalAlignment: Text.AlignVCenter
                            elide: elideMode
                            leftPadding: 5
                            color: isSelected ? "#ffffff" : "#cccccc"
                            font.pixelSize: fontSize
                        }
                        
                        // Indentation
                        Item {
                            Layout.preferredWidth: (modelData.depth||0) * 20
                            Layout.fillHeight: true
                        }
                        
                        // Expander
                        Item {
                            Layout.preferredWidth: 20
                            Layout.fillHeight: true
                            Text {
                                anchors.centerIn: parent
                                text: (root.viewMode !== 0 && modelData.isFolder) ? (modelData.expanded ? "▼" : "▶") : ""
                                color: "#aaaaaa"
                                font.pixelSize: 10
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: toggleExpand(index)
                            }
                        }
                        
                        Cell { text: modelData.name || ""; Layout.fillWidth: true; Layout.minimumWidth: minWidthName; elideMode: Text.ElideMiddle }
                        Cell { text: (modelData.data && modelData.data.version) ? "v"+modelData.data.version : ""; w: colWidthVersion; color: isSelected?"#eee":"#999" }
                        Cell { text: (modelData.data && modelData.data.frames) || ""; w: colWidthFrames }
                        Cell { text: (modelData.data && modelData.data.owner) || ""; w: colWidthOwner; color: isSelected?"#eee":"#999" }
                        Cell { text: modelData.data ? formatDate(modelData.data.date) : ""; w: colWidthDate; color: isSelected?"#eee":"#999" }
                        Cell { text: (modelData.data && modelData.data.size_str) || ""; w: colWidthSize; horizontalAlignment: Text.AlignRight; rightPadding: 5 }
                        Item { width: 20 } // Spacer at end
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
        
        // Scanned Dirs Log (Visible during scan)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: searching_attr.value ? 100 : 0
            color: "#1a1a1a"
            visible: searching_attr.value === true
            clip: true
            
            ListView {
                anchors.fill: parent
                model: scannedDirsList
                clip: true
                delegate: Text {
                    text: modelData
                    color: "#888888"
                    font.pixelSize: 10
                    width: ListView.view.width
                    elide: Text.ElideMiddle
                }
                
                // Auto-scroll to bottom
                onCountChanged: {
                    positionViewAtEnd()
                }
            }
        }

        // Bottom Footer: Progress + View Modes
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: "transparent"
            
            RowLayout {
                anchors.fill: parent
                spacing: 10
                
                // Progress Bar (Left - fills remaining space)
                ProgressBar {
                    id: scanProgress
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    Layout.alignment: Qt.AlignVCenter
                    
                    // Only visible when scanning
                    visible: searching_attr.value === true
                    
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
                
                // If not scanning, we need a spacer to push buttons to right
                Item { 
                    Layout.fillWidth: true 
                    visible: !scanProgress.visible 
                }

                // Divider (Vertical line)
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 14
                    color: "#444444"
                    Layout.alignment: Qt.AlignVCenter
                }

                // View Mode Selector (Right)
                RowLayout {
                    spacing: 0
                    Layout.alignment: Qt.AlignVCenter
                    
                    Repeater {
                        model: ["List", "Tree", "Grouped"]
                        delegate: Rectangle {
                            width: 60
                            height: 18
                            color: (viewMode === index) ? "#444444" : "transparent"
                            border.color: "#555555"
                            border.width: 1
                            
                            // Connecting borders
                            anchors.leftMargin: index > 0 ? -1 : 0 
                            
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: (viewMode === index) ? "#ffffff" : "#888888"
                                font.pixelSize: 10
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: viewMode = index
                                hoverEnabled: true
                                onEntered: parent.color = (viewMode === index) ? "#555555" : "#333333"
                                onExited: parent.color = (viewMode === index) ? "#444444" : "transparent"
                            }
                        }
                    }
                }
                
                Item { Layout.preferredWidth: 5 } // Right margin
            }
        }
        }
    }
}


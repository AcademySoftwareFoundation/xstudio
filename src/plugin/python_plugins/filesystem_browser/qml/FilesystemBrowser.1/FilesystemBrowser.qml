import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQuick.Shapes 1.15    // Added for vector icon

import xStudio 1.0
import xstudio.qml.models 1.0
import xStudio 1.0
import xstudio.qml.models 1.0

Rectangle {
    id: root
    color: XsFileSystemStyle.backgroundColor
    anchors.fill: parent // Ensure it fills the panel



    // Access the attributes exposed by the plugin
    property string currentFilterTime: "Any"
    property string currentFilterVersion: "All Versions"

    XsModuleData {
        id: pluginData
        modelDataName: "Filesystem Browser" 
    }

    // Reusable Styled MenuItem component for consistent dark theme
    component StyledItem : MenuItem {
        id: inner
        contentItem: Text {
            text: inner.text
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
            color: inner.highlighted ? "#555555" : "transparent"
        }
    }

    // Reusable Context Menu for files in both Table and Icon views
    component FileContextMenu : Menu {
        property string itemPath: ""
        
        background: Rectangle {
            implicitWidth: 150
            implicitHeight: 100 // 4 items (25 each)
            color: XsFileSystemStyle.panelBgColor
            border.color: XsFileSystemStyle.borderColor
            radius: 3
        }

        StyledItem {
            text: "Replace"
            onTriggered: sendCommand({"action": "replace_current_media", "path": itemPath})
        }
        StyledItem {
            text: "Compare with"
            onTriggered: sendCommand({"action": "compare_with_current_media", "path": itemPath})
        }
        StyledItem {
            text: "Append to Playlist"
            onTriggered: sendCommand({"action": "append_media", "path": itemPath})
        }
        StyledItem {
            text: "Copy Path"
            onTriggered: sendCommand({"action": "copy_path", "path": itemPath})
        }
        StyledItem {
            text: "Show in Finder"
            onTriggered: sendCommand({"action": "reveal_in_finder", "path": itemPath})
        }
    }

    // State for Preview Mode
    property bool isPreviewMode: false
    property string pendingPreviewPath: ""

    Timer {
        id: previewTimer
        interval: 200 // Wait for double click
        repeat: false
        onTriggered: {
            if (pendingPreviewPath !== "") {
                isPreviewMode = true
                sendCommand({"action": "preview_file", "path": pendingPreviewPath})
                pendingPreviewPath = ""
            }
        }
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

    // Dedicated attribute for sending batch thumbnail requests to Python.
    // We write a JSON array of paths; Python queues them all into the ffmpeg worker pool.
    XsAttributeValue {
        id: thumbnail_request_attr
        attributeTitle: "thumbnail_request"
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
    onFileListChanged: buildTree()
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
    // Flat list of *file* items only — used for thumbnail request calculations.
    property var thumbnailFileList: []
    // Complete mixed flat model (all groups + files). Not assigned to Repeater directly.
    property var fullFlatModel: []
    // Paginated slice of fullFlatModel actually shown in the Repeater.
    property var flatThumbnailModel: []
    // How many items from fullFlatModel are currently rendered.
    property int thumbRenderCount: 0
    readonly property int thumbPageSize: 150   // initial page
    readonly property int thumbPageStep: 100   // items added per scroll-load
    onThumbnailFileListChanged: {
        if (root.viewMode === 3)
            Qt.callLater(requestVisibleThumbnails)
    }
    // Re-request thumbnails when the rendered page extends
    onFlatThumbnailModelChanged: {
        if (root.viewMode === 3)
            Qt.callLater(requestVisibleThumbnails)
    }

    // Only request thumbnails for items currently visible in the Flow.
    // Estimates Y positions mathematically from scroll position, cell size, and Flow width.
    function requestVisibleThumbnails() {
        if (root.viewMode !== 3 || flatThumbnailModel.length === 0) return

        var scrollY = thumbFlickable.contentY
        var viewH   = thumbFlickable.height
        var cellW   = 160
        var headerH = 24
        var cellH   = 160
        var cols    = Math.max(1, Math.floor(Math.max(1, thumbFlickable.width) / cellW))

        // One cell row above/below as prefetch buffer
        var topY    = Math.max(0, scrollY - cellH)
        var bottomY = scrollY + viewH + cellH

        var pending = []
        var y = 0
        var col = 0

        for (var i = 0; i < flatThumbnailModel.length; i++) {
            var item = flatThumbnailModel[i]
            if (item.type === "header") {
                if (col > 0) { y += cellH; col = 0 }
                y += headerH
            } else {
                if (col >= cols) { col = 0; y += cellH }
                if (y + cellH >= topY && y <= bottomY) {
                    if (!item.thumbnailSource) pending.push(item.path)
                }
                col++
            }
        }

        if (pending.length > 0) {
            console.log("QML: requesting " + pending.length + " visible thumbnails")
            thumbnail_request_attr.value = JSON.stringify(pending)
        }
    }

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
                
                // NEW Logic: Folder is visible if:
                // 1. It has visible children (e.g. media or subfolders matching search)
                // 2. OR if there is NO active text search/filter, we keep it visible to allow navigation.
                var filterText = filterField.text.trim();
                node.visible = childrenVisible || (filterText === ""); 
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

        // THUMBNAIL VIEW: files-only, grouped by compressed folder path
        if (viewMode === 3) {
            var thumbList = []
            for (var i = 0; i < fileList.length; i++) {
                var file = fileList[i]
                var isDir = (file.is_folder === true || file.type === "Folder")
                if (isDir) continue
                thumbList.push({
                    "name": file.name,
                    "path": file.path,
                    "isFolder": false,
                    "frames": file.frames || "",
                    "folderGroup": file.path.replace(/\/[^\/]+$/, ""),  // raw leaf dir
                    "thumbnailSource": file.thumbnailSource || "",
                    "data": file
                })
            }

            if (thumbList.length > 0) {
                var rootAbs = current_path_attr.value || ""

                // Fast O(N·depth) group compression using cumulative descendant counts.
                // For each file dir, walk UP until we find an ancestor with 2+ total
                // descendant files. That ancestor becomes the group header.

                // 1. Accumulate file counts up the tree
                var descCount = {}
                for (var i = 0; i < thumbList.length; i++) {
                    var cursor = thumbList[i].folderGroup
                    while (cursor.length > rootAbs.length) {
                        descCount[cursor] = (descCount[cursor] || 0) + 1
                        var sl = cursor.lastIndexOf("/")
                        cursor = sl > 0 ? cursor.substring(0, sl) : rootAbs
                    }
                    descCount[rootAbs] = (descCount[rootAbs] || 0) + 1
                }

                // 2. For each file, walk up from leaf to find lowest ancestor with >= 2 files
                //    (cache results to avoid redundant walks)
                var groupCache = {}
                for (var i = 0; i < thumbList.length; i++) {
                    var leaf = thumbList[i].folderGroup
                    if (groupCache[leaf] !== undefined) {
                        thumbList[i].folderGroup = groupCache[leaf]
                        continue
                    }
                    var d = leaf
                    while (d.length > rootAbs.length && (descCount[d] || 0) < 2) {
                        var sl = d.lastIndexOf("/")
                        d = sl > 0 ? d.substring(0, sl) : rootAbs
                    }
                    var grouped = (descCount[d] || 0) >= 2 ? d : rootAbs
                    groupCache[leaf] = grouped
                    thumbList[i].folderGroup = grouped
                }
            }

            // Sort by group then name
            thumbList.sort(function(a, b) {
                if (a.folderGroup < b.folderGroup) return -1
                if (a.folderGroup > b.folderGroup) return 1
                return a.name < b.name ? -1 : 1
            })
            thumbnailFileList = thumbList

            // Build complete flat mixed model
            var flat = []
            var prevGrp = null
            for (var j = 0; j < thumbList.length; j++) {
                var t = thumbList[j]
                if (t.folderGroup !== prevGrp) {
                    flat.push({ type: "header", path: t.folderGroup })
                    prevGrp = t.folderGroup
                }
                flat.push({ type: "file", name: t.name, path: t.path,
                            frames: t.frames, thumbnailSource: t.thumbnailSource || "", data: t.data })
            }

            // Paginate: only render the first page to avoid freezing on large dirs
            fullFlatModel = flat
            thumbRenderCount = Math.min(thumbPageSize, flat.length)
            flatThumbnailModel = flat.slice(0, thumbRenderCount)
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
    property real rowHeight: XsFileSystemStyle.rowHeight
    property color textColor: XsFileSystemStyle.textColor
    property color hintColor: XsFileSystemStyle.hintColor
    property real fontSize: XsFileSystemStyle.fontSize

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        
        handle: Rectangle {
            implicitWidth: 2
            color: SplitHandle.pressed ? XsFileSystemStyle.accentColor : (SplitHandle.hovered ? XsFileSystemStyle.hintColor : XsFileSystemStyle.dividerColor)
        }

        // Tree Container
        Item {
            SplitView.preferredWidth: showDirectoryTree ? 250 : 30
            SplitView.minimumWidth: showDirectoryTree ? 150 : 30
            SplitView.maximumWidth: showDirectoryTree ? 400 : 30

            // Collapsed State (Sidebar)
            Rectangle {
                anchors.fill: parent
                color: XsFileSystemStyle.pressedColor
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
                            color: XsFileSystemStyle.hintColor
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                        
                        onClicked: showDirectoryTree = true
                        
                        ToolTip.visible: hovered
                        ToolTip.text: "Expand Directory Tree"
                    }
                    
                    Item {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        
                        Text {
                            anchors.centerIn: parent
                            text: "DIRECTORY VIEW"
                            color: XsFileSystemStyle.pressedColor === "#1a1a1a" ? "#444444" : XsFileSystemStyle.secondaryTextColor
                            font.pixelSize: 10
                            font.bold: true
                            rotation: 90
                            width: parent.height
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // Tree with Header
            ColumnLayout {
                anchors.fill: parent
                visible: showDirectoryTree
                spacing: 0
                
                // Header
                Rectangle {
                    id: treeHeader
                    Layout.fillWidth: true
                    Layout.preferredHeight: XsFileSystemStyle.headerHeight
                    color: XsFileSystemStyle.headerBgColor
                    z: 10
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 5
                        
                        Text {
                            text: "Directories"
                            color: XsFileSystemStyle.textColor
                            font.bold: true
                            font.pixelSize: XsFileSystemStyle.fontSize
                            Layout.fillWidth: true
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
                                color: XsFileSystemStyle.hintColor
                                font.pixelSize: XsFileSystemStyle.fontSize + 4
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: showDirectoryTree = false
                        }
                    }
                    
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: XsFileSystemStyle.dividerColor
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
                    
                    property int autoScanThreshold: auto_scan_threshold_attr.value || 4
                }
            }
        }

        // Main Content Side
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
                color: textColor
                verticalAlignment: Text.AlignVCenter
            }

            TextField {
                id: pathField
                Layout.fillWidth: true
                Layout.preferredHeight: rowHeight
                text: current_path_attr.value || "/"
                color: textColor
                font.pixelSize: fontSize
                selectionColor: XsFileSystemStyle.selectionColor
                selectedTextColor: XsFileSystemStyle.backgroundColor
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
                
                background: Rectangle { 
                    color: XsFileSystemStyle.panelBgColor 
                    border.color: XsFileSystemStyle.borderColor
                    border.width: 1 
                }
                focus: true
                selectByMouse: true
                
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: pathContextMenu.popup()
                }

                Menu {
                    id: pathContextMenu
                    
                    background: Rectangle {
                        implicitWidth: 120
                        implicitHeight: 75 // 3 items
                        color: XsFileSystemStyle.panelBgColor
                        border.color: XsFileSystemStyle.borderColor
                        radius: 3
                    }
                    
                    delegate: StyledItem {} // Using the shared component from the top!

                    StyledItem {
                        text: "Copy"
                        onTriggered: pathField.copy()
                    }
                    StyledItem {
                        text: "Paste"
                        onTriggered: pathField.paste()
                    }
                    StyledItem {
                        text: "Clear"
                        onTriggered: pathField.clear()
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
                    background: Rectangle { color: XsFileSystemStyle.panelBgColor; border.color: XsFileSystemStyle.borderColor }
                    contentItem: ListView { 
                        id: completionListView
                        model: completionList
                        clip: true
                        highlight: Rectangle { color: XsFileSystemStyle.hoverColor }
                        highlightMoveDuration: 0
                        delegate: Item {
                            width: parent.width
                            height: 25
                            Rectangle { anchors.fill: parent; color: "transparent" }
                            Text { 
                                text: modelData
                                color: "#ffffff"
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
                id: refreshBtn
                Layout.preferredHeight: rowHeight
                Layout.preferredWidth: rowHeight
                text: "↻"
                font.pixelSize: 16
                flat: true
                onClicked: sendCommand({"action": "force_scan"})
                ToolTip.visible: hovered
                ToolTip.text: "Refresh directory scan"
                
                background: Rectangle {
                    color: parent.down ? XsFileSystemStyle.pressedColor : (parent.hovered ? XsFileSystemStyle.hoverColor : "transparent")
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
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
                    color: parent.down || pathPopup.opened ? XsFileSystemStyle.pressedColor : (parent.hovered ? XsFileSystemStyle.hoverColor : "transparent")
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
                        color: XsFileSystemStyle.headerBgColor
                        border.color: XsFileSystemStyle.borderColor
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        spacing: 0

                        // Header
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 25
                            color: XsFileSystemStyle.panelBgColor
                            Label { 
                                text: "QUICK ACCESS"
                                color: "#ffffff"
                                font.pixelSize: fontSize
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
                                color: mouseArea.containsMouse ? XsFileSystemStyle.hoverColor : "transparent"
                                
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
                                        color: "#ffffff"
                                        font.pixelSize: fontSize
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    
                                    // Path Hint (Right aligned, faded)
                                    Text {
                                        text: modelData.path
                                        color: "#ffffff"
                                        font.pixelSize: fontSize
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
                 ToolTip.visible: hovered
                 ToolTip.text: "Filter files by modification time"
                 
                 contentItem: Text {
                     text: filterTimeCombo.displayText
                     font.pixelSize: XsFileSystemStyle.fontSize
                     color: XsFileSystemStyle.textColor
                     verticalAlignment: Text.AlignVCenter
                     leftPadding: 10
                 }

                 background: Rectangle {
                     implicitWidth: 120
                     implicitHeight: rowHeight
                     color: XsFileSystemStyle.panelBgColor
                     border.color: XsFileSystemStyle.borderColor
                     radius: 2
                 }
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
                          color: XsFileSystemStyle.textColor
                          font.pixelSize: XsFileSystemStyle.fontSize
                          elide: Text.ElideRight
                          verticalAlignment: Text.AlignVCenter
                      }
                      background: Rectangle {
                          color: parent.highlighted ? XsFileSystemStyle.hoverColor : XsFileSystemStyle.backgroundColor
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
                          border.color: XsFileSystemStyle.borderColor
                          color: XsFileSystemStyle.backgroundColor
                      }
                  }
             }
             
             ComboBox {
                 id: filterVersionCombo
                 ToolTip.visible: hovered
                 ToolTip.text: "Filter files by version (e.g. v001, v002)"
                 
                 contentItem: Text {
                     text: filterVersionCombo.displayText
                     font.pixelSize: XsFileSystemStyle.fontSize
                     color: XsFileSystemStyle.textColor
                     verticalAlignment: Text.AlignVCenter
                     leftPadding: 10
                 }

                 background: Rectangle {
                     implicitWidth: 140
                     implicitHeight: rowHeight
                     color: XsFileSystemStyle.panelBgColor
                     border.color: XsFileSystemStyle.borderColor
                     radius: 2
                 }
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
                          color: XsFileSystemStyle.textColor
                          font.pixelSize: XsFileSystemStyle.fontSize
                          elide: Text.ElideRight
                          verticalAlignment: Text.AlignVCenter
                      }
                      background: Rectangle {
                          color: parent.highlighted ? XsFileSystemStyle.hoverColor : XsFileSystemStyle.backgroundColor
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
                          border.color: XsFileSystemStyle.borderColor
                          color: XsFileSystemStyle.backgroundColor
                      }
                  }
             }


              
            // Text Filter
            TextField {
                id: filterField
                Layout.fillWidth: true
                Layout.preferredHeight: rowHeight
                placeholderText: "Filter String..."
                placeholderTextColor: XsFileSystemStyle.hintColor
                color: XsFileSystemStyle.textColor
                font.pixelSize: XsFileSystemStyle.fontSize
                leftPadding: 5
                background: Rectangle { color: XsFileSystemStyle.panelBgColor; border.color: XsFileSystemStyle.borderColor }
                onTextEdited: refreshFiltering()
            }
        }



        // Table Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: rowHeight
            color: XsFileSystemStyle.headerBgColor
            
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
                            color: root.XsFileSystemStyle.textColor
                            font.pixelSize: root.XsFileSystemStyle.fontSize
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
            
            Rectangle { anchors.fill: parent; color: XsFileSystemStyle.backgroundColor }
            
            ListView {
                id: fileListView
                anchors.fill: parent
                anchors.rightMargin: 12
                visible: root.viewMode !== 3
                focus: visible
                onVisibleChanged: { if (visible) forceActiveFocus() }

                Keys.onLeftPressed: (event) => {
                    if (currentIndex > 0) currentIndex--
                    event.accepted = true
                }
                Keys.onRightPressed: (event) => {
                    if (currentIndex < count - 1) currentIndex++
                    event.accepted = true
                }
                Keys.onReturnPressed: (event) => _handleListReturn(event)
                Keys.onEnterPressed: (event) => _handleListReturn(event)

                function _handleListReturn(event) {
                    if (currentIndex >= 0 && currentIndex < count) {
                        var md = visibleTreeList[currentIndex]
                        if (md) {
                            previewTimer.stop()
                            if (md.isFolder) {
                                sendCommand({"action": "change_path", "path": md.path})
                            } else {
                                isPreviewMode = false
                                sendCommand({"action": "load_file", "path": md.path})
                            }
                        }
                    }
                    event.accepted = true
                }

                onCurrentIndexChanged: {
                    if (activeFocus && currentItem) {
                        if (!currentItem.isItemFolder) {
                            root.pendingPreviewPath = currentItem.itemPath
                            previewTimer.restart()
                        }
                    }
                }
                
                // Nothing found message
                Text {
                    anchors.centerIn: parent
                    text: "Nothing found"
                    color: XsFileSystemStyle.hintColor
                    font.pixelSize: XsFileSystemStyle.fontSize + 6
                    visible: fileListView.count === 0 && !searching_attr.value && !scan_required_attr.value
                }
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
                    property string itemPath: modelData.path
                    property bool isItemFolder: modelData.isFolder

                    Rectangle {
                        anchors.fill: parent
                        color: isSelected ? XsFileSystemStyle.selectionColor : (isHovered ? XsFileSystemStyle.hoverColor : (index % 2 == 0 ? XsFileSystemStyle.backgroundColor : XsFileSystemStyle.alternateBgColor))
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: isHovered = true
                        onExited: isHovered = false
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            fileListView.currentIndex = index
                            fileListView.forceActiveFocus()
                            if (mouse.button === Qt.RightButton) {
                                contextMenu.popup()
                            } else if (mouse.button === Qt.LeftButton) {
                                if (!modelData.isFolder) {
                                    root.pendingPreviewPath = modelData.path
                                    previewTimer.restart()
                                }
                            }
                        }
                        onDoubleClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton) {
                                previewTimer.stop()
                                fileListView.currentIndex = index
                                if (modelData.isFolder) {
                                    sendCommand({"action": "change_path", "path": modelData.path})
                                } else {
                                    isPreviewMode = false
                                    sendCommand({"action": "load_file", "path": modelData.path})
                                }
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
                            color: isSelected ? root.XsFileSystemStyle.textColor : root.XsFileSystemStyle.secondaryTextColor
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
                                text: (root.viewMode !== 0 && root.viewMode !== 3 && modelData.isFolder) ? (modelData.expanded ? "▼" : "▶") : ""
                                color: XsFileSystemStyle.hintColor
                                font.pixelSize: 10
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: toggleExpand(index)
                            }
                        }
                        
                        Cell { text: modelData.name || ""; Layout.fillWidth: true; Layout.minimumWidth: minWidthName; elideMode: Text.ElideMiddle }
                        Cell { text: (modelData.data && modelData.data.version) ? "v"+modelData.data.version : ""; w: colWidthVersion; color: isSelected ? root.XsFileSystemStyle.textColor : root.XsFileSystemStyle.secondaryTextColor }
                        Cell { text: (modelData.data && modelData.data.frames) || ""; w: colWidthFrames; color: isSelected ? root.XsFileSystemStyle.textColor : root.XsFileSystemStyle.secondaryTextColor }
                        Cell { text: (modelData.data && modelData.data.owner) || ""; w: colWidthOwner; color: isSelected ? root.XsFileSystemStyle.textColor : root.XsFileSystemStyle.secondaryTextColor }
                        Cell { text: modelData.data ? formatDate(modelData.data.date) : ""; w: colWidthDate; color: isSelected ? root.XsFileSystemStyle.textColor : root.XsFileSystemStyle.secondaryTextColor }
                        Cell { text: (modelData.data && modelData.data.size_str) || ""; w: colWidthSize; horizontalAlignment: Text.AlignRight; rightPadding: 5 }
                        Item { width: 20 } // Spacer at end
                    }
                    
                    FileContextMenu {
                        id: contextMenu
                        itemPath: modelData.path
                    }
                }
            }

            // Thumbnail view: Flickable + Flow for reliable scrolling with folder headers
            Flickable {
                id: thumbFlickable
                anchors.fill: parent
                visible: root.viewMode === 3
                clip: true
                contentWidth: width
                contentHeight: thumbFlow.implicitHeight
                flickableDirection: Flickable.VerticalFlick
                
                focus: visible
                onVisibleChanged: { if (visible) forceActiveFocus() }

                property int thumbCurrentIndex: -1

                Keys.onLeftPressed: (event) => {
                    var newIdx = thumbCurrentIndex
                    do {
                        if (newIdx > 0) newIdx--
                        else break
                    } while (flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header")
                    if (newIdx !== thumbCurrentIndex) {
                        thumbCurrentIndex = newIdx
                        _handleThumbKeyPreview()
                    }
                    event.accepted = true
                }
                Keys.onRightPressed: (event) => {
                    var newIdx = thumbCurrentIndex
                    var maxIdx = flatThumbnailModel.length - 1
                    do {
                        if (newIdx < maxIdx) newIdx++
                        else break
                    } while (flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header")
                    if (newIdx !== thumbCurrentIndex) {
                        thumbCurrentIndex = newIdx
                        _handleThumbKeyPreview()
                    }
                    event.accepted = true
                }
                Keys.onUpPressed: (event) => {
                    var cols = Math.max(1, Math.floor(thumbFlow.width / 160))
                    var newIdx = thumbCurrentIndex - cols
                    if (newIdx >= 0) {
                        while (newIdx > 0 && flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header") newIdx--
                        thumbCurrentIndex = newIdx
                        _handleThumbKeyPreview()
                    }
                    event.accepted = true
                }
                Keys.onDownPressed: (event) => {
                    var cols = Math.max(1, Math.floor(thumbFlow.width / 160))
                    var maxIdx = flatThumbnailModel.length - 1
                    var newIdx = thumbCurrentIndex + cols
                    if (newIdx <= maxIdx) {
                        while (newIdx < maxIdx && flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header") newIdx++
                        thumbCurrentIndex = newIdx
                        _handleThumbKeyPreview()
                    }
                    event.accepted = true
                }
                Keys.onReturnPressed: (event) => _handleThumbReturn(event)
                Keys.onEnterPressed: (event) => _handleThumbReturn(event)

                function _handleThumbReturn(event) {
                    if (thumbCurrentIndex >= 0 && thumbCurrentIndex < flatThumbnailModel.length) {
                        var md = flatThumbnailModel[thumbCurrentIndex]
                        if (md && md.type === "file") {
                            previewTimer.stop()
                            isPreviewMode = false
                            sendCommand({"action": "load_file", "path": md.path})
                        }
                    }
                    event.accepted = true
                }

                function _handleThumbKeyPreview() {
                    if (thumbCurrentIndex >= 0 && thumbCurrentIndex < flatThumbnailModel.length) {
                        var md = flatThumbnailModel[thumbCurrentIndex]
                        if (md && md.type === "file") {
                            root.pendingPreviewPath = md.path
                            previewTimer.restart()
                        }
                    }
                }

                onContentYChanged: {
                    // Extend the rendered page when the user scrolls near the bottom
                    var remaining = contentHeight - contentY - height
                    if (remaining < 600 && thumbRenderCount < fullFlatModel.length) {
                        thumbRenderCount = Math.min(thumbRenderCount + thumbPageStep, fullFlatModel.length)
                        flatThumbnailModel = fullFlatModel.slice(0, thumbRenderCount)
                    }
                    Qt.callLater(requestVisibleThumbnails)
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Flow {
                    id: thumbFlow
                    width: thumbFlickable.contentWidth
                    spacing: 0

                    Repeater {
                        model: flatThumbnailModel

                        delegate: Item {
                            id: flatDelegate
                            width:  modelData.type === "header" ? thumbFlow.width : 160
                            height: modelData.type === "header" ? 24 : 160

                            // ── Folder path header (spans full row) ────────────
                            Rectangle {
                                anchors.fill: parent
                                visible: modelData.type === "header"
                                color: "#1a1a1a"
                                Rectangle { width: 3; height: parent.height; color: "#4a9eff" }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left; anchors.leftMargin: 10
                                    text: modelData.type === "header" ? modelData.path : ""
                                    color: "#7aacce"; font.pixelSize: 11; font.bold: true
                                    elide: Text.ElideLeft
                                    width: parent.width - 16
                                }
                            }

                            // ── Thumbnail cell ──────────────────────────────────
                            property bool isSelected: (index === thumbFlickable.thumbCurrentIndex)
                            
                            Rectangle {
                                anchors.fill: parent; anchors.margins: 5
                                visible: modelData.type === "file"
                                color: (isSelected) ? "#555555" : (cellMouse.containsMouse ? "#333333" : "#2a2a2a")
                                radius: 4
                                border.color: (isSelected || cellMouse.containsMouse) ? "#777777" : "transparent"
                                border.width: isSelected ? 2 : (cellMouse.containsMouse ? 1 : 0)
                            }

                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 10
                                spacing: 4
                                visible: modelData.type === "file"

                                Item {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    BusyIndicator {
                                        anchors.centerIn: parent; width: 30; height: 30
                                        running: !modelData.thumbnailSource && modelData.type === "file"
                                        visible: running
                                    }
                                    Image {
                                        anchors.fill: parent
                                        source: modelData.thumbnailSource || ""
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        visible: !!modelData.thumbnailSource
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true; height: 32; clip: true
                                    property string rawName: modelData.name || ""
                                    property string ext: {
                                        var d = rawName.lastIndexOf(".")
                                        return d >= 0 ? rawName.slice(d + 1) : ""
                                    }
                                    property string stem: {
                                        var d = rawName.lastIndexOf(".")
                                        return d >= 0 ? rawName.slice(0, d) : rawName
                                    }
                                    property string baseName: stem.replace(/[#@%]+$/, "").replace(/\.$/, "")
                                    property string frameRange: modelData.frames || ""

                                    Text {
                                        anchors.top: parent.top
                                        anchors.left: parent.left; anchors.right: parent.right
                                        text: parent.baseName; color: "#e0e0e0"; font.pixelSize: 11
                                        horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle
                                    }
                                    Text {
                                        anchors.bottom: parent.bottom; anchors.left: parent.left
                                        text: parent.ext; color: "#888888"; font.pixelSize: 10
                                        visible: parent.ext !== ""
                                    }
                                    Text {
                                        anchors.bottom: parent.bottom; anchors.right: parent.right
                                        text: parent.frameRange; color: "#888888"; font.pixelSize: 10
                                        visible: parent.frameRange !== ""
                                    }
                                }
                            }

                            MouseArea {
                                id: cellMouse
                                anchors.fill: parent; hoverEnabled: true
                                visible: modelData.type === "file"
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: (mouse) => {
                                    if (mouse.button === Qt.LeftButton) {
                                        thumbFlickable.forceActiveFocus()
                                        thumbFlickable.thumbCurrentIndex = index
                                        root.pendingPreviewPath = modelData.path
                                        previewTimer.restart()
                                    } else if (mouse.button === Qt.RightButton) {
                                        thumbContextMenu.popup()
                                    }
                                }
                                onDoubleClicked: (mouse) => {
                                    if (mouse.button === Qt.LeftButton) {
                                        previewTimer.stop()
                                        isPreviewMode = false
                                        sendCommand({"action": "load_file", "path": modelData.path})
                                    }
                                }

                                FileContextMenu {
                                    id: thumbContextMenu
                                    itemPath: modelData.path
                                }
                                
                                ToolTip {
                                    delay: 500
                                    visible: cellMouse.containsMouse && modelData.type === "file"
                                    
                                    contentItem: Text {
                                        text: {
                                            if (modelData.type !== "file") return ""
                                            // parent directory path only
                                            var txt = modelData.path
                                            var sl = txt.lastIndexOf("/")
                                            if (sl >= 0) txt = txt.substring(0, sl)
                                            
                                            txt += "\n" + (modelData.name || "")
                                            if (modelData.frames) txt += "\nFrames: " + modelData.frames
                                            if (modelData.data && modelData.data.date) txt += "\nModified: " + formatDate(modelData.data.date)
                                            if (modelData.data && modelData.data.size_str) txt += "\nSize: " + modelData.data.size_str
                                            return txt
                                        }
                                        color: "#e0e0e0"
                                        font.pixelSize: 11
                                    }
                                    
                                    background: Rectangle {
                                        color: XsFileSystemStyle.panelBgColor
                                        radius: 3
                                        border.color: XsFileSystemStyle.borderColor
                                    }
                                }
                            }
                        } // delegate
                    } // Repeater
                } // Flow
            } // Flickable
            
            // Nothing found message
            Text {
                anchors.centerIn: parent
                text: "Nothing found"
                color: "#666666"
                font.pixelSize: 18
                visible: root.viewMode === 3 && flatThumbnailModel.length === 0 && !searching_attr.value && !scan_required_attr.value
            }

            // Manual Scan Overlay
            Rectangle {
                anchors.centerIn: parent
                width: 200
                height: 100
                color: "#333333"
                visible: root.viewMode === 3 && scan_required_attr.value === true
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

            ScrollBar {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                active: true
                // The ScrollView (thumbnail mode) has its own built-in scrollbar
                visible: root.viewMode !== 3
                policy: ScrollBar.AsNeeded
                size: fileListView.visibleArea.heightRatio
                position: fileListView.visibleArea.yPosition
                onPositionChanged: if(pressed) {
                    fileListView.contentY = position * fileListView.contentHeight
                }
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

                // Preview Indicator
                Rectangle {
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignVCenter
                    color: "transparent"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "Preview"
                        color: isPreviewMode ? "#66ff66" : "#444444"
                        font.pixelSize: 10
                        font.bold: isPreviewMode
                    }
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
                        model: ["List", "Tree", "Grouped", "Thumbnails"]
                        delegate: Rectangle {
                            width: 60
                            height: 18
                            color: (viewMode === index) ? "#444444" : "transparent"
                            border.color: XsFileSystemStyle.borderColor
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

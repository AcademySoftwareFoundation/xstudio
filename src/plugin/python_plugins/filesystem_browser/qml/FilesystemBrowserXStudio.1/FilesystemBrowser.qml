import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQuick.Shapes 1.15    // Added for vector icon

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

import "./styled_widgets"
//import "XsText.qml" as Text

Item {

    id: root

    anchors.fill: parent // Ensure it fills the panel

    property var columnPositions: [0, 200, 280, 360, 460, 580] // Initial positions based on headerModel sizes
    property var columnWidths: [200, 80, 80, 100, 120, 80] // Initial widths based on headerModel sizes

    // Access the attributes exposed by the plugin
    property string currentFilterTime: "Any"
    property string currentFilterVersion: "All Versions"

    property var selectedItems: []
    property var selectedItemsUrls: []
    property var selectedItemsPaths: []
    property var underMouseIndex: -1
    property var dragVisItem: undefined

    onSelectedItemsChanged: {        
        var v = []
        var v2 = []
        if (root.viewMode === 3) {
            for (var i = 0; i < selectedItems.length; i++) {
                v.push(helpers.QUrlFromPosixPath(flatThumbnailModel[selectedItems[i]].path))
                v2.push(flatThumbnailModel[selectedItems[i]].path)
            }
        } else {
            for (var i = 0; i < selectedItems.length; i++) {
                v.push(helpers.QUrlFromPosixPath(visibleTreeList[selectedItems[i]].path))
                v2.push(visibleTreeList[selectedItems[i]].path)
            }
        }
        selectedItemsUrls = v
        selectedItemsPaths = v2
    }

    function selectItem(index, selectionMode) {

        if (selectionMode === 1) { // Shift

            if (selectedItems.includes(index)) return;
            let nearest = -1
            for (let i = 0; i < selectedItems.length; i++) {
                if (nearest === -1 || Math.abs(selectedItems[i] - index) < Math.abs(nearest - index)) {
                    nearest = selectedItems[i]
                }
            }
            if (nearest !== -1) {
                let start = Math.min(nearest, index)
                let end = Math.max(nearest, index)
                let v = selectedItems
                for (let i = start; i <= end; i++) {
                    if (!selectedItems.includes(i)) v.push(i)
                }
                selectedItems = v
            }

        } else if (selectionMode === 2) { // Ctrl

            if (selectedItems.includes(index)) {
                let v = selectedItems.filter(i => i !== index)
                selectedItems = v
            } else {
                let v = selectedItems                    
                v.push(index)
                selectedItems = v
            }

        } else {

            if (!selectedItems.includes(index)) {
                selectedItems = [index]
            }
            if (selectedItems.length == 1) {
                var item = viewMode === 3 ? flatThumbnailModel[index] : visibleTreeList[index]
                if (!item.data.is_folder) {
                    pendingPreviewPath = item.path
                    previewTimer.restart()
                }
            }
        }

    }

    XsDragDropHandler {

        id: drag_drop_handler
        dragSourceName: "External URIS"
        dragData: root.selectedItemsUrls

    }

    XsGradientRectangle{
        anchors.fill: parent
    }

    XsModuleData {
        id: pluginData
        modelDataName: "Filesystem Browser" 
    }

    XsModuleData {
        id: filterTermAttrs
        modelDataName: "Filter Terms" 
    }

    // State for Preview Mode
    property bool isPreviewMode: false
    property string pendingPreviewPath: ""
    property bool haveSubFolders: false

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

    XsAttributeValue {
        id: __deepScan
        attributeTitle: "is_deepscan"
        model: pluginData
    }
    property alias deepScan: __deepScan.value
    
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
                //console.log("history_attr: Parse Error: " + e)
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
                //console.log("pinned_attr: Parse Error: " + e)
                pinnedList = []
            }
        }
        
        onValueChanged: updateList()
        Component.onCompleted: updateList()
    }
    
    property var historyList: []
    property var pinnedList: []
    property var combinedList: []
    property var filterField: ""

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
        function updateList() {
            if (typeof(value) === "string") {
                fileList = JSON.parse(value)
            }
        }
        
        onValueChanged: updateList()
        Component.onCompleted: updateList()
    }
    property var fileList: []

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
        attributeTitle: "Mod. Time"
        model: pluginData
        role: "value" 
        onValueChanged: {
            currentFilterTime = value || "Any"
            refreshFiltering()
        }
        Component.onCompleted: {
             currentFilterTime = value || "Any"
        }
    }
    XsAttributeValue { 
        id: filter_version_attr
        attributeTitle: "Version"
        model: pluginData
        role: "value"
        onValueChanged: {
             currentFilterVersion = value || "All Versions"
             refreshFiltering()
        }
        Component.onCompleted: {
             currentFilterVersion = value || "All Versions"
        }
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

    XsAttributeValue {
        id: __view_mode
        attributeTitle: "view_mode"
        model: pluginData
    }
    property alias viewMode: __view_mode.value
    // View Mode: 0=List, 1=Tree, 2=Grouped
    onViewModeChanged: buildTree()

    property var scannedDirsList: []

    function sendCommand(cmd) {
        command_attr.value = JSON.stringify(cmd)
    }

    // Local property to hold the parsed JSON file list
    onFileListChanged: {
        buildTree()
    }
    property var completionList: []
    
    // Sorting State
    property string sortColumn: "name"
    property int sortOrder: 1 // 1 for asc, -1 for desc
    
    // tree logic
    property var treeRoots: []
    property var visibleTreeList: []
    property var collapsedPaths: ({})
    // Complete mixed flat model (all groups + files). Not assigned to Repeater directly.
    property var flatThumbnailModel: []

    function isVisible(data) {
        if (!data) return true;
        
        // Text Filter
        var filterText = filterField.trim();
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
            if (data.is_latest_version === false) return false;
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
                var filterText = filterField.trim();
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

    function buildThumbsTreeCompressed() {
        // THUMBNAIL VIEW: files-only, grouped by compressed folder path

        var subfolders = false
        var thumbList = []
        for (var i = 0; i < fileList.length; i++) {
            var file = fileList[i]
            var isDir = (file.is_folder === true || file.type === "Folder")
            if (isDir && subfolders) continue
            else if (isDir && !subfolders) {
                subfolders = true
                continue
            }
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

        haveSubFolders = subfolders

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
        flatThumbnailModel = flat
    }

    function buildThumbsTree() {

        var thumbList = []
        for (var i = 0; i < fileList.length; i++) {
            var file = fileList[i]
            var isDir = (file.is_folder === true || file.type === "Folder")
            thumbList.push({
                "name": file.name,
                "path": file.path,
                "isFolder": isDir,
                "frames": file.frames || "",
                "folderGroup": isDir ? file.path : file.path.replace(/\/[^\/]+$/, ""),  // raw leaf dir
                "thumbnailSource": file.thumbnailSource || "",
                "data": file
            })
        }

        // Sort by group then name
        thumbList.sort(function(a, b) {
            if (a.folderGroup < b.folderGroup) return -1
            if (a.folderGroup > b.folderGroup) return 1
            if (a.isFolder && !b.isFolder) return -1
            if (!a.isFolder && b.isFolder) return 1
            return a.name < b.name ? -1 : 1
        })

        // Build complete flat mixed model
        var flat = []
        for (var j = 0; j < thumbList.length; j++) {
            var t = thumbList[j]
            flat.push({ type: t.isFolder ? "header": "file", name: t.name, path: t.path,
                        frames: t.frames, thumbnailSource: t.thumbnailSource || "", data: t.data })
        }

        flatThumbnailModel = flat
    }

    function buildTree() {

        var roots = []
        if (viewMode === 0) {

            var subfolders = false
            // LIST VIEW: Flat list, no hierarchy logic
            for(var i=0; i<fileList.length; i++) {
                var file = fileList[i]
                
                // User requested to hide directories in List view
                var isDir = (file.is_folder === true || file.type === "Folder")
                if (isDir && subfolders) continue                
                else if (isDir) {
                    subfolders = true
                    continue;
                }
                let v = file.relpath.split("/")
                let name = v.length > 0 ? v[v.length-1] : file.name
                var node = {
                    "name": name,
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
            haveSubFolders = subfolders
            return
        }

        // THUMBNAIL VIEW: files AND folders. Files are sorted *after* the folder
        // that contains them.
        if (viewMode === 3) {
            buildThumbsTreeCompressed()
            /*if (deepScan) buildThumbsTreeCompressed()
            else buildThumbsTree()*/
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
        var subfolders = false
        function traverse(nodes, depth) {
            for(var i=0; i<nodes.length; i++) {
                var node = nodes[i]
                if (!subfolders && node.type!="file") {
                    subfolders = true
                }
                // we only show sub-dirs when a deep scan has been executed
                if (node.visible && (deepScan || node.type=="file" || node.data.type=="File" || node.data.type=="Sequence")) {
                    node.depth = depth 
                    visible.push(node)
                    if (node.isFolder && node.expanded) {
                        traverse(node.children, depth + 1)
                    }
                }
            }
        }
        traverse(treeRoots, 0);
        haveSubFolders = subfolders
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

    SplitView {

        anchors.fill: parent
        anchors.margins: XsStyleSheet.panelPadding

        orientation: Qt.Horizontal
        
        handle: Item {
            implicitWidth: 9
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: SplitHandle.hovered ? 3 : 1
                height: parent.height
                color: SplitHandle.pressed ? XsStyleSheet.accentColor : (SplitHandle.hovered ? XsStyleSheet.primaryTextColor : "black")
            }
        }

        FSDirectoryTree {
            id: directoryTree
            Layout.preferredWidth: showDirectoryTree ? 450 : 20
            Layout.fillHeight: true
        }

        // Main Content Side
        ColumnLayout {

            SplitView.fillWidth: true
            spacing: 1

            // Bottom Footer: Progress + View Modes
            FSMainHeader {
                Layout.fillWidth: true
                Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
            }

            // Path Input Row
            FSPathInput {
                Layout.fillWidth: true
                Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
            }
    
            ListModel {
                id: headerModel
                ListElement { title: "Name"; colId: "name"; size: 200; resizable: false; position: 0 }
                ListElement { title: "Version"; colId: "version"; size: 80; resizable: true; position: 1 }
                ListElement { title: "Frames"; colId: "frames"; size: 80; resizable: true; position: 2 }
                ListElement { title: "Owner"; colId: "owner"; size: 100; resizable: true; position: 3 }
                ListElement { title: "Date"; colId: "date"; size: 120; resizable: true; position: 4 }
                ListElement { title: "Size"; colId: "size_str"; size: 80; resizable: true; position: 5 }
            }

            // File List
            Item {

                Layout.fillWidth: true
                Layout.fillHeight: true
                
                property var mouse: Qt.point(mouseArea.mouseX, mouseArea.mouseY)

                Rectangle {
                    anchors.fill: parent
                    color: XsStyleSheet.panelBgColor
                    z: -1000
                }

                ColumnLayout {

                    anchors.fill: parent
                    spacing: 0
                    visible: root.viewMode !== 3

                    FSListHeader {
                        Layout.fillWidth: true
                        columns_root_model: headerModel
                        property alias columnPositions: root.columnPositions
                        property alias columnWidths: root.columnWidths
                    }

                    FSListView {
                        id: fileListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        mousePosition: Qt.point(mouseArea.mouseX, mouseArea.mouseY)
                    }
                }
                
                FSThumbView {
                    anchors.fill: parent
                    visible: root.viewMode === 3
                    id: thumbFlickable
                    mousePosition: Qt.point(mouseArea.mouseX, mouseArea.mouseY)
                }

                MouseArea {
                                
                    id: mouseArea
                    anchors.fill: parent
                    anchors.rightMargin: 20 // Avoid scrollbar area
                    propagateComposedEvents: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                
                    onPositionChanged: (mouse)=> {
                        if (pressed && mouse.buttons == Qt.LeftButton) {
                            var pt = mapToItem(root, mouse.x, mouse.y)
                            drag_drop_handler.doDrag(pt.x, pt.y)
                        }
                    }
                
                    onReleased: (mouse)=> {
                        if (underMouseIndex != -1 && mouse.button === Qt.LeftButton && mouse.modifiers == Qt.NoModifier && !drag_drop_handler.dragging) {    
                            selectedItems = []
                            selectItem(underMouseIndex, 0)
                        } else {
                            var pt = mapToItem(root, mouse.x, mouse.y)
                            drag_drop_handler.endDrag(pt.x, pt.y)
                        }
                    }

                    onPressed: (mouse) => {
                        if (underMouseIndex != -1) {
                            dragVisItem = fileListView.visible ? fileListView.itemAtIndex(underMouseIndex) : thumbFlickable.thumbs.itemAt(underMouseIndex)
                        }
                        drag_drop_handler.startDrag(mouseX, mouseY, dragVisItem)
                        if (underMouseIndex != -1 && mouse.button === Qt.LeftButton) {
                            selectItem(underMouseIndex, mouse.modifiers == Qt.ShiftModifier ? 1 : mouse.modifiers == Qt.ControlModifier ? 2 : 0)
                        }
                    }

                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton && underMouseIndex != -1) {
                            previewTimer.stop()
                            isPreviewMode = false
                            let clickedItemPath = root.viewMode === 3 ? flatThumbnailModel[underMouseIndex].path : visibleTreeList[underMouseIndex].path
                            sendCommand({"action": "load_file", "path": clickedItemPath})
                        }
                    }

                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton && underMouseIndex != -1) {
                            let clickedItemPath = root.viewMode === 3 ? flatThumbnailModel[underMouseIndex].path : visibleTreeList[underMouseIndex].path
                            thumbContextMenu.selectedPaths = root.selectedItemsPaths
                            thumbContextMenu.itemPath = clickedItemPath
                            thumbContextMenu.showMenu(
                                mouseArea,
                                mouse.x, mouse.y);
                        }
                    }
                
                    FSFileContextMenu {
                        id: thumbContextMenu
                    }
                        
                    
                }

                // Nothing found message
                ColumnLayout {

                    anchors.centerIn: parent
                    spacing: 20
                    XsText {
                        id: msg
                        visible: root.viewMode === 3 ? flatThumbnailModel.length === 0 : visibleTreeList.length === 0
                        text: "No media found in " + current_path_attr.value + (haveSubFolders ? deepScan ? " or its subfolders" : ", but subfolders yet to be scanned." : "")
                        color: "#666666"
                        font.pixelSize: 18
                    }
                    XsPrimaryButton {
                        Layout.preferredWidth: 200
                        Layout.alignment: Qt.AlignHCenter
                        visible: msg.visible && haveSubFolders && !deepScan
                        text: "Run Full Scan"
                        onClicked: sendCommand({"action": "force_scan"})
                        textDiv.font.pixelSize: 16
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
                    delegate: XsText {
                        text: modelData
                        color: "#888888"
                        font.pixelSize: 10
                        width: ListView.view.width
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignLeft
                    }
                    
                    // Auto-scroll to bottom
                    onCountChanged: {
                        positionViewAtEnd()
                    }
                }
            }

        }
    }

    property alias thumbToolTip: thumbToolTip
    XsToolTip {
        
        id: thumbToolTip
        delay: 500
        maxWidth: 400
    }

}

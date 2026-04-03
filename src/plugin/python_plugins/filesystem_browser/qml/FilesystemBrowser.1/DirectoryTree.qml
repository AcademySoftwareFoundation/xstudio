import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import xStudio 1.0

Rectangle {
    id: treeRoot
    color: XsFileSystemStyle.backgroundColor


    
    // Properties to communicate with parent
    property var pluginData: null
    property var currentPath: "/"
    property string baseRootPath: "/"
    
    signal sendCommand(var cmd)
    onSendCommand: (cmd) => console.log("DirectoryTree: Sending command: " + JSON.stringify(cmd))
    
    // Style constants to match FilesystemBrowser
    property real rowHeight: XsFileSystemStyle.rowHeight
    property color textColor: XsFileSystemStyle.textColor
    property color hintColor: XsFileSystemStyle.hintColor
    property real fontSize: XsFileSystemStyle.fontSize
    property color selectionColor: XsFileSystemStyle.selectionColor
    property color hoverColor: XsFileSystemStyle.hoverColor
    property color backgroundColor: XsFileSystemStyle.backgroundColor
    
    // Auto-expand logic
    property string pendingExpandPath: ""
    property bool isSyncing: false
    property int autoScanThreshold: 4

    function getPathDepth(p) {
        if (!p || p === "/") return 0;
        var parts = p.split("/");
        var count = 0;
        for(var i=0; i<parts.length; i++) {
            if (parts[i]) count++;
        }
        return count;
    }

    onCurrentPathChanged: {
        // Start sync process
        if (currentPath && currentPath !== "/") {
            pendingExpandPath = currentPath;
            isSyncing = true;
            syncToPath();
        }
    }

    function syncToPath() {
        if (!pendingExpandPath) return;
        
        // Find deepest matching node in current model
        var deepestIndex = -1;
        var deepestLen = 0;
        
        for(var i=0; i<treeModel.count; i++) {
            var node = treeModel.get(i);
            var np = node.path;
            
            // Check if this node is part of the path
            // e.g. node="/Users", pending="/Users/sam"
            // We need to handle trailing slashes carefully
            
            var match = false;
            if (pendingExpandPath === np) match = true;
            else if (pendingExpandPath.indexOf(np + "/") === 0) match = true;
            else if (np === "/" && pendingExpandPath.indexOf("/") === 0) match = true; // Root always matches
            
            if (match) {
                if (np.length > deepestLen || (np === "/" && deepestLen === 0)) {
                    deepestLen = np.length;
                    deepestIndex = i;
                }
            }
        }
        
        if (deepestIndex !== -1) {
            var node = treeModel.get(deepestIndex);
            
            if (node.path === pendingExpandPath) {
                // We reached the target!
                treeView.currentIndex = deepestIndex;
                pendingExpandPath = "";
                isSyncing = false;
                // Ensure visible
                treeView.positionViewAtIndex(deepestIndex, ListView.Center);
                // Also expand to show children as requested
                if (!node.expanded) expandNode(deepestIndex);
            } else {
                // We need to go deeper. Expand this node if not expanded.
                if (!node.expanded) {
                    expandNode(deepestIndex);
                    // wait for handleQueryResult to call us back
                } else {
                    // It is expanded, but maybe children are not loaded yet?
                    // Or maybe we just expanded it and are waiting?
                    if (node.isLoading) {
                        // Waiting
                    } else {
                        // Children present but we didn't find a better match?
                        // This implies the next segment of path doesn't exist in the tree.
                        // Stop here.
                        pendingExpandPath = "";
                        isSyncing = false;
                    }
                }
            }
        } else {
            // Should not happen if Root is present
            isSyncing = false;
        }
    }
    
    // Attribute for directory query results
    XsAttributeValue {
        id: dir_query_attr
        attributeTitle: "directory_query_result"
        model: pluginData
        role: "value"
        
        onValueChanged: {
            try {
                var val = value;
                if (val && val !== "{}") {
                    var result = JSON.parse(val);
                    handleQueryResult(result);
                }
            } catch(e) {
                console.log("DirectoryTree: Query result parse error: " + e);
            }
        }
    }
    
    // Tree Model
    // We'll use a ListModel and manually manage hierarchical indentation
    ListModel {
        id: treeModel
    }
    
    onBaseRootPathChanged: {
        treeModel.clear();
        var rootName = baseRootPath === "/" ? "Root" : (baseRootPath.split("/").pop() || baseRootPath);
        treeModel.append({
            "name": rootName,
            "path": baseRootPath,
            "level": 0,
            "expanded": false,
            "hasChildren": true,
            "isLoading": false
        });
        expandNode(0);
        
        if (currentPath && currentPath.indexOf(baseRootPath) === 0 && currentPath !== baseRootPath) {
             pendingExpandPath = currentPath;
             isSyncing = true;
             syncToPath();
        }
    }

    Component.onCompleted: {
        // Init with root
        var rootName = baseRootPath === "/" ? "Root" : (baseRootPath.split("/").pop() || baseRootPath);
        treeModel.append({
            "name": rootName,
            "path": baseRootPath,
            "level": 0,
            "expanded": false,
            "hasChildren": true, // Assume root has children
            "isLoading": false
        });
        // Immediately expand root
        expandNode(0);
        
        if (currentPath && currentPath.indexOf(baseRootPath) === 0 && currentPath !== baseRootPath) {
             pendingExpandPath = currentPath;
             isSyncing = true;
             syncToPath();
        }
    }
    
    function expandNode(index) {
        var node = treeModel.get(index);
        
        // If already expanded and not loading, we might still need to load if it has no children but should
        // However, for now let's just allow re-requesting if isLoading is false or if explicitly called when collapsed
        if (node.expanded && !node.isLoading) {
             // Check if children already exist
             var nextIndex = index + 1;
             if (nextIndex < treeModel.count) {
                 var next = treeModel.get(nextIndex);
                 if (next.level > node.level) {
                     return;
                 }
             }
             // No children? Trigger load anyway
        } else if (node.expanded) {
            return;
        }
        
        treeModel.setProperty(index, "expanded", true);
        
        if (node.isLoading) {
            return;
        }
        
        treeModel.setProperty(index, "isLoading", true);
        
        // Request subdirs
        sendCommand({"action": "get_subdirs", "path": node.path});
    }
    
    function collapseNode(index) {
        var node = treeModel.get(index);
        
        treeModel.setProperty(index, "expanded", false);
        treeModel.setProperty(index, "isLoading", false); // Important: stop loading if collapsed
        
        // Remove children from model
        // We need to remove all items following this node that have a level > node.level
        // AND stop when we hit a node with level <= node.level
        var currentLevel = node.level;
        var i = index + 1;
        var count = 0;
        
        while (i < treeModel.count) {
            var child = treeModel.get(i);
            if (child.level > currentLevel) {
                count++;
                i++;
            } else {
                break;
            }
        }
        
        if (count > 0) {
            treeModel.remove(index + 1, count);
        }
    }
    
    function handleQueryResult(result) {
        // Find which node requested this?
        // We scan the model to find the node with matching path and isLoading=true
        // Or just matching path and expanded=true but maybe no children yet?
        
        var path = result.path;
        var dirs = result.dirs;
        
        var foundIndex = -1;
        for(var i=0; i<treeModel.count; i++) {
            var node = treeModel.get(i);
            if (node.path === path && node.expanded) {
                foundIndex = i;
                node.isLoading = false;
                break; // Assuming unique paths in view? Not necessarily if symlinks, but okay for now.
            }
        }
        
        if (foundIndex !== -1) {
            
            // Check if next item is already a child
            var nextIndex = foundIndex + 1;
            var parentLevel = treeModel.get(foundIndex).level;
            
            if (nextIndex < treeModel.count) {
                var next = treeModel.get(nextIndex);
                if (next.level > parentLevel) {
                     collapseNode(foundIndex); 
                     treeModel.setProperty(foundIndex, "expanded", true); 
                }
            }
            
            // Insert children
            for(var j=0; j<dirs.length; j++) {
                var d = dirs[j];
                treeModel.insert(foundIndex + 1 + j, {
                    "name": d.name,
                    "path": d.path,
                    "level": parentLevel + 1,
                    "expanded": false,
                    "hasChildren": true, 
                    "isLoading": false
                });
            }
            
            // If no children, mark as leaf
            if (dirs.length === 0) {
                 treeModel.setProperty(foundIndex, "hasChildren", false);
            } else {
                 treeModel.setProperty(foundIndex, "hasChildren", true);
            }
            
            treeModel.setProperty(foundIndex, "isLoading", false);

            // Continue sync if active
            if (isSyncing) {
                syncToPath();
            }
        } else {
            console.log("DirectoryTree: Target node for result not found or not expanded: " + path);
            // Search for node to reset isLoading anyway
            for(var k=0; k<treeModel.count; k++) {
                if (treeModel.get(k).path === path) {
                    treeModel.setProperty(k, "isLoading", false);
                    break;
                }
            }
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Header for custom root
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? XsFileSystemStyle.headerHeight : 0
            color: XsFileSystemStyle.panelBgColor
            visible: treeRoot.baseRootPath !== "/"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 5
                spacing: 5
                
                Text {
                    text: "Directory root: " + treeRoot.baseRootPath
                    color: XsFileSystemStyle.hintColor
                    font.pixelSize: 10
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    
                    MouseArea {
                        id: rootHoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                    ToolTip.visible: rootHoverArea.containsMouse
                    ToolTip.text: treeRoot.baseRootPath
                    ToolTip.delay: 500
                }
                
                Button {
                    text: "×"
                    Layout.preferredHeight: 16
                    Layout.preferredWidth: 16
                    flat: true
                    padding: 0
                    
                    background: Rectangle {
                        color: parent.down ? "#444444" : "transparent"
                        radius: 2
                    }
                    
                    contentItem: Text {
                        text: "×"
                        color: XsFileSystemStyle.hintColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                    }
                    
                    onClicked: treeRoot.baseRootPath = "/"
                }
            }
        }
        
        ListView {
            id: treeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: treeModel
            
            delegate: Rectangle {
                id: rowDelegate
                width: ListView.view.width
                height: treeRoot.rowHeight
                color: (model.path === treeRoot.currentPath) ? treeRoot.selectionColor : ((msgMouse.containsMouse || scanMouse.containsMouse) ? XsFileSystemStyle.hoverColor : "transparent")
                
                // Row Selection MouseArea (Background)
                MouseArea {
                    id: msgMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    
                    onClicked: (mouse) => {
                         if (mouse.button === Qt.LeftButton) {
                             sendCommand({"action": "change_path", "path": model.path});
                         } else if (mouse.button === Qt.RightButton) {
                             treeContextMenu.popup();
                         }
                    }
                }

                Menu {
                    id: treeContextMenu
                    background: Rectangle {
                        implicitWidth: 150
                        implicitHeight: 75
                        color: XsFileSystemStyle.panelBgColor
                        border.color: XsFileSystemStyle.borderColor
                        radius: 3
                    }
                    
                    MenuItem {
                        id: setRootItem
                        text: "Set as Root"
                        onTriggered: {
                            treeRoot.baseRootPath = model.path;
                        }
                        
                        contentItem: Text {
                            text: setRootItem.text
                            color: "#e0e0e0"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                        }
                        
                        background: Rectangle {
                            implicitWidth: 150
                            implicitHeight: 25
                            color: setRootItem.highlighted ? "#555555" : "transparent"
                        }
                    }
                    
                    MenuItem {
                        id: revealItem
                        text: "Show in Finder"
                        onTriggered: {
                            // Using the same action name as the file context menu
                            treeRoot.sendCommand({"action": "reveal_in_finder", "path": model.path})
                        }
                        
                        contentItem: Text {
                            text: revealItem.text
                            color: "#e0e0e0"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                        }
                        
                        background: Rectangle {
                            implicitWidth: 150
                            implicitHeight: 25
                            color: revealItem.highlighted ? "#555555" : "transparent"
                        }
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    
                    // Indentation
                    Item {
                        Layout.preferredWidth: model.level * 20 + 5 
                        Layout.fillHeight: true
                    }
                    
                    // Expander Arrow
                    Item {
                        Layout.preferredWidth: 20
                        Layout.fillHeight: true
                        
                        Text {
                            anchors.centerIn: parent
                            text: model.hasChildren ? (model.expanded ? "▼" : "▶") : ""
                            color: treeRoot.hintColor
                            font.pixelSize: 10
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (model.expanded) {
                                    collapseNode(index);
                                } else {
                                    expandNode(index);
                                }
                            }
                        }
                    }
                    
                    // Folder Icon
                    Item {
                         Layout.preferredWidth: 20
                         Layout.fillHeight: true
                         Text {
                             anchors.centerIn: parent
                             text: "📁"
                             font.pixelSize: treeRoot.fontSize
                         }
                    }
                    
                    // Name
                    Text {
                        text: model.name
                        color: treeRoot.textColor
                        font.pixelSize: treeRoot.fontSize
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        leftPadding: 5
                    }

                    // Scan Button Container (Prevents layout jitter by taking space only if scan is possible)
                    Item {
                        Layout.preferredWidth: 56
                        Layout.fillHeight: true
                        visible: treeRoot.getPathDepth(model.path) <= treeRoot.autoScanThreshold && !model.isLoading

                        Rectangle {
                            anchors.centerIn: parent
                            visible: msgMouse.containsMouse || scanMouse.containsMouse
                            width: 46
                            height: 18
                            color: scanMouse.containsMouse ? XsFileSystemStyle.pressedColor : XsFileSystemStyle.panelBgColor
                            radius: 4
                            border.color: XsFileSystemStyle.borderColor
                            border.width: 1
                            
                            Text {
                                anchors.centerIn: parent
                                text: "SCAN"
                                color: XsFileSystemStyle.textColor
                                font.pixelSize: 8
                                font.bold: true
                            }
                            
                            MouseArea {
                                id: scanMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    // Set path AND trigger scan in one atomic action
                                    sendCommand({"action": "force_scan", "path": model.path})
                                }
                            }
                            
                            ToolTip.visible: scanMouse.containsMouse
                            ToolTip.text: "Force media scan in this folder"
                            ToolTip.delay: 500
                        }
                    }
                    
                    // Loading Indicator
                    Text {
                        text: "..."
                        color: treeRoot.hintColor
                        visible: model.isLoading
                        Layout.rightMargin: 5
                    }
                }
                

            }
            
            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
                width: 10
                background: Rectangle { color: XsFileSystemStyle.backgroundColor }
                contentItem: Rectangle {
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: 3
                    color: treeView.active ? XsFileSystemStyle.hintColor : XsFileSystemStyle.secondaryTextColor
                }
            }
        }
    }
}

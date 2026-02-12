import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import xStudio 1.0

Rectangle {
    id: treeRoot
    color: "#222222"
    
    // Properties to communicate with parent
    property var pluginData: null
    property var currentPath: "/"
    
    signal sendCommand(var cmd)
    onSendCommand: (cmd) => console.log("DirectoryTree: Sending command: " + JSON.stringify(cmd))
    
    // Style constants to match FilesystemBrowser
    property real rowHeight: 30
    property color textColor: "#e0e0e0"
    property color hintColor: "#aaaaaa"
    property real fontSize: 12
    property color selectionColor: "#555555"
    property color hoverColor: "#333333"
    property color backgroundColor: "#222222"
    
    // Auto-expand logic
    property string pendingExpandPath: ""
    property bool isSyncing: false

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
            console.log("DirectoryTree: dir_query_attr changed. Value length: " + (value ? value.length : "null"));
            try {
                var val = value;
                if (val && val !== "{}") {
                    var result = JSON.parse(val);
                    console.log("DirectoryTree: Parsed result for path: " + result.path + ", dirs: " + (result.dirs ? result.dirs.length : "0"));
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
    
    Component.onCompleted: {
        // Init with root
        treeModel.append({
            "name": "Root",
            "path": "/",
            "level": 0,
            "expanded": false,
            "hasChildren": true, // Assume root has children
            "isLoading": false
        });
        // Immediately expand root
        expandNode(0);
        
        if (currentPath && currentPath !== "/") {
             console.log("DirectoryTree: Initial sync request for " + currentPath);
             pendingExpandPath = currentPath;
             isSyncing = true;
             syncToPath();
        }
    }
    
    function expandNode(index) {
        var node = treeModel.get(index);
        if (node.expanded) return;
        
        node.expanded = true;
        node.isLoading = true;
        
        // Request subdirs
        sendCommand({"action": "get_subdirs", "path": node.path});
    }
    
    function collapseNode(index) {
        var node = treeModel.get(index);
        if (!node.expanded) return;
        
        node.expanded = false;
        
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
            // Check if we already have children populated (maybe partial update?)
            // For now, assume if we requested, we want to populate.
            // But if we already have children, we might duplicate.
            // Simplified: The collapse logic removes children. 
            // So if expanded is true, we expect children to be there OR we are inserting them.
            
            // Check if next item is child
            var nextIndex = foundIndex + 1;
            var parentLevel = treeModel.get(foundIndex).level;
            
            if (nextIndex < treeModel.count) {
                var next = treeModel.get(nextIndex);
                if (next.level > parentLevel) {
                     // Already populated?
                     // Maybe we should clear and re-populate?
                     // For now, let's remove existing children and re-add to be safe/fresh.
                     collapseNode(foundIndex); 
                     treeModel.get(foundIndex).expanded = true; // Re-expand state
                }
            }
            
            // Insert children
            // Prepare items
            var newItems = [];
            for(var j=0; j<dirs.length; j++) {
                var d = dirs[j];
                treeModel.insert(foundIndex + 1 + j, {
                    "name": d.name,
                    "path": d.path,
                    "level": parentLevel + 1,
                    "expanded": false,
                    "hasChildren": true, // Assume folders have children for UI purposes until proven otherwise
                    "isLoading": false
                });
            }
            
            // If no children, maybe mark as leaf?
            if (dirs.length === 0) {
                 treeModel.get(foundIndex).hasChildren = false;
            }
            
            // Continue sync if active
            if (isSyncing) {
                syncToPath();
            }
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#333333"
            
            Text {
                anchors.centerIn: parent
                text: "Directories"
                color: "#cccccc"
                font.bold: true
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
                color: (model.path === treeRoot.currentPath) ? treeRoot.selectionColor : (msgMouse.containsMouse ? treeRoot.hoverColor : "transparent")
                
                // Row Selection MouseArea (Background)
                MouseArea {
                    id: msgMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    
                    onClicked: (mouse) => {
                         sendCommand({"action": "change_path", "path": model.path});
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
                background: Rectangle { color: "#222222" }
                contentItem: Rectangle {
                    implicitWidth: 6
                    implicitHeight: 100
                    radius: 3
                    color: treeView.active ? "#555555" : "#333333"
                }
            }
        }
    }
}

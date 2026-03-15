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
    property var pinnedList: []

    signal sendCommand(var cmd)

    // Direct command channel — bypasses signal chain for reliability
    XsAttributeValue {
        id: tree_command_attr
        attributeTitle: "command_channel"
        model: pluginData
    }

    function sendTreeCommand(cmd) {
        console.log("DirectoryTree: sendTreeCommand: " + JSON.stringify(cmd));
        // Write directly to the attribute, bypassing signal chain
        if (tree_command_attr.index && tree_command_attr.index.valid) {
            tree_command_attr.value = JSON.stringify(cmd);
        } else {
            console.log("DirectoryTree: WARNING - command attr index not valid, falling back to signal");
            sendCommand(cmd);
        }
    }
    
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
    property int autoScanThreshold: 4

    // Normalize path for consistent comparison (case-insensitive on Windows, strip trailing slash)
    function normPath(p) {
        if (!p) return "";
        var n = p.replace(/\\/g, "/");
        // Remove trailing slash unless it's just "/"
        if (n.length > 1 && n.charAt(n.length - 1) === '/') {
            n = n.substring(0, n.length - 1);
        }
        // Case-insensitive on Windows
        if (Qt.platform.os === "windows") {
            n = n.toLowerCase();
        }
        return n;
    }

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
        if (currentPath && currentPath !== "/") {
            var target = normPath(currentPath);
            console.log("DirectoryTree: onCurrentPathChanged target=" + target);

            // First check if the node is already visible — just highlight it
            var found = false;
            for (var i = 0; i < treeModel.count; i++) {
                if (normPath(treeModel.get(i).path) === target) {
                    treeView.currentIndex = i;
                    treeView.positionViewAtIndex(i, ListView.Center);
                    found = true;
                    // Expand if not already expanded (so children show)
                    if (!treeModel.get(i).expanded) {
                        expandNode(i);
                    }
                    break;
                }
            }

            // If not visible, use syncToPath to progressively expand to it
            if (!found) {
                pendingExpandPath = target;
                isSyncing = true;
                // Delay sync slightly to let the change_path command finish
                // processing before we send get_subdirs through the same channel
                syncTimer.start();
            }
        }
    }

    Timer {
        id: syncTimer
        interval: 100
        repeat: false
        onTriggered: syncToPath()
    }

    function syncToPath() {
        if (!pendingExpandPath) return;

        // Find deepest matching node in current model
        var deepestIndex = -1;
        var deepestLen = 0;

        for(var i=0; i<treeModel.count; i++) {
            var node = treeModel.get(i);
            var np = normPath(node.path);

            var match = false;
            if (pendingExpandPath === np) match = true;
            else if (pendingExpandPath.indexOf(np + "/") === 0) match = true;
            else if (np === "/") match = true;

            if (match) {
                if (np.length > deepestLen || (np === "/" && deepestLen === 0)) {
                    deepestLen = np.length;
                    deepestIndex = i;
                }
            }
        }

        if (deepestIndex !== -1) {
            var node = treeModel.get(deepestIndex);
            var nodePath = normPath(node.path);

            if (nodePath === pendingExpandPath) {
                // We reached the target!
                treeView.currentIndex = deepestIndex;
                pendingExpandPath = "";
                isSyncing = false;
                treeView.positionViewAtIndex(deepestIndex, ListView.Center);
                if (!node.expanded) expandNode(deepestIndex);
            } else {
                if (!node.expanded) {
                    expandNode(deepestIndex);
                    // wait for handleQueryResult to call us back
                } else {
                    if (node.isLoading) {
                        // Waiting for results
                    } else {
                        console.log("DirectoryTree: syncToPath stuck at " + nodePath + ", target=" + pendingExpandPath);
                        pendingExpandPath = "";
                        isSyncing = false;
                    }
                }
            }
        } else {
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
                    console.log("DirectoryTree: Parsed result for path: " + result.path + ", dirs: " + (result.dirs ? result.dirs.length : "0") + ", isSyncing: " + isSyncing);
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
    
    // Delay initial expand so parent signal connections are wired up first
    Timer {
        id: initTimer
        interval: 500
        repeat: false
        onTriggered: {
            console.log("DirectoryTree: initTimer fired. cmdIndex valid=" + (tree_command_attr.index ? tree_command_attr.index.valid : "null"));
            if (tree_command_attr.index && tree_command_attr.index.valid) {
                expandNode(0);
                // After root expands and results come back, sync to the current path
                syncAfterInit = true;
            } else {
                console.log("DirectoryTree: command attr not ready yet, retrying in 500ms");
                initTimer.restart();
            }
        }
    }

    // Flag to trigger sync after the initial root expand completes
    property bool syncAfterInit: false

    Component.onCompleted: {
        var rootName = Qt.platform.os === "windows" ? "This PC" : "Root";
        treeModel.append({
            "name": rootName,
            "path": "/",
            "level": 0,
            "expanded": false,
            "hasChildren": true,
            "isLoading": false
        });
        initTimer.start();
    }
    
    function expandNode(index) {
        var node = treeModel.get(index);
        console.log("DirectoryTree: expandNode called for: " + node.path + ", expanded: " + node.expanded + ", isLoading: " + node.isLoading);
        
        // If already expanded and not loading, we might still need to load if it has no children but should
        // However, for now let's just allow re-requesting if isLoading is false or if explicitly called when collapsed
        if (node.expanded && !node.isLoading) {
             // Check if children already exist
             var nextIndex = index + 1;
             if (nextIndex < treeModel.count) {
                 var next = treeModel.get(nextIndex);
                 if (next.level > node.level) {
                     console.log("DirectoryTree: Node already expanded with children.");
                     return;
                 }
             }
             // No children? Trigger load anyway
             console.log("DirectoryTree: Node expanded but no children, re-requesting.");
        } else if (node.expanded) {
            return;
        }
        
        treeModel.setProperty(index, "expanded", true);
        
        if (node.isLoading) {
            console.log("DirectoryTree: Node is already loading, skipping command.");
            return;
        }
        
        treeModel.setProperty(index, "isLoading", true);
        
        // Request subdirs
        sendTreeCommand({"action": "get_subdirs", "path": node.path});
    }
    
    function collapseNode(index) {
        var node = treeModel.get(index);
        console.log("DirectoryTree: collapseNode called for: " + node.path);
        
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
        var path = normPath(result.path);
        var dirs = result.dirs;

        var foundIndex = -1;
        for(var i=0; i<treeModel.count; i++) {
            var node = treeModel.get(i);
            if (normPath(node.path) === path && node.expanded) {
                foundIndex = i;
                node.isLoading = false;
                break;
            }
        }
        
        if (foundIndex !== -1) {
            console.log("DirectoryTree: Found target node at index: " + foundIndex);
            
            // Check if next item is already a child
            var nextIndex = foundIndex + 1;
            var parentLevel = treeModel.get(foundIndex).level;
            
            if (nextIndex < treeModel.count) {
                var next = treeModel.get(nextIndex);
                if (next.level > parentLevel) {
                     console.log("DirectoryTree: Removing existing children before re-populating.");
                     collapseNode(foundIndex); 
                     treeModel.setProperty(foundIndex, "expanded", true); 
                }
            }
            
            // Insert children
            console.log("DirectoryTree: Inserting " + dirs.length + " children for " + path);
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
                 console.log("DirectoryTree: No children found, marking as leaf.");
                 treeModel.setProperty(foundIndex, "hasChildren", false);
            } else {
                 treeModel.setProperty(foundIndex, "hasChildren", true);
            }
            
            treeModel.setProperty(foundIndex, "isLoading", false);

            // Continue sync if active
            if (isSyncing) {
                syncToPath();
            } else if (syncAfterInit) {
                // Root just expanded — now sync to the current path
                syncAfterInit = false;
                if (currentPath && currentPath !== "/") {
                    var target = normPath(currentPath);
                    console.log("DirectoryTree: Post-init sync to " + target);
                    pendingExpandPath = target;
                    isSyncing = true;
                    syncToPath();
                }
            }
        } else {
            console.log("DirectoryTree: Target node for result not found or not expanded: " + path);
            // Search for node to reset isLoading anyway
            for(var k=0; k<treeModel.count; k++) {
                if (normPath(treeModel.get(k).path) === path) {
                    treeModel.setProperty(k, "isLoading", false);
                    break;
                }
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
                color: (treeRoot.normPath(model.path) === treeRoot.normPath(treeRoot.currentPath)) ? treeRoot.selectionColor : ((msgMouse.containsMouse || scanMouse.containsMouse) ? "#2d2d2d" : "transparent")
                
                // Row Selection MouseArea (Background)
                MouseArea {
                    id: msgMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            treeContextMenu.targetPath = model.path
                            treeContextMenu.targetName = model.name
                            treeContextMenu.popup()
                        } else {
                            sendTreeCommand({"action": "change_path", "path": model.path});
                        }
                    }
                }

                Menu {
                    id: treeContextMenu

                    property string targetPath: ""
                    property string targetName: ""

                    background: Rectangle {
                        implicitWidth: 180
                        implicitHeight: 40
                        color: "#333333"
                        border.color: "#555555"
                        radius: 3
                    }

                    delegate: MenuItem {
                        id: treeMenuItem
                        contentItem: Text {
                            text: treeMenuItem.text
                            color: "#e0e0e0"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 10
                        }
                        background: Rectangle {
                            implicitWidth: 180
                            implicitHeight: 25
                            color: treeMenuItem.highlighted ? "#555555" : "transparent"
                        }
                    }

                    MenuItem {
                        property bool pathIsPinned: {
                            var p = treeContextMenu.targetPath
                            if (!p) return false
                            for (var i = 0; i < treeRoot.pinnedList.length; i++) {
                                if (treeRoot.pinnedList[i].path === p) return true
                            }
                            return false
                        }
                        text: pathIsPinned ? "Remove from Favorites" : "Add to Favorites"
                        onTriggered: {
                            if (pathIsPinned) {
                                sendTreeCommand({"action": "remove_pin", "path": treeContextMenu.targetPath})
                            } else {
                                sendTreeCommand({"action": "add_pin", "name": treeContextMenu.targetName, "path": treeContextMenu.targetPath})
                            }
                        }
                    }

                    MenuItem {
                        text: "Scan"
                        onTriggered: sendTreeCommand({"action": "force_scan", "path": treeContextMenu.targetPath})
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
                            color: scanMouse.containsMouse ? "#2a2a2a" : "#1a1a1a"
                            radius: 4
                            border.color: "#333333"
                            border.width: 1
                            
                            Text {
                                anchors.centerIn: parent
                                text: "SCAN"
                                color: "#666666"
                                font.pixelSize: 8
                                font.bold: true
                            }
                            
                            MouseArea {
                                id: scanMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    // Set path AND trigger scan in one atomic action
                                    sendTreeCommand({"action": "force_scan", "path": model.path})
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

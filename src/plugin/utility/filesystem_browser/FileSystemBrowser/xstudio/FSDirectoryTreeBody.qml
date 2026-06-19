import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.models 1.0

import "."
import ".."

Item {

    id: treeRoot
    
    // Properties to communicate with parent
    property var currentPath: "/"
    property string baseRootPath: "/"
    
    signal sendCommand(var cmd)
    //onSendCommand: (cmd) => console.log("DirectoryTree: Sending command: " + JSON.stringify(cmd))
    
    // Style constants to match FilesystemBrowser
    
    // Auto-expand logic
    property string pendingExpandPath: ""
    property bool isSyncing: false
    readonly property string pathSep: Qt.platform.os === "windows" ? "\\" : "/"
    property bool wasClicked: false

    Timer {
        id: clickResetTimer
        interval: 1000
        running: false
        repeat: false
        onTriggered: treeRoot.wasClicked = false
    }
    onWasClickedChanged: {
        if (wasClicked) {
            clickResetTimer.start();
        }
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
        // Start sync process
        if (currentPath && currentPath !== "/") {
            pendingExpandPath = currentPath;
            isSyncing = true;
            syncToPath();
        }
    }

    function stripTrailingPathSeparator(p) {
        p = p.endsWith(pathSep) ? p.slice(0, -pathSep.length) : p
        return p;
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
            if (stripTrailingPathSeparator(node.path) === stripTrailingPathSeparator(pendingExpandPath)) {
                // We reached the target!
                treeView.currentIndex = deepestIndex;
                pendingExpandPath = "";
                isSyncing = false;
                // Ensure visible
                if (!wasClicked) { // Don't auto-scroll if user just clicked to navigate
                    treeView.positionViewAtIndex(deepestIndex, ListView.Center);
                }
                // Also expand to show children as requested
                if (!node.expanded) {
                    // Actually - we don't want to auto expand
                    // expandNode(deepestIndex);
                }

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
                //console.log("DirectoryTree: Query result parse error: " + e);
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

        //console.log("result: " + JSON.stringify(result));

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
                    "hasChildren": d.has_subdir, 
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
            //console.log("DirectoryTree: Target node for result not found or not expanded: " + path);
            // Search for node to reset isLoading anyway
            for(var k=0; k<treeModel.count; k++) {
                if (treeModel.get(k).path === path) {
                    treeModel.setProperty(k, "isLoading", false);
                    break;
                }
            }
        }
    }
    
    XsPopupMenu {
                
        id: treeContextMenu
        menu_model_name: "treeCtxMn" + treeContextMenu
        property var path: ""

        XsMenuModelItem {
            text: "Set as Root"
            menuItemPosition: 1
            menuPath: ""
            onActivated: treeRoot.baseRootPath = treeContextMenu.path
            menuModelName: treeContextMenu.menu_model_name
        }
        
        XsMenuModelItem {
            text: "Show in Finder"
            menuItemPosition: 2
            menuPath: ""
            onActivated: {
                helpers.showURIS([helpers.QUrlFromPosixPath(treeContextMenu.path)])
            }
            menuModelName: treeContextMenu.menu_model_name
        }

    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Header for custom root
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? XsStyleSheet.widgetStdHeight : 0
            color: XsStyleSheet.panelBgColor
            visible: treeRoot.baseRootPath !== "/"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 5
                spacing: 5
                
                XsText {
                    text: "Directory root: " + treeRoot.baseRootPath
                    Layout.fillWidth: true
                    Layout.preferredHeight: XsStyleSheet.widgetStdHeight
                    elide: Text.ElideLeft
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    
                    MouseArea {
                        id: rootHoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                    ToolTip.visible: rootHoverArea.containsMouse
                    ToolTip.text: treeRoot.baseRootPath
                    ToolTip.delay: 500
                }
                
                XsPrimaryButton {
                    imgSrc: "qrc:/icons/home.svg"
                    Layout.preferredHeight: 16
                    Layout.preferredWidth: 16
                    onClicked: treeRoot.baseRootPath = "/"
					imageDiv.height: height-2
					imageDiv.width: width-2
                }
            }
        }
        
        ListView {

            id: treeView
            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            model: treeModel
            
            delegate: Item {
                id: rowDelegate
                width: ListView.view.width - 10 // allow for scrollbar
                height: XsStyleSheet.widgetStdHeight
                property var isHovered: msgMouse.containsMouse || scanButton.hovered

                Rectangle {
                    anchors.fill: parent
                    color: (stripTrailingPathSeparator(model.path) === stripTrailingPathSeparator(treeRoot.currentPath)) ? XsStyleSheet.accentColor : isHovered ? XsStyleSheet.hintColor : "transparent"
                    opacity: 0.33
                }
                
                // Row Selection MouseArea (Background)
                MouseArea {
                    id: msgMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton) {
                            sendCommand({"action": "change_path", "path": model.path});
                            wasClicked = true;
                        } else if (mouse.button === Qt.RightButton) {
                            treeContextMenu.path = model.path;
                            treeContextMenu.showMenu(msgMouse, mouse.x, mouse.y);
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
                    // Expander
                    XsSecondaryButton {
                        Layout.preferredWidth: 20
                        Layout.fillHeight: true
                        imgSrc: "qrc:/icons/chevron_right.svg"
                        rotation: model.expanded ? 90 : 0
                        Behavior on rotation {NumberAnimation{duration: 150 }}
                        visible: model.hasChildren // Show expander if we know there are subdirs, or if we don't know yet (optimistic)
                        onClicked: {
                            if (model.expanded) {
                                collapseNode(index);
                            } else {
                                expandNode(index);
                            }
                        }
                    }

                    Item {
                        Layout.preferredWidth: model.hasChildren ? 0 : 20 // Align with expander space if no children
                    }
                    
                    // Folder Icon
                    XsIcon {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        Layout.alignment: Qt.AlignVCenter
                        source: model.expanded ? "qrc:/icons/folder_open.svg" : "qrc:/icons/folder.svg"
                    }
                    
                    // Name
                    XsText {
                        text: model.name
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                        leftPadding: 5
                    }

                    // Scan Button Container (Prevents layout jitter by taking space only if scan is possible)
                    Rectangle {

                        id: scanButton
                        Layout.preferredWidth: 60
                        Layout.fillHeight: true
                        Layout.margins: 5
                        property var hovered: mma.containsMouse
                        property var pressed: mma.pressed
                        radius: 4
                        border.width: 1
                        border.color: (hovered && !pressed)? XsStyleSheet.accentColor : XsStyleSheet.menuBorderColor
                        color: pressed ? XsStyleSheet.accentColor : XsStyleSheet.panelBgColor
                        XsText {
                            anchors.centerIn: parent
                            text: "Full Scan"
                            font.pixelSize: XsStyleSheet.fontSize * 0.8
                        }
                        MouseArea {
                            id: mma
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            onClicked: sendCommand({"action": "force_scan", "path": model.path})
                        }
                        visible: isHovered && !model.isLoading
                    }
                    
                    // Loading Indicator
                    XsText {
                        text: "..."
                        visible: model.isLoading
                        Layout.rightMargin: 5
                    }
                }
                

            }
            
            ScrollBar.vertical: XsScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
                width: 10
            }
        }
    }
}

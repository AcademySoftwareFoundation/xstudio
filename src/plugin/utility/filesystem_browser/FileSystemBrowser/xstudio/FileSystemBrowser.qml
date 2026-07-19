import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQuick.Shapes 1.15    // Added for vector icon

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0

import filesystembrowser.qml 1.0


import "."
//import "XsText.qml" as Text

Item {

    id: root

    anchors.fill: parent // Ensure it fills the panel

    property var columnPositions: [0, 200, 280, 360, 460, 580] // Initial positions based on headerModel sizes
    property var columnWidths: [200, 80, 80, 100, 120, 80] // Initial widths based on headerModel sizes

    property var selectedItems: []
    property var selectedItemsUrls: []
    property var selectedItemsPaths: []
    property var underMouseIndex: undefined

    property var previewing: previewMediaUuid != helpers.QVariantFromUuidString("") ? previewMediaUuid == currentPlayhead.mediaUuid : false

    // uri requires triple slash before drive letter on Windows, since this is the root.
    property var thumbSrcRoot: Qt.platform.os === "windows" ? "image://thumbnail/file:///" : "image://thumbnail/file://"

    FileSystemBrowserScanResultsModel {
        id: __scanResultsModel
        viewMode: root.viewMode
        numThumbnailCols: 4
        sortRole: root.sortRole
        sortAscending: root.sortAscending
    }

    property alias scanResultsModel: __scanResultsModel

    XsHotkeyArea {
        id: hotkey_area
        anchors.fill: parent
        context: "fsb"
        focus: true
    }
    XsFocusRemover {
        target: hotkey_area
    }

    Keys.forwardTo: hotkey_area

    function jumpSelection(step) {

        if (selectedItems.length == 0) return

        var newIdx = selectedItems[0]
        var rc = scanResultsModel.rowCount(newIdx.parent)
        while (step != 0 && newIdx.row < (rc-1) && newIdx.row > 0) {
            if (step > 0) {
                newIdx = scanResultsModel.index(newIdx.row + 1, 0, newIdx.parent)
                step--;
            } else {
                newIdx = scanResultsModel.index(newIdx.row - 1, 0, newIdx.parent)
                step++;
            }
        }

        if (newIdx.valid && newIdx != selectedItems[0]) {
            selectItem(newIdx, 0)
        }

    }

    function loadSelectedItems() {

        if (selectedItems.length == 1) {
            var idx = selectedItems[0]
            if (scanResultsModel.get(idx, "is_folder")) {
                sendCommand({"action": "change_path", "path": scanResultsModel.get(idx, "path")});
                return
            }
        }

        var v = []
        for (var i = 0; i < selectedItems.length; i++) {
            var idx = selectedItems[i]
            if (!scanResultsModel.get(idx, "is_folder")) {
                v.push(scanResultsModel.get(idx, "path"))
            }
        }
        sendCommand({"action": "load_files", "paths": v})

    }

    XsHotkey {
        context: "fsb"
        sequence:  "Left"
        name: "Step To Previous"
        description: "When using the File Browser use this hotkey to quickly step backward through the media items found by the scan and start an immediate preview."
        onActivated: {
           jumpSelection(-1)
        }
        componentName: "File Browser"
    }

    XsHotkey {
        context: "fsb"
        sequence:  "Right"
        name: "Step To Next"
        description: "When using the File Browser use this hotkey to quickly step forward through the media items found by the scan and start an immediate preview."
        onActivated: {
            jumpSelection(1)
        }
        componentName: "File Browser"
    }

    XsHotkey {
        context: "fsb"
        sequence:  "Return"
        name: "Load all selected media"
        description: "Load all selected media (File Browser)"
        onActivated: loadSelectedItems()
        componentName: "File Browser"
    }

    XsHotkey {
        context: "fsb"
        sequence:  "Ender"
        name: "SLoad all selected media"
        description: "Load all selected media (File Browser)"
        onActivated: loadSelectedItems()
        componentName: "File Browser"
    }

    XsHotkey {
        context: "fsb"
        sequence:  "Down"
        name: "Step Down"
        description: "When using the File Browser in thumbnail mode use this hotkey to quickly step down through the media items found by the scan and start an immediate preview."
        onActivated: {
            if (viewMode === 3) {
                jumpSelection(scanResultsModel.numThumbnailCols)
            }
        }
        componentName: "File Browser"
    }
    
    XsHotkey {
        context: "fsb"
        sequence:  "Down"
        name: "Step Down"
        description: "When using the File Browser in thumbnail mode use this hotkey to quickly step down through the media items found by the scan and start an immediate preview."
        onActivated: {
            if (viewMode === 3) {
                jumpSelection(scanResultsModel.numThumbnailCols)
            }
        }
        componentName: "File Browser"
    }

    onSelectedItemsChanged: {        
        var v = []
        var v2 = []
        for (var i = 0; i < selectedItems.length; i++) {
            if (scanResultsModel.get(selectedItems[i], "is_folder")) {
                for (var j = 0; j < scanResultsModel.rowCount(selectedItems[i]); j++) {
                    let idx = scanResultsModel.index(j, 0, selectedItems[i])
                    if (scanResultsModel.get(idx, "is_folder")) continue;
                    else {
                        v.push(helpers.QUrlFromPosixPath(scanResultsModel.get(idx, "path")))
                        v2.push(scanResultsModel.get(idx, "path"))
                    }
                }
            } else {
                v.push(helpers.QUrlFromPosixPath(scanResultsModel.get(selectedItems[i], "path")))
                v2.push(scanResultsModel.get(selectedItems[i], "path"))
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
                if (selectedItems[i].parent == index.parent) {
                    if (nearest === -1 || Math.abs(selectedItems[i] - index.row) < Math.abs(nearest - index.row)) {
                        nearest = selectedItems[i].row
                    }
                }
            }
            if (nearest !== -1) {
                let start = Math.min(nearest, index.row)
                let end = Math.max(nearest, index.row)
                let v = selectedItems
                for (let i = start; i <= end; i++) {
                    let sidx = scanResultsModel.index(i, 0, index.parent)
                    if (!selectedItems.includes(sidx)) v.push(sidx)
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
        }

    }

    XsGradientRectangle{
        anchors.fill: parent
    }

    XsModuleData {
        id: pluginData
        modelDataName: "Filesystem Browser" 
    }

    XsModuleData {
        id: settings
        modelDataName: "Filesystem Browser Settings" 
    }

    XsModuleData {
        id: filterTermAttrs
        modelDataName: "Filter Terms" 
    }

    // State for Preview Mode
    property bool haveSubFolders: false

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

                historyList = []
            }
        }
        
        onValueChanged: updateList()
        Component.onCompleted: updateList()
    }

    XsAttributeValue {
        id: __preview_media_uuid
        attributeTitle: "preview_media_uuid"
        model: pluginData
    }
    property var previewMediaUuid: helpers.QVariantFromUuidString(__preview_media_uuid.value)
    
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
        id: current_path_attr
        attributeTitle: "current_path"
        model: pluginData
    }

    XsAttributeValue {
        id: directory_tree_width_attr
        attributeTitle: "directory_tree_width"
        model: settings
    }

    XsAttributeValue {
        id: directory_tree_visible_attr
        attributeTitle: "directory_tree_visible"
        model: settings
    }
    property alias showDirectoryTree: directory_tree_visible_attr.value

    XsAttributeValue {
        attributeTitle: "Sort Role"
        model: settings
        id: __sortRole
    }
    property alias sortRole: __sortRole.value

    XsAttributeValue {
        attributeTitle: "Sort Ascending"
        model: settings
        id: __sortAscending
    }
    property alias sortAscending: __sortAscending.value
    

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
            scanResultsModel.currentFilterTime = value || "Any"
        }
    }
    XsAttributeValue { 
        id: filter_version_attr
        attributeTitle: "Version"
        model: pluginData
        role: "value"
        onValueChanged: {
            scanResultsModel.currentFilterVersion = value || "All Versions"
        }
        Component.onCompleted: {
            scanResultsModel.currentFilterVersion = value || "All Versions"
        }
    }
    XsAttributeValue {
        id: searching_attr
        attributeTitle: "searching"
        model: pluginData
    }
    
    XsAttributeValue {
        id: __view_mode
        attributeTitle: "view_mode"
        model: pluginData
    }
    property alias viewMode: __view_mode.value
    // View Mode: 0=List, 1=Tree, 2=Grouped

    property var scannedDirsList: []

    function sendCommand(cmd) {
        command_attr.value = JSON.stringify(cmd)
    }
    
    // Sorting State
    property string sortColumn: "name"
    property int sortOrder: 1 // 1 for asc, -1 for desc
    
    // tree logic
    property var collapsedPaths: ({})
    // Complete mixed flat model (all groups + files). Not assigned to Repeater directly.
        
    function formatDate(timestamp) {
        if (!timestamp) return ""
        var d = new Date(timestamp * 1000)
        return d.toLocaleString(Qt.locale(), Locale.ShortFormat)
    }
    
    ListModel {
        id: headerModel
        ListElement { title: "Name"; sortId: "name"; colId: "name"; size: 200; resizable: false; position: 0 }
        ListElement { title: "Version"; sortId: "version"; colId: "version"; size: 80; resizable: true; position: 1 }
        ListElement { title: "Frames"; sortId: "frames"; colId: "frames"; size: 80; resizable: true; position: 2 }
        ListElement { title: "Owner"; sortId: "owner"; colId: "owner"; size: 100; resizable: true; position: 3 }
        ListElement { title: "Date"; sortId: "date"; colId: "date"; size: 120; resizable: true; position: 4 }
        ListElement { title: "Size"; sortId: "size"; colId: "size_str"; size: 80; resizable: true; position: 5 }
    }

    Component {
        id: fileListView
        ColumnLayout {

            anchors.fill: parent
            spacing: 0
            visible: root.viewMode != 3
            function getItemAtIndex(index) {
                return fileListView.getItemAtIndex(index)
            }

            FSListHeader {
                Layout.fillWidth: true
                columns_root_model: headerModel
                /*property var columnPositions: root.columnPositions
                property var columnWidths: root.columnWidths*/
            }

            FSListView {
                id: fileListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                mousePosition: Qt.point(mouseArea.mouseX, mouseArea.mouseY)
            }
        }
    }
             
    Component {
        id: fileTreeView
        ColumnLayout {

            anchors.fill: parent
            spacing: 0
            visible: root.viewMode != 3
            function getItemAtIndex(index) {
                return fileListView.getItemAtIndex(index)
            }

            FSListHeader {
                Layout.fillWidth: true
                columns_root_model: headerModel
                /*property var columnPositions: root.columnPositions
                property var columnWidths: root.columnWidths*/
            }

            FSTreeView {
                id: fileListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                mousePosition: Qt.point(mouseArea.mouseX, mouseArea.mouseY)
            }
        }
    }
    
    Component {
        id: thumbView

        FSThumbView {
            property var view: thumbFlickable
            anchors.fill: parent
            visible: root.viewMode == 3
            id: thumbFlickable
            onWidthChanged: {
                scanResultsModel.numThumbnailCols = Math.max(1, width/160)
            }
        }
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
            SplitView.preferredWidth: showDirectoryTree ? directory_tree_width_attr.value : 30
            Layout.fillHeight: true
            property var showing: showDirectoryTree
            onShowingChanged: {
                if (showing) {
                    width = directory_tree_width_attr.value
                }
            }
            onWidthChanged: {
                if (showDirectoryTree) {
                    directory_tree_width_attr.value = width
                }
            }
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

                Loader {
                    id: viewLoader
                    anchors.fill: parent
                    sourceComponent: root.viewMode === 0 ? fileListView : root.viewMode === 1 ? fileTreeView : thumbView
                }

                MouseArea {
                                
                    id: mouseArea
                    anchors.topMargin: root.viewMode !== 3 ? XsStyleSheet.widgetStdHeight : 0 // avoid list header
                    anchors.fill: parent
                    anchors.rightMargin: 20 // Avoid scrollbar area
                    propagateComposedEvents: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                                
                    onReleased: (mouse)=> {

                        if (underMouseIndex != -1 && mouse.button === Qt.LeftButton && mouse.modifiers == Qt.NoModifier) {    
                            selectedItems = []
                            selectItem(underMouseIndex, 0)
                            
                        } 
                        
                    }

                    onPressed: (mouse) => {

                        if (underMouseIndex != undefined && mouse.button === Qt.LeftButton) {
                            selectItem(underMouseIndex, mouse.modifiers == Qt.ShiftModifier ? 1 : mouse.modifiers == Qt.ControlModifier ? 2 : 0)
                        }
                        
                    }

                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton && underMouseIndex != -1) {

                            if (scanResultsModel.get(underMouseIndex, "is_folder")) {
                                sendCommand({"action": "change_path", "path": scanResultsModel.get(underMouseIndex, "path")});
                            } else {
                                loadSelectedItems()
                            }
                        }
                    }

                    onClicked: (mouse) => {

                        if (mouse.button === Qt.RightButton && underMouseIndex != -1) {
                            let clickedItemPath = scanResultsModel.get(underMouseIndex, "path")
                            thumbContextMenu.itemPath = clickedItemPath
                            thumbContextMenu.showMenu(
                                mouseArea,
                                mouse.x, mouse.y);
                        }
                    }

                    function startDrag(draggedVisItem) {
                        dragProxy.dragItem = draggedVisItem
                    }
                
                    FSFileContextMenu {
                        id: thumbContextMenu
                    }
                        
                    DragHandler {
                        id: dragHandler
                        target: null

                        onActiveChanged: {
                            if (active && underMouseIndex != -1) {
                                dragProxy.grabToImage(function(result) {
                                    let d = root.selectedItemsPaths.map(p => encodeURIComponent(p))
                                    dragProxy.Drag.mimeData = {"text/plain": d.join("\n")}
                                    dragProxy.Drag.imageSource = result.url
                                    dragProxy.Drag.active = true
                                })
                            } else {
                                dragProxy.Drag.active = false
                            }
                        }
                    }                                

                }

                // Nothing found message
                ColumnLayout {

                    anchors.centerIn: parent
                    spacing: 20
                    visible: scanResultsModel.numMediaFiles === 0 && !searching_attr.value && !deepScan
                    XsText {
                        id: msg
                        text: "No media found in current folder."
                        color: "#666666"
                        font.pixelSize: 18
                    }
                    XsPrimaryButton {
                        Layout.preferredWidth: 200
                        Layout.alignment: Qt.AlignHCenter
                        visible: scanResultsModel.numRootFolders && !deepScan
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

    Item {
        id: dragProxy
        property var dragItem: null
        property int dragCount: selectedItems.length

        width: dragItem ? Math.min(dragItem.width, 300) : 0
        height: dragItem ? Math.max(dragItem.height, 64) : 0
        visible: false
        z: 100

        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction
        Drag.hotSpot.x: width/2
        Drag.hotSpot.y: height/2

        ShaderEffectSource {
            width: parent.width
            height: parent.height
            sourceRect.width: width
            sourceRect.height: height
            sourceItem: parent.dragItem
            opacity: 0.65
        }

        // Count badge
        Rectangle {
            x: parent.width / 2 + 20
            y: parent.height / 2 - 20
            width: dragProxy.dragCount > 1 ? 22 : 0
            height: 22
            radius: 11
            color: "#e05a00"
            clip: true

            Text {
                anchors.centerIn: parent
                text: dragProxy.dragCount
                color: "#ffffff"
                font.pixelSize: 10; font.weight: Font.Bold
            }
        }
    }

    property alias dragProxy: dragProxy

    property alias thumbToolTip: thumbToolTip
    XsToolTip {
        
        id: thumbToolTip
        delay: 500
        maxWidth: 400
    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Qt.labs.qmlmodels
import xStudio 1.0

import ".."
import "."

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

    Keys.onLeftPressed: (event) => {
        if (selectedItems.length == 0) return

        var newIdx = selectedItems[0]
        do {
            if (newIdx > 0) newIdx--
            else break
        } while (flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header")

        selectedItems = [newIdx]
        _handleThumbKeyPreview()
        event.accepted = true
    }

    Keys.onRightPressed: (event) => {

        if (selectedItems.length == 0) return
        var newIdx = selectedItems[selectedItems.length-1]
        var maxIdx = flatThumbnailModel.length - 1
        do {
            if (newIdx < maxIdx) newIdx++
            else break
        } while (flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header")
        selectedItems = [newIdx]
        _handleThumbKeyPreview()
        event.accepted = true
    }
    Keys.onUpPressed: (event) => {
        if (selectedItems.length == 0) return
        var cols = Math.max(1, Math.floor(thumbFlow.width / 160))
        var newIdx = selectedItems[0] - cols
        if (newIdx >= 0) {
            while (newIdx > 0 && flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header") newIdx--
            selectedItems = [newIdx]
            _handleThumbKeyPreview()
        }
        event.accepted = true
    }
    Keys.onDownPressed: (event) => {
        var cols = Math.max(1, Math.floor(thumbFlow.width / 160))
        var maxIdx = flatThumbnailModel.length - 1
        var newIdx = selectedItems[selectedItems.length-1] + cols
        if (newIdx <= maxIdx) {
            while (newIdx < maxIdx && flatThumbnailModel[newIdx] && flatThumbnailModel[newIdx].type === "header") newIdx++
            selectedItems = [newIdx]
            _handleThumbKeyPreview()
        }
        event.accepted = true
    }
    Keys.onReturnPressed: (event) => _handleThumbReturn(event)
    Keys.onEnterPressed: (event) => _handleThumbReturn(event)

    function _handleThumbReturn(event) {
        for (var i = 0; i < selectedItems.length; i++) {
            var idx = selectedItems[i]
            if (idx >= 0 && idx < flatThumbnailModel.length) {
                var md = flatThumbnailModel[idx]
                if (md && md.type === "file") {
                    previewTimer.stop()
                    isPreviewMode = false
                    sendCommand({"action": "load_file", "path": md.path})
                }
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


    ScrollBar.vertical: XsScrollBar {
        policy: ScrollBar.AlwaysOn
    }

    // these properties track the current visible area of the flicable, 
    // to allow thumb delegates to decide when to load/clear their thumbnail source 
    // for better thumbnail load performance
    property var windowBottom: 0
    property var windowTop: thumbFlickable.height + 160
    property var yMin: 0

    // When the user is scrolling we delay update of the windowBottom/windowTop 
    // properties until scrolling has stopped for 200ms, to avoid excessive 
    // thumbnail loading/unloading during scroll
    Timer {
        id: scrollChangeTimer
        interval: 200
        onTriggered: {
            yMin = thumbFlickable.contentY - thumbFlickable.originY
            windowBottom = yMin - 160
            windowTop = windowBottom + thumbFlickable.height + 160
        }
    }
    onOriginYChanged: scrollChangeTimer.restart()

    property var mousePosition: mouse
    property var mousePositionInFlickArea: Qt.point(mousePosition.x, mousePosition.y + yMin)
    onMousePositionChanged: {
        if (!visible) return
        for (var i = 0; i < flatThumbnailModel.length; i++) {
            var item = thumbFlow.thumbRepeater.itemAt(i)
            if (item) {
                if (item.x < mousePositionInFlickArea.x && item.y < mousePositionInFlickArea.y && 
                    (item.x + item.width) > mousePositionInFlickArea.x && (item.y + item.height) > mousePositionInFlickArea.y) {
                    underMouseIndex = i
                    var pt = mapToItem(item, mousePosition)
                    item.mouseMove(pt.x, pt.y)
                    return
                }
            }
        }        
        underMouseIndex = -1
    }

    property alias thumbs: thumbFlow.thumbRepeater

    Flow {
        id: thumbFlow
        width: thumbFlickable.contentWidth
        spacing: 0
        property alias thumbRepeater: thumbRepeater

        Repeater {
            id: thumbRepeater
            model: flatThumbnailModel

            delegate: DelegateChooser {
                role: "type"
                DelegateChoice {
                    roleValue: "header"
                    FSThumbFolderHeader {
                        width: thumbFlow.width
                        height: 32
                    }
                }
                DelegateChoice {
                    roleValue: "file"
                    FSThumbItem {
                        width: 160
                        height: 120
                    }
                }
            }
        } // Repeater
    } // Flow

    onContentYChanged: {
        scrollChangeTimer.restart()
    }

} // Flickable

// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import xStudio 1.0

import ".."
import "."

XsListView {

    id: fileListView
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

    ScrollBar.vertical: XsScrollBar {
        visible: fileListView.height < fileListView.contentHeight
    }

    onCurrentIndexChanged: {
        if (activeFocus && currentItem) {
            if (!currentItem.isItemFolder) {
                root.pendingPreviewPath = currentItem.itemPath
                previewTimer.restart()
            }
        }
    }
    
    clip: true
    model: visibleTreeList
    
    flickableDirection: Flickable.HorizontalAndVerticalFlick
    boundsBehavior: Flickable.StopAtBounds
        
    delegate: FSListItem {
        width: fileListView.width
        isItemFolder: modelData.isFolder
        itemPath: modelData.path
        property int index: model.index
    }

    property var yMin: 0

    // When the user is scrolling we delay update of the windowBottom/windowTop 
    // properties until scrolling has stopped for 200ms, to avoid excessive 
    // thumbnail loading/unloading during scroll
    Timer {
        id: scrollChangeTimer
        interval: 200
        onTriggered: {
            yMin = contentY - originY
        }
    }
    onOriginYChanged: scrollChangeTimer.restart()
    onContentYChanged: scrollChangeTimer.restart()    
    property var mousePosition: mouse
    property var mousePositionInFlickArea: Qt.point(mousePosition.x, mousePosition.y + yMin) // adjust for header height
    onMousePositionChanged: {
        if (!visible) return
        var index = fileListView.indexAt(mousePositionInFlickArea.x, mousePositionInFlickArea.y)
        underMouseIndex = index
    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import xStudio 1.0

import ".."
import "."

XsListView {

    id: fileListView
    focus: visible
    onVisibleChanged: { if (visible) forceActiveFocus() }
    property var depth: 0

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
                    sendCommand({"action": "load_file", "path": md.path})
                }
            }
        }
        event.accepted = true
    }

    ScrollBar.vertical: XsScrollBar {
        visible: fileListView.height < fileListView.contentHeight
    }

    /*onCurrentIndexChanged: {
        if (activeFocus && currentItem) {
            if (!currentItem.isItemFolder) {
                root.pendingPreviewPath = currentItem.itemPath
                previewTimer.restart()
            }
        }
    }*/
    
    clip: true
    //model: scanResultsModel
    
    flickableDirection: Flickable.HorizontalAndVerticalFlick
    boundsBehavior: Flickable.StopAtBounds
        
    property var aRootIndex: scanResultsModel.rootIndex

    model: DelegateModel {
        id: delegateModel
        model: scanResultsModel
        rootIndex: aRootIndex
        delegate:  FSListItem {
            modelIndex: scanResultsModel.index(index, 0, aRootIndex)
            width: fileListView.width
            property var n: name
            onNChanged: modelIndex = scanResultsModel.index(index, 0, aRootIndex)
        }
    }

    function getItemAtIndex(index) {
        return itemAtIndex(index)
    }

    property var yMin: contentY - originY
    property var windowBottom: yMin-60
    property var windowTop: yMin + height + 60

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
        let newIdx = scanResultsModel.indexAtVisibleRow(mousePositionInFlickArea.y/XsStyleSheet.widgetStdHeight)
        if (newIdx != underMouseIndex) {
            underMouseIndex = newIdx
        }

    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import xStudio 1.0

import ".."
import "."

Item {

    id: thumbGroup
    property var thumbsModel
    property var thumbsModelIndex

    //Layout.preferredHeight: num_thumbnail_rows*130 + header.height + 10
    implicitHeight: Math.ceil(file_count/scanResultsModel.numThumbnailCols)*120 + header.height

    property bool strictlyVisible: (y < thumbFlickable.windowTop) && (y+height) > (thumbFlickable.windowBottom)
    property bool visibleInFlickable: smallResultSet || ((y < thumbFlickable.windowTopMore) && (y+height) > (thumbFlickable.windowBottomMore))

    // ── Folder path header (spans full row) ────────────
    Rectangle {
        id: header
        width: parent.width
        height: 40
        color: "#1a1a1a"
        property bool isHovered: underMouseIndex === thumbsModelIndex
        border.color: isHovered ? XsStyleSheet.accentColor : "transparent"
        border.width: 1
        XsIcon {
            id: folderIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left; anchors.leftMargin: 10
            width: 24; height: 24
            source: "qrc:/icons/folder.svg"
        }
        XsText {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: folderIcon.right; anchors.leftMargin: 10
            text: path + "   (" + total_file_count + ")"
            font.bold: true
            elide: Text.ElideLeft
            font.pixelSize: 14
        }
    }

    DelegateModel {
        id: delegateModel
        model: thumbsModel
        rootIndex: thumbsModelIndex//visibleInFlickable ? thumbsModelIndex : undefined
        delegate: FSThumbItem {
            rooty: thumbGroup.y + header.height
            width: 160
            height: 120
        }
    }

    Connections {
        target: mouseArea
        enabled: thumbGroup.strictlyVisible
        function onPositionChanged(mouse) {
            var pt = mouseArea.mapToItem(thumbGroup, mouse.x, mouse.y)
            var row = Math.floor((pt.y-40)/120)
            var col = Math.floor(pt.x/160)
            var index = scanResultsModel.index(row*scanResultsModel.numThumbnailCols + col, 0, thumbsModelIndex)
            if (index.valid) {
                var item = thumbRepeater.itemAt(index.row)
                if (item) {
                    underMouseIndex = index
                    return true
                }
            }
            if (pt.y > 0 && pt.y < header.height && pt.x > 0 && pt.x < header.width) {
                underMouseIndex = thumbsModelIndex
                return true
            }
        }
        function onPressed() {

            if (underMouseIndex == thumbsModelIndex) {
                mouseArea.startDrag(header)
            }
        }
    }

    GridLayout {
        id: flow
        anchors.top: header.bottom
        columns: scanResultsModel.numThumbnailCols
        columnSpacing: 0
        rowSpacing: 0
        Repeater {
            id: thumbRepeater
            model: visibleInFlickable ? delegateModel : undefined
        }

    }

    function getThumbAtIndex(row) {
        return thumbRepeater.itemAt(row)
    }
    
} // delegate
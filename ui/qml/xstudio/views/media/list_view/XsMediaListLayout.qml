// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.14

import xStudio 1.0
import "."

Item {

    id: root

    property var scrollPosition: verticalScroll.position
    property var scrollPressed: verticalScroll.pressed

    Rectangle{ id: resultsBg
        anchors.fill: parent
        color: XsStyleSheet.panelBgColor
        z: -1
    }
    XsLabel {
        anchors.fill: parent
        visible: !mediaList.count
        text: 'Click the "+" button or drop files/folders here to add Media'
        color: XsStyleSheet.hintColor
        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    Flickable {

        id: flick
        anchors.fill: parent
        contentWidth: Math.max(titleBar.barWidth, width) 
        interactive: false

        ScrollBar.horizontal: XsScrollBar {
            id: scrollbar
            height: 10
            orientation: Qt.Horizontal 
            visible: flick.width < flick.contentWidth
        }

        ColumnLayout {

            id: layout
            spacing: 0
            height: root.height
            width: parent.width

            XsMediaHeader{
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: XsStyleSheet.widgetStdHeight
            }

            XsMediaList {
                id: mediaList
                Layout.fillWidth: true
                Layout.fillHeight: true
                itemRowHeight: rowHeight

                property var yPos: visibleArea.yPosition
                onYPosChanged: {
                    if (!verticalScroll.pressed) {
                        verticalScroll.position = yPos
                    }
                }
                
                property var heightRatio: visibleArea.heightRatio
                onHeightRatioChanged: {
                    verticalScroll.size = heightRatio
                }

                property var scrollPositionTracker: scrollPosition
                onScrollPositionTrackerChanged: {
                    if (scrollPressed) {
                        contentY = (scrollPosition * contentHeight) + originY
                    }
                }

            }

        }
    }

    XsScrollBar {
        id: verticalScroll
        width: 10
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.right: parent.right
        orientation: Qt.Vertical
        anchors.topMargin: titleBar.height
        visible: mediaList.height < mediaList.contentHeight
    }    

}

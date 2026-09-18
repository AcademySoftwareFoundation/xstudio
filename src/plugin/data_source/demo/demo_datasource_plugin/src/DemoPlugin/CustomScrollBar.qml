// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

// To get a scrollbar that works as expected, we need custom logic as
// follows. This is because our tree view item expansion doesn't work
// perfectly with QML ListView, which tries to predict the height of the
// items that are in the view but not visible - this prediction breaks because
// of the variable size of the items in the list (due to the way they expand
// when the user hits the expand button)
ScrollBar {
    
    id: verticalScroll
    width: 8
    anchors.bottom: target.bottom
    anchors.top: target.top
    anchors.right: target.right
    anchors.rightMargin: 3
    orientation: Qt.Vertical
    visible: target.height < target.contentHeight
    policy: ScrollBar.AlwaysOn
    property var target

    property var yPos: target.visibleArea.yPosition
    onYPosChanged: {
        if (!pressed) {
            position = yPos
        }
    }
    
    property var heightRatio: target.visibleArea.heightRatio
    onHeightRatioChanged: {
        verticalScroll.size = heightRatio
    }

    onPositionChanged: {
        if (pressed) {
            target.contentY = (position * target.contentHeight) + target.originY
        }
    }                

    contentItem:
    Rectangle {
        implicitWidth: 5
        implicitHeight: 5
        radius: width/1.1
        color: verticalScroll.pressed ? palette.highlight: "grey"
    }    
}   
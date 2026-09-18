// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

GridView{
    property bool isScrollbarVisibile : true
    property alias scrollBar: scrollBar

    width: contentWidth < parent.width - x ? contentWidth : parent.width - x
    height: contentHeight < parent.height - y ? contentHeight : parent.height - y

    cellHeight: contentItem.childrenRect.height
    cellWidth: contentItem.childrenRect.width

    interactive: true
    flow: GridView.FlowLeftToRight

    ScrollBar.vertical: XsScrollBar {
        id: scrollBar
        x: 0
        visible: isScrollbarVisibile && parent.height < parent.contentHeight
    }

    moveDisplaced: Transition {
        NumberAnimation{properties: "x, y"; duration: 10000}
    }
}


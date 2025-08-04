// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic




import xStudio 1.0

GridView{ id: widget
    // Rectangle{anchors.fill: parent; color: "yellow"; opacity: 0.3}

    property bool isScrollbarVisibile : true
    property alias scrollBar: scrollBar

    width: contentWidth<parent.width-x? contentWidth : parent.width-x
    height: contentHeight<parent.height-y? contentHeight : parent.height-y

    cellHeight: contentItem.childrenRect.height
    cellWidth: contentItem.childrenRect.width

    // clip: true
    interactive: true
    flow: GridView.FlowLeftToRight

    // model:

    ScrollBar.vertical: XsScrollBar {id: scrollBar; x: 0; 
        visible: isScrollbarVisibile && widget.height < widget.contentHeight;
    }

    moveDisplaced: Transition {
        NumberAnimation{properties: "x, y"; duration: 10000}
    }

}


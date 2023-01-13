// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

Rectangle {

    id: divider
    width: XsStyle.dividerWidth*visible
    color: XsStyle.dividerColor
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    property real ratio: 0.5
    property real rmin: 0.05
    property real stored_ratio: 0.0
    visible: !(ratio == 0.0 || ratio == 1.0)
    property bool hidden: false
    property var parentWidth: parent.width

    onRatioChanged: {
        if (!dragArea.drag.active) {
            resposition()
        }        
    }

    onXChanged: {
        if (dragArea.drag.active) {
            ratio = x/parentWidth
        }
    }

    onParentWidthChanged: {
        resposition()
    }

    function resposition() {
        x = ratio*parentWidth
    }

    PropertyAnimation {
        id: animator
        target: divider
        property: "ratio"
        running: false
        duration: XsStyle.presentationModeAnimLength
        onFinished: {
            if (!(ratio == 1.0 || ratio == 0.0)) {
                hidden = false
            }
        }
    }


    function send_to(new_ratio) {

        if (!hidden) stored_ratio = ratio
        hidden = true
        animator.to = new_ratio
        animator.running = true
    }

    function unhide() {
        
        //if (!(ratio == 1.0 || ratio == 0.0)) return
        animator.to = stored_ratio        
        animator.running = true
        
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        drag.target: parent
        cursorShape: Qt.SizeHorCursor
        // Disable smoothed so that the Item pixel from where we started the drag remains under the mouse cursor
        drag.smoothed: false
        drag.minimumX: rmin*parentWidth
        drag.maximumX: (1.0-rmin)*parentWidth
    }

}
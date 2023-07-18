// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12

import xStudio 1.0

Rectangle {

    id: listViewContainer
    height: header.item_height
    //z: (dragArea.drag.active || x_animator.running) ? 100 : 0
    property var uuid
    property var currentIndex
    property int targetIndex: currentIndex
    property var reapeaterGroup
    property bool underMouse: false
    property bool selected: selection_index != 0
    property int selection_index: selection_uuids.indexOf(uuid) + 1
    property bool being_dragged_outside: false

    property alias media_item: media_item
    property real preview_frame: -1
    color: "transparent"
    property var orig_x
    property var orig_y

    onUnderMouseChanged: {
        if (!underMouse) {
            preview_frame = -1
        }
    }

    XsMediaViewListItem {
        id: media_item
        preview_frame: listViewContainer.preview_frame
    }

    PropertyAnimation {
        id: x_animator
        target: listViewContainer
        property: "x"
        running: false
        duration: dragDropAnimationDuration
    }

    PropertyAnimation {
        id: y_animator
        target: listViewContainer
        property: "y"
        running: false
        duration: dragDropAnimationDuration
    }

    function reverse_animation() {

        z = 0
        x_animator.running = false
        y_animator.running = false
        x_animator.to = orig_x
        y_animator.to = orig_y
        x_animator.running = true
        y_animator.running = true

    }

    function drag_animation(target_x, target_y, zz) {
        z = zz
        x_animator.running = false
        y_animator.running = false
        x_animator.to = target_x
        y_animator.to = target_y
        x_animator.running = true
        y_animator.running = true
    }

    function drag_stop() {
        z = 0
    }

    function immediate_drag(target_x, target_y) {
        if (y_animator.running) {
            y_animator.to = target_y
        } else {
            y = target_y
        }
        if (x_animator.running) {
            x_animator.to = target_x
        } else {
            x = target_x
        }
    }

    function setNewTargetIndex (newIdx) {
        y_animator.to = newIdx*header.item_height
        y_animator.running = true
        targetIndex = newIdx
    }

    Rectangle {
        anchors.fill: parent
        visible: underMouse
        color: "transparent"
        opacity: 0.5
        border.color: "white"
    }

    Rectangle {
        anchors.fill: parent
        visible: being_dragged_outside
        color: "orange"
        opacity: 0.5
        //border.color: "white"
    }
}

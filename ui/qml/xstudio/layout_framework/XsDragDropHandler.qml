// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

Item {
    
    id: ddHandler
    property var targetWidget: parent
    property string dragSourceName: "Unknown drag source"
    property var dragData
    property var dragStartPosition
    property bool isDragTarget: false

    property real dragThresholdPixels: 30

    signal dragEntered(var mousePosition, string source, var data)
    signal dragged(var mousePosition, string source, var data)
    signal dropped(var mousePosition, string source, var data)
    signal dragExited()
    signal dragEnded()

    function startDrag(mouseX, mouseY) {
        dragStartPosition = targetWidget.mapToItem(
            appWindow.contentItem,
            mouseX,
            mouseY
            )
        dragging = false
    }

    property bool dragging: false

    function endDrag(mouseX, mouseY) {

        if (dragging) {
            var dragMousePosition = targetWidget.mapToItem(
                appWindow.contentItem,
                mouseX,
                mouseY
                )
            global_drag_drop_handler.endDrag(
                dragMousePosition,
                dragSourceName,
                dragData)
            dragging = false
            dragEnded()
        }
    }

    function doDrag(mouseX, mouseY) {

        var dragMousePosition = targetWidget.mapToItem(
            appWindow.contentItem,
            mouseX,
            mouseY
            )

        if (!dragging) {
            // drag only starts whne the mouse has moved by dragThresholdPixels
            var dx = dragMousePosition.x-dragStartPosition.x
            var dy = dragMousePosition.y-dragStartPosition.y
            var d = Math.sqrt(dx*dx + dy*dy)
            if (d > dragThresholdPixels) {
                global_drag_drop_handler.drag(
                    dragMousePosition,
                    dragSourceName,
                    dragData)
                dragging = true
            }

        } else {
            global_drag_drop_handler.drag(
                dragMousePosition,
                dragSourceName,
                dragData)
        }
    }

    property bool pointerInside: false

    Connections {

        target: global_drag_drop_handler
        enabled: visible

        function onTargetHasChanged(target, oldTarget, dragCursorPosition, dragDropSource, dragDropData) {

            var local_drag_pos = appWindow.contentItem.mapToItem(
                targetWidget,
                dragCursorPosition.x,
                dragCursorPosition.y
                )
            if (target == ddHandler) {
                dragEntered(local_drag_pos, dragDropSource, dragDropData)
                isDragTarget = true
            } else if (oldTarget == ddHandler) {
                dragExited()
                isDragTarget = false
            } else if (target == undefined && pointerInside) {
                // pointer has left a target, but pointer is still inside
                // THIS handler so we ask to make ourselves the target
                global_drag_drop_handler.setTarget(ddHandler, dragCursorPosition, dragDropSource, dragDropData)
            }

        }

        function onDragDropDragged(dragCursorPosition, dragDropSource, dragDropData) {
            var local_drag_pos = appWindow.contentItem.mapToItem(
                targetWidget,
                dragCursorPosition.x,
                dragCursorPosition.y
                )
            if (local_drag_pos.x > 0 && local_drag_pos.y > 0 &&
                local_drag_pos.x < targetWidget.width && local_drag_pos.y < targetWidget.height) 
            {
                // mouse is inside targetWidget bounds
                if (!pointerInside) {
                    // mouse has *just* entered - make us the target
                    global_drag_drop_handler.setTarget(ddHandler, dragCursorPosition, dragDropSource, dragDropData)
                    pointerInside = true
                }
                if (global_drag_drop_handler.currentTarget == ddHandler) {
                    // if we are the target, emit dragged signal
                    dragged(local_drag_pos, dragDropSource, dragDropData)
                }

            } else if (pointerInside) {

                // mouse has left targetWidget bounds
                pointerInside = false
                // if we were the target, make sure we aren't anymore (so
                // another widget that the pointer is inside can take over)
                if (global_drag_drop_handler.currentTarget == ddHandler) {
                    global_drag_drop_handler.setTarget(undefined, dragCursorPosition, dragDropSource, dragDropData)
                }
            }
        }

        function onDragFinished(dragCursorPosition, dragDropSource, dragDropData) {
            var local_drag_pos = appWindow.contentItem.mapToItem(
                targetWidget,
                dragCursorPosition.x,
                dragCursorPosition.y
                )
            if (local_drag_pos.x > 0 && local_drag_pos.y > 0 &&
                local_drag_pos.x < targetWidget.width && local_drag_pos.y < targetWidget.height) 
            {
                if (global_drag_drop_handler.currentTarget == ddHandler) {
                    dropped(local_drag_pos, dragDropSource, dragDropData)
                }
            }
            pointerInside = false
        }

    }
}

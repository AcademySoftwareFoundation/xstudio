// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15

DropArea { 
    
    // The goal for this adaption of DropArea is to make drag/drops from inside
    // xSTUDIO work in a compatible way with drag/drops from elsewhere on the
    // desktop.

    // This Item has signals that XsDragDropReceiver listens to - UI items that
    // want to accept drag/drop events can add an XsDragDropReceiver and then
    // listen to its signals to know when the mouse/cursor has entered their space 
    // with data to drop etc.

    id: drag_handler
    anchors.fill: parent
    property var dragMousePosition
    property var dragStartPosition

    property var currentTarget

    property bool dragging: false
    property bool externalDrag: false

    signal dragDropDragged(var dragCursorPosition, string dragDropSource, var dragDropData)
    signal dragFinished(var dragCursorPosition, string dragDropSource, var dragDropData)
    signal targetHasChanged(var target, var oldTarget, var dragCursorPosition, string dragDropSource, var dragDropData)

    onDraggingChanged: {
        if (!dragging || externalDrag) {
            // for externalDrag into xstudio, we don't want to override the 
            // cursor shape as the desktop should be showing a drag-drop cursor
            cursor_override_ma.cursorShape = undefined
        } else {
            cursor_override_ma.cursorShape = Qt.DragMoveCursor
        }
    }    

    function setTarget(newTarget, dragCursorPosition, dragDropSource, dragDropData) {
        var oldTarget = currentTarget
        currentTarget = newTarget
        targetHasChanged(currentTarget, oldTarget, dragCursorPosition, dragDropSource, dragDropData)
    }

    function drag(dragMousePosition,
        dragSourceName,
        dragData) {
            // an XsDragDropHandler is telling us something is being dragged
            // ...  rebroadcast to all XsDragDropHandlers via this signal
            externalDrag = false
            dragging = true
            dragDropDragged(dragMousePosition, dragSourceName, dragData)
        }

    function endDrag(dragMousePosition,
        dragSourceName,
        dragData) {
            // ... as above, re-broadcast
            dragging = false
            dragFinished(dragMousePosition, dragSourceName, dragData)
        }        

    MouseArea {
        id: cursor_override_ma
        anchors.fill: parent
        enabled: false
        cursorShape: undefined
    }
    property alias cursor_override_ma: cursor_override_ma
        
    // QML Set-up for generic drag drop handling from outside the application
    keys: [
        "text/uri-list",
        "xstudio/media-ids",
        "application/x-dneg-ivy-entities-v1"
    ]

    onEntered: {
        externalDrag = true
        dragging = true
        dragDropDragged(Qt.point(drag.x, drag.y), "External", drag)
    }

    onPositionChanged: {
        dragDropDragged(Qt.point(drag.x, drag.y), "External", drag)
    }

    onDropped: {

        if(drop.hasUrls) {
            let uris = ""
            drop.urls.forEach(function (item, index) {
                uris = uris + String(item) +"\n"
            })
            dragFinished(Qt.point(drop.x, drop.y), "External URIS", uris)
        } else {

            // prepare drop data
            let data = {}
            for(let i=0; i< drop.keys.length; i++){
                data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
            }
            dragFinished(Qt.point(drop.x, drop.y), "External JSON", data)

        }
        dragging = false
        externalDrag = false
        
    }    
}


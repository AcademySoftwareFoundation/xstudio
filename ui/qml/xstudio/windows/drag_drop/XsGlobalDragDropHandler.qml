// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects
import xstudio.qml.helpers 1.0

DropArea { 
    
    // The goal for this adaption of DropArea is to make drag/drops from inside
    // xSTUDIO work in a compatible way with drag/drops from elsewhere on the
    // desktop.

    // This Item has signals that XsDragDropHandler listens to - UI items that
    // want to accept drag/drop events can add an XsDragDropHandler and then
    // listen to its signals to know when the mouse/cursor has entered their space 
    // with data to drop etc.

    id: drag_handler
    anchors.fill: parent
    property var dragMousePosition
    property var dragStartPosition

    property var currentTarget

    property var dragItem: null
    property int dragCount: 0

    property bool dragging: false
    property bool externalDrag: false

    signal dragDropDragged(var dragCursorPosition, string dragDropSource, var dragDropData)
    signal dragFinished(var dragCursorPosition, string dragDropSource, var dragDropData)
    signal targetHasChanged(var target, var oldTarget, var dragCursorPosition, string dragDropSource, var dragDropData)

    onDraggingChanged: {
        if (!dragging || externalDrag) {
            // for externalDrag into xstudio, we don't want to override the 
            // cursor shape as the desktop should be showing a drag-drop cursor
            helpers.restoreOverrideCursor()
            dragItem = null
        } else {
            if (dragItem) {
                drag_proxy.grabToImage(function(result) {
                    helpers.setOverrideCursor(result)
                })
            } else {
                helpers.setOverrideCursor("Qt.DragMoveCursor")
            }
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

    // QML Set-up for generic drag drop handling from outside the application
    keys: [
        "text/uri-list",
        "xstudio/media-ids",
        "text/plain",
        "application/x-dneg-ivy-entities-v1"
    ]

    onEntered: (drag)=> {
        externalDrag = true
        dragging = true
        dragDropDragged(Qt.point(drag.x, drag.y), "External", drag)
    }

    onPositionChanged: (drag)=> {
        dragDropDragged(Qt.point(drag.x, drag.y), "External", drag)
    }

    onDropped: (drop)=> {

        if(drop.hasUrls) {
            let uris = ""
            drop.urls.forEach(function (item, index) {
                uris = uris + String(item) +"\n"
            })
            dragFinished(Qt.point(drop.x, drop.y), "External URIS", uris)
            drop.accept()
        } else {

            // prepare drop data
            let data = {}
            for(let i=0; i< drop.keys.length; i++){
                data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
            }
            dragFinished(Qt.point(drop.x, drop.y), "External JSON", data)
            drop.accept()

        }
        dragging = false
        externalDrag = false
        
    }    

    Item {
        id: drag_proxy
        property int max_width: 300
        property int min_height: 64
        width: dragItem ? (dragItem.width < max_width ? dragItem.width : max_width) : 0
        height: dragItem ? (dragItem.height > min_height ? dragItem.height : min_height) : 0
        visible: false

        ShaderEffectSource {
            width: parent.width
            height: parent.height
            sourceItem: dragItem
            sourceRect.width: width
            sourceRect.height: height
            opacity: 0.65
        }

        Image {
            id: drag_cursor
            source: "qrc:/cursors/drag_cursor.svg"
            sourceSize.width: 32
            sourceSize.height: 32
            x: drag_proxy.width/2 - width/2
            y: drag_proxy.height/2 - height/2
        }

        Rectangle {
            id: count_bg
            width: dragCount > 1 ? 18 : 0
            height: width
            radius: width/2
            x: drag_cursor.x + drag_cursor.width
            y: drag_cursor.y + drag_cursor.height - height/2
            color: "#FFF00000"
            //visible: dragCount > 1 // this doesn't work for some reason

            Text {
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.bold: true
                text: dragCount
                color: "white"
            }
        }
    }
}


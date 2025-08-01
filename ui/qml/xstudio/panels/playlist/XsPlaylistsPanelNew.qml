// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick 2.14
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import QtQml.Models 2.12
import QtQml 2.12
import Qt.labs.qmlmodels 1.0
import xstudio.qml.uuid 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1
import xstudio.qml.helpers 1.0

Rectangle {
    id: panel
    Layout.minimumHeight: 100

    color: XsStyle.mainBackground
    focus: true

    DropArea {
        id: dp
        anchors.fill: parent

        property var previousItem: null

        keys: [
            "text/uri-list",
            "xstudio/media-ids",
            "application/x-dneg-ivy-entities-v1",
            "xstudio/container-ids"
        ]

        onExited: {
            moveTimer.stop()
            if(previousItem) {
                unDropping(previousItem)
                previousItem = null
            }
        }

        XsTimer {
            id: moveTimer
        }

        function unDropping(item) {
            item.insertionFlag = false
            item.dropFlag = false
            if(item.dropEndFlag != undefined)
                item.dropEndFlag = false
            else if(item.parent != undefined && item.parent.parent != undefined && item.parent.parent.dropEndFlag != undefined)
                item.parent.parent.dropEndFlag = false
        }

        function bumpPosition(dragx, dragy) {
            // logic for auto scrolling..
            if(session_listview.visibleArea.heightRatio < 1.0 && !moveTimer.running) {
                let index = session_listview.indexAt(dragx, dragy + session_listview.contentY)
                if(index != -1) {
                    if(!session_listview.atYBegining && dragy < 15 && index) {
                        moveTimer.setTimeout(function() { return function() {
                            session_listview.positionViewAtIndex(index - 1, ListView.Beginning)
                            bumpPosition(dragx, dragy)
                        }}(), 250);
                    } else if(!session_listview.atYEnd && dragy > session_listview.height - 15) {
                        if(index < session_listview.model.count) {
                            moveTimer.setTimeout(function() { return function() {
                                session_listview.positionViewAtIndex(index + 1, ListView.End)
                                bumpPosition(dragx, dragy)
                            }}(), 250);
                        } else {
                            moveTimer.setTimeout(function() { return function() {
                                session_listview.positionViewAtEnd()
                            }}(), 250);
                        }
                    }
                }
            }
        }

        onPositionChanged: {
            // something dragging in...
            // try and map to listview item
            let item = session_listview.itemAt(drag.x, drag.y + session_listview.contentY)
            // check for expanded playlist
            if(item != undefined && item.expanded != undefined && item.expanded) {
                // check for subitem..
                let m = session_listview.mapToItem(item.list_view, drag.x, drag.y + session_listview.contentY)
                let mi = item.list_view.itemAt(m.x, m.y)
                if(mi)
                    item = mi
            }

            if(item != previousItem) {
                if(previousItem) {
                    unDropping(previousItem)
                }
                previousItem = item
                if(item) {
                    if(previousItem.dropEndFlag != undefined)
                        previousItem.dropEndFlag = true
                    else if(previousItem.parent != undefined && previousItem.parent.parent != undefined && previousItem.parent.parent.dropEndFlag != undefined)
                        previousItem.parent.parent.dropEndFlag = true

                    item.insertionFlag = drag.keys.includes("xstudio/container-ids")
                    item.dropFlag = !drag.keys.includes("xstudio/container-ids")
                }
            }

            moveTimer.stop()
            bumpPosition(drag.x, drag.y)
        }

        onDropped: {
            // check for session file
           moveTimer.stop()
           if(drop.hasUrls) {
                for(var i=0; i < drop.urls.length; i++) {
                    if(drop.urls[i].toLowerCase().endsWith('.xst') || drop.urls[i].toLowerCase().endsWith('.xsz')) {
                        Future.promise(studio.loadSessionRequestFuture(drop.urls[i])).then(function(result){})
                        app_window.sessionFunction.newRecentPath(drop.urls[i])
                        return;
                    }
                }
            }

            // prepare drop data
            let data = {}
            for(let i=0; i< drop.keys.length; i++){
                data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
            }

            if("xstudio/media-ids" in data) {
                let before = null
                let internal_copy = false

                if(previousItem) {
                    before = previousItem.modelIndex()
                }

                // does media exist in our parent.
                if(before) {
                    let mi = app_window.sessionModel.search_recursive(
                        helpers.QVariantFromUuidString(data["xstudio/media-ids"].split("\n")[0]), "idRole"
                    )

                    if(app_window.sessionModel.getPlaylistIndex(before) == app_window.sessionModel.getPlaylistIndex(mi)) {
                        internal_copy = true
                    }
                }

                if(internal_copy) {
                    Future.promise(
                        app_window.sessionModel.handleDropFuture(Qt.CopyAction, data, before)
                    ).then(function(quuids){})
                } else {
                    media_move_copy_dialog.data = data
                    media_move_copy_dialog.index = before
                    media_move_copy_dialog.open()
                }

            } else {
                if(previousItem) {
                    Future.promise(
                        app_window.sessionModel.handleDropFuture(drop.proposedAction, data, previousItem.modelIndex())
                    ).then(function(quuids){})
                } else {
                    Future.promise(
                        app_window.sessionModel.handleDropFuture(drop.proposedAction, data)
                    ).then(function(quuids){})
                }
            }

            if(previousItem) {
                unDropping(previousItem)
                previousItem = null
            }
        }
    }

    XsMediaMoveCopyDialog {
        id: media_move_copy_dialog
    }

    Label {
        anchors.centerIn: parent
        verticalAlignment: Qt.AlignVCenter
        horizontalAlignment: Qt.AlignHCenter

        text: 'Drop media or folders here\nto create new playlists'
        color: XsStyle.controlTitleColor
        opacity: dp.containsDrag ? 1.0 : 0.3
        font.pixelSize: 14
    }

    DelegateChooser {
        id: chooser
        role: "typeRole"

        XsDelegateChoiceDivider {}
        XsDelegateChoicePlaylist {}
    }

    // property alias sess: sess
    DelegateModel {
        id: sess
        property var srcModel: app_window.sessionModel
        model: srcModel
        rootIndex: srcModel.length ? srcModel.index(0,0) : null
        delegate: chooser
    }

    ListView {
        id: session_listview
        anchors.fill: parent
        implicitHeight: contentItem.childrenRect.height
        model: sess
        focus: true
        clip: true
        z:1

       ScrollBar.vertical: XsScrollBar {
            policy: session_listview.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        // so we can place at bottom of list.
        footer: Rectangle {
            color: "transparent"
            width: parent.width
            height: 40
        }
    }

    Item {
        id: dragContainer
        anchors.fill: session_listview

        Rectangle {
            id: dragItem
            color: "transparent"
            visible: false
            width: 100
            height: 50
            property string label: "ITEMS"
            property var source: ""

            XsLabel {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                height: 16
                width: parent.width
                horizontalAlignment: Text.AlignRight
                text: dragItem.label
                z: 10
            }

            Image {
                id: image
                anchors.fill: parent
                // anchors.bottomMargin: 16
                fillMode: Image.PreserveAspectFit
                cache: true
                smooth: true
                transformOrigin: Item.Center
                // asynchronous: true
                // sourceSize.width: Math.max(128, dragContainer.nearestPowerOf2(width))
                source: dragItem.source
            }

        }

        Drag.active: moveDragHandler.active || copyDragHandler.active
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.MoveAction

        Connections {
            target: app_window.sessionSelectionModel
            function onSelectionChanged(selected, deselected) {
                let indexs = app_window.sessionSelectionModel.selectedIndexes
                if(indexs.length) {
                    dragItem.label = indexs.length + " items"
                    let item = session_listview.itemAtIndex(indexs[0].row)
                    if(item) {
                        item.grabToImage(function(result){
                            dragItem.source = result.url
                            dragItem.grabToImage(function(result){
                                dragContainer.Drag.imageSource = result.url
                            })
                        })
                    }
                }
            }
        }

        function startDrag(mode) {
            dragContainer.Drag.supportedActions = mode
            let indexs = app_window.sessionSelectionModel.selectedIndexes
            let ids = []

            for(let i=0;i<indexs.length; ++i) {
                ids.push([indexs[i].row, indexs[i].model.get(indexs[i], "idRole")])
            }
            ids = ids.sort((a,b) => a[0] - b[0] )
            for(let i=0;i<ids.length; ++i) {
                ids[i] = ids[i][1]
            }

            if(!ids.length) {
                // cancel drag nothing being dragged.
                dragContainer.Drag.cancel()
            } else {
                dragContainer.Drag.mimeData = {
                    "xstudio/container-ids": ids.join("\n")
                }
            }
        }

        DragHandler {
            id: moveDragHandler
            acceptedModifiers: Qt.NoModifier
            onActiveChanged: {
                if(active) {
                    dragContainer.startDrag(Qt.MoveAction)
                } else{
                    // no idea why I have to do this
                    dragContainer.x = 0
                    dragContainer.y = 0
                }
            }
        }

        DragHandler {
            id: copyDragHandler
            acceptedModifiers: Qt.ControlModifier
            onActiveChanged: {
                if(active) {
                    dragContainer.startDrag(Qt.CopyAction)
                } else{
                    // no idea why I have to do this
                    dragContainer.x = 0
                    dragContainer.y = 0
                }
            }
        }
    }


}

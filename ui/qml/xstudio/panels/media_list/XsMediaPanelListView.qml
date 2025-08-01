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
    color: XsStyle.mainBackground

    property int itemWidth: width
    property int itemHeight: thumbSize * 9/16
    property alias thumbSize: media_list_thumbnail_size.value
    property int selectionIndicatorWidth: 20
    property int filenameLeft: thumbSize + selectionIndicatorWidth
    property alias mediaMenu: media_menu

    XsModelProperty {
        id: media_list_thumbnail_size
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/media_list_thumbnail_size", "pathRole")
    }

// application/x-dneg-ivy-entities-v1 [{"type": "File", "show": "MEG2", "id": "a11d54b9-ab19-4b57-8ba1-b75071d256f9", "name": "main_proxy0", "path": "/jobs/MEG2/060_tre_1610/ELEMENT/E_060_tre_1610_comp_precomp_diffusion_v001/4222x1768/E_060_tre_1610_comp_precomp_diffusion_v001.1041-1135####.exr"}]


    DropArea {
        id: drop_area
        property var previousItem: null
        keys: [
            "text/uri-list",
            "xstudio/media-ids",
            "application/x-dneg-ivy-entities-v1"
        ]

        anchors.fill: parent

        onExited: {
            moveTimer.stop()
            if(previousItem) {
                previousItem.insertionFlag = false
                previousItem = null
            }
        }

        onEntered: {
            dragX = drag.x
            dragY = drag.y
            moveTimer.start()
        }

        property var dragX
        property var dragY

        Timer {
            id: moveTimer
            repeat: true
            interval: 100
            onTriggered: drop_area.bumpPosition(drop_area.dragX, drop_area.dragY)
        }

        function bumpPosition(dragx, dragy) {
            // logic for auto scrolling..
            if(list_view.visibleArea.heightRatio < 1.0) {
                let index = list_view.indexAt(dragx, dragy + list_view.contentY)

                if(index != -1) {
                    if(!list_view.atYBegining && dragy - itemHeight < 30 && index) {
                        // console.log("bump up", index - 1)
                        list_view.positionViewAtIndex(index - 1, ListView.Beginning)
                    } else if(!list_view.atYEnd && dragy > list_view.height - 30) {
                        if(index < list_view.model.count) {
                            list_view.positionViewAtIndex(index + 1, ListView.End)
                            // console.log("bump down", index + 1)
                        } else {
                            list_view.positionViewAtEnd()
                            // console.log("bump END")
                        }
                    }
                }
            }
        }

        onPositionChanged: {
            // something dragging in...
            // try and map to listview item
            dragX = drag.x
            dragY = drag.y

            let item = list_view.itemAt(drag.x, drag.y + list_view.contentY)
            if(item != previousItem) {
                if(previousItem) {
                    previousItem.insertionFlag = false
                }
                previousItem = item
                if(item) {
                    item.insertionFlag = true
                }
            }
        }

        onDropped: {
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
                // console.log(drop.keys[i], data[drop.keys[i]])
            }

            // console.log("drop.proposedAction",drop.proposedAction)
            let dropindex = sess.rootIndex.parent
            // console.log(previousItem, dropindex)

            if(previousItem) {
                dropindex = previousItem.modelIndex()
            }
            // console.log(previousItem, dropindex)
            // if media drop..
            if(dropindex.valid) {
                Future.promise(app_window.sessionModel.handleDropFuture(drop.proposedAction, data, dropindex)).then(
                    function(quuids){
                        // if(viewport.playhead)
                        //     viewport.playhead.jumpToSource(quuids[0])
                        // session.selectedSource.selectionFilter.newSelection([quuids[0]])
                    }
                )
            } else {
                // no playlist etc.
                Future.promise(
                    app_window.sessionModel.handleDropFuture(drop.proposedAction, data)
                ).then(function(quuids){})
            }

            if(previousItem) {
                previousItem.insertionFlag = false
                previousItem = null
            }
        }
    }



    DelegateChooser {
        id: chooser
        role: "typeRole"

        XsDelegateMedia {
        }
    }

    Timer {
      id: callback_timer
        function setTimeout(callback, delayTime) {
            callback_timer.interval = delayTime;
            callback_timer.repeat = false;
            callback_timer.triggered.connect(callback);
            callback_timer.triggered.connect(function release () {
                callback_timer.triggered.disconnect(callback); // This is important
                callback_timer.triggered.disconnect(release); // This is important as well
            });
            callback_timer.start();
        }
    }

    Component.onCompleted: {
        updateMedia()
    }

    Connections {
        target: app_window.sessionSelectionModel
        function onCurrentIndexChanged() {
            updateMedia()
        }
    }

    function updateMedia() {
        let index = app_window.sessionSelectionModel.currentIndex

        if(index.valid) {
            // wait for valid index..
            let mind = index.model.index(0, 0, index)
            if(mind.valid) {
                sess.srcModel = app_window.sessionModel
                sess.rootIndex = mind
            } else {
                callback_timer.setTimeout(function() { return function() {
                    updateMedia()
                }}(), 500);
            }
        } else {
            sess.srcModel = null
            sess.rootIndex = null
        }
    }

    // app_window.mediaSelectionModel
    XsMediaMenu {
        id: media_menu
    }

    DelegateModel {
        id: sess
        property var srcModel: app_window.sessionModel
        model: srcModel
        rootIndex: null
        delegate: chooser
    }

    ListView {
        id:list_view

        anchors.fill: parent
        // implicitHeight: contentItem.childrenRect.height
        model: sess
        focus: true
        clip: true
        headerPositioning: ListView.OverlayHeader

        displaced: Transition {
            NumberAnimation {
                properties: "x,y"
                duration: 100
            }
        }

       ScrollBar.vertical: XsScrollBar {
            policy: list_view.visibleArea.heightRatio < 1.0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }
        // so we can place at bottom of list.
        footer: Rectangle {
            color: "transparent"
            width: parent.width
            height: itemHeight
        }

        header: XsMediaPaneListHeader {
            width: parent.width
            selection_indicator_width: selectionIndicatorWidth
            item_height: itemHeight
            thumb_size: thumbSize
            filename_left: filenameLeft

            onThumb_sizeChanged: thumbSize = thumb_size
        }

        z:1

        Connections {
            target: app_window.mediaSelectionModel
            function onCurrentIndexChanged() {
                if(app_window.screenSource.index == app_window.currentSource.index) {
                    list_view.positionViewAtIndex(app_window.mediaSelectionModel.currentIndex.row, ListView.Contain)
                }
            }
        }
    }


    Item {
        id: dragContainer
        anchors.fill: list_view
        anchors.topMargin: 20

        function nearestPowerOf2(n) {
            return 1 << 31 - Math.clz32(n);
        }

        Rectangle {
            id: dragItem
            color: "transparent"
            visible: false
            width: height * 16/9
            height: 100
            property string label: "ITEMS"
            property var source: "qrc:///feather_icons/film.svg"

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
                sourceSize.width: Math.max(128, dragContainer.nearestPowerOf2(width))
                source: dragItem.source
            }

        }

        property alias dragged_media: dragged_media

        ItemSelectionModel {
            id: dragged_media
        }

        Drag.active: moveDragHandler.active
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction

        Connections {
            target: app_window.mediaSelectionModel
            function onSelectionChanged(selected, deselected) {
                let indexs = app_window.mediaSelectionModel.selectedIndexes
                if(indexs.length) {
                    dragItem.label = indexs.length + " items"
                    let item = list_view.itemAtIndex(indexs[0].row)
                    if(item){
                        dragItem.source = item.thumb.source
                            dragItem.grabToImage(function(result){
                            dragContainer.Drag.imageSource = result.url
                        })
                    }
                }
            }
        }

        function startDrag(mode) {
            dragContainer.Drag.supportedActions = mode
            let indexs = app_window.mediaSelectionModel.selectedIndexes

            dragged_media.model = app_window.mediaSelectionModel.model
            dragged_media.select(
                helpers.createItemSelection(app_window.mediaSelectionModel.selectedIndexes),
                ItemSelectionModel.ClearAndSelect
            )

            let ids = []

            // order by row not selection order..

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
                    "xstudio/media-ids": ids.join("\n")
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
                    dragged_media.clear()
                }
            }
        }
    }
}

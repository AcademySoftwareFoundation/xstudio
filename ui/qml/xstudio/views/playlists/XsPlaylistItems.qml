// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15
import QuickFuture 1.0

import xStudio 1.0
import "./delegates"

XsListView { id: playlists

    model: playlistsModel

    property var itemsDataModel: null

    property real itemRowStdHeight: XsStyleSheet.widgetStdHeight + 4
    property real subitemIndent: 48
    property real rightSpacing: showScrollbar ? 12 : 2
    property real flagIndicatorWidth: 4

    Behavior on rightSpacing {NumberAnimation {duration: 150}}

    Rectangle{ id: resultsBg
        anchors.fill: parent
        color: XsStyleSheet.panelBgColor
        z: -1
    }

    DelegateModel {
        id: playlistsModel

        // this is required as "model" doesn't issue notifications on change
        property var notifyModel: theSessionData

        // we use the main session data model
        model: notifyModel

        // point at session 0,0, it's children are the playlists.
        rootIndex: notifyModel.playlistsRootIdx
        delegate: chooser
    }

    DelegateChooser {
        id: chooser
        role: "typeRole"

        DelegateChoice {
            roleValue: "ContainerDivider";

            XsPlaylistDividerDelegate{
                modelIndex: helpers.makePersistent(playlistsModel.modelIndex(index))
                width: playlists.width
            }
        }
        DelegateChoice {
            roleValue: "Playlist";

            XsPlaylistItemDelegate {
                modelIndex: helpers.makePersistent(playlistsModel.modelIndex(index))
                width: playlists.width
            }
        }

    }

    XsLabel {
        anchors.fill: parent
        visible: !playlistsModel.count
        text: 'Click the "+" button or drop files/folders here to add Media'
        color: XsStyleSheet.hintColor
        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
    }

    property var dragTargetIndex

    function isInSelection(idx) {
        return false//mediaSelectionModel.selectedIndexes.includes(mediaListModelData.rowToSourceIndex(idx))
    }

    function computeTargetDropIndex(dropCoordY) {

        var oldDragTarget = dragTargetIndex

        if (dropCoordY < 0 || dropCoordY > height) {
            dragTargetIndex = undefined
            if (oldDragTarget) {
                playlists.itemAtIndex(oldDragTarget.row).isDragTarget = false
            }
            return
        }

        /*var idx = playlists.indexAt(60, dropCoordY + contentY)
        if (idx != -1) {
            var y = playlists.mapToItem(playlists.itemAtIndex(idx), 10, dropCoordY).y
            if (y > itemRowHeight/2 && idx < (mediaList.count-1)) {
                idx = idx+1
            }

            // the index that we are going to drop items into cannot
            // be one of the selected items. Find the nearest unselected
            // index
            if (isInSelection(idx)) {
                var lidx = idx
                while (isInSelection(lidx)) {
                    lidx = lidx-1
                    if (!lidx) break
                }
                var hidx = idx
                while (isInSelection(hidx)) {
                    if (hidx = (playlists.count-1)) break
                    hidx = hidx+1
                }

                if ((idx-lidx) < (hidx-idx)) {
                    idx = lidx
                } else {
                    idx = hidx
                }

            }
            if (mediaList.itemAtIndex(idx)) {
                mediaList.itemAtIndex(idx).isDragTarget = true
                dragTargetIndex = mediaList.itemAtIndex(idx).modelIndex()
            }

        } else {
            dragTargetIndex = undefined
        }

        if (oldDragTarget && oldDragTarget != dragTargetIndex) {
            if (mediaList.itemAtIndex(oldDragTarget.row)) {
                mediaList.itemAtIndex(oldDragTarget.row).isDragTarget = false
            }
        }*/

    }

    XsDragDropHandler {

        id: drag_drop_handler
        targetWidget: playlists
        dragSourceName: "PlayListView"
        dragData: sessionSelectionModel.selectedIndexes

        onDragged: {
            computeTargetDropIndex(mousePosition.y)
        }

        onDropped: {

            if (source == "External URIS") {

                // by providing invalid index, backend will create named
                // playlists for us
                var idx = theSessionData.index(-1,-1)

                Future.promise(
                    theSessionData.handleDropFuture(
                        Qt.CopyAction,
                        {"text/uri-list": data},
                        idx)
                ).then(function(quuids){
                    if (idx) mediaSelectionModel.selectNewMedia(idx, quuids)
                })
            } else if (source == "External JSON") {

                // by providing invalid index, backend will create named
                // playlists for us
                var idx = theSessionData.index(-1,-1)

                Future.promise(
                    theSessionData.handleDropFuture(
                        Qt.CopyAction,
                        data,
                        idx)
                ).then(function(quuids){
                    if (idx) mediaSelectionModel.selectNewMedia(idx, quuids)
                })
            } else if (source == "PlayListView") {

            }
        }

    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: drag_drop_handler.isDragTarget ? Qt.darker(palette.highlight, 3) : "transparent"
        border.width: 1
    }

}

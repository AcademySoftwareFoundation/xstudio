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

import xStudio 1.0

Rectangle {
    id: panel
    color: XsStyle.mainBackground
    property var internalDragDropCursor
    property var dragdropSourcePlaylistUuid

    Keys.onPressed: {
      console.log("move left 2", event);
    }
    focus: true

    // function dragCursorFromMediaList(playlist_uuid, pos) {
    //   var localPos = mapFromGlobal(pos.x, pos.y)
    //   if (localPos.x > 0.0 && localPos.y > 0.0 && localPos.x < width && localPos.y < height) {
    //     internalDragDropCursor = pos
    //     dragdropSourcePlaylistUuid = playlist_uuid
    //   }
    // }

    DropArea {
        anchors.fill: parent

        onEntered: {
            if(drag.hasUrls || drag.hasText) {
                drag.acceptProposedAction()
            }
        }
        onDropped: {
            if(drop.hasUrls) {
                for(var i=0; i < drop.urls.length; i++) {
                  if(drop.urls[i].toLowerCase().endsWith(".xst")) {
                    Future.promise(studio.loadSessionRequestFuture(drop.urls[i])).then(function(result){})
                    app_window.sessionFunction.newRecentPath(drop.urls[i])
                    return
                  }
                }
            }

             let data = {}
             for(let i=0; i< drop.keys.length; i++){
                data[drop.keys[i]] = drop.getDataAsString(drop.keys[i])
             }
            Future.promise(session.handleDropFuture(data)).then(function(quuids){})
        }
    }

    Label {
      text: 'Drop media or folders here\nto create new playlists'
      color: XsStyle.controlTitleColor
      opacity: 0.3
      visible: {
        return session ? session.itemModel.rowCount() == 0 : 1
      }
      anchors.centerIn: parent
      verticalAlignment: Qt.AlignVCenter
      horizontalAlignment: Qt.AlignHCenter
      font.pixelSize: 14

    }

  DelegateChooser {
    id: chooser
    role: "type"
  }


  property var internal_drag_drop_target

  function dropping_items_from_media_list(source_uuid, source_playlist, drag_drop_items, mousePos) {

    var local_pos = mapFromGlobal(mousePos)
    var item = mainContent.the_view.layout.itemAt(local_pos.x + layout.contentX, local_pos.y + layout.contentY)

    if (item && item.content) {
      item.content.dropping_items_from_media_list(source_uuid, source_playlist, drag_drop_items, mousePos)
    }

  }

  function dragging_items_from_media_list(source_uuid, parent_playlist_uuid, drag_drop_items, mousePos) {

    var local_pos = mapFromGlobal(mousePos)
    var item = mainContent.the_view.layout.itemAt(local_pos.x + layout.contentX, local_pos.y + layout.contentY)

    if (item && item.content) {
      if (internal_drag_drop_target != item.content) {
        if (internal_drag_drop_target) {
          internal_drag_drop_target.not_drag_drop_target()
        }
        internal_drag_drop_target = item.content
      }
      internal_drag_drop_target.dragging_items_from_media_list(source_uuid, parent_playlist_uuid, mousePos)
    } else if (internal_drag_drop_target) {
      internal_drag_drop_target.not_drag_drop_target()
      internal_drag_drop_target = null
    }

  }


  property alias mainContent: mainContent

  Item {

    id: mainContent
    property alias the_view: cl.the_view

    z: 10
    focus: true

    anchors {
        left: parent.left
        right: parent.right
        top: parent.top
        bottom: parent.bottom
    }
    ColumnLayout {

        anchors.fill: parent
        spacing: 0
        id: cl
        property alias the_view: the_view

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: the_view
            property alias layout: layout
            // ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            ListView {
              id: layout
              implicitHeight: contentItem.childrenRect.height
              model: session ? session.itemModel : null
              delegate: chooser
              focus: true
              clip: true
            }
        }
    }
  }

}
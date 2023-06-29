// SPDX-License-Identifier: Apache-2.0
import Qt.labs.qmlmodels 1.0
import QtQuick.Controls 2
import QtQuick 2.14

import xStudio 1.0

DelegateChooser {
  role: "type"
  DelegateChoice {
    roleValue: "ContainerGroup";
    XsDraggableItem {
      XsPlaylistSubGroupWidget {
        id: content
        anchors.centerIn: parent
        width: layout.width
        text: object.name
        type: object.type
        backend: object
        parent_backend: playlist.backend
        listView: layout
        listViewIndex: index
      }
      draggedItemParent: playlistMainContent

      onMoveItemRequested: {
        // console.log("move", from,to)
        ListView.view.model.move(from, 1, to);
      }
    }
  }
  DelegateChoice {
    roleValue: "ContainerDivider";
    XsDraggableItem {
      XsPlaylistDividerWidget {
        id: content
        anchors.centerIn: parent
        width: layout.width
        text: object.name
        type: object.type
        backend: object
        parent_backend: playlist.backend
        listView: layout
        listViewIndex: index
      }
      draggedItemParent: playlistMainContent

      onMoveItemRequested: {
        // console.log("move", from,to)
        ListView.view.model.move(from, 1, to);
      }
    }
  }
  DelegateChoice {
    roleValue: "Subset";
    XsDraggableItem {
      property alias content: content
      XsPlaylistSubsetWidget {
        id: content
        anchors.centerIn: parent
        width: layout.width
        text: object.name
        type: object.type
        backend: object
        parent_backend: playlist.backend
        listView: layout
        listViewIndex: index
      }
      draggedItemParent: playlistMainContent

      onMoveItemRequested: {
        // console.log("move", from,to)
        ListView.view.model.move(from, 1, to);
      }
    }
  }
  DelegateChoice {
    roleValue: "Timeline";
    XsDraggableItem {
      property alias content: content
      XsPlaylistTimelineWidget {
        id: content
        anchors.centerIn: parent
        width: layout.width
        text: object.name
        type: object.type
        backend: object
        parent_backend: playlist.backend
        listView: layout
        listViewIndex: index
      }
      draggedItemParent: playlistMainContent

      onMoveItemRequested: {
        // console.log("move", from,to)
        ListView.view.model.move(from, 1, to);
      }
    }
  }
  DelegateChoice {
    roleValue: "ContactSheet";
    XsDraggableItem {
      property alias content: content
      XsPlaylistContactSheetWidget {
        id: content
        anchors.centerIn: parent
        width: layout.width
        text: object.name
        type: object.type
        backend: object
        parent_backend: playlist.backend
        listView: layout
        listViewIndex: index
      }
      draggedItemParent: playlistMainContent

      onMoveItemRequested: {
        // console.log("move", from,to)
        ListView.view.model.move(from, 1, to);
      }
    }
  }
}

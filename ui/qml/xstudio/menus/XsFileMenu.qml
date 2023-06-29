// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Dialogs 1.0

import xstudio.qml.clipboard 1.0
import xStudio 1.0


XsMenu {
    title: qsTr("File")
    XsMenuItem {
        mytext: qsTr("&New Session")
        shortcut: "Ctrl+N"
        onTriggered: app_window.sessionFunction.saveBeforeNewSession()
    }
    XsMenuItem {
        mytext: qsTr("Open Session...")
          shortcut: "Ctrl+O"
          onTriggered: app_window.sessionFunction.saveBeforeOpen()
    }

    XsMenuItem {
        mytext: qsTr("Import Session...")
        onTriggered: app_window.sessionFunction.importSession()
    }

    XsMenu {
      title: qsTr("Recent")
      Repeater {
        model: preferences.recent_history.value
        XsMenuItem {
            mytext: modelData
            onTriggered: app_window.sessionFunction.saveBeforeOpen(modelData)
        }
      }
    }

    XsMenuItem {
        mytext: qsTr("Save Session")
        shortcut: "Ctrl+S"
        onTriggered: app_window.sessionFunction.saveSession()
    }
    XsMenuItem {
        mytext: qsTr("Save Session As...")
        shortcut: "Ctrl+Shift+S"
        onTriggered: app_window.sessionFunction.saveSessionAs()
    }

    XsMenuSeparator {}

    XsMenuItem {
        mytext: qsTr("Add Media...")
        onTriggered: sessionFunction.addMedia(app_window.sessionFunction.firstSelected("Playlist"))
    }

    XsMenuItem {
        mytext: qsTr("Add Media From Clipboard")
        onTriggered: sessionFunction.addMediaFromClipboard(app_window.sessionFunction.firstSelected("Playlist"))
    }

    XsMenuSeparator {}

    XsMenu {
          title: "Export"
          fakeDisabled: false
          XsMenuItem {
              mytext: qsTr("Selected Playlists As Session...")
              onTriggered: app_window.sessionFunction.saveSelectedSessionDialog()
          }
          XsMenuItem {
              mytext: qsTr("Notes As CSV...")
              onTriggered: app_window.sessionFunction.exportNotesCSV()
          }
          XsMenuItem {
              mytext: qsTr("Notes + Drawings...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("CDL...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("EDL...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("OTIO...")
              enabled: false
          }
    }
    XsMenu {
          title: "Export Media"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("Current Timeline Sequence....")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Selected Media Items...")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Snapshot Current Frame...")
              enabled: false
          }
    }
    XsMenu {
          title: "Gather Media"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("Selected Playlists....")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Selected Media...")
              enabled: false
          }
    }
    XsMenu {
          title: "Relink Media"
          fakeDisabled: true
          XsMenuItem {
              mytext: qsTr("All Missing Files....")
              enabled: false
          }
          XsMenuItem {
              mytext: qsTr("Selected Missing Files...")
              enabled: false
          }
    }
    XsMenuItem {
        mytext: qsTr("Copy Session Link")
        enabled: true
        onTriggered: app_window.sessionFunction.copySessionLink()
    }
    XsMenuItem {
        mytext: qsTr("Refresh All Metadata")
        enabled: false
    }

    XsMenuSeparator {}
    XsMenuItem {
        mytext: qsTr("&Quit")
        shortcut: "Ctrl+Q"
        onTriggered: {
          app_window.close()
        }
    }
}

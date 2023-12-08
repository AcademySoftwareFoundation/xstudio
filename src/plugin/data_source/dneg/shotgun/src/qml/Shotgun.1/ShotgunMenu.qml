// SPDX-License-Identifier: Apache-2.0
import xStudio 1.0

XsMenu {
    title: qsTr("Shotgun Playlists")

    XsMenuItem {
        mytext: qsTr("Create Selected Shotgun Playlists...")
        onTriggered: sessionFunction.object_map["ShotgunRoot"].create_playlist()
    }

    XsMenuItem {
        mytext: qsTr("Update Selected Shotgun Playlists")
        onTriggered: sessionFunction.object_map["ShotgunRoot"].update_playlist()
    }

    XsMenuItem {
        mytext: qsTr("Refresh Selected Shotgun Playlists")
        onTriggered: sessionFunction.object_map["ShotgunRoot"].refresh_playlist()
    }

    XsMenuSeparator {}

    XsMenuItem {
        mytext: qsTr("Authentication...")
        onTriggered: sessionFunction.object_map["ShotgunRoot"].do_authentication()
    }
    // XsMenuItem {
    //     mytext: qsTr("Preferences...")
    //     onTriggered: session.object_map["ShotgunPlaylist"].do_preferences()
    // }
}

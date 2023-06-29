// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

XsMenu {
    title: qsTr("Edit")
    XsMenuItem {
        mytext: qsTr("Copy")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Cut")
        enabled: false
    }
    XsMenuItem {
        mytext: qsTr("Paste")
        enabled: false
    }
    XsMenuSeparator {}
    XsMenuItem {
        mytext: qsTr("Copy Selected File Names")
        onTriggered: app_window.sessionFunction.copyMediaFileName(true)
    }
    XsMenuItem {
        mytext: qsTr("Copy Selected File Paths")
        onTriggered: app_window.sessionFunction.copyMediaFilePath(true)
    }
}



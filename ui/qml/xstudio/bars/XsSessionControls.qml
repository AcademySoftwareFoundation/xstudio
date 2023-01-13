// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3

import xStudio 1.0

Rectangle {

    // The main row with play controls.
    height: XsStyle.sessionBarHeight*opacity
    visible: opacity !== 0
    opacity: 1

    Behavior on opacity {
        NumberAnimation { duration: XsStyle.presentationModeAnimLength }
    }

    color: XsStyle.mainBackground

    gradient: Gradient {
       orientation: Gradient.Horizontal
       GradientStop { position: 0.2; color: XsStyle.controlBackground}
       GradientStop { position: 0.5; color: XsStyle.mainBackground}
       GradientStop { position: 0.8; color: XsStyle.controlBackground}
    }

    function toggleNotes(show=undefined) {
        mediaToolsTray.toggleNotesDialog(show)
    }
    function toggleDrawDialog(show=undefined) {
        mediaToolsTray.toggleDrawDialog(show)
    }

    Rectangle {

        id: myRowLayout
        anchors.fill: parent
        color: "transparent"

        /*onWidthChanged: {
            if(width < (mediaToolsTray.layout.implicitWidth + sessionSettingsTray.layout.implicitWidth)) {
                mediaToolsTray.collapsed = true
                sessionSettingsTray.collapsed = true
            } else {
                mediaToolsTray.collapsed = false
                sessionSettingsTray.collapsed = false
            }
        }*/

        XsMediaToolsTray {
            id: mediaToolsTray
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }

        XsLayoutTray {
            id: layoutTray
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }

        XsPaneVisibilityTray {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            //width:300
            id: paneVisibilityTray
        }
    }
}

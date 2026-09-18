// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0
import xstudio.qml.models 1.0

Rectangle {

    XsModuleData {
        id: decklink_settings
        modelDataName: "Decklink Settings"
    }
    property alias decklink_settings: decklink_settings

    XsAttributeValue {
        id: __sdiRunning
        attributeTitle: "Enabled"
        model: decklink_settings
    }
    property alias sdiRunning: __sdiRunning.value

    anchors.fill: parent

    XsIcon {
        anchors.centerIn: parent
        source: "qrc:/bmd_icons/sdi-logo.svg"
        width: 24
        height: 24

    }
    color: sdiRunning ? "green" : "transparent"
    opacity: sdiRunning ? 0.5 : 1.0
}
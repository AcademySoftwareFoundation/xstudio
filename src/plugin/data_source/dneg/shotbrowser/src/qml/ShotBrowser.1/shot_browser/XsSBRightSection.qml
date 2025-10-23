// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.helpers 1.0

ColumnLayout {
    property alias delegateModel: view.delegateModel
    spacing: panelPadding

    XsSBR1Tools{
        Layout.fillWidth: true
    }

    Rectangle{
        color: panelColor
        Layout.fillWidth: true
        Layout.fillHeight: true


    XsHotkeyArea {
        id: hotkey_area
        anchors.fill: parent
        context: "ShotBrowser" + panel
        focus: false
    }

    Keys.forwardTo: hotkey_area

    XsHotkey {
        context: "ShotBrowser" + panel
        sequence:  "Ctrl+A"
        name: "Select All"
        description: "Select All"
        onActivated: {
            versionResultPopup.popupDelegateModel = delegateModel
            versionResultPopup.selectAll()
        }
        componentName: "ShotBrowser"
    }


        XsSBR2Views{
            id: view
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.topMargin: 2
            anchors.rightMargin: rightSpacing ? 0 : 2
            anchors.bottomMargin: 2
        }
    }

    XsSBR3Actions{
        Layout.fillWidth: true
        Layout.minimumHeight: XsStyleSheet.widgetStdHeight
        Layout.maximumHeight: XsStyleSheet.widgetStdHeight
    }
}

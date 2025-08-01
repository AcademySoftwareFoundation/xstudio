// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0
import xstudio.qml.viewport 1.0

XsDialogModal {
    width: 600
    height: 560

    property int hotkey_view_width: 300
    property int hotkey_view_height: 30

    XsHotkeysInfo {
        id: hotkeys_info
    }

    Flickable {

        id: media_list
        property int comlumns: width/hotkey_view_width < 1 ? 1 : width/hotkey_view_width
        property int rows: repeater.count / comlumns
        contentHeight: rows*hotkey_view_height
        flickableDirection: Flickable.VerticalFlick
        anchors.fill: parent

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
        }

        Rectangle {

            anchors.fill: parent
            anchors.margins: 20
            color: "transparent"

            Repeater {

                id: repeater
                model: hotkeys_info

                XsHotkeyView {
                    width: hotkey_view_width
                    height: hotkey_view_height
                    x: Math.floor(index/media_list.rows)*hotkey_view_width
                    y: (index%media_list.rows)*hotkey_view_height
                }
            }
        }
    }
}

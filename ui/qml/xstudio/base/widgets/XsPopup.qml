// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

Popup {
    id: popup
    focus: true

    x: Math.round((parent.width - width) / 2)
    y: -height

    function toggleShow() {
        if(visible) {
            close()
        }
        else{
            open()
        }
    }

    background: Rectangle {
        id: bgrect
        anchors.fill: parent
        border {
            color: XsStyle.menuBorderColor
            width: XsStyle.menuBorderWidth
        }
        color: XsStyle.mainBackground
        radius: XsStyle.menuRadius
    }
}

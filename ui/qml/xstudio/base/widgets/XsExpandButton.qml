// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick 2.14
import QtGraphicalEffects 1.12
import QtQml 2.14

import xStudio 1.0

Image {
    property bool expanded: false

    source: "qrc:///feather_icons/chevron-right.svg"
    sourceSize.height: height
    sourceSize.width:  width
    smooth: true
    transformOrigin: Item.Center
    rotation: expanded ? 90 : 0
    MouseArea {
        anchors.fill: parent
        onClicked: expanded = !expanded
    }

    layer {
        enabled: true
        effect: ColorOverlay {
            color: XsStyle.controlColor
        }
    }
}
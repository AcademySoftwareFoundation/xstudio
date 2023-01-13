// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1

Rectangle{ id: frame


    property real framePadding: 6
    property real frameWidth: 1
    property real frameRadius: 2
    property color frameColor: palette.base


    MouseArea{
        anchors.fill: parent; onPressed: focus=true
    }

    x: framePadding
    y: framePadding

    width: parent.width - framePadding*2
    height: parent.height - framePadding*2

    color: "transparent"
    radius: frameRadius
    border.color: frameColor
    border.width: frameWidth

}




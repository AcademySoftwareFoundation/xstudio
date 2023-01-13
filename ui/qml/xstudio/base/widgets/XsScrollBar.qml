// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1

ScrollBar { id: widget
    property color thumbColorPressed: palette.highlight
    property color thumbColorHovered: palette.text
    property color thumbColorNormal: "light grey"

    property real thumbWidth: thumb.implicitWidth

    padding: 1
    minimumSize: 0.1

    contentItem:
    Rectangle { id: thumb
        implicitWidth: 5
        implicitHeight: 100
        radius: width/1.1
        color: widget.pressed ? thumbColorPressed: widget.hovered? thumbColorHovered: thumbColorNormal
        opacity: 0.7
    }
}
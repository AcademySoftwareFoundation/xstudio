// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls.Basic
import QtQuick

import xStudio 1.0

ScrollBar { id: widget

    property color thumbColorPressed: palette.highlight
    property color thumbColorHovered: palette.text
    property color thumbColorNormal: XsStyleSheet.hintColor

    property real thumbWidth: thumb.implicitWidth
    property bool animatedGlow: false

    width: 12
    padding: 2
    minimumSize: 0.1

    snapMode: ScrollBar.SnapOnRelease

    contentItem:
    Rectangle { id: thumb
        implicitWidth: 5
        implicitHeight: 5
        radius: width/1.1
        color: widget.pressed ? thumbColorPressed: thumbColorNormal
        // opacity: hovered || active ? 0.8 : 0.4
        border.color: widget.pressed ? "transparent" : Qt.lighter(thumbColorNormal, 1.0+Math.abs(borderGlow-1.0))
        border.width: 1
        property real borderGlow: 1.0

        // For long pop-up menus the scrollbar can sometimes be hard to spot.
        // This animation on the border colour makes it more obvious without
        // being too loud.
        PropertyAnimation {
            target: thumb
            property: "borderGlow"
            from: 0
            to: 2
            duration: 2000
            loops: Animation.Infinite
            running: animatedGlow
        }

    }


}
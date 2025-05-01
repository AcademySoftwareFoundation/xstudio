// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

import xStudio 1.0

Label{
    // id: widget
    // property color textColorNormal: palette.text
    // property var textElide: widget.elide
    // property real textWidth: metrics.width
    // property bool shadow: false
    // property bool hovered: mArea.containsMouse
    // text: ""

    color: palette.text
    elide: Text.ElideRight
    wrapMode: Text.WordWrap

    opacity: enabled ? 1.0 : 0.5

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    /*MultiEffect {
        shadowEnabled: shadow
        anchors.fill: widget
        source: widget
        shadowColor: "#ff000000"
        shadowVerticalOffset: 4
        shadowHorizontalOffset: 0
        shadowScale: 1.4
        shadowBlur: 0.2
        z: -1
    }*/

    // TextMetrics {
    //     id: metrics
    //     font: widget.font
    //     text: widget.text
    // }

    //
    // MouseArea{
    //      id: mArea
    //      anchors.fill: parent
    //      hoverEnabled: true
    //      propagateComposedEvents: true
    // }

}
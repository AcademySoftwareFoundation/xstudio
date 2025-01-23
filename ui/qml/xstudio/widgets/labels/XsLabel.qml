// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0

Label{
    id: widget

    property color textColorNormal: palette.text
    property var textElide: widget.elide
    property real textWidth: metrics.width
    property bool shadow: false
    property bool hovered: mArea.containsMouse

    text: ""
    color: textColorNormal
    elide: Text.ElideRight
    wrapMode: Text.WordWrap
    opacity: enabled ? 1.0 : 0.5

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    // width: parent.width
    // height: parent.height
    // topPadding: 2
    // bottomPadding: 2
    // leftPadding: 20
    // rightPadding: 20

    DropShadow {
        anchors.fill: widget
        horizontalOffset: 0
        verticalOffset: 0
        radius: 1.0
        samples: 4
        color: "#ff000000"
        source: widget
        visible: shadow
        z: -1
    }

    TextMetrics {
        id: metrics
        font: widget.font
        text: widget.text
    }

    MouseArea{
         id: mArea
         anchors.fill: parent
         hoverEnabled: true
         propagateComposedEvents: true
    }

}
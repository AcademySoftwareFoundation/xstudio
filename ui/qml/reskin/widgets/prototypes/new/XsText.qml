// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudioReskin 1.0

Text {
    id: widget

    property color textColorNormal: palette.text
    property var textElide: widget.elide
    property real textWidth: metrics.width

    text: ""
    color: textColorNormal 

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    // topPadding: 2
    // bottomPadding: 2
    // leftPadding: 20
    // rightPadding: 20
    elide: Text.ElideRight
    width: parent.width
    height: parent.height

    TextMetrics {
        id: metrics
        font: widget.font
        text: widget.text
    }

    // XsToolTip{
    //     text: widget.text
    //     visible: widget.hovered && parent.truncated
    //     width: metricsDiv.width == 0? 0 : widget.width
    //     // x: 0 //#TODO: flex/pointer
    // }
}
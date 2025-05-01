// SPDX-License-Identifier: Apache-2.0
import QtQuick



import xStudio 1.0

Text {
    id: widget

    property color textColorNormal: palette.text
    property var textElide: widget.elide
    property real textWidth: metrics.advanceWidth

    opacity: enabled ? 1.0 : 0.5
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

    //elide: Text.ElideRight
    wrapMode: Text.NoWrap

    // width: parent.width
    // height: parent.height

    TextMetrics {
        text: widget.text
        id: metrics
        font: widget.font
    }

}
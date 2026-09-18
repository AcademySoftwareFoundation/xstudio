// SPDX-License-Identifier: Apache-2.0
import QtQuick

import xStudio 1.0

Text {
    id: control
    property alias textWidth: metrics.advanceWidth

    opacity: enabled ? 1.0 : 0.5
    text: ""
    color: XsStyleSheet.primaryTextColor

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    wrapMode: Text.NoWrap

    TextMetrics {
        id: metrics

        text: control.text
        font: control.font
    }
}
// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic

import xStudio 1.0

Label{
    color: XsStyleSheet.primaryTextColor
    elide: Text.ElideRight
    wrapMode: Text.WordWrap

    opacity: enabled ? 1.0 : 0.5

    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
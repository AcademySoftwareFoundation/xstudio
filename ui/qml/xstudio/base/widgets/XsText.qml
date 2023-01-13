// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1

Text{
    id: widget

    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"

    font.pixelSize: XsStyle.menuFontSize
    font.family: XsStyle.fontFamily
    color: textColorNormal
    elide: Text.ElideRight
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    width: parent.width
}




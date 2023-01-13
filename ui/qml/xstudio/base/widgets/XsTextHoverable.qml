// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1

Control {
    id: widget

    property alias text: displayText.text
    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"

    XsText{ id: displayText
        color: parent.hovered? textColorPressed: textColorNormal
        width: parent.width
        anchors.top: parent.top
        font.italic: true
    }

}




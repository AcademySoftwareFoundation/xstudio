// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14

import xStudio 1.1

Control {
    id: widget

    property alias text: checkBox.text
    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"

    property alias model: textField.text
    property alias placeholderText: textField.placeholderText
    property alias inputMethodHints: textField.inputMethodHints

    XsCheckbox{ id: checkBox
        forcedTextHover: parent.hovered
        width: 100
        height: parent.height
        leftPadding: 12
    }

    XsTextField{id: textField
        height: parent.height
        anchors.left: checkBox.right
        anchors.right: parent.right
        anchors.rightMargin: 12
    }

}




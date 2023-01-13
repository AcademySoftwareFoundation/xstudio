// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.15

import xStudio 1.1

Control {
    id: widget

    property alias text: textHolder.text
    property color textColorPressed: palette.text
    property color textColorNormal: "light grey"

    property alias model: comboBox.model
    property alias textRole: comboBox.textRole
    property alias editable: comboBox.editable
    property alias currentIndex: comboBox.currentIndex
    property real comboBoxLeftPadding: 6 //similar to checkbox

    Text{ id: textHolder
        text: ""
        font.pixelSize: XsStyle.menuFontSize
        font.family: XsStyle.fontFamily
        color: widget.hovered? textColorPressed: textColorNormal
        width: parent.width
        height: parent.height/2
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
    }

    XsComboBox{ id: comboBox
        enabled: parent.enabled
        height: parent.height/2
        anchors.left: parent.left
        anchors.leftMargin: comboBoxLeftPadding
        anchors.right: parent.right
        anchors.top: textHolder.bottom
    }
}




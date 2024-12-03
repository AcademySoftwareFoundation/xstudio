// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import xStudio 1.0

Rectangle{
    property alias text: textDiv.text
    property alias textDiv: textDiv
    property alias textColor: textDiv.color

    color: XsStyleSheet.widgetBgNormalColor

    XsText{ id: textDiv //XsTextInput
        text: ""
        color: palette.text
        font.bold: true
        width : parent.width - panelPadding*2
        height: parent.height
        anchors.verticalCenter: parent.verticalCenter
        // enabled: false
        // readOnly: true
        leftPadding: panelPadding
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
    }
}
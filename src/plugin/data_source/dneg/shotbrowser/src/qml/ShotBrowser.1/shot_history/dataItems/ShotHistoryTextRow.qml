// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0

Rectangle{

    property alias text: textDiv.text
    property alias textDiv: textDiv
    property alias textColor: textDiv.color

    color: XsStyleSheet.widgetBgNormalColor

    XsText{ id: textDiv
        text: ""
        color: XsStyleSheet.hintColor
        // height: parent.height
        anchors.verticalCenter: parent.verticalCenter
        anchors.fill: parent
        elide: Text.ElideRight
    }
}
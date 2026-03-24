// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0


Rectangle { id: widget

    property var filteredCount: "-"
    property var totalCount: "-"

    property color bgColor: XsStyleSheet.panelBgColor
    property color borderColor: XsStyleSheet.menuBorderColor

    implicitWidth: 100
    implicitHeight: 28
    border.color: borderColor
    border.width: 1
    color: bgColor

    XsText{
        text: filteredCount + " / " +  totalCount
        width: parent.width
        anchors.centerIn: parent
    }
}
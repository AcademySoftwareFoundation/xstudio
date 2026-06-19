// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import xStudio 1.0

import "."

// ── Folder path header (spans full row) ────────────
Rectangle {
    color: "#1a1a1a"
    border.color: isHovered ? XsStyleSheet.accentColor : "transparent"
    border.width: 1
    XsIcon {
        id: folderIcon
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left; anchors.leftMargin: 10
        width: 24; height: 24
        source: "qrc:/icons/folder.svg"
    }
    XsText {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: folderIcon.right; anchors.leftMargin: 10
        text: modelData.path
        font.bold: true
        elide: Text.ElideLeft
        font.pixelSize: 14
    }
    property bool isHovered: underMouseIndex === index

}

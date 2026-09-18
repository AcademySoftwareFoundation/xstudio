// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts
import QtQml.Models 2.15

import xstudio.qml.helpers 1.0
import xStudio 1.0
import "."

XsPlaylistItemBase {
    property var iconSource: "qrc:/icons/list_alt.svg"

    indent: false
    width: parent.width;
    isDivider: true

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: indent ? subitemIndent + XsStyleSheet.panelPadding * 3 : rightSpacing
        anchors.rightMargin: rightSpacing
        spacing: 10

        Rectangle{
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true

            height: 1;
            color: XsStyleSheet.hintColor
        }

        XsText {
            Layout.alignment: Qt.AlignVCenter

            text: nameRole
            color: XsStyleSheet.hintColor
        }

        Rectangle{
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true

            height: 1;
            color: XsStyleSheet.hintColor
        }
    }
}
// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts
import QtQml.Models 2.15

import xstudio.qml.helpers 1.0
import xStudio 1.0
import "."

XsPlaylistItemBase {

    id: contentDiv
    width: parent.width;

    property var iconSource: "qrc:/icons/list_alt.svg"
    property bool indent: false
    isDivider: true

    RowLayout {

        id: layout
        anchors.fill: contentDiv
        anchors.leftMargin: indent ? subitemIndent+itemPadding*6 : rightSpacing
        anchors.rightMargin: rightSpacing
        spacing: 10

        Rectangle{
            height: 1;
            color: hintColor
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
        }

        XsText {

            id: textDiv
            text: nameRole
            color: hintColor
            Layout.alignment: Qt.AlignVCenter

        }

        Rectangle{
            height: 1;
            color: hintColor
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
        }

    }

}
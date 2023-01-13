// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12

import xStudio 1.0

MenuSeparator {
    property string mytext: ""
    contentItem: Rectangle {
        implicitHeight: mytext===""?1:10
        border.width: 0
        color: mytext===""?XsStyle.menuBorderColor:"transparent"
        Rectangle {
            anchors.left:parent.left
            anchors.right:parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: mytext!==""
            implicitHeight: 1
            border.width: 0
            color:XsStyle.menuBorderColor
        }
        Label {
            visible: mytext!==""
            text: mytext
            background: Rectangle{color:XsStyle.mainBackground}
            anchors.left:parent.left
            leftPadding: 3
            rightPadding: 3
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            color: XsStyle.menuBorderColor
            font.capitalization: Font.AllUppercase
            font.family: XsStyle.fontFamily
            font.hintingPreference: Font.PreferNoHinting
            font.pixelSize: 10
        }
    }
}

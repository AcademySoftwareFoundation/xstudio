// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudio 1.0

Item{

    id: dateDiv
    property var leftMargin: 12
    property var text

    XsText{
        property bool invalidValue: dateDiv.text == undefined || dateDiv.text == "null"
        text: invalidValue ? "N/A" : dateDiv.text
        x: position == "left" ? leftMargin : (parent.width-width)/2
        anchors.verticalCenter: parent.verticalCenter
        font.weight: isActive? Font.ExtraBold : Font.Normal
        color: invalidValue ? XsStyleSheet.widgetBgNormalColor : palette.text
    }

    Rectangle{
        width: headerThumbWidth;
        height: parent.height
        anchors.right: parent.right
        color: headerThumbColor
    }

    clip: true

}

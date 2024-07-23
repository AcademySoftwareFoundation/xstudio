// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudioReskin 1.0

Item{ 
    
    id: dateDiv

    property var raw_text

    property var text: regex ? (raw_text ? raw_text.replace(regex, format_out) : "-") : (raw_text ? raw_text : "-")

    property var regex: format_regex ? new RegExp(format_regex) : undefined

    XsText{ 
        text: "" + dateDiv.text
        width: parent.width - itemPadding*2
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        font.weight: isActive? Font.ExtraBold : Font.Normal
        color: isActive && highlightTextOnActive? highlightColor : palette.text
    }
    Rectangle{
        width: headerThumbWidth; 
        height: parent.height
        anchors.right: parent.right
        color: bgColorPressed
    }

} 

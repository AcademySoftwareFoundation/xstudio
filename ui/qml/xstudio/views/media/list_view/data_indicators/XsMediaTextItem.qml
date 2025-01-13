// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudio 1.0

import "../../common_delegates"

Item{

    id: dateDiv
    property var leftMargin: 12
    property var text
    property var isIndex: false
    property bool showBorder: actorUuidRole == currentPlayhead.mediaUuid

    Component {
        id: highlight
        XsMediaThumbnailHighlight {
            anchors.fill: parent
            z: 100
            borderThickness: 5
        }
    }
    Loader {
        id: loader
        anchors.fill: parent
    }
    onShowBorderChanged: {
        if (showBorder && isIndex) loader.sourceComponent = highlight
        else loader.sourceComponent = undefined
    }



    XsText{
        property bool invalidValue: dateDiv.text == undefined || dateDiv.text == "null"
        text: invalidValue ? "N/A" : dateDiv.text
        x: position == "left" ? leftMargin : (parent.width-width)/2
        anchors.verticalCenter: parent.verticalCenter
        font.weight: isActive? Font.ExtraBold : Font.Normal
        color: invalidValue ? XsStyleSheet.widgetBgNormalColor : palette.text
        
        horizontalAlignment: position == "left" ? Text.AlignLeft : Text.AlignHCenter
        elide: Text.ElideMiddle
        width: parent.width - leftMargin*2
    }

    Rectangle{
        width: headerThumbWidth;
        height: parent.height
        anchors.right: parent.right
        color: headerThumbColor
    }

    clip: true

}

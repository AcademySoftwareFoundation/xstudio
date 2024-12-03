// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtGraphicalEffects 1.15
import QtQml.Models 2.14
import xStudio 1.0

import xstudio.qml.helpers 1.0
import "."

Rectangle{

    id: thumbnailDiv
    height: width / (16/9) //to keep 16:9 ratio
    color: "transparent"
    property bool showBorder: false
    property bool forcedHover: hovered
    property int highlightBorderThickness: 5
    clip: true

    XsText{
        text: mediaStatusRole == "Online" ? "No\nImage" : "Media\nNot Found"
        width: parent.width - itemPadding*2
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        font.weight: isActive? Font.ExtraBold : Font.Normal
        visible: thumbnailImageRole == undefined
        color: hintColor
    }

    XsImagePainter {
        id: thumbnailImgDiv
        anchors.fill: parent
        image: thumbnailImageRole
        clip: true
        fill: true
    }


    XsImage{

        id: play_image
        anchors.centerIn: parent
        height: parent.height*0.85
        width: height

        source: "qrc:/icons/play_circle.svg"
        visible: forcedHover
        imgOverlayColor: playOnClick ? palette.text : palette.highlight

        property var mx: mouseX
        property var my: mouseY
        onMxChanged: testPos()
        onMyChanged: testPos()
        function testPos() {
            if (forcedHover) {
                var pos = mouseArea.mapToItem(
                    play_image,
                    mouseX,
                    mouseY
                    )
                playOnClick = pos.x > 0 && pos.x < width && pos.y > 0 && pos.y < height
            } else {
                playOnClick = false
            }
        }

    }

    Component {
        id: highlight

        XsMediaThumbnailHighlight {
            anchors.fill: parent
            z: 100
            borderThickness: highlightBorderThickness
        }
    }

    Loader {
        id: loader
        anchors.fill: thumbnailImgDiv
    }

    onShowBorderChanged: {
        if (showBorder) loader.sourceComponent = highlight
        else loader.sourceComponent = undefined
    }

    Rectangle{
        width: headerThumbWidth;
        height: parent.height
        anchors.right: parent.right
        color: XsStyleSheet.widgetBgNormalColor
    }

}
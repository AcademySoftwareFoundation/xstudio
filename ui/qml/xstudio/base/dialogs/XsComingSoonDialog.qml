// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.2

import xStudio 1.0

XsWindow {
    property alias image: myImage.source
    property alias icon: myIcon.source
    property alias comment: myText.text

    keepCentered: true
    centerOnOpen: true

    Component.onCompleted: {
        if(myImage.sourceSize.height !== 0) {
            myIconRect.visible = false
            myText.visible = false
            height = myImage.sourceSize.height
            width = myImage.sourceSize.width
        } else {
            imagerect.visible = false
            width = myText.width + 40
        }
    }

    Rectangle {
        id: imagerect
        color: "transparent"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Image {
            id: myImage
            anchors.fill: parent
        }
    }

    Rectangle {
        id: myIconRect
        anchors.top: parent.top
        anchors.bottom: myText.top
        anchors.topMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        color: "transparent"

        XsColoredImage {
            anchors.fill:parent
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            id: myIcon
            height: 140
            width: 140
        }
    }
    Text {
        id: myText
        color: XsStyle.mainColor
        anchors.bottomMargin: 20
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtMultimedia

import xStudio 1.0

MouseArea {
    id: playerbox
    property var images: []
    property int index: 0
    hoverEnabled: true

    onEntered: forceActiveFocus(playerbox)

    signal eject()

    onVisibleChanged: {
        if(visible)
            forceActiveFocus(playerbox)
    }

    onImagesChanged: index = 0

    onClicked: {
        if(images.length) {
            if(index == images.length-1)
                index = 0
            else
                index++
        }
    }

    function next() {
        if(images.length) {
            if(index < images.length-1)
                index++
        }
    }

    function previous() {
        if(images.length) {
            if(index)
                index--
        }
    }

    function exitPlayback() {
        playerbox.images = []
        eject()
    }

    Keys.onEscapePressed: playerbox.exitPlayback()
    Keys.onSpacePressed: playerbox.next()

    Rectangle {
        anchors.fill: parent
        color: "black"

        Image {
            anchors.fill: parent
            source: images.length ? images[index][1] : ""
            fillMode: Image.PreserveAspectFit
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 4
            height: parent.height / 20
            color: "#AA000000"


            RowLayout {
                spacing:4
                anchors.fill: parent

                MouseArea {
                    Layout.fillHeight: true
                    Layout.preferredWidth: height
                    hoverEnabled: true

                    onClicked: playerbox.exitPlayback()

                    XsIcon{
                        anchors.fill: parent
                        source: "qrc:/icons/close.svg"
                        imgOverlayColor: parent.containsMouse ? palette.highlight : XsStyleSheet.primaryTextColor
                    }
                }

                MouseArea {
                    Layout.fillHeight: true
                    Layout.preferredWidth: height
                    hoverEnabled: true
                    onClicked: index && playerbox.previous()

                    XsIcon {
                        anchors.fill: parent
                        rotation: 180
                        source: "qrc:/icons/chevron_right.svg"
                        imgOverlayColor: (parent.containsMouse && index ? palette.highlight : XsStyleSheet.primaryTextColor)
                        opacity: index ? 1.0 : 0.5
                    }
                }

                MouseArea {
                    Layout.fillHeight: true
                    Layout.preferredWidth: height
                    hoverEnabled: true

                    onClicked: index < images.length-1 && playerbox.next()

                    XsIcon{
                        anchors.fill: parent
                        source: "qrc:/icons/chevron_right.svg"

                        imgOverlayColor: (parent.containsMouse && index != images.length-1 ? palette.highlight : XsStyleSheet.primaryTextColor)
                        opacity: index != images.length-1 ? 1.0 : 0.5
                    }
                }

                XsText {
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    text: images.length ? images[index][0] : "No Images"
                }
            }
        }
    }
}
// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5

import xStudio 1.0

Item {

    property alias powerDownImage:powerDownImage
    property int shutdownDuration: 500
    property bool okToCloseWindow: false
    property var closing_widget: undefined
    anchors.fill: parent
    visible: false
    id: power_down

    XsTimer {id: myTimer}

    function do_anim(capture_widget) {

        if (preferences.do_shutdown_anim.value & !okToCloseWindow) {
            closing_widget = capture_widget
            powerDownImage.height = closing_widget.height
            powerDownImage.width = closing_widget.width
            closing_widget.grabToImage(function(result) {
                powerDownImage.source = result.url;
            });
            okToCloseWindow = true
            color = "black"
            visible = true
            myTimer.setTimeout(Qt.quit, shutdownDuration)
            return true
        } else {
            return false
        }
    
    }

    Item {
        visible: power_down.visible
        anchors.fill: parent
        anchors.centerIn: parent
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#000" }
                GradientStop { position: 0.5; color: "#222" }
                GradientStop { position: 1.0; color: "#000" }
            }
        }
        Image {
            id: powerDownImage
            anchors.centerIn: parent
        }
        Label {
            id: powerDownLabel
            anchors.fill: parent
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            text: Qt.application.name
            color: "#888888"
            opacity: .5
            font.pixelSize: 100
            font.bold: true
            rotation: -20
        }
    }

    Rectangle {
        id: powerDownRect
        height: 5
        width: 5
        radius: 5
        color: "#888888"
        visible: power_down.visible
        opacity: 0
        anchors.centerIn: parent
        onVisibleChanged: {
            if (visible) {
                seqAnim.running = true
            }
        }

        Rectangle {
            width: 5
            height: 5
            radius: 5
            color: "#fff"
            anchors.left: powerDownRect.left
            anchors.verticalCenter: powerDownRect.verticalCenter
        }
        Rectangle {
            width: 5
            height: 5
            radius: 5
            color: "#fff"
            anchors.right: powerDownRect.right
            anchors.verticalCenter: powerDownRect.verticalCenter
        }

        SequentialAnimation {
            id: seqAnim
            running: false
            NumberAnimation { target: powerDownRect; property: "opacity"; to: 1; duration:shutdownDuration*0.01 }
            ParallelAnimation {
                NumberAnimation {target:powerDownImage; property:"height"; to: 5; duration: shutdownDuration*0.33 }
                NumberAnimation { target: powerDownRect; property: "width"; to: 5; duration: shutdownDuration*0.33 }
            }
            ParallelAnimation {
                NumberAnimation {target:powerDownImage; property:"width"; to: 5; duration: shutdownDuration*0.33 }
                NumberAnimation { target: powerDownRect; property: "width"; to: 5; duration: shutdownDuration*0.33 }
            }
            NumberAnimation { target: powerDownRect; property: "opacity"; to: .35; duration: shutdownDuration*0.33 }
        }
    }
}

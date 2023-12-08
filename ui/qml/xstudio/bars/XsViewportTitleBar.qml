// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5

import xStudio 1.0

Rectangle {

    color: XsStyle.mainBackground
    height: XsStyle.viewerTitleBarHeight*opacity
    implicitHeight: XsStyle.viewerTitleBarHeight*opacity

    property alias mediaToolsTray: mediaToolsTray
    visible: opacity != 0.0

    Behavior on opacity {
        NumberAnimation {duration: 200}
    }

    XsMediaToolsTray {
        id: mediaToolsTray
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: XsStyle.viewerTitleBarButtonsMargin
    }

    Rectangle {

        x: Math.max(parent.width/2-width/2, mediaToolsTray.x+mediaToolsTray.width)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: label_metrics.width
        color: "transparent"

        Text {

            id: label
            text: app_window.mediaImageSource.fileName
            color: XsStyle.controlColor
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignLeft
            anchors.fill: parent

            font {
                pixelSize: XsStyle.mediaInfoBarFontSize+6
                family: XsStyle.controlTitleFontFamily
                hintingPreference: Font.PreferNoHinting
            }
        }

        TextMetrics {
            id:     label_metrics
            font:   label.font
            text:   label.text
        }

    }

    Rectangle {

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 30
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#00bb7700"}
            GradientStop { position: 1.0; color: XsStyle.mainBackground}
        }
        z: 1
    }

}
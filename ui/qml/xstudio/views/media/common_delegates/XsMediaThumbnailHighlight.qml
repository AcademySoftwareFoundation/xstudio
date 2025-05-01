// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Effects

Item {
    anchors.fill: thumbnailImgDiv
    visible: showBorder
    z: 100

    property int borderThickness: 6

    Rectangle {
        id: rect
        color: "transparent"
        border.color: palette.highlight
        border.width: borderThickness/2
        anchors.fill: parent
        visible: false
    }

    MultiEffect {
        source: rect
        anchors.fill: parent
        blurEnabled: true
        blurMax: borderThickness*2
        blur: 1
    }

}
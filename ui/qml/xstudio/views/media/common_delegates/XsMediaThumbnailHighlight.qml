// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtGraphicalEffects 1.15

Item {
    anchors.fill: thumbnailImgDiv
    visible: showBorder
    z: 100
    property int borderThickness: 5
    LinearGradient {
        width: parent.width
        height: borderThickness
        start: Qt.point(0,0)
        end: Qt.point(0,height)
        cached: true
        gradient: Gradient {
            GradientStop {  position: 0.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 1.0)}
            GradientStop {  position: 1.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.0)}
        }
    }
    LinearGradient {
        width: parent.width
        height: borderThickness
        anchors.bottom: parent.bottom
        start: Qt.point(0,height)
        end: Qt.point(0,0)
        cached: true
        gradient: Gradient {
            GradientStop {  position: 0.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 1.0)}
            GradientStop {  position: 1.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.0)}
        }
    }
    LinearGradient {
        width: borderThickness
        height: parent.height
        anchors.left: parent.left
        end: Qt.point(width,0)
        start: Qt.point(0,0)
        cached: true
        gradient: Gradient {
            GradientStop {  position: 0.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 1.0)}
            GradientStop {  position: 1.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.0)}
        }
    }
    LinearGradient {
        width: borderThickness
        height: parent.height
        anchors.right: parent.right
        end: Qt.point(0,0)
        start: Qt.point(width,0)
        cached: true
        gradient: Gradient {
            GradientStop {  position: 0.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 1.0)}
            GradientStop {  position: 1.0; color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.0)}
        }
    }
}
// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15 //for ColorOverlay


SplitView {

    property real framePadding: 6
    property color textColorActive: "white"
    property color textColorNormal: "light grey"

    focus: false

    property Component splitHandleHorizontal:
    Rectangle {
        implicitWidth: framePadding; implicitHeight: framePadding; color: "transparent"
        Image {
            visible: parent.SplitHandle.hovered
            anchors.centerIn: parent
            source: "qrc:/feather_icons/more-horizontal.svg"
            height: framePadding
            width: framePadding*3
            rotation: 90
            smooth: true
            layer {
                enabled: true
                effect:
                ColorOverlay {
                    color: parent.SplitHandle.pressed? itemColorActive: textColorNormal
                }
            }
        }
    }

    property Component splitHandleVertical:
    Rectangle {
        implicitWidth: framePadding; implicitHeight: framePadding; color: "transparent"
        Image {
            visible: parent.SplitHandle.hovered
            anchors.centerIn: parent
            source: "qrc:/feather_icons/more-horizontal.svg"
            height: framePadding
            width: framePadding*3
            rotation: 0
            smooth: true
            layer {
                enabled: true
                effect:
                ColorOverlay {
                    color: parent.SplitHandle.pressed? itemColorActive: textColorNormal
                }
            }
        }
    }

    orientation: Qt.Horizontal
    handle: orientation === Qt.Horizontal? splitHandleHorizontal: splitHandleVertical
}
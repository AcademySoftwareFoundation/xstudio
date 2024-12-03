// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15

import xStudio 1.0

BusyIndicator {
    id: control
    property int size: Math.min(control.height, control.width)
    visible: item.opacity != 0

    property double spot_radius: size / 10.0
    property int spot_count: 6

    contentItem: Item {
        anchors.fill: parent

        Item {
            id: item
            x: parent.width / 2 - (size/2)
            y: parent.height / 2 - (size/2)
            width: size
            height: size
            opacity: control.running ? 1 : 0

            Behavior on opacity {
                OpacityAnimator {
                    duration: 250
                }
            }

            Timer {

                interval: 200
                running: true
                repeat: true

                onTriggered: {
                    control.rotation = control.rotation + 60.0
                    if (control.rotation >= 360.0) control.rotation = 0.0
                }
            }

            Repeater {
                id: repeater
                model: spot_count

                Rectangle {
                    x: item.width / 2 - width / 2
                    y: item.height / 2 - height / 2
                    width: spot_radius * 2
                    height: spot_radius * 2
                    radius: spot_radius
                    color: XsStyleSheet.accentColor

                    opacity: index / (repeater.count-1)
                    transform: [
                        Translate {
                            y: -Math.min(item.width, item.height) * 0.5 + (size/10)
                        },
                        Rotation {
                            angle: index / repeater.count * 360
                            origin.x: size/10
                            origin.y: size/10
                        }
                    ]
                }
            }
        }
    }
}
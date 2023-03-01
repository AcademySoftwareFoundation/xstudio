// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.12

import xStudio 1.0

Rectangle {

    id: cont

    color: XsStyle.transportBackground
    //border.color: XsStyle.transportBorderColor
    //border.width: 1

    property alias to: control.to
    property alias from: control.from

    Rectangle {

        id: control
        anchors.fill: parent
        anchors.leftMargin: (handleWidth/2)+1
        anchors.rightMargin: (handleWidth/2)+1
        anchors.topMargin: 1
        anchors.bottomMargin: 1

        color: "transparent"

        // the cpp code sets the first frame as 0; We will display frame 1
        // as the first frame. We will adjust the frame to +1/-1 for when we
        // are sending/getting the current frame number.
        property var from: 0.5
        property var to: (playhead ? playhead.frames < 1 ? 1 : playhead.frames: 1) - 0.5

        property var playheadFrame: playhead ? playhead.frame:1

        property int numTicks: 0
        property real tickSpacing: width / numTicks
        property int tickHeight: height * .5
        property int duration: to - from
        property int handleWidth: 13
        property int maxNumTicks: 200
        property int mousePressValue
        property int clickedX

        onDurationChanged: {
            if (duration <= maxNumTicks) {
                numTicks = duration
            } else {
                var retval = duration
                while (retval > maxNumTicks) {
                    retval = retval / 2
                }
                numTicks = retval
            }
        }

        Item {
            id: myHandle
            x: control.width*((control.playheadFrame) / (control.to-control.from+0.00001)) - width/2
            y: (control.height - height) / 2
            property int absx: 0
            property int absy: 0
            width: control.handleWidth
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            layer.enabled: true
            layer.samples: 4

            property real shapeBlockPerc: .25

            Shape {
                id: shape
                property int nib_width: control.handleWidth
                property int nib_height: myHandle.shapeBlockPerc * myHandle.height

                ShapePath {
                    id: bottomShapePath
                    strokeWidth: 1.5
                    strokeColor: XsStyle.highlightColor
                    fillColor: XsStyle.highlightColor
                    Behavior on fillColor {
                        ColorAnimation {duration: XsStyle.colorDuration}
                    }

                    // Start at bottom-left
                    startX:0
                    startY:myHandle.height - 1

                    // to bottom right
                    PathLine {x: bottomShapePath.startX + shape.nib_width; y: bottomShapePath.startY}

                    // to tip (top middle)
                    PathLine {x: bottomShapePath.startX + (shape.nib_width/2); y: bottomShapePath.startY - shape.nib_height}

                    // to bottom left
                    PathLine {x: bottomShapePath.startX; y: bottomShapePath.startY}
                }

                ShapePath {
                    id: topShapePath
                    strokeWidth: 1.5
                    strokeColor: XsStyle.highlightColor
                    fillColor: XsStyle.highlightColor
                    Behavior on fillColor {
                        ColorAnimation {duration: XsStyle.colorDuration}
                    }

                    // Start at top-left
                    startX: 0
                    startY: 1

                    // to tip (bottom middle)
                    PathLine {
                        x: topShapePath.startX + (shape.nib_width/2)
                        y: topShapePath.startY + shape.nib_height
                    }

                    // to top right
                    PathLine {
                        x: topShapePath.startX + shape.nib_width
                        y: topShapePath.startY
                     }


                    // to bottom left
                    PathLine {
                        x: topShapePath.startX
                        y: topShapePath.startY
                    }
                }

                ShapePath {
                    id: midLine
                    strokeWidth: 1.5
                    strokeColor: XsStyle.highlightColor
                    fillColor: XsStyle.highlightColor
                    Behavior on fillColor {
                        ColorAnimation {duration: XsStyle.colorDuration}
                    }
                    startX: shape.nib_width/2
                    startY: 1
                    PathLine {x: midLine.startX; y: myHandle.height - 1}
                }
            }
        }

        // little green dot showing the on-screen frame
        Rectangle {
            x: control.width*(viewport.onScreenImageLogicalFrame-control.from+0.5)/(control.to-control.from)-width/2
            y: (control.height - height) / 2
            width: 3
            height: 3
            color: "green"
            radius: 2
        }

        Rectangle {

            id: bgRect
            anchors.fill: parent
            z: -100

            // Blanking section for loop 'in' point
            Rectangle {

                height:parent.height-2
                color: XsStyle.outsideLoopTimelineColor
                x: 1
                y: 1
                width: playhead ? control.width*(playhead.loopStart-control.from)/(control.to-control.from) : 0
                visible: playhead ? playhead.useLoopRange : 0
            }

            // Blanking section for loop 'out' point
            Rectangle {
                height:parent.height-2
                anchors.right: parent.right
                color: XsStyle.outsideLoopTimelineColor
                x: 0
                y: 1
                // width: playhead ? control.width*(control.to-control.from-playhead.loopEnd-1.5)/(control.to-control.from) : 0

                width: playhead ? Math.min(
                    (control.width / control.duration) * ((
                        (control.to - control.from) - playhead.loopEnd
                    )-0.5),
                    control.width-1
                ) : 0


                visible: playhead ? playhead.useLoopRange : 0
            }

            // This is the color to the right of the handle.
            color: XsStyle.transportBackground
            Rectangle {
                height:1
                color: XsStyle.transportBorderColor
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
            Item {
                Repeater {
                    model: control.numTicks+1
                    Rectangle {
                        width: 1
                        height: control.tickHeight
                        color: XsStyle.transportBorderColor
                        x: Math.min(((control.width) / control.numTicks) * index,control.width-1)
                        y: bgRect.height / 2 - control.tickHeight / 2 - 1
                    }
                }
            }
            Item {
                Repeater {
                    model: playhead ? playhead.cachedFrames : null
                    Rectangle {
                        height: (control.tickHeight / 3) + 1
                        color: XsStyle.highlightColor
                        opacity: 0.5
                        x: Math.min((control.width / control.duration) * (modelData.x ? modelData.x-0.5 : 0), control.width-1 )
                        width: Math.min(
                            (control.width / control.duration) * (
                                (modelData.y - modelData.x) + (modelData.y == control.duration || !modelData.x ? 0.5 : 1.0)
                            ),
                            control.width-1
                        )
                        y: bgRect.height / 2 - control.tickHeight / 6 - 1
                    }
                }
            }
            Item {
                Repeater {
                    model: playhead ? playhead.bookmarkedFrames : null
                    Rectangle {
                        height: bgRect.height - ((control.tickHeight / 6) - 2)*2
                        color: modelData.c ? modelData.c  : preferences.accent_color
                        opacity: 0.5
                        x: Math.min((control.width / control.duration) * (modelData.x ?  modelData.x-0.5  : 0), control.width-1 )
                        width: Math.min(
                            (control.width / control.duration) * (
                                (modelData.y - modelData.x) + (modelData.y == control.duration || !modelData.x ? 0.5 : 1.0)
                            ),
                            control.width-1
                        )
                        y: control.tickHeight / 6 - 2// bgRect.height - control.tickHeight / 6 - 2
                    }
                }
            }
        }

        MouseArea {
            id: mouseArea
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: true

            property bool preScrubPlaying: false

            onPressed: {
                if (playhead && playhead.playing) {
                    preScrubPlaying = true
                    playhead.playing = false
                }
            }
            onReleased: {
                if (preferences.restore_play_state_after_scrub.value && playhead) {
                    playhead.playing = preScrubPlaying
                }
                preScrubPlaying = false
            }

            onMouseXChanged: {
                if (pressed) {
                    playhead.frame = Math.round(mouseX * (control.to - control.from + 0.00001) / control.width)
                }
            }

            onHoveredChanged: {
                if (!mouseArea.containsMouse) {
                    status_bar.clearMessage()
                }
            }
        }
    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts
import QtQuick.Shapes 1.12

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item {

    id: control
    property var tickSpacings: [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000]
    property real tickSpacing: 1.0
    property real minTickSpacing: 5

    property bool zoomToLoop: viewportPlayhead.zoomToLoop === true && viewportPlayhead.enableLoopRange === true ? true : false
    property var zoomStart: viewportPlayhead.loopStartFrame
    property var zoomEnd: viewportPlayhead.loopEndFrame

    property int mediaTransitions: viewportPlayhead.mediaTransitionFrames ? viewportPlayhead.mediaTransitionFrames.length-1 : 0


    property real scaleFactor: width / (zoomToLoop ? (zoomEnd - zoomStart) : (viewportPlayhead.durationFrames-1))

    onScaleFactorChanged: {
        if(isNaN(viewportPlayhead.durationFrames)) {
            enabled = false
            opacity = 0
        } else {
            enabled = true
            opacity = 1
            computeTickSpacing(scaleFactor)
        }
    }

    property int numBookmarks: {
        // pretty ugly, but only way I can suppress errors before these attr
        // values are intialised
        if (viewportPlayhead.bookmarkedFrames != undefined &&
            viewportPlayhead.bookmarkedFrameColours != undefined &&
            typeof viewportPlayhead.bookmarkedFrameColours.length != "undefined" &&
            typeof viewportPlayhead.bookmarkedFrames.length != "undefined") {
            return Math.min(viewportPlayhead.bookmarkedFrameColours.length, viewportPlayhead.bookmarkedFrames.length)
        }
        return 0;
    }

    XsModelProperty {
        id: __restorePlayAfterScrub
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/restore_play_state_after_scrub", "pathRole")
    }
    property alias restorePlayAfterScrub: __restorePlayAfterScrub.value

    function computeTickSpacing(scaleFactor) {
        for (var i in tickSpacings) {
            if (scaleFactor*tickSpacings[i] > minTickSpacing) {
                tickSpacing = scaleFactor*tickSpacings[i]
                return
            }
        }
        tickSpacing = 10
    }

    property int numTicks: width/tickSpacing

    // ********************** LOOP REGION ****************************

    XsModelProperty {
        id: timeline_units_pref
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/timeline_units", "pathRole")
    }
    property alias timelineUnits: timeline_units_pref.value

    function currentFrameText(selected, offset) {
        switch(selected) {
            case "Frames":
                return isNaN(offset) ? "----" : String(offset + 1).padStart(4, '0')
            break;
            case "Time":
                let result = "--:--:--"
                if(!isNaN(offset * viewportPlayhead.frameRate)) {
                    var seconds = Math.floor(offset * viewportPlayhead.frameRate)
                    var minutes = Math.floor(seconds / 60)
                    var hours = Math.floor(minutes / 60)
                    var SS = String(seconds % 60).padStart(2, '0')
                    var MM = String(minutes % 60).padStart(2, '0')
                    if (hours > 0) {
                        var HH = hours % 24
                        result = HH + ':' + MM + ':' + SS
                    } else {
                        result = MM + ':' + SS
                    }
                }
                return result
            break;
            case "Timecode":
                return viewportPlayhead.timecode && mediaTransitions < 2? helpers.stringFromTimeCode(
                    viewportPlayhead.timecodeAsFrame - viewportPlayhead.logicalFrame + offset,
                    1.0 / viewportPlayhead.frameRate
                ) : "--:--:--:--"
            break;
            case "Frames From Timecode":
                return isNaN(viewportPlayhead.timecodeAsFrame) || mediaTransitions > 1? "----" : String(viewportPlayhead.timecodeAsFrame - viewportPlayhead.logicalFrame+offset).padStart(4, '0')
            break;

        }
    }

    Rectangle {
        id: loopRegion
        opacity: 0.2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: scaleFactor * (viewportPlayhead.loopStartFrame - (zoomToLoop ? zoomStart : 0))
        width: scaleFactor*(viewportPlayhead.loopEndFrame - viewportPlayhead.loopStartFrame)
        property bool vis: viewportPlayhead.enableLoopRange === true && (viewportPlayhead.loopStartFrame !== 0 || viewportPlayhead.loopEndFrame < (viewportPlayhead.durationFrames-1))
        visible: vis == true
        color: XsStyleSheet.accentColor
    }

    // frame start
    XsLabel {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: -2
        anchors.leftMargin: 2

        horizontalAlignment: Text.AlignLeft
        width: 100
        height: 10

        text: currentFrameText(timelineUnits, 0)
    }

    // end frame
    XsLabel {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: -2
        anchors.rightMargin: 2

        width: 100
        height: 10

        text: currentFrameText(timelineUnits, viewportPlayhead.durationFrames-1)

        // font.pixelSize: XsStyleSheet.fontSize
        horizontalAlignment: Text.AlignRight
    }

    // loop start
    XsLabel {
        anchors.top: loopRegion.top
        anchors.left: loopRegion.left
        anchors.topMargin: -1
        anchors.leftMargin: 2

        visible: loopRegion.visible
        horizontalAlignment: Text.AlignLeft
        width: 100
        height: 10

        text: currentFrameText(timelineUnits, viewportPlayhead.loopStartFrame)

    }

    // loop end
    XsLabel {
        anchors.top: loopRegion.top
        anchors.right: loopRegion.right
        anchors.topMargin: -1
        anchors.rightMargin: 2

        visible: loopRegion.visible
        width: 100
        height: 10

        text: currentFrameText(timelineUnits, viewportPlayhead.loopEndFrame)

        // font.pixelSize: XsStyleSheet.fontSize
        horizontalAlignment: Text.AlignRight
    }

    // ********************** CLIP REGIONS ****************************

    // draw faint strips for alternating media/clips

    Repeater {
        model: mediaTransitions
        Rectangle {
            height: parent.height
            width: scaleFactor*(viewportPlayhead.mediaTransitionFrames[index+1]-viewportPlayhead.mediaTransitionFrames[index])
            x: scaleFactor * (viewportPlayhead.mediaTransitionFrames[index] - (zoomToLoop ? zoomStart : 0) )
            opacity: (index%2 == 0) ? 0.05 : 0.1
            visible: !zoomToLoop || (viewportPlayhead.mediaTransitionFrames[index]  < zoomEnd && viewportPlayhead.mediaTransitionFrames[index+1] > zoomStart)
        }
    }

    // ********************** FRAME TICKS ****************************

    Item {
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        Repeater {
            model: control.numTicks
            Rectangle {
                width: 1
                height: isBigTick ? parent.height : parent.height - 4
                color: "white"
                opacity: isBigTick ? 0.4 : 0.25
                x: (tickSpacing * index)
                y: isBigTick ? 0 : 4
                property bool isBigTick: index == Math.round((index/5.0))*5
            }
        }
    }


    // ********************** CACHED FRAMES ****************************

    // Draws the image cache indicator bar(s)
    XsBufferedUIProperty {
        id: bufferedCachedFrames
        source: viewportPlayhead.cachedFrames
        playing: viewportPlayhead.playing != undefined ? viewportPlayhead.playing : false
    }
    property alias cachedFrames: bufferedCachedFrames.value

    // 0: in cache, 1: held frame, 2: not on disk
    property var cache_indicator_colours: ["green", "#FFE08000", "#FFB00000"]

    Item {
        anchors.fill: parent
        clip: true
        Repeater {
            model: cachedFrames
            Rectangle {
                color: cache_indicator_colours[cachedFrames[index].colour_idx]
                opacity: 0.5
                x: scaleFactor * (cachedFrames[index].start - (zoomToLoop ? zoomStart : 0) - 0.5)
                width: scaleFactor*(cachedFrames[index].duration)
                height: 5
                y: 10
                visible: !zoomToLoop || (cachedFrames[index].start  < zoomEnd && (cachedFrames[index].start+cachedFrames[index].duration) > zoomStart)
            }
        }
    }



    // ********************** CURRENT FRAME ****************************
    // This is the playhead widget - a vertical line with a little upside-down
    // house shape at the top

    property var handleWidth: 7
    property var handleColour: XsStyleSheet.accentColor

    XsBufferedUIProperty {
        id: bufferedX
        source: scaleFactor * (viewportPlayhead.logicalFrame - (zoomToLoop ? zoomStart : 0))
        playing: viewportPlayhead.playing != undefined ? viewportPlayhead.playing : false
    }

    Item {

        x: bufferedX.value - width/2
        width: control.handleWidth
        property alias handleColour: control.handleColour

        anchors.top: parent.top
        anchors.bottom: parent.bottom

        layer.enabled: true
        layer.samples: 4

        Shape {

            id: shape

            ShapePath {
                strokeWidth: 1
                strokeColor: handleColour
                fillColor: handleColour
                startX: 0
                startY: 0
                PathLine {x: control.handleWidth; y: 0}
                PathLine {x: control.handleWidth; y: control.handleWidth/2}
                PathLine {x: control.handleWidth/2; y: control.handleWidth/1.5}
                PathLine {x: 0; y: control.handleWidth/2}
            }

            ShapePath {
                strokeWidth: 1.5
                strokeColor: handleColour
                fillColor: handleColour
                startX: control.handleWidth/2
                startY: control.handleWidth/1.5
                PathLine {
                    x: control.handleWidth/2
                    y: control.height
                }
            }
        }
    }

    // ********************** BOOKMARKS ****************************

    Item {
        Repeater {
            model: viewportPlayhead.bookmarkedFrames
            Rectangle {
                height: 5
                color: bmColour ? bmColour : XsStyleSheet.accentColor
                property var bmColour: viewportPlayhead.bookmarkedFrameColours[index]
                opacity: 1
                radius: height/2
                // if indicator width is less than diameter (5 pixels) of a dot then
                // we need to adjust the x to ensure the dot is aligned with the frame number
                x: Math.max(0, xx)
                property var xx: scaleFactor * (viewportPlayhead.bookmarkedFrames[index * 2] - (zoomToLoop ? zoomStart : 0)) - (w < 5 ? (5-w)/2 : 0.0)
                property var w: scaleFactor * viewportPlayhead.bookmarkedFrames[index * 2 + 1]

                width: Math.min( Math.min(Math.max(5, w), Math.max(5, w) + xx), control.width - x )
                y: control.height - height + 1.2

                visible: !zoomToLoop || (viewportPlayhead.bookmarkedFrames[index * 2]  < zoomEnd && (viewportPlayhead.bookmarkedFrames[index * 2] + viewportPlayhead.bookmarkedFrames[index * 2 + 1]) > zoomStart)
            }
        }
    }

    // ********************** MOUSEAREA ****************************

    MouseArea {

        id: mouseArea
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        anchors.fill: parent
        propagateComposedEvents: true

        property bool preScrubPlaying: false

        onPressed: (mouse)=> {
            if (mouse.button == Qt.RightButton) {
                viewportPlayhead.zoomToLoop = !viewportPlayhead.zoomToLoop
            } else {
                viewportPlayhead.scrubbingFrames = true
                if (viewportPlayhead.playing) {
                    preScrubPlaying = true
                    viewportPlayhead.playing = false
                }
            }
        }

        onReleased: {
            if(viewportPlayhead.scrubbingFrames) {
                viewportPlayhead.scrubbingFrames = false
                if (restorePlayAfterScrub && preScrubPlaying) {
                    viewportPlayhead.playing = preScrubPlaying
                }
                preScrubPlaying = false
            }
        }

        onMouseXChanged: {
            if (pressed && pressedButtons == Qt.LeftButton) {
                if(zoomToLoop)
                    viewportPlayhead.logicalFrame = Math.min( Math.max( Math.round(mouseX / scaleFactor) + zoomStart, zoomStart), zoomEnd)
                else
                    viewportPlayhead.logicalFrame = Math.round(mouseX / scaleFactor)
            }
        }

    }
}

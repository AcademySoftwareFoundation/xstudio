// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQml.Models 2.12

import xStudio 1.0
import xstudio.qml.helpers 1.0


Rectangle {
	id: control
	property int start: 0
    property int duration: 0
    property int secondOffset: 0
    property real fractionOffset: 0
    property real fps: 24
    property real tickWidth: (control.width / duration)
	property color tickColor: "black"
	property bool renderFrames: tickWidth >= 3
	property bool renderSeconds: duration > fps && tickWidth * fps >= 3
    property bool endTicks: true
    property bool preScrubPlaying: false
	color: "transparent"

    signal frameClicked(int frame)
    signal framePressed(int frame)
    signal frameDragging(int frame)

    XsModelProperty {
        id: __restorePlayAfterScrub
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/restore_play_state_after_scrub", "pathRole")
    }
    property alias restorePlayAfterScrub: __restorePlayAfterScrub.value

    MouseArea{
        id: mArea
        anchors.fill: parent
        hoverEnabled: true
        property bool dragging: false
        onClicked: mouse => {
            if (mouse.button == Qt.LeftButton) {
                frameClicked(start + ((mouse.x + fractionOffset)/ tickWidth))
            }
        }

        onReleased: {
            if(timelinePlayhead.scrubbingFrames && dragging) {
                dragging = false
                timelinePlayhead.scrubbingFrames = false
                if (restorePlayAfterScrub && preScrubPlaying) {
                    timelinePlayhead.playing = preScrubPlaying
                }
            }
        }
        onPressed: mouse => {

            // here we make sure the viewport is attached to the correct
            // playhead - we want the 'main' playhead of the timeline not the
            // auxillary one which is used for showing individual clips
            viewportCurrentMediaContainerIndex = timelineModel.rootIndex.parent

            // we ensure the timeline playhead is back in 'pinned' mode. This
            // means, regardless of the media selection, the playhead source
            // is pinned to the timeline itslef and isn't going to use the
            // selected media as it's source (which is usual behaviour)
            timelinePlayhead.pinnedSourceMode = true

            if (mouse.button == Qt.LeftButton) {
                if (timelinePlayhead.playing) {
                    preScrubPlaying = true
                    timelinePlayhead.playing = false
                } else {
                    preScrubPlaying = false
                }

                timelinePlayhead.scrubbingFrames = true
                framePressed(start + ((mouse.x + fractionOffset)/ tickWidth))
                dragging = true
            }
        }

        onPositionChanged: mouse => {
            if (dragging) {
                frameDragging(start + ((mouse.x + fractionOffset)/ tickWidth))
            }
        }
    }


	// frame repeater
    Repeater {
        model: control.height > 8 && renderFrames ? duration-(endTicks ? 0 : 1) : null
        Rectangle {
            height: control.height / 2
            color: tickColor

            x: ((index+(endTicks ? 0 : 1)) * tickWidth) - fractionOffset
            visible: x >=0
            width: 1
        }
    }

    Repeater {
        model: control.height > 4 && renderSeconds ? Math.ceil(duration / fps) - (endTicks ? 0 : 1) : null
        Rectangle {
            height: control.height
            color: tickColor

            x: (((index + (endTicks ? 0 : 1)) * (tickWidth * fps)) - (secondOffset * tickWidth)) - fractionOffset
            visible: x >=0
            width: 1
        }
    }
}
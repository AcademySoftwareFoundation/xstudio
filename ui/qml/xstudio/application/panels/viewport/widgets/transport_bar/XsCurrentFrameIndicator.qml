import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

XsViewerTextDisplay
{

    id: playheadPosition
    Layout.preferredWidth: 100
    Layout.preferredHeight: parent.height
    modelDataName: playheadPosition + "_menu"
    menuWidth: 175
    property int selected: 0

    XsModelProperty {
        id: timeline_units_pref
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/timeline_units", "pathRole")
    }
    property alias timelineUnits: timeline_units_pref.value

    XsMenuModelItem {
        menuPath: ""
        menuItemType: "radiogroup"
        menuItemPosition: 1
        choices: ["Frames", "Time", "Timecode", "Frames From Timecode"]
        currentChoice: timelineUnits
        menuModelName: playheadPosition + "_menu"
        onCurrentChoiceChanged: {
            if (currentChoice != timelineUnits) {
                timelineUnits = currentChoice
            }
            selected = choices.indexOf(currentChoice)
        }
        property var timelineUnits_: timelineUnits
        onTimelineUnits_Changed: {
            if (timelineUnits_ != undefined && currentChoice != timelineUnits_)
                currentChoice = timelineUnits_
        }
    }

    function currentFrameText(selected) {
        switch(selected) {
            case 0:
                return isNaN(viewportPlayhead.logicalFrame) ? "----" : String(viewportPlayhead.logicalFrame+1).padStart(4, '0')
            break;
            case 1:
                let result = "--:--:--"
                if(!isNaN(viewportPlayhead.positionSeconds)) {
                    var seconds = Math.floor(viewportPlayhead.positionSeconds)
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
            case 2:
                return viewportPlayhead.timecode ? viewportPlayhead.timecode : "--:--:--:--"
            break;
            case 3:
                return isNaN(viewportPlayhead.timecodeAsFrame) ? "----" : String(viewportPlayhead.timecodeAsFrame).padStart(4, '0')
            break;

        }
    }

    XsBufferedUIProperty {
        id: bufferedPlayheadFrame
        source: currentFrameText(selected)
        playing: viewportPlayhead.playing != undefined ? viewportPlayhead.playing : false
    }

    text: bufferedPlayheadFrame.value
}

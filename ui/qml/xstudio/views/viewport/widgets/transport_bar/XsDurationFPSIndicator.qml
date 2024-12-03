import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
// import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

XsViewerTextDisplay
{

    id: playheadPosition
    Layout.preferredWidth: textWidth + 8
    Layout.preferredHeight: parent.height
    modelDataName: playheadPosition + "_menu"
    menuWidth: 175
    property int selected: 0
    fontFamily: XsStyleSheet.fixedWidthFontFamily

    XsModelProperty {
        id: durationPref
        role: "valueRole"
        index: globalStoreModel.searchRecursive("/ui/qml/timeline_indicator", "pathRole")
    }
    property alias timelineIndicator: durationPref.value

    property var timelineUnits

    property int display_mode: timelineUnits == "Frames" || timelineUnits == "Frames From Timecode"? 0 : (timelineUnits == "Time" ? 1 : 2)

    XsMenuModelItem {
        menuPath: ""
        menuItemType: "radiogroup"
        menuItemPosition: 1
        choices: ["Duration", "Remaining", "FPS"]
        currentChoice: timelineIndicator
        menuModelName: playheadPosition + "_menu"
        onCurrentChoiceChanged: {
            if (currentChoice != timelineIndicator) {
                timelineIndicator = currentChoice
            }
            selected = choices.indexOf(currentChoice)
        }
        property var timelineIndicator_: timelineIndicator
        onTimelineIndicator_Changed: {
            if (timelineIndicator_ != undefined && currentChoice != timelineIndicator_)
                currentChoice = timelineIndicator_
        }
    }

    property string fps: view.frame_rate_expr != undefined ? view.frame_rate_expr : "--"
    property var duration: {
        switch(display_mode) {
            case 0:
                if(isNaN(viewportPlayhead.durationFrames))
                    return "----"
                return String(viewportPlayhead.durationFrames).padStart(4, '0')
            case 1:
                return timeStr(viewportPlayhead.durationSeconds);
            case 2:
                return getTimeCodeStr(
                    viewportPlayhead.durationFrames,
                    Math.round(1.0 / viewportPlayhead.frameRate));
        }
        return "--"
    }
    property string remaining: {
        switch(display_mode) {
            case 0:
                if(isNaN(viewportPlayhead.durationFrames) || isNaN(viewportPlayhead.logicalFrame))
                    return "----"
                else
                    return String(viewportPlayhead.durationFrames-viewportPlayhead.logicalFrame-1).padStart(4, '0')
            case 1:
                return timeStr(viewportPlayhead.durationSeconds - viewportPlayhead.positionSeconds);
            case 2:
                return getTimeCodeStr(
                    viewportPlayhead.durationFrames - viewportPlayhead.logicalFrame-1,
                    Math.round(1.0/viewportPlayhead.frameRate));
        }
        return "--"
    }


    text: selected == 0 ? duration : selected == 1 ? remaining : fps

    function timeStr(tt) {
        let result = "--:--:--"

        if(!isNaN(tt)) {
            var seconds = Math.floor(tt)
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
    }

    function getTimeCodeStr(displayValue, fps)
    {
        let result = "--:--:--:--"

        if(!isNaN(displayValue) && !isNaN(fps)) {
            // e.g. minus 1 so that frame 30 for 30fps should still be 00:00
            // displayValue = displayValue
            var frames = displayValue % Math.round(fps)
            var seconds = Math.floor(displayValue / fps)
            var minutes = Math.floor(seconds / 60)
            var hours = Math.floor(minutes / 60)
            var FF = String(frames).padStart(2, '0')

            var SS = String(seconds % 60).padStart(2, '0')
            var MM = String(minutes % 60).padStart(2, '0')
            var HH = String(hours).padStart(2, '0')
            result = HH + ':' + MM + ':' + SS + ':' + FF
        }

        return result
    }
}
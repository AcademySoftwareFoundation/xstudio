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
    Layout.preferredWidth: 100
    Layout.preferredHeight: parent.height
    modelDataName: playheadPosition + "_menu"
    menuWidth: 175
    property int selected: 0
    fontFamily: XsStyleSheet.fixedWidthFontFamily

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

    property string timecode: viewportPlayhead.timecode ? viewportPlayhead.timecode : "--:--:--:--"

    property string timecodeFrame: isNaN(viewportPlayhead.timecodeAsFrame) ? "----" : String(viewportPlayhead.timecodeAsFrame).padStart(4, '0')

    property string frame: isNaN(viewportPlayhead.logicalFrame) ? "----" : String(viewportPlayhead.logicalFrame+1).padStart(4, '0')

    property string seconds: {
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
    }

    text: selected == 0 ? frame : selected == 1 ? seconds : selected == 2 ? timecode : timecodeFrame
}

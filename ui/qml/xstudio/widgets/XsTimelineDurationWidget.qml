// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.impl 2.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xstudio.qml.properties 1.0
import xStudio 1.0

XsMultiWidget {
	id: timedur
	text: "Duration"
	tooltip: "Duration/FPS.  Click to choose Duration, Remaining or FPS display mode."
    property double value: 0
	implicitWidth: (display_mode == 2 || selected == 2) ? 100 : 50
    selected: -1

    XsPreferenceSet {
        id: timeline_units_pref
        preferencePath: "/ui/qml/timeline_units"
    }
    property var timeline_units: timeline_units_pref.properties.value
    property int display_mode: timeline_units == "Frames" || timeline_units == "Frames From Timecode"? 0 : (timeline_units == "Time" ? 1 : 2)

    XsPreferenceSet {
        id: timeline_indicator_pref
        preferencePath: "/ui/qml/timeline_indicator"
    }
    property var timeline_indicator: timeline_indicator_pref.properties.value

    // get preference change from backend
    onTimeline_indicatorChanged: {
        if (timeline_indicator == "Duration") {
            selected = 0
        } else if (timeline_indicator == "Remaining") {
            selected = 1
        } else if (timeline_indicator == "FPS") {
            selected = 2
        }
    }

    // set preference change from frontend
    onSelectedChanged: {
        if (selected == 0) {
            timeline_indicator_pref.properties.value = "Duration"
        } else if (selected == 1) {
            timeline_indicator_pref.properties.value = "Remaining"
        } else if (selected == 2) {
            timeline_indicator_pref.properties.value = "FPS"
        }
    }

    Label {
        id: label1
        property string label: "Duration"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: {
            switch(display_mode) {
                case 0:
                    return XsUtils.pad("" + value, 4);
                case 1:
                    return XsUtils.getTimeStr(value);
                case 2:
                    return XsUtils.getTimeCodeStr(value, Math.round(playhead ? playhead.playheadRate : 24));
            }
            return "--"
        }
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 0
    }
    Label {
        id: label2
        property string label: "Remaining"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: {
            switch(display_mode) {
                case 0:
                    return XsUtils.pad("" + value, 4);
                case 1:
                    return XsUtils.getTimeStr(value + 1);
                case 2:
                    return XsUtils.getTimeCodeStr(value, Math.round(playhead ? playhead.playheadRate: 24));
            }
            return "--"
        }
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 1
    }
    Label {
        id: label3
        property string label: "FPS"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: viewport.fpsExpression
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 2
    }
}
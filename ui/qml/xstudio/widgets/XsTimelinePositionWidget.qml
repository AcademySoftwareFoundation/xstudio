// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.impl 2.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0

import xstudio.qml.properties 1.0

XsMultiWidget {
	id: timepos
	text: "Playhead position"
	tooltip: "Playhead Time.  Click to choose Frames, Time, Timecode or Frames From Timecode display."
    property double value: 0
    selected: -1

	implicitWidth: selected == 2 ? 100 : 50

    XsPreferenceSet {
        id: timeline_units_pref
        preferencePath: "/ui/qml/timeline_units"
    }

    property var timeline_units: timeline_units_pref.properties.value

    onTimeline_unitsChanged: {
        if (timeline_units == "Frames" && selected != 0) {
            selected = 0
        } else if (timeline_units == "Time" && selected != 1) {
            selected = 1
        } else if (timeline_units == "Timecode" && selected != 2) {
            selected = 2
        } else if (timeline_units == "Frames From Timecode" && selected != 3) {
            selected = 3
        }
    }

    onSelectedChanged: {
        if (selected == 0 && timeline_units != "Frames") {
            timeline_units_pref.properties.value = "Frames"
        } else if (selected == 1 && timeline_units != "Time") {
            timeline_units_pref.properties.value = "Time"
        } else if (selected == 2 && timeline_units != "Timecode") {
            timeline_units_pref.properties.value = "Timecode"
        } else if (selected == 3 && timeline_units != "Frames From Timecode") {
            timeline_units_pref.properties.value = "Frames From Timecode"
        }
    }

    Label {
        property string label: "Frames"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: XsUtils.pad(""+value, 4)
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 0
    }
    Label {
        property string label: "Time"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: XsUtils.getTimeStr(value)
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 1
    }
    Label {
        property string label: "Timecode"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: playhead ? playhead.timecode : "--"
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 2
    }
    Label {
        property string label: "Frames From Timecode"

        Layout.fillWidth: true
        Layout.fillHeight: true
        verticalAlignment: "AlignVCenter"
        horizontalAlignment: "AlignHCenter"
        color: mouseArea.containsMouse?XsStyle.hoverColor:XsStyle.highlightColor
        text: playhead ?  XsUtils.pad(""+playhead.timecodeFrames, 4) : "--"
        font.family: "Courier"
        font.weight: Font.Black
        font.pixelSize: XsStyle.timeFontSize
        visible: selected === 3
    }
}
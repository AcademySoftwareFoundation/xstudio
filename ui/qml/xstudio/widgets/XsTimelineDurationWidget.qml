// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.impl 2.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xstudio.qml.helpers 1.0
import xStudio 1.0

XsMultiWidget {
	id: timedur
	text: "Duration"
	tooltip: "Duration/FPS.  Click to choose Duration, Remaining or FPS display mode."
    property double value: 0
	implicitWidth: (display_mode == 2 || selected == 2) ? 100 : 50
    selected: -1

    XsModelProperty {
        id: timeline_units_pref
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/timeline_units", "pathRole")
    }

    XsModelProperty {
        id: timeline_indicator_pref
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/timeline_indicator", "pathRole")
        onValueChanged: {
            if (value == "Duration") {
                selected = 0
            } else if (value == "Remaining") {
                selected = 1
            } else if (value == "FPS") {
                selected = 2
            }
        }
    }

    property alias timeline_units: timeline_units_pref.value
    property alias timeline_indicator: timeline_indicator_pref.value

    property int display_mode: timeline_units == "Frames" || timeline_units == "Frames From Timecode"? 0 : (timeline_units == "Time" ? 1 : 2)


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
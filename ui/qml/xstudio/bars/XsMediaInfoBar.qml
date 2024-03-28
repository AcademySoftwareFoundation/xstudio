// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5

import xStudio 1.0
import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0

Rectangle {

    id: mediaInfoBar
    implicitHeight: XsStyle.mediaInfoBarFontSize*2.2*opacity//filenameLabel.height + XsStyle.mediaInfoBarMargins*2
    color: XsStyle.mediaInfoBarBackground
    opacity: 1
    visible: opacity !== 0
    z: 1000

    property var playhead: viewport.playhead
    property var format: mediaImageSource.values.formatRole ? mediaImageSource.values.formatRole : "TBD"
    property var bitDepth: mediaImageSource.values.bitDepthRole ? mediaImageSource.values.bitDepthRole : "TBD"
    property var fpsString: mediaImageSource.values.rateFPSRole ? mediaImageSource.values.rateFPSRole : "TBD"
    property var resolution: mediaImageSource.values.resolutionRole ? mediaImageSource.values.resolutionRole : "TBD"

    function sourceOffsetFramesChanged() {
        offsetInput.text = (playhead.sourceOffsetFrames > 0 ? '+' : '') + playhead.sourceOffsetFrames
    }

    property bool offset_enabled: true//{!playhead ? false : (playhead.compareMode != 0 && playhead.compareMode != 5)}

    XsModuleAttributes {
        id: playhead_attrs
        attributesGroupNames: "playhead"
    }

    property bool auto_align_enabled: playhead_attrs.compare !== undefined ? playhead_attrs.compare == "A/B" : false

    Behavior on opacity {
        NumberAnimation { duration: playerWidget.doTrayAnim?200:0 }
    }

    RowLayout {

        anchors.fill: parent

        XsMediaInfoBarAutoAlign{
            id: auto_align_option
            label_text: "Auto Align"
            visible: auto_align_enabled
            tooltip_text: "Select option 'On' to auto align and extend range to include frames from all sources. Select 'On (Trim)' to auto align and reduce to frame range common to all sources."
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsMediaInfoBarOffset {
            id: offset_group
            visible: offset_enabled
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsMediaInfoBarItem {
            id: format_display
            label_text: "Format"
            value_text: format
            tooltip_text: "The image format or video codec of the current source"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsMediaInfoBarItem {
            id: bitdepth_display
            label_text: "Bit Depth"
            value_text: bitDepth
            tooltip_text: "The image bitdepth of the current source"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsMediaInfoBarItem {
            id: fps_display
            label_text: "FPS"
            value_text: fpsString
            tooltip_text: "The playback rate of the current source"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsMediaInfoBarItem {
            id: res_display
            label_text: "Res"
            value_text: resolution
            tooltip_text: "The image resolution in pixels of the current source"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

    }
}
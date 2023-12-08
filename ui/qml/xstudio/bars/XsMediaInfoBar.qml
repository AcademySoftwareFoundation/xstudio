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
    property var format: app_window.mediaImageSource.values.formatRole ? app_window.mediaImageSource.values.formatRole : "TBD"
    property var bitDepth: app_window.mediaImageSource.values.bitDepthRole ? app_window.mediaImageSource.values.bitDepthRole : "TBD"
    property var fpsString: app_window.mediaImageSource.values.rateFPSRole ? app_window.mediaImageSource.values.rateFPSRole : "TBD"
    property var resolution: app_window.mediaImageSource.values.resolutionRole ? app_window.mediaImageSource.values.resolutionRole : "TBD"
    property var pixel_colour: sessionWidget.viewport.colourUnderCursor

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

    /*Rectangle {
        id: filename_display
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: offset_group.left
        color: "transparent"

        Label {

            id: label
            text: filename
            color: XsStyle.controlColor
            anchors.fill: parent
            anchors.leftMargin: 8
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter

            font {
                pixelSize: XsStyle.mediaInfoBarFontSize+2
                family: XsStyle.controlTitleFontFamily
                hintingPreference: Font.PreferNoHinting
            }
        }
    }*/

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

    /*Rectangle {

        id: pixel_colour
        color: "transparent"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        property int comp_width: 34
        width: comp_width*3

        Text {

            text: mediaInfoBar.pixel_colour[0]
            color: "#f66"
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: 0

            font {
                family: XsStyle.pixValuesFontFamily
                pixelSize: XsStyle.pixValuesFontSize
            }

        }

        Text {

            text: mediaInfoBar.pixel_colour[1]
            color: "#6f6"
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: pixel_colour.comp_width

            font {
                family: XsStyle.pixValuesFontFamily
                pixelSize: XsStyle.pixValuesFontSize
            }

        }

        Text {

            text: mediaInfoBar.pixel_colour[2]
            color: "#88f"
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: pixel_colour.comp_width*2

            font {
                family: XsStyle.pixValuesFontFamily
                pixelSize: XsStyle.pixValuesFontSize
            }

        }

    }*/

    /*

        ListModel {

        id: basic_data

        ListElement {
            labelText: "Format"
            tooltip: "The image format or video codec of the current source"
            demotext: "OpenEXR"
        }
        ListElement {
            labelText: "Bit Depth"
            tooltip: "The image bitdepth of the current source"
            demotext: "16 bit float"
        }
        ListElement {
            labelText: "FPS"
            tooltip: "The playback rate of the current source"
            demotext: "23.976"
        }
        ListElement {
            labelText: "Res"
            tooltip: "The image resolution in pixels of the current source"
            demotext: "8888 x 8888"
        }
    }



    Rectangle {
        id: topLine
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        y: 1
        color: XsStyle.mediaInfoBarBorderColour
    }

    Rectangle {
        id: bottomLine
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        y: parent.height-2
        color: XsStyle.mediaInfoBarBorderColour
    }*/
}

/*    ListModel {

        id: firstPartModel

        ListElement {
            labelText: "FileName"
            keyword: "filename"
            tooltip: "The filename of the current source"
            demotext: "The filename of the current source"
        }

    }

    ListModel {

        id: midPartModel

        ListElement {
            labelText: "Compare Layer"
            keyword: "compare_layer"
            tooltip: "In A/B compare mode, indicates which source you are viewing. Hit numeric keys to switch between sources."
            demotext: "A"
        }

    }

    ListModel {

        id: secondPartModel

        ListElement {
            labelText: "Format"
            keyword: "format"
            tooltip: "The image format or video codec of the current source"
            demotext: "OpenEXR"
        }
        ListElement {
            labelText: "Bit Depth"
            keyword: "bitdepth"
            tooltip: "The image bitdepth of the current source"
            demotext: "16 bit float"
        }
        ListElement {
            labelText: "FPS"
            keyword: "fps"
            tooltip: "The playback rate of the current source"
            demotext: "23.976"
        }
        ListElement {
            labelText: "Res"
            keyword: "resolution"
            tooltip: "The image resolution in pixels of the current source"
            demotext: "8888 x 8888"
        }
    }

    ListModel {
        id: pixColourModel
        ListElement {
            channel: "R"
            keyword: "red_pix_val"
        }
        ListElement {
            channel: "G"
            keyword: "green_pix_val"
        }
        ListElement {
            channel: "B"
            keyword: "blue_pix_val"
        }
    }

    Component {

        id: mediaInfoItemDelegate

        Row
        {
            spacing: 8
            Layout.fillWidth: keyword == 'filename'
            enabled: keyword == 'compare_layer' ? mediaInfoBar.offset_enabled : true

            Rectangle {
                width: mediaInfoBar.itemSpacing
                height: 10
                color: "transparent"
                visible: enabled ? (keyword != 'filename') : false
            }

            Label {

                id: label
                text: labelText
                color: XsStyle.controlTitleColor
                visible: enabled
                Layout.fillWidth: keyword == 'filename'
                property bool mouseHovered: mouseArea.containsMouse
                horizontalAlignment: Qt.AlignRight
                verticalAlignment: Qt.AlignVCenter

                font {
                    pixelSize: XsStyle.mediaInfoBarFontSize
                    family: XsStyle.controlTitleFontFamily
                    hintingPreference: Font.PreferNoHinting
                }

                TextMetrics {
                    id: textMetricsL
                    font: label.font
                    text: label.text
                }

                width: textMetricsL.width

                MouseArea {
                    id: mouseArea
                    cursorShape: Qt.PointingHandCursor
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: {
                        if(mouse.button & Qt.RightButton) {
                            contextMenu.popup()
                        }
                    }
                }

                onMouseHoveredChanged: {
                    if (mouseHovered) {
                        status_bar.normalMessage(tooltip, labelText)
                    } else {
                        status_bar.clearMessage()
                    }
                }
            }

            Label {

                id: value
                text: getMediaInfo(keyword)
                color: XsStyle.controlColor
                visible: enabled
                property bool fill_space: {keyword == 'filename'}
                width: {
                    return fill_space ? parent.width - 100 : textMetrics.width
                }
                Layout.fillWidth: fill_space
                elide: fill_space ? Text.ElideLeft : Text.ElideNone

                horizontalAlignment: Qt.AlignLeft
                verticalAlignment: Qt.AlignVCenter

                font {
                    pixelSize: XsStyle.mediaInfoBarFontSize
                    family: XsStyle.controlContentFontFamily
                    hintingPreference: Font.PreferNoHinting
                }

                TextMetrics {
                    id: textMetrics
                    font: value.font
                    text: value.fill_space ? value.text : demotext
                }

            }

        }
    }

    Component {

        id: pixColourDelegate
        Row
        {
            spacing: 4

            Label {

                id: label
                text: channel
                color: XsStyle.controlTitleColor
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 3
                verticalAlignment: Qt.AlignVCenter

                font {
                    pixelSize: XsStyle.mediaInfoBarFontSize
                    family: XsStyle.controlTitleFontFamily
                    hintingPreference: Font.PreferNoHinting
                }

                TextMetrics {
                    id: textMetrics
                    font: label.font
                    text: label.text
                }

                width: textMetrics.width
            }

            Label {

                id: value
                text: getMediaInfo(keyword)
                anchors.margins: 3
                color: XsStyle.controlColor
                // VCenter doesn't quite work with this fixed width font
                y: (parent.height-valTextMetrics.height)/2+1

                font {
                    pixelSize: XsStyle.pixValuesFontSize
                    family: XsStyle.pixValuesFontFamily
                    hintingPreference: Font.PreferNoHinting
                }

                TextMetrics {
                    id: valTextMetrics
                    font: value.font
                    text: value.text
                }

                width: 32

            }

            Label {
                width: 4
            }
        }
    }

    RowLayout {

        id: row_layout
        anchors.fill: parent
        Layout.fillWidth: true

        Rectangle {
            width: 5
        }

        Repeater {
            model: firstPartModel
            delegate: mediaInfoItemDelegate
        }

        Repeater {
            model: midPartModel
            delegate: mediaInfoItemDelegate
            visible: mediaInfoBar.offset_enabled
        }

        Rectangle {
            width: mediaInfoBar.itemSpacing
            height: 10
            color: "transparent"
            visible: mediaInfoBar.offset_enabled
        }

        Rectangle {

            color: label.mouseHovered ? XsStyle.highlightColor : XsStyle.mediaInfoBarBackground
            width: { label.width + offsetInputBox.width + 16}
            Layout.fillHeight: true
            Layout.topMargin: 3
            Layout.bottomMargin: 2
            visible: mediaInfoBar.offset_enabled

            id: offset_group

            Label {

                id: label
                text: 'Offset'
                color: enabled ? (mouseHovered ? "white" : XsStyle.controlTitleColor) : XsStyle.controlTitleColorDisabled
                enabled: mediaInfoBar.offset_enabled
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 3
                verticalAlignment: Qt.AlignVCenter
                property bool mouseHovered: mouseArea.containsMouse

                font {
                    pixelSize: XsStyle.mediaInfoBarFontSize
                    family: XsStyle.controlTitleFontFamily
                    hintingPreference: Font.PreferNoHinting
                }

                TextMetrics {
                    id: textMetricsL
                    font: label.font
                    text: label.text
                }
                width: textMetricsL.width

                MouseArea {

                    id: mouseArea
                    property var offsetStart: 0
                    property var xdown
                    property bool dragging: false
                    anchors.fill: parent
                    // We make the mouse area massive so the cursor remains
                    // as Qt.SizeHorCursor during dragging
                    anchors.margins: dragging ? -2048 : 0

                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: true
                    focus: true
                    cursorShape: containsMouse ? Qt.SizeHorCursor : Qt.ArrowCursor
                    onPressed: {
                        dragging = true
                        offsetStart = playhead.sourceOffsetFrames
                        xdown = mouseX
                        focus = true
                    }
                    onReleased: {
                        dragging = false
                        focus = false
                    }
                    onMouseXChanged: {
                        if (pressed) {
                            var new_offset = offsetStart + Math.round((mouseX - xdown)/10)
                            playhead.sourceOffsetFrames = new_offset
                        }
                    }

                }

                onMouseHoveredChanged: {
                    if (mouseHovered) {
                        status_bar.normalMessage("In a/b mode sets frame offset on this source relative to others. Click and drag this label to adjust with mouse.", "Source compare offset")
                    } else {
                        status_bar.clearMessage()
                    }
                }

            }

            Rectangle {

                color: enabled ? XsStyle.mediaInfoBarOffsetBgColour : XsStyle.mediaInfoBarOffsetBgColourDisabled
                border.color: enabled ? XsStyle.mediaInfoBarOffsetEdgeColour : XsStyle.mediaInfoBarOffsetEdgeColourDisabled
                border.width: 1
                width: offsetInput.font.pixelSize*2
                height: offsetInput.font.pixelSize*1.2
                id: offsetInputBox
                enabled: mediaInfoBar.offset_enabled
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 3

                TextInput {

                    id: offsetInput
                    text: playhead ? "" + playhead.sourceOffsetFrames : ""
                    Layout.minimumWidth: font.pixelSize*2
                    width: font.pixelSize*2
                    color: enabled ? XsStyle.controlColor : XsStyle.controlColorDisabled
                    selectByMouse: true
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter

                    font {
                        pixelSize: XsStyle.mediaInfoBarFontSize
                        family: XsStyle.controlContentFontFamily
                        hintingPreference: Font.PreferNoHinting
                    }

                    onEditingFinished: {
                        focus = false
                        playhead.sourceOffsetFrames = parseInt(text)
                    }
                }


            }
        }

        Repeater {
            model: secondPartModel
            delegate: mediaInfoItemDelegate
        }

        Rectangle {
            width: mediaInfoBar.itemSpacing
            height: 10
            color: "transparent"
            visible: mediaInfoBar.offset_enabled
        }

        Repeater {
            model: pixColourModel
            delegate: pixColourDelegate
        }

    }

}*/

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.15

import xStudio 1.0

Rectangle {

    color: label.mouseHovered ? XsStyle.highlightColor : XsStyle.mediaInfoBarBackground

    id: offset_group

    RowLayout {
        anchors.centerIn: parent
        Text {

            id: label
            text: 'Offset'
            color: enabled ? (mouseHovered ? "white" : XsStyle.controlTitleColor) : XsStyle.controlTitleColorDisabled
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
                    sessionWidget.playerWidget.viewport.forceActiveFocus()
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

            color: enabled ? XsStyle.mediaInfoBarOffsetBgColor : XsStyle.mediaInfoBarOffsetBgColorDisabled
            border.color: enabled ? XsStyle.mediaInfoBarOffsetEdgeColor : XsStyle.mediaInfoBarOffsetEdgeColorDisabled
            border.width: 1
            width: offsetInput.font.pixelSize*2
            height: offsetInput.font.pixelSize*1.2
            id: offsetInputBox

            TextInput {

                id: offsetInput
                text: playhead ? "" + playhead.sourceOffsetFrames : ""
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
                    sessionWidget.playerWidget.viewport.forceActiveFocus()
                }
            }
        }
    }
}
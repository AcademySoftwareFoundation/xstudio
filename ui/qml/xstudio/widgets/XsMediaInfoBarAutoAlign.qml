// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0
import xstudio.qml.module 1.0

Rectangle {

    color: "transparent"
    property string label_text
    property string tooltip_text
    width: tm1.width + theCheckbox.width + gap
    property int gap: 4
    height: 20
    property var font_family: XsStyle.mediaInfoBarFontSize
    property var font_size: XsStyle.mediaInfoBarFontSize
    property bool mouseHovered: mouseArea.containsMouse

    XsModuleAttributes {
        id: playhead_attrs
        attributesGroupName: "playhead"
    }

    Text {
        text: label_text
        color: XsStyle.controlTitleColor
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        id: theLabel
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    
        font {
            pixelSize: font_family
            family: font_size
            hintingPreference: Font.PreferNoHinting
        }

    }

    TextMetrics {
        id: tm1
        font: theLabel.font
        text: theLabel.text
    }

    XsCheckbox {
        id: theCheckbox
        width: theLabel.font.pixelSize*1.2
        height: theLabel.font.pixelSize*1.2
        anchors.left: theLabel.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 5

        checked: playhead_attrs.auto_align !== undefined ? playhead_attrs.auto_align : false
        onTriggered: {
            playhead_attrs.auto_align = !playhead_attrs.auto_align
        }
    }

    MouseArea {

        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
    }

    onMouseHoveredChanged: {
        if (mouseHovered) {
            status_bar.normalMessage(tooltip_text, label_text)
        } else {
            status_bar.clearMessage()
        }
    }

}
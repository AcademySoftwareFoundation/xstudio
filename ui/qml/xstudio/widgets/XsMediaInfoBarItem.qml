// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.15

import xStudio 1.0

Rectangle {

    color: "transparent"
    property string label_text
    property string value_text
    property string tooltip_text
    property var font_family: XsStyle.mediaInfoBarFontSize
    property var font_size: XsStyle.mediaInfoBarFontSize
    property bool mouseHovered: mouseArea.containsMouse

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

    RowLayout {
        anchors.centerIn: parent

        Text {

            text: label_text
            color: XsStyle.controlTitleColor
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            id: theLabel
            Layout.fillHeight: true
        
            font {
                pixelSize: font_family
                family: font_size
                hintingPreference: Font.PreferNoHinting
            }

        }

        Text {

            text: value_text
            color: XsStyle.controlColor
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            id: theValue
            Layout.fillHeight: true
        
            font {
                pixelSize: font_family
                family: font_size
                hintingPreference: Font.PreferNoHinting
            }
    
        }
    }

    /*

    TextMetrics {
        id: tm2
        font: theValue.font
        text: theValue.text
    }

    */

}
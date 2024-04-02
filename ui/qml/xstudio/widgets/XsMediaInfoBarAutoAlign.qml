// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0


Rectangle {

    color: "transparent"
    property string label_text
    property string tooltip_text
    property int gap: 4
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
        Layout.margins: 0

        Text {

            text: label_text
            color: XsStyle.controlTitleColor
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            id: theLabel
            Layout.fillHeight: true
            anchors.margins: 3
        
            font {
                pixelSize: font_family
                family: font_size
                hintingPreference: Font.PreferNoHinting
            }

            TextMetrics {
                id: tm1
                font: theLabel.font
                text: theLabel.text
            }
        }


        XsModuleAttributesModel {
            id: align_attr
            attributesGroupNames: "playhead_align_mode"
        }

        Repeater {

            // Using a repeater here - but 'playhead_align_mode' only
            // has one row by the way. The use of a repeater means the model role
            // data are all visible in the XsComboBox instance.
            model: align_attr
            Layout.fillHeight: true

            XsComboBox {

                width: 90
                Layout.fillHeight: true

                model: combo_box_options
                property var value_: value ? value : null
                onValue_Changed: {
                    currentIndex = indexOfValue(value_)
                }
                Component.onCompleted: currentIndex = indexOfValue(value_)
                onCurrentValueChanged: {
                    value = currentValue;
                }
                downArrowVisible: false
            }
        }

    }
    
}
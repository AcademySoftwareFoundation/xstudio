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
    width: tm1.width + popup_container.width + gap
    property int gap: 4
    height: parent.height
    property var font_family: XsStyle.mediaInfoBarFontSize
    property var font_size: XsStyle.mediaInfoBarFontSize
    property bool mouseHovered: mouseArea.containsMouse

    Text {

        text: label_text
        color: XsStyle.controlTitleColor
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        id: theLabel
        anchors.top: parent.top
        anchors.bottom: parent.bottom
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


    Rectangle {
        height: parent.height
        width: 90
        Layout.fillWidth: true
        color: "transparent"
        anchors.left: theLabel.right
        anchors.margins: 3
        id: popup_container

        XsModuleAttributesModel {
            id: align_attr
            attributesGroupName: "playhead_align_mode"
        }

        Repeater {

            // Using a repeater here - but 'vp_mouse_wheel_behaviour_attr' only
            // has one row by the way. The use of a repeater means the model role
            // data are all visible in the XsComboBox instance.
            model: align_attr

            XsComboBox {

                width: 110
                anchors.fill: parent
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
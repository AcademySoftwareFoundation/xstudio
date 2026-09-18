// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import xStudio 1.0
import xstudio.qml.models 1.0

Flickable {

    id: flick
    clip: true
    contentHeight: contentItem.childrenRect.height

    ScrollBar.vertical: XsScrollBar {
        id: scrollbar
        width: 10
        visible: flick.height < grid.height
    }


    XsAttributeValue {
        id: __audio_sync
        attributeTitle: "Audio Sync Delay"
        model: decklink_settings
    }
    property alias audio_sync: __audio_sync.value


    XsAttributeValue {
        id: __video_sync
        attributeTitle: "Video Sync Delay"
        model: decklink_settings
    }
    property alias video_sync: __video_sync.value

    XsAttributeValue {
        id: __auto_mute
        attributeTitle: "Auto Disable PC Audio"
        model: decklink_settings
    }
    property alias auto_mute: __auto_mute.value


    GridLayout {

        id: grid
        columns: 2
        rowSpacing: 10
        columnSpacing: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top

        XsText {
            text: "SDI Output"
            Layout.alignment: Qt.AlignRight
        }

        DecklinkOutputButton {

            Layout.preferredHeight: 20
            Layout.preferredWidth: 80
            Layout.alignment: Qt.AlignHCenter | Qt.AlignLeft

        }

        XsText {
            text: "Resolution"
            Layout.alignment: Qt.AlignRight
        }

        DecklinkMultichoiceSetting {
            attr_name: "Output Resolution"
            attrs_model: decklink_settings
            Layout.preferredHeight: 24
            Layout.preferredWidth: widgetWidth
            Layout.alignment: Qt.AlignHCenter | Qt.AlignLeft
            enabled: !startStop

            hoverEnabled: true            
            XsToolTip {
                text: "Set the SDI output resolution.."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }

        }

        XsText {
            text: "Frame Rate"
            Layout.alignment: Qt.AlignRight
        }

        DecklinkMultichoiceSetting {
            attr_name: "Refresh Rate"
            attrs_model: decklink_settings
            Layout.preferredHeight: 24
            Layout.preferredWidth: widgetWidth
            Layout.alignment: Qt.AlignHCenter | Qt.AlignLeft
            enabled: !startStop

            hoverEnabled: true            
            XsToolTip {
                text: "Set the SDI output refresh rate. For best results this should match the media you are viewing or be somewhat higher (e.g. 60Hz for 24fps media should also work well)."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }

        }

        XsText {
            text: "Pixel Format"
            Layout.alignment: Qt.AlignRight
        }

        DecklinkMultichoiceSetting {
            attr_name: "Pixel Format"
            attrs_model: decklink_settings
            Layout.preferredHeight: 24
            Layout.preferredWidth: widgetWidth
            Layout.alignment: Qt.AlignHCenter | Qt.AlignLeft
            enabled: !startStop

            hoverEnabled: true            
            XsToolTip {
                text: "Set the SDI output pixel format."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }

        }

        XsText {
            text: "Track Zoom/Pan"
            Layout.alignment: Qt.AlignRight
        }

        XsCheckBox {

            Layout.alignment: Qt.AlignLeft
            checked: track_zoom
            onClicked: track_zoom = !track_zoom
            Layout.preferredHeight: 24

            XsToolTip {
                text: "When checked, the SDI output will match the zoom/pan state of the viewport in the main xSTUDIO UI. Otherwise, the image will be fitted into the SDI output are according to the Fit Mode (below)."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }
        }

        XsText {
            text: "Fit Mode"
            Layout.alignment: Qt.AlignRight
        }

        DecklinkMultichoiceSetting {
            attr_name: "Fit (F)"
            attrs_model: decklink_viewport_attributes
            Layout.preferredHeight: 24
            Layout.preferredWidth: widgetWidth
            Layout.alignment: Qt.AlignHCenter | Qt.AlignLeft
            enabled: !track_zoom
        }

        XsText {
            text: "OCIO Display"
            Layout.alignment: Qt.AlignRight
        }

        DecklinkMultichoiceSetting {
            attr_name: "Display"
            attrs_model: decklink_viewport_attributes
            Layout.preferredHeight: 24
            Layout.preferredWidth: widgetWidth
            Layout.alignment: Qt.AlignHCenter | Qt.AlignLeft

            hoverEnabled: true            
            XsToolTip {
                text: "Set the OpenColorIO Display profile for the SDI output stream."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }

        }

        XsText {
            text: "Audio Sync Adjust / millisecs"
            Layout.alignment: Qt.AlignRight
        }

        XsTextField {

            Layout.alignment: Qt.AlignLeft
            Layout.preferredHeight: 24                
            Layout.preferredWidth: widgetWidth/2
                        
            text: "" + audio_sync
            width: 24
            selectByMouse: true

            property var valFollower: audio_sync
            onValFollowerChanged: {
                text = "" + audio_sync
            }
            onEditingFinished: {
                focus = false
                audio_sync = parseInt(text)
            }

            hoverEnabled: true            

            XsToolTip {
                text: "This value sets a delay (measured in milliseconds) applied to the audio signal vs. the video frames in the SDI stream. If your set-up uses a SDI->HDMI converter box and the audio signal is split out and sent to an amplifier, for example, you may find that the audio sync with the video on your display is affected. By adjusting this value you can compensate to achieve perfect sync between video and audio."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }

        }        

        XsText {
            text: "Auto Mute PC Audio"
            Layout.alignment: Qt.AlignRight
        }

        XsCheckBox {

            Layout.alignment: Qt.AlignLeft
            checked: auto_mute
            onClicked: auto_mute = !auto_mute
            Layout.preferredHeight: 24

            hoverEnabled: true            

            XsToolTip {
                text: "Check this box if you want xSTUDIO to automatically mute the audio output (on the host PC) when the SDI output is active. This is useful if your PC is wired to a separate amp/speaker system to the SDI output and you don't want to get audio through the PC when using the SDI signal."
                visible: parent.hovered
                maxWidth: flick.width*2/3
            }

        }

    }
}

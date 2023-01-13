// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3

import xStudio 1.0


Rectangle {
    id: mainControls

    color: XsStyle.mainBackground

    implicitHeight: XsStyle.transportHeight * opacity

    property int buttonPadding: 7

    visible: opacity !== 0
    opacity: 1
    Behavior on opacity {
        NumberAnimation { duration: playerWidget.doTrayAnim?200:0 }
    }

    property var playheadFrame: playhead ? playhead.frame + 1 : 1
    property var playheadFrames: playhead ? playhead.frames : 1

    RowLayout {

        anchors.fill: parent
        anchors.topMargin: 2
        anchors.bottomMargin: 2

        anchors.leftMargin: spacing
        anchors.rightMargin: spacing

        // The top timeline row.
        id: toolBar

        spacing: 4


        /*XsTray {
            id: leftTray
            anchors.left: parent.left
            color: XsStyle.controlBackground

            XsTrayDivider {}
        }*/
        XsTransportTray {
            id: transportTray
            Layout.fillHeight: true
            pad: mainControls.buttonPadding
        }


        XsTimelinePositionWidget {
            id:currentTimeText
            value: playheadFrame
            Layout.fillHeight: true
        }

        XsTimelineSlider {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsTimelineDurationWidget {
            id: durationTimeText
            value: {preferences.timeline_indicator.value === "Duration" ? playheadFrames : playheadFrames-playheadFrame }
            Layout.fillHeight: true
        }

        XsVolumeWidget {
            Layout.fillHeight: true
            //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
            buttonPadding: mainControls.buttonPadding
        }

        XsLoopWidget {
            Layout.fillHeight: true
            //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
            buttonPadding: mainControls.buttonPadding
        }

        XsSnapshotWidget {
            Layout.fillHeight: true
            //icon.color: hovered ? XsStyle.controlColor : XsStyle.controlTitleColor
            buttonPadding: mainControls.buttonPadding
        }

        XsTrayButton {

            Layout.fillHeight: true
            text: "Pop Out"
            icon.source: "qrc:/icons/popout_viewer.png"
            tooltip: "Open the pop-out interface."
            buttonPadding: mainControls.buttonPadding
            visible: sessionWidget.window_name == "main_window"
            onClicked: {
                app_window.togglePopoutViewer()
            }
            toggled_on: app_window ? app_window.popout_window ? app_window.popout_window.visible : false : false
    
        }
    }

}

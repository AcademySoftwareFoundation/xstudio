// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.1
import QtQuick.Layouts 1.12

import xStudio 1.0

RowLayout {

    property int pad: 0

    XsTrayButton {
        Layout.fillHeight: true
        text: "Session Pane"
        icon.source: "qrc:/icons/session.png"
        buttonPadding: pad
        prototype: true
        //toggled_on: sessionWidget.session_pane_visible

        // checked: mediaDialog.visible
        tooltip: "Hide/show the session pane (playlists)"

        onClicked: {
            //sessionWidget.toggleSessionPane()
        }

    }

    XsTrayButton {
        Layout.fillHeight: true
        text: "Media Pane"
        icon.source: "qrc:/icons/media.png"
        buttonPadding: pad
        prototype: true
        //toggled_on: sessionWidget.media_pane_visible

        // checked: mediaDialog.visible
        tooltip: sessionWidget.viewer_pane_visible ? "Hide the media list pane": "Show the media list pane"

        onClicked: {
            //sessionWidget.toggleMediaPane()
        }

    }


    XsTrayButton {
        Layout.fillHeight: true
        text: "Timeline Pane"
        icon.source: "qrc:/icons/timeline.png"
        buttonPadding: pad
        //toggled_on: sessionWidget.timeline_pane_visible
        prototype: true

        // checked: mediaDialog.visible
        tooltip: sessionWidget.timeline_pane_visible ? "Hide the timeline pane": "Show the timeline pane"

        onClicked: {
            //sessionWidget.toggleTimelinePane()
        }

    }

    XsTrayButton {
        Layout.fillHeight: true
        text: "Viewer panel"
        icon.source: "qrc:/icons/viewer.png"
        buttonPadding: pad
        prototype: true
        //toggled_on: sessionWidget.player_pane_visible

        // checked: mediaDialog.visible
        tooltip: sessionWidget.viewer_pane_visible ? "Hide the viewer pane": "Show the viewer pane"

        onClicked: {
            //sessionWidget.toggleViewerPane()
        }

    }

    XsTrayButton {
        Layout.fillHeight: true
        text: "Settings Dialog"
        source: "qrc:///feather_icons/settings.svg"
        buttonPadding: pad
        prototype: false
        tooltip: "Open the Display Settings Panel.  Configure the HUD, Mask."
        toggled_on: sessionWidget.settings_dialog_visible

        onClicked: {
            sessionWidget.toggleSettingsDialog()
        }


    }


}
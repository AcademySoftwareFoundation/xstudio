// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

import xStudio 1.0

Rectangle {

    id: popupBox
    property int edge_space: 8
    width: layout.width
    height: layout.height + edge_space*2
    visible: opacity != 0
    opacity: 0
    property var text: ""
    radius: XsStyle.menuRadius
    property real target_y: 0.0
    y: target_y-height
    clip: true
    focus: true

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    border {
        color: XsStyle.menuBorderColor
        width: XsStyle.menuBorderWidth
    }
    color: XsStyle.mainBackground

    function toggleShow() {
        if (visible) {
            opacity = 0
            focus = false
        }
        else 
        {
            opacity = 1
            focus = true
        }
    }

    ColumnLayout {

        id: layout
        y: edge_space
        spacing: 3

        XsSimpleMenuItem {
            text: "Compact Layout"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.compact_mode
            onTriggered: {
                sessionWidget.playerWidget.compactModeOn()
            }
        }

        XsSimpleMenuItem {
            text: "Expert Layout"
            Layout.fillWidth: true
            checked: !sessionWidget.playerWidget.compact_mode
            onTriggered: {
                sessionWidget.playerWidget.compactModeOff()
            }
        }

        Rectangle {
            height: 1
            color: XsStyle.menuBorderColor
            Layout.fillWidth: true
        }

        XsSimpleMenuItem {
            text: "Media Info Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.media_info_bar_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleMediaInfoBarVisible()
            }
        }

        XsSimpleMenuItem {
            text: "Tool Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.tool_bar_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleToolBarVisible()
            }
        }

        XsSimpleMenuItem {
            text: "Transport Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.transport_controls_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleTransportControlsVisible()                
            }
        }

        XsSimpleMenuItem {
            text: "Layout Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.layout_bar_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleLayoutBarVisible()                                
            }
        }

        XsSimpleMenuItem {
            text: "Viewport Title Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.viewport_title_bar_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleViewportTitleBarVisible()                                
            }
        }

        XsSimpleMenuItem {
            text: "Status Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.status_bar_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleStatusBarVisible()                                                
            }
        }

        /*Rectangle {
            height: 1
            color: XsStyle.menuBorderColor
            Layout.fillWidth: true
        }

        XsSimpleMenuItem {
            text: "Menu Bar"
            Layout.fillWidth: true
            checked: sessionWidget.playerWidget.menu_bar_visible
            onTriggered: {
                sessionWidget.playerWidget.toggleMenuBarVisible()                                
            }
        }*/

        Rectangle {
            height: 1
            color: XsStyle.menuBorderColor
            Layout.fillWidth: true
        }

        Rectangle {

            color: "transparent"
            Layout.leftMargin: 18
            width: m8.width + t8.width + 20
            height: m8.height
            
            XsSimpleMenuItem {
                anchors.left: parent.left
                text: sessionWidget.playerWidget.controlsVisible ? "Hide All UI Bars" : "Show UI Bars"
                id: m8
                checkable: false
                onTriggered: {
                    sessionWidget.playerWidget.toggleControlsVisible()
                }
            }

            Text {
                text: "Ctrl + H"
                id: t8
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: XsStyle.popupControlFontSize
                font.family: XsStyle.controlTitleFontFamily
                color: XsStyle.highlightColor
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
    
            }

        }

    }

}
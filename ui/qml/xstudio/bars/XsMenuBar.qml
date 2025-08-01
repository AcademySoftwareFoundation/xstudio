// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12

import xStudio 1.0

MenuBar  {
    id:myMenuBar

    height: XsStyle.menuBarHeight*opacity
    implicitHeight: XsStyle.menuBarHeight*opacity
    visible: opacity != 0.0
    background: Rectangle {color: XsStyle.mainBackground}
    property var playerWidget: undefined
    property var playhead: viewport.playhead

    property alias playerWidget: myMenuBar.playerWidget
    property alias playhead: myMenuBar.playhead

    property alias panelMenu: panel_menu
    property alias mediaMenu: media_menu

    // property alias panelMenu: myMenuBar.panel_menu

    delegate: MenuBarItem {
        id: menuBarItem
        height: XsStyle.menuBarHeight
        contentItem: Text {
            text: XsUtils.replaceMenuText(menuBarItem.text)
            font.pixelSize: XsStyle.menuFontSize
            font.family: XsStyle.menuFontFamily
            font.hintingPreference: Font.PreferNoHinting
            opacity: enabled ? 1.0 : 0.3
            color: XsStyle.hoverColor//menuBarItem.highlighted ? XsStyle.hoverColor : XsStyle.mainColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            x: 30
        }
        background: Rectangle {
//            color: menuBarItem.menu.opened?XsStyle.highlightColor : "transparent"
            color: menuBarItem.menu.opened?XsStyle.primaryColor: "transparent"
            gradient:menuBarItem.menu.opened?styleGradient.accent_gradient:Gradient.gradient
        }
        onFocusChanged: {
            // this prevents stealing keypresses that might be needed elsewhere.
            // (like space for play/pause-- not opening a menu)
            //focus = false
        }

    }
    XsFileMenu {}
    XsEditMenu {}
    XsPlaylistMenu {}
    XsMediaMenu {
        id: media_menu
    }
    XsTimelineMenu {}
    XsPlaybackMenu {
        id: playback_menu
    }
    XsViewerContextMenu {
    }
    XsLayoutMenu {}
    XsSnapshotMenu {}
    XsPanelMenu {
        id: panel_menu
    }
    XsMenu {
        title: qsTr("Publish")
        id: publish_menu_toplevel
        XsModuleMenuBuilder {
            id: publish_menu
            parent_menu: publish_menu_toplevel
            root_menu_name: "publish_menu"
        }
    }

    XsMenu {
        title: qsTr("Colour")
        id: colour_menu_toplevel
        XsModuleMenuBuilder {
            parent_menu: colour_menu_toplevel
            root_menu_name: "Colour"
        }
    }

    XsHelpMenu {}


}


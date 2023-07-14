// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14
import QtQuick.Layouts 1.4

import xStudioReskin 1.0
import xstudio.qml.models 1.0
import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0

ApplicationWindow { 
    
    id: appWindow
    visible: true
    color: "#00000000"
    title: "xSTUDIO Re-Skin UI (Under Construction)"

    property var window_name: "main_window"

    // override default palette
    palette.base: XsStyleSheet.panelBgColor        //"#414141" //XsStyle.controlBackground
    palette.highlight: XsStyleSheet.accentColor   //"#D17000" //"#bb7700"    //highlightColor
    palette.text: XsStyleSheet.primaryTextColor                       //XsStyle.hoverColor
    palette.buttonText: XsStyleSheet.primaryTextColor
    palette.windowText: XsStyleSheet.primaryTextColor
    palette.button: Qt.darker( "#414141" , 2.4) //XsStyle.controlTitleColor
    palette.light: "#bb7700"                    //highlightColor
    palette.highlightedText: Qt.darker( "#414141" , 2.0) //XsStyle.mainBackground
    palette.brightText: "#bb7700"               //highlightColor
   
    XsReskinPanelsLayoutModel {

        id: frontend_model
        property var root_index: frontend_model.search_recursive(window_name, "window_name")
        onJsonChanged: {
            root_index = frontend_model.search_recursive(window_name, "window_name")
        }
    }

    onXChanged: {
        frontend_model.set(frontend_model.root_index, x, "position_x")
    }

    onYChanged: {
        frontend_model.set(frontend_model.root_index, y, "position_y")
    }

    onWidthChanged: {
        frontend_model.set(frontend_model.root_index, width, "width")
    }

    onHeightChanged: {
        frontend_model.set(frontend_model.root_index, height, "height")
    }
    
    property alias playlistModel: sessionModel.playlists

    property alias sessionModel: sessionModel

    XsSessionModel {
        id: sessionModel
        sessionActorAddr: studio.sessionActorAddr

        onPlaylistsChanged: {
            let ind = sessionModel.search_recursive("Playlist", "typeRole")
            if(ind.valid) {
                screenSource.index = ind
            }
        }

    }

    property alias screenSource: screenSource
    
    XsModelPropertyMap {
        id: screenSource
        index: sessionModel.index(-1,-1)
        onIndexChanged: {
            sessionModel.setPlayheadTo(index)
        }
    }

    property var viewport

    Item {
        anchors.fill: parent
        Keys.forwardTo: viewport
        focus: true
    }
    
    ColumnLayout {

        anchors.fill: parent
        spacing: 0

        XsMainMenuBar {
            Layout.fillWidth: true
        }

        XsSplitPanel {
            id: root_panel
            Layout.fillWidth: true
            Layout.fillHeight: true
            panels_layout_model: frontend_model
            panels_layout_model_index: frontend_model.root_index
        }
    
    }

    XsViewsModel {
        id: views_model
    }

    Component.onCompleted: {

        views_model.register_view("../views/media/XsMedialist.qml", "Media List")
        views_model.register_view("../views/playlists/XsPlaylists.qml", "Playlists View")
        views_model.register_view("../views/timeline/XsTimeline.qml", "Timeline View")
        views_model.register_view("../views/viewport/XsViewport.qml", "Player Viewport")

        x = frontend_model.get(frontend_model.root_index, "position_x")
        y = frontend_model.get(frontend_model.root_index, "position_y")
        width = frontend_model.get(frontend_model.root_index, "width")
        height = frontend_model.get(frontend_model.root_index, "height")
    }

}


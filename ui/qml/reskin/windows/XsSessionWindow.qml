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
    palette.base: XsStyleSheet.panelBgColor
    palette.highlight: XsStyleSheet.accentColor //== "#666666" ? Qt.lighter(XsStyleSheet.accentColor, 1.5) : XsStyleSheet.accentColor
    palette.text: XsStyleSheet.primaryTextColor
    palette.buttonText: XsStyleSheet.primaryTextColor
    palette.windowText: XsStyleSheet.primaryTextColor
    palette.button: Qt.darker("#414141", 2.4)
    palette.light: "#bb7700"
    palette.highlightedText: Qt.darker("#414141", 2.0)
    palette.brightText: "#bb7700"
   
    FontLoader {id: fontInter; source: "qrc:/fonts/Inter/Inter-Regular.ttf"}

    XsReskinPanelsLayoutModel {

        id: ui_layouts_model
        property var root_index: ui_layouts_model.search_recursive(window_name, "window_name")
        onJsonChanged: {
            root_index = ui_layouts_model.search_recursive(window_name, "window_name")
        }
    }

    onXChanged: {
        ui_layouts_model.set(ui_layouts_model.root_index, x, "position_x")
    }

    onYChanged: {
        ui_layouts_model.set(ui_layouts_model.root_index, y, "position_y")
    }

    onWidthChanged: {
        ui_layouts_model.set(ui_layouts_model.root_index, width, "width")
    }

    onHeightChanged: {
        ui_layouts_model.set(ui_layouts_model.root_index, height, "height")
    }
    
    /*****************************************************************
     * 
     * The sessionData entry is THE entry point to xstudio's backend
     * data. Most of the session state can be reached through this
     * 2D tree of data. Developers need to know about how QAbstractItemModel
     * c++ class works in the QML layer to understand how we index
     * into the sessionData object and use it as a model at different 
     * levels to instance UI elements to display all information about playlists,
     * media, timelines, playheads and so-on.
     * 
     ****************************************************************/
    XsSessionData {
        id: sessionData
        sessionActorAddr: studio.sessionActorAddr
    }

    // Makes these important global items visible in child contexts 
    property alias theSessionData: sessionData.session
    property alias sessionSelectionModel: sessionData.sessionSelectionModel
    property alias globalStoreModel: sessionData.globalStoreModel
    property alias mediaSelectionModel: sessionData.mediaSelectionModel
    property alias playlistsModelData: sessionData.playlistsModelData
    property alias viewedMediaSetIndex: sessionData.viewedMediaSetIndex
    property alias selectedMediaSetIndex: sessionData.selectedMediaSetIndex
    property alias viewedMediaSetProperties: sessionData.viewedMediaSetProperties
    property alias selectedMediaSetProperties: sessionData.selectedMediaSetProperties
    property alias currentPlayheadData: sessionData.current_playhead_data
    property alias callbackTimer: sessionData.callbackTimer

    property var child_dividers: []

    property var layouts_model
    property var layouts_model_root_index

    ColumnLayout {

        anchors.fill: parent
        spacing: 1 
 
        RowLayout{
            Layout.fillWidth: true
            spacing: 1

            XsMainMenuBar { id: menuBar
                Layout.preferredWidth: parent.width*1.85/3
            }
            XsLayoutModeBar { 
                id: layoutBar
                layouts_model: ui_layouts_model
                layouts_model_root_index: ui_layouts_model.root_index
                Layout.fillWidth: true 
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            XsSplitPanel {
                id: root_panel
                panels_layout_model: ui_layouts_model
                panels_layout_model_index: layoutBar.current_layout_index
            }
        }
    
    }

    XsViewsModel {
        id: views_model
    }

    Component.onCompleted: {

        views_model.register_view("qrc:/views/playlists/XsPlaylists.qml", "Playlists")
        views_model.register_view("qrc:/views/media/XsMedialist.qml", "Media")
        views_model.register_view("qrc:/views/viewport/XsViewport.qml", "Viewport")
        views_model.register_view("qrc:/views/timeline/XsTimeline.qml", "Timeline")
        
        x = ui_layouts_model.get(ui_layouts_model.root_index, "position_x")
        y = ui_layouts_model.get(ui_layouts_model.root_index, "position_y")
        width = ui_layouts_model.get(ui_layouts_model.root_index, "width")
        height = ui_layouts_model.get(ui_layouts_model.root_index, "height")
    }

}


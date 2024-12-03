import QtQuick 2.12
import QtQuick.Layouts 1.15

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "./widgets"

Rectangle{
    id: viewportWidget

    color: "transparent"
    anchors.fill: parent
    property color gradient_colour_top: "#5C5C5C"
    property color gradient_colour_bottom: "#474747"

    property var view: viewLayout
    focus: true

    Item {
        anchors.fill: parent
        focus: true
        Keys.forwardTo: view
    }

    property real panelPadding: elementsVisible || tabBarVisible ||
        actionBarVisible || infoBarVisible ||
        toolBarVisible || transportBarVisible ? XsStyleSheet.panelPadding : 0

    Behavior on panelPadding {NumberAnimation{ duration: 150 }}

    XsGradientRectangle{
        anchors.fill: actionBar
    }
    XsViewportActionBar{
        id: actionBar
        anchors.top: parent.top
        actionbar_model_data_name: view.name + "_actionbar"
        opacity: elementsVisible && actionBarVisible ? 1 : 0
        showContextMenu: viewport.showContextMenu

        visible: opacity != 0.0
        Behavior on opacity {NumberAnimation{ duration: 150 }}

    }

    XsGradientRectangle{
        anchors.top: infoBar.top
        anchors.bottom: infoBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        topPosition: -y/height
        bottomPosition: parent.height/height + topPosition
    }
    XsViewportInfoBar{
        id: infoBar
        anchors.top: actionBar.bottom
        opacity: elementsVisible && infoBarVisible ? 1 : 0

        visible: opacity != 0.0
        Behavior on opacity {NumberAnimation{ duration: 150 }}
    }

    property color gradient_dark: "black"
    property color gradient_light: "white"

    XsPlayhead {
        id: viewportPlayhead
        uuid: view.playheadUuid
    }

    property alias viewportPlayhead: viewportPlayhead

    /*
    // Leaving this here for posterity. Uncommenting this will result in
    // debug spam telling you about data in viewportPlayheadDataModel.
    // It gives you an idea about what 'XsModuleData' data does.
    XsModuleData {
        id: viewportPlayheadDataModel

        // each playhead exposes its attributes in a model named after the
        // playhead UUID. We connect to the model like this:
        modelDataName: viewportPlayhead.uuid
    }
    Repeater {
        model: viewportPlayheadDataModel
        Item {
            property var value_: value
            onValue_Changed: {
                console.log("playhead attr changed", title, value)
            }
            Component.onCompleted: {
                console.log("playhead attr: ", title, value)
            }
        }
    }
    */

    property var visible_doc_widgets: []
    
    property var dock_widgets_model: []

    onDock_widgets_modelChanged: {

        if (dock_widgets_model.length == 0) return
        // update the list of all docked widgets which are visible
        var v = []
        for (var i = 0; i < dock_widgets_model.length; ++i) {
            if (dock_widgets_model[i][1]) {
                v.push(dock_widgets_model[i][0])
            }
        }
        visible_doc_widgets = v
    }

    function toggle_dockable_widget(widget_name) {

        var u = dock_widgets_model
        for (var i = 0; i < u.length; ++i) {
            if (u[i][0] == widget_name) {
                // found a match. toggle visibility
                u[i][1] = !u[i][1]
                dock_widgets_model = u
                return
            }
        }
        var new_entry = [widget_name, true, "left"]
        u.push(new_entry)
        dock_widgets_model = u

    }

    function move_dockable_widget(widget_name, placement) {

        var u = dock_widgets_model
        for (var i = 0; i < u.length; ++i) {
            var w = u[i]
            if (u[i][0] == widget_name) {
                // found a match. toggle visibility
                u[i][2] = placement
                dock_widgets_model = u
                return
            }
        }

    }

    XsLabel {
        text: "Media Not Found"
        color: XsStyleSheet.hintColor
        anchors.centerIn: parent
        font.pixelSize: XsStyleSheet.fontSize*1.2
        font.weight: Font.Medium
        visible: false //#TODO
    }

    RowLayout {

        id: viewLayout
        anchors.top: infoBar.bottom
        anchors.bottom: toolBar.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        XsLeftRightDockedTools {
            Layout.fillHeight: true
            dockedWidgetsModel: dock_widgets_model
            Layout.preferredWidth: contentItem.childrenRect.width
        }


        ColumnLayout {
            spacing: 0

            XsTopBottomDockedTools {
                Layout.fillWidth: true
                dockedWidgetsModel: dock_widgets_model
                Layout.preferredHeight: contentItem.childrenRect.height
                placement: "top"
            }

            XsViewport { id: viewport
                z: -100
                Layout.fillWidth: true
                Layout.fillHeight: true
                Component.onCompleted: {
                    viewportWidget.view = viewport
                }

            }

            XsTopBottomDockedTools {
                Layout.fillWidth: true
                dockedWidgetsModel: dock_widgets_model
                Layout.preferredHeight: contentItem.childrenRect.height
                placement: "bottom"
            }
        }

        XsLeftRightDockedTools {
            Layout.fillHeight: true
            dockedWidgetsModel: dock_widgets_model
            placement: "right"
            Layout.preferredWidth: contentItem.childrenRect.width
        }

    }

    XsGradientRectangle{
        anchors.fill: toolBar
    }
    XsViewportToolBar{
        id: toolBar
        anchors.bottom: transportBar.top
        toolbar_model_data_name: view.name + "_toolbar"
        opacity: elementsVisible && toolBarVisible ? 1 : 0

        visible: opacity != 0.0
        Behavior on opacity {NumberAnimation{ duration: 150 }}
    }

    XsGradientRectangle{
        anchors.fill: transportBar
    }
    XsViewportTransportBar {
        id: transportBar
        anchors.bottom: parent.bottom
        opacity: elementsVisible && transportBarVisible ? 1 : 0

        visible: opacity != 0.0
        Behavior on opacity {NumberAnimation{ duration: 150 }}
    }


    property var currentLayout: isPopoutViewer? "popout" : appWindow.layoutName
    onCurrentLayoutChanged:{
        if (currentLayout !== "Present") {
            menuBarVisible = true
        }
        // else{
        //     menuBarVisible = elementsVisible
        // }
    }

    property bool elementsVisible: true
    onElementsVisibleChanged: {
        if(currentLayout == "Present") {
            menuBarVisible = elementsVisible
        }

        if (typeof forceHideTabs !== "undefined") {
            if(elementsVisible && tabBarVisible){
                forceHideTabs = false
            } else if(!elementsVisible){
                forceHideTabs = true
            }
        }
    }
    property bool menuBarVisible: true
    onMenuBarVisibleChanged:{
        appWindow.set_menu_bar_visibility(menuBarVisible)
    }
    property bool tabBarVisible: true
    onTabBarVisibleChanged:{
        if (typeof forceHideTabs !== "undefined") {
            forceHideTabs = !tabBarVisible
        }
    }
    property bool actionBarVisible: isQuickview? false : true
    property bool infoBarVisible: isQuickview? false : true
    property bool toolBarVisible: isQuickview? false : true
    property bool transportBarVisible: true

    XsStoredPanelProperties {
        propertyNames: ["dock_widgets_model", "tabBarVisible", "transportBarVisible", "toolBarVisible", "infoBarVisible", "actionBarVisible"]
    }

    
}
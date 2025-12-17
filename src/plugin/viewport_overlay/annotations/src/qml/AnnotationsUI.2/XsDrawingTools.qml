// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.bookmarks 1.0

Item {

    id: drawDialog

    objectName: "XStudioPanel"
    anchors.fill: parent
    anchors.rightMargin: horizontal ? 90 : 0

    // this var is REQUIRED to set the vertical size of the widget
    property var dockWidgetSize: horizontal ? XsStyleSheet.primaryButtonStdHeight + 4 : 90

    property real buttonWidth: 0
    property real buttonHeight: 20

    property real colSpacing: buttonHeight
    property real itemSpacing: 1
    property real framePadding: XsStyleSheet.panelPadding/2
    property color toolInactiveTextColor: XsStyleSheet.secondaryTextColor

    property real fontSize: XsStyleSheet.fontSize
    property string fontFamily: XsStyleSheet.fontFamily
    property color textValueColor: "white"

    property int maxDrawSize: 250
    property bool horizontal: false

    property bool isAnyToolSelected: currentTool !== "None" && currentTool !== "Colour Picker"

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: annotations_model_data
        modelDataName: "annotations_tool_settings"
    }

    /* Here we locate particular nodes in the annotations_model_data giving
    convenient access to backend data. Seems crazy but this is the QML way! */
    XsAttributeValue {
        id: active_tool
        attributeTitle: "Active Tool"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: tool_types_choices
        role: "combo_box_options"
        attributeTitle: "Active Tool"
        model: annotations_model_data
    }

    property alias currentTool: active_tool.value
    property alias toolChoices: tool_types_choices.value

    //property var current_tool_properties: pen_tool_properties
    property var current_tool_properties

    onCurrentToolChanged: {

        if(currentTool === "Draw") 
        {
            current_tool_properties = pen_tool_properties
        }
        else if(currentTool === "Brush")
        {
            current_tool_properties = brush_tool_properties
        }
        else if(currentTool === "Laser")
        {
            current_tool_properties = laser_tool_properties
        }
        else if(currentTool === "Square" || currentTool === "Circle"  || currentTool === "Arrow"  || currentTool === "Line")
        {
            current_tool_properties = shapes_tool_properties
        }
        else if(currentTool === "Text")
        {
            current_tool_properties = text_tool_properties
        }
        else if(currentTool === "Erase")
        {
            current_tool_properties = erase_tool_properties
        }
        else if (currentTool === "Colour Picker") 
        {
            current_tool_properties = colour_pick_properties
        }
    }
    
    XsGradientRectangle{
        anchors.fill: parent
    }

    Component {
        id: brush_tool_properties
        XsBrushToolProperties {
            root: drawDialog
        }
    }

    Component {
        id: pen_tool_properties
        XsPenToolProperties {
            root: drawDialog
        }
    }

    Component {
        id: laser_tool_properties
        XsLaserToolProperties {
            root: drawDialog
        }
    }

    Component {
        id: shapes_tool_properties
        XsShapesToolProperties {
            root: drawDialog
        }
    }

    Component {
        id: text_tool_properties
        XsTextToolProperties {
            root: drawDialog
        }
    }

    Component {
        id: erase_tool_properties
        XsEraseToolProperties {
            root: drawDialog
        }
    }

    Component {
        id: colour_pick_properties
        XsColourPick {
            root: drawDialog
        }
    }

    // We wrap all the widgets in a top level Item that can forward keyboard
    // events back to the viewport for consistent
    GridLayout {

        rows: horizontal ? 1 : -1
        columns: horizontal ? -1 : 1

        anchors.fill: parent
        anchors.margins: 2
        columnSpacing: 0
        rowSpacing: 0
    
        XsToolSelector {
            id: toolSelector
            Layout.fillWidth: horizontal ? false : true
            Layout.fillHeight: horizontal ? true : false
            z: 1
        }

        Item {
            Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
            Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight : -1
        }

        Loader { 
            id: loader
            sourceComponent: current_tool_properties
            Layout.fillWidth: horizontal ? false : true
            Layout.fillHeight: horizontal ? true : false
        }

        Item {
            Layout.preferredHeight: horizontal ? -1 : XsStyleSheet.primaryButtonStdHeight
            Layout.preferredWidth: horizontal ? XsStyleSheet.primaryButtonStdHeight : -1
        }

        XsToolActions{ 
            id: toolActionSection
            Layout.fillWidth: horizontal ? false : true
            Layout.fillHeight: horizontal ? true : false
        }
        
        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        XsToolDisplay{ 
            id: toolDispOptions
            Layout.fillWidth: horizontal ? false : true
            Layout.fillHeight: horizontal ? true : false
        }

    }

    ListModel{ id: eraseColorPresetModel
        ListElement{
            preset: "white"
        }
    }

    ListModel{ id: drawColourPresetsModel
        ListElement{
            preset: "#ff0000" //- "red"
        }
        ListElement{
            preset: "#ffa000" //- "orange"
        }
        ListElement{
            preset: "#ffff00" //- "yellow"
        }
        ListElement{
            preset: "#28dc00" //- "green"
        }
        ListElement{
            preset: "#0050ff" //- "blue"
        }
        ListElement{
            preset: "#8c00ff" //- "violet"
        }
        // ListElement{
        //     preset: "#ff64ff" //- "pink"
        // }
        ListElement{
            preset: "#ffffff" //- "white"
        }
        ListElement{
            preset: "#000000" //- "black"
        }
    }
    ListModel{ id: textColourPresetsModel
        ListElement{
            preset: "#ff0000" //- "red"
        }
        ListElement{
            preset: "#ffa000" //- "orange"
        }
        ListElement{
            preset: "#ffff00" //- "yellow"
        }
        ListElement{
            preset: "#28dc00" //- "green"
        }
        ListElement{
            preset: "#0050ff" //- "blue"
        }
        ListElement{
            preset: "#8c00ff" //- "violet"
        }
        ListElement{
            preset: "#ffffff" //- "white"
        }
        ListElement{
            preset: "#000000" //- "black"
        }
    }
    ListModel{ id: shapesColourPresetsModel
        ListElement{
            preset: "#ff0000" //- "red"
        }
        ListElement{
            preset: "#ffa000" //- "orange"
        }
        ListElement{
            preset: "#ffff00" //- "yellow"
        }
        ListElement{
            preset: "#28dc00" //- "green"
        }
        ListElement{
            preset: "#0050ff" //- "blue"
        }
        ListElement{
            preset: "#8c00ff" //- "violet"
        }
        ListElement{
            preset: "#ffffff" //- "white"
        }
        ListElement{
            preset: "#000000" //- "black"
        }
    }

}
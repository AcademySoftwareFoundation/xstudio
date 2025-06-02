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
    anchors.bottomMargin: horizontal ? 0 : XsStyleSheet.primaryButtonStdHeight
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
    property bool isAnyToolSelected: currentTool !== "None" && currentTool != "Colour Picker"

    property bool horizontal: false

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: annotations_model_data
        modelDataName: "annotations_tool_settings"
    }

    /* Here we locate particular nodes in the annotations_model_data giving
    convenient access to backend data. Seems crazy but this is the QML way! */
    XsAttributeValue {
        id: draw_pen_size
        attributeTitle: "Draw Pen Size"
        model: annotations_model_data
    }

    XsAttributeValue {
        id: shapes_pen_size
        attributeTitle: "Shapes Pen Size"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: erase_pen_size
        attributeTitle: "Erase Pen Size"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: text_size
        attributeTitle: "Text Size"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: pen_colour
        attributeTitle: "Pen Colour"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: pen_opacity
        attributeTitle: "Pen Opacity"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: active_tool
        attributeTitle: "Active Tool"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: text_background_colour
        attributeTitle: "Text Background Colour"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: text_background_opacity
        attributeTitle: "Text Background Opacity"
        model: annotations_model_data
    }
    XsAttributeValue {
        id: tool_types_choices
        role: "combo_box_options"
        attributeTitle: "Active Tool"
        model: annotations_model_data
    }

    // Un-comment this when Laser is implemented in combo_box_options

    // make a local binding to the backend attribute
    property alias currentDrawPenSize: draw_pen_size.value
    property alias currentShapePenSize: shapes_pen_size.value
    property alias currentErasePenSize: erase_pen_size.value
    property alias currentTextSize: text_size.value
    property alias currentToolColour: pen_colour.value
    property alias currentOpacity: pen_opacity.value
    property alias currentTool: active_tool.value
    property alias backgroundColor: text_background_colour.value
    property alias backgroundOpacity: text_background_opacity.value
    property alias toolChoices: tool_types_choices.value
    
    property var toolSizeAttrName: "Draw Pen Size"

    onCurrentToolChanged: {
        if(currentTool === "Draw" || currentTool === "Laser")
        {
            currentColorPresetModel = drawColourPresetsModel
            toolSizeAttrName = "Draw Pen Size"
        }
        else if(currentTool === "Erase")
        {
            currentColorPresetModel = eraseColorPresetModel
            toolSizeAttrName = "Erase Pen Size"

        }
        else if(currentTool === "Text")
        {
            currentColorPresetModel = textColourPresetsModel
            toolSizeAttrName = "Text Size"

        }
        else if(currentTool === "Square" || currentTool === "Circle"  || currentTool === "Arrow"  || currentTool === "Line")
        {
            currentColorPresetModel = shapesColourPresetsModel
            toolSizeAttrName = "Shapes Pen Size"

        }
    }


    function setPenSize(penSize) {
        if(currentTool === "Draw" || currentTool === "Laser")
        {
            currentDrawPenSize = penSize
        }
        else if(currentTool === "Erase")
        {
            currentErasePenSize = penSize
        }
        else if(currentTool === "Square" || currentTool === "Circle"  || currentTool === "Arrow"  || currentTool === "Line")
        {
            currentShapePenSize = penSize
        }
        else if(currentTool === "Text")
        {
            currentTextSize = penSize
        }
    }

    // map the local property for currentToolSize to the backend value ... to modify the tool size, we only change the backend
    // value binding

    property ListModel currentColorPresetModel: drawColourPresetsModel
    
    XsGradientRectangle{
        anchors.fill: parent
    }

    Component {
        id: colour_pick_props
        XsColourPick {

        }
    }

    Component {
        id: tool_properties
        XsToolProperties {
            root: drawDialog
        }
    }

    XsColourDialog {
        id: colorDialog
        title: "Please pick a colour"
        property var lastColour
        
        linkColour: currentToolColour

        onCurrentColourChanged: {
            currentToolColour = currentColour
        }
        onAccepted: {
            close()
        }
        onRejected: {
            currentToolColour = lastColour
            close()
        }
        onVisibleChanged: {
            if (visible) {
                currentColour = currentToolColour
                lastColour = currentToolColour
            }
        }
    }

    property alias colorDialog: colorDialog

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
            sourceComponent: currentTool === "Colour Picker" ? colour_pick_props : tool_properties
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
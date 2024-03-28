// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.1
import xstudio.qml.module 1.0

XsWindow {

    id: drawDialog
    title: "Drawing Tools"

    width: minimumWidth
    minimumWidth: 215
    maximumWidth: minimumWidth

    height: minimumHeight
    minimumHeight: toolActionFrame.y+toolActionFrame.height+advancedOptionsBtn.height/2+framePadding
    maximumHeight: minimumHeight

    property int maxDrawSize: 250

    onVisibleChanged: {
        if (!visible) {
            // ensure keyboard events are returned to the viewport
            sessionWidget.playerWidget.viewport.forceActiveFocus()
        }
    }

    property bool isAdvancedOptionsClicked: false
    property real buttonHeight: 20
    property real toolPropLoaderHeight: 0
    property real toolAadvancedFrameHeight: 115
    property real defaultHeight: toolSelectorFrame.height + toolActionFrame.height + framePadding*3


    property real itemSpacing: framePadding/2
    property real framePadding: 6
    property real framePadding_x2: framePadding*2
    property real frameWidth: 1
    property real frameRadius: 2
    property real frameOpacity: 0.3
    property color frameColor: XsStyle.menuBorderColor


    property color hoverTextColor: palette.text //-whitish //XsStyle.hoverBackground
    property color hoverToolInactiveColor: XsStyle.indevColor //-greyish
    property color toolActiveBgColor: palette.highlight //-orangish
    property color toolActiveTextColor: "white" //palette.highlightedText
    property color toolInactiveBgColor: palette.base //-greyish
    property color toolInactiveTextColor: XsStyle.controlTitleColor//-greyish

    property real fontSize: XsStyle.menuFontSize/1.1
    property string fontFamily: XsStyle.menuFontFamily
    property color textButtonColor: toolInactiveTextColor
    property color textValueColor: "white"


    property bool isAnyToolSelected: currentTool !== "None"

    XsModuleAttributes {
        id: anno_tool_backend_settings
        attributesGroupNames: "annotations_tool_settings_0"
    }


    // make a local binding to the backend attribute
    property int currentDrawPenSizeBackendValue: anno_tool_backend_settings.draw_pen_size ? anno_tool_backend_settings.draw_pen_size : 0
    property int currentShapePenSizeBackendValue: anno_tool_backend_settings.shapes_pen_size ? anno_tool_backend_settings.shapes_pen_size : 0
    property int currentErasePenSizeBackendValue: anno_tool_backend_settings.erase_pen_size ? anno_tool_backend_settings.erase_pen_size : 0
    property int currentTextSizeBackendValue: anno_tool_backend_settings.text_size ? anno_tool_backend_settings.text_size : 0
    property color currentToolColourBackendValue: anno_tool_backend_settings.pen_colour ? anno_tool_backend_settings.pen_colour : "#000000"
    property int currentOpacityBackendValue: anno_tool_backend_settings.pen_opacity ? anno_tool_backend_settings.pen_opacity : 0
    property string currentToolBackendValue: anno_tool_backend_settings.active_tool ? anno_tool_backend_settings.active_tool : ""

    property color currentToolColour: currentToolColourBackendValue
    property int currentToolSize: currentTool === "Text" ? currentTextSizeBackendValue : currentTool === "Erase" ? currentErasePenSizeBackendValue : (currentTool === "Shapes" ? currentShapePenSizeBackendValue : currentDrawPenSizeBackendValue)
    property int currentToolOpacity: currentOpacityBackendValue
    property string currentTool: currentToolBackendValue

    function setPenSize(penSize) {
        if(currentTool === "Draw")
        { //Draw
            anno_tool_backend_settings.draw_pen_size = penSize
        }
        else if(currentTool === "Erase")
        { //Erase
            anno_tool_backend_settings.erase_pen_size = penSize
        }
        else if(currentTool === "Shapes")
        { //Shapes
            anno_tool_backend_settings.shapes_pen_size = penSize
        }
        else if(currentTool === "Text")
        { //Shapes
            anno_tool_backend_settings.text_size = penSize
        }
    }

    onCurrentToolChanged: {
        if(currentTool === "Draw")
        { //Draw
            currentColorPresetModel = drawColourPresetsModel
        }
        else if(currentTool === "Erase")
        { //Erase
            currentColorPresetModel = eraseColorPresetModel
        }
        else if(currentTool === "Text")
        { //Text
            currentColorPresetModel = textColourPresetsModel
        }
        else if(currentTool === "Shapes")
        { //Shapes
            currentColorPresetModel = shapesColourPresetsModel
        }

    }

    // make a read only binding to the "annotations_tool_active_0" backend attribute
    property bool annotationToolActive: anno_tool_backend_settings.annotations_tool_active ? anno_tool_backend_settings.annotations_tool_active : false

    // is the mouse over the handles for moving, scaling or deleting text captions?
    property int movingScalingText: anno_tool_backend_settings.moving_scaling_text ? anno_tool_backend_settings.moving_scaling_text : 0

    // Are we in an active drawing mode?
    property bool drawingActive: annotationToolActive && currentTool !== "None"

    // Set the Cursor as required
    property var activeCursor: drawingActive ? movingScalingText > 1 ? Qt.ArrowCursor : currentTool == "Text" ? Qt.IBeamCursor : Qt.CrossCursor : Qt.ArrowCursor

    onActiveCursorChanged: {
        playerWidget.viewport.setRegularCursor(activeCursor)
    }

    // map the local property for currentToolSize to the backend value ... to modify the tool size, we only change the backend
    // value binding

    property ListModel currentColorPresetModel: drawColourPresetsModel

    XsWindowStateSaver
    {
        windowObj: drawDialog
        windowName: "annotations_toolbox"
        override_pref_path: "/plugin/annotations/toolbox_window_settings"
    }

    // Rectangle{ anchors.fill: parent ;color: "yellow"; opacity: 0.3 }

    // We wrap all the widgets in a top level Item that can forward keyboard
    // events back to the viewport for consistent
    Item {
        anchors.fill: parent
        Keys.forwardTo: [sessionWidget]
        focus: true

        Rectangle{ id: toolSelectorFrame
            width: parent.width - framePadding_x2
            x: framePadding
            anchors.top: parent.top
            anchors.topMargin: framePadding
            anchors.bottom: toolProperties.bottom
            anchors.bottomMargin: -framePadding

            color: "transparent"
            border.width: frameWidth
            border.color: frameColor
            opacity: frameOpacity
            radius: frameRadius

        }

        ToolSelector {
            id: toolSelector
            opacity: 1
            anchors.fill: toolSelectorFrame
        }

        Loader { id: toolProperties
            width: toolSelectorFrame.width
            height: toolPropLoaderHeight
            x: toolSelectorFrame.x
            y: buttonHeight*2+framePadding_x2//toolSelectorFrame.toolSelector.y + toolSelectorFrame.toolSelector.height

            sourceComponent:
            Item{

                Rectangle{

                    id: row1
                    y: itemSpacing
                    width: toolProperties.width
                    height: buttonHeight*1.5 + itemSpacing*2
                    color: "transparent";
                    visible: (isAnyToolSelected && currentTool !== "Erase")

                    DrawCategories {
                        id: drawCategories
                        anchors.fill: parent
                        visible: (currentTool === "Draw")
                    }

                    TextCategories {
                        id: textCategories
                        anchors.fill: parent
                        visible: (currentTool === "Text")
                    }

                    ShapeCategories {

                        id: shapeCategories
                        anchors.fill: parent
                        visible: (currentTool === "Shapes")

                    }

                }


                Row{id: row2
                    x: framePadding //+ itemSpacing/2
                    y: row1.y + row1.height
                    z: 1
                    width: toolProperties.width - framePadding*2
                    height: (buttonHeight*3) + (spacing*2)
                    spacing: itemSpacing*2

                    Column {
                        z: 2
                        width: parent.width/2-spacing
                        spacing: itemSpacing

                        XsButton{ id: sizeProp
                            property bool isPressed: false
                            property bool isMouseHovered: sizeMArea.containsMouse
                            property real prevValue: maxDrawSize/2
                            property real newValue: maxDrawSize/2
                            enabled: isAnyToolSelected
                            isActive: isPressed
                            x: spacing/2
                            width: parent.width-x; height: buttonHeight;
                            // color: isPressed || isMouseHovered? (enabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;

                            Text{
                                text: (currentTool=="Shapes")?"Width": "Size"
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
                                width: parent.width/1.8
                                horizontalAlignment: Text.AlignHCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                topPadding: framePadding/1.4
                            }
                            XsTextField{ id: sizeDisplay
                                text: currentToolSize
                                property var backendSize: currentToolSize
                                onBackendSizeChanged: {
                                    text = currentToolSize
                                }
                                focus: sizeMArea.containsMouse && !parent.isPressed
                                onFocusChanged:{
                                    if(focus) {
                                        drawDialog.requestActivate()                                        
                                        selectAll()
                                        forceActiveFocus()
                                    }
                                    else{
                                        deselect()
                                    }
                                }
                                maximumLength: 3
                                inputMask: "900"
                                inputMethodHints: Qt.ImhDigitsOnly 
                                // validator: IntValidator {bottom: 0; top: maxDrawSize;}
                                selectByMouse: false
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: parent.enabled? textValueColor : Qt.darker(textValueColor,1.5)
                                width: parent.width/2.2
                                height: parent.height
                                horizontalAlignment: TextInput.AlignHCenter
                                anchors.right: parent.right
                                topPadding: framePadding/5
                                onEditingCompleted: {
                                    accepted()
                                }
                                onAccepted:{
                                    if(parseInt(text) >= maxDrawSize){
                                        setPenSize(maxDrawSize)
                                    }
                                    else if(parseInt(text) <= 1){
                                        setPenSize(1)
                                    }
                                    else{
                                        setPenSize(parseInt(text))
                                    }

                                    text = "" + backendSize
                                    selectAll()
                                }
                            }
                            MouseArea{
                                id: sizeMArea
                                anchors.fill: parent
                                cursorShape: Qt.SizeHorCursor
                                hoverEnabled: true
                                propagateComposedEvents: true
                                property real prevMX: 0
                                property real deltaMX: 0
                                property real stepSize: 0.25
                                property int valueOnPress: 0
                                onMouseXChanged: {
                                    if(parent.isPressed && parent.enabled)
                                    {
                                        deltaMX = mouseX - prevMX

                                        let deltaValue = parseInt(deltaMX*stepSize)
                                        let valueToApply = Math.round(valueOnPress + deltaValue)

                                        if(deltaMX>0)
                                        {
                                            if(valueToApply >= maxDrawSize){ 
                                                setPenSize(maxDrawSize)
                                                valueOnPress = maxDrawSize
                                                prevMX = mouseX
                                            }
                                            else {
                                                setPenSize(valueToApply)
                                            }
                                        }
                                        else {
                                            if(valueToApply < 1){
                                                setPenSize(1)
                                                valueOnPress = 1
                                                prevMX = mouseX
                                            }
                                            else {
                                                setPenSize(valueToApply)
                                            }
                                        }

                                        sizeDisplay.text = currentToolSize

                                        if(deltaMX!=0){
                                            sizeProp.newValue = currentToolSize
                                        }
                                    }
                                }
                                onPressed: {
                                    prevMX = mouseX
                                    valueOnPress = currentToolSize

                                    parent.isPressed = true
                                    focus = true
                                }
                                onReleased: {
                                    if(prevMX !== mouseX) {
                                        sizeProp.prevValue = valueOnPress
                                        sizeProp.newValue = currentToolSize
                                    }
                                    parent.isPressed = false
                                    focus = false
                                }
                                onDoubleClicked: {
                                    if(currentToolSize == sizeProp.newValue){
                                        setPenSize(sizeProp.prevValue)
                                    }
                                    else{
                                        sizeProp.prevValue = currentToolSize
                                        setPenSize(sizeProp.newValue)
                                    }
                                    sizeDisplay.text = currentToolSize
                                }
                            }
                        }
                        XsButton{ id: opacityProp
                            property bool isPressed: false
                            property bool isMouseHovered: opacityMArea.containsMouse
                            property real prevValue: defaultValue/2
                            property real defaultValue: 100
                            enabled: isAnyToolSelected && currentTool != "Erase"
                            isActive: isPressed
                            x: spacing/2
                            width: parent.width-x; height: buttonHeight;
                            // color: isPressed || isMouseHovered? (enabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;
                            Text{
                                text: "Opacity"
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
                                width: parent.width/1.8
                                horizontalAlignment: Text.AlignHCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                topPadding: framePadding/1.4
                            }
                            XsTextField{ id: opacityDisplay
                                bgColorNormal: parent.enabled?palette.base:"transparent"
                                borderColor: bgColorNormal
                                text: currentTool != "Erase" ? currentToolOpacity : 100
                                property var backendOpacity: currentTool != "Erase" ? currentToolOpacity : 100 // we don't set this anywhere else, so this is read-only - always tracks the backend opacity value
                                onBackendOpacityChanged: {
                                    // if the backend value has changed, update the text
                                    text = currentTool != "Erase" ? currentToolOpacity : 100
                                }
                                focus: opacityMArea.containsMouse && !parent.isPressed
                                onFocusChanged:{
                                    if(focus) {
                                        drawDialog.requestActivate()                                        
                                        selectAll()
                                        forceActiveFocus()
                                    }
                                    else{
                                        deselect()
                                    }
                                }
                                maximumLength: 3
                                // inputMask: "900"
                                inputMethodHints: Qt.ImhDigitsOnly
                                // validator: IntValidator {bottom: 0; top: 100;}
                                selectByMouse: false
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: parent.enabled? textValueColor : Qt.darker(textValueColor,1.5)
                                width: parent.width/2.2
                                height: parent.height
                                horizontalAlignment: TextInput.AlignHCenter
                                anchors.right: parent.right
                                topPadding: framePadding/5
                                onEditingCompleted:{
                                    accepted()
                                }
                                onAccepted:{
                                    if(currentTool != "Erase"){

                                        if(parseInt(text) >= 100) {
                                            anno_tool_backend_settings.pen_opacity = 100
                                        } 
                                        else if(parseInt(text) <= 1) {
                                            anno_tool_backend_settings.pen_opacity = 1
                                        }
                                        else {
                                            anno_tool_backend_settings.pen_opacity = parseInt(text)
                                        }
                                        
                                        text = "" + backendOpacity
                                        selectAll()
                                    }
                                }
                            }
                            MouseArea{
                                id: opacityMArea
                                
                                anchors.fill: parent
                                cursorShape: Qt.SizeHorCursor
                                hoverEnabled: true
                                propagateComposedEvents: true
                                property real prevMX: 0
                                property real deltaMX: 0.0
                                property real stepSize: 0.25
                                property int valueOnPress: 0
                                onMouseXChanged: {
                                    if(parent.isPressed)
                                    {
                                        deltaMX = mouseX - prevMX
                                        // prevMX = mouseX
                                        // var new_opac =  (Math.max(Math.min(100.0, anno_tool_backend_settings.pen_opacity + stepSize), 0.0) + 0.1) - 0.1
                                        // anno_tool_backend_settings.pen_opacity = parseInt(new_opac)

                                        let deltaValue = parseInt(deltaMX*stepSize)
                                        let valueToApply = Math.round(valueOnPress + deltaValue)

                                        if(deltaMX>0)
                                        {
                                            if(valueToApply >= 100) {
                                                anno_tool_backend_settings.pen_opacity=100
                                                valueOnPress = 100
                                                prevMX = mouseX
                                            }
                                            else {
                                                anno_tool_backend_settings.pen_opacity = valueToApply
                                            }
                                        }
                                        else {
                                            if(valueToApply < 1){
                                                anno_tool_backend_settings.pen_opacity=1
                                                valueOnPress = 1
                                                prevMX = mouseX
                                            }
                                            else {
                                                anno_tool_backend_settings.pen_opacity = valueToApply
                                            }
                                        }

                                        opacityDisplay.text = currentTool != "Erase" ? currentToolOpacity : 100
                                    }
                                }
                                onPressed: {
                                    prevMX = mouseX
                                    valueOnPress = anno_tool_backend_settings.pen_opacity

                                    parent.isPressed = true
                                    focus = true
                                }
                                onReleased: {
                                    parent.isPressed = false
                                    focus = false
                                }
                                onDoubleClicked: {
                                    if(anno_tool_backend_settings.pen_opacity == opacityProp.defaultValue){
                                        anno_tool_backend_settings.pen_opacity = opacityProp.prevValue
                                    }
                                    else{
                                        opacityProp.prevValue = anno_tool_backend_settings.pen_opacity
                                        anno_tool_backend_settings.pen_opacity = opacityProp.defaultValue
                                    }
                                    opacityDisplay.text = currentTool != "Erase" ? currentToolOpacity : 100
                                }
                            }
                        }
                        XsButton{ id: colorProp
                            property bool isPressed: false
                            property bool isMouseHovered: colorMArea.containsMouse
                            enabled: (isAnyToolSelected && currentTool !== "Erase")
                            isActive: isPressed
                            x: spacing/2
                            width: parent.width-x; height: buttonHeight;
                            // color: isPressed || isMouseHovered? (enabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;

                            MouseArea{
                                id: colorMArea
                                // enabled: currentTool !== 1
                                hoverEnabled: true
                                anchors.fill: parent
                                onClicked: {
                                        parent.isPressed = false
                                        colorDialog.open()
                                }
                                onPressed: {
                                        parent.isPressed = true
                                }
                                onReleased: {
                                        parent.isPressed = false
                                }
                            }
                            Text{
                                text: "Colour"
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
                                width: parent.width/2
                                horizontalAlignment: Text.AlignHCenter
                                anchors.right: parent.horizontalCenter
                                anchors.rightMargin: -3
                                topPadding: framePadding/1.2
                            }
                            Rectangle{ id: colorPreviewDuplicate
                                opacity: (!isAnyToolSelected || currentTool === "Erase")? (parent.enabled?1:0.5): 0
                                height: parent.height/1.4;
                                color: currentTool === "Erase" ? "white" : currentToolColour
                                border.width: frameWidth
                                border.color: parent.enabled? (currentToolColour=="white" || currentToolColour=="#ffffff")? "black": "white" : Qt.darker("white",1.5)
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.horizontalCenter
                                anchors.leftMargin: parent.width/7
                                anchors.right: parent.right
                                anchors.rightMargin: parent.width/10
                            }
                            Rectangle{ id: colorPreview
                                visible: (isAnyToolSelected && currentTool !== "Erase")
                                x: colorPreviewDuplicate.x
                                y: colorPreviewDuplicate.y
                                width: colorPreviewDuplicate.width
                                onWidthChanged: {
                                    x= colorPreviewDuplicate.x
                                    y= colorPreviewDuplicate.y
                                }
                                height: colorPreviewDuplicate.height
                                color: currentTool === "Erase" ? "white" : currentToolColour;
                                border.width: frameWidth;
                                border.color: (color=="white" || color=="#ffffff")? "black": "white"

                                scale: dragArea.drag.active? 0.6: 1
                                Behavior on scale {NumberAnimation{ duration: 250 }}

                                Drag.active: dragArea.drag.active
                                Drag.hotSpot.x: colorPreview.width/2
                                Drag.hotSpot.y: colorPreview.height/2
                                MouseArea{
                                    id: dragArea
                                    anchors.fill: parent
                                    drag.target: parent

                                    drag.minimumX: -framePadding
                                    drag.maximumX: toolSelectorFrame.width - framePadding*5
                                    drag.minimumY: buttonHeight
                                    drag.maximumY: buttonHeight*2.5

                                    onReleased: {
                                        colorProp.isPressed = false
                                        parent.Drag.drop()
                                        parent.x = colorPreviewDuplicate.x
                                        parent.y = colorPreviewDuplicate.y
                                    }
                                    onClicked: {
                                        colorProp.isPressed = false
                                        colorDialog.open()
                                    }
                                    onPressed: {
                                        colorProp.isPressed = true
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { id: toolPreview
                        width: parent.width/2 - spacing
                        height: currentTool === "Text"? (parent.height/3-spacing) : (parent.height-spacing) 
                        color: "#595959" //"transparent"
                        border.color: frameColor
                        border.width: frameWidth
                        // clip: true

                        Grid {id: checkerBg;
                            property real tileSize: framePadding
                            anchors.fill: parent;
                            anchors.centerIn: parent
                            anchors.margins: tileSize/2;
                            clip: true;
                            rows: Math.floor(height/tileSize);
                            columns: Math.floor(width/tileSize);
                            Repeater {
                                model: checkerBg.columns*checkerBg.rows
                                Rectangle {
                                    property int oddRow: Math.floor(index / checkerBg.columns)%2
                                    property int oddColumn: (index % checkerBg.columns)%2
                                    width: checkerBg.tileSize; height: checkerBg.tileSize
                                    color: (oddRow == 1 ^ oddColumn == 1) ? "#949494": "#595959"
                                }
                            }
                        }

                        Rectangle{

                            id: clippedPreview
                            anchors.fill: parent
                            color: "transparent"
                            clip: true

                            Rectangle {id: drawPreview
                                visible: currentTool === "Draw"
                                anchors.centerIn: parent
                                property real sizeScaleFactor: (parent.height)/maxDrawSize
                                width: currentToolSize *sizeScaleFactor
                                height: width
                                radius: width/2
                                color: currentToolColour
                                opacity: currentToolOpacity/100

                                RadialGradient {
                                    visible: false
                                    anchors.fill: parent
                                    source: parent
                                    gradient:
                                    Gradient {
                                        GradientStop {
                                            position: 0.1; color: currentToolColour
                                        }
                                        GradientStop {
                                            position: 1.0; color: "black"
                                        }
                                    }
                                }

                            }

                            Rectangle { id: erasePreview
                                visible: currentTool === "Erase"
                                anchors.centerIn: parent
                                property real sizeScaleFactor: (parent.height)/maxDrawSize
                                width: currentToolSize * sizeScaleFactor
                                height: width
                                radius: width/2
                                color: "white"
                                opacity: 1
                            }

                            Item{ id: textPreview
                                anchors.centerIn: parent
                                visible: currentTool === "Text"

                                Rectangle{ id: textFillPreview
                                    anchors.fill: textFontPreview
                                    color: textCategories.backgroundColor
                                    opacity: textCategories.backgroundOpacity / 100
                                }
    
                                Text{ id: textFontPreview
                                    text: "Example"
                                    property real sizeScaleFactor: 80/100
                                    font.pixelSize: currentToolSize * sizeScaleFactor
                                    //font.family: textCategories.currentValue
                                    color: currentToolColour
                                    opacity: currentToolOpacity / 100
                                    horizontalAlignment: Text.AlignHCenter
                                    anchors.centerIn: parent
                                }
                            }

                            Image { id: shapePreview
                                visible: currentTool === "Shapes"
                                anchors.centerIn: parent

                                width: parent.height
                                height: width

                                fillMode: Image.PreserveAspectFit
                                antialiasing: true
                                smooth: true

                                property string src: shapeCategories.shapePreviewSVGSource(currentToolSize)
                                onSrcChanged: {
                                    shapePreview.source = ""
                                    shapePreview.source = shapePreview.src
                                }
                                source: src
                                opacity: currentToolOpacity/100
                                layer {
                                    enabled: true
                                    effect:
                                    ColorOverlay {
                                        color: currentToolColour
                                    }
                                }
                            }

                            MouseArea{ id: testPreviewWithKeys //#test-block
                                anchors.fill: parent
                                onWheel: {
                                    if (wheel.modifiers)
                                    {
                                        if (wheel.angleDelta.y > 0 && Qt.ControlModifier)
                                        {
                                            if(currentToolSize < maxDrawSize) setPenSize(currentToolSize+1)
                                        }
                                        else if(wheel.angleDelta.y < 0 && Qt.ControlModifier)
                                        {
                                            if(currentToolSize >1) setPenSize(currentToolSize-1)
                                        }
                                        else if (wheel.angleDelta.x != 0)
                                        {

                                            var delta = wheel.angleDelta.x < 0 ? -2 : 2
                                            var new_opac = anno_tool_backend_settings.pen_opacity + delta
                                            anno_tool_backend_settings.pen_opacity = Math.round(Math.max(Math.min(1, new_opac), 0))
                                        }
                                        wheel.accepted=true
                                    }
                                    else{
                                        wheel.accepted=false
                                    }
                                }
                            }
                        }
                    }
                }


                Rectangle{ id: row3
                    y: row2.y + row2.height + presetColours.spacing
                    width: toolProperties.width
                    height: buttonHeight *1.5
                    visible: (isAnyToolSelected && currentTool !== "Erase")
                    color: "transparent"

                    ListView{ id: presetColours
                        x: frameWidth +spacing*2
                        width: parent.width - frameWidth*2 - spacing*2
                        height: parent.height
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: (itemSpacing!==0)?itemSpacing/2: 0
                        clip: true
                        interactive: false
                        orientation: ListView.Horizontal

                        model: currentColorPresetModel
                        delegate:
                        Item{
                            property bool isMouseHovered: presetMArea.containsMouse
                            width: presetColours.width/9-presetColours.spacing;
                            height: presetColours.height
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width
                                height: width
                                radius: width/2
                                color: preset
                                border.width: 1
                                border.color: parent.isMouseHovered? toolActiveBgColor: (currentToolColour === preset)? toolActiveTextColor: "black"

                                MouseArea{
                                    id: presetMArea
                                    property color temp_color
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {

                                        temp_color = currentColorPresetModel.get(index).preset;
                                        anno_tool_backend_settings.pen_colour = temp_color

                                    }
                                }

                                DropArea {
                                    anchors.fill: parent
                                    Image {
                                        visible: parent.containsDrag
                                        anchors.fill: parent
                                        source: "qrc:///feather_icons/plus-circle.svg"
                                        layer {
                                            enabled: (preset=="black" || preset=="#000000")
                                            effect:
                                            ColorOverlay {
                                                color: "white"
                                            }
                                        }
                                    }
                                    onDropped: {
                                        currentColorPresetModel.setProperty(index, "preset", currentToolColour.toString())
                                    }
                                }
                            }
                        }
                    }
                }

                Component.onCompleted: {
                    toolPropLoaderHeight = row3.y + row3.height
                }
            }


            ColorDialog { id: colorDialog
                title: "Please pick a color"
                color: currentToolColour
                onAccepted: {
                    anno_tool_backend_settings.pen_colour = currentColor
                    close()
                }
                onRejected: {
                    close()
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
                ListElement{
                    preset: "#ff64ff" //- "pink"
                }
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
                    preset: "#ff64ff" //- "pink"
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
                    preset: "#ff64ff" //- "pink"
                }
                ListElement{
                    preset: "#ffffff" //- "white"
                }
                ListElement{
                    preset: "#000000" //- "black"
                }
            }
        }


        // lower z layer than toolActionFrame
        Rectangle{ id: toolAadvancedFrame
            visible: isAdvancedOptionsClicked
            x: framePadding
            anchors.top: toolActionFrame.bottom
            anchors.topMargin: -frameWidth

            width: parent.width - framePadding_x2
            height: toolAadvancedFrameHeight

            color: "transparent"
            opacity: frameOpacity
            border.width: frameWidth
            border.color: frameColor
            radius: frameRadius
        }
        Item{
            visible: isAdvancedOptionsClicked
            anchors.fill: toolAadvancedFrame
            Text{
                text: "Currently Unavailable"
                font.pixelSize: fontSize // /1.3
                font.family: fontFamily
                color: textButtonColor
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                anchors.centerIn: parent
            }
        }



        Rectangle{ id: toolActionFrame
            x: framePadding
            anchors.top: toolSelectorFrame.bottom
            anchors.topMargin: framePadding

            width: parent.width - framePadding_x2
            height: toolSelectorFrame.height/1.5

            color: "transparent"
            opacity: frameOpacity
            border.width: frameWidth
            border.color: frameColor
            radius: frameRadius
        }
        Item{ id: toolActionSection
            x: toolActionFrame.x
            width: toolActionFrame.width

            ListView{ id: toolActionUndoRedo

                width: parent.width - framePadding_x2
                height: buttonHeight
                x: framePadding + spacing/2
                y: toolActionFrame.y + framePadding + spacing/2

                spacing: itemSpacing
                clip: true
                interactive: false
                orientation: ListView.Horizontal

                model:
                ListModel{
                    id: modelUndoRedo
                    ListElement{
                        action: "Undo"
                    }
                    ListElement{
                        action: "Redo"
                    }
                }
                delegate:
                XsButton{
                    text: model.action
                    width: toolActionUndoRedo.width/modelUndoRedo.count - toolActionUndoRedo.spacing
                    height: buttonHeight
                    onClicked: {
                        anno_tool_backend_settings.action_attribute = text
                    }
                }
            }

            ListView{ id: toolActionCopyPasteClear

                width: parent.width - framePadding_x2
                height: buttonHeight
                x: framePadding + spacing/2
                y: toolActionUndoRedo.y + toolActionUndoRedo.height + spacing

                spacing: itemSpacing
                clip: true
                interactive: false
                orientation: ListView.Horizontal

                model:
                ListModel{
                    id: modelCopyPasteClear
                    ListElement{
                        action: "Copy"
                    }
                    ListElement{
                        action: "Paste"
                    }
                    ListElement{
                        action: "Clear"
                    }
                }
                delegate:
                XsButton{
                    text: model.action
                    width: toolActionCopyPasteClear.width/modelCopyPasteClear.count - toolActionCopyPasteClear.spacing
                    height: buttonHeight
                    enabled: text == "Clear"
                    onClicked: {
                        anno_tool_backend_settings.action_attribute = text
                    }

                }
            }

            Rectangle{ id: displayAnnotations
                width: parent.width - framePadding_x2
                height: buttonHeight;
                color: "transparent";
                anchors.top: toolActionCopyPasteClear.bottom
                anchors.topMargin: framePadding_x2
                anchors.horizontalCenter: parent.horizontalCenter

                Text{
                    text: "Display Annotations"
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    color: textButtonColor
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: framePadding/2
                }
            }

            // Sigh - hooking up the draw mode backen attr to the combo box here
            // is horrible! Need something better than this!

            XsModuleAttributes {
                // this lets us get at the combo_box_options for the 'Display Mode' attr
                id: annotations_tool_draw_mode_options
                attributesGroupNames: "annotations_tool_draw_mode_0"
                roleName: "combo_box_options"
            }

            XsModuleAttributes {
                // this lets us get at the value for the 'Display Mode' attr
                id: annotations_tool_draw_mode
                attributesGroupNames: "annotations_tool_draw_mode_0"
            }

            XsComboBox {

                id: dropdownAnnotations

                property var displayModeOptions: annotations_tool_draw_mode_options.display_mode ? annotations_tool_draw_mode_options.display_mode : []
                property var displayModeValue: annotations_tool_draw_mode.display_mode ? annotations_tool_draw_mode.display_mode : ""

                model: displayModeOptions
                width: parent.width/1.3;
                height: buttonHeight
                anchors.top: displayAnnotations.bottom
                anchors.topMargin: itemSpacing
                anchors.horizontalCenter: parent.horizontalCenter
                onCurrentTextChanged: {
                    if (currentText != displayModeValue && annotations_tool_draw_mode.display_mode != undefined) {
                        annotations_tool_draw_mode.display_mode = currentText
                    }
                }
                onDisplayModeValueChanged: {

                    if (displayModeOptions.indexOf(displayModeValue) != -1) {
                        currentIndex = displayModeOptions.indexOf(displayModeValue)
                    }
                }

            }


            XsButton{ id: advancedOptionsBtn
                text: "Advanced Options"
                enabled: false
                width: parent.width/1.3
                height: buttonHeight
                borderRadius: 6
                y: toolActionFrame.y + toolActionFrame.height - height/2
                anchors.horizontalCenter: parent.horizontalCenter
                Image{
                    width: 10
                    height: 10
                    y: parent.height/2 -height/2
                    anchors.left: parent.left
                    anchors.leftMargin: framePadding_x2
                    source: "qrc:///feather_icons/play.svg"
                    rotation: (isAdvancedOptionsClicked)? 90: 0
                    Behavior on rotation {NumberAnimation{ duration: 150 }}
                    layer {
                        enabled: true
                        effect:
                        ColorOverlay {
                            color: parent.isMouseHovered? textValueColor: toolInactiveTextColor
                        }
                    }
                }
                onClicked: {
                    isAdvancedOptionsClicked = !isAdvancedOptionsClicked
                }
            }
        }
    }

}
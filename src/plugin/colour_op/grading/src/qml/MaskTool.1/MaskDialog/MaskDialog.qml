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

Item {

    id: drawDialog

    property int maxDrawSize: 600

    onVisibleChanged: {
        if (!visible) {
            // ensure keyboard events are returned to the viewport
            sessionWidget.playerWidget.viewport.forceActiveFocus()
        }
    }

    property real buttonHeight: 20
    property real toolPropLoaderHeight: 0
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
        id: grading_settings
        attributesGroupNames: "grading_settings"
    }

    XsModuleAttributes {
        id: mask_tool_settings
        attributesGroupNames: "mask_tool_settings"
    }


    // make a local binding to the backend attribute
    property int currentDrawPenSizeBackendValue: mask_tool_settings.draw_pen_size ? mask_tool_settings.draw_pen_size : 0
    property int currentErasePenSizeBackendValue: mask_tool_settings.erase_pen_size ? mask_tool_settings.erase_pen_size : 0
    property color currentToolColourBackendValue: mask_tool_settings.pen_colour ? mask_tool_settings.pen_colour : "#000000"
    property int currentOpacityBackendValue: mask_tool_settings.pen_opacity ? mask_tool_settings.pen_opacity : 0
    property int currentSoftnessBackendValue: mask_tool_settings.pen_softness ? mask_tool_settings.pen_softness : 0
    property string currentToolBackendValue: mask_tool_settings.drawing_tool ? mask_tool_settings.drawing_tool : ""

    property color currentToolColour: currentToolColourBackendValue
    property int currentToolSize: currentTool === "Erase" ? currentErasePenSizeBackendValue : currentDrawPenSizeBackendValue
    property int currentToolOpacity: currentOpacityBackendValue
    property int currentToolSoftness: currentSoftnessBackendValue
    property string currentTool: currentToolBackendValue

    function setPenSize(penSize) {
        if(currentTool === "Draw")
        { //Draw
            mask_tool_settings.draw_pen_size = penSize
        }
        else if(currentTool === "Erase")
        { //Erase
            mask_tool_settings.erase_pen_size = penSize
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
    }

    // make a read only binding to the "mask_tool_active" backend attribute
    property bool maskToolActive: mask_tool_settings.mask_tool_active ? mask_tool_settings.mask_tool_active : false

    // Are we in an active drawing mode?
    property bool drawingActive: maskToolActive && currentTool !== "None"

    // Set the Cursor as required
    property var activeCursor: drawingActive ? Qt.CrossCursor : Qt.ArrowCursor

    onActiveCursorChanged: {
        playerWidget.viewport.setRegularCursor(activeCursor)
    }

    // map the local property for currentToolSize to the backend value ... to modify the tool size, we only change the backend
    // value binding

    property ListModel currentColorPresetModel: drawColourPresetsModel

    // We wrap all the widgets in a top level Item that can forward keyboard
    // events back to the viewport for consistent
    Item {
        anchors.fill: parent
        Keys.forwardTo: [sessionWidget]
        focus: true

        Rectangle{
            id: toolSelectorFrame
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

        Loader {
            id: toolProperties
            width: toolSelectorFrame.width
            height: toolPropLoaderHeight
            x: toolSelectorFrame.x
            y: buttonHeight*2+framePadding_x2//toolSelectorFrame.toolSelector.y + toolSelectorFrame.toolSelector.height

            sourceComponent:
            Item{

                Row{id: row1
                    x: framePadding //+ itemSpacing/2
                    y: itemSpacing*5 //row1.y + row1.height
                    z: 1
                    width: toolProperties.width - framePadding*2
                    height: (buttonHeight*4) + (spacing*2)
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
                                            mask_tool_settings.pen_opacity = 100
                                        } 
                                        else if(parseInt(text) <= 1) {
                                            mask_tool_settings.pen_opacity = 1
                                        }
                                        else {
                                            mask_tool_settings.pen_opacity = parseInt(text)
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
                                        // var new_opac =  (Math.max(Math.min(100.0, mask_tool_settings.pen_opacity + stepSize), 0.0) + 0.1) - 0.1
                                        // mask_tool_settings.pen_opacity = parseInt(new_opac)

                                        let deltaValue = parseInt(deltaMX*stepSize)
                                        let valueToApply = Math.round(valueOnPress + deltaValue)

                                        if(deltaMX>0)
                                        {
                                            if(valueToApply >= 100) {
                                                mask_tool_settings.pen_opacity=100
                                                valueOnPress = 100
                                                prevMX = mouseX
                                            }
                                            else {
                                                mask_tool_settings.pen_opacity = valueToApply
                                            }
                                        }
                                        else {
                                            if(valueToApply < 1){
                                                mask_tool_settings.pen_opacity=1
                                                valueOnPress = 1
                                                prevMX = mouseX
                                            }
                                            else {
                                                mask_tool_settings.pen_opacity = valueToApply
                                            }
                                        }

                                        opacityDisplay.text = currentTool != "Erase" ? currentToolOpacity : 100
                                    }
                                }
                                onPressed: {
                                    prevMX = mouseX
                                    valueOnPress = mask_tool_settings.pen_opacity

                                    parent.isPressed = true
                                    focus = true
                                }
                                onReleased: {
                                    parent.isPressed = false
                                    focus = false
                                }
                                onDoubleClicked: {
                                    if(mask_tool_settings.pen_opacity == opacityProp.defaultValue){
                                        mask_tool_settings.pen_opacity = opacityProp.prevValue
                                    }
                                    else{
                                        opacityProp.prevValue = mask_tool_settings.pen_opacity
                                        mask_tool_settings.pen_opacity = opacityProp.defaultValue
                                    }
                                    opacityDisplay.text = currentTool != "Erase" ? currentToolOpacity : 100
                                }
                            }
                        }
                        XsButton{ id: softnessProp
                            property bool isPressed: false
                            property bool isMouseHovered: softnessMArea.containsMouse
                            property real prevValue: defaultValue/2
                            property real defaultValue: 100
                            enabled: isAnyToolSelected && currentTool != "Erase"
                            isActive: isPressed
                            x: spacing/2
                            width: parent.width-x; height: buttonHeight;
                            // color: isPressed || isMouseHovered? (enabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;
                            Text{
                                text: "Softness"
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
                                width: parent.width/1.8
                                horizontalAlignment: Text.AlignHCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                topPadding: framePadding/1.4
                            }
                            XsTextField{ id: softnessDisplay
                                bgColorNormal: parent.enabled?palette.base:"transparent"
                                borderColor: bgColorNormal
                                text: currentTool != "Erase" ? currentToolSoftness : 100
                                property var backendSoftness: currentTool != "Erase" ? currentToolSoftness : 100 // we don't set this anywhere else, so this is read-only - always tracks the backend opacity value
                                onBackendSoftnessChanged: {
                                    // if the backend value has changed, update the text
                                    text = currentTool != "Erase" ? currentToolSoftness : 100
                                }
                                focus: softnessMArea.containsMouse && !parent.isPressed
                                onFocusChanged:{
                                    if(focus) {
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
                                            mask_tool_settings.pen_softness = 100
                                        } 
                                        else if(parseInt(text) <= 0) {
                                            mask_tool_settings.pen_softness = 0
                                        }
                                        else {
                                            mask_tool_settings.pen_softness = parseInt(text)
                                        }
                                        
                                        text = "" + backendSoftness
                                        selectAll()
                                    }
                                }
                            }
                            MouseArea{
                                id: softnessMArea
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
                                        // var new_opac =  (Math.max(Math.min(100.0, mask_tool_settings.pen_softness + stepSize), 0.0) + 0.1) - 0.1
                                        // mask_tool_settings.pen_softness = parseInt(new_opac)

                                        let deltaValue = parseInt(deltaMX*stepSize)
                                        let valueToApply = Math.round(valueOnPress + deltaValue)

                                        if(deltaMX>0)
                                        {
                                            if(valueToApply >= 100) {
                                                mask_tool_settings.pen_softness=100
                                                valueOnPress = 100
                                                prevMX = mouseX
                                            }
                                            else {
                                                mask_tool_settings.pen_softness = valueToApply
                                            }
                                        }
                                        else {
                                            if(valueToApply < 0){
                                                mask_tool_settings.pen_softness=0
                                                valueOnPress = 0
                                                prevMX = mouseX
                                            }
                                            else {
                                                mask_tool_settings.pen_softness = valueToApply
                                            }
                                        }

                                        softnessDisplay.text = currentTool != "Erase" ? currentToolSoftness : 100
                                    }
                                }
                                onPressed: {
                                    prevMX = mouseX
                                    valueOnPress = mask_tool_settings.pen_softness

                                    parent.isPressed = true
                                    focus = true
                                }
                                onReleased: {
                                    parent.isPressed = false
                                    focus = false
                                }
                                onDoubleClicked: {
                                    if(mask_tool_settings.pen_softness == softnessProp.defaultValue){
                                        mask_tool_settings.pen_softness = softnessProp.prevValue
                                    }
                                    else{
                                        softnessProp.prevValue = mask_tool_settings.pen_softness
                                        mask_tool_settings.pen_softness = softnessProp.defaultValue
                                    }
                                    softnessDisplay.text = currentTool != "Erase" ? currentToolSoftness : 0
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
                        height: parent.height - spacing
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

                        }
                    }
                }


                Rectangle{ id: row2
                    y: row1.y + row1.height + presetColours.spacing
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
                                        mask_tool_settings.pen_colour = temp_color

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
                    toolPropLoaderHeight = row2.y + row2.height
                }
            }


            ColorDialog { id: colorDialog
                title: "Please pick a color"
                color: currentToolColour
                onAccepted: {
                    mask_tool_settings.pen_colour = currentColor
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
                        grading_settings.drawing_action = text
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
                        grading_settings.drawing_action = text
                    }

                }
            }

            ListView{ id: toolActionDisplayMode

                width: parent.width - framePadding_x2
                height: buttonHeight
                x: framePadding + spacing/2
                y: toolActionCopyPasteClear.y + toolActionCopyPasteClear.height + spacing

                spacing: itemSpacing
                clip: true
                interactive: false
                orientation: ListView.Horizontal

                model:
                ListModel{
                    id: modelDisplayMode
                    ListElement{
                        action: "Mask"
                        tooltip: "Show mask being draw"
                    }
                    ListElement{
                        action: "Grade"
                        tooltip: "Show masked grade result"
                    }
                }
                delegate:
                XsButton{
                    isActive: mask_tool_settings.display_mode == text
                    text: model.action
                    tooltip: model.tooltip
                    width: toolActionDisplayMode.width/modelDisplayMode.count - toolActionDisplayMode.spacing
                    height: buttonHeight
                    onClicked: {
                        mask_tool_settings.display_mode = text
                    }
                }
            }

        }
    }

}
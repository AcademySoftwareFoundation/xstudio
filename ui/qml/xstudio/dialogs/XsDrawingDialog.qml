// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.0
import xstudio.qml.bookmarks 1.0


XsWindow {
    id: drawDialog
    title: "Drawing Tools"

    centerOnOpen: true

    width: minimumWidth
    minimumWidth: 215
    maximumWidth: (isTestMode)?app_window.width: minimumWidth

    height: minimumHeight
    minimumHeight: (isTestMode)?defaultHeight: isAdvancedOptionsClicked? (defaultHeight+toolAadvancedFrameHeight): (defaultHeight+framePadding_x2)
    maximumHeight: (isTestMode)?app_window.height: isAdvancedOptionsClicked? (defaultHeight+toolAadvancedFrameHeight): (defaultHeight+framePadding_x2)

    // Component.onCompleted: {
    //     // x: app_window.x + (width / 2)
    //     // y: app_window.y + (height / 2)
    //     console.debug("DrawingTools >> (x, y): "+x+", "+y)
    // }

    property bool isDevMode: true
    property bool isTestMode: false
    Item{
        z: 10
        visible: isDevMode
        anchors.fill: parent
        focus: true
        Keys.onPressed: {
            if ((event.key == Qt.Key_T) && (event.modifiers & Qt.ControlModifier))
            isTestMode = !isTestMode
        }
        Text{
            visible: isTestMode
            text: "TEST MODE"
            color: "red"
            opacity: 0.5
            font.pixelSize: fontSize
            font.family: fontFamily
            anchors.horizontalCenter: parent.horizontalCenter
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


    property bool isAnyToolSelected: currentTool !== -1
    property int currentTool: 0
    onCurrentToolChanged: {
        if(currentTool == -1)
        { //Tools disabled
            currentToolSize = 0
            currentToolOpacity = 0.0
            currentToolColor = "transparent"
        }
        else
        {
            var currentToolProperties = toolSelectorModel.get(currentTool)
            currentToolSize = currentToolProperties.toolSize
            currentToolOpacity = currentToolProperties.toolOpacity
            currentToolColor = currentToolProperties.toolColor

            if(currentTool === 0)
            { //Draw
                currentColorPresetModel = drawColourPresetsModel
            }
            else if(currentTool === 1)
            { //Erase
                currentColorPresetModel = eraseColorPresetModel
            }
            else if(currentTool === 2)
            { //Text
                currentColorPresetModel = textColourPresetsModel
            }
            else if(currentTool === 3)
            { //Shapes
                currentColorPresetModel = shapesColourPresetsModel
            }
        }
    }
    property real currentToolSize: toolSelectorModel.get(0).toolSize
    property real currentToolOpacity: toolSelectorModel.get(0).toolOpacity
    property string currentToolColor: toolSelectorModel.get(0).toolColor
    property ListModel currentColorPresetModel: drawColourPresetsModel



    // Rectangle{ anchors.fill: parent ;color: "yellow"; opacity: 0.3 }


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
    Item{
        ListView{ id: toolSelector

            width: toolSelectorFrame.width - framePadding_x2
            height: buttonHeight*2
            x: toolSelectorFrame.x + framePadding + spacing/2
            y: toolSelectorFrame.y + framePadding + spacing/2

            spacing: itemSpacing
            clip: true
            orientation: ListView.Horizontal
            interactive: isTestMode

            model: toolSelectorModel
            delegate: toolSelectorDelegate

            onCurrentIndexChanged:
            if(currentIndex == 0 || currentIndex == 1 || currentIndex == 2 || currentIndex == 3)
            {
                currentTool = currentIndex;
            }
            else
            {
                currentTool = -1; //Disables tool
            }
        }
        ListModel{ id: toolSelectorModel
            ListElement{
                toolName: "Draw"
                toolImage: "qrc:///feather_icons/edit-2.svg"
                toolSize: 80
                toolOpacity: 1.0
                toolColor: "red"
                toolColorPresets: "red, blue, yellow"
            }
            ListElement{
                toolName: "Erase"
                toolImage: "qrc:///feather_icons/book.svg"
                toolSize: 70
                toolOpacity: 1.0
                toolColor: "white"
                toolColorPresets: "white"
            }
            ListElement{
                toolName: "Text"
                toolImage: "qrc:///feather_icons/type.svg"
                toolSize: 21
                toolOpacity: 1.0
                toolColor: "yellow"
                toolColorPresets: "yellow"
            }
            ListElement{
                toolName: "Shapes"
                toolImage: "qrc:///feather_icons/star.svg"
                toolSize: 50
                toolOpacity: 1.0
                toolColor: "blue"
                toolColorPresets: "light green"
            }
        }
        Component{ id: toolSelectorDelegate
            Rectangle{
                property bool isMouseHovered: toolSelectorMArea.containsMouse
                width: toolSelector.width/toolSelectorModel.count -toolSelector.spacing
                height: toolSelector.height
                color: "transparent"

                Rectangle{
                    width: parent.width
                    height: parent.height
                    color: (toolSelector.currentIndex===index || isMouseHovered)?toolActiveBgColor: toolInactiveBgColor
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    Text{
                        id: tText
                        text: toolName

                        font.pixelSize: fontSize
                        font.family: fontFamily
                        color: (toolSelector.currentIndex===index || isMouseHovered)? toolActiveTextColor: toolInactiveTextColor
                        horizontalAlignment: Text.AlignHCenter

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: framePadding/2
                    }
                    Image {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: framePadding/2
                        width: 20
                        height: width
                        source: toolImage
                        anchors.horizontalCenter: parent.horizontalCenter
                        layer {
                            enabled: true
                            effect:
                            ColorOverlay {
                                color: (toolSelector.currentIndex===index || isMouseHovered)? toolActiveTextColor: toolInactiveTextColor
                            }
                        }
                    }
                    MouseArea{
                        id: toolSelectorMArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if(toolSelector.currentIndex == index)
                            { //Disables tool
                                toolSelector.currentIndex = -1
                            }
                            else
                            {
                                toolSelector.currentIndex = index
                            }
                        }
                    }
                }
            }
        }
    }


    Loader { id: toolProperties
        width: toolSelectorFrame.width
        height: toolPropLoaderHeight
        x: toolSelectorFrame.x
        y: toolSelector.y + toolSelector.height

        sourceComponent:
        Item{

            Rectangle{ id: row1
                y: itemSpacing
                width: toolProperties.width
                height: buttonHeight*1.5 + itemSpacing*2
                color: "transparent";
                visible: (currentTool !== -1 && currentTool !== 1)

                Item{ id: drawCategories
                    anchors.fill: parent
                    visible: (currentTool === 0)

                    Rectangle{ id: divLine
                        anchors.centerIn: parent
                        width: parent.width - height*2
                        height: frameWidth
                        color: frameColor
                        opacity: frameOpacity
                    }
                    ListView{ id: drawList
                        x: spacing/2
                        width: parent.width/1.3
                        height: buttonHeight/1.3
                        anchors.centerIn: parent
                        spacing: (itemSpacing!==0)?itemSpacing/2: 0
                        clip: true
                        orientation: ListView.Horizontal
                        interactive: isTestMode

                        model:
                        ListModel{
                            ListElement{
                                action: "Sketch"
                            }
                            ListElement{
                                action: "Laser"
                            }
                            ListElement{
                                action: "Onion"
                            }
                        }
                        delegate:
                        Rectangle{
                            property bool isMouseHovered: drawListMArea.containsMouse
                            property bool isEnabled: index==0

                            width: drawList.width/3-drawList.spacing
                            height: drawList.height
                            radius: (height/1.2)
                            color: isMouseHovered?
                            (isEnabled? toolActiveBgColor: hoverToolInactiveColor):
                            (drawList.currentIndex===index)? toolActiveBgColor: toolInactiveBgColor

                            Text{
                                text: action
                                font.pixelSize: fontSize/1.2
                                font.family: fontFamily
                                color: (drawList.currentIndex===index || parent.isMouseHovered)? toolActiveTextColor: toolInactiveTextColor
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            MouseArea{
                                id: drawListMArea
                                // visible: (index==0)
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if(index==0) drawList.currentIndex = index
                                }
                            }
                        }
                    }
                }
                Item{ id: textCategories
                    anchors.fill: parent
                    visible: (currentTool === 2)

                    ComboBox { id: dropdownFonts
                        property bool isMouseHovered: dropdownFontsMArea.containsMouse
                        width: parent.width-framePadding_x2 -itemSpacing*2
                        height: buttonHeight;
                        anchors.centerIn: parent
                        model: [ "Arial", "Arial Black", "Courier New", "Helvetica", "Overpass", "Times New Roman", "Verdana" ]
                        //Qt.fontFamilies()

                        background:
                        Rectangle{
                            color: (dropdownFonts.isMouseHovered)? toolActiveBgColor: toolInactiveBgColor;
                        }

                        contentItem:
                        Rectangle{
                            color: "transparent";
                            width: dropdownFonts.width
                            height: dropdownFonts.height
                            Text{
                                text: dropdownFonts.displayText
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                color: textValueColor
                                width: dropdownFonts.width
                                horizontalAlignment: Text.AlignHCenter
                                height: dropdownFonts.height
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        popup:
                        Popup{
                            id: popupFontOptions
                            width: dropdownFonts.width
                            height: contentItem.implicitHeight
                            padding: 0

                            contentItem:
                            ListView {
                                implicitHeight: contentHeight
                                model: dropdownFonts.popup.visible ? dropdownFonts.delegateModel: null
                                ScrollBar.vertical: ScrollBar { }
                            }
                            background:
                            Rectangle{
                                radius: frameRadius*3
                                color: toolInactiveBgColor
                                border.color: frameColor
                            }
                        }

                        delegate:
                        ItemDelegate {
                            height: dropdownFonts.height
                            width: dropdownFonts.width
                            padding: 0

                            leftPadding: width/4
                            text: dropdownFonts.textRole ? (Array.isArray(dropdownFonts.model) ? modelData[dropdownFonts.textRole]: model[dropdownFonts.textRole]): modelData
                            palette.text: dropdownFonts.palette.text
                            palette.highlightedText: dropdownFonts.palette.highlightedText
                            font.weight: dropdownFonts.currentIndex === index ? Font.DemiBold: Font.Normal
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            highlighted: dropdownFonts.highlightedIndex === index

                            // contentItem:
                            // Rectangle{
                            //     width: dropdownFonts.width
                            //     height: dropdownFonts.implicitHeight
                            //     color: toolInactiveBgColor
                            //     Text {
                            //         text: modelData;
                            //         font.pixelSize: fontSize
                            //         font.family: fontFamily
                            //         font.weight: dropdownFonts.currentIndex === index ? Font.DemiBold: Font.Normal
                            //         color: textValueColor
                            //         width: dropdownFonts.width
                            //         horizontalAlignment: Text.AlignJustify
                            //         // leftPadding: width/6
                            //         height: dropdownFonts.height
                            //         verticalAlignment: Text.AlignVCenter
                            //     }
                            // }
                        }

                        MouseArea{
                            id: dropdownFontsMArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                popupFontOptions.open()
                            }
                        }
                    }
                }
                Item{ id: shapeCategories
                    anchors.fill: parent
                    visible: (currentTool === 3)

                    Rectangle{
                        anchors.centerIn: parent
                        width: parent.width - height*2
                        height: frameWidth
                        color: frameColor
                        opacity: frameOpacity
                    }
                    ListView{ id: shapesList

                        width: parent.width-framePadding_x2
                        height: buttonHeight
                        x: framePadding + spacing/2
                        // y: -parent.height/3
                        anchors.verticalCenter: parent.verticalCenter

                        spacing: itemSpacing
                        clip: true
                        orientation: ListView.Horizontal
                        interactive: isTestMode

                        model: shapesModel

                        delegate:
                        Rectangle{
                            property bool isMouseHovered: shapeListMArea.containsMouse
                            width: shapesList.width/shapesModel.count-shapesList.spacing;
                            height: shapesList.height
                            color: (shapesList.currentIndex===index || isMouseHovered)?toolActiveBgColor: toolInactiveBgColor
                            // radius: (height/1.2)

                            Image {
                                anchors.centerIn: parent
                                width: height
                                height: parent.height-(shapesList.spacing*2)
                                source: shapeImage
                                layer {
                                    enabled: true
                                    effect:
                                    ColorOverlay {
                                        color: (shapesList.currentIndex===index || isMouseHovered)? toolActiveTextColor: toolInactiveTextColor
                                    }
                                }
                            }
                            MouseArea{
                                id: shapeListMArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    shapesList.currentIndex = index
                                }
                            }
                        }
                    }
                    ListModel{ id: shapesModel
                        ListElement{
                            shapeImage: "qrc:///feather_icons/circle.svg"
                            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
                            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\" ><circle cx=\"24\" cy=\"24\" r=\"20\"></circle></svg>"
                        }
                        ListElement{
                            shapeImage: "qrc:///feather_icons/square.svg"
                            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
                            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\" ><rect x=\"6\" y=\"6\" width=\"36\" height=\"36\" rx=\"4\" ry=\"4\"></rect></svg>"
                        }
                        ListElement{
                            shapeImage: "qrc:///feather_icons/arrow-right.svg"
                            shapeSVG1: "data: image/svg+xml;utf8, <svg viewBox=\"0 0 48 48\" width=\"100%\" height=\"100%\" stroke=\"white\" stroke-width=\""
                            shapeSVG2: "\" fill=\"none\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><line x1=\"10\" y1=\"24\" x2=\"38\" y2=\"24\"></line><polyline points=\"24 10 38 24 24 38\"></polyline></svg>"
                        }
                    }
                }
            }


            Row{id: row2
                x: framePadding
                y: row1.y + row1.height
                z: 1
                width: toolProperties.width - framePadding*2
                height: (buttonHeight*3) + (spacing*2)
                spacing: itemSpacing*2

                Column {
                    z: 2
                    width: parent.width/2-spacing
                    spacing: itemSpacing

                    Rectangle{ id: sizeProp
                        property bool isPressed: false
                        property bool isMouseHovered: sizeMArea.containsMouse
                        property bool isEnabled: isAnyToolSelected
                        x: spacing/2
                        width: parent.width-x; height: buttonHeight;
                        color: isPressed || isMouseHovered? (isEnabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;

                        Text{
                            text: (currentTool==3)?"Width": "Size"
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
                            width: parent.width/2
                            horizontalAlignment: Text.AlignHCenter
                            anchors.right: parent.horizontalCenter
                            // anchors.verticalCenter: parent.verticalCenter
                            topPadding: framePadding/1.2
                            // Rectangle{ anchors.fill: parent ;color: "green"; opacity: 0.3 }
                            // Rectangle{ anchors.top: parent.top; width: parent.width; height: parent.topPadding; color: "yellow"; opacity: 0.3 }
                        }
                        Text{
                            text: currentToolSize
                            font.pixelSize: fontSize + 2
                            font.family: fontFamily
                            color: textValueColor
                            width: parent.width/2
                            horizontalAlignment: Text.AlignHCenter
                            anchors.left: parent.horizontalCenter
                            topPadding: framePadding/1.2
                        }
                        MouseArea{
                            id: sizeMArea
                            anchors.fill: parent
                            cursorShape: (parent.isEnabled)? Qt.SizeHorCursor: Qt.ArrowCursor
                            hoverEnabled: true
                            property real prevMX: 0
                            property real deltaMX: 0
                            property real stepSize: 1
                            onMouseXChanged: {
                                if(parent.isPressed && parent.isEnabled)
                                {
                                    deltaMX = mouseX - prevMX
                                    prevMX = mouseX

                                    if(deltaMX>0)
                                    {
                                        if(currentToolSize+deltaMX > 100) currentToolSize=100
                                        else {
                                            currentToolSize+=deltaMX
                                        }
                                    }
                                    else {
                                        if(currentToolSize-(-deltaMX) < 1) currentToolSize=1
                                        else {
                                            currentToolSize+=deltaMX //currentToolSize-=(-deltaMX)
                                        }
                                    }
                                    toolSelectorModel.setProperty(currentTool, "toolSize", currentToolSize)
                                }
                            }
                            onPressed: {
                                prevMX = mouseX
                                if(parent.isEnabled) parent.isPressed = true
                            }
                            onReleased: {
                                if(parent.isEnabled) parent.isPressed = false
                            }
                        }
                    }
                    Rectangle{ id: opacityProp
                        property bool isPressed: false
                        property bool isMouseHovered: opacityMArea.containsMouse
                        property bool isEnabled: isAnyToolSelected
                        x: spacing/2
                        width: parent.width-x; height: buttonHeight;
                        color: isPressed || isMouseHovered? (isEnabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;
                        Text{
                            text: "Opacity"
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
                            width: parent.width/2
                            horizontalAlignment: Text.AlignHCenter
                            anchors.right: parent.horizontalCenter
                            topPadding: framePadding/1.2
                        }
                        Text{
                            text: currentToolOpacity.toFixed(2)
                            font.pixelSize: fontSize + 2
                            font.family: fontFamily
                            color: textValueColor
                            width: parent.width/2
                            horizontalAlignment: Text.AlignHCenter
                            anchors.left: parent.horizontalCenter
                            topPadding: framePadding/1.2
                        }
                        MouseArea{
                            id: opacityMArea
                            anchors.fill: parent
                            cursorShape: (parent.isEnabled)? Qt.SizeHorCursor: Qt.ArrowCursor
                            hoverEnabled: true
                            property real prevMX: 0
                            property real deltaMX: 0
                            property real stepSize: 0.02
                            onMouseXChanged: {
                                if(parent.isPressed && parent.isEnabled)
                                {
                                    deltaMX = mouseX - prevMX
                                    prevMX = mouseX
                                    if(deltaMX>0)
                                    {
                                        if(currentToolOpacity+(deltaMX*stepSize) > 1) currentToolOpacity=1
                                        else {
                                            currentToolOpacity+=(deltaMX*stepSize)
                                        }

                                    }
                                    else {
                                        if(currentToolOpacity-(-deltaMX*stepSize) < 0.02) currentToolOpacity=0.02
                                        else{
                                            currentToolOpacity-=(-deltaMX*stepSize)
                                        }
                                    }
                                    toolSelectorModel.setProperty(currentTool, "toolOpacity", currentToolOpacity)
                                }
                            }
                            onPressed: {
                                prevMX = mouseX
                                if(parent.isEnabled) parent.isPressed = true
                            }
                            onReleased: {
                                if(parent.isEnabled) parent.isPressed = false
                            }
                        }
                    }
                    Rectangle{ id: colorProp
                        property bool isPressed: false
                        property bool isMouseHovered: colorMArea.containsMouse
                        property bool isEnabled: (currentTool !== -1 && currentTool !== 1)
                        x: spacing/2
                        width: parent.width-x; height: buttonHeight;
                        color: isPressed || isMouseHovered? (isEnabled? toolActiveBgColor: hoverToolInactiveColor): toolInactiveBgColor;

                        MouseArea{
                            id: colorMArea
                            // enabled: currentTool !== 1
                            hoverEnabled: true
                            anchors.fill: parent
                            onClicked: {
                                if(parent.isEnabled)
                                {
                                    parent.isPressed = false
                                    colorDialog.open()
                                }
                            }
                            onPressed: {
                                if(colorProp.isEnabled)
                                {
                                    parent.isPressed = true
                                }
                            }
                            onReleased: {
                                if(colorProp.isEnabled)
                                {
                                    parent.isPressed = false
                                }
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
                            topPadding: framePadding/1.2
                        }
                        Rectangle{ id: colorPreviewDuplicate
                            opacity: (currentTool === -1 || currentTool === 1)? 1: 0
                            height: parent.height/1.4;
                            color: currentToolColor
                            border.width: frameWidth
                            border.color: (currentToolColor=="white" || currentToolColor=="#ffffff")? "black": "white"
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.horizontalCenter
                            anchors.leftMargin: parent.width/8
                            anchors.right: parent.right
                            anchors.rightMargin: parent.width/8
                        }
                        Rectangle{ id: colorPreview
                            visible: (currentTool !== -1 && currentTool !== 1)
                            x: colorPreviewDuplicate.x
                            y: colorPreviewDuplicate.y
                            width: colorPreviewDuplicate.width
                            onWidthChanged: {
                                x= colorPreviewDuplicate.x
                                y= colorPreviewDuplicate.y
                            }
                            height: colorPreviewDuplicate.height
                            color: currentToolColor;
                            border.width: frameWidth;
                            border.color: (currentToolColor=="white" || currentToolColor=="#ffffff")? "black": "white"

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
                            visible: currentTool === 0
                            anchors.centerIn: parent
                            property real sizeScaleFactor: (parent.height/1.3)/100
                            width: currentToolSize *sizeScaleFactor
                            height: width
                            radius: width/2
                            color: currentToolColor
                            opacity: currentToolOpacity

                            RadialGradient {
                                visible: false
                                anchors.fill: parent
                                source: parent
                                gradient:
                                Gradient {
                                    GradientStop {
                                        position: 0.1; color: currentToolColor
                                    }
                                    GradientStop {
                                        position: 1.0; color: "black"
                                    }
                                }
                            }

                        }

                        Rectangle { id: erasePreview
                            visible: currentTool === 1
                            anchors.centerIn: parent
                            property real sizeScaleFactor: (parent.height/1.3)/100
                            width: currentToolSize * sizeScaleFactor
                            height: width
                            radius: width/2
                            color: currentToolColor
                            opacity: currentToolOpacity
                        }

                        Text{ id: textPreview
                            text: "Example"
                            visible: currentTool === 2
                            property real sizeScaleFactor: 80/100
                            font.pixelSize: currentToolSize *sizeScaleFactor
                            font.family: dropdownFonts.currentValue
                            color: currentToolColor
                            opacity: currentToolOpacity
                            horizontalAlignment: Text.AlignHCenter
                            anchors.centerIn: parent
                        }

                        Image { id: shapePreview
                            visible: currentTool === 3
                            anchors.centerIn: parent

                            width: parent.height
                            height: width

                            fillMode: Image.PreserveAspectFit
                            antialiasing: true
                            smooth: true

                            property string src: shapesModel.get(shapesList.currentIndex).shapeSVG1 + (currentToolSize*7/100) + shapesModel.get(shapesList.currentIndex).shapeSVG2
                            onSrcChanged: {
                                shapePreview.source = ""
                                shapePreview.source = shapePreview.src
                            }
                            source: src
                            opacity: currentToolOpacity
                            layer {
                                enabled: true
                                effect:
                                ColorOverlay {
                                    color: currentToolColor
                                }
                            }
                        }

                        MouseArea{ id: testPreviewWithKeys //#test-block
                            anchors.fill: parent
                            visible: isTestMode
                            onWheel: {
                                if (wheel.modifiers)
                                {
                                    if (wheel.angleDelta.y > 0 && Qt.ControlModifier)
                                    {
                                        if(currentToolSize < 100) currentToolSize++
                                        toolSelectorModel.setProperty(currentTool, "toolSize", currentToolSize)
                                    }
                                    else if(wheel.angleDelta.y < 0 && Qt.ControlModifier)
                                    {
                                        if(currentToolSize >1) currentToolSize--
                                        toolSelectorModel.setProperty(currentTool, "toolSize", currentToolSize)
                                    }
                                    else if (wheel.angleDelta.x>0)
                                    {
                                        if(currentToolOpacity <= 0.98)currentToolOpacity+=0.02
                                        else
                                        {
                                            currentToolOpacity=1.00
                                        }
                                        toolSelectorModel.setProperty(currentTool, "toolOpacity", currentToolOpacity)
                                    }
                                    else if(wheel.angleDelta.x<0)
                                    {
                                        if(currentToolOpacity >0.02) currentToolOpacity-=0.02
                                        toolSelectorModel.setProperty(currentTool, "toolOpacity", currentToolOpacity)
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
                visible: (currentTool !== -1 && currentTool !== 1)
                color: "transparent"

                ListView{ id: presetColours
                    x: frameWidth +spacing*2
                    width: parent.width - frameWidth*2 - spacing*2
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: (itemSpacing!==0)?itemSpacing/2: 0
                    clip: true
                    orientation: ListView.Horizontal
                    interactive: isTestMode

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
                            border.color: parent.isMouseHovered? toolActiveBgColor: (currentToolColor === preset)? toolActiveTextColor: "black"

                            MouseArea{
                                id: presetMArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    currentToolColor = currentColorPresetModel.get(index).preset
                                    toolSelectorModel.setProperty(currentTool, "toolColor", currentToolColor)
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
                                    currentColorPresetModel.setProperty(index, "preset", currentToolColor.toString())
                                    toolSelectorModel.setProperty(currentTool, "toolColor", currentToolColor)
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
            color: currentToolColor
            onAccepted: {
                currentToolColor = currentColor
                toolSelectorModel.setProperty(currentTool, "toolColor", currentToolColor)
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
                preset: "red"
            }
            ListElement{
                preset: "orange"
            }
            ListElement{
                preset: "yellow"
            }
            ListElement{
                preset: "green"
            }
            ListElement{
                preset: "teal"
            }
            ListElement{
                preset: "light blue"
            }
            ListElement{
                preset: "blue"
            }
            ListElement{
                preset: "purple"
            }
            ListElement{
                preset: "black"
            }
        }
        ListModel{ id: textColourPresetsModel
            ListElement{
                preset: "red"
            }
            ListElement{
                preset: "orange"
            }
            ListElement{
                preset: "yellow"
            }
            ListElement{
                preset: "green"
            }
            ListElement{
                preset: "teal"
            }
            ListElement{
                preset: "light blue"
            }
            ListElement{
                preset: "blue"
            }
            ListElement{
                preset: "purple"
            }
            ListElement{
                preset: "black"
            }
        }
        ListModel{ id: shapesColourPresetsModel
            ListElement{
                preset: "red"
            }
            ListElement{
                preset: "orange"
            }
            ListElement{
                preset: "yellow"
            }
            ListElement{
                preset: "green"
            }
            ListElement{
                preset: "teal"
            }
            ListElement{
                preset: "light blue"
            }
            ListElement{
                preset: "blue"
            }
            ListElement{
                preset: "purple"
            }
            ListElement{
                preset: "black"
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
        height: toolSelectorFrame.height/1.6

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
            orientation: ListView.Horizontal
            interactive: isTestMode

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
            Rectangle{
                property bool isMousePressed: false
                property bool isMouseHovered: mArea.containsMouse
                width: toolActionUndoRedo.width/modelUndoRedo.count - toolActionUndoRedo.spacing
                height: buttonHeight;
                color: isMousePressed || isMouseHovered? toolActiveBgColor: toolInactiveBgColor;
                scale: isMousePressed? 0.9: 1

                Text{
                    text: action
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    color: isMousePressed || parent.isMouseHovered? toolActiveTextColor: textButtonColor
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    anchors.verticalCenter: parent.verticalCenter
                }

                MouseArea{
                    id: mArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: {
                        isMousePressed = true
                    }
                    onReleased: {
                        isMousePressed = false
                    }
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
            orientation: ListView.Horizontal
            interactive: isTestMode

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
            Rectangle{
                property bool isMousePressed: false
                property bool isMouseHovered: mouseArea.containsMouse
                width: toolActionCopyPasteClear.width/modelCopyPasteClear.count - toolActionCopyPasteClear.spacing
                height: buttonHeight;
                color: isMousePressed || isMouseHovered? toolActiveBgColor: toolInactiveBgColor;
                scale: isMousePressed? 0.9: 1

                Text{
                    text: action
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    color: isMousePressed || parent.isMouseHovered? toolActiveTextColor: textButtonColor
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    anchors.verticalCenter: parent.verticalCenter
                }

                MouseArea{ id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: {
                        isMousePressed = true
                    }
                    onReleased: {
                        isMousePressed = false
                    }
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

        ComboBox { id: dropdownAnnotations
            property bool isMouseHovered: dropdownAnnotationsMArea.containsMouse
            width: parent.width/1.3;
            height: buttonHeight
            anchors.top: displayAnnotations.bottom
            anchors.topMargin: itemSpacing
            anchors.horizontalCenter: parent.horizontalCenter
            model: [ "Only When Paused", "Always", "With Drawing Tools" ]

            background:
            Rectangle{
                radius: height/1.2
                color: (dropdownAnnotations.isMouseHovered)? toolActiveBgColor: toolInactiveBgColor
            }

            contentItem:
            Rectangle{
                radius: height/1.2
                color: "transparent";
                width: dropdownAnnotations.width
                height: dropdownAnnotations.height
                Text{
                    text: dropdownAnnotations.displayText
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    color: textValueColor
                    width: dropdownAnnotations.width
                    horizontalAlignment: Text.AlignHCenter
                    height: dropdownAnnotations.height
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // indicator:
            // Item{
            //     implicitHeight: dropdownAnnotations.contentHeight
            //     implicitWidth: dropdownAnnotations.contentHeight
            //     Image{
            //         width: width; height: parent.implicitHeight;
            //         horizontalAlignment: Image.AlignRight
            //         source: "qrc:///feather_icons/chevron-up.svg"
            //     }
            //     Image{
            //         width: width; height: parent.implicitHeight;
            //         horizontalAlignment: Image.AlignRight
            //         source: "qrc:///feather_icons/chevron-down.svg"
            //     }
            // }

            popup:
            Popup{
                id: popupOptions
                width: dropdownAnnotations.width
                height: contentItem.implicitHeight
                padding: 0

                contentItem:
                ListView {
                    interactive: isTestMode
                    implicitHeight: contentHeight
                    model: dropdownAnnotations.popup.visible ? dropdownAnnotations.delegateModel: null
                }

                background:
                Rectangle{
                    radius: frameRadius*3
                    color: toolInactiveBgColor
                    border.color: frameColor
                }
            }
            delegate:
            ItemDelegate {
                width: dropdownAnnotations.width
                height: dropdownAnnotations.height
                padding: 0

                leftPadding: width/4.5
                text: dropdownAnnotations.textRole ? (Array.isArray(dropdownAnnotations.model) ? modelData[dropdownAnnotations.textRole]: model[dropdownAnnotations.textRole]): modelData
                palette.text: dropdownAnnotations.palette.text
                palette.highlightedText: dropdownAnnotations.palette.highlightedText
                font.weight: dropdownAnnotations.currentIndex === index ? Font.DemiBold: Font.Normal
                font.pixelSize: fontSize // /1.2
                font.family: fontFamily
                highlighted: dropdownAnnotations.highlightedIndex === index

                // contentItem:
                // Rectangle{
                //     id: rectDlgt
                //     width: dropdownAnnotations.width
                //     height: dropdownAnnotations.implicitHeight
                //     color: toolInactiveBgColor
                //     Text{
                //         text: modelData
                //         font.pixelSize: fontSize/1.2
                //         font.family: fontFamily
                //         font.weight: dropdownAnnotations.currentIndex === index ? Font.DemiBold: Font.Normal
                //         color: textValueColor
                //         width: parent.width
                //         horizontalAlignment: Text.AlignHCenter
                //         height: parent.height
                //         verticalAlignment: Text.AlignVCenter
                //     }
                // }
            }

            MouseArea{
                id: dropdownAnnotationsMArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    popupOptions.open()
                }
            }
        }


        Rectangle{ id: advancedOptionsBtn
            property bool isMouseHovered: advancedOptionsMArea.containsMouse
            width: parent.width/1.3;
            height: buttonHeight // /1.3;
            radius: height/1.2
            color: isMouseHovered? toolActiveBgColor: toolInactiveBgColor;
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
            Text{
                text: "Advanced Options"
                font.pixelSize: fontSize // /1.2
                font.family: fontFamily
                color: parent.isMouseHovered? textValueColor: textButtonColor
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
            }

            MouseArea{
                id: advancedOptionsMArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    isAdvancedOptionsClicked = !isAdvancedOptionsClicked
                }
            }
        }
    }

}


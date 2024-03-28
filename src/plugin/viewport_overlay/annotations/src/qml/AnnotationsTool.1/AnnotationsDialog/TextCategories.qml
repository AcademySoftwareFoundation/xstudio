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

Item{ id: textCategory

    property real itemSpacing: framePadding/2
    property real framePadding: 6
    property real framePadding_x2: framePadding*2
    property real frameWidth: 1

    property int iconsize: XsStyle.menuItemHeight *.66
    property real fontSize: XsStyle.menuFontSize/1.1
    property string fontFamily: XsStyle.menuFontFamily

    property color toolInactiveTextColor: XsStyle.controlTitleColor
    property color textButtonColor: toolInactiveTextColor
    property color textValueColor: "white"

    XsModuleAttributesModel {
        id: anno_font_options
        attributesGroupNames: "annotations_tool_fonts_0"
    }

    XsModuleAttributes {
        id: anno_tool_backend_settings
        attributesGroupNames: "annotations_tool_settings_0"
    }

    property color backgroundColorBackendValue: anno_tool_backend_settings.text_background_colour ? anno_tool_backend_settings.text_background_colour : "#000000"
    property int backgroundOpacityBackendValue: anno_tool_backend_settings.text_background_opacity ? anno_tool_backend_settings.text_background_opacity : 0

    property color backgroundColor: backgroundColorBackendValue
    property int backgroundOpacity: backgroundOpacityBackendValue

    Repeater {

        // Using a repeater here - but 'vp_mouse_wheel_behaviour_attr' only
        // has one row by the way. The use of a repeater means the model role
        // data are all visible in the XsComboBox instance.
        model: anno_font_options

        XsComboBox { 
        
            id: dropdownFonts
    
            width: parent.width -framePadding_x2 -itemSpacing/2
            height: buttonHeight
            model: combo_box_options
            anchors.left: parent.left
            anchors.leftMargin: framePadding
            anchors.verticalCenter: parent.verticalCenter

            property var value_: value ? value : null
            onValue_Changed: {
                currentIndex = indexOfValue(value_)
            }

            Component.onCompleted: currentIndex = indexOfValue(value_)
            onCurrentValueChanged: {
                value = currentValue;
            }
        }
    }

    XsButton{ id: fillOpacityProp
        property bool isPressed: false
        property bool isMouseHovered: opacityMArea.containsMouse
        property real prevValue: defaultValue/2
        property real defaultValue: 50
        isActive: isPressed
        
        width: (parent.width-framePadding_x2)/2 -itemSpacing/2
        height: buttonHeight
        anchors.right: parent.right
        anchors.rightMargin: framePadding
        // anchors.verticalCenter: parent.verticalCenter
        y: buttonHeight*3 -1

        Text{
            text: "BG Opac."
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
            text: backgroundOpacity
            property var backendOpacity: backgroundOpacity
            // we don't set this anywhere else, so this is read-only - always tracks the backend opacity value
            onBackendOpacityChanged: {
                // if the backend value has changed, update the text
                text = backgroundOpacity
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
                if(parseInt(text) >= 100) {
                    anno_tool_backend_settings.text_background_opacity = 100
                }
                else if(parseInt(text) <= 0) {
                    anno_tool_backend_settings.text_background_opacity = 0
                }
                else {
                    anno_tool_backend_settings.text_background_opacity = parseInt(text)
                }
                selectAll()
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

                    let deltaValue = parseInt(deltaMX*stepSize)
                    let valueToApply = Math.round(valueOnPress + deltaValue)

                    if(deltaMX>0)
                    {
                        if(valueToApply >= 100) {
                            anno_tool_backend_settings.text_background_opacity = 100
                            valueOnPress = 100
                            prevMX = mouseX
                        }
                        else {
                            anno_tool_backend_settings.text_background_opacity = valueToApply
                        }
                    }
                    else {
                        if(valueToApply < 1){
                            anno_tool_backend_settings.text_background_opacity = 0
                            valueOnPress = 0
                            prevMX = mouseX
                        }
                        else {
                            anno_tool_backend_settings.text_background_opacity = valueToApply
                        }
                    }
                    opacityDisplay.text = backgroundOpacity
                }
            }
            onPressed: {
                prevMX = mouseX
                valueOnPress = anno_tool_backend_settings.text_background_opacity

                parent.isPressed = true
                focus = true
            }
            onReleased: {
                parent.isPressed = false
                focus = false
            }
            onDoubleClicked: {
                if(anno_tool_backend_settings.text_background_opacity == fillOpacityProp.defaultValue){
                    anno_tool_backend_settings.text_background_opacity = fillOpacityProp.prevValue
                }
                else{
                    fillOpacityProp.prevValue = backgroundOpacity
                    anno_tool_backend_settings.text_background_opacity = fillOpacityProp.defaultValue
                }
                opacityDisplay.text = backgroundOpacity
            }
        }
    }
    
    XsButton{ id: fillColorProp
        property bool isPressed: false
        property bool isMouseHovered: fillMArea.containsMouse
        isActive: isPressed
        width: (parent.width-framePadding_x2)/2 -itemSpacing/2
        height: buttonHeight
        anchors.right: parent.right
        anchors.rightMargin: framePadding
        y: buttonHeight*4 + itemSpacing -1

        MouseArea{
            id: fillMArea
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
            text: " BG Col."
            font.pixelSize: fontSize
            font.family: fontFamily
            color: parent.isPressed || parent.isMouseHovered? textValueColor: textButtonColor
            width: parent.width/2
            horizontalAlignment: Text.AlignLeft //HCenter
            anchors.left: parent.left
            anchors.leftMargin: framePadding
            topPadding: framePadding/1.2
        }
        Rectangle{ id: colorPreview
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: parent.width/7
            anchors.right: parent.right
            anchors.rightMargin: parent.width/10
            height: parent.height/1.4;
            color: backgroundColor
            border.width: frameWidth
            border.color: (color=="white" || color=="#ffffff")? "black": "white"
            MouseArea{
                id: dragArea
                anchors.fill: parent
                onReleased: {
                    fillColorProp.isPressed = false
                }
                onClicked: {
                    fillColorProp.isPressed = false
                    colorDialog.open()
                }
                onPressed: {
                    fillColorProp.isPressed = true
                }
            }
        }
    }

    ColorDialog { id: colorDialog
        title: "Please pick a BG-Colour"
        color: backgroundColor
        onAccepted: {
            anno_tool_backend_settings.text_background_colour = color
            close()
        }
        onRejected: {
            close()
        }
    }

}
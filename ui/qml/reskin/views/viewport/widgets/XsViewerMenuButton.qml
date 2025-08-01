// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15
import QtQml.Models 2.12

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Item{
    id: widget

    property alias buttonWidget: buttonWidget

    property color bgColorPressed: palette.highlight //"#D17000" 
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    property bool isBgGradientVisible: true
    property color textColor: XsStyleSheet.secondaryTextColor //"#F1F1F1"
    property var tooltip: ""
    property var tooltipTitle: ""
    property alias bgDiv: bgDiv
    // property var textElide: textDiv.elide
    // property alias textDiv: textDiv
    property bool clickDisabled: false //enabled

    property string text: ""
    property string hotkeyText: ""
    property string shortText: ""
    property string valueText: ""
    property bool isShortened: false
    property real shortThresholdWidth: 100
    property bool isShortTextOnly: false
    property bool showValueWhenShortened: false
    property real shortOnlyThresholdWidth: shortThresholdWidth-40
        //Math.max(60 , statusDiv.textWidth) 
        //textDiv.textWidth +statusDiv.textWidth

    // property alias menuWidth: btnMenu.menuWidth
    property real menuWidth: width
    // property alias menu: menuOptions
    // property string menuValue: "" //menuOptions.menuAt(menuOptions.currentIndex)
    property bool isActive: btnMenu.visible
    property bool subtleActive: false
    property bool isMultiSelectable: false

    property var menuModel: ""

    function closeMenu()
    {
        btnMenu.visible = false
    }

    function menuTriggered(value){
        valueText = value
        closeMenu()
    }

    onWidthChanged: {
        if(width < shortThresholdWidth) {
            isShortened = true
            if(width < shortOnlyThresholdWidth) {
                if(showValueWhenShortened) isShortTextOnly = false
                else isShortTextOnly = true
            }
            else isShortTextOnly = false
        }
        else {
            isShortened = false
            isShortTextOnly = false
        }
    }

    Button {
        id: buttonWidget
    
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        anchors.fill: parent
        hoverEnabled: !clickDisabled
        focusPolicy: Qt.NoFocus
    
        contentItem:
        Item{ id: contentDiv
            anchors.fill: parent
            opacity: enabled ? 1.0 : 0.33

            Item{
                width: parent.width>itemsWidth? itemsWidth+2 : parent.width
                height: parent.height
                anchors.centerIn: parent
                clip: true

                property real itemsWidth: textDiv.textWidth +statusDiv.textWidth
            
                XsText {
                    id: textDiv
                    text: isShortened? 
                        showValueWhenShortened? "" : widget.shortText 
                        : hotkeyText==""? 
                            widget.text : 
                            widget.text+" ("+hotkeyText+")"
                    color: textColor
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    elide: Text.ElideMiddle
                    // font: buttonWidget.font
                    font.pixelSize: XsStyleSheet.fontSize
                    font.family: XsStyleSheet.fontFamily
                }   
                XsText {
                    id: statusDiv
                    text: isShortTextOnly? 
                        showValueWhenShortened? valueText : ""
                        : " "+valueText 
                    font.pixelSize: XsStyleSheet.fontSize
                    font.family: XsStyleSheet.fontFamily
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: textDiv.left
                    anchors.leftMargin: textDiv.textWidth
                    clip: true
                    elide: Text.ElideMiddle

                    // Rectangle{anchors.fill: parent; color: "blue"; opacity:.3}
                    
                    // onTextWidthChanged:{
                    //     if(textWidth>textDiv.textWidth) textWidth = textDiv.textWidth
                    // }

                    // width: contentDiv.width - textDiv.textWidth - 2
                    // Rectangle{anchors.fill: parent; color: "red"; opacity:.3}
                }
            }
        }

        // XsToolTip{ //.#TODO:
            //         text: parent.text
            //         visible: buttonWidget.hovered && parent.truncated
            //         width: buttonWidget.width == 0? 0 : 150
            //         x: 0
            //     } 
        // ToolTip.text: buttonWidget.text
        // ToolTip.visible: buttonWidget.hovered && textDiv.truncated
    

        background:
        Rectangle {
            id: bgDiv
            implicitWidth: 100
            implicitHeight: 40
            border.color: buttonWidget.down || buttonWidget.hovered ? bgColorPressed: borderColorNormal
            border.width: borderWidth
            color: "transparent"

            Rectangle{
                visible: isBgGradientVisible
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: buttonWidget.down || (isActive && !subtleActive)? bgColorPressed: "#33FFFFFF" }
                    GradientStop { position: 1.0; color: buttonWidget.down || (isActive && !subtleActive)? bgColorPressed: forcedBgColorNormal }
                }
            }
    
            Rectangle {
                id: bgFocusDiv
                implicitWidth: parent.width+borderWidth
                implicitHeight: parent.height+borderWidth
                visible: buttonWidget.activeFocus
                color: "transparent"
                opacity: 0.33
                border.color: bgColorPressed
                border.width: borderWidth
                anchors.centerIn: parent 
            }
        }
    
        //onPressed: focus = true
        //onReleased: focus = false
        
        
        onClicked: {
            btnMenu.x = x//-btnMenu.width
            btnMenu.y = y-btnMenu.height
            btnMenu.visible = !btnMenu.visible
        }
    }
    MouseArea{ id: clickBlocker
        anchors.centerIn: parent
        enabled: clickDisabled
        width: enabled? parent.width : 0
        height: enabled? parent.height : 0
    }


    // This menu works by picking up the 'value' and 'combo_box_options' role
    // data that is exposed via the model that instantiated this XsViewerMenuButton
    // instance
    XsMenuMultiChoice { 
        id: btnMenu
        visible: false
    }

}

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.12

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Item {

    // Note that the model gives use a boolean 'is_checked', plus 'name' and
    // 'hotkey' strings.
                
    id: widget
    width: parentWidth //(menuWidth > menuStdWidth)? menuWidth : menuStdWidth
    height: XsStyleSheet.menuHeight
    
    property var menu_model
    property var menu_model_index
    property var parent_menu

    property string label: name ? name : ""
    property bool isChecked: isRadioButton? radioSelectedChoice==label : is_checked

    signal clicked()

    property bool isHovered: menuMouseArea.containsMouse
    property bool isActive: menuMouseArea.pressed
    property bool isFocused: menuMouseArea.activeFocus

    property real parentWidth: 0
    property real menuWidth: parentWidth<menuRealWidth? menuRealWidth : parentWidth
    property real menuRealWidth: checkBox.width + labelMetrics.width + (checkBoxPadding*2) + (labelPadding)
    property real menuStdWidth: XsStyleSheet.menuStdWidth

    property bool isRadioButton: false
    property string radioSelectedChoice: ""
    property real checkBoxSize: XsStyleSheet.menuIndicatorSize
    property real checkBoxPadding: XsStyleSheet.menuLabelPaddingLR/4 //XsStyleSheet.menuPadding*2
    property real labelPadding: XsStyleSheet.menuPadding
    property color bgColorActive: palette.highlight
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color labelColor: palette.text
    property color hotKeyColor: XsStyleSheet.menuHotKeyColor
    property color borderColorHovered: palette.highlight
    property color borderColorNormal: "transparent"
    property real borderWidth: XsStyleSheet.widgetBorderWidth

    MouseArea{
        id: menuMouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true

        onClicked: {
            widget.clicked()
        }
    }

    Rectangle {
        id: bgHighlightDiv
        anchors.fill: parent
        color: widget.isActive ? bgColorActive : forcedBgColorNormal
        border.color: widget.isHovered ? borderColorHovered : borderColorNormal
        border.width: borderWidth
    }
    Rectangle {
        id: bgFocusDiv
        visible: widget.isFocused
        anchors.fill: parent
        color: "transparent"
        opacity: 0.33
        border.color: borderColorHovered
        border.width: borderWidth
        anchors.centerIn: parent 
    }

    // Image {
    //     id: checkBox
    //     source: isRadioButton? 
    //         isChecked?  "qrc:/icons/radio_button_checked.svg" : "qrc:/icons/radio_button_unchecked.svg" :
    //         isChecked?  "qrc:/icons/check_box_checked.svg" : "qrc:/icons/check_box_unchecked.svg"
    //     width: checkBoxSize
    //     height: checkBoxSize
    //     anchors.left: parent.left
    //     anchors.leftMargin: checkBoxPadding
    //     anchors.verticalCenter: parent.verticalCenter
    //     layer {
    //         enabled: true
    //         effect: ColorOverlay { color: isChecked? widget.highlighted?labelColor:bgColorPressed : hotKeyColor }
    //     }
    // }
    Rectangle{anchors.fill: checkBox; color: bgColorPressed; visible: isChecked; scale: 0.8; radius: isRadioButton? width/2:0}         
    Image {
        id: checkBox
        source: isRadioButton? 
            isChecked?  "qrc:/icons/radio_button_checked.svg" : "qrc:/icons/radio_button_unchecked.svg" :
            isChecked?  "qrc:/icons/check_box_checked.svg" : "qrc:/icons/check_box_unchecked.svg"
        width: checkBoxSize
        height: checkBoxSize
        anchors.left: parent.left
        anchors.leftMargin: checkBoxPadding
        anchors.verticalCenter: parent.verticalCenter
        layer {
            enabled: true
            // effect: ColorOverlay { color: isChecked? labelColor : hotKeyColor }
            effect: ColorOverlay { color: hotKeyColor }
        }
    }






    // Image {
    //     id: extraBoxFrame
    //     source: isRadioButton? "qrc:/icons/radio_button_unchecked.svg" :
    //             "qrc:/icons/check_box_unchecked.svg"
    //     width: checkBoxSize
    //     height: checkBoxSize
    //     anchors.left: parent.left
    //     anchors.leftMargin: checkBoxPadding
    //     anchors.verticalCenter: parent.verticalCenter
    //     visible: isChecked
    //     layer {
    //         enabled: true
    //         effect: ColorOverlay { color: hotKeyColor }
    //     }
    // }


    Text {
        id: labelDiv
        text: label
        height: parent.height
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        color: labelColor 
        anchors.left: checkBox.right
        anchors.leftMargin: labelPadding
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        width: parent.width - (checkBoxSize + checkBoxPadding*2)
        // clip: true
    }
    Text {
        id: hotKeyDiv
        text: hotkey ? "   " + hotkey : ""
        height: parent.height
        font: labelDiv.font
        color: hotKeyColor 
        anchors.right: parent.right
        anchors.rightMargin: checkBoxPadding //labelPadding
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }
    TextMetrics {
        id: labelMetrics
        font: labelDiv.font
        text: labelDiv.text + hotKeyDiv.text
    }

}
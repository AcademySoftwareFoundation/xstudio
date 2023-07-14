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
    width: menuRealWidth<menuStdWidth? menuStdWidth : menuRealWidth
    height: XsStyleSheet.menuHeight
    property real menuRealWidth: checkBox.width + labelMetrics.width + (checkBoxPadding*2) + (labelPadding)

    property var menu_model
    property var menu_model_index

    property string label: name ? name : ""
    property bool isChecked: isRadioButton? radioSelectedChoice==label : is_checked

    signal checked()

    property bool isHovered: menuMouseArea.containsMouse
    property bool isActive: menuMouseArea.pressed
    property bool isFocused: menuMouseArea.activeFocus

    property bool isRadioButton: false
    property var radioSelectedChoice: ""
    property real menuStdWidth: XsStyleSheet.menuStdWidth
    property real checkBoxSize: XsStyleSheet.menuIndicatorSize
    property real checkBoxPadding: XsStyleSheet.menuPadding*2
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
            // isChecked = !isChecked
            if (!isRadioButton) is_checked = !is_checked
            console.log("is_checked", is_checked)
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

    Image {
        id: checkBox
        source: isRadioButton? 
            isChecked?  "qrc:/assets/icons/new/radio_button_checked.svg" : "qrc:/assets/icons/new/radio_button_unchecked.svg" :
            isChecked?  "qrc:/assets/icons/new/check_box_checked.svg" : "qrc:/assets/icons/new/check_box_unchecked.svg"
        width: checkBoxSize
        height: checkBoxSize
        anchors.left: parent.left
        anchors.leftMargin: checkBoxPadding
        anchors.verticalCenter: parent.verticalCenter
        layer {
            enabled: true
            effect: ColorOverlay { color: isChecked? widget.highlighted?labelColor:bgColorPressed : hotKeyColor }
        }
    }


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
    }
    Text {
        id: hotKeyDiv
        text: hotkey ? "   " + hotkey : ""
        height: parent.height
        font: labelDiv.font
        color: hotKeyColor 
        anchors.right: parent.right
        anchors.rightMargin: labelPadding
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }
    TextMetrics {
        id: labelMetrics
        font: labelDiv.font
        text: labelDiv.text + hotKeyDiv.text
    }

}
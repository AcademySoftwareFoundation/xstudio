import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.15

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Item {

    id: widget            
    width: ( (menuRealWidth > menuStdWidth) || is_in_bar )? menuRealWidth : menuStdWidth
    height: XsStyleSheet.menuHeight

    property var menu_model
    property var menu_model_index
    property var sub_menu: null
    property bool is_in_bar: false
    property var parent_menu

    property bool isHovered: menuMouseArea.containsMouse
    property bool isActive: menuMouseArea.pressed || isSubMenuActive
    property bool isFocused: menuMouseArea.activeFocus
    property bool isSubMenuActive: sub_menu? sub_menu.visible : false

    property real menuStdWidth: XsStyleSheet.menuStdWidth
    property real menuRealWidth: labelMetrics.width + (labelPadding*2)
 
    property real labelPadding: is_in_bar? XsStyleSheet.menuLabelPaddingLR : XsStyleSheet.menuLabelPaddingLR/2
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
            if (sub_menu) {
                if (is_in_bar) {
                    sub_menu.x = 0
                    sub_menu.y = widget.height
                } else {
                    sub_menu.x = widget.width
                    sub_menu.y = 0
                }
                sub_menu.visible = true
            } else {

                // this ultimately sends a signal to the 'GlobalUIModelData'
                // indicating that a menu item was activated. It then messages
                // all instances of XsMenuModelItem that are registered to the
                // given menu item. The parent menu is used to identify which 
                // menu instance the click came from in the case where there
                // are multiple instances of the same menu.
                console.log("widget.parent", widget.parent)
                menu_model.nodeActivated(menu_model_index)
            }
        }
    }

    Rectangle {
        id: bgHighlightDiv
        implicitWidth: parent.width
        implicitHeight: widget.is_in_bar? parent.height- (borderWidth*2) : parent.height
        anchors.verticalCenter: parent.verticalCenter
        color: widget.isActive ? bgColorActive : forcedBgColorNormal
        border.color: widget.isHovered ? borderColorHovered : borderColorNormal
        border.width: borderWidth
    }
    Rectangle {
        id: bgFocusDiv
        visible: widget.isFocused
        implicitWidth: parent.width
        implicitHeight: widget.is_in_bar? parent.height+ (borderWidth*2) : parent.height
        anchors.verticalCenter: parent.verticalCenter
        color: "transparent"
        opacity: 0.33
        border.color: borderColorHovered
        border.width: borderWidth
        anchors.centerIn: parent 
    }

    Text {
        id: labelDiv
        text: name ? name : "Unknown" //+ (sub_menu && !is_in_bar ? "   >>" : "")
        height: parent.height
        font.pixelSize: XsStyleSheet.fontSize
        font.family: XsStyleSheet.fontFamily
        color: labelColor 
        anchors.left: parent.left
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

    Image {
        id: subMenuIndicatorDiv
        visible: sub_menu? !is_in_bar: false
        source: "qrc:/assets/icons/new/chevron_right.svg"
        sourceSize.height: 16
        sourceSize.width: 16
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: labelPadding
        smooth: true
        antialiasing: true
        layer {
            enabled: true
            effect: ColorOverlay { color: hotKeyColor }
        }
    }


    Component.onCompleted: {
        make_submenus()
    }

    function make_submenus() {

        if (!menu_model_index.valid) return;
        if (menu_model.rowCount(menu_model_index)) {
            let component = Qt.createComponent("./XsMenu.qml")
            if (component.status == Component.Ready) {
                sub_menu = component.createObject(
                    widget,
                    {
                        menu_model: widget.menu_model,
                        menu_model_index: widget.menu_model_index
                    })

            } else {
                console.log("Failed to create menu component: ", component, component.errorString())
            }
        } else if (menu_model.get(menu_model_index,"menu_item_type") == "multichoice") {
            let component = Qt.createComponent("./XsMenuMultiChoice.qml")
            if (component.status == Component.Ready) {
                sub_menu = component.createObject(
                    widget,
                    {
                        menu_model: widget.menu_model,
                        menu_model_index: widget.menu_model_index
                    })

            } else {
                console.log("Failed to create menu component: ", component, component.errorString())
            }
        } else if (menu_model.get(menu_model_index,"menu_item_type") == "choice") {
            let component = Qt.createComponent("./XsMenuChoice.qml")
            if (component.status == Component.Ready) {
                sub_menu = component.createObject(
                    widget,
                    {
                        menu_model: widget.menu_model,
                        menu_model_index: widget.menu_model_index
                    })

            } else {
                console.log("Failed to create menu component: ", component, component.errorString())
            }
        }
    }

    
}
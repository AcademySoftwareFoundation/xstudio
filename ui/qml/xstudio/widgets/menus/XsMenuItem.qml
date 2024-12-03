import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.3

import xStudio 1.0
import xstudio.qml.models 1.0

Item {

    id: widget
    height: XsStyleSheet.menuHeight

    // Note .. menu item width needs to be set by the widest menu item in the
    // same menu. This creates a circular dependency .. the menu item width
    // depends on the widest item. If it is the widest item its width
    // depends on itself. There must be a better QML solution for this
    property real minWidth: gap.width + final_spacer.width + indentSpacer.width + iconDiv.width + hotkey_metrics.width + label_metrics.width + gap.width + subMenuIndicatorDiv.width + margin*4

    property real indent: 0
    property real leftIconSize: 3 + iconDiv.width
    property var margin: 4
    property var sub_menu
    property var menu_model
    property var menu_model_index
    property bool is_in_bar: false
    property var parent_menu
    property alias icon: iconDiv.source
    property var is_enabled: typeof menu_item_enabled !== 'undefined' ? menu_item_enabled : true
    property bool has_sub_menu: false

    property bool isHovered: menuMouseArea.containsMouse
    property bool isActive: menuMouseArea.pressed

    property real labelPadding: is_in_bar? XsStyleSheet.menuLabelPaddingLR/2 : XsStyleSheet.menuLabelPaddingLR/2
    property color hotKeyColor: XsStyleSheet.secondaryTextColor
    property real borderWidth: XsStyleSheet.widgetBorderWidth
    property var __menuContextData: menuContextData

    onVisibleChanged: {
        if (sub_menu && !visible) sub_menu.visible = false
    }

    enabled: is_enabled

    // this darkens the text, icons etc if disabled
    opacity: enabled ? 1 : 0.5

    onIsHoveredChanged: {
        if (isHovered && (!is_in_bar || typeof parent_menu.has_focus === "undefined" || parent_menu.has_focus)) {
            callbackTimer.setTimeout(function(w) { return function() {
                if (w.isHovered) {
                    showSubMenu()
                }
                }}(widget), 200);
        }
    }

    function hideSubMenus() {
        if (sub_menu) {
            sub_menu.visible = false
            sub_menu.x = sub_menu.x+20
        }
    }

    function showSubMenu() {
        if (parent_menu) parent_menu.hideOtherSubMenus(widget)
        //else console.log("NO PARENT", name)
        make_submenus()
        if (!sub_menu) return;

        if (is_in_bar) {
            sub_menu.showMenu(
                widget,
                0,
                widget.height);
        } else {
            sub_menu.showMenu(
                widget,
                widget.width,
                0,
                0);
        }
    }

    MouseArea
    {

        id: menuMouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        onClicked: {
            showSubMenu()
            if (is_in_bar) {
                parent_menu.has_focus = true
            }
            if (!sub_menu) {

                // this ultimately sends a signal to the 'GlobalUIModelData'
                // indicating that a menu item was activated. It then messages
                // all instances of XsMenuModelItem that are registered to the
                // given menu item. The parent menu is used to identify which
                // menu instance the click came from in the case where there
                // are multiple instances of the same menu.
                // console.log("widget.parent", widget.parent)
                menu_model.nodeActivated(menu_model_index, "menu", helpers.contextPanel(widget))
                parent_menu.closeAll()

            }
        }
    }

    Rectangle {
        id: bgHighlightDiv
        implicitWidth: parent.width
        implicitHeight: widget.is_in_bar? parent.height- (borderWidth*2) : parent.height
        anchors.verticalCenter: parent.verticalCenter
        border.color: palette.highlight
        border.width: borderWidth
        color: isActive ? palette.highlight : "transparent"
        visible: widget.isHovered

    }

    RowLayout {

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: margin
        spacing: 0

        XsImage {

            id: iconDiv
            visible: source != ""
            source: menu_icon ? menu_icon : ""
            Layout.preferredWidth: source != "" ? height : 0
            //height: parent.height
            Layout.alignment: Qt.AlignVCenter
            Layout.fillHeight: true
            layer {
                enabled: true
                effect: ColorOverlay { color: hotKeyColor }
            }

        }

        Item {
            id: indentSpacer
            Layout.preferredWidth: iconDiv.width ? 3 : indent
        }

        XsText {

            id: labelDiv
            text: name ? name : "Unknown" //+ (sub_menu && !is_in_bar ? "   >>" : "")
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            color: palette.text
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignVCenter

        }

        TextMetrics {
            id:     label_metrics
            font:   labelDiv.font
            text:   labelDiv.text
        }

        Item {
            id: gap
            width: hotKeyDiv.text != "" || subMenuIndicatorDiv.visible ? 10 : 0
        }

        Item {
            Layout.fillWidth: true
        }

        XsText {

            Layout.alignment: Qt.AlignVCenter
            id: hotKeyDiv
            text: hotkey_sequence ? hotkey_sequence : ""
            font: labelDiv.font
            color: hotKeyColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        TextMetrics {
            id:     hotkey_metrics
            font:   hotKeyDiv.font
            text:   hotKeyDiv.text
        }

        Image {

            Layout.alignment: Qt.AlignVCenter
            id: subMenuIndicatorDiv
            visible: widget.has_sub_menu ? !is_in_bar: false
            width: visible ? 16 : 0
            source: "qrc:/icons/chevron_right.svg"
            sourceSize.height: 16
            sourceSize.width: 16
            smooth: true
            layer {
                enabled: true
                effect: ColorOverlay { color: hotKeyColor }
            }
        }

        // this is needed because the chevron has about 3 empty pixels around it
        // meaning it doesn't line up with the right edge of hotkey text ...
        // there is probably a better way?!
        Item {
            id: final_spacer
            width : hotkey_sequence ? 4 : 0
        }

    }

    Component.onCompleted: {
        if (menu_model && menu_model.rowCount(menu_model_index) || menu_model.get(menu_model_index,"menu_item_type") == "multichoice") {
            has_sub_menu = true
        }
    }

    function make_submenus() {

        if (sub_menu) return;

        if (menu_model.rowCount(menu_model_index)) {
            let component = Qt.createComponent("./XsMenu.qml")
            if (component.status == Component.Ready) {
                sub_menu = component.createObject(
                    widget,
                    {
                        parent_menu: widget.parent_menu,
                        menu_model: widget.menu_model,
                        menu_model_index: widget.menu_model_index,
                        menuContextData: widget.__menuContextData
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
                        menu_model_index: widget.menu_model_index,
                        parent_menu: widget.parent_menu,
                    })

            } else {
                console.log("Failed to create menu component: ", component, component.errorString())
            }
        }

    }


}
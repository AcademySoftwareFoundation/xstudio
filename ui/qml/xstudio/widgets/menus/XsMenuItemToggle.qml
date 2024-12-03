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
    property real minWidth: leftIconSize + final_spacer.width + hotkey_metrics.width + label_metrics.width + margin*4

    property real leftIconSize: menuIcon? indentSpacer.width + checkDiv.width + iconDiv.width : indentSpacer.width + checkDiv.width

    property bool isChecked: isRadioButton? radioSelectedChoice==actualValue : is_checked
    property bool isRadioButton: false
    property var radioSelectedChoice: ""
    property var label: name ? name : ""
    property var menuIcon: ""

    property var margin: 4 // don't need this ?
    property var sub_menu
    property var menu_model
    property var menu_model_index
    property var parent_menu
    property alias icon: checkDiv.source
    property var actualValue

    property bool isHovered: menuMouseArea.containsMouse
    property bool isActive: menuMouseArea.pressed
    property var is_enabled: typeof menu_item_enabled !== 'undefined' ? menu_item_enabled : true

    property color textColor: palette.text
    property color hotKeyColor: XsStyleSheet.secondaryTextColor
    
    property real borderWidth: XsStyleSheet.widgetBorderWidth

    enabled: is_enabled
    opacity: enabled ? 1 : 0.5

    function hideSubMenus() {}

    signal clicked()

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
        implicitWidth: parent.width
        implicitHeight: parent.height
        anchors.verticalCenter: parent.verticalCenter
        border.color: palette.highlight
        border.width: borderWidth
        color: isActive ? palette.highlight : "transparent"
        visible: widget.isHovered

    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: margin
        spacing: 0

        Item{
            width: XsStyleSheet.menuCheckboxSize
            height: XsStyleSheet.menuCheckboxSize

            XsImage {
                id: checkDiv
                anchors.fill: parent
                source: isRadioButton?
                    isChecked ? "qrc:/icons/radio_button_checked.svg" : "qrc:/icons/radio_button_unchecked.svg" :
                    isChecked ? "qrc:/icons/check_box_checked.svg" : "qrc:/icons/check_box_unchecked.svg"
                sourceSize.height: height
                sourceSize.width: width
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                layer {
                    enabled: true
                    effect: ColorOverlay { color: textColor }
                }
            }
        }
        XsImage {
            id: iconDiv
            visible: source != ""
            width: source != "" ? height : 0
            height: XsStyleSheet.menuCheckboxSize
            source: menuIcon ? menuIcon : ""
            // source: "qrc:/icons/swap_horiz.svg"
            sourceSize.height: height
            sourceSize.width: width
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            layer {
                enabled: true
                effect: ColorOverlay { color: textColor }
            }
        }


        Item {
            id: indentSpacer
            width: 3
        }

        XsText {
            id: labelDiv
            text: label ? label : "Unknown" //+ (sub_menu && !is_in_bar ? "   >>" : "")
            font.pixelSize: XsStyleSheet.fontSize
            font.family: XsStyleSheet.fontFamily
            color: textColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

        }

        TextMetrics {
            id:     label_metrics
            font:   labelDiv.font
            text:   labelDiv.text
        }

        Item {
            Layout.fillWidth: true
        }

        XsText {
            id: hotKeyDiv
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            text: hotkey_sequence ? hotkey_sequence : ""
            font: labelDiv.font
            color: hotKeyColor
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }

        TextMetrics {
            id:     hotkey_metrics
            font:   hotKeyDiv.font
            text:   hotKeyDiv.text
        }

        Item {
            id: final_spacer
            width : 4
        }
    }
}
// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12

import xStudio 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.uuid 1.0

MenuItem {

    id: menuItem

    implicitHeight: XsStyle.menuItemHeight
    font.pixelSize: XsStyle.menuFontSize

    property string iconbg: ""
    property alias iconcolor: colorolay.color
    property int iconsize: XsStyle.menuItemHeight *.66

    property string myicon: ""

    enabled: xs_module_menu_item_enabled === true ? true : xs_module_menu_item_enabled === false ? false : true
    checkable: xs_module_menu_item_checkable
    checked: checkable ? xs_module_menu_item_value : false
    text: xs_module_menu_item_text
    property var shortcut: undefined

    action: Action {
        onTriggered: {
            xs_module_menu_item_value = !xs_module_menu_item_value
        }
    }

    QMLUuid {
        id: null_uuid
    }

    XsHotkeyReference {
        id: hotkey_ref
        hotkeyUuid: menuItem.hotkey_uuid ? menuItem.hotkey_uuid : null_uuid.asQuuid
        onSequenceChanged: {
            if (sequence) {
                menuItem.shortcut = sequence
            }
        }
    }

    property var hotkey_uuid: xs_module_menu_hotkey_uuid ? xs_module_menu_hotkey_uuid : undefined

    indicator: Item {
        id:checkBoxItem
        // Icon (radio or checkbox)
        implicitWidth: XsStyle.menuItemHeight
        implicitHeight: XsStyle.menuItemHeight
        visible: checkable
        x: 3
        Rectangle {
            width: iconsize
            height: iconsize
            Gradient {
                id:indGrad
                orientation: Gradient.Horizontal
            }

//            color: menuItem.checked?XsStyle.highlightColor:"transparent"
            color: menuItem.checked?XsStyle.primaryColor:"transparent"
            function getBGGrad() {
                preferences.assignColorStringToGradient(iconbg, indGrad)
                return indGrad
            }
            gradient:iconbg===""?menuItem.checked?styleGradient.accent_gradient:Gradient.gradient:getBGGrad()

            anchors.centerIn: parent
            //            visible: menuItem.checkable
            border.width: 1 // mycheckableDecorated?1:0
            border.color: XsStyle.mainColor
            radius: iconsize / 2
            Image {
                id: checkIcon
                visible: menuItem.checked
                sourceSize.height: XsStyle.menuItemHeight
                sourceSize.width: XsStyle.menuItemHeight
                source: "qrc:///feather_icons/check.svg"
                width: iconsize // 2
                height: iconsize // 2
                anchors.centerIn: parent
            }
            ColorOverlay{
                id: colorolay
                anchors.fill: checkIcon
                source:checkIcon
                visible: menuItem.checked
                color: XsStyle.hoverColor//menuItem.highlighted?XsStyle.hoverColor:XsStyle.menuBackground
                antialiasing: true
            }
        }
    }
    //    icon
    //    icon.source:myicon//typeof(menuItem.myicon) === "undefined"?"":menuItem.myicon
    //    Item {
    //        Image {
    //            id: name
    //            source: menuItem.myicon
    //        }
    //    }
    property alias textPart: contentPart.textPart
    contentItem: Rectangle {
        id: contentPart
        color: "transparent"
        anchors.fill: parent
        implicitHeight: XsStyle.menuItemHeight
        implicitWidth: textPart.implicitWidth + shortcutPart.implicitWidth
        Image {
            id: iconPart
            source: myicon
            anchors.right: contentPart.right
//            anchors.left: contentPart.left
//            anchors.leftMargin: checkBoxItem.visible?30:5
            anchors.rightMargin: 5
            anchors.verticalCenter: contentPart.verticalCenter
            visible: myicon !== ""
            width: iconsize
            height: iconsize

            ColorOverlay{
                anchors.fill: iconPart
                source:iconPart
                visible: iconPart.visible
                color: XsStyle.hoverColor
                antialiasing: true
            }
        }
        property alias textPart:textPart
        Text {
            id: textPart
            anchors.fill: parent
            //            anchors.top: parent.top
            //            anchors.bottom: parent.bottom
            //            anchors.right: parent.right
            //            anchors.left: iconPart.visible?iconPart.right:parent.left
            // leftPadding: checkBoxItem.visible ? checkBoxItem.width+checkBoxItem.x+3 : 10//3

            leftPadding: menuItem.menu === null?iconsize:
                menuItem.menu.hasCheckable?menuItem.indicator.width + 6:
                    checkBoxItem.visible ? checkBoxItem.width+checkBoxItem.x+3 : iconPart.visible?iconsize + 10:iconsize

            rightPadding: menuItem.arrow.width
            text: menuItem.text
            //            font: menuItem.font
            font.pixelSize: XsStyle.menuFontSize
            font.family: XsStyle.menuFontFamily
            font.hintingPreference: Font.PreferNoHinting
            opacity: enabled ? 1.0 : 0.5
            color: menuItem.subMenu?menuItem.subMenu.fakeDisabled?"#88ffffff":XsStyle.hoverColor:XsStyle.hoverColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Text {
            id: shortcutPart
            padding: 5
            anchors.right: parent.right
            anchors.rightMargin: menuItem.subMenu?20:5
            anchors.verticalCenter: parent.verticalCenter
            height: 18
            text: menuItem.shortcut?XsUtils.getPlatformShortcut(menuItem.shortcut):""
            //            font: menuItem.font
            //            font.pixelSize:             shortcutPart.text !== "" &&
            //                                        !menuItem.shortcut.includes('Ctrl') &&
            //                                        !menuItem.shortcut.includes('Alt')?9:12
            font.pixelSize: XsStyle.menuFontSize
            font.family: XsStyle.menuFontFamily
            font.hintingPreference: Font.PreferNoHinting

            opacity: enabled ? 1.0 : 0.3
            color: menuItem.highlighted?textPart.color:XsStyle.highlightColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        //        Rectangle {
        //            visible: shortcutPart.text !== "" &&
        //                     !menuItem.shortcut.includes('Ctrl') &&
        //                     !menuItem.shortcut.includes('Alt')
        //            color: "transparent"
        //            anchors.fill: shortcutPart
        //            border {
        //                width: 1
        //                color: shortcutPart.color
        //            }
        //            radius: 5
        //        }
    }
    background: Rectangle {

        anchors {
            margins: 1
            fill: parent
        }

        implicitHeight: XsStyle.menuItemHeight
        opacity: enabled ? 1 : 0.3
        //        color: menuItem.highlighted ?
        //                   XsStyle.highlightColor: "transparent"
        color: menuItem.highlighted?XsStyle.primaryColor:"transparent"
        gradient:menuItem.highlighted?styleGradient.accent_gradient:Gradient.gradient


    }

}

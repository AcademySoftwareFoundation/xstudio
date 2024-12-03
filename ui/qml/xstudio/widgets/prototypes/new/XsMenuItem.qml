// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.12

import xStudio 1.0

MenuItem {
    id: widget

    implicitHeight: visible ? 22 : 0
    font.pixelSize: XsStyleSheet.fontSize
    font.family: XsStyleSheet.fontFamily

    // ListElement{ menuText:"RGB"; isSelected: true; isSelectable:true; isMultiSelectable: true; hasSub:false; key1:""; key2:""}

    property color bgColorPressed: palette.highlight //XsStyle.hoverColor
    property string iconbg: ""
    property int iconsize: 16 //XsStyle.menuItemHeight *.66

    property alias mytext: myAction.text
    property bool isMultiCheckable: false
    property alias isCheckable: myAction.checkable

    property alias isChecked: myAction.checked
    property string shortcut: ""
    property string imgSrc: ""
    property bool hasSubMenu: false

    property var actiongroup

    action: Action {
        id: myAction

        ActionGroup.group: typeof(actiongroup)!= "undefined" ? actiongroup : null

        onTriggered: {
            if(widget.checkable && actiongroup) {
                if (!myAction.checked) {
                    myAction.checked = true
                }
            }
        }

    }

    arrow: Image {
        id:myIcon
        source:"qrc:/icons/chevron_right.svg"
        sourceSize.height: 16//parent.height
        sourceSize.width: 16//parent.width
        x: parent.width - width - 5
        visible: widget.subMenu || widget.hasSubMenu
        anchors.verticalCenter: parent.verticalCenter

        ColorOverlay{
            id: myIconOverlay
            anchors.fill: myIcon
            source: myIcon
            color: textPart.color
            antialiasing: true
        }

    }

    indicator: Item {
        id:checkBoxItem
        // Icon (radio or checkbox)
        implicitWidth: widget.checkable? XsStyle.menuItemHeight : 0
        implicitHeight: XsStyle.menuItemHeight
        visible: widget.checkable
        x: 3

        Rectangle {
            width: iconsize
            height: iconsize
            Gradient {
                id:indGrad
                orientation: Gradient.Horizontal
            }

            color: "transparent" // widget.checked?XsStyle.primaryColor:"transparent"
            // gradient:iconbg===""?widget.checked?styleGradient.accent_gradient:Gradient.gradient:getBGGrad()
            function getBGGrad() {
                preferences.assignColorStringToGradient(iconbg, indGrad)
                return indGrad
            }

            anchors.centerIn: parent
            // border.width: 1 // checkableDecorated?1:0
            // border.color: actiongroup?
            //                   widget.checked?bgColorPressed:widget.highlighted?bgColorPressed:XsStyle.mainColor
            //                     :widget.highlighted?bgColorPressed:XsStyle.mainColor
            // radius: actiongroup?iconsize / 2:0

            Image {
                id: checkIcon
                // visible: widget.isCheckable
                source: widget.isMultiCheckable? widget.checked? "qrc:/icons/check_box_checked.svg" : "qrc:/icons/check_box_unchecked.svg"
                : widget.checked? "qrc:/icons/radio_button_checked.svg" : "qrc:/icons/radio_button_unchecked.svg"
                width: iconsize // 2
                height: iconsize // 2
                anchors.centerIn: parent

                layer {
                    enabled: true
                    effect: ColorOverlay { color: widget.checked? widget.highlighted?"#F1F1F1":bgColorPressed:"#C1C1C1" }
                }
            }

        }
    }

    //    icon
    //    icon.source:imgSrc//typeof(widget.imgSrc) === "undefined"?"":widget.imgSrc
    //    Item {
    //        Image {
    //            id: name
    //            source: widget.imgSrc
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
            source: imgSrc
            anchors.right: contentPart.right
            anchors.rightMargin: 5
            anchors.verticalCenter: contentPart.verticalCenter
            visible: imgSrc !== ""
            width: iconsize
            height: iconsize

            ColorOverlay{
                anchors.fill: iconPart
                source:iconPart
                visible: iconPart.visible
                color: bgColorPressed
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
            leftPadding: widget.menu === null?iconsize:
                                                 widget.menu.hasCheckable?widget.indicator.width + 6:
                                                                             iconPart.visible?iconsize + 10:iconsize
            rightPadding: widget.arrow.width
            text: widget.mytext //?XsUtils.replaceMenuText(widget.mytext):widget.text
            //            font: widget.font
            font.pixelSize: XsStyle.menuFontSize
            font.family: XsStyle.menuFontFamily
            font.hintingPreference: Font.PreferNoHinting
            opacity: enabled ? 1.0 : 0.5
            color: "#F1F1F1" //widget.subMenu?widget.subMenu.fakeDisabled?"#88ffffff":XsStyle.hoverColor:XsStyle.hoverColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Text {
            id: shortcutPart
            padding: 5
            anchors.right: parent.right
            anchors.rightMargin: widget.subMenu?20:5
            anchors.verticalCenter: parent.verticalCenter
            height: 18
            text: widget.shortcut?widget.shortcut:"" //XsUtils.getPlatformShortcut(widget.shortcut):""
            font.pixelSize: XsStyle.menuFontSize
            font.family: XsStyle.menuFontFamily
            font.hintingPreference: Font.PreferNoHinting

            opacity: enabled ? 1.0 : 0.3
            color: widget.highlighted?textPart.color:XsStyleSheet.accentColor
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        //        Rectangle {
        //            visible: shortcutPart.text !== "" &&
        //                     !widget.shortcut.includes('Ctrl') &&
        //                     !widget.shortcut.includes('Alt')
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
        //        color: widget.highlighted ?
        //                   XsStyle.highlightColor: "transparent"
        // color: widget.highlighted? palette.highlight : "transparent"
        // gradient:widget.highlighted? styleGradient.accent_gradient : Gradient.gradient

        gradient: Gradient {
            GradientStop { position: 0.0; color: widget.highlighted? bgColorPressed: "transparent" } //XsStyleSheet.controlColour }
            GradientStop { position: 1.0; color: widget.highlighted? bgColorPressed: "transparent" } //"#11FFFFFF" }
        }

    }

}

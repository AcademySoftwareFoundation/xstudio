// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.15 //for ColorOverlay

import xStudio 1.0

Item{
    id: widget

    property color bgColorPressed: palette.highlight //"#D17000"
    property color bgColorNormal: "#1AFFFFFF"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    property color textColorPressed: "#F1F1F1"
    property color textColorNormal: "light grey"
    property var tooltip: ""
    property var tooltipTitle: ""
    property alias bgDiv: bgDiv
    property var textElide: textDiv.elide
    property alias textDiv: textDiv
    property alias buttonWidget: buttonWidget

    property string text: ""
    property string menuValue: ""//menuOptions.menuAt(menuOptions.currentIndex)
    property bool isActive: false
    property bool subtleActive: false
    property bool isMultiSelectable: false
    property bool isSelectable: false

    property var menuModel: ""

    Button {
        id: buttonWidget

        font.pixelSize: 12 //XsStyle.menuFontSize
        // font.family: XsStyle.fontFamily

        anchors.fill: parent
        hoverEnabled: true

        contentItem:
        Item{
            anchors.fill: parent
            opacity: enabled ? 1.0:0.33

            Text {
                id: textDiv
                text: widget.text+" <b>"+widget.menuValue+"</b>"
                font: buttonWidget.font
                color: textColorPressed
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                topPadding: 4
                bottomPadding: 4
                leftPadding: 20
                rightPadding: 20
                anchors.horizontalCenter: parent.horizontalCenter
                elide: Text.ElideRight
                width: parent.width
                height: parent.height
            }
        }


        ToolTip.text: buttonWidget.text
        ToolTip.visible: buttonWidget.hovered && textDiv.truncated


        background:
        Rectangle {
            id: bgDiv
            implicitWidth: 100
            implicitHeight: 40
            border.color: buttonWidget.down || buttonWidget.hovered ? bgColorPressed: borderColorNormal
            border.width: borderWidth

            gradient: Gradient {
                GradientStop { position: 0.0; color: buttonWidget.down || (isActive && !subtleActive)? bgColorPressed: XsStyleSheet.controlColour }
                GradientStop { position: 1.0; color: buttonWidget.down || (isActive && !subtleActive)? bgColorPressed: forcedBgColorNormal }
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

        /*onPressed: focus = true
        onReleased: focus = false*/
        onClicked: menuOptions.toggleShow()

    }

    XsViewerMenu { id: menuOptions
        width: parent.width*2
        x: buttonWidget.x
        y: buttonWidget.y + buttonWidget.height

        forcedBgColorNormal: widget.forcedBgColorNormal

        ActionGroup{id: actionGroup}

        Instantiator { id: dynamicMenu
            active: true
            model: menuModel

            delegate: XsMenuItem {
                mytext: menuText
                actiongroup: isSelectable && !isMultiSelectable? actionGroup : null
                hasSubMenu: hasSub
                shortcut: key2===""? key1 : key1+"+"+key2
                // isMultiCheckable: isMultiSelectable
                isCheckable: isSelectable
                isChecked: isSelected

                onTriggered: {
                    menuValue= menuText //dynamicMenu.objectAt(index).
                }
            }

            onObjectAdded: menuOptions.insertItem(index, object)
            onObjectRemoved: menuOptions.removeItem(index, object)
        }

        // XsViewerMenuItem {
        //     bgColorPressed: palette.highlight
        //     mytext: "Default_0"; onTriggered: menuValue= mytext
        //     shortcut: "Key"
        // }

    }

}

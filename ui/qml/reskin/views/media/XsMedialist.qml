// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0

Rectangle{

    id: panel

    // width: parent.width
    // height: parent.height
    color: XsStyleSheet.panelBgColor
    anchors.fill: parent


    property color bgColorPressed: palette.highlight 
    property color bgColorNormal: "transparent"
    property color forcedBgColorNormal: bgColorNormal
    property color borderColorHovered: bgColorPressed
    property color borderColorNormal: "transparent"
    property real borderWidth: 1
    property bool isSubDivider: false

    property real textSize: XsStyleSheet.fontSize
    property var textFont: XsStyleSheet.fontFamily
    property color textColorNormal: palette.text
    property color hintColor: XsStyleSheet.hintColor
    // property var textElide: textDiv.elide
    // property alias textDiv: textDiv

    Rectangle{
        id: titleBar
        color: XsStyleSheet.panelTitleBarColor
        width: parent.width
        height: XsStyleSheet.widgetStdHeight

        /*XsText{
            text: "Files"
            anchors.left: parent.left
            anchors.leftMargin: 4
            horizontalAlignment: Text.AlignLeft
        }*/
        
        XsSecondaryButton{ id: infoBtn
            width: 16
            height: 16
            imgSrc: "qrc:/assets/icons/new/error.svg"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
        }

        XsSecondaryButton{
            width: 16
            height: 16
            imgSrc: "qrc:/assets/icons/new/filter_none.svg"
            anchors.right: infoBtn.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
        }

    }
    
    ListModel{
        id: dataModel
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
        ListElement{type: "divider"; title: "Library"}
        ListElement{type: "content"; title: ""}
        ListElement{type: "content"; title: ""}
    }

    ListView {
        id: playlist

        y: titleBar.height
        spacing: 0
        width: contentWidth
        height: contentHeight
        contentHeight: contentItem.childrenRect.height
        contentWidth: contentItem.childrenRect.width
        orientation: ListView.Vertical
        snapMode: ListView.SnapToItem

        model:  dataModel
        // delegate: Rectangle{
        //     width: panel.width
        //     height: XsStyleSheet.widgetStdHeight
        //     color: XsStyleSheet.accentColor
        //     opacity: index%2==0?0.2:0.7
        // }

        delegate: Item{
            width: panel.width
            height: XsStyleSheet.widgetStdHeight

            Button {
                id: dividerDiv
                text: title
                width: parent.width; height: parent.height
                visible: type=="divider"
            
                font.pixelSize: textSize
                font.family: textFont
                hoverEnabled: true
                opacity: enabled ? 1.0 : 0.33
            
                contentItem:
                Item{
                    anchors.fill: parent

                    Rectangle{ id: leftLine; height: 1; color: "#474747"; anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left; anchors.leftMargin: isSubDivider? 8:4 //XsStyleSheet.menuPadding
                        anchors.right: textDiv.left; anchors.rightMargin: 8
                    }
                    Rectangle{ id: rightLine; height: 1; color: "#474747"; anchors.verticalCenter: parent.verticalCenter
                        anchors.left: textDiv.right; anchors.leftMargin: 8
                        anchors.right: parent.right; anchors.rightMargin: 4
                    }
                    Text {
                        id: textDiv
                        text: dividerDiv.text+"-"+index
                        font: dividerDiv.font
                        color: hintColor 
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        topPadding: 2
                        bottomPadding: 2
                        
                        anchors.horizontalCenter: parent.horizontalCenter
                        elide: Text.ElideRight
                        // width: parent.width
                        height: parent.height
            
                        // XsToolTip{
                        //     text: dividerDiv.text
                        //     visible: dividerDiv.hovered && parent.truncated
                        //     width: metricsDiv.width == 0? 0 : dividerDiv.width
                        // }
                    }
                }
            
                background:
                Rectangle {
                    id: bgDiv
                    implicitWidth: 100
                    implicitHeight: 40
                    border.color: dividerDiv.down || dividerDiv.hovered ? borderColorHovered: borderColorNormal
                    border.width: borderWidth
                    color: dividerDiv.down? bgColorPressed : forcedBgColorNormal
                    
                    Rectangle {
                        id: bgFocusDiv
                        implicitWidth: parent.width+borderWidth
                        implicitHeight: parent.height+borderWidth
                        visible: dividerDiv.activeFocus
                        color: "transparent"
                        opacity: 0.33
                        border.color: borderColorHovered
                        border.width: borderWidth
                        anchors.centerIn: parent
                    }
                }
            
                onPressed: focus = true
                onReleased: focus = false
            
            }
            Button {
                id: contentDiv
                text: title
                width: parent.width; height: parent.height
                visible: type=="content"
            
                font.pixelSize: textSize
                font.family: textFont
                hoverEnabled: true
                opacity: enabled ? 1.0 : 0.33
            
                contentItem:
                Item{
                    anchors.fill: parent

                   
                    Text {
                        id: textDiv2
                        text: contentDiv.text+"-"+index
                        font: contentDiv.font
                        color: textColorNormal 
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        topPadding: 2
                        bottomPadding: 2
                        leftPadding: 8
                        
                        anchors.horizontalCenter: parent.horizontalCenter
                        elide: Text.ElideRight
                        // width: parent.width
                        height: parent.height
            
                        // XsToolTip{
                        //     text: contentDiv.text
                        //     visible: contentDiv.hovered && parent.truncated
                        //     width: metricsDiv.width == 0? 0 : contentDiv.width
                        // }
                    }
                }
            
                background:
                Rectangle {
                    id: bgDiv2
                    implicitWidth: 100
                    implicitHeight: 40
                    border.color: contentDiv.down || contentDiv.hovered ? borderColorHovered: borderColorNormal
                    border.width: borderWidth
                    color: contentDiv.down? bgColorPressed : forcedBgColorNormal
                    
                    Rectangle {
                        id: bgFocusDiv2
                        implicitWidth: parent.width+borderWidth
                        implicitHeight: parent.height+borderWidth
                        visible: contentDiv.activeFocus
                        color: "transparent"
                        opacity: 0.33
                        border.color: borderColorHovered
                        border.width: borderWidth
                        anchors.centerIn: parent
                    }
                }
            
                onPressed: focus = true
                onReleased: focus = false
            
            }
        }

       

        // model: DelegateModel {           
        //     model: dataModel
        //     rootIndex: 0
        
        //     delegate: DelegateChooser {
        //         role: "menu_item_type"
    
        //         DelegateChoice {
        //             roleValue: "medialist"

        //             // XsMenuItemNew { 
        //             //     // again, we pass in the model to the menu item and
        //             //     // step one level deeper into the tree by useing row=index
        //             //     menu_model: the_popup.menu_model
        //             //     menu_model_index: the_popup.menu_model.index(
        //             //         index, // row = child index
        //             //         0, // column = 0 (always, we don't use columns)
        //             //         the_popup.menu_model_index // the parent index into the model
        //             //     )
        //             // }
        //         }

        //     }
        // }

    }


}
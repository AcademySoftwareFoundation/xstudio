// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0

Item{

    id: panel

    anchors.fill: parent
    // height: parent.height


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

    Item{id: actionDiv
        width: parent.width; 
        height: 28+(8*2)

        // Rectangle{anchors.fill: parent; color: "red"}
        
        XsPrimaryButton{ id: addPlaylistBtn
            width: 40
            height: 28
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            imgSrc: "qrc:/assets/icons/new/add.svg"
        }
        XsSearchBar{
            width: parent.width - morePlaylistBtn.width - addPlaylistBtn.width - (8*4)
            height: 28
            anchors.left: addPlaylistBtn.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: activeFocus?"":"Search playlists..."
        }
        XsPrimaryButton{ id: morePlaylistBtn
            width: 40
            height: 28
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            imgSrc: "qrc:/assets/icons/new/more_vert_500.svg"
        }

        // XsPlaylistPanel{
        //     x: 8
        //     y: actionDiv.height+4
        //     width: widget.width-(8*2)
        //     height: widget.height-y-(4*2)
        // }
    }
    
    Rectangle{

        x: 8
        y: actionDiv.height+4
        width: panel.width-(8*2)
        height: panel.height-y-(4*2)

        color: XsStyleSheet.panelBgColor

    
        Rectangle{
            id: titleBar
            color: XsStyleSheet.panelTitleBarColor
            width: parent.width
            height: XsStyleSheet.widgetStdHeight
    
            Text{
                text: "Playlist"
                anchors.left: parent.left
                anchors.leftMargin: 4
                horizontalAlignment: Text.AlignLeft
            }
            
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
            ListElement{type: "divider"; title: "Playlist"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "divider"; title: "Playlist"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "divider"; title: "Playlist"}
            ListElement{type: "content"; title: "Item"}
            ListElement{type: "divider"; title: "Playlist"}
            ListElement{type: "content"; title: "Item"}
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
    
                XsPlaylistDividerDelegate{
                    visible: type=="divider"
                }
                XsPlaylistItemDelegate{
                    visible: type=="content"
                }
                
            }
    
           
    
    
        }

    }
    
}
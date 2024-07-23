// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudioReskin 1.0

Item{

    id: panel

    anchors.fill: parent

    property color panelColor: XsStyleSheet.panelBgColor
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

    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real secBtnWidth: XsStyleSheet.secondaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    // background
    Rectangle{
        z: -1000
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#5C5C5C" }
            GradientStop { position: 1.0; color: "#474747" }
        }
    }
    
    Item{id: actionDiv
        width: parent.width; 
        height: btnHeight+(panelPadding*2)
        
        RowLayout{
            x: panelPadding
            spacing: 1
            width: parent.width-(x*2)
            height: btnHeight
            anchors.verticalCenter: parent.verticalCenter

            XsPrimaryButton{ id: addPlaylistBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/add.svg"
            }
            XsPrimaryButton{ id: deleteBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/delete.svg"
            }
            XsSearchButton{ id: searchBtn
                Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
                Layout.preferredHeight: parent.height
                isExpanded: false
                hint: "Search playlists..."
            }
            Item{
                Layout.fillWidth: true
                Layout.preferredHeight: parent.height
            }
            XsPrimaryButton{ id: morePlaylistBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/icons/more_vert.svg"
            }
        }
        
    }
    
    Rectangle{ id: playlistDiv
        x: panelPadding
        y: actionDiv.height
        width: panel.width-(x*2)
        height: panel.height-y-panelPadding
        color: panelColor
        
        Rectangle{ id: titleBar
            color: XsStyleSheet.panelTitleBarColor
            width: parent.width
            height: XsStyleSheet.widgetStdHeight
    
            XsText{
                text: "Playlist ("+playlistItems.count+")"
                anchors.left: parent.left
                anchors.leftMargin: panelPadding
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignLeft
            }
    
            XsSecondaryButton{
                width: secBtnWidth
                height: secBtnWidth
                imgSrc: "qrc:/icons/filter_none.svg"
                anchors.right: errorBtn.left
                anchors.rightMargin: panelPadding
                anchors.verticalCenter: parent.verticalCenter
            }

            XsSecondaryButton{ id: errorBtn
                width: secBtnWidth
                height: secBtnWidth
                imgSrc: "qrc:/icons/error.svg"
                anchors.right: parent.right
                anchors.rightMargin: panelPadding + panelPadding/2
                anchors.verticalCenter: parent.verticalCenter
            }
    
        }
            
        XsPlaylistItems{

            id: playlistItems
            y: titleBar.height
            itemRowWidth: playlistDiv.width

        }


    }
    
}
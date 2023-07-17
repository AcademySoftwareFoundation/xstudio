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

        RowLayout{
            x: 8
            spacing: 1
            width: parent.width-(x*2)
            height: XsStyleSheet.widgetStdHeight+4
            anchors.verticalCenter: parent.verticalCenter
            
            XsPrimaryButton{ id: addBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                Layout.alignment: Qt.AlignLeft
                imgSrc: "qrc:/assets/icons/new/add.svg"
            }
            XsPrimaryButton{ id: searchBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/assets/icons/new/search_w500.svg"
            }
            XsPrimaryButton{ id: deleteBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/assets/icons/new/delete_w500.svg"
            }
            Text{
                Layout.fillWidth: true
                Layout.preferredHeight: parent.height
                text: "Media Library"
                color: textColorNormal
                // Layout.alignment: Qt.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
            XsPrimaryButton{ id: listViewBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/assets/icons/new/view_w500.svg"
            }
            XsPrimaryButton{ id: gridViewBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                imgSrc: "qrc:/assets/icons/new/view_grid_w500.svg"
            }
            XsPrimaryButton{ id: moreBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                Layout.alignment: Qt.AlignRight
                imgSrc: "qrc:/assets/icons/new/more_vert_500.svg"
            }

        }
        
    }
    
    Rectangle{ id: mediaListDiv
        x: 8
        y: actionDiv.height+4
        width: panel.width-(8*2)
        height: panel.height-y-(4*2)
        color: XsStyleSheet.panelBgColor
    
        Rectangle{ id: titleBar
            color: XsStyleSheet.panelTitleBarColor
            width: parent.width
            height: XsStyleSheet.widgetStdHeight
    
            Text{
            text: "Files"
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignLeft
                color: textColorNormal
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
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:""}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:""}
            ListElement{_type: "content"; _title: "Library"; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Library"; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Library"; _thumbnail:""}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:""}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:""}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Library"; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:""}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:""}
            ListElement{_type: "content"; _title: "Media"  ; _thumbnail:"qrc:/assets/images/sample.png"}
            ListElement{_type: "content"; _title: "Library"; _thumbnail:"qrc:/assets/images/sample.png"}
        }
    
        ListView { id: mediaList
    
            y: titleBar.height
            clip: true
            spacing: 0
            width: contentWidth
            height: contentHeight<parent.height-y? contentHeight : parent.height-y
            contentHeight: contentItem.childrenRect.height
            contentWidth: contentItem.childrenRect.width
            orientation: ListView.Vertical
            snapMode: ListView.SnapToItem
    
            model: chooserModel //dataModel

            // delegate: Item{
            //     width: mediaListDiv.width
            //     height: XsStyleSheet.widgetStdHeight

            //     XsMediaItemDelegate{
            //     }
            // }
            
        }

        DelegateModel { 
            id: chooserModel    
            rootIndex: 0

            model: dataModel
            delegate: chooser 
        }

        DelegateChooser {
            id: chooser
            role: "_type"

            DelegateChoice {
                roleValue: "content";

                XsMediaItemDelegate{ 
                    width: mediaListDiv.width
                    height: XsStyleSheet.widgetStdHeight
                } 

            }
        }

    }
    
}
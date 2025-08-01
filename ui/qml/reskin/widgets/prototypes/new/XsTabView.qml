// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

import xStudioReskin 1.0

TabView{
    id: widget

    property var defaultTab: defaultTab
    signal menuClicked()

    XsTab{
        id: defaultTab
        title: "Tab-0"
    }

    style: TabViewStyle{
        //control: TabView
        //frame: Component
        //frameOverlap: int
        //leftCorner: Component
        //rightCorner: Component
        //tab: Component
        //tabBar: Component
        //tabOverlap: int
        //tabsAlignment: int
        //tabsMovable: bool

        tabBar:Rectangle{
            color: XsStyleSheet.panelBgColor

            XsSecondaryButton{ id: menuBtn
                width: 16 
                height: 16
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                imgSrc: "qrc:/assets/icons/new/menu.svg"
                smooth: true
                antialiasing: true
        
                onClicked: menuClicked()
            }
        }
        tab: Rectangle{
            color: styleData.selected? "#5C5C5C":"#474747" //#TODO: to check with UX
            implicitWidth: 100//Math.max(textDiv.width + 2, 80)
            implicitHeight: 28
            Text{ id: textDiv
                text: styleData.title
                anchors.centerIn: parent
                color: palette.text
                font.bold: styleData.selected
            }
            Rectangle{id: topline
                color: XsStyleSheet.panelTabColor
                width: parent.width
                height: 1
            }
            Rectangle{id: rightline
                color: XsStyleSheet.panelTabColor
                width: 1
                height: parent.height
            }
            XsSecondaryButton{ id: expandBtn
                width: 16 
                height: 16
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                imgSrc: "qrc:/assets/icons/new/chevron_right.svg"
                rotation: 90
                smooth: true
                antialiasing: true

                // onClicked: 
            }

            XsSecondaryButton{ id: addBtn
                // visible: false
                width: 16 
                height: 16
                anchors.left: parent.right
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                imgSrc: "qrc:/assets/icons/new/add.svg"
                smooth: true
                antialiasing: true
        
                onPressed: parent.color = palette.highlight
                onReleased: parent.color = styleData.selected? "#5C5C5C":"#474747" 
            }
            
        }
        frame: Rectangle{
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#5C5C5C" }
                GradientStop { position: 1.0; color: "#474747" }
            }
            Item{id: actionDiv
                width: parent.width; height: 28+(8*2)
                visible: widget.currentIndex==0 //#TODO

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
            }
        }
    }

    
    

}
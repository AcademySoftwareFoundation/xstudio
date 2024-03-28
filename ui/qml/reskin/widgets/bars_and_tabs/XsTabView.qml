// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0

TabView{

    id: widget

    currentIndex: 0
    onCurrentIndexChanged:{
        currentTab = widget.getTab(widget.currentIndex)
    }

    property string panelId: ""
    property var currentTab: defaultTab
    property real buttonSize: XsStyleSheet.menuIndicatorSize
    property real panelPadding: XsStyleSheet.panelPadding
    property real menuWidth: 160//panelMenu.menuWidth
    property real tabWidth: 95
    property bool tab_bg_visible: true

    function addNewTab(title){
        addTab(title, emptyComp)
        widget.currentIndex = widget.count-1
    }

    style: TabViewStyle{
        tabsMovable: true

        tabBar: Rectangle{
            color: XsStyleSheet.panelBgColor

            // XsSecondaryButton{ id: addBtn0
            //     // visible: false
            //     width: buttonSize
            //     height: width
            //     anchors.right: menuBtn.left
            //     anchors.rightMargin: 8/2
            //     anchors.verticalCenter: menuBtn.verticalCenter
            //     imgSrc: "qrc:/icons/add.svg"
            //     smooth: true
            //     antialiasing: true
        
            //     onClicked: {
            //         addTab("New", emptyComp)
            //         widget.currentIndex = widget.count-1
            //         // currentTab = widget.getTab(widget.currentIndex)
            //     }
            //     Component{ id: emptyComp0
            //         Rectangle {
            //             anchors.fill: parent
            //             color: "#5C5C5C" 
            //             Text {
            //                 anchors.centerIn: parent
            //                 text: "Empty"
            //             }
            //         }
            //     }
            // }

            // For adding a new tab
            XsSecondaryButton{ 
                
                id: addBtn
                // visible: false
                width: buttonSize
                height: buttonSize
                z: 1
                x: tabWidth*count + panelPadding/2
                anchors.verticalCenter: menuBtn.verticalCenter
                imgSrc: "qrc:/icons/add.svg"
                
                // Rectangle{anchors.fill: parent; color: "red"; opacity:.3}
        
                onClicked: {
                    tabTypeMenu.x = x
                    tabTypeMenu.y = y+height
                    tabTypeMenu.visible = !tabTypeMenu.visible
                }
                
            }

            XsMenuNew { 
                id: tabTypeMenu
                visible: false
                menuWidth: 80
                menu_model: tabTypeModel
                menu_model_index: tabTypeModel.index(-1, -1)
            }
            XsMenusModel {
                id: tabTypeModel
                modelDataName: "TabMenu"+panelId
                onJsonChanged: {
                    tabTypeMenu.menu_model_index = index(-1, -1)
                }
            }

            XsSecondaryButton{ id: menuBtn
                width: buttonSize 
                height: buttonSize
                anchors.right: parent.right
                anchors.rightMargin: panelPadding
                anchors.verticalCenter: parent.verticalCenter
                imgSrc: "qrc:/icons/menu.svg"
                isActive: panelMenu.visible
                onClicked: {
                    panelMenu.x = menuBtn.x-panelMenu.width
                    panelMenu.y = menuBtn.y //+ menuBtn.height
                    panelMenu.visible = !panelMenu.visible
                }
            }
            XsMenuNew { 
                id: panelMenu
                visible: false
                menu_model: panelMenuModel
                menu_model_index: panelMenuModel.index(-1, -1)
                menuWidth: widget.menuWidth
            }
            XsMenusModel {
                id: panelMenuModel
                modelDataName: "PanelMenu"+panelId
                onJsonChanged: {
                    panelMenu.menu_model_index = index(-1, -1)
                }
            }
        }

        tab: Rectangle{ id: tabDiv
            color: styleData.selected? "#5C5C5C":"#474747" //#TODO: to check with UX
            implicitWidth: tabWidth //metrics.width + typeSelectorBtn.width + panelPadding*2 //Math.max(metrics.width + 2, 80)
            implicitHeight: XsStyleSheet.widgetStdHeight

            
            // Rectangle{id: topline
            //     color: XsStyleSheet.panelTabColor
            //     width: parent.width
            //     height: 1
            // }
            // Rectangle{id: rightline
            //     color: XsStyleSheet.panelTabColor
            //     width: 1
            //     height: parent.height
            // }
            Item{
                anchors.centerIn: parent
                width: textDiv.width + typeSelectorBtn.width

                Text{ id: textDiv
                    text: styleData.title
                    // width: metrics.width
                    // width: parent.width - typeSelectorBtn.width-typeSelectorBtn.anchors.rightMargin
                    // anchors.left: parent.left
                    // anchors.leftMargin: panelPadding
                    // anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    color: palette.text
                    font.bold: styleData.selected
                    elide: Text.ElideRight

                    TextMetrics {
                        id: metrics
                        text: textDiv.text
                        font: textDiv.font
                    }

                    // XsToolTip{
                    //     text: textDiv.text
                    //     // visible: tabMArea.hovered && parent.truncated
                    //     width: metrics.width == 0? 0 : textDiv.width
                    //     // x: 0 //#TODO: flex/pointer
                    // }
                }
                XsSecondaryButton{ id: typeSelectorBtn
                    width: buttonSize
                    height: width
                    anchors.left: textDiv.right
                    anchors.leftMargin: 1
                    // anchors.right: parent.right
                    // anchors.rightMargin: panelPadding
                    anchors.verticalCenter: parent.verticalCenter
                    imgSrc: "qrc:/icons/chevron_right.svg"
                    rotation: 90
                    smooth: true
                    antialiasing: true
                    isActive: typeMenu.visible

                    onClicked: {
                        typeMenu.x = typeSelectorBtn.x
                        typeMenu.y = typeSelectorBtn.y+typeSelectorBtn.height
                        typeMenu.visible = !typeMenu.visible
                    }
                }
            }

            XsMenuNew { 
                id: typeMenu
                visible: false
                menuWidth: 80
                menu_model: typeModel
                menu_model_index: typeModel.index(-1, -1)
            }

            XsMenusModel {
                id: typeModel
                modelDataName: "TabMenu"+panelId
                onJsonChanged: {
                    typeMenu.menu_model_index = index(-1, -1)
                }
            }
            
            XsMenuModelItem {
                text: ""
                menuPath: ""
                // menuItemPosition: 1
                menuItemType: "divider"
                menuModelName: "TabMenu"+panelId
                onActivated: {
                }
            }
            XsMenuModelItem {
                text: "Close Tab"
                menuPath: ""
                // menuItemPosition: 1
                menuItemType: "button"
                menuModelName: "TabMenu"+panelId
                onActivated: {
                    removeTab(getTab(index)) //#TODO: WIP
                }
            }
        }

        frame: Rectangle{
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#5C5C5C" }
                GradientStop { position: 1.0; color: "#474747" }
            }
            visible: tab_bg_visible          
        }

    }

}
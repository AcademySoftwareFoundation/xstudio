// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0

TabView {

    id: container
    anchors.fill: parent
    property string panelId: "" + container

    // This object instance has been build via a model.
    // These properties point us into that node of the model that created us.
    property var panels_layout_model
    property var panels_layout_model_index
    property var panels_layout_model_row

    property int modified_tab_index: -1

    // update the currentIndex from the model
    currentIndex: current_tab === undefined ? 0 : current_tab

    // when user changes tab, store in the model
    onCurrentIndexChanged: {
        if (currentIndex != current_tab) {
            current_tab = currentIndex
        }
    }

    // Here we make the tabs by iterating over the panels_layout_model at
    // the current 'panels_layout_model_index'. The current index is where
    // we which is the level at which the panel isn't split, in other words
    // where we need to insert a 'view'. However, we can have multiple 'views'
    // within a panel thanks to the tabbing feature. The info on the tabs
    // is within the child nodes of this node in the panels_layout_model.
    Repeater {
        model: DelegateModel {
            id: tabs
            model: panels_layout_model
            rootIndex: panels_layout_model_index
            delegate: XsTab {
                id: defaultTab
                title: tab_view ? tab_view : ""
            }    
        }        
    }


    XsModelProperty {
        id: current_view
        role: "tab_view"
        index: panels_layout_model_index
    }

    property real buttonSize: XsStyleSheet.menuIndicatorSize
    property real panelPadding: XsStyleSheet.panelPadding
    property real menuWidth: 170//panelMenu.menuWidth
    property real tabWidth: 95

    /***********************************************************************
     * 
     * Create the menu that appears when you click on the + tab button or on
     * the chevron dropdown button on a particular tab
     *  
     */

    // This instance of the 'XsViewsModel' type gives us access to the global
    // model that contains details of all 'views' that are available
    XsViewsModel {
        id: views_model
    }

    // Declare a unique menu model for this instance of the XsViewContainer
    XsMenusModel {
        id: tabTypeModel
        modelDataName: "TabMenu"+panelId
        onJsonChanged: {
            tabTypeMenu.menu_model_index = index(-1, -1)
        }
    }

    // Build a menu from the model immediately above
    XsMenuNew { 
        id: tabTypeMenu
        visible: false
        menuWidth: 80
        menu_model: tabTypeModel
        menu_model_index: tabTypeModel.index(-1, -1)
    }

    // Add menu items for each view registered with the global model
    // of views
    Repeater {
        model: views_model
        Item {
            XsMenuModelItem {
                text: view_name
                menuPath: "" //"Set Panel To|"
                menuModelName: "TabMenu"+panelId
                menuItemPosition: index
                onActivated: {
                    if (modified_tab_index >= 0)
                        switch_tab_to(view_name)
                    else if (modified_tab_index == -10)
                        add_tab(view_name)
                }
            }
        }    
    }

    // Add a divider
    XsMenuModelItem {
        text: ""
        menuPath: ""
        menuItemPosition: 99
        menuItemType: "divider"
        menuModelName: "TabMenu"+panelId
    }

    // Add a 'Close Tab' menu item
    XsMenuModelItem {
        text: "Close Tab"
        menuPath: ""
        menuItemPosition: 100
        menuItemType: "button"
        menuModelName: "TabMenu"+panelId
        onActivated: {
            remove_tab(modified_tab_index) //#TODO: WIP
        }
    }

    /************************************************************************/

    style: TabViewStyle{

        tabsMovable: true

        tabBar: Rectangle{

            color: XsStyleSheet.panelBgColor

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
                        
                onClicked: {
                    modified_tab_index = -10
                    tabTypeMenu.x = x
                    tabTypeMenu.y = y+height
                    tabTypeMenu.visible = !tabTypeMenu.visible
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

        }

        tab: Rectangle{ id: tabDiv
            color: styleData.selected? "#5C5C5C":"#474747" //#TODO: to check with UX
            implicitWidth: tabWidth //metrics.width + typeSelectorBtn.width + panelPadding*2 //Math.max(metrics.width + 2, 80)
            implicitHeight: XsStyleSheet.widgetStdHeight

            
            Item{
                anchors.centerIn: parent
                width: textDiv.width + typeSelectorBtn.width

                Text{ id: textDiv
                    text: styleData.title
                    anchors.verticalCenter: parent.verticalCenter
                    color: palette.text
                    font.bold: styleData.selected
                    elide: Text.ElideRight

                    TextMetrics {
                        id: metrics
                        text: textDiv.text
                        font: textDiv.font
                    }

                }
                XsSecondaryButton{ 

                    id: typeSelectorBtn
                    width: buttonSize
                    height: width
                    anchors.left: textDiv.right
                    anchors.leftMargin: 1
                    anchors.verticalCenter: parent.verticalCenter
                    imgSrc: "qrc:/icons/chevron_right.svg"
                    rotation: 90
                    smooth: true
                    antialiasing: true
                    isActive: showingMenu && tabTypeMenu.visible
                    property bool showingMenu: false

                    onClicked: {
                        // store which tab has been clicked on
                        modified_tab_index = index
                        tabTypeMenu.x = parent.x+typeSelectorBtn.x
                        tabTypeMenu.y = parent.y+typeSelectorBtn.y+typeSelectorBtn.height
                        tabTypeMenu.visible = !tabTypeMenu.visible
                        showingMenu = tabTypeMenu.visible
                    }
                }
            }
            
        }

        frame: Rectangle{
            visible: false          
        }

    }
    
    XsMenuModelItem {
        text: "Close Panel"
        menuPath: ""
        menuItemPosition: 5
        menuModelName: "PanelMenu"+panelId
        onActivated: {
            close_panel()
        }
    }

    XsMenuNew { 
        id: panelMenu
        visible: false
        menu_model: panelMenuModel
        menu_model_index: panelMenuModel.index(-1, -1)
    }

    XsMenusModel {
        id: panelMenuModel
        modelDataName: "PanelMenu"+panelId
        onJsonChanged: {
            panelMenu.menu_model_index = index(-1, -1)
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: "PanelMenu"+panelId
    }
    XsMenuModelItem {
        text: "Split Panel Vertical"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "PanelMenu"+panelId
        onActivated: {
            split_panel(true)
        }
    }
    XsMenuModelItem {
        text: "Split Panel Horizontal"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: "PanelMenu"+panelId
        onActivated: {
            split_panel(false)
        }
    }

    function undock_panel() {
    }

    function split_panel(horizontal) {
        panels_layout_model.split_panel(panels_layout_model_index, horizontal)
    }

    function switch_tab_to(new_tab_view) {
        panels_layout_model.set(panels_layout_model.index(modified_tab_index, 0, panels_layout_model_index), new_tab_view, "tab_view")
    }

    function add_tab(new_tab_view) {
        var r = panels_layout_model.rowCount(panels_layout_model_index)
        panels_layout_model.insertRowsSync(r, 1, panels_layout_model_index)
        panels_layout_model.set(
            panels_layout_model.index(
                r,
                0,
                panels_layout_model_index
                ),
            new_tab_view,
            "tab_view")
    }

    function remove_tab(tab_index) {
        panels_layout_model.removeRows(tab_index, 1, panels_layout_model_index)
    }

    function close_panel() {
        // our parent must be a 'splitter'
        panels_layout_model.close_panel(panels_layout_model_index)
    }


}
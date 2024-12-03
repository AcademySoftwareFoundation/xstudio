import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import Qt.labs.qmlmodels 1.0
import QtQml.Models 2.14

import xStudio 1.0

Item {

    id: container
    anchors.fill: parent
    property string panelId: "" + container

    property var tabCount: tabs_repeater.count

    // This object instance has been build via a model.
    // These properties point us into that node of the model that created us.
    property var panels_layout_model
    property var panels_layout_model_index: panels_layout_model.index(index, 0, parent_index)
    property var parent_index

    property var finkldex: index
    onFinkldexChanged: {
        foo()
    }

    function foo() {
        if (panels_layout_model != undefined && index != undefined && parent_index != undefined) {
            panels_layout_model_index = panels_layout_model.index(index, 0, parent_index)
        }
    }

    property int modified_tab_index: -1

    property int currentIndex: current_tab === undefined ? 0 : current_tab
    // when user changes tab, store in the model
    onCurrentIndexChanged: {
        if (currentIndex != current_tab) {
            current_tab = currentIndex
        }
    }

    DelegateModel {
        id: _tabThings
        model: panels_layout_model
        rootIndex: panels_layout_model_index
        delegate: tabThing
    }

    XsModelProperty {
        id: current_view
        role: "tab_view"
        index: panels_layout_model_index
    }

    property real buttonSize: XsStyleSheet.menuIndicatorSize
    property real panelPadding: XsStyleSheet.panelPadding
    property real tabWidth: 95

    // Thanks to using panelId this is a unique string for each instance of
    // this item type
    property string __menuModelName: "TabMenu"+panelId

    /***********************************************************************
     *
     * Create the menu that appears when you click on the + tab button or on
     * the chevron dropdown button on a particular tab
     *
     */

    // Declare a unique menu model for this instance of the XsViewContainer
    XsMenusModel {
        id: tabTypeModel
        modelDataName: container.__menuModelName
        onJsonChanged: {
            tabTypeMenu.menu_model_index = index(-1, -1)
        }
    }

    // Build a menu from the model immediately above
    XsMenuNew {
        id: tabTypeMenu
        visible: false
        menu_model: tabTypeModel
        menu_model_index: tabTypeModel.index(-1, -1)
    }

    // Add menu items for each view registered with the global model
    // of views
    Repeater {
        model: viewsModel
        property var foo: viewsModel.length
        Item {
            XsMenuModelItem {
                text: menuItemType == "divider" ? "" : view_name
                menuItemType: view_name == "divider" ? "divider" : "button"
                menuPath: "" //"Set Panel To|"
                menuModelName: container.__menuModelName
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
        menuModelName: container.__menuModelName
    }

    // Add a 'Close Tab' menu item
    XsMenuModelItem {
        text: "Close Tab"
        menuPath: ""
        menuItemPosition: 100
        menuItemType: "button"
        menuModelName: container.__menuModelName
        onActivated: {
            remove_tab(modified_tab_index) //#TODO: WIP
        }
    }

    property bool forceHideTabs: false

    property bool tabsVisible: forceHideTabs ? false : tabsVisibility == 2 || (tabsVisibility == 1 && tabCount > 1)
    property var tabsHeight: tabsVisible ? XsStyleSheet.widgetStdHeight : 0
    Behavior on tabsHeight {NumberAnimation{ duration: 150 }}

    /************************************************************************/

    property var totalTabsWidth: 10

    Rectangle {

        color: XsStyleSheet.panelBgColor
        visible: tabsVisible
        id: theTabBar

        width: parent.width
        height: tabsHeight

        RowLayout {

            spacing: 0

            Repeater {
                model: _tabThings
            }

            Item {
                // padding before the plug button
                Layout.preferredWidth: 4
            }

            // For adding a new tab
            XsSecondaryButton{

                id: addBtn
                // visible: false
                Layout.preferredWidth: buttonSize
                Layout.preferredHeight: tabsHeight
                z: 1
                imgSrc: "qrc:/icons/add.svg"

                onClicked: {
                    modified_tab_index = -10
                    tabTypeMenu.x = x
                    tabTypeMenu.y = y+height
                    tabTypeMenu.visible = !tabTypeMenu.visible
                }

            }

        }

        XsSecondaryButton{
            id: menuBtn
            width: buttonSize
            height: tabsHeight
            anchors.right: parent.right
            anchors.rightMargin: panelPadding
            anchors.verticalCenter: parent.verticalCenter
            imgSrc: "qrc:/icons/menu.svg"
            isActive: panelMenu.visible
            onClicked: {
                panelMenu.showMenu(menuBtn, 0, 0)
            }
        }

    }

    // Here we make the tabs by iterating over the panels_layout_model at
    // the current 'panels_layout_model_index'. The current index is where
    // we which is the level at which the panel isn't split, in other words
    // where we need to insert a 'view'. However, we can have multiple 'views'
    // within a panel thanks to the tabbing feature. The info on the tabs
    // is within the child nodes of this node in the panels_layout_model.
    Repeater {
        id: tabs_repeater
        model: _tabs
    }

    DelegateModel {
        id: _tabs
        model: panels_layout_model
        rootIndex: panels_layout_model_index
        delegate: XsLayoutTab {
            anchors.top: theTabBar.bottom
            anchors.bottom: container.bottom
            anchors.left: container.left
            anchors.right: container.right
            id: defaultTab
            title: tab_view ? tab_view : ""
            is_current_tab: index == currentIndex
        }
    }


    Component {

        id: tabThing

        Item {

            id: tabDiv
            Layout.preferredWidth: tabItem.width + panelPadding/2
            Layout.preferredHeight: tabsHeight

            visible: tabsHeight != 0
            property bool selected: currentIndex == index

            Behavior on implicitHeight {NumberAnimation{ duration: 150 }}

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onPressed: {
                    currentIndex = index
                }
            }

            Rectangle
            {   id: tabItem
                color: selected? XsStyleSheet.panelBgGradTopColor : XsStyleSheet.menuBarColor //#TODO: to check with UX
                border.color: mouseArea.containsMouse ? palette.highlight : "transparent"
                border.width: 1
                width: metrics.width + typeSelectorBtn.width + panelPadding*6
                height: parent.height

                XsText {

                    id: textDiv
                    x: panelPadding*2
                    text: model.tab_view != undefined ? model.tab_view : ""
                    anchors.verticalCenter: parent.verticalCenter
                    color: palette.text
                    font.bold: selected
                    elide: Text.ElideRight

                    TextMetrics {
                        id: metrics
                        text: textDiv.text
                    }

                }
                XsSecondaryButton{

                    id: typeSelectorBtn
                    width: buttonSize
                    height: width
                    anchors.right: parent.right
                    anchors.rightMargin: panelPadding
                    anchors.verticalCenter: parent.verticalCenter
                    imgSrc: "qrc:/icons/chevron_right.svg"
                    rotation: 90
                    smooth: true
                    antialiasing: true
                    property bool showingMenu: tabTypeMenu.visible
                    onShowingMenuChanged: {
                        if (!showingMenu) isActive = false
                    }

                    onClicked: {
                        // store which tab has been clicked on
                        modified_tab_index = index
                        tabTypeMenu.showMenu(typeSelectorBtn, width/2, height/2)
                        isActive = true
                    }
                }
            }

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
        text: "Split Panel Vertically"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "PanelMenu"+panelId
        onActivated: {
            split_panel(true)
        }
        menuCustomIcon: "qrc:/icons/splitscreen2.svg"
    }
    XsMenuModelItem {
        text: "Split Panel Horizontally"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: "PanelMenu"+panelId
        onActivated: {
            split_panel(false)
        }
        menuCustomIcon: "qrc:/icons/splitscreen.svg"
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
        currentIndex = r
    }

    function remove_tab(tab_index) {

        var rc = panels_layout_model.rowCount(panels_layout_model_index)
        if (rc == 1) return;
        if (currentIndex >= (rc-1)) {
            currentIndex = rc-2;
        }
        panels_layout_model.removeRowsSync(tab_index, 1, panels_layout_model_index)

    }

    function close_panel() {
        // our parent must be a 'splitter'
        panels_layout_model.close_panel(panels_layout_model_index)
    }


}
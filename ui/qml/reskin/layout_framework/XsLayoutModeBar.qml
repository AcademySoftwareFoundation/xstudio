import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0

Rectangle { id: modeBar

    height: XsStyleSheet.menuHeight
    // color: XsStyleSheet.menuBarColor
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter( XsStyleSheet.menuBarColor, 1.15) }
        GradientStop { position: 1.0; color: Qt.darker( XsStyleSheet.menuBarColor, 1.15) }
    }

    property string barId: ""
    property real panelPadding: XsStyleSheet.panelPadding
    property real buttonHeight: XsStyleSheet.widgetStdHeight-4

    // Rectangle{anchors.fill: parent; color: "red"; opacity:.3}

    property var selected_layout_index

    XsSecondaryButton{ id: menuBtn
        width: XsStyleSheet.menuIndicatorSize
        height: XsStyleSheet.menuIndicatorSize
        anchors.right: parent.right
        anchors.rightMargin: panelPadding
        anchors.verticalCenter: parent.verticalCenter
        imgSrc: "qrc:/icons/menu.svg"
        isActive: barMenu.visible
        onClicked: {
            barMenu.x = menuBtn.x-barMenu.width
            barMenu.y = menuBtn.y //+ menuBtn.height
            barMenu.visible = !barMenu.visible
        }
    }
    XsMenuNew {
        id: barMenu
        visible: false
        menu_model: barMenuModel
        menu_model_index: barMenuModel.index(-1, -1)
        menuWidth: 160
    }
    XsMenusModel {
        id: barMenuModel
        modelDataName: "ModeBarMenu"+barId
        onJsonChanged: {
            barMenu.menu_model_index = index(-1, -1)
        }
    }


    XsMenuModelItem {
        text: "Remove Current Layout"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
        }
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: "ModeBarMenu"+barId
    }
    XsMenuModelItem {
        text: "Reset Default Layouts"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
        }
    }
    XsMenuModelItem {
        text: "Reset Current Layout"
        menuPath: ""
        menuItemPosition: 3
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
        }
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: "ModeBarMenu"+barId
    }
    XsMenuModelItem {
        text: "Save As New..."
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
        }
    }
    XsMenuModelItem {
        text: "Duplicate..."
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
            layouts_model.duplicate_layout(current_layout_index)
        }
    }
    XsMenuModelItem {
        text: "Rename..."
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "ModeBarMenu"+barId
        property var idx: 1
        onActivated: {
            // TODO: pop-up string query dialog to get new name
            layouts_model.set(current_layout_index, "Renamed "+ idx, "layout_name")
            idx = idx+1
        }
    }
    XsMenuModelItem {
        text: "New Layout..."
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
            var rc = layouts_model.rowCount(layouts_model_root_index)
            layouts_model.insertRowsSync(rc, 1, layouts_model_root_index)
            layouts_model.set(layouts_model.index(rc, 0, layouts_model_root_index),
            '{
                "children": [
                  {
                    "child_dividers": [],
                    "children": [
                      {
                        "children": [
                          {
                            "tab_view": "Playlists"
                          }
                        ],
                        "current_tab": 0
                      }
                    ],
                    "split_horizontal": false
                  }
                ],
                "enabled": true,
                "layout_name": "New Layout"
              }', "jsonRole")
        }
    }


    property var layouts_model
    property var layouts_model_root_index
    property var current_layout_index: layouts_model.index(selected_layout, 0, layouts_model_root_index)
    property int selected_layout: 0

    DelegateModel {

        id: the_layouts
        // this DelegateModel is set-up to iterate over the contents of the Media
        // node (i.e. the MediaSource objects)
        model: layouts_model
        rootIndex: layouts_model_root_index
        delegate:
            XsNavButton {
                property real btnWidth: 20+textWidth

                text: layout_name
                width: btnView.width>(btnWidth*btnView.model.count)? btnWidth : btnView.width/btnView.model.count
                height: buttonHeight

                isActive: index==selected_layout
                onClicked:{
                    selected_layout = index
                }

                Rectangle{ id: btnDivider
                    visible: index != 0
                    width:btnView.spacing
                    height: btnView.height/1.2
                    color: palette.base
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.right
                }
            }
    }

    ListView {
        id: btnView
        x: panelPadding
        orientation: ListView.Horizontal
        spacing: 1
        width: parent.width - menuBtn.width - panelPadding*3
        height: buttonHeight
        contentHeight: contentItem.childrenRect.height
        contentWidth: contentItem.childrenRect.width
        snapMode: ListView.SnapToItem
        interactive: false
        layoutDirection: Qt.RightToLeft
        anchors.verticalCenter: parent.verticalCenter
        currentIndex: selected_layout

        model: the_layouts

    }

}
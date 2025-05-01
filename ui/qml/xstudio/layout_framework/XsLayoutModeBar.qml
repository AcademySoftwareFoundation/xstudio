import QtQuick




import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

Item { id: modeBar

    height: XsStyleSheet.menuHeight

    XsGradientRectangle{
        anchors.fill: parent
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 0.5
        color: "black"
    }

    property string barId: ""
    property real panelPadding: XsStyleSheet.panelPadding
    property real buttonHeight: XsStyleSheet.widgetStdHeight-4

    // Rectangle{anchors.fill: parent; color: "red"; opacity:.3}

    property var selected_layout_index

    XsSecondaryButton{

        id: menuBtn
        width: XsStyleSheet.menuIndicatorSize
        height: XsStyleSheet.menuIndicatorSize
        anchors.right: parent.right
        anchors.rightMargin: panelPadding
        anchors.verticalCenter: parent.verticalCenter
        imgSrc: "qrc:/icons/menu.svg"
        isActive: barMenu.visible
        onClicked: {
            barMenu.showMenu(menuBtn, width/2, height/2)
        }

    }

    XsPopupMenu {
        id: barMenu
        visible: false
        menu_model_name: "ModeBarMenu"+barId
    }

    XsMenusModel {
        id: barMenuModel
        modelDataName: "ModeBarMenu"+barId
        onJsonChanged: {
            barMenu.menu_model_index = index(-1, -1)
        }
    }

    XsMenuModelItem {
        text: "New Layout"
        menuPath: ""
        menuItemPosition: 1
        menuModelName: "ModeBarMenu"+barId

        onActivated: {
            // TODO: pop-up string query dialog to get new name
            dialogHelpers.textInputDialog(
                acceptResult,
                "New Layout",
                "Enter a name for new layout.",
                "New Layout",
                ["Cancel", "Create"])
        }
        function acceptResult(new_name, button) {

            if (button == "Create") {
                var row = layouts_model.add_layout(new_name, layouts_model_root_index, "Viewport")
                callbackTimer.setTimeout(function(layout_idx) { return function() {
                    selected_layout = layout_idx
                    }}(row), 500);
            }
        }
    }

    XsMenuModelItem {
        text: "Rename Layout ..."
        menuPath: ""
        menuItemPosition: 2
        menuModelName: "ModeBarMenu"+barId
        property var idx: 1
        enabled: appWindow.layoutName != "Present"
        onActivated: {
            // TODO: pop-up string query dialog to get new name
            dialogHelpers.textInputDialog(
                acceptResult,
                "Rename Layout",
                "Enter a new name for layout \""+layoutProperties.values.layout_name+"\"",
                layoutProperties.values.layout_name,
                ["Cancel", "Rename"])
        }
        function acceptResult(new_name, button) {
            if (button == "Rename") {
                layouts_model.set(current_layout_index, new_name, "layout_name")
            }
        }
    }

    XsMenuModelItem {
        text: "Duplicate Layout ..."
        menuPath: ""
        menuItemPosition: 3
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
            // TODO: pop-up string query dialog to get new name
            dialogHelpers.textInputDialog(
                acceptResult,
                "Dulplicate Layout",
                "Enter a new name for duplicated layout \""+layoutProperties.values.layout_name+"\"",
                layoutProperties.values.layout_name,
                ["Cancel", "Duplicate"])
        }
        function acceptResult(new_name, button) {
            if (button == "Duplicate") {
                var idx = layouts_model.duplicate_layout(current_layout_index)
                layouts_model.set(idx, new_name, "layout_name")
                selected_layout = idx.row
            }
        }
    }

    XsMenuModelItem {
        text: "Remove Current Layout"
        menuPath: ""
        menuItemPosition: 3.5
        menuModelName: "ModeBarMenu"+barId
        enabled: appWindow.layoutName != "Present"
        onActivated: {
            var rc = layouts_model.rowCount(current_layout_index.parent)
            if (rc > 1) {
                var row = current_layout_index.row
                layouts_model.removeRowsSync(row, 1, current_layout_index.parent)
                if (row > 0) {
                    selected_layout = row-1
                } else {
                    selected_layout = 0
                }
            }

        }
    }

    property bool movingLayout: false

    XsMenuModelItem {
        text: "Move Layout Left"
        menuPath: ""
        menuItemPosition: 3.6
        menuModelName: "ModeBarMenu"+barId
        enabled: appWindow.layoutName != "Present"
        onActivated: {

            movingLayout = true
            var rc = layouts_model.rowCount(current_layout_index.parent)
            if (selected_layout < (rc-1)) {
                layouts_model.moveRows(
                    current_layout_index.parent,
                    selected_layout,
                    1,
                    current_layout_index.parent,
                    selected_layout+2
                    )
                selected_layout = selected_layout+1
            }
            movingLayout = false

        }
    }

    XsMenuModelItem {
        text: "Move Layout Right"
        menuPath: ""
        menuItemPosition: 3.7
        menuModelName: "ModeBarMenu"+barId
        enabled: appWindow.layoutName != "Present"
        onActivated: {
            movingLayout = true
            if (selected_layout > 1) {
                layouts_model.moveRows(
                    current_layout_index.parent,
                    selected_layout,
                    1,
                    current_layout_index.parent,
                    selected_layout-1
                    )
                selected_layout = selected_layout-1
            }
            movingLayout = false
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        text: "Tab Visibility"
        menuPath: ""
        menuItemPosition: 4
        menuModelName: "ModeBarMenu"+barId
    }

    XsMenuModelItem {

        menuItemType: "radiogroup"
        choices: ["Hide All", "Hide Single Tabs", "Show All"]
        text: "Tab Visbility"
        menuPath: ""
        menuItemPosition: 5
        menuModelName: "ModeBarMenu"+barId
        onCurrentChoiceChanged: {
            appWindow.tabsVisibility = choices.indexOf(currentChoice)
        }
        // awkward two way binding as usual!
        property var follower: appWindow.tabsVisibility
        onFollowerChanged: {
            if (choices[follower] && choices[follower] != currentChoice) {
                currentChoice = choices[follower]
            }
        }
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 6
        menuModelName: "ModeBarMenu"+barId
    }


    XsMenuModelItem {
        text: "Restore Default Layouts"
        menuPath: ""
        menuItemPosition: 7
        menuModelName: "ModeBarMenu"+barId
        onActivated: {
            layouts_model.restoreDefaults()
        }
    }


    XsModelPropertyMap {
        id: layoutProperties
        index: current_layout_index
    }

    property var layouts_model
    property var layouts_model_root_index
    property var current_layout_index: layouts_model.index(selected_layout, 0, layouts_model_root_index)
    property int selected_layout: -1

    onSelected_layoutChanged: {
        if (layouts_model_root_index.valid) {
            layouts_model.set(layouts_model_root_index, selected_layout, "current_layout")
        }
    }

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
                font.bold: true

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
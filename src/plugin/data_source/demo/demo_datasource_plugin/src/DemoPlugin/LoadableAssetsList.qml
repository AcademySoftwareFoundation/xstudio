// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// this import allows us to use the DemoPluginDatamodel
import demoplugin.qml 1.0

Item {

    // This gives us a crude table-like view of the 'assets' that the items 
    // in the shot tree contain. There are a few ways to achieve a table
    // like interface in QML - this is not necessarily the best! However it
    // should be useful in showing how to use data models and selection 
    // models to drive UI building.
    // In this case we build a flat list of data to show in the asset view
    // here in QML based on the user selection in the sequence/shot tree. A
    // more efficient solution would be to request data

    id: tree
    property var tree_selection: []
    clip: true
    property bool can_load_media: assetSelectionModel.hasSelection && !can_load_sequence
    property bool can_load_sequence: false

    // here is our instance of the data model to drive the UI - it's an
    // instance of the 'DemoPluginVersionsModel' defined in demo_plugin_ui.hpp
    DemoPluginVersionsModel {
        id: versionsModel
        property var root_index: dataModel.index(0, 0)
        onModelReset: {
            root_index = dataModel.index(0, 0)
        }
    }
    property alias versionsModel: versionsModel

    ItemSelectionModel {
        id: versionsSelectionModel
        model: versionsModel
        onSelectedIndexesChanged: {
            // scan to see if selection includes an OTIO
            var have_otio = false
            for (var i = 0; i < selectedIndexes.length; ++i) {
                if (versionsModel.data(selectedIndexes[i], "version_type") == "OTIO") {
                    have_otio = true
                    break
                }
            }
            can_load_sequence = have_otio
        }
    }
    property alias assetSelectionModel: versionsSelectionModel

    DelegateModel {

        id: delegateModel
        model: versionsModel
        rootIndex: versionsModel.root_index
        delegate: RenderItem {
            model_index: versionsModel.index(index, 0)
        }
    }

    ColumnLayout {

        anchors.fill: parent
        spacing: 0

        Rectangle {

            Layout.fillWidth: true
            Layout.preferredHeight: 40
            border.color: XsStyleSheet.menuDividerColor
            border.width: 1

            gradient: Gradient {
                GradientStop { position: 0.0; color: XsStyleSheet.panelBgColor }
                GradientStop { position: 0.33; color: XsStyleSheet.panelBgFlatColor }
                GradientStop { position: 1.0; color: XsStyleSheet.panelBgColor }
            }

            XsText {
                anchors.centerIn: parent
                text: "Asset List"
            }

        }

        Item {
            Layout.preferredHeight: 8
        }

        RowLayout {
            id: cols_layout
            spacing: 0
            Repeater {
                model: columns_model
                Item {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: columnWidth
                    Rectangle {
                        height: 1
                        width: parent.width
                        color: XsStyleSheet.menuDividerColor
                    }
                    Rectangle {
                        height: parent.height
                        width: 1
                        color: XsStyleSheet.menuDividerColor
                    }
                    Text {
                        anchors.fill: parent
                        text: name
                        color: palette.text
                        font.pixelSize: XsStyleSheet.fontSize
                        font.family: XsStyleSheet.fontFamily
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                    }
                }
            }
            Rectangle {
                width: 1
                Layout.preferredHeight: 30
                color: XsStyleSheet.menuDividerColor
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: cols_layout.contentWidth
            color: XsStyleSheet.menuDividerColor
        }

        // This list view shows a bunch of SequenceItem items as driven by the 
        // rows in delegateModel
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: view
            model: delegateModel
            clip: true
            CustomScrollBar {        
                target: view
            }    

        }

    }

    // Define a model that controls the columns of data we see at the 'RenderItem'
    // level in our tree.
    ListModel {
        id: columns_model
        ListElement {
            name: ""
            roleName: "icon"
            columnWidth: 40
            columnType: "icon"
        }
        ListElement {
            name: "Name"
            roleName: "version_name"
            columnWidth: 250
            columnType: "text"
        }
        ListElement {
            name: "Type"
            roleName: "version_type"
            columnWidth: 80
            columnType: "text"
        }        
        ListElement {
            name: "Artist"
            roleName: "artist"
            columnWidth: 150
            columnType: "text"
        }
        ListElement {
            name: "Status"
            roleName: "status"
            columnWidth: 100
            columnType: "text"
        }        
        ListElement {
            name: "Frame Range"
            roleName: "frame_range"
            columnWidth: 100
            columnType: "text"
        }        

    }
    // For Items, we can ensure that they are visible in all our children like this
    property alias columns_model: columns_model

    function selected_sequences() {
        var selected_sequence_uuids = []
        for (var i = 0; i < assetSelectionModel.selectedIndexes.length; ++i) {
            var media_item_uuid = versionsModel.data(assetSelectionModel.selectedIndexes[i], "uuid")
            if (media_item_uuid != undefined) {
                selected_sequence_uuids.push(media_item_uuid)
            }
        }
        return selected_sequence_uuids
    }

    function selected_media() {
        var selected_media_uuids = []
        for (var i = 0; i < assetSelectionModel.selectedIndexes.length; ++i) {
            var media_item_uuid = versionsModel.data(assetSelectionModel.selectedIndexes[i], "uuid")
            if (media_item_uuid != undefined) {
                selected_media_uuids.push(media_item_uuid)
            }
        }
        return selected_media_uuids
    }

    // Add a Right-mouse button click menu. We use the Component/Loader approach
    // so that the pop-up menu is onlhy built when it is (first) shown. This helps
    // with load times when xSTUDIO starts up.
    Component {
        id: contextMenuComponent
        XsPopupMenu {
            id: contextMenu
            visible: false
            // we need to set a name for the model that drives this menu. Because
            // we could have multiple instances of the demo plugin panel in the
            // xSTUDIO layouts, we must have a unique menu model name for each 
            // instance. Using the id of the parent item will ensure this
            menu_model_name: "demo_plugin_menu" + panel
        }
    }
    Loader {
        id: loader
    }

    function showContextMenu(mx, my, parent) {
        if (loader.item == undefined) {
            loader.sourceComponent = contextMenuComponent
        }
        loader.item.showMenu(
            parent,
            mx,
            my);
    }

    // To add menu items to our pop-up menu we use 'XsMenuItem'.
    // These can be instanced in a Repeater, for example
    Repeater {
        model: ["Final", "Pending", "Approved", "Declined", "Proposed"]
        Item {
            XsMenuModelItem {
                text: modelData
                // path sets the submenu heirarchy under which this menu item
                // will appear. Submenus are separated with a '|' (pipe)
                menuPath: "Status|Set Status on Selected Items"
                menuModelName: "demo_plugin_menu" + panel
                menuItemPosition: index // index is set by the Repeater
                enabled: assetSelectionModel.selectedIndexes.length != 0
                onActivated: {
                    for (var i = 0; i < assetSelectionModel.selectedIndexes.length; ++i) {
                        var idx = assetSelectionModel.selectedIndexes[i]
                        versionsModel.set(
                            idx, // index into model
                            modelData.toLowerCase(), // the value to change to (e.g. 'approved', 'final' etc.)
                            "status" // the name of the roleData that we are changing - must match a role name set for our DataModel C++ class
                        )
                    }
                }
            }
        }
    }

    MouseArea {

        id: ma
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: (mouse) => {

            if (mouse.buttons == Qt.RightButton) {
                showContextMenu(mouseX, mouseY, ma)
            }
        }
    }

}

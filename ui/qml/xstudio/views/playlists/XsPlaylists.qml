// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

import xStudio 1.0
import xstudio.qml.helpers 1.0
import "./widgets"

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
    property color textColorNormal: palette.text
    property color hintColor: XsStyleSheet.hintColor

    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real secBtnWidth: XsStyleSheet.secondaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    XsGradientRectangle{
        anchors.fill: parent
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        propagateComposedEvents: true
        acceptedButtons: Qt.RightButton
        onPressed: (mouse)=> {
            if (mouse.buttons == Qt.RightButton) {
                showContextMenu(mouseX, mouseY, ma)
            }
        }
    }

    XsModelProperty {
        id: notificationProperty
        role: "notificationRole"
        index: theSessionData.playlistsRootIdx
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: panelPadding

        RowLayout{
            // x: panelPadding
            spacing: 1
            Layout.fillWidth: true

            XsPrimaryButton{ id: addPlaylistBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: btnHeight
                imgSrc: "qrc:/icons/add.svg"
                onClicked: {
                    var pos = mapToItem(panel, x+width/2, y+height/2)
                    togglePlusMenu(pos.x ,pos.y, addPlaylistBtn)
                }
            }

            XsPrimaryButton{ id: deleteBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: btnHeight
                imgSrc: "qrc:/icons/delete.svg"
                onClicked: toggleRemoveMenu(width/2, height/2, deleteBtn)
                // onClicked: {
                //     removeSelected()
                // }
            }

            // XsSearchButton{ id: searchBtn
            //     Layout.preferredWidth: isExpanded? btnWidth*6 : btnWidth
            //     Layout.preferredHeight: btnHeight
            //     isExpanded: false
            //     hint: "Search playlists..."
            // }
            Item {
                Layout.fillWidth: true

            }

            // XsText {
            //     Layout.fillWidth: true
            //     Layout.preferredHeight: btnHeight
            //     elide: Text.ElideMiddle
            //     text: filename
            //     font.bold: true
            //     property string path: sessionProperties.values.pathRole ? sessionProperties.values.pathRole : ""
            //     property string filename: path ? path.substring(path.lastIndexOf("/")+1) : sessionProperties.values.nameRole ? sessionProperties.values.nameRole : ""
            // }

            Repeater {
                // Layout.preferredHeight: btnHeight
                model: notificationProperty.value == undefined ? [] : notificationProperty.value

                // notify widget
                XsNotification {
                    Layout.preferredHeight: btnHeight
                    Layout.preferredWidth: height
                    text: modelData.text
                    type: modelData.type
                    percentage: modelData.progress_percent || 0.0
                }
            }


            XsPrimaryButton{
                id: morePlaylistBtn
                Layout.preferredWidth: btnWidth
                Layout.preferredHeight: btnHeight
                imgSrc: "qrc:/icons/more_vert.svg"
                onClicked: {
                    toggleContextMenu(width/2, height/2, morePlaylistBtn)
                }
            }
        }

        XsPlaylistItems{

            id: playlistItems
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

    }


    XsPopupMenu {
        id: flagMenu
        visible: false
        menu_model_name: "playlist_flag_menu_"+flagMenu
        property var panelContext: helpers.contextPanel(flagMenu)
        property var itemIndex: null

        XsFlagMenuInserter {
            text: ""
            menuPath: ""
            panelContext: flagMenu.panelContext
            menuModelName: flagMenu.menu_model_name
            onFlagSet: (flag, flag_text) => {
                theSessionData.set(flagMenu.itemIndex, flag, "flagColourRole")

                if (flag_text)
                    theSessionData.set(flagMenu.itemIndex, flag_text, "flagTextRole")
            }
        }
    }

    function showFlagMenu(mx, my, source, itemIndex) {
        let sp = mapFromItem(source, mx, my)
        flagMenu.x = sp.x
        flagMenu.y = sp.y
        flagMenu.itemIndex = itemIndex
        flagMenu.visible = true
    }

    Loader {
        id: menu_loader
    }


    Component {
        id: plusMenuComponent
        XsPlaylistPlusMenu {
            property bool hoveredButton: false
            closePolicy: hoveredButton ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
    }

    function togglePlusMenu(mx, my, parent) {
        if (menu_loader.item == undefined || !menu_loader.item.visible) {
            showMenu(mx, my, parent)
        } else {
            menu_loader.item.close()
        }
    }

    function showMenu(mx, my, parent) {
        if (menu_loader.item == undefined) {
            menu_loader.sourceComponent = plusMenuComponent
        }
        menu_loader.item.hoveredButton = Qt.binding(function() { return parent.hovered })
        menu_loader.item.x = mx
        menu_loader.item.y = my
        menu_loader.item.visible = true
    }

    Loader {
        id: context_menu_loader
    }

    Component {
        id: contextMenuComponent
        XsPlaylistContextMenu {
            property bool hoveredButton: false
            closePolicy: hoveredButton ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
    }

    function removeSelected() {
        dialogHelpers.multiChoiceDialog(
            doRemove,
            "Remove Selected Items",
            "Remove selected items?",
            ["Cancel", "Remove Items"],
            undefined)

    }

    Loader {
        id: menu_loader_remove
    }

    Component {
        id: removeMenuComponent
        XsPlaylistRemoveMenu {
            property bool hoveredButton: false
            closePolicy: hoveredButton ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
    }


    function toggleRemoveMenu(mx, my, parent) {
        if (menu_loader_remove.item == undefined || !menu_loader_remove.item.visible) {
            showRemoveMenu(mx, my, parent)
        } else {
            menu_loader_remove.item.close()
        }
    }

    function showRemoveMenu(mx, my, parent) {
        if (menu_loader_remove.item == undefined) {
            menu_loader_remove.sourceComponent = removeMenuComponent
        }
        menu_loader_remove.item.hoveredButton = Qt.binding(function() { return parent.hovered })
        menu_loader_remove.item.showMenu(
            parent,
            mx,
            my);
    }


    function doRemove(button) {

        if (button == "Cancel") return

        let indexes_to_be_deleted = []
        let selected = sessionSelectionModel.selectedIndexes
        var new_selected_item = undefined;
        for (var i = 0; i < selected.length; ++i) {
            let index = selected[i]
            indexes_to_be_deleted.push(helpers.makePersistent(index))

            // if a subset or timeline is deleted, we want to select its
            // parent playlist afterwards (as long as the parent isn't also
            // to be deleted)
            let type = theSessionData.get(index, "typeRole")
            if (type == "Subset" || type == "Timeline" || type == "ContactSheet") {
                var playlist_idx = index.parent.parent
                if (!selected.includes(playlist_idx)) {
                    new_selected_item = helpers.makePersistent(playlist_idx)
                }
            }
        }

        if (!new_selected_item) {

            // if playlist 2 of 4 is being deleted, we want to select playlist
            // 2 after the deletion (i.e. the playlist that WAS 3rd before the
            // deletion).

            for (var i = 0; i < indexes_to_be_deleted.length; ++i) {
                // check if
                var index_to_be_deleted = indexes_to_be_deleted[i]

                if ((index_to_be_deleted.row+1) < theSessionData.rowCount(index_to_be_deleted.parent)) {
                    var next_idx = helpers.makePersistent(
                        theSessionData.index(
                            index_to_be_deleted.row+1,
                            0,
                            index_to_be_deleted.parent
                            )
                        )

                    if (!indexes_to_be_deleted.includes(next_idx)) {
                        new_selected_item = next_idx
                    }
                }
            }
            // If playlist 4 of 4 is being deleted we want to select 3rd.
            if (!new_selected_item) {

                for (var i = 0; i < indexes_to_be_deleted.length; ++i) {
                    // check if
                    var index_to_be_deleted = indexes_to_be_deleted[i]
                    if (index_to_be_deleted.row) {
                        var prev_idx = helpers.makePersistent(
                            theSessionData.index(
                                index_to_be_deleted.row-1,
                                0,
                                index_to_be_deleted.parent
                                )
                            )
                        if (!indexes_to_be_deleted.includes(prev_idx)) {
                            new_selected_item = prev_idx
                        }
                    }
                }
            }

        }

        sessionSelectionModel.clear()

        for (var i = 0; i < indexes_to_be_deleted.length; ++i) {
            theSessionData.removeRows(
                indexes_to_be_deleted[i].row,
                1,
                false,
                indexes_to_be_deleted[i].parent
                )
        }

        // select the first remaining playlist, if any
        if (new_selected_item) {

            sessionSelectionModel.setCurrentIndex(
                new_selected_item,
                ItemSelectionModel.ClearAndSelect
                )

        } else if (theSessionData.rowCount(theSessionData.index(0,0))) {

            sessionSelectionModel.setCurrentIndex(
                theSessionData.index(0,0,theSessionData.index(0,0)),
                ItemSelectionModel.ClearAndSelect
                )
        }

    }

    function toggleContextMenu(mx, my, parent) {
        if (context_menu_loader.item == undefined || !context_menu_loader.item.visible) {
            showContextMenu(mx, my, parent)
        } else {
            context_menu_loader.item.close()
        }
    }

    function showContextMenu(mx, my, parentWidget) {
        if (context_menu_loader.item == undefined) {
            context_menu_loader.sourceComponent = contextMenuComponent
        }
        context_menu_loader.item.hoveredButton = Qt.binding(function() { return parentWidget.hovered })
        context_menu_loader.item.visible = true
        context_menu_loader.item.showMenu(
            parentWidget,
            mx,
            my);
    }


}
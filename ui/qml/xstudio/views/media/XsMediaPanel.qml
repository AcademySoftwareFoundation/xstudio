// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtQuick.Controls.Styles 1.4
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtQuick.Layouts 1.15

import xStudio 1.0

import xstudio.qml.session 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0
// WHY ARE WE DOING THIS AGAIN ?
import "./list_view"
import "./grid_view"
import "./widgets"
import "./functions"

Item{

    id: panel
    anchors.fill: parent
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
    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    property real rowHeight: XsStyleSheet.widgetStdHeight

    property var mediaSelection: mediaSelectionModel.selectedIndexes

    property bool is_list_view: true
    property int standardCellSize: 150
    property real cellSize: 200

    // persist the type of media view and cell size
    XsStoredPanelProperties {
        propertyNames: ["is_list_view", "cellSize"]
    }

    /**************************************************************
    HotKeys
    ****************************************************************/
    XsMediaHotKeys{
        id: hotkey_area
    }
    property var select_all_hotkey: hotkey_area.select_all_hotkey
    property var deselect_all_hotkey: hotkey_area.deselect_all_hotkey
    property alias delete_selected: hotkey_area.delete_selected_hotkey
    property alias reload_selected_media_hotkey: hotkey_area.reload_selected_media_hotkey


    XsGradientRectangle{
        anchors.fill: parent
    }


    XsMediaListColumnsModel {
        id: columns_root_model
    }
    property alias columns_root_model: columns_root_model

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: panelPadding
        spacing: panelPadding

        XsMediaToolBar{
            id: toolbar
            Layout.fillWidth: true
        }

        Loader {
            id: loader
            sourceComponent: is_list_view ? list_view : grid_view
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Component {
            id: list_view

            XsMediaListLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
        Component {
            id: grid_view

            XsMediaGridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

    }

    function sort_media(sort_column_idx, ascending) {
        theSessionData.sortByMediaDisplayInfo(
            currentMediaContainerIndex,
            sort_column_idx,
            ascending)
    }

    /**************************************************************
    Functions
    ****************************************************************/

    XsMediaListFunctions {
        id: media_list_functions
    }
    property var selectAll: media_list_functions.selectAll
    property var selectAllOffline: media_list_functions.selectAllOffline
    property var invertSelection: media_list_functions.invertSelection
    property var selectFlagged: media_list_functions.selectFlagged
    property var selectAllAdjusted: media_list_functions.selectAllAdjusted
    property var deselectAll: media_list_functions.deselectAll
    property var mediaIndexAfterRemoved: media_list_functions.mediaIndexAfterRemoved
    property var deleteSelected: media_list_functions.deleteSelected
    property var deleteOffline: media_list_functions.deleteOffline
    property var selectUp: media_list_functions.selectUp
    property var selectDown: media_list_functions.selectDown
    property var cycleSelection: media_list_functions.cycleSelection
    property var addToNewSequence: media_list_functions.addToNewSequence
    property var addToNewSubset: media_list_functions.addToNewSubset
    property var addToNewContactSheet: media_list_functions.addToNewContactSheet
    
    Loader {
        id: menu_loader_plus
    }

    Loader {
        id: menu_loader_remove
    }

    Loader {
        id: menu_loader_context
    }

    Component {
        id: plusMenuComponent
        XsMediaListPlusMenu {
            property bool hoveredButton: false
            closePolicy: hoveredButton ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
    }

    Component {
        id: removeMenuComponent
        XsMediaListRemoveMenu {
            property bool hoveredButton: false
            closePolicy: hoveredButton ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
    }

    Component {
        id: contextMenuComponent
        XsMediaListContextMenu {
            property bool hoveredButton: false
            closePolicy: hoveredButton ? Popup.CloseOnEscape :  Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
    }

    function togglePlusMenu(mx, my, parent) {
        if (menu_loader_plus.item == undefined || !menu_loader_plus.item.visible) {
            showPlusMenu(mx, my, parent)
        } else {
            menu_loader_plus.item.close()
        }
    }

    function showPlusMenu(mx, my, parent) {
        if (menu_loader_plus.item == undefined) {
            menu_loader_plus.sourceComponent = plusMenuComponent
        }
        menu_loader_plus.item.hoveredButton = Qt.binding(function() { return parent.hovered })
        menu_loader_plus.item.showMenu(
            parent,
            mx,
            my);
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

    function toggleContextMenu(mx, my, parent) {
        if (menu_loader_context.item == undefined || !menu_loader_context.item.visible) {
            showContextMenu(mx, my, parent)
        } else {
            menu_loader_context.item.close()
        }
    }

    function showContextMenu(mx, my, parent) {
        if (menu_loader_context.item == undefined) {
            menu_loader_context.sourceComponent = contextMenuComponent
        }
        // What's this line for?!
        // menu_loader_context.item.hoveredButton = Qt.binding(function() { return parent.hovered })
        menu_loader_context.item.showMenu(
            parent,
            mx,
            my);
    }


}
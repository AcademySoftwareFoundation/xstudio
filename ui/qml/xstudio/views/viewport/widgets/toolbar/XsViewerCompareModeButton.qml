// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

// 'XsViewerAnyMenuButton' can be used as a base for a button that appears
// in the viewport toolbar and that shows a pop-up menu when it is clicked
// on.
XsViewerAnyMenuButton  {

    id: source_button
    anchors.fill: parent

    // here we choose which menu model is used to build the pop-up that appears
    // when you click on the source button in the toolbar. We want the menu
    // to show the sources of the current playhead, which are defined by the
    // Source and Audio Source attributes in the Playhead class.
    // see PlayheadActor::make_source_menu_model in the C++ source.
    menuModelName: "compare_modes" + source_button

    text: "Compare"
    valueText: compare_mode

    property var settingsDlg

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: compare_modes
        modelDataName: "viewport_layouts"
    }

    Repeater {
        model: compare_modes

        // add a menu item to toggle on/off each HUD plugin or show its
        // settings
        Item {
            XsMenuModelItem {
                text: title
                menuItemType: "toggle"
                menuPath: ""
                menuModelName: "compare_modes" + source_button
                // for 'toggle' menus, use property 'isChecked' for check state
                isChecked: title == compare_mode
                // Awkward two way binding needed here...
                onActivated: {
                    compare_mode = title
                }
                enabled: title == "String" ? !viewportPlayhead.timelineMode : true
            }

        }
    }

    function show_settings(user_data) {
        if (typeof user_data == "string") {
            // custom settings dialog
            settingsDlg = Qt.createQmlObject(user_data, source_button)
            settingsDlg.visible = true
        } else {
            // show the settings box for the HUD item:
            var settings_group_name = compare_mode + " Settings"
            settingsDlg = Qt.createQmlObject('import xStudio 1.0; XsAttributesPanel {}', source_button, settings_group_name)
            settingsDlg.title = settings_group_name
            settingsDlg.attributesGroupName = settings_group_name
            settingsDlg.self_destroy = true
            settingsDlg.visible = true
        }
    }

    XsAttributeValue {
        id: active_tool
        attributeTitle: "HUD"
        model: toolbar_data
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 10.0
        menuModelName: "compare_modes" + source_button
    }

    XsMenuModelItem {
        text: "Settings ..."
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 11.0
        menuModelName: "compare_modes" + source_button
        onActivated: {
            show_settings(false)
        }
    }

    // connect to the viewport toolbar data
    XsModuleData {
        id: toolbar_data
        modelDataName: view.name + "_toolbar"
    }

    // access the value of the attribute called 'Compare' which is exposed in the
    // viewport _toolbar. 
    XsAttributeValue {
        id: __compare_mode
        attributeTitle: "Compare"
        model: toolbar_data
    }
    property alias compare_mode: __compare_mode.value


}

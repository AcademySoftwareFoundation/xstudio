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
    menuModelName: "hud_button_menu" + source_button

    text: "HUD"
    valueText: hud_enabled ? "On" : "Off"

    /* This connects to the backend annotations tool object and exposes its
    ui data via model data */
    XsModuleData {
        id: hud_toggles
        modelDataName: "hud_element_toggles"
    }

    Repeater {
        model: hud_toggles

        // add a menu item to toggle on/off each HUD plugin or show its
        // settings
        Item {
            property var settingsDlg
            XsMenuModelItem {
                text: title
                menuItemType: "toggle_settings"
                menuPath: ""
                menuItemPosition: toolbar_position != undefined ? toolbar_position : 100.0+index
                menuModelName: "hud_button_menu" + source_button
                // for 'toggle' menus, use property 'isChecked' for check state
                isChecked: value
                property var value__: value
                // Awkward two way binding needed here...
                onValue__Changed: {
                    if (value__ != isChecked) {
                        isChecked = value__
                    }
                }
                onIsCheckedChanged: {
                    if (value != isChecked) {
                        value = isChecked
                    }
                }
                onActivated: {
                    if (typeof user_data == "string") {
                        // custom settings dialog
                        settingsDlg = Qt.createQmlObject(user_data, source_button)
                        settingsDlg.visible = true
                    } else {
                        // show the settings box for the HUD item:
                        var settings_group_name = title + " Settings"
                        settingsDlg = Qt.createQmlObject('import xStudio 1.0; XsAttributesPanel {}', source_button, settings_group_name)
                        settingsDlg.title = settings_group_name
                        settingsDlg.attributesGroupName = settings_group_name
                        settingsDlg.self_destroy = true
                        settingsDlg.visible = true
                    }
                    hide_menu()
                }
            }

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
        menuModelName: "hud_button_menu" + source_button
    }

    // connect to the viewport toolbar data
    XsModuleData {
        id: toolbar_data
        modelDataName: view.name + "_toolbar"
    }

    // access the value of the attribute called 'HUD' which is exposed in the
    // viewport _toolbar. Not the hud attribute belongs to the Viewport backend
    // class
    XsAttributeValue {
        id: hud_enabled_prop
        attributeTitle: "HUD"
        model: toolbar_data
    }
    property alias hud_enabled: hud_enabled_prop.value

    XsMenuModelItem {
        text: "HUD Enabled"
        menuItemType: "toggle"
        menuPath: ""
        menuItemPosition: 11.0
        menuModelName: "hud_button_menu" + source_button
        isChecked: hud_enabled != undefined ? hud_enabled : false
        onActivated: {
            hud_enabled = !hud_enabled
            hide_menu()
        }
    }

}

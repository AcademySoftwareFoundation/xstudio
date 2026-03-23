// SPDX-License-Identifier: Apache-2.0
import QtQuick

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
                menuItemType: user_data ? "toggle_settings" : "toggle"
                menuPath: ""
                menuModelName: "compare_modes" + source_button
                menuItemPosition: toolbar_position != undefined ? toolbar_position : 100.0+index
                // for 'toggle' menus, use property 'isChecked' for check state

                // Here we have the usual awkward two way binding. WHen user clicks
                // on menu, isChecked is toggled by the backend stuff, so our
                // binding on the next line gets wiped.
                isChecked: modeActive
                onIsCheckedChanged: {
                    if (isChecked && !modeActive) {
                        compare_mode = title
                        hide_menu()
                    }
                    
                }

                // read only follower
                property var modeActive: title == compare_mode
                onModeActiveChanged: {
                    if (modeActive != isChecked) {
                        isChecked = modeActive
                    }
                }

                // Clicking on the cog icon results in actived signal
                onActivated: {
                    if (menuItemType == "toggle_settings") {
                        var settings_group_name = title + " Settings"
                        settingsDlg = Qt.createQmlObject('import xStudio 1.0; XsAttributesPanel {}', source_button, settings_group_name)
                        settingsDlg.title = settings_group_name
                        settingsDlg.attributesGroupName = settings_group_name
                        settingsDlg.self_destroy = true
                        settingsDlg.visible = true
                        hide_menu()
                    } else {
                        compare_mode = title
                        hide_menu()
                    }
                }

                // Special case, String mode is not applicable when viewing a 
                // timeline
                enabled: title == "String" ? !viewportPlayhead.timelineMode : true

                
            }

        }
    }

    // Note: in backend where 'ViewportLayoutPlugin' calls add_layout_mode to 
    // add to this list of compare modes, a position in the menu is provided.
    // I have added 'regular' compare modes (Off, A/B, String etc.) in range
    // 100+ (so they are nearest the button at the bottom of the list),
    // 'layout' compare modes like Grid, Pip start from 20.0 and then composites
    // are 1.0 and above.
    // It is possible in the backend to add dividers to the model too but this
    // way is just as easy
    XsMenuModelItem {
        menuItemType: "divider"
        text: "Layouts"
        menuPath: ""
        menuItemPosition: 19.0
        menuModelName: "compare_modes" + source_button
    }

    XsMenuModelItem {
        menuItemType: "divider"
        text: "Composite"
        menuPath: ""
        menuItemPosition: 0.0
        menuModelName: "compare_modes" + source_button
    }

    XsMenuModelItem {
        menuItemType: "divider"
        text: "General"
        menuPath: ""
        menuItemPosition: 99.0
        menuModelName: "compare_modes" + source_button
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

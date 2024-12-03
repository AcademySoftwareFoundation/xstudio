import QtQuick 2.12
import QtQuick.Layouts 1.15
import QtQml.Models 2.15
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

import "."
import "./toolbar"

Item {
    id: toolBar
    width: parent.width
    height: btnHeight*opacity
    visible: opacity != 0.0
    Behavior on opacity {NumberAnimation{ duration: 150 }}

    property string panelIdForMenu: panelId //TODO: to fix, not defined for quick-view

    property real barPadding: XsStyleSheet.panelPadding
    property real btnWidth: XsStyleSheet.primaryButtonStdWidth
    property real btnHeight: XsStyleSheet.widgetStdHeight
    property string toolbar_model_data_name

    // Here is where we get all the data about toolbar items that is broadcast
    // from the backend. Note that each entry in the model (which is a simple
    // 1-dimensional list)
    //
    // Each item can (but doesn't have to) provide 'role' data with the following
    // names, which are 'visible' in delegates of Repeater(s) etc:
    //
    // type, attr_enabled, activated, title, abbr_title, combo_box_options,
    // combo_box_abbr_options, combo_box_options_enabled, tooltip,
    // custom_message, integer_min, integer_max, float_scrub_min,
    // float_scrub_max, float_scrub_step, float_scrub_sensitivity,
    // float_display_decimals, value, default_value, short_value,
    // disabled_value, attr_uuid, groups, menu_paths, toolbar_position,
    // override_value, serialize_key, qml_code, preference_path,
    // init_only_preference_path, font_size, font_family, text_alignment,
    // text_alignment_box, attr_colour, hotkey_uuid
    //
    // Some important ones are:
    // 'title' (the name of the corresponding backend attribute)
    // 'value' (the actual data value of the attribute)
    // 'type' (the attribute type, e.g. float, bool, multichoice)
    // 'combo_box_options' (for multichoice attrs, this is a list of strings)

    XsModuleData {
        id: toolbar_model_data
        modelDataName: toolbar_model_data_name
    }

    XsPreference {
        id: __toolbarHiddenItems
        index: globalStoreModel.searchRecursive("/ui/viewport/hidden_toolbar_items", "pathRole")
    }
    property var toolbarHiddenItems: __toolbarHiddenItems.value
    onToolbarHiddenItemsChanged: {
        filterModel.setRoleFilter(toolbarHiddenItems, "title")
        filterModel.sortRoleName = "toolbar_position"
    }

    XsFilterModel {
        id: filterModel
        sourceModel: toolbar_model_data
        sortAscending: true
        invert: true // inverts the selection, so gives us thing NOT in 'toolbarHiddenItems' list
    }

    RowLayout{

        id: rowDiv
        x: barPadding
        spacing: 1
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        property var buttonWidth: width/filterModel.length

        Repeater {

            id: the_view
            model: filterModel

            delegate: chooser

            DelegateChooser {
                id: chooser
                role: "type"

                DelegateChoice {
                    roleValue: "FloatScrubber"

                    XsViewerToolbarValueScrubber
                    {
                        Layout.fillWidth: true
                        Layout.preferredWidth: rowDiv.buttonWidth
                        Layout.preferredHeight: rowDiv.height
                        text: title
                        shortText: abbr_title
                        fromValue: float_scrub_min
                        toValue: float_scrub_max
                        stepSize: float_scrub_step
                        decimalDigits: 2
                    }

                }

                DelegateChoice {
                    roleValue: "IntegerValue"

                    XsViewerToolbarValueScrubber
                    {
                        Layout.fillWidth: true
                        Layout.preferredWidth: rowDiv.buttonWidth
                        Layout.preferredHeight: rowDiv.height
                        text: title
                        shortText: abbr_title
                        fromValue: -1000
                        toValue: 1000
                        stepSize: 10
                        decimalDigits: 0
                        sensitivity: 0.5
                    }

                }

                DelegateChoice {
                    roleValue: "ComboBox"

                    XsViewerMenuButton
                    {
                        Layout.fillWidth: true
                        Layout.preferredWidth: rowDiv.buttonWidth
                        Layout.preferredHeight: rowDiv.height
                        text: title
                        shortText: abbr_title
                        valueText: value
                    }

                }

                DelegateChoice {

                    roleValue: "OnOffToggle"
                    XsViewerToggleButton
                    {
                        Layout.fillWidth: true
                        Layout.preferredWidth: rowDiv.buttonWidth
                        Layout.preferredHeight: rowDiv.height
                        text: title
                        shortText: abbr_title
                    }


                }

                DelegateChoice {

                    roleValue: "QmlCode"
                    Item {

                        id: parent_item
                        Layout.fillWidth: true
                        Layout.preferredWidth: rowDiv.buttonWidth
                        Layout.preferredHeight: rowDiv.height
                        property var dynamic_widget
                        property var qml_code_: qml_code
                        onQml_code_Changed: {
                            dynamic_widget = Qt.createQmlObject(qml_code, parent_item)
                        }
                    }

                }

            }
        }


    }
}
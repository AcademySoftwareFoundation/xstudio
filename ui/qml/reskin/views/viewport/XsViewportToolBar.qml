import QtQuick 2.12
import QtQuick.Layouts 1.15
import Qt.labs.qmlmodels 1.0

import xStudioReskin 1.0
import xstudio.qml.models 1.0
import "./widgets"

Item {
    id: toolBar
    width: parent.width
    height: btnHeight //+(barPadding*2) 

    property string panelIdForMenu: panelId

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

    RowLayout{

        id: rowDiv
        x: barPadding
        spacing: 1
        width: parent.width-(x*2)-(spacing*(btnCount))
        height: btnHeight
        anchors.verticalCenter: parent.verticalCenter

        property int btnCount: toolbar_model_data.length-3 //-3 because Source button not working yet and hiding zoom and pan for now
        property real preferredBtnWidth: (width/btnCount) //- (spacing)
        
        Repeater {

            id: the_view
            model: toolbar_model_data
    
            delegate: chooser

            DelegateChooser {
                id: chooser
                role: "type" 
            
                DelegateChoice {
                    roleValue: "FloatScrubber"
                    
                    XsViewerSeekEditButton{ 
                        Layout.preferredWidth: rowDiv.preferredBtnWidth
                        Layout.preferredHeight: parent.height
                        text: title
                        shortText: abbr_title
                        fromValue: float_scrub_min
                        toValue: float_scrub_max
                        stepSize: float_scrub_step
                        decimalDigits: 2
                    }

                }


                DelegateChoice {
                    roleValue: "ComboBox"
                    
                    XsViewerMenuButton
                    { 
                        Layout.preferredWidth: rowDiv.preferredBtnWidth
                        Layout.preferredHeight: parent.height
                        text: title
                        shortText: abbr_title
                        valueText: value
                    }

                }

                DelegateChoice {
                    roleValue: "OnOffToggle"
                    
                    XsViewerToggleButton
                    {
                        property bool isZmPan: title == "Zoom (Z)" || title == "Pan (X)"
                        Layout.preferredWidth: isZmPan ? 0 : rowDiv.preferredBtnWidth
                        Layout.preferredHeight: parent.height
                        text: title
                        shortText: abbr_title
                        visible: !isZmPan
                    }

                }
            }
        }
    
    }
}
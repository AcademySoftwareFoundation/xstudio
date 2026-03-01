// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

import xStudio 1.0
import xstudio.qml.models 1.0
import xstudio.qml.clipboard 1.0

Item{ id: menuDiv


    property var copy_buffer: []
    property alias moreMenu: moreMenu


    Clipboard{
        id: clipboard
    }


    XsPopupMenu {
        id: moreMenu
        visible: false
        menu_model_name: "moreMenu"+menuDiv
    }

    XsMenuModelItem {
        menuItemType: "radiogroup"
        choices: attrs.media_colour_managed ? ["scene_linear", "compositing_log"] : ["raw"]
        
        property string currentColorSpace: attrs.media_colour_managed ? attrs.colour_space : "raw"
        
        currentChoice: currentColorSpace
        onCurrentChoiceChanged: {
            if (currentChoice != "raw")
                attrs.colour_space = currentChoice
        }

        enabled: hasActiveGrade() && attrs.media_colour_managed
        text: ""
        menuPath: "Color Space"
        menuItemPosition: 1
        menuModelName: moreMenu.menu_model_name
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 2
        menuModelName: moreMenu.menu_model_name
    }

    XsMenuModelItem {
        text: "Rename..."
        enabled: false
        menuPath: ""
        menuItemPosition: 3
        menuModelName: moreMenu.menu_model_name
        onActivated: {}
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 4
        menuModelName: moreMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Copy Selected Layer"
        enabled: hasActiveGrade()
        menuPath: ""
        menuItemPosition: 5
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            attrs.grading_action = "Copy Layer"
        }
    }
    XsMenuModelItem {
        text: "Copy All Layers"
        enabled: bookmarkList.count > 0
        menuPath: ""
        menuItemPosition: 5.5
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            attrs.grading_action = "Copy All Layer"
        }
    }
    XsMenuModelItem {
        text: "Paste Layers"
        enabled: attrs.grade_copying
        menuPath: ""
        menuItemPosition: 6
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            attrs.grading_action = "Paste Layer"
        }
    }
    XsMenuModelItem {
        text: "Reset"
        enabled: hasActiveGrade()
        menuPath: ""
        menuItemPosition: 6.5
        menuModelName: moreMenu.menu_model_name
        onActivated: {
                attrs.grading_action = "Clear"
        }
    }
    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 7
        menuModelName: moreMenu.menu_model_name
    }
    XsMenuModelItem {
        text: "Copy Nuke Node"
        menuPath: ""
        menuItemPosition: 8
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            copyNukeNode();
        }
    }

    function saveCDL(fileUrl, folder) {
        var path = fileUrl.toString()
        if (!path.endsWith(".cdl") && !path.endsWith(".cc") && !path.endsWith(".ccc")) {
            path += ".cdl"
        }
        attrs.grading_action = "Save CDL " + path
    }

    XsMenuModelItem {
        text: "Save CDL..."
        menuPath: ""
        menuItemPosition: 9
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            dialogHelpers.showFileDialog(
                menuDiv.saveCDL,
                file_functions.defaultSessionFolder(),
                "Save CDL",
                "cdl",
                [ "CDL files (*.cdl)", "CC files (*.cc)", "CCC files (*.ccc)" ],
                false,
                false
            )
        }
    }

    function copyNukeNode() {

        // TODO: ColSci
        // Use Grade node instead of OCIOCDLTransform to handle contrast?

        var offset = attrs.getAttrValue("Offset")
        var power = attrs.getAttrValue("Power")
        var slope = attrs.getAttrValue("Slope")
        var sat = attrs.getAttrValue("Saturation")
        var exp = attrs.getAttrValue("Exposure")
        var cont = attrs.getAttrValue("Contrast")

        var cdl_node = "OCIOCDLTransform {\n"
        if (attrs.colour_space != "scene_linear") {
            cdl_node += "  working_space " + attrs.colour_space + "\n"
        }
        cdl_node += "  slope { "
        cdl_node += (slope[0] * slope[3] * Math.pow(2.0, exp)) + " "
        cdl_node += (slope[1] * slope[3] * Math.pow(2.0, exp)) + " "
        cdl_node += (slope[2] * slope[3] * Math.pow(2.0, exp)) + " "
        cdl_node += "}\n"
        cdl_node += "  offset { "
        cdl_node += (offset[0] + offset[3]) + " "
        cdl_node += (offset[1] + offset[3]) + " "
        cdl_node += (offset[2] + offset[3]) + " "
        cdl_node += "}\n"
        cdl_node += "  power { "
        cdl_node += (power[0] * power[3]) + " "
        cdl_node += (power[1] * power[3]) + " "
        cdl_node += (power[2] * power[3]) + " "
        cdl_node += "}\n"
        cdl_node += "  saturation " + sat + "\n"
        cdl_node += "}"

        clipboard.text = cdl_node
    }


}
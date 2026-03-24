// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

import xStudio 1.0
import xstudio.qml.bookmarks 1.0
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
        text: "Export as Nuke Nodes to Clipboard"
        enabled: hasActiveGrade()
        menuPath: ""
        menuItemPosition: 8
        menuModelName: moreMenu.menu_model_name
        onActivated: {
            copyNukeNodes();
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
        text: "Export as CDL file..."
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

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 10
        menuModelName: moreMenu.menu_model_name
    }

    function saveSelectedCDLs(folder) {
        attrs.grading_action = "Save Selected CDLs " + folder
    }

    XsMenuModelItem {
        text: "Selected CDLs..."
        menuPath: "File|Export"
        menuItemPosition: 1
        menuModelName: "main menu bar"
        onActivated: {
            dialogHelpers.showFolderDialog(
                menuDiv.saveSelectedCDLs,
                file_functions.defaultSessionFolder(),
                "Save Selected Medias CDLs",
            )
        }
        Component.onCompleted: {
            setMenuPathPosition("File|Export", 7.7)
        }
    }

    function copyNukeNodes() {

        var offset = attrs.getAttrValue("Offset")
        var power = attrs.getAttrValue("Power")
        var slope = attrs.getAttrValue("Slope")
        var sat = attrs.getAttrValue("Saturation")
        var exp = attrs.getAttrValue("Exposure")
        var cont = attrs.getAttrValue("Contrast")

        var nuke_nodes = "";

        nuke_nodes += "EXPTool {\n"
        nuke_nodes += "  mode Stops\n"
        nuke_nodes += "  red " + exp + "\n"
        nuke_nodes += "  green " + exp + "\n"
        nuke_nodes += "  blue " + exp + "\n"
        nuke_nodes += "}\n"

        nuke_nodes += "Contrast {\n"
        nuke_nodes += "  colorValue " + cont + "\n"
        nuke_nodes += "}\n"

        nuke_nodes += "OCIOCDLTransform {\n"
        if (attrs.colour_space != "scene_linear") {
            nuke_nodes += "  working_space " + attrs.colour_space + "\n"
        }
        nuke_nodes += "  slope { "
        nuke_nodes += (slope[0] * slope[3]) + " "
        nuke_nodes += (slope[1] * slope[3]) + " "
        nuke_nodes += (slope[2] * slope[3]) + " "
        nuke_nodes += "}\n"
        nuke_nodes += "  offset { "
        nuke_nodes += (offset[0] + offset[3]) + " "
        nuke_nodes += (offset[1] + offset[3]) + " "
        nuke_nodes += (offset[2] + offset[3]) + " "
        nuke_nodes += "}\n"
        nuke_nodes += "  power { "
        nuke_nodes += (power[0] * power[3]) + " "
        nuke_nodes += (power[1] * power[3]) + " "
        nuke_nodes += (power[2] * power[3]) + " "
        nuke_nodes += "}\n"
        nuke_nodes += "  saturation " + sat + "\n"
        nuke_nodes += "}"

        clipboard.text = nuke_nodes
    }


}
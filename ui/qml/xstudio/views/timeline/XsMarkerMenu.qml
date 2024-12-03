import xstudio.qml.models 1.0
import xStudio 1.0
import QtQml.Models 2.14

XsPopupMenu {

    id: markerMenu
    visible: false
    menu_model_name: "marker_menu_"+markerMenu

    property var panelContext: helpers.contextPanel(timelineMenu)
    property var theTimeline: panelContext.theTimeline
    property var markerIndex: null

    function setName(new_name, button) {
        if (button == "Rename") {
            markerIndex.model.set(markerIndex, new_name, "nameRole")
        }
    }

    function setComment(new_name, button) {
        if (button == "Set") {
            markerIndex.model.set(markerIndex, JSON.parse(new_name), "commentRole")
        }
    }

   XsMenuModelItem {
        text: qsTr("Set Name")
        menuPath: ""
        menuItemPosition: 1
        menuModelName: markerMenu.menu_model_name
        onActivated: {
            dialogHelpers.textInputDialog(
                markerMenu.setName,
                "Set Marker Name",
                "Enter a name.",
                markerIndex.model.get(markerIndex, "nameRole"),
                ["Cancel", "Rename"])
        }
    }

    XsMenuModelItem {
        text: qsTr("Set Comment")
        menuPath: ""
        menuItemPosition: 2
        menuModelName: markerMenu.menu_model_name
        onActivated: {
            dialogHelpers.textAreaInputDialog(
                markerMenu.setName,
                "Set Marker Comment",
                "Enter a comment.",
                JSON.stringify(markerIndex.model.get(markerIndex, "commentRole"), null, 2),
                ["Cancel", "Set"])
        }
    }

     XsFlagMenuInserter {
        text: qsTr("Marker Colour")
        menuModelName: markerMenu.menu_model_name
        menuPath: ""
        menuPosition: 3
        onFlagSet: markerIndex.model.set(markerIndex, flag == "#00000000" ? "": flag, "flagRole")
        panelContext: markerMenu.panelContext
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 9
        menuModelName: markerMenu.menu_model_name
    }

   XsMenuModelItem {
        text: qsTr("Remove Marker")
        menuPath: ""
        menuItemPosition: 10
        menuModelName: markerMenu.menu_model_name
        onActivated: markerIndex.model.removeRows(markerIndex.row, 1, markerIndex.parent)
    }
}
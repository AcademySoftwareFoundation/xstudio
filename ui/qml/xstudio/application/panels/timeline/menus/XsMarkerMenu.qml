import xstudio.qml.models 1.0
import xStudio 1.0


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
            try {
                var a = JSON.parse(new_name);
                markerIndex.model.set(markerIndex, a, "commentRole")
            } catch (e) {
                markerIndex.model.set(markerIndex, new_name, "commentRole")
            }
            
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
            var str
            try {
                str = JSON.stringify(markerIndex.model.get(markerIndex, "commentRole"), null, 2)
            } catch (e) {
                str = "" + markerIndex.model.get(markerIndex, "commentRole")
            }
            dialogHelpers.textAreaInputDialog(
                markerMenu.setComment,
                "Set Marker Comment",
                "Enter a comment.",
                str,
                ["Cancel", "Set"])
        }
    }

     XsFlagMenuInserter {
        text: qsTr("Marker Colour")
        menuModelName: markerMenu.menu_model_name
        menuPath: ""
        menuPosition: 3
        onFlagSet: (flag, flag_text) => {
            markerIndex.model.set(markerIndex, flag == "#00000000" ? "": flag, "flagRole")
        }
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
import xstudio.qml.models 1.0
import xStudioReskin 1.0

XsMenuNew {

    id: timelineMenu
    menu_model: timelineMenuModel
    menu_model_index: timelineMenuModel.index(-1, -1)

    property var timelinePanel

    XsMenusModel {
        id: timelineMenuModel

        // N.B. appending 'timelineMenu' means we have a unique menu model for each 
        // instance of the XsTimelineMenu. This is important because we could
        // have multiple timeline instances, each with its own XsTimelineMenu...
        // Menu events like 'activated' are passed through the backend model
        // and therefore when they come back up to the UI layer, if we didn't
        // have unique models per timeline panel, the 'onActivated' would be
        // trigged on multiple instances of the correspondin XsMenuModelItem.
        modelDataName: "timelineMenu" + timelineMenu 
        onJsonChanged: {
            timelineMenu.menu_model_index = index(-1, -1)
        }
    }

    XsMenuModelItem {
        text: qsTr("Set Focus")
        menuPath: ""
        menuItemPosition: 1
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            timelineFocusSelection.select(helpers.createItemSelection(timelineSelection.selectedIndexes), ItemSelectionModel.ClearAndSelect)
        }
    }

    XsMenuModelItem {
        text: qsTr("Clear Focus")
        menuPath: ""
        menuItemPosition: 2
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            timelineFocusSelection.clear()
        }
    }
    
    XsMenuModelItem {
        text: qsTr("Move Left")
        menuPath: ""
        menuItemPosition: 3
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            if(timelineSelection.selectedIndexes.length) {
                timelinePanel.moveItem(timelineSelection.selectedIndexes[0], -1)
            }
        }
    }

    XsMenuModelItem {
        text: qsTr("Move Right")
        menuPath: ""
        menuItemPosition: 4
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            if(timelineSelection.selectedIndexes.length) {
                timelinePanel.moveItem(timelineSelection.selectedIndexes[0], 1)
            }
        }
    }

    XsMenuModelItem {
        text: qsTr("Jump to Start")
        menuPath: ""
        menuItemPosition: 4
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.jumpToStart()
    }

    XsMenuModelItem {
        text: qsTr("Jump to End")
        menuPath: ""
        menuItemPosition: 4
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.jumpToEnd()
    }

    XsMenuModelItem {
        text: qsTr("Align Left")
        menuPath: ""
        menuItemPosition: 5
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.leftAlignItems(timelineSelection.selectedIndexes)
    }

    XsMenuModelItem {
        text: qsTr("Align Right")
        menuPath: ""
        menuItemPosition: 6
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.rightAlignItems(timelineSelection.selectedIndexes)
    }

    XsMenuModelItem {
        text: qsTr("Move Range")
        menuPath: ""
        menuItemPosition: 7
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.moveItemFrames(timelineSelection.selectedIndexes[0], 0, 20, 40, true)
    }

    XsMenuModelItem {
        text: qsTr("Delete")
        menuPath: ""
        menuItemPosition: 8
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.deleteItems(timelineSelection.selectedIndexes)
    }

    XsMenuModelItem {
        text: qsTr("Delete Range")
        menuPath: ""
        menuItemPosition: 9
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.deleteItemFrames(timelineSelection.selectedIndexes[0], 10, 20)
    }

    XsMenuModelItem {
        text: qsTr("Undo")
        menuPath: ""
        menuItemPosition: 10
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.undo(viewedMediaSetProperties.index)
    }

    XsMenuModelItem {
        text: qsTr("Redo")
        menuPath: ""
        menuItemPosition: 11
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.redo(viewedMediaSetProperties.index)
    }

    XsMenuModelItem {
        text: qsTr("Enable")
        menuPath: ""
        menuItemPosition: 12
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.enableItems(timelineSelection.selectedIndexes, true)
    }

    XsMenuModelItem {
        text: qsTr("Disable")
        menuPath: ""
        menuItemPosition: 12
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.enableItems(timelineSelection.selectedIndexes, false)
    }

    XsMenuModelItem {
        text: qsTr("Add Media")
        menuPath: ""
        menuItemPosition: 13
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.addClip(
                timelineSelection.selectedIndexes[0].parent, timelineSelection.selectedIndexes[0].row,
                viewedMediaSetIndex
            )
    }

    XsMenuModelItem {
        text: qsTr("Add Gap")
        menuPath: ""
        menuItemPosition: 14
        menuModelName: timelineMenuModel.modelDataName
        onActivated: timelinePanel.addGap(timelineSelection.selectedIndexes[0].parent, timelineSelection.selectedIndexes[0].row)
    }

    XsMenuModelItem {
        text: qsTr("Split")
        menuPath: ""
        menuItemPosition: 15
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            if(timelineSelection.selectedIndexes.length) {
                let index = timelineSelection.selectedIndexes[0]
                timelinePanel.splitClip(index, theSessionData.get(index, "trimmedStartRole") + (theSessionData.get(index, "trimmedDurationRole") /2))
            }
        }
    }

    XsMenuModelItem {
        text: qsTr("Duplicate")
        menuPath: ""
        menuItemPosition: 16
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            let indexes = timelineSelection.selectedIndexes
            for(let i=0;i<indexes.length; i++) {
                theSessionData.duplicateRows(indexes[i].row, 1, indexes[i].parent)
            }
        }
    }

    XsMenuModelItem {
        text: qsTr("Change Name")
        menuPath: ""
        menuItemPosition: 16
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            let indexes = timelineSelection.selectedIndexes
            for(let i=0;i<indexes.length; i++) {
                set_name_dialog.index = indexes[i]
                set_name_dialog.text = theSessionData.get(indexes[i], "nameRole")
                set_name_dialog.open()
            }
        }
    }

    XsMenuModelItem {
        text: qsTr("Add Item")
        menuPath: ""
        menuItemPosition: 17
        menuModelName: timelineMenuModel.modelDataName
        onActivated: {
            if(timelineSelection.selectedIndexes.length) {
                new_item_dialog.insertion_parent = timelineSelection.selectedIndexes[0].parent
                new_item_dialog.insertion_row = timelineSelection.selectedIndexes[0].row
            }
            else {
                new_item_dialog.insertion_parent = viewedMediaSetProperties.index
                new_item_dialog.insertion_row = 0
            }
            new_item_dialog.open()
        }
    }
}
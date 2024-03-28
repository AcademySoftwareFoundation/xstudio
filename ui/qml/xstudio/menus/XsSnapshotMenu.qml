// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQml 2.14
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import xstudio.qml.models 1.0


XsMenu {
    id: snapshotMenu
    title: "Snapshots"

    XsSnapshotModel {
    	id: snapshotModel
    	paths: preferences.snapshot_paths.value
    	onModelReset: snapshot_items.rootIndex = index(-1,-1)
    }

    XsNewSnapshotDialog {
    	id: snapshot_path_dialog
    	onOkayed: {
    		if(text.length && path.length) {
    			let v = preferences.snapshot_paths.value
    			v.push({'path': path, "name":text})
    			preferences.snapshot_paths.value = v

                text = ""
                path = ""
    		}
    	}
    }

	onAboutToShow: snapshotModel.rescan(snapshot_items.rootIndex, 1)

    DelegateChooser {
        id: chooser
        role: "typeRole"

		DelegateChoice {
		    roleValue: "DIRECTORY"

		    XsSnapshotDirectoryMenu {
	    		itemType: typeRole
	    		title: nameRole
				rootIndex: snapshot_items.srcModel.index(index, 0, snapshot_items.rootIndex)
				snapshotModel: snapshot_items.srcModel
		    }
		}

   	    DelegateChoice {
		    roleValue: "FILE"

	    	XsMenuItem {
	    		property string itemType: typeRole
	    		mytext: nameRole
	    	}
		}
    }

    DelegateModel {
        id: snapshot_items
        property var srcModel: snapshotModel
        model: srcModel
        rootIndex: null
        delegate: chooser
    }

    Instantiator {
    	model: snapshot_items
    	onObjectAdded: {
    		if(object.itemType == "DIRECTORY")
	    		snapshotMenu.insertMenu(index,object)
			else
	    		snapshotMenu.insertItem(index,object)
    	}
    	onObjectRemoved: {
    		if(object.itemType == "DIRECTORY")
    			snapshotMenu.removeMenu(object)
    		else
	    		snapshotMenu.removeItem(object)
    	}
    }


    XsMenuSeparator {
        visible: true
    }

	XsMenuItem {
		mytext: "Select Folder..."
		onTriggered: snapshot_path_dialog.open()
    }
}
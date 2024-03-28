// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQml 2.14
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.models 1.0

XsMenu {
	id: control

	property var snapshotModel: null
	property var rootIndex: null
	property string itemType: typeRole
	title: nameRole

    function createSelf(parent, myModel, myRootIndex, myType, myTitle) {
        let comp = Qt.createComponent("XsSnapshotDirectoryMenu.qml").createObject(parent, {
        		snapshotModel: myModel, rootIndex: myRootIndex, itemType:myType,
        		title: myTitle
        	})

        if (comp == null) {
            console.log("Error creating object");
        }
        return comp;
    }

    XsStringRequestDialog {
        id: add_folder
        okay_text: "Add"
        title: "Add Folder"

        onOkayed: snapshotModel.createFolder(rootIndex, text)
    }

    XsStringRequestDialog {
        id: save_snapshot
        okay_text: "Save"
        title: "Save Snapshot"

        onOkayed: {
        	let path = snapshotModel.buildSavePath(rootIndex, text)
	        app_window.sessionFunction.newRecentPath(path)
	        app_window.sessionFunction.saveSessionPath(path).then(function(result){
	            if (result != "") {
	                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
	                dialog.title = "Save session failed"
	                dialog.text = result
	                dialog.show()
	            } else {
	                app_window.sessionFunction.newRecentPath(path)
	                app_window.sessionFunction.copySessionLink(false)
	                snapshotModel.rescan(rootIndex, 0);
	            }
	        })
        }
    }

    DelegateChooser {
        id: chooser
        role: "typeRole"

		DelegateChoice {
		    roleValue: "DIRECTORY"

			Item {
				id: menu_holder
	    		property string itemType: typeRole
	    		property var item: null

				Component.onCompleted: {
					item = createSelf(menu_holder, control.snapshotModel, control.snapshotModel.index(index, 0, control.rootIndex), typeRole, nameRole)
				}
		    }
		}

   	    DelegateChoice {
		    roleValue: "FILE"

	    	XsMenuItem {
	    		property string itemType: typeRole
	    		mytext: nameRole
	    		onTriggered: Future.promise(studio.loadSessionRequestFuture(pathRole)).then(function(result){})
	    	}
		}
    }

    DelegateModel {
        id: snapshot_items
        property var srcModel: control.snapshotModel
        model: srcModel
        rootIndex: control.rootIndex
        delegate: chooser
    }

    onAboutToShow: {
    	control.snapshotModel.rescan(control.rootIndex,1)
    	if(builder.model != null)
    		builder.model = snapshot_items
    }

    Instantiator {
    	id: builder
    	model: []
    	onObjectAdded: {
    		if(object.itemType == "DIRECTORY")
	    		control.insertMenu(index, object.item)
			else
	    		control.insertItem(index, object)
    	}
    	onObjectRemoved: {
    		if(object.itemType == "DIRECTORY")
    			control.removeMenu(object.item)
    		else
	    		control.removeItem(object)
    	}
    }

    XsMenuSeparator {
        visible: true
    }

	XsMenuItem {
		mytext: "Add Folder..."
		onTriggered: add_folder.open()
    }
	XsMenuItem {
		mytext: "Save Snapshot..."
		onTriggered: save_snapshot.open()
    }
	XsMenuItem {
		mytext: "Remove "+control.snapshotModel.get(rootIndex, "nameRole")+" Menu"
		visible: !rootIndex.parent.valid
		onTriggered: {
			let v = preferences.snapshot_paths.value
			let new_v = []
			let ppath = control.snapshotModel.get(rootIndex, "pathRole")

			for(let i =0; i< v.length;i++) {
				if(ppath != v[i].path)
					new_v.push(v[i])
			}

			preferences.snapshot_paths.value = new_v
		}
    }
}


// SPDX-License-Identifier: Apache-2.0
import xstudio.qml.uuid 1.0

import xStudio 1.0

XsStringRequestDialog {
    property var uuid: null_uuid.asQuuid
    property var playlist: null
    property bool insert_new: false


    okay_text: "Add Divider"
    text: "Untitled Divider"

    y: playlist_panel.mapToGlobal(0, 25).y
    centerOn: playlist_panel

    function newItem(parent, obj, value) {
        if(obj.selected) {
            if(parent.createDivider) {
                if(!insert_new) {
                    parent.createDivider(value, null_uuid.asQuuid);
                } else {
                    obj = parent.itemModel.next_object(obj)
                    if(obj) {
                        parent.createDivider(value, obj.cuuid);
                    } else {
                        parent.createDivider(value, null_uuid.asQuuid);
                    }
                }
                return true
            } else if(obj.createDivider) {
                obj.createDivider(value, null_uuid.asQuuid);
                return true
            }
        }
        return false
    }

    onOkayed: {
        var pobj = playlist ? playlist : app_window.session
        if(uuid != null_uuid.asQuuid || !XsUtils.forAllItems(pobj, null, newItem, text)) {
            pobj.createDivider(text, uuid);
        }
        pobj.expanded = true;
    }
    QMLUuid {
        id: null_uuid
    }
}

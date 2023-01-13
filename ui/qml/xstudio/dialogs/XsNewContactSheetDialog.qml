// SPDX-License-Identifier: Apache-2.0
import xstudio.qml.uuid 1.0

import xStudio 1.0

XsStringRequestDialog {
    property var uuid: null_uuid.asQuuid
    property bool insert_new: false
    property var playlist: null
    signal created(QMLUuid uuid)

    okay_text: "Add Contact Sheet"
    text: "Untitled Contact Sheet"

    y: playlist_panel.mapToGlobal(0, 25).y
    centerOn: playlist_panel

    function newItem(parent, obj, value) {
        if(obj.selected) {
            if(parent.createContactSheet) {
                if(!insert_new) {
                    result_uuid.setFromQuuid(parent.createContactSheet(value, null_uuid.asQuuid))
                } else {
                    obj = parent.itemModel.next_object(obj)
                    if(obj) {
                        result_uuid.setFromQuuid(parent.createContactSheet(value, obj.cuuid))
                    } else {
                        result_uuid.setFromQuuid(parent.createContactSheet(value, null_uuid.asQuuid))
                    }
                }
                created(result_uuid)
                return true
            } else if(obj.createContactSheet) {
                result_uuid.setFromQuuid(obj.createContactSheet(value, null_uuid.asQuuid))
                created(result_uuid)
                return true
            }
        }
        return false
    }

    onOkayed: {
        var pobj = playlist ? playlist : app_window.session
        if(uuid != null_uuid.asQuuid || !XsUtils.forAllItems(pobj, null, newItem, text)) {
            result_uuid.setFromQuuid(pobj.createContactSheet(text, uuid))
            created(result_uuid)
        }
        pobj.expanded = true;
    }
    QMLUuid {
        id: null_uuid
    }
    QMLUuid {
        id: result_uuid
    }
}

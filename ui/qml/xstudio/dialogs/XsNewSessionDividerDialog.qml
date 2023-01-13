// SPDX-License-Identifier: Apache-2.0
import xstudio.qml.uuid 1.0

import xStudio 1.0

XsStringRequestDialog {
    property var uuid: null_uuid.asQuuid
    property bool insert_new: false
    property bool dated: false
    okay_text: "Add Divider"
    text: dated ?  XsUtils.yyyymmdd("-") : "Untitled Divider"

    y: playlist_panel.mapToGlobal(0, 25).y
    centerOn: playlist_panel

   function newItem(parent, obj, value) {
        if(obj.selected && parent.createDivider && parent == app_window.session) {
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
        }
        return false
    }

    onOkayed: {
        if(uuid != null_uuid.asQuuid || !XsUtils.forAllItems(app_window.session, null, newItem, text)) {
            session.createDivider(text, uuid);
        }
        session.expanded = true;
    }
    QMLUuid {
        id: null_uuid
    }
}

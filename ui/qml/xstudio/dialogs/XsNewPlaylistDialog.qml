// SPDX-License-Identifier: Apache-2.0
import xstudio.qml.uuid 1.0

import xStudio 1.0

XsStringRequestDialog {
    property var uuid: null_uuid.asQuuid
    property bool insert_new: false
    signal created(QMLUuid uuid)
    signal created_secondary(QMLUuid uuid)
    okay_text: "Add Playlist"
    text: "Untitled Playlist"

    y: playlist_panel.mapToGlobal(0, 25).y
    centerOn: playlist_panel

    function newItem(parent, obj, value) {
        if(!insert_new) {
            result_uuid.setFromQuuid(parent.createPlaylist(value, null_uuid.asQuuid))
            created(result_uuid)
            return true
        } else if(obj.selected && parent.createPlaylist) {
            obj = parent.itemModel.next_object(obj)
            if(obj) {
                result_uuid.setFromQuuid(parent.createPlaylist(value, obj.cuuid))
            } else {
                result_uuid.setFromQuuid(parent.createPlaylist(value, null_uuid.asQuuid))
            }
            created(result_uuid)
            return true
        }
        return false
    }

    function newItem_secondary(parent, obj, value) {
        if(!insert_new) {
            result_uuid.setFromQuuid(parent.createPlaylist(value, null_uuid.asQuuid))
            created_secondary(result_uuid)
            return true
        } else if(obj.selected && parent.createPlaylist) {
            obj = parent.itemModel.next_object(obj)
            if(obj) {
                result_uuid.setFromQuuid(parent.createPlaylist(value, obj.cuuid))
            } else {
                result_uuid.setFromQuuid(parent.createPlaylist(value, null_uuid.asQuuid))
            }
            created_secondary(result_uuid)
            return true
        }
        return false
    }

    onOkayed: {
        if(uuid != null_uuid.asQuuid || !XsUtils.forFirstItem(app_window.session, null, newItem, text)) {
        	result_uuid.setFromQuuid(session.createPlaylist(text, uuid))
            created(result_uuid)
        }
    	session.expanded = true;
    }

    onSecondary_okayed:{
        if(uuid != null_uuid.asQuuid || !XsUtils.forFirstItem(app_window.session, null, newItem_secondary, text)) {
            result_uuid.setFromQuuid(session.createPlaylist(text, uuid))
            created_secondary(result_uuid)
        }
        session.expanded = true;
    }

    QMLUuid {
        id: null_uuid
    }
    QMLUuid {
        id: result_uuid
    }
}

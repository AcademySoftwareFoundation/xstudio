// SPDX-License-Identifier: Apache-2.0
import xstudio.qml.uuid 1.0

import xStudio 1.0

XsStringRequestDialog {
    property var uuid: null_uuid.asQuuid
    property var playlist: null

    y: playlist_panel.mapToGlobal(0, 25).y
    centerOn: playlist_panel.mapToGlobal(0, 0)

    okay_text: "Add Timeline"
    text: "Untitled Timeline"

    function newItem(parent, obj, value) {
        if(obj.selected) {
            if(parent.createTimeline) {
                obj = parent.itemModel.next_object(obj)
                if(obj) {
                    parent.createTimeline(value, obj.cuuid);
                } else {
                    parent.createTimeline(value, null_uuid.asQuuid);
                }
                return true
            } else if(obj.createTimeline) {
                obj.createTimeline(value, null_uuid.asQuuid);
                return true
            }
        }
        return false
    }

    onOkayed: {
        var pobj = playlist ? playlist : app_window.session
        if(uuid != null_uuid.asQuuid || !XsUtils.forAllItems(pobj, null, newItem, text)) {
            pobj.createTimeline(text, uuid);
        }
        pobj.expanded = true;
    }
    QMLUuid {
        id: null_uuid
    }
}

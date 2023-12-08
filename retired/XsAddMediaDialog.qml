// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

FileDialog {
    id: media_dialog
    title: "Select media files"
    folder: session.pathNative ? XsUtils.stem(session.path.toString()).replace("localhost","") : shortcuts.home

    nameFilters:  [ "Media files ("+helpers.validMediaExtensions()+")", "All files (*)" ]
    selectExisting: true
    selectMultiple: true

    function newItem(parent, obj, value) {
        if(obj.selected && obj.loadMedia) {
            Future.promise(obj.loadMediaFuture(value)
            ).then(function(quuids){})
            return true
        }
        return false
    }

    onAccepted: {
        // check for empty session..
        if(app_window.session.itemModel.empty()) {
            app_window.session.createPlaylist()
        }
        for (var i = 0; i < media_dialog.fileUrls.length; i++) {
            XsUtils.forFirstItem(app_window.session, null, newItem, media_dialog.fileUrls[i])
        }
    }
    onRejected: {
    }
}

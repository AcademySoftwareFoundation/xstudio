// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

FileDialog {
    title: "Import session"
    folder: app_window.sessionFunction.defaultSessionFolder() || shortcuts.home
    defaultSuffix: "xst"

    nameFilters:  ["xStudio (*.xst *.xsz)"]
    selectExisting: true
    selectMultiple: false
    onAccepted: {
        Future.promise(app_window.sessionModel.importFuture(fileUrl, null)).then(function(result){})
        app_window.sessionFunction.newRecentPath(fileUrl)
    }
    onRejected: {
    }
}

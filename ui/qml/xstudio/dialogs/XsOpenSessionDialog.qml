// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

FileDialog {
    title: "Open Session"
    folder: app_window.sessionFunction.defaultSessionFolder() || shortcuts.home
    defaultSuffix: "xst"

    nameFilters:  ["xStudio (*.xst *.xsz)"]
    selectExisting: true
    selectMultiple: false
    onAccepted: {
        Future.promise(studio.loadSessionFuture(fileUrl)).then(
            function(result){
                // console.log(result)
            }
        )
        app_window.sessionFunction.newRecentPath(fileUrl)
        app_window.sessionFunction.defaultSessionFolder(path.slice(0, path.lastIndexOf("/") + 1))
    }
    onRejected: {
    }
}

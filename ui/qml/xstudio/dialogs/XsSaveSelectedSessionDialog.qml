// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.3

import xStudio 1.0

//  QML url is stupid..

FileDialog {
    title: "Save selected as session"
    folder: app_window.sessionFunction.defaultSessionFolder() || shortcuts.home
    defaultSuffix: preferences.session_compression.value ? "xsz" : "xst"

    signal saved
    signal cancelled

    nameFilters:  ["xStudio (*.xst *.xsz)"]
    selectExisting: false
    selectMultiple: false

    onAccepted: {
        // check for extension.
        var path = fileUrl.toString()
        var ext = path.split('.').pop()
        if(path == ext) {
            path = path + (preferences.session_compression.value ? ".xsz" : ".xst")
        }
        app_window.sessionFunction.saveSelectedSession(path).then(function(result){
            if (result != "") {
                var dialog = XsUtils.openDialog("qrc:/dialogs/XsErrorMessage.qml")
                dialog.title = "Save selected session failed"
                dialog.text = result
                dialog.show()
                cancelled()
            } else {
                saved()
                app_window.sessionFunction.newRecentPath(path)
            }
        })
    }

    onRejected: {
        cancelled()
    }
}


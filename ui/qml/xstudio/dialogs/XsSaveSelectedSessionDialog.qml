// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.3

import xStudio 1.0

//  QML url is stupid..

FileDialog {
    title: "Save selected as session"
    folder: session.pathNative ? XsUtils.stem(session.path.toString()).replace("localhost","") : shortcuts.home
    defaultSuffix: "xst"

    signal saved
    signal cancelled

    nameFilters:  ["XStudio (*.xst)"]
    selectExisting: false
    selectMultiple: false

    onAccepted: {
        // check for extension.
        var path = fileUrl.toString()
        var ext = path.split('.').pop()
        if(path == ext) {
            path = path + ".xst"
        }
        session.save_selected_session(path)
        saved()
    }

    onRejected: {
        cancelled()
    }
}


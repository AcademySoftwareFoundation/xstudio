// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.0

import xStudio 1.0

FileDialog {
    title: "Open session"
    folder: session.pathNative ? XsUtils.stem(session.path.toString()).replace("localhost","") : shortcuts.home
    defaultSuffix: "xst"

    nameFilters:  ["Xstudio (*.xst)"]
    selectExisting: true
    selectMultiple: false
    onAccepted: {
        session.load(fileUrl)
        app_window.session.new_recent_path(fileUrl)
    }
    onRejected: {
    }
}

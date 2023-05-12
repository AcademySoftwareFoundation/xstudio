// SPDX-License-Identifier: Apache-2.0
import QtQuick.Dialogs 1.3

import xStudio 1.0

//  QML url is stupid..

FileDialog {
    title: "Save session"
    folder: getFolderPath()
    defaultSuffix: "xst"

    signal saved
    signal cancelled

    nameFilters:  ["XStudio (*.xst)"]
    selectExisting: false
    selectMultiple: false

    function getFolderPath() {
        if(preferences.current_saved_session_folder.value != "")
            return preferences.current_saved_session_folder.value

        return session.pathNative ? XsUtils.stem(session.path.toString()).replace("localhost","") : shortcuts.home
    }

    onAccepted: {
        // check for extension.
        var path = fileUrl.toString()
        var ext = path.split('.').pop()
        if(path == ext) {
            path = path + ".xst"
        }

        app_window.session.new_recent_path(path)
        session.save_session_path(path)
        app_window.session.copy_session_link(false)
        saved()

        preferences.current_saved_session_folder.value = path.slice(0, path.lastIndexOf("/") + 1)
    }

    onRejected: {
        cancelled()
    }
}


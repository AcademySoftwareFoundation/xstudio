// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.1


XsButtonDialog {
    text: "Selected Media"
    width: 300
    buttonModel: ["Cancel", "Move", "Copy"]
    property var data: null
    property var index: null

    onSelected: {
        if(button_index == 1) {
            // is selection still valid ?
            let items = XsUtils.cloneArray(app_window.mediaSelectionModel.selectedIndexes).sort((a,b) => b.row - a.row )
            app_window.sessionFunction.setActiveMedia(app_window.sessionFunction.mediaIndexAfterRemoved(items))
            if(index == null)
                Future.promise(
                    app_window.sessionModel.handleDropFuture(Qt.MoveAction, data)
                ).then(function(quuids){})
            else
                Future.promise(
                    app_window.sessionModel.handleDropFuture(Qt.MoveAction, data, index)
                ).then(function(quuids){})

        } else if(button_index == 2) {
            if(index == null)
                Future.promise(
                    app_window.sessionModel.handleDropFuture(Qt.CopyAction, data)
                ).then(function(quuids){})
            else
                Future.promise(
                    app_window.sessionModel.handleDropFuture(Qt.CopyAction, data, index)
                ).then(function(quuids){})
        }
    }
}

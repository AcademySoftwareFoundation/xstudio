// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

import xStudio 1.0
import ShotBrowser 1.0
import QuickFuture 1.0
import QuickPromise 1.0
import xstudio.qml.helpers 1.0


RowLayout{
    spacing: 2
    property int parentWidth: width
    readonly property int cellWidth: parentWidth / children.length

    XsPrimaryButton{
        text: "Add"
        Layout.fillHeight: true
        Layout.preferredWidth: cellWidth
        onClicked: ShotBrowserHelpers.addToCurrent(resultsSelectionModel.selectedIndexes)
    }
    XsPrimaryButton{
        text: "Replace"
        Layout.fillHeight: true
        Layout.preferredWidth: cellWidth
        onClicked: ShotBrowserHelpers.replaceSelectedResults(resultsSelectionModel.selectedIndexes)
    }
    XsPrimaryButton{
        text: "Compare"
        Layout.fillHeight: true
        Layout.preferredWidth: cellWidth
        onClicked: ShotBrowserHelpers.compareSelectedResults(resultsSelectionModel.selectedIndexes)
    }
}

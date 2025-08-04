// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0

RowLayout {
    spacing: 2

    XsPrimaryButton{
        Layout.fillHeight: true
        Layout.fillWidth: true

        text: "   Add   "
        onClicked: {
            if(resultsBaseModel.groupDetail.flags.includes("Load Sequence"))
                ShotBrowserHelpers.addSequencesToNewPlaylist(resultsSelectionModel.selectedIndexes)
            else
                ShotBrowserHelpers.addToCurrent(resultsSelectionModel.selectedIndexes, false, addAfterSelection.value)
        }
    }

    XsPrimaryButton{
        Layout.fillHeight: true
        Layout.fillWidth: true

        text: "Replace"
        onClicked: ShotBrowserHelpers.replaceSelectedResults(resultsSelectionModel.selectedIndexes)
        enabled: ! resultsBaseModel.groupDetail.flags.includes("Load Sequence")

    }

    XsPrimaryButton{
        Layout.fillHeight: true
        Layout.fillWidth: true

        text: "Compare"
        enabled: ! resultsBaseModel.groupDetail.flags.includes("Load Sequence")
        onClicked: ShotBrowserHelpers.compareSelectedResults(resultsSelectionModel.selectedIndexes)
    }
}

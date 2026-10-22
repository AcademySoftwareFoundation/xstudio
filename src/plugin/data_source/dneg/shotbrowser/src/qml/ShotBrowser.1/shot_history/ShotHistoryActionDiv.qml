// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Layouts

import xStudio 1.0
import ShotBrowser 1.0
import QuickFuture 1.0
import QuickPromise 1.0
import xstudio.qml.helpers 1.0


RowLayout{
    spacing: 2
    property int parentWidth: width
    readonly property int cellWidth: parentWidth / children.length

    property bool targetIsTimeline: viewedMediaSetProperties.values.typeRole == "Timeline" && currentPlayhead.pinnedSourceMode

    XsPrimaryButton{
        text: targetIsTimeline ? "Add to Timeline" : "Add"
        Layout.fillHeight: true
        Layout.preferredWidth: cellWidth
        onClicked: ShotBrowserHelpers.addToCurrent(resultsSelectionModel.selectedIndexes, true, addMode.value)
        hiddenToolTip: targetIsTimeline ? "Load the selected media into new track(s) in the timeline - new clips will be conformed over the current on-screen clip where possible (otherwise new clips are added to a new 'Unconformed' video track)." : "Add selected search results to the current playlist."
        hiddenToolTipMaxWidth: 400
    }
    XsPrimaryButton{
        text: targetIsTimeline ? "Replace in Timeline" : "Replace"
        Layout.fillHeight: true
        Layout.preferredWidth: cellWidth
        onClicked: ShotBrowserHelpers.replaceSelectedResults(resultsSelectionModel.selectedIndexes)
        hiddenToolTip: targetIsTimeline ? "Load the selected into the timeline by REPLACING the current on-screen clip where possible (otherwise new clips are added to a new 'Unconformed' video track)." : "Replace the selected media in your current playlist with the selected search results above."
        hiddenToolTipMaxWidth: 400
    }
    XsPrimaryButton{
        enabled: !targetIsTimeline
        text: "Compare"
        Layout.fillHeight: true
        Layout.preferredWidth: cellWidth
        onClicked: ShotBrowserHelpers.compareSelectedResults(resultsSelectionModel.selectedIndexes)
    }
}

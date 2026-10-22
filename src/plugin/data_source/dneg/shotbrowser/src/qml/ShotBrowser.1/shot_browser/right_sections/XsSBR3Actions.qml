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

    property bool targetIsTimeline: viewedMediaSetProperties.values.typeRole == "Timeline" && currentPlayhead.pinnedSourceMode

    XsPrimaryButton {
        Layout.fillHeight: true
        Layout.fillWidth: true

        text: targetIsTimeline ? "Add to Timeline" : "Add"
        onClicked: {
            if(resultsBaseModel.groupDetail.flags.includes("Load Sequence"))
                ShotBrowserHelpers.addSequencesToNewPlaylist(resultsSelectionModel.selectedIndexes)
            else
                ShotBrowserHelpers.addToCurrent(resultsSelectionModel.selectedIndexes, false, addMode.value)
        }
        hiddenToolTip: targetIsTimeline ? "This option will look for shots in the timeline that match the shots for the media that you have selected from the above search results. Where a match is found the media will be cut-in to the timeline to match the cut-range of the corresponding clip(s) already in the timeline. If you are adding a SINGLE media item and it isn't matched to any shots in the timeline, it will be auto-conformed over the current on-screen clip (that is the clip under the current playhead). Otherwise any media being added that doesn't match any clips in the timeline will be added to an 'Uncoformed' video track." : "Add selected search results to the current playlist."
        hiddenToolTipMaxWidth: 400

    }

    XsPrimaryButton{
        Layout.fillHeight: true
        Layout.fillWidth: true

        text: targetIsTimeline ? "Replace in Timeline" : "Replace"
        onClicked: ShotBrowserHelpers.replaceSelectedResults(resultsSelectionModel.selectedIndexes)
        enabled: !resultsBaseModel.groupDetail.flags.includes("Load Sequence")
        hiddenToolTip: targetIsTimeline ? "This option will look for shots in the timeline that match the shots for the selected items in the search results above. Where a match is found the media will REPLACE the corresponding clip(s) already in the timeline. If you are adding a SINGLE media item and it isn't matched to any shots in the timeline, it will replace the current on-screen clip (that is the clip under the current playhead). Otherwise any media being added that doesn't match any clips in the timeline will be added to an 'Uncoformed' video track." : "Replace the selected media in your current playlist with the selected search results above."
        hiddenToolTipMaxWidth: 400

    }

    XsPrimaryButton{
        Layout.fillHeight: true
        Layout.fillWidth: true

        text: "Compare"
        enabled: !targetIsTimeline && !resultsBaseModel.groupDetail.flags.includes("Load Sequence")
        onClicked: ShotBrowserHelpers.compareSelectedResults(resultsSelectionModel.selectedIndexes)
    }
}

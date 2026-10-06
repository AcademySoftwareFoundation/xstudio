// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import xstudio.qml.viewport 1.0
import xstudio.qml.clipboard 1.0

import xStudio 1.0

Item {

    id: panel
    anchors.fill: parent

    property var viewedTimelineIndex: helpers.qModelIndex()
    property var timelines: [helpers.qModelIndex()]
    property bool multiTimelineMode: timelines.length > 1

    XsPreference {
        id: __show_clip_handles
        path: "/core/sequence/show_clip_handles"
    }
    property alias show_clip_handles: __show_clip_handles.value

    XsPreference {
        id: __show_edit_indicator
        path: "/core/sequence/show_edit_indicator"
    }
    property alias show_edit_indicator: __show_edit_indicator.value


    Clipboard {
      id: clipboard
    }

    property alias clipboard: clipboard

    function setTimelines() {
        // tricky logic implemented here. We only enter multi-timeline compare if
        // the current viewed container is a timeline, and also if the same container
        // is selected in the playlist panel, and also if more than one timeline
        // is selected. Otherwise we fall back to 'normal' mode where a single
        // timeline is displayed (the current or last viewed timeline)
        let stw = theSessionData.selectedTimelinesIndeces
        let newtimelines = [viewedTimelineIndex]
        let viewedTimlineIsSelected = stw.indexOf(viewedMediaSetProperties.index) != -1

        if (viewedTimlineIsSelected && stw.length > 1) {
            for (var i = 0; i < stw.length; ++i) {
                if (stw[i] == viewedMediaSetProperties.index) continue
                let timelineIndex = helpers.makePersistent(theSessionData.index(2, 0, stw[i]))
                if (theSessionData.get(stw[i], "typeRole") == "Timeline") {
                    newtimelines.push(stw[i])
                }
            }
        }

        var changed = newtimelines.length != timelines.length
        if (!changed) {
            // js can't compare these arrays. weird.
            for (var i = 0; i < newtimelines.length; ++i) {
                if (newtimelines[i] != timelines[i]) {
                    changed = true
                    break
                }
            }
        }
        if (changed) timelines = newtimelines
        
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 1
        Repeater {
            model: timelines
            XsTimelinePanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                property var timelineIndex: helpers.makePersistent(theSessionData.index(2, 0,timelines[index]))
                objectName: "XsTimelinePanel"
            }
        }
    }

    /*property var timelineIndex: helpers.qModelIndex()*/

    function viewedMediaSetChanged() {

        if(viewedMediaSetProperties.index.valid && viewedMediaSetProperties.values.typeRole == "Timeline") {

            viewedTimelineIndex = helpers.makePersistent(viewedMediaSetProperties.index)
            forceActiveFocus()
            getViewedTimelinePlayheadUuid(0)

        }
        setTimelines()

    }

    Component.onCompleted: {
        viewedMediaSetChanged()
    }

    Connections {
      target: viewedMediaSetProperties
      function onIndexChanged() {
        viewedMediaSetChanged()
      }
    }

    Connections {
        target: theSessionData
        function onSelectedTimelinesIndecesChanged() {
            setTimelines()
        }
    }
    
    /*XsModelProperty {
        // XsModelProperty is able to 'watch' an index and emits onIndexChanged
        // when the index goes invalid. We use this to watch whether the active
        // timeline is deleted - if so we run viewedMediaSetChanged which will
        // clear our model and index because otherwise Qt crashes as it seems
        // to hang on to items that have been deleted from the model.
        id: timelineProperty
        index: timelineIndex ? timelineIndex : helpers.qModelIndex()
        role: "propertyRole"
        onValueChanged: { viewedMediaSetChanged() }
        onIndexChanged: {
            if(!index || !index.valid) {
                // perhaps the timeline we were viewing has been deleted?
                // This will make sure the timeline UI will get cleared
                timelineIndex = helpers.qModelIndex()
            }
        }
    }*/

    // We need to enact some tricky logic here. If a timeline is selected for
    // playing in the viewport, then timelinePlayhead should be connected to 
    // the playhead of that timeline.
    // If something other than a timeline is selected for playing, but we 
    // already have loaded a timeline into the TimelineUI panel, then 
    // timelinePlayhead should remain connected to the playhead of the timeline.
    // If we are in multi-timeline compare mode (a timeline is selected for
    // viewing and there are also other timelines selected in the Playlists panel)
    // then timelinePlayhead should connect to the special multiTimeline playhead
    // that is owned by the xSTUDIO Session object.
    property var multiComparePlayheadID: helpers.QUuidFromUuidString("442f6169-a3b0-465d-850d-03280b224d70")
    property var currentPlayheadUuid: currentPlayhead.uuid
    property alias timelinePlayhead: timelinePlayhead
    property bool isPlayheadActive: timelinePlayhead.pinnedSourceMode ? currentPlayhead.uuid == timelinePlayhead.uuid : false
    property var viewedTimelinePlayheadUuid: undefined

    XsPlayhead {
        id: timelinePlayhead
        uuid: timelines.length > 1 ? multiComparePlayheadID : viewedTimelinePlayheadUuid
        Component.onCompleted: {
            getViewedTimelinePlayheadUuid(0)
        }
    }

    function getViewedTimelinePlayheadUuid(retry) {

        // connect to the timeline playhead ...
        let playhead_idx = theSessionData.searchRecursive(
            "Playhead",
            "typeRole",
            viewedTimelineIndex
            )

        if (playhead_idx.valid) {
            let playhead_uuid = theSessionData.get(playhead_idx, "actorUuidRole")
            if (playhead_uuid == undefined) {
                // uh-oh - remember the session model is populated asynchronously
                // we might need to wait few milliseconds until "actorUuidRole" for
                // the playhead has been filled in.
                if (retry < 3) {
                    callbackTimer.setTimeout(function() { return function() {
                        getViewedTimelinePlayheadUuid(retry+1)
                    }}(), 200);
                }
            } else {
                viewedTimelinePlayheadUuid = playhead_uuid
            }
        } else if (retry < 3) {
            callbackTimer.setTimeout(function() { return function() {
                getViewedTimelinePlayheadUuid(retry+1)
            }}(), 200);
        }

    }

}